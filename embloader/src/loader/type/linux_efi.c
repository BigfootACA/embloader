#include "../loader.h"
#include "linuxboot.h"
#include "log.h"

/**
 * @brief Boot Linux kernel using EFI stub loader.
 * This function parses Linux boot configuration, loads the kernel and initrd,
 * then boots using the EFI stub method with proper kernel parameters.
 *
 * @param loader pointer to the loader configuration containing Linux boot info
 * @return EFI_SUCCESS on successful boot, EFI_INVALID_PARAMETER for invalid
 *         parameters, EFI_LOAD_ERROR if boot data parsing or loading fails
 */
EFI_STATUS embloader_loader_boot_linux_efi(embloader_loader *loader) {
	if (!loader || !loader->node) return EFI_INVALID_PARAMETER;
	linux_bootinfo *info = linux_bootinfo_parse(loader->node);
	if (!info) {
		log_error("failed to parse linux-efi boot info");
		return EFI_LOAD_ERROR;
	}
	info->root = g_embloader.dir.root;
	linux_data *data = linux_data_load(info);
	if (!data) {
		linux_bootinfo_clean(info);
		log_error("failed to load linux-efi data");
		return EFI_LOAD_ERROR;
	}
	linux_bootinfo_clean(info);
	if (!data->kernel || data->kernel_size == 0) {
		linux_data_clean(data);
		log_error("no kernel loaded for linux-efi");
		return EFI_LOAD_ERROR;
	}
	EFI_STATUS status = linux_boot_use_efi(data);
	linux_data_clean(data);
	return status;
}
