#!/bin/bash
set -e

# 脚本所在目录为工程根（可直接 ./build.sh 或从任意路径调用）
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${ROOT_DIR}"

export CROSS_COMPILE="${CROSS_COMPILE:-arm-linaro-linux-uclibcgnueabihf-}"
export ARCH="${ARCH:-arm}"

TOOLCHAIN_PREFIX="${CROSS_COMPILE%-}"
export CC="${TOOLCHAIN_PREFIX}-gcc"
export CXX="${TOOLCHAIN_PREFIX}-g++"
export LD="${TOOLCHAIN_PREFIX}-ld"
export AR="${TOOLCHAIN_PREFIX}-ar"
export RANLIB="${TOOLCHAIN_PREFIX}-ranlib"
export STRIP="${TOOLCHAIN_PREFIX}-strip"

UDHCP_ARCHIVE="$(find "${ROOT_DIR}" -maxdepth 1 -type f \( -name 'udhcp-*.tar.gz' -o -name 'udhcp*.tgz' \) | sort | head -n 1)"

BUILD_DIR="${ROOT_DIR}/build"
OUTPUT_DIR="${ROOT_DIR}/output"

if [ "${1:-}" = "clean" ]; then
    rm -rf "${BUILD_DIR}" "${OUTPUT_DIR}"
    echo "clean: removed ${BUILD_DIR} and ${OUTPUT_DIR}"
    exit 0
fi

if [ -z "${UDHCP_ARCHIVE}" ]; then
    echo "udhcp source archive not found in ${ROOT_DIR}" >&2
    exit 1
fi

rm -rf "${BUILD_DIR}" "${OUTPUT_DIR}"
mkdir -p "${BUILD_DIR}" "${OUTPUT_DIR}"

tar -zxf "${UDHCP_ARCHIVE}" -C "${BUILD_DIR}"

UDHCP_DIR="$(find "${BUILD_DIR}" -maxdepth 1 -type d -name 'udhcp-*' | sort | head -n 1)"

if [ -z "${UDHCP_DIR}" ] || [ ! -d "${UDHCP_DIR}" ]; then
    echo "udhcp source directory not found under ${BUILD_DIR} after extract" >&2
    exit 1
fi

# 与 oss/udhcpd/build-udhcpd.sh 一致：仅编服务器 udhcpd（不编 udhcpc / dumpleases）
make -C "${UDHCP_DIR}" clean
make -j8 -C "${UDHCP_DIR}" udhcpd CROSS_COMPILE="${CROSS_COMPILE}"

"${STRIP}" --remove-section=.note --remove-section=.comment "${UDHCP_DIR}/udhcpd"
cp "${UDHCP_DIR}/udhcpd" "${OUTPUT_DIR}/udhcpd"
chmod 755 "${OUTPUT_DIR}/udhcpd"

echo
echo "build done:"
echo "  ${OUTPUT_DIR}/udhcpd"
echo "sources and objects: ${BUILD_DIR}/"
