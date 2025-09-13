#include <Library/UefiBootServicesTableLib.h>
#include <inttypes.h>
#include "linuxboot.h"
#include "efi-utils.h"
#include "file-utils.h"
#include "readable.h"
#include "log.h"

/**
 * @brief Load Linux kernel image from file
 *
 * This function loads a Linux kernel image from the specified file path,
 * allocates appropriate memory pages, and prepares it for execution.
 *
 * @param data Pointer to linux_data structure to store the loaded kernel
 * @param info Pointer to linux_bootinfo structure containing kernel file path
 * @return EFI_STATUS Returns EFI_SUCCESS on success, or appropriate error code on failure
 */
EFI_STATUS linux_load_kernel(linux_data *data, linux_bootinfo *info) {
	EFI_STATUS status;
	char buff[64];
	void *ptr = NULL, *pages = NULL;
	size_t len = 0, pcnt = 0;
	if (!data || !info || !info->kernel)
		return EFI_INVALID_PARAMETER;
	log_info("loading kernel %s", info->kernel);
	status = efi_file_open_read_all(
		info->root, info->kernel, &ptr, &len
	);
	if (EFI_ERROR(status)) {
		log_error(
			"load kernel %s failed: %s",
			info->kernel, efi_status_to_string(status)
		);
		goto fail;
	}
	log_info(
		"loaded kernel %s size %s (%" PRIu64 " bytes)",
		info->kernel,
		format_size_float(buff, len),
		(uint64_t)len
	);
	pages = (void*)UINT32_MAX;
	pcnt = EFI_SIZE_TO_PAGES(len);
	status = gBS->AllocatePages(
		AllocateMaxAddress, EfiLoaderData,
		pcnt, (UINTN*)&pages
	);
	if (EFI_ERROR(status)) {
		pages = NULL, pcnt = 0;
		log_error(
			"alloc pages for kernel %s failed: %s",
			info->kernel, efi_status_to_string(status)
		);
		goto fail;
	}
	memcpy(pages, ptr, len);
	free(ptr);
	data->kernel = pages;
	data->kernel_size = EFI_PAGES_TO_SIZE(pcnt);
	log_info(
		"kernel %s at %p with %" PRIu64 " pages",
		info->kernel, data->kernel, (uint64_t)pcnt
	);
	return EFI_SUCCESS;
fail:
	if (pages) gBS->FreePages((UINTN)pages, pcnt);
	if (ptr) free(ptr);
	return status;
}
