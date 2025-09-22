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
	void *ptr = NULL;
	size_t len = 0;
	if (!data || !info || !info->kernel)
		return EFI_INVALID_PARAMETER;
	log_info("loading kernel %s", info->kernel);
	status = efi_file_open_read_pages(
		info->root, info->kernel, &ptr, &len
	);
	if (EFI_ERROR(status)) {
		log_error(
			"load kernel %s failed: %s",
			info->kernel, efi_status_to_string(status)
		);
		return status;
	}
	log_info(
		"loaded kernel %s size %s (%" PRIu64 " bytes)",
		info->kernel,
		format_size_float(buff, len),
		(uint64_t)len
	);
	data->kernel = ptr;
	data->kernel_size = len;
	log_info(
		"kernel %s at %p with %" PRIu64 " pages",
		info->kernel, data->kernel,
		(uint64_t)EFI_SIZE_TO_PAGES(len)
	);
	return EFI_SUCCESS;
}
