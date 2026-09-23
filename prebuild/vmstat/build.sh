#!/bin/bash
set -e

# prebuild/vmstat: cross-build vmstat for ts (libproc2 in-binary, libc dynamic OK)
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${ROOT_DIR}"

export CROSS_COMPILE="${CROSS_COMPILE:-arm-ts-linux-uclibcgnueabihf-}"
export ARCH="${ARCH:-arm}"

TOOLCHAIN_PREFIX="${CROSS_COMPILE%-}"
export CC="${TOOLCHAIN_PREFIX}-gcc"
export STRIP="${TOOLCHAIN_PREFIX}-strip"

PROCPS_ARCHIVE="$(find "${ROOT_DIR}" -maxdepth 1 -type f \( -name 'procps-*.tar.gz' -o -name 'procps*.tgz' \) | sort | head -n 1)"
BUILD_DIR="${ROOT_DIR}/build"
OUTPUT_DIR="${ROOT_DIR}/output"
XLOCALE_STUB="${ROOT_DIR}/uclibc_xlocale_stub.h"

if [ "${1:-}" = "clean" ]; then
    rm -rf "${BUILD_DIR}" "${OUTPUT_DIR}"
    echo "clean: removed ${BUILD_DIR} and ${OUTPUT_DIR}"
    exit 0
fi

if ! command -v "${CC}" >/dev/null 2>&1; then
    echo "cross compiler not found: ${CC}" >&2
    exit 1
fi
if [ -z "${PROCPS_ARCHIVE}" ]; then
    echo "procps source archive not found in ${ROOT_DIR}" >&2
    exit 1
fi
if [ ! -f "${XLOCALE_STUB}" ]; then
    echo "missing stub header: ${XLOCALE_STUB}" >&2
    exit 1
fi

rm -rf "${BUILD_DIR}" "${OUTPUT_DIR}"
mkdir -p "${BUILD_DIR}" "${OUTPUT_DIR}"
tar -zxf "${PROCPS_ARCHIVE}" -C "${BUILD_DIR}"

PROCPS_DIR="$(find "${BUILD_DIR}" -maxdepth 1 -type d -name 'procps-*' | sort | head -n 1)"
if [ -z "${PROCPS_DIR}" ] || [ ! -d "${PROCPS_DIR}" ]; then
    echo "procps source directory not found under ${BUILD_DIR}" >&2
    exit 1
fi

cd "${PROCPS_DIR}"
[ -f ./configure ] || ./autogen.sh

# Minimal configure:
#   --without-ncurses  vmstat 不需要 ncurses
#   --disable-shared   libproc2 编进 vmstat，设备上不需要 libproc2.so
#   ac_cv_*            交叉编译避免误生成 rpl_malloc/rpl_realloc
#   -include stub      ts uClibc 无 xlocale，不改源码
./configure \
    --host="${TOOLCHAIN_PREFIX}" \
    --without-ncurses \
    --disable-shared \
    ac_cv_func_malloc_0_nonnull=yes \
    ac_cv_func_realloc_0_nonnull=yes \
    CC="${CC}" \
    CFLAGS="-Os -include ${XLOCALE_STUB}"

make -j"$(nproc 2>/dev/null || echo 8)" src/vmstat

VMSTAT_BIN="${PROCPS_DIR}/src/vmstat"
[ -f "${VMSTAT_BIN}" ] || { echo "vmstat binary not produced" >&2; exit 1; }

"${STRIP}" --remove-section=.note --remove-section=.comment "${VMSTAT_BIN}"
cp "${VMSTAT_BIN}" "${OUTPUT_DIR}/vmstat"
chmod 755 "${OUTPUT_DIR}/vmstat"

echo
echo "build done: ${OUTPUT_DIR}/vmstat"
file "${OUTPUT_DIR}/vmstat"
"${TOOLCHAIN_PREFIX}-readelf" -d "${OUTPUT_DIR}/vmstat" 2>/dev/null | grep NEEDED || true
