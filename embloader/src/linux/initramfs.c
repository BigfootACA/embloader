#include <Uefi.h>
#include <Protocol/LoadFile2.h>
#include <Protocol/DevicePath.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <inttypes.h>
#include "linuxboot.h"
#include "efi-utils.h"
#include "file-utils.h"
#include "readable.h"
#include "log.h"

#define LINUX_INITRD_MEDIA_GUID \
	{ 0x5568e427, 0x68fc, 0x4f3d, { 0xac, 0x74, 0xca, 0x55, 0x52, 0x31, 0xcc, 0x68 } }

struct initrd_loader {
	EFI_LOAD_FILE2_PROTOCOL load;
	const void *address;
	size_t length;
};

static const struct {
	VENDOR_DEVICE_PATH vendor;
	EFI_DEVICE_PATH end;
} __attribute__((packed)) initrd_dp = {
	.vendor = {
		.Header = {
			.Type = MEDIA_DEVICE_PATH,
			.SubType = MEDIA_VENDOR_DP,
			.Length = {(UINT8)(sizeof(initrd_dp.vendor)), 0},
		},
		.Guid = LINUX_INITRD_MEDIA_GUID,
	},
	.end = {
		.Type = END_DEVICE_PATH_TYPE,
		.SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE,
		.Length = { (UINT8)(sizeof(initrd_dp.end)), 0},
	}
};

static EFIAPI EFI_STATUS initrd_load_file(
	EFI_LOAD_FILE2_PROTOCOL *this,
	EFI_DEVICE_PATH *path,
	BOOLEAN policy,
	UINTN *size,
	void *buffer
) {
	struct initrd_loader *loader;
	if (!this || !size || !path) return EFI_INVALID_PARAMETER;
	if (policy) return EFI_UNSUPPORTED;
	loader = BASE_CR(this, struct initrd_loader, load);
	if (loader->length == 0 || !loader->address) return EFI_NOT_FOUND;
	if (!buffer || *size < loader->length) {
		*size = loader->length;
		return EFI_BUFFER_TOO_SMALL;
	}
	memcpy(buffer, loader->address, loader->length);
	*size = loader->length;
	return EFI_SUCCESS;
}

/**
 * @brief Register initramfs as EFI LoadFile2 protocol
 *
 * This function registers an initramfs image as an EFI LoadFile2 protocol
 * so it can be loaded by the Linux kernel during boot.
 *
 * @param ptr Pointer to the initramfs data in memory
 * @param size Size of the initramfs data in bytes
 * @param hand Pointer to store the created EFI handle
 * @return EFI_STATUS Returns EFI_SUCCESS on success, or appropriate error code on failure
 */
EFI_STATUS linux_initramfs_register(const void *ptr, size_t size, EFI_HANDLE *hand) {
	EFI_STATUS status;
	EFI_DEVICE_PATH *dp = (EFI_DEVICE_PATH *) &initrd_dp;
	EFI_HANDLE handle;
	struct initrd_loader *loader;
	if (!ptr || size == 0) return EFI_INVALID_PARAMETER;
	status = gBS->LocateDevicePath(&gEfiLoadFile2ProtocolGuid, &dp, &handle);
	if (status != EFI_NOT_FOUND) return EFI_ALREADY_STARTED;
	loader = malloc(sizeof(struct initrd_loader));
	if (!loader) return EFI_OUT_OF_RESOURCES;
	loader->load.LoadFile = initrd_load_file;
	loader->address = ptr;
	loader->length = size;
	status = gBS->InstallMultipleProtocolInterfaces(
		hand,
		&gEfiDevicePathProtocolGuid, &initrd_dp,
		&gEfiLoadFile2ProtocolGuid, loader,
		NULL
	);
	if (EFI_ERROR(status)) free(loader);
	return status;
}

/**
 * @brief Unregister previously registered initramfs
 *
 * This function unregisters a previously registered initramfs LoadFile2 protocol
 * and frees associated resources.
 *
 * @param hand EFI handle of the registered initramfs
 * @return EFI_STATUS Returns EFI_SUCCESS on success, or appropriate error code on failure
 */
EFI_STATUS linux_initramfs_unregister(EFI_HANDLE hand) {
	EFI_STATUS status;
	struct initrd_loader *loader;
	if (!hand) return EFI_SUCCESS;
	status = gBS->HandleProtocol(
		hand, &gEfiLoadFile2ProtocolGuid, (void **) &loader
	);
	if (EFI_ERROR(status)) return status;
	status = gBS->UninstallMultipleProtocolInterfaces(
		hand,
		&gEfiDevicePathProtocolGuid, &initrd_dp,
		&gEfiLoadFile2ProtocolGuid, loader,
		NULL
	);
	if (EFI_ERROR(status)) return status;
	free(loader);
	return EFI_SUCCESS;
}

struct initramfs_file {
	char *name;
	void *data;
	size_t size;
};

static int free_initramfs_file(void *p) {
	struct initramfs_file *f = (struct initramfs_file*)p;
	if (!f) return -EINVAL;
	if (f->name) free(f->name);
	if (f->data) FreePages(f->data, EFI_SIZE_TO_PAGES(f->size));
	memset(f, 0, sizeof(*f));
	free(f);
	return 0;
}

/**
 * @brief Load initramfs files for Linux boot
 *
 * This function loads and combines multiple initramfs files into a single
 * initramfs archive for Linux kernel boot.
 *
 * @param data Pointer to linux_data structure to store the combined initramfs
 * @param info Pointer to linux_bootinfo structure containing initramfs file list
 * @return EFI_STATUS Returns EFI_SUCCESS on success, or appropriate error code on failure
 */
EFI_STATUS linux_load_initramfs(linux_data *data, linux_bootinfo *info) {
	EFI_STATUS status;
	char buff[64];
	void *ptr = NULL, *pages = NULL;
	size_t len = 0, pcnt = 0, total_len = 0;
	list *ptrs = NULL, *p;
	if (!data || !info) return EFI_INVALID_PARAMETER;
	if ((p = list_first(info->initramfs))) do {
		LIST_DATA_DECLARE(initramfs, p, char*);
		if (!initramfs) continue;
		ptr = NULL, len = 0;
		log_info("loading initramfs %s", initramfs);
		status = efi_file_open_read_pages(
			info->root, initramfs, &ptr, &len
		);
		if (EFI_ERROR(status)) {
			log_error(
				"load initramfs %s failed: %s",
				initramfs, efi_status_to_string(status)
			);
			goto fail;
		}
		log_info(
			"loaded initramfs %s size %s (%" PRIu64 " bytes)",
			initramfs, format_size_float(buff, len), (uint64_t)len
		);
		struct initramfs_file file = {
			.name = strdup(initramfs),
			.data = ptr, .size = len,
		};
		list_obj_add_new_dup(&ptrs, &file, sizeof(file));
		total_len += len, ptr = NULL, len = 0;
	} while ((p = p->next));
	if (!ptrs) {
		log_debug("no initramfs loaded");
		return EFI_SUCCESS;
	}
	log_info(
		"loaded %d initramfs size %s (%" PRIu64 " bytes)",
		list_count(ptrs),
		format_size_float(buff, total_len),
		(uint64_t)total_len
	);
	pages = (void*) UINT32_MAX;
	pcnt = EFI_SIZE_TO_PAGES(total_len);
	status = gBS->AllocatePages(
		AllocateMaxAddress, EfiLoaderData,
		pcnt, (EFI_PHYSICAL_ADDRESS*)&pages
	);
	if (EFI_ERROR(status)) {
		pages = NULL, pcnt = 0;
		log_error(
			"alloc pages for initramfs %s failed: %s",
			info->initramfs, efi_status_to_string(status)
		);
		goto fail;
	}
	int cnt = 0;
	size_t offset = 0;
	if ((p = list_first(ptrs))) do {
		LIST_DATA_DECLARE(file, p, struct initramfs_file*);
		if (!file || !file->data || file->size == 0) continue;
		if (offset + file->size > total_len) {
			log_error("initramfs %s size overflow at %" PRIu64, file->name, offset);
			goto fail;
		}
		memcpy(pages + offset, file->data, file->size);
		offset += file->size, cnt++;
	} while ((p = p->next));
	list_free_all(ptrs, free_initramfs_file);
	data->initramfs = pages;
	data->initramfs_size = total_len;
	log_info(
		"initramfs collections (%d) at %p with %" PRIu64 " pages",
		cnt, data->initramfs, (uint64_t)pcnt
	);
	return EFI_SUCCESS;
fail:
	if (pages) gBS->FreePages((UINTN)pages, pcnt);
	list_free_all(ptrs, free_initramfs_file);
	return status;
}
