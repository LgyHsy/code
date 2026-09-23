#!/bin/bash
set -e

# prebuild/pppd: cross-build pppd + rp-pppoe.so for TCL12Q (uclibc)
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${ROOT_DIR}"

export CROSS_COMPILE="${CROSS_COMPILE:-arm-ts-linux-uclibcgnueabihf-}"
export ARCH="${ARCH:-arm}"

TOOLCHAIN_PREFIX="${CROSS_COMPILE%-}"
export CC="${TOOLCHAIN_PREFIX}-gcc"
export CXX="${TOOLCHAIN_PREFIX}-g++"
export LD="${TOOLCHAIN_PREFIX}-ld"
export AR="${TOOLCHAIN_PREFIX}-ar"
export RANLIB="${TOOLCHAIN_PREFIX}-ranlib"
export STRIP="${TOOLCHAIN_PREFIX}-strip"

if ! command -v "${CC}" >/dev/null 2>&1; then
    TS_BIN="/home/leo/host-tools/gcc-ts-10.3-2023.10-x86_64-arm-none-linux-uclibcgnueabihf/bin"
    if [ -x "${TS_BIN}/${CC}" ]; then
        export PATH="${TS_BIN}:${PATH}"
    fi
fi

PPP_ARCHIVE="$(find "${ROOT_DIR}" -maxdepth 1 -type f \( -name 'ppp-*.tar.gz' -o -name 'ppp-*.tgz' \) | sort | head -n 1)"

BUILD_DIR="${ROOT_DIR}/build"
OUTPUT_DIR="${ROOT_DIR}/output"

if [ "${1:-}" = "clean" ]; then
    rm -rf "${BUILD_DIR}" "${OUTPUT_DIR}"
    echo "clean: removed ${BUILD_DIR} and ${OUTPUT_DIR}"
    exit 0
fi

if [ -z "${PPP_ARCHIVE}" ]; then
    echo "ppp source archive not found in ${ROOT_DIR}" >&2
    exit 1
fi

if ! command -v "${CC}" >/dev/null 2>&1; then
    echo "compiler not found: ${CC} (set CROSS_COMPILE or PATH)" >&2
    exit 1
fi

rm -rf "${BUILD_DIR}" "${OUTPUT_DIR}"
mkdir -p "${BUILD_DIR}" "${OUTPUT_DIR}"

tar -zxf "${PPP_ARCHIVE}" -C "${BUILD_DIR}"

PPP_DIR="$(find "${BUILD_DIR}" -maxdepth 1 -type d -name 'ppp-*' | sort | head -n 1)"
if [ -z "${PPP_DIR}" ] || [ ! -d "${PPP_DIR}" ]; then
    echo "ppp source directory not found under ${BUILD_DIR}" >&2
    exit 1
fi

# Minimal client: no libpcap / openssl / IPv6 / shadow; crypt() from libc
sed -i 's/^FILTER=y/#FILTER=y/' "${PPP_DIR}/pppd/Makefile.linux"
sed -i 's/^USE_EAPTLS=y/#USE_EAPTLS=y/' "${PPP_DIR}/pppd/Makefile.linux"
sed -i 's/^HAVE_INET6=y/#HAVE_INET6=y/' "${PPP_DIR}/pppd/Makefile.linux"
sed -i 's/^HAS_SHADOW=y/#HAS_SHADOW=y/' "${PPP_DIR}/pppd/Makefile.linux"
sed -i 's/^#USE_CRYPT=y/USE_CRYPT=y/' "${PPP_DIR}/pppd/Makefile.linux"
sed -i 's/^SUBDIRS :=.*/SUBDIRS := pppoe/' "${PPP_DIR}/pppd/plugins/Makefile.linux"
sed -i 's/^PLUGINS :=.*/PLUGINS :=/' "${PPP_DIR}/pppd/plugins/Makefile.linux"
sed -i 's/^CFLAGS += -DUSE_EAPTLS=1/#CFLAGS += -DUSE_EAPTLS=1/' "${PPP_DIR}/pppd/plugins/Makefile.linux"

cd "${PPP_DIR}"
./configure \
    --prefix=/usr \
    --sysconfdir=/etc \
    --cross_compile="${CROSS_COMPILE}" \
    --cc=gcc \
    --cflags="-Os -pipe"

make -j"$(nproc 2>/dev/null || echo 8)" -C pppd
make -j"$(nproc 2>/dev/null || echo 8)" -C pppd/plugins/pppoe

PPPD_BIN="${PPP_DIR}/pppd/pppd"
PPPOE_SO="${PPP_DIR}/pppd/plugins/pppoe/pppoe.so"
if [ ! -x "${PPPD_BIN}" ] || [ ! -f "${PPPOE_SO}" ]; then
    echo "build failed: pppd or pppoe.so missing" >&2
    exit 1
fi

"${STRIP}" --remove-section=.note --remove-section=.comment "${PPPD_BIN}" 2>/dev/null || true
"${STRIP}" --remove-section=.note --remove-section=.comment "${PPPOE_SO}" 2>/dev/null || true

cp "${PPPD_BIN}" "${OUTPUT_DIR}/pppd"
cp "${PPPOE_SO}" "${OUTPUT_DIR}/pppoe.so"
cp "${PPPOE_SO}" "${OUTPUT_DIR}/rp-pppoe.so"
chmod 755 "${OUTPUT_DIR}/pppd" "${OUTPUT_DIR}/pppoe.so" "${OUTPUT_DIR}/rp-pppoe.so"

echo
echo "build done:"
echo "  ${OUTPUT_DIR}/pppd"
echo "  ${OUTPUT_DIR}/rp-pppoe.so"
file "${OUTPUT_DIR}/pppd" "${OUTPUT_DIR}/rp-pppoe.so"
echo "sources and objects: ${BUILD_DIR}/"
