#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include "internal.h"
#include "debugs.h"
#include "efi-utils.h"
#include "file-utils.h"
#include "str-utils.h"
#include "log.h"

static bool sdboot_check_fs(embloader_dir *dir) {
	EFI_STATUS status;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
	if (!dir) return false;
	dir->root = NULL;
	status = gBS->HandleProtocol(
		dir->handle,
		&gEfiSimpleFileSystemProtocolGuid,
		(VOID**) &fs
	);
	if (EFI_ERROR(status) || !fs) return false;
	status = fs->OpenVolume(fs, &dir->root);
	if (EFI_ERROR(status) || !dir->root) return false;
	if (!efi_folder_exists(dir->root, "/loader")) {
		dir->root->Close(dir->root);
		dir->root = NULL;
		return false;
	}
	dir->dp = efi_device_path_from_handle(dir->handle);
	char *dpt = efi_device_path_to_text(dir->dp);
	if (dpt) {
		log_info("Found systemd-boot folder in %s", dpt);
		free(dpt);
	} else log_info("Found systemd-boot folder (handle: %p)", dir->handle);
	return true;
}

static bool menu_loader_compare(list *l, void *d) {
	if (!l || !d) return false;
	LIST_DATA_DECLARE(el, l, embloader_loader*);
	sdboot_boot_loader *m = d;
	if (!el || !m) return false;
	if (el->type != LOADER_SDBOOT) return false;
	return el->extra == m;
}

static EFI_STATUS sdboot_boot_add_auto_items() {
	list *p;
	int offset = 0;
	bool auto_firmware = g_embloader.sdboot->menu.auto_firmware;
	bool auto_reboot = g_embloader.sdboot->menu.auto_reboot;
	bool auto_poweroff = g_embloader.sdboot->menu.auto_poweroff;
	if ((p = list_first(g_embloader.menu->loaders))) do {
		LIST_DATA_DECLARE(item, p, embloader_loader*);
		if (!item) continue;
		if (auto_firmware && item->type == LOADER_EFISETUP) auto_firmware = false;
		if (auto_reboot && embloader_loader_is_reboot(item)) auto_reboot = false;
		if (auto_poweroff && embloader_loader_is_shutdown(item)) auto_poweroff = false;
		if (item->type == LOADER_SDBOOT)
			offset = MAX(offset, item->priority + 1);
	} while ((p = p->next));
	embloader_loader loader;
	if (auto_firmware) {
		memset(&loader, 0, sizeof(embloader_loader));
		loader.name = strdup("sdboot-auto-firmware");
		loader.title = strdup("Reboot Into Firmware Interface");
		loader.type = LOADER_EFISETUP;
		loader.priority = offset++;
		list_obj_add_new_dup(&g_embloader.menu->loaders, &loader, sizeof(loader));
	}
	if (auto_poweroff) {
		memset(&loader, 0, sizeof(embloader_loader));
		loader.name = strdup("sdboot-auto-shutdown");
		loader.title = strdup("Shut Down The System");
		loader.type = LOADER_REBOOT;
		loader.priority = offset++;
		loader.node = confignode_new_map();
		confignode_path_set_string(loader.node, "action", "shutdown");
		list_obj_add_new_dup(&g_embloader.menu->loaders, &loader, sizeof(loader));
	}
	if (auto_reboot) {
		memset(&loader, 0, sizeof(embloader_loader));
		loader.name = strdup("sdboot-auto-poweroff");
		loader.title = strdup("Reboot The System");
		loader.type = LOADER_REBOOT;
		loader.priority = offset++;
		loader.node = confignode_new_map();
		confignode_path_set_string(loader.node, "action", "reboot");
		list_obj_add_new_dup(&g_embloader.menu->loaders, &loader, sizeof(loader));
	}
	return EFI_SUCCESS;
}

static int sdboot_compare_entries(embloader_loader *a, embloader_loader *b) {
	int cmp;
	if (!a || !b) return 0;
	if (a->type != LOADER_SDBOOT || b->type != LOADER_SDBOOT) return 0;
	sdboot_boot_loader *loader_a = a->extra;
	sdboot_boot_loader *loader_b = b->extra;
	if (!loader_a || !loader_b) return 0;
	bool has_sort_key_a = loader_a->sort_key && loader_a->sort_key[0];
	bool has_sort_key_b = loader_b->sort_key && loader_b->sort_key[0];
	if (has_sort_key_a && !has_sort_key_b) return -1;
	if (!has_sort_key_a && has_sort_key_b) return 1;
	if (has_sort_key_a && has_sort_key_b) {
		if ((cmp = strcmp(loader_a->sort_key, loader_b->sort_key)) != 0) return cmp;
		if (
			loader_a->machine_id && loader_b->machine_id &&
			(cmp = strcmp(loader_a->machine_id, loader_b->machine_id)) != 0
		) return cmp;
		if (
			loader_a->version && loader_b->version &&
			(cmp = vercmp(loader_b->version, loader_a->version)) != 0
		) return cmp;
	}
	if (loader_a->name && loader_b->name)
		return vercmp(loader_b->name, loader_a->name);
	return 0;
}

static void sdboot_items_sort() {
	list *p;
	int sdboot_offset = confignode_path_get_int(
		g_embloader.config, "menu.sdboot.priority-offset", 100, NULL
	);
	list *items = NULL;
	if ((p = list_first(g_embloader.menu->loaders))) do {
		LIST_DATA_DECLARE(item, p, embloader_loader*);
		if (!item || item->type != LOADER_SDBOOT) continue;
		list_obj_add_new(&items, item);
	} while ((p = p->next));
	list *pa, *pb;
	if ((pa = list_first(items))) do {
		LIST_DATA_DECLARE(item_a, pa, embloader_loader*);
		if (!item_a) continue;
		int priority = sdboot_offset;
		if ((pb = list_first(items))) do {
			LIST_DATA_DECLARE(item_b, pb, embloader_loader*);
			if (!item_b || item_a == item_b) continue;
			int cmp = sdboot_compare_entries(item_a, item_b);
			if (cmp > 0) priority++;
		} while ((pb = pb->next));
		item_a->priority = priority;
	} while ((pa = pa->next));
	list_free_all(items, NULL);
}

static EFI_STATUS sdboot_boot_add_items() {
	list *p;
	if (!g_embloader.menu || !g_embloader.sdboot) return EFI_NOT_READY;
	if ((p = list_first(g_embloader.sdboot->loaders))) do {
		LIST_DATA_DECLARE(item, p, sdboot_boot_loader*);
		if (!item || list_search_one(
			g_embloader.menu->loaders,
			menu_loader_compare, item
		)) continue;
		embloader_loader *loader;
		loader = malloc(sizeof(embloader_loader));
		if (!loader) continue;
		memset(loader, 0, sizeof(embloader_loader));
		loader->name = item->name ? strdup(item->name) : NULL;
		loader->title = item->title ? strdup(item->title) : NULL;
		loader->type = LOADER_SDBOOT;
		loader->complete = true;
		loader->extra = item;
		list_obj_add_new(&g_embloader.menu->loaders, loader);
	} while ((p = p->next));
	sdboot_items_sort();
	sdboot_boot_add_auto_items();
	embloader_sort_menu_loaders();
	return EFI_SUCCESS;
}

/**
 * @brief Load systemd-boot compatible configuration menus from all available filesystems
 *
 * Searches for /loader/loader.conf and /loader/entries/ config files across
 * multiple partitions if configured. Integrates found entries into global menu.
 *
 * @return EFI_SUCCESS if valid configs found,
 *         EFI_UNSUPPORTED if disabled,
 *         EFI_NOT_FOUND if no valid configs,
 *         other error codes on failure
 */
EFI_STATUS sdboot_boot_load_menu() {
	EFI_STATUS status;
	EFI_HANDLE *handles = NULL;
	embloader_dir dir;
	UINTN handle_count = 0;
	if (!confignode_path_get_bool(
		g_embloader.config, "menu.sdboot.enabled", true, NULL
	)) {
		log_info("systemd-boot menu disabled by config");
		return EFI_UNSUPPORTED;
	} else log_info("loading systemd-boot menu");
	sdboot_menu *menu = g_embloader.sdboot;
	if (!menu) {
		menu = sdboot_menu_new();
		if (!menu) return EFI_OUT_OF_RESOURCES;
		g_embloader.sdboot = menu;
	}
	bool have_success = false;
	bool use_multiple = confignode_path_get_bool(
		g_embloader.config, "menu.sdboot.multi-partitions", false, NULL
	);
	memset(&dir, 0, sizeof(dir));
	dir.handle = g_embloader.dir.handle;
	if (sdboot_check_fs(&dir)) {
		status = sdboot_boot_load_root(menu, &dir);
		if (!EFI_ERROR(status)) have_success = true;
	}
	if (have_success && !use_multiple) goto done;
	status = gBS->LocateHandleBuffer(
		ByProtocol,
		&gEfiSimpleFileSystemProtocolGuid,
		NULL, &handle_count, &handles
	);
	if (!EFI_ERROR(status)) for (UINTN i = 0; i < handle_count; i++) {
		dir.handle = handles[i];
		if (!sdboot_check_fs(&dir)) continue;
		status = sdboot_boot_load_root(menu, &dir);
		if (EFI_ERROR(status)) {
			dir.root->Close(dir.root);
			dir.root = NULL;
		} else have_success = true;
		if (have_success && !use_multiple) goto done;
	}
done:
	if (handles) FreePool(handles);
	if (!have_success) {
		log_warning("no valid systemd-boot configs found");
		return EFI_NOT_FOUND;
	}
	return sdboot_boot_add_items();
}
