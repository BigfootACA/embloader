#include "linuxboot.h"
#include "log.h"

/**
 * @brief Load and prepare boot arguments for Linux kernel
 *
 * This function loads boot arguments from the configuration, processes profile-specific
 * arguments, and combines them into a single bootargs string for the kernel.
 *
 * @param data Pointer to linux_data structure to store the boot arguments
 * @param info Pointer to linux_bootinfo structure containing boot configuration
 * @return EFI_STATUS Returns EFI_SUCCESS on success, or appropriate error code on failure
 */
EFI_STATUS linux_load_bootargs(linux_data *data, linux_bootinfo *info) {
	list *p;
	if (!data || !info) return EFI_INVALID_PARAMETER;
	list *bootargs = NULL;
	confignode *np = confignode_map_get(g_embloader.config, "profiles");
	if (np && (p = list_last(g_embloader.profiles))) do {
		LIST_DATA_DECLARE(profile, p, char*);
		if (!profile) continue;
		confignode *cp = confignode_map_get(np, profile);
		if (!cp) continue;
		confignode_path_foreach(iter, cp, "bootargs") {
			if (!confignode_is_type(iter.node, CONFIGNODE_TYPE_VALUE)) continue;
			char *arg = confignode_value_get_string(iter.node, NULL, NULL);
			if (arg) list_obj_add_new(&bootargs, arg);
		}
	} while ((p = p->prev));
	confignode_path_foreach(iter, g_embloader.config, "menu.bootargs") {
		if (!confignode_is_type(iter.node, CONFIGNODE_TYPE_VALUE)) continue;
		char *arg = confignode_value_get_string(iter.node, NULL, NULL);
		if (arg) list_obj_add_new(&bootargs, arg);
	}
	if (info->bootargs) {
		list *n = list_duplicate_chars(info->bootargs, NULL);
		if (n) list_obj_add(&bootargs, n);
	}
	if (data->bootargs) free(data->bootargs), data->bootargs = NULL;
	data->bootargs = list_to_string(bootargs, " ");
	log_info("loaded bootargs %s", data->bootargs);
	return EFI_SUCCESS;
}
