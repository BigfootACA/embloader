#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <inttypes.h>
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

static int sdboot_boot_free_item(void *data) {
	embloader_loader *loader = data;
	if (!loader) return 0;
	if (loader->node) confignode_clean(loader->node);
	free(loader->bootargs);
	free(loader->title);
	free(loader->name);
	free(loader);
	return 0;
}

static EFI_STATUS sdboot_boot_add_auto_item(
	list **loaders,
	const char *name,
	const char *title,
	embloader_loader_type type,
	const char *mode,
	int64_t priority
) {
	embloader_loader *loader;
	if (!loaders) return EFI_INVALID_PARAMETER;
	loader = malloc(sizeof(embloader_loader));
	if (!loader) return EFI_OUT_OF_RESOURCES;
	memset(loader, 0, sizeof(embloader_loader));
	loader->name = strdup(name);
	loader->title = strdup(title);
	loader->type = type;
	loader->priority = priority;
	if (!loader->name || !loader->title) goto fail;
	if (mode) {
		loader->node = confignode_new_map();
		if (
			!loader->node ||
			!confignode_path_set_string(loader->node, "mode", mode)
		) goto fail;
	}
	if (list_obj_add_new(loaders, loader) != 0) goto fail;
	return EFI_SUCCESS;
fail:
	sdboot_boot_free_item(loader);
	return EFI_OUT_OF_RESOURCES;
}

static void sdboot_boot_find_auto_items(
	list *loaders,
	bool *auto_firmware,
	bool *auto_reboot,
	bool *auto_poweroff
) {
	list *p;
	if ((p = list_first(loaders))) do {
		LIST_DATA_DECLARE(item, p, embloader_loader*);
		if (!item) continue;
		if (*auto_firmware && item->type == LOADER_EFISETUP)
			*auto_firmware = false;
		if (*auto_reboot && embloader_loader_is_reboot(item))
			*auto_reboot = false;
		if (*auto_poweroff && embloader_loader_is_shutdown(item))
			*auto_poweroff = false;
	} while ((p = p->next));
}

static EFI_STATUS sdboot_boot_add_auto_items(list **loaders, int64_t offset) {
	EFI_STATUS status;
	if (!loaders || !g_embloader.menu || !g_embloader.sdboot)
		return EFI_NOT_READY;
	bool auto_firmware = g_embloader.sdboot->menu.auto_firmware && efi_is_setup_supported();
	bool auto_reboot = g_embloader.sdboot->menu.auto_reboot;
	bool auto_poweroff = g_embloader.sdboot->menu.auto_poweroff;
	sdboot_boot_find_auto_items(
		g_embloader.menu->loaders,
		&auto_firmware, &auto_reboot, &auto_poweroff
	);
	sdboot_boot_find_auto_items(
		*loaders, &auto_firmware, &auto_reboot, &auto_poweroff
	);
	if (auto_firmware) {
		status = sdboot_boot_add_auto_item(
			loaders,
			"sdboot-auto-firmware", "Reboot Into Firmware Interface",
			LOADER_EFISETUP, NULL, offset
		);
		if (EFI_ERROR(status)) return status;
		if (offset < INT64_MAX) offset++;
	}
	if (auto_poweroff) {
		status = sdboot_boot_add_auto_item(
			loaders,
			"sdboot-auto-shutdown", "Shut Down The System",
			LOADER_REBOOT, "shutdown", offset
		);
		if (EFI_ERROR(status)) return status;
		if (offset < INT64_MAX) offset++;
	}
	if (auto_reboot) {
		status = sdboot_boot_add_auto_item(
			loaders,
			"sdboot-auto-reboot", "Reboot The System",
			LOADER_REBOOT, "reboot", offset
		);
		if (EFI_ERROR(status)) return status;
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

typedef struct sdboot_item_priority {
	embloader_loader *item;
	int64_t priority;
} sdboot_item_priority;

static EFI_STATUS sdboot_items_collect(list *loaders, list **items) {
	list *p;
	if ((p = list_first(loaders))) do {
		LIST_DATA_DECLARE(item, p, embloader_loader*);
		if (!item || item->type != LOADER_SDBOOT) continue;
		sdboot_item_priority priority = {
			.item = item,
		};
		if (list_obj_add_new_dup(items, &priority, sizeof(priority)) != 0)
			return EFI_OUT_OF_RESOURCES;
	} while ((p = p->next));
	return EFI_SUCCESS;
}

static EFI_STATUS sdboot_items_prepare(
	list *pending,
	list **items,
	int64_t *auto_offset
) {
	list *pa, *pb;
	EFI_STATUS status;
	if (!items || !auto_offset) return EFI_INVALID_PARAMETER;
	*items = NULL;
	*auto_offset = 0;
	status = sdboot_items_collect(g_embloader.menu->loaders, items);
	if (EFI_ERROR(status)) goto fail;
	status = sdboot_items_collect(pending, items);
	if (EFI_ERROR(status)) goto fail;
	int64_t sdboot_offset = confignode_path_get_int(
		g_embloader.config, "menu.sdboot.priority-offset", 100, NULL
	);
	if ((pa = list_first(*items))) do {
		LIST_DATA_DECLARE(priority_a, pa, sdboot_item_priority*);
		if (!priority_a || !priority_a->item) continue;
		int64_t priority = sdboot_offset;
		if ((pb = list_first(*items))) do {
			LIST_DATA_DECLARE(priority_b, pb, sdboot_item_priority*);
			if (
				!priority_b || !priority_b->item ||
				priority_a == priority_b
			) continue;
			int cmp = sdboot_compare_entries(
				priority_a->item, priority_b->item
			);
			if (cmp > 0 && priority < INT64_MAX) priority++;
		} while ((pb = pb->next));
		priority_a->priority = priority;
		if (priority >= *auto_offset) {
			*auto_offset = priority;
			if (*auto_offset < INT64_MAX) (*auto_offset)++;
		}
	} while ((pa = pa->next));
	return EFI_SUCCESS;
fail:
	if (*items) list_free_all_def(*items);
	*items = NULL;
	return status;
}

static void sdboot_items_publish(list *pending, list *priorities) {
	list *p;
	if ((p = list_first(priorities))) do {
		LIST_DATA_DECLARE(priority, p, sdboot_item_priority*);
		if (priority && priority->item)
			priority->item->priority = priority->priority;
	} while ((p = p->next));
	if (!pending) return;
	list *first = list_first(pending);
	if (!g_embloader.menu->loaders) {
		g_embloader.menu->loaders = first;
	} else {
		list *last = list_last(g_embloader.menu->loaders);
		last->next = first;
		first->prev = last;
	}
	if ((p = first)) do {
		LIST_DATA_DECLARE(item, p, embloader_loader*);
		if (item && item->type == LOADER_SDBOOT && item->extra) {
			sdboot_boot_loader *loader = item->extra;
			loader->item = item;
		}
	} while ((p = p->next));
}

static EFI_STATUS sdboot_boot_add_items() {
	list *p;
	EFI_STATUS status;
	list *pending = NULL, *priorities = NULL;
	int64_t auto_offset;
	if (!g_embloader.menu || !g_embloader.sdboot) return EFI_NOT_READY;
	if ((p = list_first(g_embloader.sdboot->loaders))) do {
		LIST_DATA_DECLARE(item, p, sdboot_boot_loader*);
		if (!item) continue;
		if (
			list_search_one(g_embloader.menu->loaders, menu_loader_compare, item) ||
			list_search_one(pending, menu_loader_compare, item)
		) continue;
		embloader_loader *loader;
		loader = malloc(sizeof(embloader_loader));
		if (!loader) goto fail;
		memset(loader, 0, sizeof(embloader_loader));
		loader->name = item->name ? strdup(item->name) : NULL;
		loader->title = item->title ? strdup(item->title) : NULL;
		if (
			(item->name && !loader->name) ||
			(item->title && !loader->title)
		) {
			sdboot_boot_free_item(loader);
			goto fail;
		}
		loader->type = LOADER_SDBOOT;
		loader->complete = true;
		loader->editor = g_embloader.sdboot->menu.editor;
		loader->extra = item;
		if (item->options) {
			loader->bootargs = list_to_string(item->options, " ");
			if (!loader->bootargs) {
				sdboot_boot_free_item(loader);
				goto fail;
			}
		}
		if (list_obj_add_new(&pending, loader) != 0) {
			sdboot_boot_free_item(loader);
			goto fail;
		}
	} while ((p = p->next));
	status = sdboot_items_prepare(pending, &priorities, &auto_offset);
	if (EFI_ERROR(status)) goto fail;
	status = sdboot_boot_add_auto_items(&pending, auto_offset);
	if (EFI_ERROR(status)) goto fail;
	sdboot_items_publish(pending, priorities);
	if (priorities) list_free_all_def(priorities);
	embloader_sort_menu_loaders();
	return EFI_SUCCESS;
fail:
	if (priorities) list_free_all_def(priorities);
	if (pending) list_free_all(pending, sdboot_boot_free_item);
	return EFI_OUT_OF_RESOURCES;
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
	EFI_LOADED_IMAGE_PROTOCOL *li;
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
	if ((li = efi_get_loaded_image())) {
		dir.handle = li->DeviceHandle;
		if (sdboot_check_fs(&dir)) {
			status = sdboot_boot_load_root(menu, &dir);
			if (!EFI_ERROR(status)) {
				log_info("loaded systemd-boot menu from current device");
				have_success = true;
				if (!use_multiple) goto done;
			}
		}
	}
	if (have_success && !use_multiple) goto done;
	dir.handle = g_embloader.dir.handle;
	if (sdboot_check_fs(&dir)) {
		status = sdboot_boot_load_root(menu, &dir);
		if (!EFI_ERROR(status)) {
			log_info("loaded systemd-boot menu from previous device");
			have_success = true;
			if (!use_multiple) goto done;
		}
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
		} else {
			log_info("loaded systemd-boot menu from scanned device %" PRIuPTR, i);
			have_success = true;
			if (!use_multiple) goto done;
		}
	}
done:
	if (handles) FreePool(handles);
	if (!have_success) {
		log_warning("no valid systemd-boot configs found");
		return EFI_NOT_FOUND;
	}
	return sdboot_boot_add_items();
}
