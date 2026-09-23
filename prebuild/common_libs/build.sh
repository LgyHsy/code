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
export source_dir=/tmp/fdk-aac-0.1.6

rm -fr fdk-aac-0.1.6
tar -zxvf fdk-aac-0.1.6.tgz
cd fdk-aac-0.1.6
./configure --prefix=$(pwd)/fdk-aac --host="${TOOLCHAIN_PREFIX}" --with-pic
make clean 
make
make install

cd -
rm -rf lame-3.100.20170918
tar -zxvf lame-3.100.20170918.tgz
cd lame-3.100.20170918
./configure --prefix=$(pwd)/lame --host="${TOOLCHAIN_PREFIX}"  --with-pic
make clean 
make
make install
