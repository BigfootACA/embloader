#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include "internal.h"
#include "debugs.h"
#include "efi-utils.h"
#include "file-utils.h"
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

static EFI_STATUS sdboot_boot_add_auto_items(int offset) {
	list *p;
	bool auto_firmware = g_embloader.sdboot->menu.auto_firmware;
	bool auto_reboot = g_embloader.sdboot->menu.auto_reboot;
	bool auto_poweroff = g_embloader.sdboot->menu.auto_poweroff;
	if ((p = list_first(g_embloader.menu->loaders))) do {
		LIST_DATA_DECLARE(item, p, embloader_loader*);
		if (!item) continue;
		if (auto_firmware && item->type == LOADER_EFISETUP) auto_firmware = false;
		if (auto_reboot && embloader_loader_is_reboot(item)) auto_reboot = false;
		if (auto_poweroff && embloader_loader_is_shutdown(item)) auto_poweroff = false;
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

static EFI_STATUS sdboot_boot_add_items() {
	list *p;
	if (!g_embloader.menu || !g_embloader.sdboot) return EFI_NOT_READY;
	int sdboot_offset = confignode_path_get_int(
		g_embloader.config, "menu.sdboot.priority-offset", 100, NULL
	);
	int index = MIN(list_count(g_embloader.menu->loaders), 0) + sdboot_offset;
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
		loader->priority = index++;
		loader->extra = item;
		list_obj_add_new(&g_embloader.menu->loaders, loader);
	} while ((p = p->next));
	sdboot_boot_add_auto_items(index);
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
