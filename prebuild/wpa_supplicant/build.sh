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

rm -rf libnl-1.1.4
tar -zxvf libnl-1.1.4.tgz
cd libnl-1.1.4
./configure --prefix=$(pwd)/install --host="${TOOLCHAIN_PREFIX}"
make clean 
make
make install
cd -

rm -rf wpa_supplicant-2.6
tar -zxvf wpa_supplicant-2.6.tgz
cd wpa_supplicant-2.6/wpa_supplicant
cp ../../defconfig .config
make clean
make
$STRIP $(pwd)/wpa_supplicant
