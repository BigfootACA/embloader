#!/bin/bash

set -ex

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build/disk"
IMAGE_NAME="embloader-boot.img"
IMAGE_SIZE_MIB=128
MOUNT_POINT=""
IMAGE_PATH="$BUILD_DIR/$IMAGE_NAME"
QEMU_LOG="$PROJECT_ROOT/qemu.log"
LOOP_DEVICE=""
if [ -z "${EDK2_TARGET:-}" ]; then
	export EDK2_TARGET=RELEASE
fi
if [ -z "${ARCH:-}" ]; then
	ARCH="$(uname -m)"
	export ARCH
fi
case "$ARCH" in
	x86_64|x64|amd64) ARCH=x86_64; EDK2_ARCH=X64 ;;
	aarch64|arm64|armv8a) ARCH=aarch64; EDK2_ARCH=AARCH64 ;;
	arm|armhf|arm32|armv6l|armv7l|armv8l) ARCH=arm; EDK2_ARCH=ARM ;;
	i386|i486|i586|i686|x86|ia32) ARCH=i386; EDK2_ARCH=IA32 ;;
	riscv64) EDK2_ARCH=RISCV64 ;;
	*) echo "Unsupported architecture: $ARCH" >&2; exit 1 ;;
esac
export ARCH EDK2_ARCH
EDK2_LARCH="${EDK2_ARCH,,}"
export EDK2_LARCH

function cleanup() {
	if [ -n "$MOUNT_POINT" ] && mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
		umount "$MOUNT_POINT" || true
	fi
	if [ -n "$MOUNT_POINT" ] && [ -d "$MOUNT_POINT" ]; then
		rmdir "$MOUNT_POINT" || true
	fi
	if [ -n "$LOOP_DEVICE" ]; then
		losetup -d "$LOOP_DEVICE" 2>/dev/null || true
	fi
}

trap cleanup EXIT

function check_tool() {
	command -v "$1" >/dev/null || { echo "Missing $1"; exit 1; }
}

function build_efi(){
	cd "$PROJECT_ROOT"
	bash build.sh
}

function pack_boot(){
	cd "$PROJECT_ROOT"
	for tool in dd mkfs.fat parted losetup mount mountpoint umount mktemp; do
		check_tool "$tool"
	done
	if [ -z "${OLD_UID:-}" ]; then
		export OLD_UID="$UID"
	fi
	if [ -z "${OLD_GID:-}" ]; then
		OLD_GID="$(id -g)"
		export OLD_GID
	fi
	if [[ ! "$OLD_UID" =~ ^[0-9]+$ ]] || [[ ! "$OLD_GID" =~ ^[0-9]+$ ]]; then
		echo "Invalid owner: $OLD_UID:$OLD_GID" >&2
		exit 1
	fi
	if [ "$UID" != 0 ]; then
		check_tool sudo
		sudo -E bash "$SCRIPT_DIR/run.sh" pack_boot
		return
	fi
	mkdir -p "$BUILD_DIR"

	rm -f "$IMAGE_PATH"
	dd if=/dev/zero of="$IMAGE_PATH" bs=1M count="$IMAGE_SIZE_MIB"

	parted -s "$IMAGE_PATH" mklabel gpt
	parted -s "$IMAGE_PATH" mkpart primary fat32 1MiB 100%
	parted -s "$IMAGE_PATH" set 1 esp on

	LOOP_DEVICE=$(losetup --show -fP "$IMAGE_PATH")
	mkfs.fat -F32 "${LOOP_DEVICE}p1"

	MOUNT_POINT="$(mktemp -d /tmp/embloader-mount.XXXXXX)"
	mount "${LOOP_DEVICE}p1" "$MOUNT_POINT"

	mkdir -p "$MOUNT_POINT/EFI/BOOT"
	EFIARCH=""
	EFIFILE=""
	case "$EDK2_ARCH" in
		X64) EFIARCH="X64"; EFIFILE="BOOTX64.EFI" ;;
		AARCH64) EFIARCH="AARCH64"; EFIFILE="BOOTAA64.EFI" ;;
		ARM) EFIARCH="ARM"; EFIFILE="BOOTARM.EFI" ;;
		IA32) EFIARCH="IA32"; EFIFILE="BOOTIA32.EFI" ;;
		RISCV64) EFIARCH="RISCV64"; EFIFILE="BOOTRISCV64.EFI" ;;
		*) echo "Unsupported EFI architecture: $EDK2_ARCH" >&2; exit 1 ;;
	esac
	cp \
		"build/Build/embloader/${EDK2_TARGET:-RELEASE}_${EDK2_TOOLCHAIN:-GCC5}/$EFIARCH/embloader.efi" \
		"$MOUNT_POINT/EFI/BOOT/$EFIFILE"

	mkdir -p "$MOUNT_POINT/embloader"
	cp "configs/general/config.static.yaml" "$MOUNT_POINT/embloader/"
	cp "configs/general/config.dynamic.yaml" "$MOUNT_POINT/embloader/"

	if [ -f "local/qemu.dtb" ]; then
		mkdir -p "$MOUNT_POINT/dtbs/mainline/qemu"
		cp "local/qemu.dtb" "$MOUNT_POINT/dtbs/mainline/qemu/qemu-virt.dtb"
	fi

	mkdir -p "$MOUNT_POINT/dtbo/mainline/common"
	for i in "$PROJECT_ROOT"/local/*.dtso; do
		[ -f "$i" ] || continue
		dtc -I dts -O dtb \
			-o "$MOUNT_POINT/dtbo/mainline/common/$(basename "$i" .dtso).dtbo" \
			"$i"
	done
	for i in "$PROJECT_ROOT"/local/*.dtbo; do
		[ -f "$i" ] || continue
		cp "$i" "$MOUNT_POINT/dtbo/mainline/common/"
	done
	if [ -f /boot/vmlinuz-linux-mainline-aarch64 ]; then
		cp /boot/vmlinuz-linux-mainline-aarch64 "$MOUNT_POINT/"
	fi
	if [ -f /boot/initramfs-linux-mainline-aarch64.img ]; then
		cp /boot/initramfs-linux-mainline-aarch64.img "$MOUNT_POINT/"
	fi
	if [ -f /boot/vmlinuz-linux ]; then
		cp /boot/vmlinuz-linux "$MOUNT_POINT/"
	fi
	if [ -f /boot/initramfs-linux.img ]; then
		cp /boot/initramfs-linux.img "$MOUNT_POINT/"
	fi

	if [ -f "/usr/share/edk2-shell/$EDK2_LARCH/Shell_Full.efi" ]; then
		cp "/usr/share/edk2-shell/$EDK2_LARCH/Shell_Full.efi" "$MOUNT_POINT/efi-shell.efi"
	fi

	sync
	umount "$MOUNT_POINT"
	rmdir "$MOUNT_POINT"
	MOUNT_POINT=""
	losetup -d "$LOOP_DEVICE"
	LOOP_DEVICE=""

	chown -- "$OLD_UID:$OLD_GID" "$IMAGE_PATH"
}

function run_qemu_x86_64(){
	exec qemu-system-x86_64 \
		-enable-kvm \
		-nic none \
		-display none \
		-serial stdio \
		-bios /usr/share/edk2/x64/OVMF.4m.fd \
		-drive "file=$IMAGE_PATH,if=virtio,format=raw"
}

function run_qemu_aarch64_debug_real(){
	exec qemu-system-aarch64 \
		-m 1G \
		-machine virt \
		-nic none \
		-display none \
		-serial stdio \
		-cpu cortex-a72 \
		-device virtio-gpu \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-mouse \
		-device usb-tablet \
		-bios "$PROJECT_ROOT/local/QEMU_EFI.fd" \
		-drive "file=$IMAGE_PATH,if=virtio,format=raw" \
		-s
}

function run_qemu_aarch64_debug(){
	local run_command
	printf -v run_command 'bash %q run_qemu_aarch64_debug_real' "$SCRIPT_DIR/run.sh"
	exec script \
		--return \
		--log-out "$QEMU_LOG" \
		--flush \
		--quiet \
		-c "$run_command"
}

function run_qemu_aarch64_release(){
	exec qemu-system-aarch64 \
		-m 1G \
		-machine virt \
		-nic none \
		-display none \
		-serial stdio \
		-bios /usr/share/edk2/aarch64/QEMU_CODE.fd \
		-cpu host \
		-enable-kvm \
		-drive "file=$IMAGE_PATH,if=virtio,format=raw"
}

function run_qemu_aarch64(){
	case "$EDK2_TARGET" in
		RELEASE) run_qemu_aarch64_release ;;
		DEBUG|NOOPT) run_qemu_aarch64_debug ;;
		*) echo "Unknown EDK2_TARGET: $EDK2_TARGET"; exit 1 ;;
	esac
}

function run_qemu(){
	case "$EDK2_ARCH" in
		X64) run_qemu_x86_64 ;;
		AARCH64) run_qemu_aarch64 ;;
		*) echo "Unsupported QEMU architecture: $EDK2_ARCH"; exit 1 ;;
	esac
}

case "$1" in
	build_efi) build_efi ;;
	pack_boot) pack_boot ;;
	run_qemu) run_qemu ;;
	run_qemu_x86_64) run_qemu_x86_64 ;;
	run_qemu_aarch64) run_qemu_aarch64 ;;
	run_qemu_aarch64_release) run_qemu_aarch64_release ;;
	run_qemu_aarch64_debug) run_qemu_aarch64_debug ;;
	run_qemu_aarch64_debug_real) run_qemu_aarch64_debug_real ;;
	"") build_efi; pack_boot; run_qemu ;;
	*) echo "Unknown command: $1" >&2; exit 1 ;;
esac
