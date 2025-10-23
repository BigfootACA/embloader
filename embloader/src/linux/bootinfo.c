#include "linuxboot.h"
#include "str-utils.h"

/**
 * @brief Parse Linux boot information from configuration node
 *
 * This function parses a configuration node to extract Linux boot information
 * including kernel path, initramfs files, boot arguments, and device tree overlays.
 *
 * @param node Pointer to confignode containing Linux boot configuration
 * @return linux_bootinfo* Pointer to parsed linux_bootinfo structure, or NULL on failure
 */
linux_bootinfo* linux_bootinfo_parse(confignode *node) {
	list *q;
	linux_bootinfo *info = malloc(sizeof(linux_bootinfo));
	if (!info) return NULL;
	memset(info, 0, sizeof(linux_bootinfo));
	info->kernel = confignode_path_get_string(node, "kernel", NULL, NULL);
	if ((q = confignode_path_get_string_or_list_to_list(
		node, "initramfs", NULL
	))) list_obj_add(&info->initramfs, q);
	if ((q = confignode_path_get_string_or_list_to_list(
		node, "bootargs", NULL
	))) list_obj_add(&info->bootargs, q);
	confignode_path_foreach(iter3, node, "overlays") {
		linux_overlay ovl;
		memset(&ovl, 0, sizeof(ovl));
		if (confignode_is_type(iter3.node, CONFIGNODE_TYPE_VALUE)) {
			ovl.path = confignode_value_get_string(iter3.node, NULL, NULL);
		} else if (confignode_is_type(iter3.node, CONFIGNODE_TYPE_MAP)) {
			confignode *params = confignode_path_lookup(iter3.node, "params", false);
			const char *name = confignode_get_key(iter3.node);
			ovl.path = confignode_path_get_string(iter3.node, "path", name, NULL);
			ovl.params = confignode_copy(params);
		} else continue;
		list_obj_add_new_dup(&info->dtoverlay, &ovl, sizeof(ovl));
	}
	info->devicetree = confignode_path_get_string(node, "devicetree", NULL, NULL);
	return info;
}

static int dtoverlay_free(void *d) {
	linux_overlay *ovl = d;
	if (!ovl) return -1;
	if (ovl->path) free(ovl->path);
	if (ovl->params) confignode_clean(ovl->params);
	memset(ovl, 0, sizeof(linux_overlay));
	free(ovl);
	return 0;
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
