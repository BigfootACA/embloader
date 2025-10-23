#include <Library/UefiBootServicesTableLib.h>
#include <stdio.h>
#include "log.h"
#include "menu.h"
#include "embloader.h"
#include "efi-utils.h"

/**
 * @brief Check if the menu has any loader entries marked as complete.
 * This function determines whether there are any complete loader entries
 * available in the current menu that can be booted immediately.
 *
 * @return true if at least one complete loader exists, false otherwise
 */
bool embloader_menu_is_complete() {
	embloader_menu *menu = g_embloader.menu;
	list *p;
	if (!menu) return false;
	if ((p = list_first(menu->loaders))) do {
		LIST_DATA_DECLARE(item, p, embloader_loader*);
		if (!item) continue;
		if (item->complete) return true;
	} while ((p = p->next));
	return false;
}

static bool loader_filter_profiles(confignode *node, const char *path, bool dir) {
	confignode_path_foreach(iter, node, path) {
		char *profile = confignode_value_get_string(iter.node, NULL, NULL);
		if (!profile) continue;
		bool have = list_search_string(g_embloader.profiles, profile) != NULL;
		free(profile);
		if (have != dir) return false;
	}
	return true;
}

static bool loader_is_valid(embloader_loader *loader) {
	if (!loader) return false;
	if (!loader->name || !loader->title) return false;
	if (loader->type == LOADER_NONE) return false;
	if (loader->type == LOADER_SDBOOT && !loader->extra) return false;
	if (loader->type == LOADER_EFISETUP && !embloader_is_efisetup_supported()) return false;
	if (loader->node) {
		confignode *match = confignode_path_lookup(loader->node, "match", false);
		if (match && !embloader_try_matches(match)) return false;
		if (!loader_filter_profiles(loader->node, "profiles.allowed", true)) return false;
		if (!loader_filter_profiles(loader->node, "profiles.disallowed", false)) return false;
	}
	return true;
}

/**
 * @brief Load a loader configuration from a config node.
 * This function parses a configuration node representing a boot loader entry
 * and creates an embloader_loader structure with the parsed information.
 *
 * @param node configuration node containing loader settings
 */
void embloader_load_loader(confignode *node) {
	if (!node || !confignode_is_type(node, CONFIGNODE_TYPE_MAP)) return;
	embloader_loader *loader = malloc(sizeof(embloader_loader));
	if (!loader) return;
	memset(loader, 0, sizeof(embloader_loader));
	loader->name = strdup(confignode_get_key(node));
	loader->title = confignode_path_get_string(node, "title", NULL, NULL);
	const char *type_str = confignode_path_get_string(node, "type", NULL, NULL);
	loader->type = embloader_menu_get_loader_type(type_str);
	loader->complete = confignode_path_get_bool(node, "complete", true, NULL);
	int count = g_embloader.menu->loaders ? list_count(g_embloader.menu->loaders) : 0;
	loader->priority = confignode_path_get_int(node, "priority", count + 1, NULL);
	loader->node = node;
	if (!loader_is_valid(loader)) {
		if (loader->name) free(loader->name);
		if (loader->title) free(loader->title);
		free(loader);
		return;
	}
	if (loader->type == LOADER_EFI) {
		loader->bootargs = confignode_path_get_string_or_list(node, "cmdline", NULL, NULL);
	} else if (loader->type == LOADER_LINUX_EFI) {
		loader->bootargs = confignode_path_get_string_or_list(node, "bootargs", NULL, NULL);
	}
	list_obj_add_new(&g_embloader.menu->loaders, loader);
}

static bool loaders_sorter(list *f1, list *f2){
	if (!f1 || !f2) return false;
	LIST_DATA_DECLARE(l1, f1, embloader_loader*);
	LIST_DATA_DECLARE(l2, f2, embloader_loader*);
	if (!l1 || !l2) return false;
	return l1->priority > l2->priority;
}

/**
 * @brief Sort menu loaders by their priority values.
 * This function sorts all loader entries in the menu according to their
 * priority values, with higher priority entries appearing first.
 */
void embloader_sort_menu_loaders() {
	embloader_menu *menu = g_embloader.menu;
	if (!menu) return;
	list_sort(menu->loaders, loaders_sorter);
}

/**
 * @brief Load and initialize the boot menu from configuration.
 * This function reads menu settings and loader configurations from the
 * global configuration, creates the menu structure, and loads all loaders.
 */
void embloader_load_menu() {
	embloader_menu *menu = g_embloader.menu;
	confignode *cfg = g_embloader.config;
	if (!menu) {
		menu = malloc(sizeof(embloader_menu));
		if (!menu) return;
		memset(menu, 0, sizeof(embloader_menu));
		g_embloader.menu = menu;
	}
	if (menu->default_entry)
		free(menu->default_entry);
	menu->timeout = confignode_path_get_int(cfg, "menu.timeout", 10, NULL);
	menu->save_default = confignode_path_get_bool(cfg, "menu.save-default", false, NULL);
	menu->default_entry = confignode_path_get_string(cfg, "menu.default", NULL, NULL);
	menu->title = confignode_path_get_string(cfg, "menu.title", "Embedded Bootloader", NULL);
	confignode_path_foreach(iter, g_embloader.config, "loaders")
		embloader_load_loader(iter.node);
	embloader_sort_menu_loaders();
}

/**
 * @brief Start the appropriate menu interface based on configuration.
 * This function dispatches to the correct menu implementation (text, TUI, HII, or GUI)
 * based on the configured menu type and handles user selection.
 *
 * @param selected pointer to receive the selected loader entry
 * @return EFI_SUCCESS if a loader was selected, EFI_INVALID_PARAMETER for invalid
 *         parameters, EFI_UNSUPPORTED for unknown menu types, or error from menu implementation
 */
EFI_STATUS embloader_menu_start(embloader_loader **selected, uint64_t *flags) {
	if (!selected) return EFI_INVALID_PARAMETER;
	*selected = NULL;
	EFI_STATUS status = EFI_UNSUPPORTED;
	char *menu = confignode_path_get_string(
		g_embloader.config, "menu.type", NULL, NULL
	);
	if (!menu) {
		status = embloader_gui_menu_start(selected, flags);
		if (status != EFI_UNSUPPORTED) return status;
		status = embloader_hii_menu_start(selected, flags);
		if (status != EFI_UNSUPPORTED) return status;
		status = embloader_tui_menu_start(selected, flags);
		if (status != EFI_UNSUPPORTED) return status;
		status = embloader_text_menu_start(selected, flags);
		if (status != EFI_UNSUPPORTED) return status;
		log_warning("no supported menu types available");
		return EFI_UNSUPPORTED;
	}
	if (strcasecmp(menu, "text") == 0)
		status = embloader_text_menu_start(selected, flags);
	else if (strcasecmp(menu, "tui") == 0)
		status = embloader_tui_menu_start(selected, flags);
	else if (strcasecmp(menu, "hii") == 0)
		status = embloader_hii_menu_start(selected, flags);
	else if (strcasecmp(menu, "gui") == 0)
		status = embloader_gui_menu_start(selected, flags);
	else log_warning("unknown menu type %s", menu);
	if (menu) free(menu);
	return status;
}

/**
 * @brief Display and handle the main boot menu loop.
 * This function implements the main menu loop, handling menu display,
 * user selection, and boot attempts. Continues until successful boot
 * or preparation phase completion.
 *
 * @return EFI_SUCCESS on successful boot, EFI_NOT_STARTED if preparation needed,
 *         or error status from menu operations
 */
EFI_STATUS embloader_show_menu() {
	EFI_STATUS status;
	while (true) {
		embloader_load_ktype();
		if (embloader_menu_is_complete()) {
			uint64_t flags = 0;
			embloader_loader *loader = NULL;
			status = embloader_menu_start(&loader, &flags);
			if (EFI_ERROR(status)) return status;
			if (!loader) continue;
			status = embloader_loader_boot(loader);
			printf("Press any key to continue...\n");
			efi_wait_any_key(gST->ConIn);
		} else {
			status = embloader_prepare_boot();
			if (EFI_ERROR(status)) return status;
			return EFI_NOT_STARTED;
		}
	}
	return EFI_NOT_STARTED;
}
