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

HOSTAP_ARCHIVE="$(find "${ROOT_DIR}" -maxdepth 1 -type f \( -name 'hostapd-*.tar.gz' -o -name 'hostapd*.tgz' \) | sort | head -n 1)"
LIBNL_ARCHIVE="${ROOT_DIR}/../wpa_supplicant/libnl-1.1.4.tgz"

BUILD_DIR="${ROOT_DIR}/build"
OUTPUT_DIR="${ROOT_DIR}/output"
STAGING_DIR="${BUILD_DIR}/staging"

if [ "${1:-}" = "clean" ]; then
    rm -rf "${BUILD_DIR}" "${OUTPUT_DIR}"
    echo "clean: removed ${BUILD_DIR} and ${OUTPUT_DIR}"
    exit 0
fi

if [ -z "${HOSTAP_ARCHIVE}" ]; then
    echo "hostapd source archive not found in ${ROOT_DIR}" >&2
    exit 1
fi

if [ ! -f "${LIBNL_ARCHIVE}" ]; then
    echo "libnl archive not found: ${LIBNL_ARCHIVE}" >&2
    exit 1
fi

rm -rf "${BUILD_DIR}" "${OUTPUT_DIR}"
mkdir -p "${BUILD_DIR}" "${OUTPUT_DIR}" "${STAGING_DIR}"

tar -zxf "${LIBNL_ARCHIVE}" -C "${BUILD_DIR}"
tar -zxf "${HOSTAP_ARCHIVE}" -C "${BUILD_DIR}"

LIBNL_DIR="$(find "${BUILD_DIR}" -maxdepth 1 -type d -name 'libnl-*' | sort | head -n 1)"
HOSTAP_DIR="$(find "${BUILD_DIR}" -maxdepth 1 -type d -name 'hostapd-*' | sort | head -n 1)"

if [ -z "${LIBNL_DIR}" ] || [ ! -d "${LIBNL_DIR}" ]; then
    echo "libnl source directory not found under ${BUILD_DIR} after extract" >&2
    exit 1
fi
if [ -z "${HOSTAP_DIR}" ] || [ ! -d "${HOSTAP_DIR}" ]; then
    echo "hostapd source directory not found under ${BUILD_DIR} after extract" >&2
    exit 1
fi

cd "${LIBNL_DIR}"
./configure --prefix="${STAGING_DIR}" --host="${TOOLCHAIN_PREFIX}"
make clean
make -j8
make install
cd "${ROOT_DIR}"

# 与 oss/hostapd/prebuild-defconfig 一致（未启用 CONFIG_LIBNL_TINY：本脚本用 libnl-1.1.4 装到 STAGING）
cat > "${HOSTAP_DIR}/hostapd/.config" <<EOF
CONFIG_DRIVER_NL80211=y

CONFIG_IEEE80211N=y
CONFIG_WPA=y
CONFIG_TLS=internal
CONFIG_CRYPTO=internal

CONFIG_NO_ACCOUNTING=y
CONFIG_NO_RADIUS=y
CONFIG_NO_VLAN=y
CONFIG_NO_DUMP_STATE=y

PKG_CONFIG=pkg-config

CONFIG_INTERNAL_LIBTOMMATH=y
CFLAGS += -I${STAGING_DIR}/include
LIBS += -L${STAGING_DIR}/lib
EOF

make -C "${HOSTAP_DIR}/hostapd" clean
make -j8 -C "${HOSTAP_DIR}/hostapd" hostapd \
  EXTRA_CFLAGS="-Os -ffunction-sections -fdata-sections" \
  LDFLAGS="-Wl,--gc-sections"

"${STRIP}" "${HOSTAP_DIR}/hostapd/hostapd"
cp "${HOSTAP_DIR}/hostapd/hostapd" "${OUTPUT_DIR}/hostapd"
chmod 755 "${OUTPUT_DIR}/hostapd"

echo
echo "build done:"
echo "  ${OUTPUT_DIR}/hostapd"
echo "sources and objects: ${BUILD_DIR}/"
