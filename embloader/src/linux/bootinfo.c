#include "linuxboot.h"
#include "str-utils.h"

static void dtoverlay_clean(linux_overlay *ovl) {
	if (!ovl) return;
	if (ovl->path) free(ovl->path);
	if (ovl->params) confignode_clean(ovl->params);
	memset(ovl, 0, sizeof(linux_overlay));
}

static int dtoverlay_free(void *d) {
	linux_overlay *ovl = d;
	if (!ovl) return -1;
	dtoverlay_clean(ovl);
	free(ovl);
	return 0;
}

static bool copy_optional_string(
	confignode *node,
	const char *path,
	char **out
) {
	bool ok = false;
	confignode *value = confignode_path_lookup(node, path, false);
	if (!value) return true;
	char *copy = confignode_value_get_string(value, NULL, &ok);
	if (!ok || !copy) {
		free(copy);
		return false;
	}
	*out = copy;
	return true;
}

static bool append_config_string(list **items, confignode *node) {
	bool ok = false;
	char *value = confignode_value_get_string(node, NULL, &ok);
	if (!ok || !value) {
		free(value);
		return false;
	}
	if (list_obj_add_new(items, value) != 0) {
		free(value);
		return false;
	}
	return true;
}

static bool copy_config_string_list(confignode *node, list **out) {
	list *items = NULL;
	if (confignode_is_type(node, CONFIGNODE_TYPE_VALUE)) {
		if (!append_config_string(&items, node)) return false;
	} else if (
		confignode_is_type(node, CONFIGNODE_TYPE_ARRAY) ||
		confignode_is_type(node, CONFIGNODE_TYPE_MAP)
	) {
		confignode_foreach(iter, node) {
			if (!confignode_is_type(iter.node, CONFIGNODE_TYPE_VALUE))
				continue;
			if (!append_config_string(&items, iter.node)) goto fail;
		}
	} else return false;
	*out = items;
	return true;
fail:
	if (items) list_free_all_def(items);
	return false;
}

static bool copy_optional_string_list(
	confignode *node,
	const char *path,
	list **out
) {
	list *items = NULL;
	confignode *value = confignode_path_lookup(node, path, false);
	if (!value) return true;
	if (!copy_config_string_list(value, &items)) return false;
	if (!items) return true;
	if (list_obj_add(out, items) != 0) {
		list_free_all_def(items);
		return false;
	}
	return true;
}

/**
 * @brief Parse Linux boot information from configuration node
 *
 * This function parses a configuration node to extract Linux boot information
 * including kernel path, initramfs files, boot arguments, and device tree overlays.
 *
 * @param node Pointer to confignode containing Linux boot configuration
 * @return linux_bootinfo* Pointer to parsed linux_bootinfo structure, or NULL on failure
 */
linux_bootinfo* linux_bootinfo_parse(embloader_loader *loader) {
	if (!loader) return NULL;
	confignode *node = loader->node;
	linux_bootinfo *info = malloc(sizeof(linux_bootinfo));
	if (!info) return NULL;
	memset(info, 0, sizeof(linux_bootinfo));
	info->loader = loader;
	if (!copy_optional_string(node, "kernel", &info->kernel)) goto fail;
	if (!copy_optional_string_list(
		node, "initramfs", &info->initramfs
	)) goto fail;
	if (!copy_optional_string_list(
		node, "bootargs", &info->bootargs
	)) goto fail;
	confignode_path_foreach(iter3, node, "overlays") {
		linux_overlay ovl;
		memset(&ovl, 0, sizeof(ovl));
		if (confignode_is_type(iter3.node, CONFIGNODE_TYPE_VALUE)) {
			bool ok = false;
			ovl.path = confignode_value_get_string(
				iter3.node, NULL, &ok
			);
			if (!ok || !ovl.path) {
				dtoverlay_clean(&ovl);
				goto fail;
			}
		} else if (confignode_is_type(iter3.node, CONFIGNODE_TYPE_MAP)) {
			bool ok = false;
			confignode *params = confignode_path_lookup(iter3.node, "params", false);
			confignode *path = confignode_path_lookup(iter3.node, "path", false);
			const char *name = confignode_get_key(iter3.node);
			if (path) {
				ovl.path = confignode_value_get_string(path, NULL, &ok);
				if (!ok || !ovl.path) goto fail;
			} else if (!name || !(ovl.path = strdup(name))) goto fail;
			if (params && !(ovl.params = confignode_copy(params))) {
				dtoverlay_clean(&ovl);
				goto fail;
			}
		} else continue;
		if (list_obj_add_new_dup(
			&info->dtoverlay, &ovl, sizeof(ovl)
		) != 0) {
			dtoverlay_clean(&ovl);
			goto fail;
		}
		memset(&ovl, 0, sizeof(ovl));
	}
	if (!copy_optional_string(
		node, "devicetree", &info->devicetree
	)) goto fail;
	return info;
fail:
	linux_bootinfo_clean(info);
	return NULL;
}

/**
 * @brief Clean up and free a linux_bootinfo structure
 *
 * This function frees all allocated memory within a linux_bootinfo structure
 * including kernel path, device tree, initramfs list, boot arguments, and overlays.
 *
 * @param info Pointer to linux_bootinfo structure to be cleaned up
 */
void linux_bootinfo_clean(linux_bootinfo *info) {
	if (!info) return;
	if (info->kernel) free(info->kernel);
	if (info->devicetree) free(info->devicetree);
	if (info->bootargs_override) free(info->bootargs_override);
	if (info->initramfs)
		list_free_all_def(info->initramfs);
	if (info->bootargs)
		list_free_all_def(info->bootargs);
	if (info->dtoverlay)
		list_free_all(info->dtoverlay, dtoverlay_free);
	memset(info, 0, sizeof(linux_bootinfo));
	free(info);
}
