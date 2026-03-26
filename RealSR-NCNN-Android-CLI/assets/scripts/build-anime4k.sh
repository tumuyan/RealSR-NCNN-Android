#!/bin/bash
# Build script for Anime4k on Linux
# Prerequisites: run 3rdparty/setup.sh first
#
# Note: Anime4k's CMakeLists.txt has Android-specific hardcoded paths.
# This script patches ncnn_DIR and OpenCV_DIR for Linux builds.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
JNI_DIR="$PROJECT_ROOT/Anime4k/src/main/jni"
THIRDPARTY_DIR="$PROJECT_ROOT/../3rdparty"
BINARY_NAME="Anime4k"

# Verify prerequisites
for d in "$THIRDPARTY_DIR/ncnn-ubuntu-vulkan-shared/lib" "$THIRDPARTY_DIR/ncnn-ubuntu-vulkan-shared/include/ncnn"; do
    if [[ ! -d "$d" ]]; then
        echo "Error: $d not found. Please run 3rdparty/setup.sh first."
        exit 1
    fi
done

if [[ ! -f "$THIRDPARTY_DIR/ncnn-ubuntu-vulkan-shared/lib/libncnn.so" ]]; then
    echo "Error: libncnn.so not found. Please run 3rdparty/setup.sh first."
    exit 1
fi

echo "Project root:   $PROJECT_ROOT"
echo "JNI dir:        $JNI_DIR"
echo "3rdparty dir:   $THIRDPARTY_DIR"

# ======================== Patch CMakeLists for Linux ========================
echo ""
echo "=== Patching Anime4k CMakeLists.txt for Linux ==="

# Backup original CMakeLists.txt
CMAKE_FILE="$JNI_DIR/CMakeLists.txt"
if [[ ! -f "$CMAKE_FILE.orig" ]]; then
    cp "$CMAKE_FILE" "$CMAKE_FILE.orig"
    echo "  Backup saved to CMakeLists.txt.orig"
else
    # Restore from backup before patching
    cp "$CMAKE_FILE.orig" "$CMAKE_FILE"
    echo "  Restored from CMakeLists.txt.orig"
fi

# Patch ncnn_DIR for Linux (replace Android-specific path)
sed -i 's|set(ncnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/ncnn-android-vulkan-shared/${ANDROID_ABI})|set(ncnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/ncnn-ubuntu-vulkan-shared)|' "$CMAKE_FILE"

# Patch OpenCV_DIR: use system OpenCV on Linux (find_package will locate it)
sed -i 's|set(OpenCV_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/OpenCV-android-sdk/sdk/native/jni)|# OpenCV_DIR: use system OpenCV on Linux (detected by find_package)|' "$CMAKE_FILE"

# Patch libncnn.so path for the imported target
sed -i 's|${ncnn_DIR}/lib/libncnn.so|${ncnn_DIR}/lib/libncnn.so|' "$CMAKE_FILE"

echo "  CMakeLists.txt patched for Linux."

# ======================== Build Anime4k ========================
echo ""
echo "=== Building ${BINARY_NAME} ==="
cd "$JNI_DIR"
rm -rf build
mkdir build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DEnable_NCNN=ON \
    -DEnable_OpenCL=OFF \
    -DBuild_GUI=OFF \
    -DBuild_C_Wrapper=OFF \
    -DBuild_Static_C_Wrapper=OFF \
    -DBuild_AviSynthPlus_Plugin=OFF \
    -DBuild_VapourSynth_Plugin=OFF
make -j"$(nproc)"

# Copy ncnn libs next to the binary
cp "$THIRDPARTY_DIR/ncnn-ubuntu-vulkan-shared/lib/"*.so ./bin/

echo ""
echo "========================================"
echo "Build completed successfully!"
echo "========================================"
echo ""
echo "Output: $JNI_DIR/build/bin/${BINARY_NAME}"
ls -lh "bin/${BINARY_NAME}"
echo ""
echo "Shared libraries:"
ls -lh bin/*.so
