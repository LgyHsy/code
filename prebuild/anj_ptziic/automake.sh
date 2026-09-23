#!/bin/bash
set -e

rm -rf build
mkdir build
cp *.c build/
cp *.h build/
cd build
cmake ../
make
./make_anj_ptziic_module.sh

echo "****** anj_ptziic.ko ******"
ls -l anj_ptziic.ko

# 部署到 TYE40H rootfs，由 /opt/ch/loadko 在 anjcam 之前 insmod
DEPLOY_DIR=/home/leo/nfs/code/ipc2/trunk/build/TYE40H/modify/rootfs/opt/ch
if [ -d "$DEPLOY_DIR" ]; then
	cp -f anj_ptziic.ko $DEPLOY_DIR/
	echo "deploy -> $DEPLOY_DIR/anj_ptziic.ko"
fi
