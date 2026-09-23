#!/bin/bash

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"


TARGET_STATIC=1       # 1=静态库，0=动态库
BUILD_TYPE="Release"  # Release 或 Debug
CLEAN_BUILD=1         # 是否清理构建

if [ "$1" = "clean" ]; then
    CLEAN_BUILD=1
fi

BUILD_PROJECT_NAME=""
BUILD_PLATFORMS_NAME=""
TARGET_LIB_DIR=""

# 从config.cmake读取
if [ -z "$BUILD_PLATFORMS_NAME" ] && [ -f "$PROJECT_ROOT/config.cmake" ]; then
    if grep -q "set(BUILD_PLATFORMS_NAME" "$PROJECT_ROOT/config.cmake"; then
        BUILD_PLATFORMS_NAME=$(grep "set(BUILD_PLATFORMS_NAME" "$PROJECT_ROOT/config.cmake" | cut -d'(' -f2 | cut -d' ' -f2 | tr -d ')')
    fi

    if grep -q "set(BUILD_PROJECT_NAME" "$PROJECT_ROOT/config.cmake"; then
        BUILD_PROJECT_NAME=$(grep "set(BUILD_PROJECT_NAME" "$PROJECT_ROOT/config.cmake" | cut -d'(' -f2 | cut -d' ' -f2 | tr -d ')')
    fi

    if grep -q "set(BUILD_COMPILER" "$PROJECT_ROOT/config.cmake"; then
        BUILD_COMPILER=$(grep "set(BUILD_COMPILER" "$PROJECT_ROOT/config.cmake" | cut -d'(' -f2 | cut -d' ' -f2 | tr -d ')')
        export CC="${BUILD_COMPILER}gcc"
        export CXX="${BUILD_COMPILER}g++"
        export AR="${BUILD_COMPILER}ar"
    fi
    
    if grep -q "set(DEBUG y)" "$PROJECT_ROOT/config.cmake"; then
        BUILD_TYPE="Debug"
    fi
fi

# 预编译库按工具链存放（与主工程 module_option / prebuild 约定一致）
if [ -z "$BUILD_COMPILER" ]; then
    echo "BUILD_COMPILER not set (need config.cmake with CONFIG_BUILD_COMPILER)"
    exit 1
fi
TARGET_LIB_DIR="$PROJECT_ROOT/prebuild/common_libs/${BUILD_COMPILER}/lib"
echo "Target lib directory: $TARGET_LIB_DIR"


if [ $CLEAN_BUILD -eq 1 ]; then
    echo "Cleaning SN build..."
    rm -rf "$SCRIPT_DIR/build"
    rm -f "${TARGET_LIB_DIR:?}"/libsoftsn.*
    echo "Clean completed!"
fi

if [ ! -d "$SCRIPT_DIR/src" ]; then
    echo "Error: src directory not found in $SCRIPT_DIR"
    exit 1
fi


echo "======================================="
echo "Building softsn library"
echo "Build type: $BUILD_TYPE"
echo "Library type: $([ $TARGET_STATIC -eq 1 ] && echo "Static (libsoftsn.a)" || echo "Shared (libsoftsn.so)")"
echo "Compiler: ${CC:-System default}"
echo "======================================="


BUILD_DIR="$SCRIPT_DIR/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR" || exit 1

# 准备CMake参数
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_PLATFORMS_NAME=$BUILD_PLATFORMS_NAME -DBUILD_PROJECT_NAME=$BUILD_PROJECT_NAME"
if [ $TARGET_STATIC -eq 1 ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DBUILD_SHARED_LIBS=OFF"
else
    CMAKE_ARGS="$CMAKE_ARGS -DBUILD_SHARED_LIBS=ON"
fi


echo "Configuring CMake..."
cmake .. $CMAKE_ARGS

if [ $? -ne 0 ]; then
    echo "CMake configuration failed!"
    exit 1
fi

echo "Compiling..."
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

# 复制到目标目录
echo "Copying library to target directory..."
mkdir -p "$TARGET_LIB_DIR"

if [ $TARGET_STATIC -eq 1 ]; then
    if [ -f "libsoftsn.a" ]; then
        cp libsoftsn.a "$TARGET_LIB_DIR/"
        echo "Copied: libsoftsn.a -> $TARGET_LIB_DIR/"
    else
        echo "Error: libsoftsn.a not found in build directory!"
        exit 1
    fi
else
    if [ -f "libsoftsn.so" ]; then
        cp libsoftsn.so "$TARGET_LIB_DIR/"
        echo "Copied: libsoftsn.so -> $TARGET_LIB_DIR/"
    else
        echo "Error: libsoftsn.so not found in build directory!"
        exit 1
    fi
fi

echo "======================================="
echo "Build completed successfully!"
echo "======================================="