#!/bin/bash
set -ex
if ! [ -f qemu.log ]; then
	echo "Missing qemu.log"
	exit 1
fi
grep '^add-symbol-file\ ' qemu.log > /tmp/.gdb.symbols.sh
exec gdb -ex 'target extended-remote 127.0.0.1:1234' -x /tmp/.gdb.symbols.sh
