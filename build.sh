#!/bin/bash
set -ex
cd "$(dirname "$0")"
if [[ -e .git ]]; then
	submodules=(
		edk2
		embloader/ext/newlib
		embloader/ext/json-c
		embloader/ext/dtc
		embloader/ext/libufdt
		embloader/ext/stb
		embloader/ext/libyaml
		embloader/ext/lvgl
		embloader/ext/nanosvg
	)
	git submodule update --init --depth 1 -- "${submodules[@]}"
	git -C edk2 submodule update --init --depth 1 -- \
		BaseTools/Source/C/BrotliCompress/brotli
else
	required_files=(
		edk2/edksetup.sh
		edk2/BaseTools/Source/C/BrotliCompress/brotli/c/common/constants.c
		embloader/ext/newlib/newlib/libc/include/stdlib.h
		embloader/ext/json-c/json_object.c
		embloader/ext/dtc/libfdt/fdt.c
		embloader/ext/libufdt/ufdt_overlay.c
		embloader/ext/stb/stb_image.h
		embloader/ext/libyaml/src/api.c
		embloader/ext/lvgl/src/lv_init.c
		embloader/ext/nanosvg/src/nanosvg.h
	)
	for required_file in "${required_files[@]}"; do
		if [[ ! -f "$required_file" ]]; then
			echo "Missing build dependency: $required_file" >&2
			exit 1
		fi
	done
fi
EDK2_PATH="$(realpath edk2)"
export EDK2_PATH
make_args=()
jobs="${EMBLOADER_JOBS:-}"
if [[ -z "$jobs" && " ${DEB_BUILD_OPTIONS:-} " =~ [[:space:]]parallel=([0-9]+)[[:space:]] ]]; then
	jobs=${BASH_REMATCH[1]}
fi
if [[ -n "$jobs" ]]; then
	if [[ ! "$jobs" =~ ^[1-9][0-9]*$ ]]; then
		echo "Invalid build parallelism: $jobs" >&2
		exit 1
	fi
	make_args=(-j "$jobs")
elif [[ "${MAKEFLAGS:-}" != *-j* && "${MAKEFLAGS:-}" != *--jobs* && \
		"${MAKEFLAGS:-}" != *jobserver* ]]; then
	make_args=(-j "$(nproc)")
fi
make "${make_args[@]}" build-edk2
