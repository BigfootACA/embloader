#include <Library/MemoryAllocationLib.h>
#include "internal.h"
#include "log.h"
#include "efi-utils.h"
#include "linuxboot.h"

/**
 * @brief Convert a sdboot loader configuration to linux bootinfo structure.
 *
 * Translates systemd-boot loader entry to internal Linux boot parameters.
 * Memory management: returned structure must be freed with linux_bootinfo_clean().
 *
 * @param loader the sdboot boot loader to convert (must not be NULL)
 * @return newly allocated linux_bootinfo structure, or NULL on allocation failure
 */
linux_bootinfo* sdboot_boot_loader_to_linux_bootinfo(sdboot_boot_loader *loader) {
	list *p;
	linux_bootinfo *info = malloc(sizeof(linux_bootinfo));
	if (!info) return NULL;
	memset(info, 0, sizeof(linux_bootinfo));
	info->root = loader->root;
	if (loader->kernel) info->kernel = strdup(loader->kernel);
	if (loader->initramfs) info->initramfs = list_duplicate_chars(loader->initramfs, NULL);
	if (loader->options) info->bootargs = list_duplicate_chars(loader->options, NULL);
	if (loader->devicetree) info->devicetree = strdup(loader->devicetree);
	if (loader->dtoverlay && (p = list_first(loader->dtoverlay))) do {
		LIST_DATA_DECLARE(item, p, char*);
		if (!item) continue;
		linux_overlay ovl;
		memset(&ovl, 0, sizeof(ovl));
		ovl.path = strdup(item);
		list_obj_add_new_dup(&info->dtoverlay, &ovl, sizeof(ovl));
	} while ((p = p->next));
	return info;
}

/**
 * @brief Execute a sdboot loader entry
 *
 * Handles both EFI application loading and Linux kernel booting based on
 * the loader configuration. Updates global boot type information.
 *
 * @param menu the sdboot menu context (must not be NULL)
 * @param loader the boot loader entry to execute (must not be NULL and valid)
 * @return EFI_SUCCESS on successful boot initiation, error status otherwise
 */
EFI_STATUS sdboot_boot_loader_invoke(sdboot_menu *menu, sdboot_boot_loader *loader) {
	if (!menu || !loader) return EFI_INVALID_PARAMETER;
	if (!sdboot_boot_loader_check(loader)) return EFI_INVALID_PARAMETER;
	if (loader->version) {
		log_debug("sdboot loader use version %s as ktype", loader->version);
		if (g_embloader.ktype) free(g_embloader.ktype);
		g_embloader.ktype = strdup(loader->version);
	}
	if (loader->efi) {
		EFI_DEVICE_PATH_PROTOCOL *dp;
		dp = efi_device_path_from_handle(loader->root_handle);
		dp = efi_device_path_append_filepath(dp, loader->efi);
		if (!dp) {
			log_error("failed to create efi device path for %s", loader->efi);
			return EFI_LOAD_ERROR;
		}
		char *args = list_to_string(loader->options, " ");
		EFI_STATUS status = embloader_start_efi(dp, NULL, 0, args);
		if (args) free(args);
		FreePool(dp);
		return status;
	} else if (loader->kernel) {
		linux_bootinfo *info = sdboot_boot_loader_to_linux_bootinfo(loader);
		if (!info) {
			log_error("failed to convert sdboot loader to linux bootinfo");
			return EFI_OUT_OF_RESOURCES;
		}
		linux_data *data = linux_data_load(info);
		linux_bootinfo_clean(info);
		if (!data) {
			log_error("failed to load linux data from sdboot loader");
			return EFI_LOAD_ERROR;
		}
		EFI_STATUS status = linux_boot_use_efi(data);
		linux_data_clean(data);
		return status;
	} else {
		log_error("sdboot loader has neither efi nor kernel");
		return EFI_UNSUPPORTED;
	}
}
