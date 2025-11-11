#!/bin/bash
set -ex
cd "$(dirname "$0")"
git submodule update --init --recursive --depth 1
export EDK2_PATH=$(realpath edk2)
make -j $(nproc) basetools
make -j $(nproc) build-edk2
