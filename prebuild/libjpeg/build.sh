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
		
export libtool_dir=/tmp/libtool-2.4.6
export source_dir=/tmp/jpeg-9c

rm -rf libtool-2.4.6
rm -rf jpeg-9c

tar -zxvf libtool-2.4.6.tar.gz
tar -zxvf jpegsrc.v9c.tar.gz

cd libtool-2.4.6 
./configure --host="${TOOLCHAIN_PREFIX}"
make clean 
make

cp libtool ../jpeg-9c
cd ../jpeg-9c
./configure --host="${TOOLCHAIN_PREFIX}" CFLAGS='-O2' --prefix=$(pwd)/install
make clean 
make    
make install
		
$STRIP install/bin/*
$STRIP --strip-debug --strip-unneeded install/lib/libjpeg.so.9.3.0

