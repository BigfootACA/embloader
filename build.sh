#!/bin/bash
set -ex
cd "$(dirname "$0")"
git submodule update --init --depth 1
if ! [ -d edk2 ] && ! [ -d ../edk2 ]; then
	git clone https://github.com/tianocore/edk2.git edk2 \
		-b edk2-stable202508 --depth=1
	git -C edk2 submodule update --init --depth 1
fi
if [ -d ../edk2 ]; then
	export EDK2_PATH=$(realpath ../edk2)
elif [ -d edk2 ]; then
	export EDK2_PATH=$(realpath edk2)
fi
make -j $(nproc) basetools
make -j $(nproc) build-edk2
