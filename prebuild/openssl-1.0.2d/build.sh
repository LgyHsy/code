#!/bin/bash

export CROSS_COMPILE=${CROSS_COMPILE:-arm-ts-linux-uclibcgnueabihf-}
export ARCH=${ARCH:-arm}
export STRIP="${TOOLCHAIN_PREFIX}"strip

rm -rf openssl-1.0.2l
tar -zxvf openssl-1.0.2l.tar.gz
cd openssl-1.0.2l  

./Configure --prefix=$(pwd)/libopenssl linux-armv4 no-asm shared

make depend
make links

make clean 
make	
make install
