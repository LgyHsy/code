#!/bin/bash

export CROSS_COMPILE=${CROSS_COMPILE:-arm-ts-linux-uclibcgnueabihf}
export ARCH=${ARCH:-arm}
TOOLCHAIN_PREFIX=${CROSS_COMPILE%-}
export CC="${TOOLCHAIN_PREFIX}-gcc"
export CXX="${TOOLCHAIN_PREFIX}-g++"
export LD="${TOOLCHAIN_PREFIX}-ld"
export LDXX="${TOOLCHAIN_PREFIX}"-g++
export AR="${TOOLCHAIN_PREFIX}"-ar
export RANLIB="${TOOLCHAIN_PREFIX}"-ranlib
export STRIP="${TOOLCHAIN_PREFIX}"-strip

rm -rf mbedtls-2.13.0
tar -zxvf mbedtls-2.13.0.tar.gz
cp mbedtls_config_for_email.h mbedtls-2.13.0/include/mbedtls/config.h

cd mbedtls-2.13.0

export replace_from_str='DESTDIR=/usr/local'
export replace_to_str='DESTDIR=release'
sed -i "s#$replace_from_str#$replace_to_str#g" `grep $replace_from_str -rl Makefile`

make clean
make lib
make install

cd -
rm -rf curl-7.65.3
tar -zxvf curl-7.65.3.tgz
cd curl-7.65.3

./configure CC="${TOOLCHAIN_PREFIX}-gcc" \
	--host=arm-linux \
	--with-mbedtls=/$(pwd)/../../mbedtls-2.13.0/release/lib \
	--enable-optimize \
	--disable-debug \
	--disable-curldebug \
	--disable-symbol-hiding \
	--disable-dict \
	--disable-gopher \
	--disable-imap \
	--disable-pop3 \
	--disable-rtsp \
	--disable-smtp \
	--disable-telnet \
	--disable-sspi \
	--disable-smb \
	--disable-ntlm-wb \
	--disable-tls-srp \
	--disable-soname-bump \
	--disable-manual \
	--disable-file \
	--disable-ldap \
	--disable-tftp \
	--enable-http \
	--disable-ftp \
	--disable-ipv6
make clean
make
