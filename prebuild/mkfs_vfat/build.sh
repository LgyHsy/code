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
	
rm mkfs.vfat
$CC -o mkfs.vfat formatsd_no_pagebuffer.c
$STRIP mkfs.vfat
