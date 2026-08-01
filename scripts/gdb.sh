#!/bin/bash
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
QEMU_LOG="$PROJECT_ROOT/qemu.log"
if ! [ -f "$QEMU_LOG" ]; then
	echo "Missing $QEMU_LOG"
	exit 1
fi
symbols_file="$(mktemp /tmp/embloader-gdb-symbols.XXXXXX)"
trap 'rm -f "$symbols_file"' EXIT
grep '^add-symbol-file ' "$QEMU_LOG" > "$symbols_file"
gdb -ex 'target extended-remote 127.0.0.1:1234' -x "$symbols_file"
