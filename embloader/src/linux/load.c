#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include "linuxboot.h"
#include "log.h"

/**
 * @brief Load Linux boot data from boot information
 *
 * This function loads all necessary Linux boot components including kernel,
 * initramfs, device tree, and boot arguments based on the provided boot information.
 *
 * @param info Pointer to linux_bootinfo structure containing boot configuration
 * @return linux_data* Pointer to loaded linux_data structure, or NULL on failure
 */
linux_data* linux_data_load(linux_bootinfo *info) {
	if (!info || !info->root) return NULL;
	if (!info->kernel) {
		log_error("no kernel specified");
		return NULL;
	}
	linux_data *data = malloc(sizeof(linux_data));
	if (!data) return NULL;
	memset(data, 0, sizeof(linux_data));
	if (EFI_ERROR(linux_load_kernel(data, info))) goto fail;
	if (EFI_ERROR(linux_load_initramfs(data, info))) goto fail;
	if (EFI_ERROR(linux_load_bootargs(data, info))) goto fail;
	if (EFI_ERROR(linux_load_devicetree(data, info))) goto fail;
	if (EFI_ERROR(linux_load_dtoverlay(data, info))) {
		if (confignode_path_get_bool(
			g_embloader.config,
			"devicetree.skip-overlays-error",
			true, NULL
		)) goto fail;
		log_warning("continue boot without applying dtbo");
	}
	log_debug("linux boot data prepared");
	return data;
fail:
	linux_data_clean(data);
	return NULL;
}

/**
 * @brief Clean up and free a linux_data structure
 *
 * This function frees all allocated memory within a linux_data structure
 * including kernel pages, initramfs pages, boot arguments, and device tree.
 *
 * @param data Pointer to linux_data structure to be cleaned up
 */
void linux_data_clean(linux_data *data) {
	if (!data) return;
	if (data->kernel) gBS->FreePages(
		(UINTN) data->kernel,
		EFI_SIZE_TO_PAGES(data->kernel_size)
	);
	if (data->initramfs) gBS->FreePages(
		(UINTN) data->initramfs,
		EFI_SIZE_TO_PAGES(data->initramfs_size)
	);
	if (data->fdt) free(data->fdt);
	if (data->bootargs) free(data->bootargs);
	memset(data, 0, sizeof(linux_data));
	free(data);
}
