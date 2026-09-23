#!/bin/sh

function MAKE()
{
	if [ X$1 == X ];then
		echo "no module to make"
	else
		make -C /home/leo/nfs/sdk/kernel_sc377 M=$1 modules
	fi
}
echo "****** build linux mosule *****"
