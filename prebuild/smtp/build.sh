#!/bin/bash
set -e

# prebuild/smtp: build /bin/mail using mbedtls (server_name=... interface)
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

BUILD_DIR="${ROOT_DIR}/build"
STAGING_DIR="${BUILD_DIR}/staging"
OUTPUT_DIR="${ROOT_DIR}/output"
MAIL_SRC="${ROOT_DIR}/src/mail.cpp"
MAIL_CONFIG="${ROOT_DIR}/src/mbedtls_config_for_email_minimal.h"

MBEDTLS_ARCHIVE="${ROOT_DIR}/mbedtls-2.13.0.tar.gz"

if [ "${1:-}" = "clean" ]; then
    rm -rf "${BUILD_DIR}" "${OUTPUT_DIR}"
    echo "clean: removed ${BUILD_DIR} and ${OUTPUT_DIR}"
    exit 0
fi

if [ ! -f "${MBEDTLS_ARCHIVE}" ]; then
    echo "mbedtls-2.13.0.tar.gz not found under ${ROOT_DIR}" >&2
    exit 1
fi

if [ ! -f "${MAIL_SRC}" ] || [ ! -f "${MAIL_CONFIG}" ]; then
    echo "mail source or config missing under ${ROOT_DIR}/src" >&2
    exit 1
fi

rm -rf "${BUILD_DIR}" "${OUTPUT_DIR}"
mkdir -p "${BUILD_DIR}" "${STAGING_DIR}" "${OUTPUT_DIR}"

tar -zxf "${MBEDTLS_ARCHIVE}" -C "${BUILD_DIR}"

MBEDTLS_DIR="$(find "${BUILD_DIR}" -maxdepth 1 -type d -name 'mbedtls-*' | sort | head -n 1)"
if [ -z "${MBEDTLS_DIR}" ] || [ ! -d "${MBEDTLS_DIR}" ]; then
    echo "mbedtls source directory not found under ${BUILD_DIR}" >&2
    exit 1
fi

cp "${MAIL_CONFIG}" "${MBEDTLS_DIR}/include/mbedtls/config.h"

cd "${MBEDTLS_DIR}"

export replace_from_str='DESTDIR=/usr/local'
export replace_to_str="DESTDIR=${STAGING_DIR}"
sed -i "s#${replace_from_str}#${replace_to_str}#g" Makefile

make clean
make -j"$(nproc 2>/dev/null || echo 8)" lib
make install

cd "${ROOT_DIR}"

"${CXX}" -static -Os \
    -I"${STAGING_DIR}/include" \
    -I"${ROOT_DIR}/src" \
    "${MAIL_SRC}" \
    -o "${OUTPUT_DIR}/mail" \
    "${STAGING_DIR}/lib/libmbedtls.a" \
    "${STAGING_DIR}/lib/libmbedx509.a" \
    "${STAGING_DIR}/lib/libmbedcrypto.a" \
    -lpthread -lstdc++ -lm

"${STRIP}" --remove-section=.note --remove-section=.comment "${OUTPUT_DIR}/mail" 2>/dev/null || true
chmod 755 "${OUTPUT_DIR}/mail"

echo
echo "build done:"
echo "  mail: ${OUTPUT_DIR}/mail"
file "${OUTPUT_DIR}/mail"
