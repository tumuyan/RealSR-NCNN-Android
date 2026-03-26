#!/bin/bash
# Setup script for NCNN, MNN, and libwebp libraries
# Designed for Ubuntu 22.04 (also supports other Linux distros with source build)
#
# CMake expected paths (from RealSR-NCNN-Android-CLI/CMake/):
#   MNN:     3rdparty/MNN/libs/libMNN.so  +  3rdparty/MNN/include/
#   NCNN:    3rdparty/ncnn-ubuntu-vulkan-shared/lib/libncnn.so  +  3rdparty/ncnn-ubuntu-vulkan-shared/include/ncnn/
#   libwebp: 3rdparty/libwebp/  (source, built via CMake add_subdirectory)
#   OpenCV:  system (libopencv-dev via apt)

set -e

# Version configuration
NCNN_VERSION="20260113"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "Working directory: $SCRIPT_DIR"

# System check
if [[ ! -f /etc/os-release ]]; then
    echo "Error: Cannot detect OS. /etc/os-release not found."
    exit 1
fi

. /etc/os-release

if [[ "$ID" == "ubuntu" ]]; then
    if [[ "$VERSION_ID" != "22.04" ]]; then
        echo "Warning: This script is designed for Ubuntu 22.04. Current: Ubuntu $VERSION_ID"
        echo "Continuing anyway..."
    fi
    IS_UBUNTU=1
else
    echo "Detected non-Ubuntu system: $PRETTY_NAME"
    echo "NCNN will be built from source instead of downloading prebuilt binaries."
    IS_UBUNTU=0
fi

# ======================== Install build dependencies ========================
echo ""
echo "=== Installing build dependencies ==="
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
    ca-certificates \
    build-essential \
    unzip \
    cmake \
    git \
    wget \
    libopencv-dev \
    ocl-icd-opencl-dev \
    python3-pip \
    clinfo \
    libvulkan-dev \
    vulkan-tools

update-ca-certificates

# ======================== Install Python dependencies ========================
echo ""
echo "=== Installing Python dependencies ==="

if command -v pip3 &>/dev/null; then
    pip3 install --break-system-packages \
        numpy \
        opencv-python \
        scikit-image \
        scipy
    echo "  Python packages installed successfully."
else
    echo "  Warning: pip3 not found, skipping Python package installation."
    echo "  You may need: apt-get install -y python3-pip"
fi

# ======================== Setup MNN ========================
echo ""
echo "=== Setting up MNN ==="

MNN_DIR="MNN"

if [[ -d "$MNN_DIR/libs" ]] && [[ -d "$MNN_DIR/include" ]]; then
    echo "MNN already exists, skipping."
else
    rm -rf "$MNN_DIR"

    echo "Cloning MNN source..."
    git clone https://github.com/alibaba/MNN --depth 1 "$MNN_DIR"

    echo "Building MNN from source..."
    mkdir -p "$MNN_DIR/libs"
    mkdir -p "$MNN_DIR/build"
    cd "$MNN_DIR/build"
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DMNN_TENSORRT=OFF \
        -DMNN_OPENGL=OFF \
        -DMNN_OPENCL=ON \
        -DMNN_VULKAN=ON \
        -DMNN_CUDA=OFF \
        -DMNN_SEP_BUILD=OFF
    make -j"$(nproc)"
    cd "$SCRIPT_DIR"
    cp -v "$(find "$MNN_DIR/build" -name 'libMNN*.so')" "$MNN_DIR/libs/"

    # Verify
    if [[ -d "$MNN_DIR/include" ]]; then
        echo "  MNN include directory found."
    else
        echo "  Error: MNN include directory not found."
        exit 1
    fi

    if ls "$MNN_DIR"/libs/libMNN* 1>/dev/null 2>&1; then
        echo "  MNN libraries:"
        ls -lh "$MNN_DIR"/libs/
    else
        echo "  Error: libMNN.so not found in $MNN_DIR/libs/"
        exit 1
    fi
fi

echo "  MNN location: $SCRIPT_DIR/$MNN_DIR"

# ======================== Setup NCNN ========================
echo ""
echo "=== Setting up NCNN ${NCNN_VERSION} ==="

NCNN_DIR="ncnn-ubuntu-vulkan-shared"

if [[ -d "$NCNN_DIR" ]]; then
    echo "NCNN directory '$NCNN_DIR' already exists, skipping."
else
    if [[ "$IS_UBUNTU" -eq 1 ]]; then
        # ---- Ubuntu: download prebuilt release package ----
        NCNN_EXTRACT_DIR="ncnn-${NCNN_VERSION}-ubuntu-2404-shared"
        NCNN_ZIP="ncnn-${NCNN_VERSION}-ubuntu-2404-shared.zip"

        rm -rf "$NCNN_EXTRACT_DIR"

        echo "Downloading NCNN ${NCNN_VERSION} prebuilt for Ubuntu..."
        wget -nv "https://github.com/Tencent/ncnn/releases/download/${NCNN_VERSION}/${NCNN_ZIP}" -O "${NCNN_ZIP}"

        echo "Extracting NCNN..."
        unzip -q "${NCNN_ZIP}"

        mv "$NCNN_EXTRACT_DIR" "$NCNN_DIR"
        rm -f "${NCNN_ZIP}"
    else
        # ---- Non-Ubuntu: build from source ----
        NCNN_SRC_DIR="ncnn-src"
        rm -rf "$NCNN_SRC_DIR"

        echo "Cloning NCNN source (tag: ${NCNN_VERSION})..."
        git clone --recursive --depth 1 --branch "$NCNN_VERSION" \
            https://github.com/Tencent/ncnn.git "$NCNN_SRC_DIR"

        echo "Building NCNN from source (Release, Vulkan, shared lib)..."
        mkdir -p "$NCNN_SRC_DIR/build"
        cd "$NCNN_SRC_DIR/build"
        cmake .. \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX=install \
            -DNCNN_VULKAN=ON \
            -DNCNN_SHARED_LIB=ON \
            -DNCNN_BUILD_EXAMPLES=OFF \
            -DNCNN_BUILD_TOOLS=OFF \
            -DNCNN_BUILD_BENCHMARK=OFF
        cmake --build . -j"$(nproc)"
        cmake --build . --target install/strip
        cd "$SCRIPT_DIR"

        echo "Copying build results to $NCNN_DIR/..."
        mkdir -p "$NCNN_DIR"
        cp -a "$NCNN_SRC_DIR/build/install/lib" "$NCNN_DIR/lib"
        cp -a "$NCNN_SRC_DIR/build/install/include" "$NCNN_DIR/include"

        echo "Cleaning up source directory..."
        rm -rf "$NCNN_SRC_DIR"
    fi

    # Verify: CMake expects lib/libncnn.so and include/ncnn/
    if [[ -f "$NCNN_DIR/lib/libncnn.so" ]]; then
        echo "  NCNN library found: $NCNN_DIR/lib/libncnn.so"
    else
        echo "  Error: libncnn.so not found in $NCNN_DIR/lib/"
        exit 1
    fi

    if [[ -d "$NCNN_DIR/include/ncnn" ]]; then
        echo "  NCNN headers found: $NCNN_DIR/include/ncnn/"
    else
        echo "  Error: ncnn headers not found in $NCNN_DIR/include/ncnn/"
        exit 1
    fi
fi

echo "  NCNN location: $SCRIPT_DIR/$NCNN_DIR"

# ======================== Setup libwebp ========================
echo ""
echo "=== Setting up libwebp ==="

if [[ -d "libwebp" ]]; then
    echo "libwebp directory already exists, skipping clone."
else
    echo "Cloning libwebp from GitHub..."
    git clone https://github.com/webmproject/libwebp --depth 1
fi

echo "  libwebp location: $SCRIPT_DIR/libwebp"

# ======================== Setup Models ========================
echo ""
echo "=== Setting up models ==="

MODELS_REPO="https://cnb.cool/ai-zoomer/realsr/models.git"
MODELS_TARGET_DIR="$SCRIPT_DIR/../RealSR-NCNN-Android-CLI/assets/Linux-x64"
MODELS_TMP_DIR="$SCRIPT_DIR/models-tmp"

mkdir -p "$MODELS_TARGET_DIR"

if ls "$MODELS_TARGET_DIR"/models-* 1>/dev/null 2>&1; then
    echo "Model directories already exist in $MODELS_TARGET_DIR, skipping."
else
    rm -rf "$MODELS_TMP_DIR"

    echo "Cloning models repository..."
    git clone --depth 1 "$MODELS_REPO" "$MODELS_TMP_DIR"

    echo "Copying model directories to $MODELS_TARGET_DIR..."
    cp -r "$MODELS_TMP_DIR"/models-* "$MODELS_TARGET_DIR/"

    echo "Cleaning up temporary clone..."
    rm -rf "$MODELS_TMP_DIR"

    # Verify
    MODEL_COUNT=$(ls -d "$MODELS_TARGET_DIR"/models-* 2>/dev/null | wc -l)
    if [[ "$MODEL_COUNT" -gt 0 ]]; then
        echo "  $MODEL_COUNT model directories copied:"
        ls -d "$MODELS_TARGET_DIR"/models-*
    else
        echo "  Error: No model directories found after copy."
        exit 1
    fi
fi

echo "  Models location: $MODELS_TARGET_DIR"

# ======================== Summary ========================
echo ""
echo "========================================"
echo "Setup completed successfully!"
echo "========================================"
echo ""
echo "Library paths:"
echo "  MNN:      $SCRIPT_DIR/$MNN_DIR"
echo "            libs:    $MNN_DIR/libs/"
echo "            include: $MNN_DIR/include/"
echo "  NCNN:     $SCRIPT_DIR/$NCNN_DIR"
if [[ "$IS_UBUNTU" -eq 1 ]]; then
    echo "            source:   prebuilt release (${NCNN_VERSION})"
else
    echo "            source:   built from source (${NCNN_VERSION})"
fi
echo "            lib:     $NCNN_DIR/lib/"
echo "            include: $NCNN_DIR/include/ncnn/"
echo "  libwebp:  $SCRIPT_DIR/libwebp (source)"
echo "  OpenCV:   system (libopencv-dev)"
echo "  Models:   $MODELS_TARGET_DIR"
echo ""
