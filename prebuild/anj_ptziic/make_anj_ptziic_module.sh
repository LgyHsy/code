#!/bin/bash

# ts_5326 平台内核树（TYE40H 与 TCA55 同平台，共用该树）
KDIR=/home/leo/nfs/code/ipc2/trunk/build/TCA55/release/kernel
CROSS_COMPILE=arm-ts-linux-uclibcgnueabihf-

# 该内核 Makefile 为 ARCH := $(TARGET_ARCH) 且 CONFIG_CROSS_COMPILE 为空，
# 所以 ARCH / TARGET_ARCH / CROSS_COMPILE 必须显式传。
function MAKE()
{
	if [ X$1 == X ];then
		echo "no module to make"
	else
		make -C $KDIR M=$1 ARCH=arm TARGET_ARCH=arm CROSS_COMPILE=$CROSS_COMPILE modules
	fi
}
echo "****** build linux module *****"
