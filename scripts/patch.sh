#!/bin/bash
set -eo pipefail

function patch_submodules() {
	git config --file .gitmodules --get-regexp '^submodule\..*\.path$' | \
	while read -r _ submodule; do
		submod_id="${submodule//\//-}"
		for patch in "patches/$submod_id"/*.patch; do
			if ! [ -f "$patch" ]; then
				continue
			fi
			if patch -d "$submodule" -sfRp1 --dry-run < "$patch" &>/dev/null; then
				continue
			fi
			echo "Apply $patch to $submodule"
			patch -d "$submodule" -tNp1 < "$patch"
		done
	done
}

function check_tool() {
	if ! command -v "$1" &>/dev/null; then
		echo "$1 not found" >&2
		exit 1
	fi
}

function main() {
	check_tool git
	check_tool patch
	if ! [ -f .gitmodules ]; then
		exit 0
	fi
	patch_submodules
}

cd "$(dirname "$0")/../"
main
