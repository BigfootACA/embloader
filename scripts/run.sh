#!/bin/bash

set -ex

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build/disk"
IMAGE_NAME="embloader-boot.img"
IMAGE_SIZE="64M"
MOUNT_POINT="/tmp/embloader_mount"
IMAGE_PATH="$BUILD_DIR/$IMAGE_NAME"

function cleanup() {
	mountpoint -q "$MOUNT_POINT" 2>/dev/null && umount "$MOUNT_POINT" || true
	[ -d "$MOUNT_POINT" ] && rmdir "$MOUNT_POINT" || true
	[ -n "$LOOP_DEVICE" ] && losetup -d "$LOOP_DEVICE" 2>/dev/null || true
}

trap cleanup EXIT

command -v dd >/dev/null || { echo "Missing dd"; exit 1; }
command -v mkfs.fat >/dev/null || { echo "Missing mkfs.fat"; exit 1; }
command -v parted >/dev/null || { echo "Missing parted"; exit 1; }
command -v losetup >/dev/null || { echo "Missing losetup"; exit 1; }

function build_efi(){
	cd "$PROJECT_ROOT"
	make -j 4
}

function pack_boot(){
	if [ -z "$OLD_UID" ]; then
		export OLD_UID="$UID"
	fi
	if [ "$UID" != 0 ]; then
		sudo -E bash -$- "$0" pack_boot
		return
	fi
	mkdir -p "$BUILD_DIR"

	rm -f "$IMAGE_PATH"
	dd if=/dev/zero of="$IMAGE_PATH" bs=1M count=128

	parted -s "$IMAGE_PATH" mklabel gpt
	parted -s "$IMAGE_PATH" mkpart primary fat32 1MiB 100%
	parted -s "$IMAGE_PATH" set 1 esp on

	LOOP_DEVICE=$(losetup --show -fP "$IMAGE_PATH")
	mkfs.fat -F32 "${LOOP_DEVICE}p1"

	mkdir -p "$MOUNT_POINT"
	mount "${LOOP_DEVICE}p1" "$MOUNT_POINT"

	mkdir -p "$MOUNT_POINT/EFI/BOOT"
	EFIARCH=""
	EFIFILE=""
	case "$(uname -m)" in
		x86_64) EFIARCH="X64"; EFIFILE="BOOTX64.EFI" ;;
		aarch64) EFIARCH="AARCH64"; EFIFILE="BOOTAA64.EFI" ;;
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

	sync
	umount "$MOUNT_POINT"
	losetup -d "$LOOP_DEVICE"
	LOOP_DEVICE=""

	chown "$OLD_UID" "$IMAGE_PATH"
}

function run_qemu_x86_64(){
	exec qemu-system-x86_64 \
		-enable-kvm \
		-nic none \
		-display none \
		-serial stdio \
		-bios /usr/share/edk2/x64/OVMF.4m.fd \
		-drive file=$IMAGE_PATH,if=virtio,format=raw
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
		-bios local/QEMU_EFI.fd \
		-drive file=$IMAGE_PATH,if=virtio,format=raw \
		-drive file=/tmp/esp.img,if=virtio,format=raw \
		-s
}

function run_qemu_aarch64_debug(){
	exec script \
		--log-out qemu.log \
		--flush \
		--quiet \
		-c "bash -$- '$0' run_qemu_aarch64_debug_real"
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
		-drive file=$IMAGE_PATH,if=virtio,format=raw \
		-drive file=/tmp/esp.img,if=virtio,format=raw
}

function run_qemu_aarch64(){
	case "$EDK2_TARGET" in
		RELEASE) run_qemu_aarch64_release ;;
		DEBUG) run_qemu_aarch64_debug ;;
		*) echo "Unknown EDK2_TARGET: $EDK2_TARGET"; exit 1 ;;
	esac
}

function run_qemu(){
	case "$(uname -m)" in
		x86_64) run_qemu_x86_64 ;;
		aarch64) run_qemu_aarch64 ;;
		*) echo "Unsupported architecture: $(uname -m)"; exit 1 ;;
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
esac
