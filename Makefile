SHELL = /bin/bash
ARCH ?= $(shell $(CC) -dumpmachine | cut -d- -f1)
EDK2_PATH ?= $(realpath edk2)
EDK2_TARGET ?= RELEASE
EDK2_TOOLCHAIN ?= GCC5
EDK_TOOLS_PATH ?= $(EDK2_PATH)/BaseTools
PACKAGES_PATH ?= $(EDK2_PATH):$(shell pwd)
WORKSPACE ?= $(shell pwd)/build
ifeq ($(ARCH),x86_64)
	EDK2_ARCH = X64
else ifeq ($(ARCH),aarch64)
	EDK2_ARCH = AARCH64
else ifeq ($(ARCH),riscv64)
	EDK2_ARCH = RISCV64
else ifeq ($(ARCH),i386)
	EDK2_ARCH = IA32
else
$(error Unsupported architecture: $(ARCH))
endif

export EDK2_PATH EDK_TOOLS_PATH PACKAGES_PATH WORKSPACE

all: build-edk2

clean:
	$(MAKE) -C $(EDK2_PATH)/BaseTools clean
	$(MAKE) -C $(EDK2_PATH)/BaseTools/Source/C/BrotliCompress/brotli clean
	$(MAKE) -C $(EDK2_PATH)/CryptoPkg/Library/MbedTlsLib/mbedtls clean
	$(MAKE) -C $(EDK2_PATH)/MdeModulePkg/Library/BrotliCustomDecompressLib/brotli clean
	rm -rf --no-preserve-root --one-file-system $(WORKSPACE)/Build

basetools:
	$(MAKE) -C $(EDK_TOOLS_PATH)

patched: scripts/patch.sh patches/*/*.patch
	bash scripts/patch.sh

build-edk2: patched
	mkdir -p $(WORKSPACE)
	source $(EDK2_PATH)/edksetup.sh && \
	build \
		-a $(EDK2_ARCH) \
		-t $(EDK2_TOOLCHAIN) \
		-b $(EDK2_TARGET) \
		-D DISABLE_NEW_DEPRECATED_INTERFACES=TRUE \
		-p embloader/embloader.dsc

qemu-x86_64: build-edk2
	qemu-system-x86_64 \
		-enable-kvm \
		-bios /usr/share/edk2/x64/OVMF.4m.fd \
		-display none \
		-serial stdio \
		-nic none \
		-kernel $(WORKSPACE)/Build/embloader/$(EDK2_TARGET)_$(EDK2_TOOLCHAIN)/X64/embloader.efi

qemu: qemu-$(ARCH)

.PHONY: all clean build-edk2 patched basetools qemu qemu-x86_64
