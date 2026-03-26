#!/bin/bash
# Build script for srmd-ncnn on Linux
# Prerequisites: run 3rdparty/setup.sh first

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
JNI_DIR="$PROJECT_ROOT/SRMD/src/main/jni"
THIRDPARTY_DIR="$PROJECT_ROOT/../3rdparty"
BINARY_NAME="srmd-ncnn"

# Verify prerequisites
for d in "$THIRDPARTY_DIR/ncnn-ubuntu-vulkan-shared/lib" "$THIRDPARTY_DIR/ncnn-ubuntu-vulkan-shared/include/ncnn" "$THIRDPARTY_DIR/libwebp"; do
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

# ======================== Build srmd ========================
echo ""
echo "=== Building ${BINARY_NAME} ==="
cd "$JNI_DIR"
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"

# Copy ncnn libs next to the binary
cp "$THIRDPARTY_DIR/ncnn-ubuntu-vulkan-shared/lib/"*.so ./

echo ""
echo "========================================"
echo "Build completed successfully!"
echo "========================================"
echo ""
echo "Output: $JNI_DIR/build/${BINARY_NAME}"
ls -lh "${BINARY_NAME}"
echo ""
echo "Shared libraries:"
ls -lh *.so
