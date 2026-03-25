#!/bin/bash
# Setup script for NCNN, MNN, and libwebp libraries
# Designed for Ubuntu 22.04
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

if [[ "$ID" != "ubuntu" ]] && [[ "$VERSION_ID" != "22.04" ]]; then
    echo "Warning: This script is designed for Ubuntu 22.04. Current system: $PRETTY_NAME"
    echo "Continuing anyway..."
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

NCNN_EXTRACT_DIR="ncnn-${NCNN_VERSION}-ubuntu-2404-shared"
NCNN_DIR="ncnn-ubuntu-vulkan-shared"
NCNN_ZIP="ncnn-${NCNN_VERSION}-ubuntu-2404-shared.zip"

if [[ -d "$NCNN_DIR" ]]; then
    echo "NCNN directory '$NCNN_DIR' already exists, skipping download."
else
    # Remove leftover extraction dir if exists
    rm -rf "$NCNN_EXTRACT_DIR"

    echo "Downloading NCNN ${NCNN_VERSION} for Ubuntu 24.04..."
    wget -nv "https://github.com/Tencent/ncnn/releases/download/${NCNN_VERSION}/${NCNN_ZIP}" -O "${NCNN_ZIP}"

    echo "Extracting NCNN..."
    unzip -q "${NCNN_ZIP}"

    # Rename to expected CMake path: 3rdparty/ncnn-ubuntu-vulkan-shared/
    mv "$NCNN_EXTRACT_DIR" "$NCNN_DIR"

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

    rm -f "${NCNN_ZIP}"
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
echo "            lib:     $NCNN_DIR/lib/"
echo "            include: $NCNN_DIR/include/ncnn/"
echo "  libwebp:  $SCRIPT_DIR/libwebp (source)"
echo "  OpenCV:   system (libopencv-dev)"
echo ""
