#!/bin/bash

export CROSS_COMPILE=${CROSS_COMPILE:-arm-ts-linux-uclibcgnueabihf}
export ARCH=${ARCH:-arm}
TOOLCHAIN_PREFIX=${CROSS_COMPILE%-}
export CC="${TOOLCHAIN_PREFIX}-gcc -DNDEBUG"
export CXX="${TOOLCHAIN_PREFIX}-g++ -DNDEBUG"
export LD="${TOOLCHAIN_PREFIX}-ld"
export AR="${TOOLCHAIN_PREFIX}"-ar
export LDXX="${TOOLCHAIN_PREFIX}"-g++
export RANLIB="${TOOLCHAIN_PREFIX}"-ranlib
export STRIP="${TOOLCHAIN_PREFIX}"-strip

rm -rf civetweb
tar -zxvf civetweb_1.7.tgz
cd civetweb
make clean
make lib  
make slib
