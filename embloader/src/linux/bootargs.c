#include "linuxboot.h"
#include "log.h"

static bool append_config_bootargs(
	list **bootargs,
	confignode *node,
	const char *path
) {
	confignode *configured = confignode_path_lookup(node, path, false);
	if (!configured) return true;
	bool ok = false;
	list *values = confignode_value_get_string_or_list_to_list(configured, &ok);
	if (!ok) return false;
	if (values && list_obj_add(bootargs, values) != 0) {
		list_free_all_def(values);
		return false;
	}
	return true;
}

/**
 * @brief Prepare boot arguments string from configuration and optional default list
 *
 * This function collects boot arguments from multiple sources in the following order:
 * 1. Profile-specific bootargs from configuration profiles (in reverse order)
 * 2. Global bootargs from menu configuration
 * 3. Optional default bootargs list passed as parameter
 * All arguments are combined into a single space-separated string.
 *
 * @param def_bootargs Optional list of default boot arguments to append, or NULL
 * @return Pointer to newly allocated bootargs string, or NULL on error
 *         Caller is responsible for freeing the returned string
 */
char* linux_prepare_bootargs(list *def_bootargs) {
	list *p;
	list *bootargs = NULL;
	confignode *np = confignode_map_get(g_embloader.config, "profiles");
	if (np && (p = list_last(g_embloader.profiles))) do {
		LIST_DATA_DECLARE(profile, p, char*);
		if (!profile) continue;
		confignode *cp = confignode_map_get(np, profile);
		if (!cp) continue;
		if (!append_config_bootargs(&bootargs, cp, "bootargs")) goto fail;
	} while ((p = p->prev));
	if (!append_config_bootargs(
		&bootargs, g_embloader.config, "menu.bootargs"
	)) goto fail;
	if (def_bootargs) {
		list *n = list_duplicate_chars(def_bootargs, NULL);
		if (!n) goto fail;
		if (list_obj_add(&bootargs, n) != 0) {
			list_free_all_def(n);
			goto fail;
		}
	}
	if (!bootargs) return strdup("");
	char *result = list_to_string(bootargs, " ");
	if (!result) {
		bool have_value = false;
		if ((p = list_first(bootargs))) do {
			LIST_DATA_DECLARE(value, p, char*);
			if (value && value[0]) {
				have_value = true;
				break;
			}
		} while ((p = p->next));
		if (!have_value) result = strdup("");
	}
	list_free_all_def(bootargs);
	return result;
fail:
	if (bootargs) list_free_all_def(bootargs);
	return NULL;
}

/**
 * @brief Prepare boot arguments using an editable entry-specific string
 *
 * @param def_bootargs Entry-specific boot arguments, or NULL
 * @return Newly allocated combined boot arguments, or NULL on allocation failure
 */
char* linux_prepare_bootargs_string(const char *def_bootargs) {
	list *bootargs = NULL;
	if (def_bootargs) {
		bootargs = list_new_strdup(def_bootargs);
		if (!bootargs) return NULL;
	}
	char *result = linux_prepare_bootargs(bootargs);
	if (bootargs) list_free_all_def(bootargs);
	return result;
}

/**
 * @brief Prepare boot arguments string for Linux kernel from bootinfo structure
 *
 * This is a convenience wrapper around linux_prepare_bootargs() that extracts
 * the bootargs list from a linux_bootinfo structure. The function collects
 * arguments from configuration profiles, menu settings, and the bootinfo's
 * bootargs list.
 *
 * @param info Pointer to linux_bootinfo structure containing boot configuration,
 *             or NULL to only use configuration-based arguments
 * @return Pointer to newly allocated bootargs string, or NULL on error
 *         Caller is responsible for freeing the returned string
 */
char* linux_bootinfo_prepare_bootargs(linux_bootinfo *info) {
	return linux_prepare_bootargs(info ? info->bootargs : NULL);
}

/**
 * @brief Load and prepare boot arguments for Linux kernel
 *
 * This function loads boot arguments into the linux_data structure. If the bootinfo
 * contains a bootargs_override string, it is used directly. Otherwise, arguments are
 * collected from configuration profiles, menu settings, and the bootinfo's bootargs list.
 * The resulting string is stored in data->bootargs, replacing any existing value.
 *
 * @param data Pointer to linux_data structure to store the boot arguments
 * @param info Pointer to linux_bootinfo structure containing boot configuration
 * @return EFI_SUCCESS on success
 *         EFI_INVALID_PARAMETER if data or info is NULL
 *         EFI_OUT_OF_RESOURCES if memory allocation fails
 */
EFI_STATUS linux_load_bootargs(linux_data *data, linux_bootinfo *info) {
	char *bootargs;
	if (!data || !info) return EFI_INVALID_PARAMETER;
	bootargs = info->bootargs_override ?
		strdup(info->bootargs_override) :
		linux_bootinfo_prepare_bootargs(info);
	if (!bootargs) return EFI_OUT_OF_RESOURCES;
	if (data->bootargs) free(data->bootargs), data->bootargs = NULL;
	data->bootargs = bootargs;
	log_info("loaded bootargs %s", data->bootargs);
	return EFI_SUCCESS;
}
