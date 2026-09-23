#!/bin/bash
set -e

# 脚本所在目录为工程根（可直接 ./build.sh 或从任意路径调用）
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${ROOT_DIR}"

# TCL12Q (ts_5326) 默认工具链；可通过环境变量覆盖
export CROSS_COMPILE="${CROSS_COMPILE:-arm-ts-linux-uclibcgnueabihf-}"
export ARCH="${ARCH:-arm}"

TOOLCHAIN_PREFIX="${CROSS_COMPILE%-}"
export CC="${TOOLCHAIN_PREFIX}-gcc"
export CXX="${TOOLCHAIN_PREFIX}-g++"
export LD="${TOOLCHAIN_PREFIX}-ld"
export AR="${TOOLCHAIN_PREFIX}-ar"
export RANLIB="${TOOLCHAIN_PREFIX}-ranlib"
export STRIP="${TOOLCHAIN_PREFIX}-strip"

FTP_ARCHIVE="$(find "${ROOT_DIR}" -maxdepth 1 -type f \( -name 'netkit-ftp-*.tar.gz' -o -name 'netkit-ftp*.tgz' \) | sort | head -n 1)"

BUILD_DIR="${ROOT_DIR}/build"
STAGING_DIR="${BUILD_DIR}/staging"
OUTPUT_DIR="${ROOT_DIR}/output"

if [ "${1:-}" = "clean" ]; then
    rm -rf "${BUILD_DIR}" "${OUTPUT_DIR}"
    echo "clean: removed ${BUILD_DIR} and ${OUTPUT_DIR}"
    exit 0
fi

if [ -z "${FTP_ARCHIVE}" ]; then
    echo "netkit-ftp source archive not found in ${ROOT_DIR}" >&2
    exit 1
fi

rm -rf "${BUILD_DIR}" "${OUTPUT_DIR}"
mkdir -p "${BUILD_DIR}" "${STAGING_DIR}/bin" "${OUTPUT_DIR}"

tar -zxf "${FTP_ARCHIVE}" -C "${BUILD_DIR}"

FTP_DIR="$(find "${BUILD_DIR}" -maxdepth 1 -type d -name 'netkit-ftp-*' | sort | head -n 1)"

if [ -z "${FTP_DIR}" ] || [ ! -d "${FTP_DIR}" ]; then
    echo "netkit-ftp source directory not found under ${BUILD_DIR} after extract" >&2
    exit 1
fi

# main.c 引用了 platform.h，源码包中未附带，构建时生成空桩即可
cat > "${FTP_DIR}/ftp/platform.h" <<'EOF'
#ifndef PLATFORM_H
#define PLATFORM_H
#endif
EOF

# 交叉编译配置：静态链接，不依赖 readline/ncurses
cat > "${FTP_DIR}/MCONFIG" <<EOF
CC=${CC}
CFLAGS=-Os -Wall -Wstrict-prototypes
LDFLAGS=-static
LIBS=
USE_READLINE=0
EXEC_DIR=${STAGING_DIR}/bin
EOF

make -C "${FTP_DIR}/ftp" clean
make -j"$(nproc 2>/dev/null || echo 8)" -C "${FTP_DIR}/ftp"

"${STRIP}" --remove-section=.note --remove-section=.comment "${STAGING_DIR}/bin/ftpclient"
cp "${STAGING_DIR}/bin/ftpclient" "${OUTPUT_DIR}/ftp"
chmod 755 "${OUTPUT_DIR}/ftp"

echo
echo "build done:"
echo "  ${OUTPUT_DIR}/ftp"
file "${OUTPUT_DIR}/ftp"
echo "sources and objects: ${BUILD_DIR}/"
