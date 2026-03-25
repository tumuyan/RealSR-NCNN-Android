#!/bin/bash
# Build script for mnnsr-ncnn on Linux
# Prerequisites: run 3rdparty/setup.sh first

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
JNI_DIR="$PROJECT_ROOT/MNN-SR/src/main/jni"
THIRDPARTY_DIR="$PROJECT_ROOT/../3rdparty"

# Verify prerequisites
for d in "$THIRDPARTY_DIR/MNN/libs" "$THIRDPARTY_DIR/MNN/include" "$THIRDPARTY_DIR/libwebp"; do
    if [[ ! -d "$d" ]]; then
        echo "Error: $d not found. Please run 3rdparty/setup.sh first."
        exit 1
    fi
done

echo "Project root:   $PROJECT_ROOT"
echo "JNI dir:        $JNI_DIR"
echo "3rdparty dir:   $THIRDPARTY_DIR"

# ======================== Build mnnsr ========================
echo ""
echo "=== Building mnnsr ==="
cd "$JNI_DIR"
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"

# Copy MNN libs next to the binary
cp "$THIRDPARTY_DIR/MNN/libs/"*.so ./

echo ""
echo "========================================"
echo "Build completed successfully!"
echo "========================================"
echo ""
echo "Output: $JNI_DIR/build/mnnsr-ncnn"
ls -lh mnnsr-ncnn
echo ""
echo "Shared libraries:"
ls -lh *.so
