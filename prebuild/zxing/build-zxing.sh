#!/bin/bash
set -euo pipefail

export CROSS_COMPILE=${CROSS_COMPILE:-arm-linaro-linux-uclibcgnueabihf}
export ARCH=${ARCH:-arm}
TOOLCHAIN_PREFIX=${CROSS_COMPILE%-}
CC_BIN=$(command -v "${TOOLCHAIN_PREFIX}-gcc")
CXX_BIN=$(command -v "${TOOLCHAIN_PREFIX}-g++")
AR_BIN=$(command -v "${TOOLCHAIN_PREFIX}-gcc-ar")
AR_REAL_BIN=$(command -v "${TOOLCHAIN_PREFIX}-ar")
RANLIB_BIN=$(command -v "${TOOLCHAIN_PREFIX}-gcc-ranlib")
STRIP_BIN=$(command -v "${TOOLCHAIN_PREFIX}-strip")

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
SRC_NAME="zxing-cpp-2.0.0"
SRC_TARBALL="${SRC_NAME}.tar.gz"
SRC_URL="https://github.com/zxing-cpp/zxing-cpp/archive/refs/tags/v2.0.0.tar.gz"
SRC_DIR="${ROOT_DIR}/zxing-cpp-2.0.0"
BUILD_DIR="${SRC_DIR}/build"
OUT_DIR="${SRC_DIR}/output"
LIB_DIR="${OUT_DIR}/lib"
LIB_UPPER="${LIB_DIR}/libZXing.a"
LIB_LOWER="${LIB_DIR}/libzxing.a"
PATCH_FILE="${ROOT_DIR}/zxing_qr_only.patch"
ZXING_PROFILE=${ZXING_PROFILE:-min-qr}
SIZE_GOAL_KB=${SIZE_GOAL_KB:-200}

download_source_if_needed() {
    if [ -f "${ROOT_DIR}/${SRC_TARBALL}" ]; then
        echo "Archive exists, skip download: ${SRC_TARBALL}"
        return 0
    fi

    echo "Downloading source archive: ${SRC_TARBALL}"
    if command -v wget > /dev/null 2>&1; then
        wget -O "${ROOT_DIR}/${SRC_TARBALL}" "${SRC_URL}"
    elif command -v curl > /dev/null 2>&1; then
        curl -L "${SRC_URL}" -o "${ROOT_DIR}/${SRC_TARBALL}"
    else
        echo "Neither wget nor curl is available, cannot download source archive"
        exit 1
    fi
}

extract_source_if_needed() {
    if [ -d "${SRC_DIR}" ]; then
        echo "Source directory exists, skip extract: ${SRC_NAME}"
        return 0
    fi

    if [ ! -f "${ROOT_DIR}/${SRC_TARBALL}" ]; then
        echo "Archive not found: ${ROOT_DIR}/${SRC_TARBALL}"
        exit 1
    fi

    echo "Extracting source archive: ${SRC_TARBALL}"
    tar -xzf "${ROOT_DIR}/${SRC_TARBALL}" -C "${ROOT_DIR}"
}

apply_patch_once() {
    local patch_file="$1"

    if [ ! -f "${patch_file}" ]; then
        echo "Patch not found: ${patch_file}"
        exit 1
    fi

    if patch -p1 -d "${SRC_DIR}" --dry-run -R < "${patch_file}" > /dev/null 2>&1; then
        echo "Patch already applied: ${patch_file}"
        return 0
    fi

    local patch_log
    patch_log=$(mktemp)

    echo "Ensuring patch applied: ${patch_file}"
    set +e
    patch -p1 -d "${SRC_DIR}" --forward --batch < "${patch_file}" >"${patch_log}" 2>&1
    local patch_rc=$?
    set -e

    if [ ${patch_rc} -eq 0 ]; then
        rm -f "${patch_log}"
        return 0
    fi

    if grep -q "saving rejects to file" "${patch_log}"; then
        cat "${patch_log}"
        rm -f "${patch_log}"
        echo "Patch apply failed: reject files were generated"
        exit 1
    fi

    cat "${patch_log}"
    rm -f "${patch_log}"
    echo "Patch apply failed"
    exit 1
}

repack_archive_for_min_size() {
    local in_lib="$1"
    local work_dir
    work_dir=$(mktemp -d)

    echo "Repack archive with section-GC: ${in_lib}"

    pushd "${work_dir}" > /dev/null
    "${AR_REAL_BIN}" x "${in_lib}"

    cat > zxing_gc_root.cpp <<'EOF'
#include <ZXing/ReadBarcode.h>
#include <ZXing/Result.h>

namespace {
using namespace ZXing;
auto* keep_read_barcode = &ReadBarcode;
auto* keep_read_barcodes = &ReadBarcodes;
}

extern "C" void zxing_gc_roots()
{
    (void)keep_read_barcode;
    (void)keep_read_barcodes;
}
EOF

    "${CXX_BIN}" -c zxing_gc_root.cpp -o zxing_gc_root.o \
        -std=gnu++17 -Os -ffunction-sections -fdata-sections -fno-rtti \
        -fno-unwind-tables -fno-asynchronous-unwind-tables \
        -I"${OUT_DIR}/include"

    # Link all archive objects into one relocatable object and drop unreachable sections.
    "${CXX_BIN}" -r -nostdlib -Wl,--gc-sections -Wl,-u,zxing_gc_roots -o zxing_min.o ./*.o

    "${AR_REAL_BIN}" crs "${in_lib}" zxing_min.o
    popd > /dev/null
    rm -rf "${work_dir}"
}

download_source_if_needed
extract_source_if_needed

cd "${SRC_DIR}"
rm -rf "${OUT_DIR}" "${BUILD_DIR}"
mkdir -p "${OUT_DIR}" "${BUILD_DIR}"
cd "${BUILD_DIR}"

if [ "${ZXING_PROFILE}" = "min-qr" ]; then
    apply_patch_once "${PATCH_FILE}"
fi

echo "ZXING_PROFILE=${ZXING_PROFILE}"

if [ "${ZXING_PROFILE}" = "min-qr" ]; then
    QR_ONLY_CMAKE="-DZXING_FORCE_QR_ONLY=ON"
else
    QR_ONLY_CMAKE="-DZXING_FORCE_QR_ONLY=OFF"
fi

cmake .. \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DCMAKE_INSTALL_PREFIX="${OUT_DIR}" \
    -DCMAKE_C_COMPILER=${CC_BIN} \
    -DCMAKE_CXX_COMPILER=${CXX_BIN} \
    -DCMAKE_AR=${AR_BIN} \
    -DCMAKE_RANLIB=${RANLIB_BIN} \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_BLACKBOX_TESTS=OFF \
    -DBUILD_UNIT_TESTS=OFF \
    -DBUILD_WRITERS=OFF \
    -DBUILD_READERS=ON \
    ${QR_ONLY_CMAKE} \
    -DCMAKE_CXX_FLAGS="-Os -ffunction-sections -fdata-sections -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables" \
    -DCMAKE_C_FLAGS="-Os -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables" \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF

make -j"$(nproc)"
make install

if [ "${ZXING_PROFILE}" = "min-qr" ] && [ -f "${LIB_UPPER}" ]; then
    cp -f "${LIB_UPPER}" "${LIB_UPPER}.before_repack"
    repack_archive_for_min_size "${LIB_UPPER}"
    before_size_kb=$(du -k "${LIB_UPPER}.before_repack" | awk '{print $1}')
    after_size_kb=$(du -k "${LIB_UPPER}" | awk '{print $1}')
    if [ "${after_size_kb}" -ge "${before_size_kb}" ]; then
        echo "Repack not beneficial: ${after_size_kb} KB >= ${before_size_kb} KB, restore original archive"
        mv -f "${LIB_UPPER}.before_repack" "${LIB_UPPER}"
    else
        echo "Repack beneficial: ${before_size_kb} KB -> ${after_size_kb} KB"
        rm -f "${LIB_UPPER}.before_repack"
    fi
fi

# Safe shrink for static archive: remove debug symbols only, then rebuild index.
if [ -f "${LIB_UPPER}" ]; then
    "${STRIP_BIN}" -g "${LIB_UPPER}"
    "${RANLIB_BIN}" "${LIB_UPPER}"
    cp -f "${LIB_UPPER}" "${LIB_LOWER}"
fi

if [ -f "${LIB_LOWER}" ]; then
    lib_size_kb=$(du -k "${LIB_LOWER}" | awk '{print $1}')
    echo "libzxing.a size: ${lib_size_kb} KB (goal <= ${SIZE_GOAL_KB} KB)"
fi

echo "Build done: ${LIB_LOWER}"

