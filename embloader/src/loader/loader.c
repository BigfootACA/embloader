#include "loader.h"
#include "efi-utils.h"
#include "log.h"
#include <Library/UefiBootServicesTableLib.h>

/**
 * @brief Boot a loader using the appropriate method based on its type.
 * This function dispatches the boot process to the correct implementation
 * based on the loader type and handles common preparation steps.
 *
 * @param loader pointer to the loader configuration to boot
 * @return EFI_SUCCESS on successful boot, EFI_INVALID_PARAMETER for invalid
 *         parameters, or specific error codes from the boot implementation
 */
EFI_STATUS embloader_loader_boot(embloader_loader *loader) {
	bool found = false;
	EFI_STATUS status = EFI_UNSUPPORTED;
	if (!loader || !loader->name) return EFI_INVALID_PARAMETER;
	if (!loader->title) log_info("booting %s ...", loader->name);
	else log_info("booting %s (%s) ...", loader->title, loader->name);
	if (loader->node) {
		char *ktype = confignode_path_get_string(
			loader->node, "ktype", NULL, NULL
		);
		if (ktype) {
			log_debug("loader use ktype %s", ktype);
			if (g_embloader.ktype) free(g_embloader.ktype);
			g_embloader.ktype = ktype;
		}
	}
	gBS->SetWatchdogTimer(5 * 60, 0, 0, NULL);
	for (int i = 0; embloader_loader_funcs[i].name; i++) {
		if (!embloader_loader_funcs[i].func) continue;
		if (loader->type != embloader_loader_funcs[i].type) continue;
		status = embloader_loader_funcs[i].func(loader);
		found = true;
		break;
	}
	gBS->SetWatchdogTimer(0, 0, 0, NULL);
	if (!found) {
		log_warning("unknown loader type %d", loader->type);
	} else if (EFI_ERROR(status)) {
		log_error("boot %s failed: %s", loader->name, efi_status_to_string(status));
	}
	return status;
}

/**
 * @brief Convert loader type string to enumeration value.
 * This function parses a string representation of a loader type and returns
 * the corresponding enumeration value.
 *
 * @param type_str string representation of the loader type (case-insensitive)
 * @return corresponding embloader_loader_type value, or LOADER_NONE if unknown
 */
embloader_loader_type embloader_menu_get_loader_type(const char *type_str) {
	for (int i = 0; embloader_loader_funcs[i].name; i++)
		if (strcasecmp(type_str, embloader_loader_funcs[i].name) == 0)
			return embloader_loader_funcs[i].type;
	return LOADER_NONE;
}

/**
 * @brief Find a loader by its name in the global menu.
 * This function searches the global embloader menu for a loader entry
 * matching the specified name (case-insensitive).
 *
 * @param name the name of the loader to find (must not be NULL)
 * @return pointer to the found embloader_loader, or NULL if not found
 */
embloader_loader *embloader_find_loader(const char *name) {
	list *p;
	if (!name ||!g_embloader.menu) return NULL;
	if ((p = list_first(g_embloader.menu->loaders))) do {
		LIST_DATA_DECLARE(loader, p, embloader_loader*);
		if (
			loader && name && loader->name &&
			strcasecmp(name, loader->name) == 0
		) return loader;
	} while ((p = p->next));
	return NULL;
}
