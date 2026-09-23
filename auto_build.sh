#!/bin/bash
stty echo

BUILD_MODE=$2
BUILD_PROJECT_NAME=$1

ensure_config() {
	if [ -e ".config" ]; then
		echo ".config exist!"
		return
	fi

	echo ".config not exist copy defconfig BUILD_PROJECT_NAME=$BUILD_PROJECT_NAME !"
	if [ X"$BUILD_PROJECT_NAME" != X ];then
		cp project/$BUILD_PROJECT_NAME/config/def_config .config
	else
		echo "./auto_build.sh (project) [all]"
		echo "for release: ./auto_build.sh MYF30"
		echo "for all libs : ./auto_build.sh MYF30 all"
		exit 1
	fi
}

run_build() {
	local cmake_args="$1"
	mkdir build
	cp .config build/
	cd build
	cmake $cmake_args ../
	make -k -j32
	cd -
}

if [ X"$1" == Xclean ];then
    echo clean
    rm build -rf
    rm out -rf
	rm .config config.cmake
    echo clean end
else
	echo build
	ensure_config

	if [ X"$BUILD_MODE" == Xall ];then
		rm -rf out
		run_build "-DANJ_BUILD_ALL_MODULES=ON -DANJ_SKIP_APP=ON"
	else
		run_build ""
	fi
	echo build end
fi
