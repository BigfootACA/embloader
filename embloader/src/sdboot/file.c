#include "log.h"
#include "encode.h"
#include "internal.h"
#include "file-utils.h"
#include "efi-utils.h"
#include "str-utils.h"

/**
 * @brief Load and parse a single sdboot configuration file.
 *
 * Supports both menu configuration files (loader.conf) and boot entry files.
 * For entry files, automatically extracts the entry name from filename and
 * performs architecture filtering.
 *
 * @param menu the sdboot menu structure to update (must not be NULL)
 * @param dir the directory context containing filesystem info (must not be NULL)
 * @param base the base directory to load from (must not be NULL)
 * @param fname the relative filename to load (must not be NULL)
 * @param is_entry true for boot entry files, false for menu config files
 * @return EFI_SUCCESS on success,
 *         EFI_INVALID_PARAMETER for invalid params,
 *         EFI_NOT_FOUND if file doesn't exist, other EFI errors on failure
 */
EFI_STATUS sdboot_boot_load_file(
	sdboot_menu *menu,
	embloader_dir *dir,
	EFI_FILE_PROTOCOL *base,
	const char *fname,
	bool is_entry
) {
	EFI_STATUS status;
	char *buff = NULL;
	size_t flen = 0;
	sdboot_boot_loader *loader = NULL;
	EFI_FILE_PROTOCOL *file;
	const char *name = NULL;
	size_t name_len = 0;
	char *p;
	if (!menu || !dir || !base || !fname) return EFI_INVALID_PARAMETER;
	log_info("loading sdboot boot config from file %s", fname);
	status = efi_open(
		base, &file, fname,
		EFI_FILE_MODE_READ, 0
	);
	if (EFI_ERROR(status) || !file) {
		if (status != EFI_NOT_FOUND) log_error(
			"open sdboot boot config file %s failed: %s",
			fname, efi_status_to_string(status)
		);
		return status;
	}
	status = efi_file_read_all(file, (void**) &buff, &flen);
	if (EFI_ERROR(status)) {
		log_error(
			"failed to read file %s: %s",
			fname, efi_status_to_string(status)
		);
		file->Close(file);
		return status;
	}
	file->Close(file);
	if (is_entry) {
		if ((p = strrchr(fname, '/'))) name = p + 1;
		else if ((p = strrchr(fname, '\\'))) name = p + 1;
		else name = fname;
		name_len = strlen(name);
		if (endwithsi(name, name_len, ".conf")) name_len -= 5;
		if (!(loader = sdboot_boot_loader_new())) {
			log_error("failed to create sdboot loader");
			status = EFI_OUT_OF_RESOURCES;
			goto fail;
		}
		loader->name = strndup(name, name_len);
		if (!loader->name) {
			log_error("failed to allocate memory for loader name");
			status = EFI_OUT_OF_RESOURCES;
			goto fail;
		}
		loader->root_handle = dir->handle;
		loader->root = dir->root;
		if (EFI_ERROR(status) || !loader->root) {
			log_error(
				"open root directory failed: %s",
				efi_status_to_string(status)
			);
			if (!EFI_ERROR(status)) status = EFI_LOAD_ERROR;
			goto fail;
		}
		log_debug("created sdboot loader %s", loader->name);
	}
	bool ret = sdboot_boot_parse_content(
		fname, buff, flen,
		is_entry ? NULL : &menu->menu,
		is_entry ? loader : NULL
	);
	if (!ret) {
		status = EFI_LOAD_ERROR;
		goto fail;
	}
	if (is_entry && !sdboot_boot_loader_check(loader)) {
		log_warning("sdboot boot loader %s is invalid", loader->name);
		status = EFI_LOAD_ERROR;
		goto fail;
	}
	free(buff);
	if (is_entry) {
		if (loader->arch != sdboot_get_current_arch()) {
			log_debug(
				"skipping sdboot loader %s due to architecture mismatch",
				loader->name
			);
			sdboot_boot_loader_free(loader);
		} else list_obj_add_new(&menu->loaders, loader);
	}
	return EFI_SUCCESS;
fail:
	if (loader) sdboot_boot_loader_free(loader);
	if (buff) free(buff);
	return status;
}

/**
 * @brief Load all boot entry configuration files from a directory.
 *
 * Scans the specified directory for .conf files and loads each as a boot entry.
 * Handles UTF-16 to UTF-8 filename conversion and filters non-configuration files.
 * Continues processing even if individual files fail to load.
 *
 * @param menu the sdboot menu structure to add entries to (must not be NULL)
 * @param dir the directory context containing filesystem info (must not be NULL)
 * @param folder the directory path to scan for entries (must not be NULL)
 * @return EFI_SUCCESS if at least one entry loaded successfully,
 *         EFI_NOT_FOUND if no valid entries found, other EFI errors on failure
 */
EFI_STATUS sdboot_boot_load_entries_folder(
	sdboot_menu *menu,
	embloader_dir *dir,
	const char *folder
) {
	EFI_STATUS status;
	EFI_FILE_INFO *info = NULL;
	EFI_FILE_PROTOCOL *fp = NULL;
	UINTN infosz = 0, ps;
	bool have_success = false;
	if (!menu || !dir || !folder) return EFI_INVALID_PARAMETER;
	status = efi_open(
		dir->root, &fp, folder,
		EFI_FILE_MODE_READ,
		EFI_FILE_DIRECTORY
	);
	if (EFI_ERROR(status) || !fp) {
		log_error(
			"open sdboot boot entries folder %s failed: %s",
			folder, efi_status_to_string(status)
		);
		return status;
	}
	while (true) {
		if (info) memset(info, 0, infosz);
		ps = infosz;
		status = fp->Read(fp, &ps, info);
		if (status == EFI_NOT_FOUND) break;
		else if (status == EFI_BUFFER_TOO_SMALL) {
			if (ps <= infosz) {
				log_error("filesystem driver returned bad size for file info");
				status = EFI_DEVICE_ERROR;
				break;
			}
			infosz = ps;
			if (info) free(info);
			if (!(info = malloc(infosz))) {
				log_error("failed to allocate memory for file info");
				status = EFI_OUT_OF_RESOURCES;
				break;
			}
			continue;
		} else if (EFI_ERROR(status)) {
			log_error(
				"failed to read file info: %s",
				efi_status_to_string(status)
			);
			break;
		} else if (ps == 0) break;
		if (!info || infosz < OFFSET_OF(EFI_FILE_INFO, FileName) + sizeof(CHAR16)) {
			log_warning("bad filesystem driver for read folder");
			status = EFI_DEVICE_ERROR;
			break;
		}
		if (!info->FileName[0] ||info->FileName[0] == L'.') continue;
		if (info->Attribute & EFI_FILE_DIRECTORY) continue;
		char *fname = encode_utf16_to_utf8(info->FileName);
		if (!fname) continue;
		size_t flen = strlen(fname);
		if (!endwithsi(fname, flen, ".conf")) {
			free(fname);
			continue;
		}
		status = sdboot_boot_load_file(menu, dir, fp, fname, true);
		free(fname);
		if (!EFI_ERROR(status)) have_success = true;
		else log_error("failed to load sdboot boot entry file: %s", efi_status_to_string(status));
	}
	if (fp) fp->Close(fp);
	if (info) free(info);
	if (!have_success) {
		log_error("no valid sdboot boot entry files found");
		return EFI_NOT_FOUND;
	}
	return EFI_SUCCESS;
}

static embloader_loader* find_efisetup() {
	list *p;
	if (g_embloader.menu && (p = list_first(g_embloader.menu->loaders))) do {
		LIST_DATA_DECLARE(item, p, embloader_loader*);
		if (!item) continue;
		if (item->type == LOADER_EFISETUP) return item;
	} while ((p = p->next));
	return NULL;
}

static embloader_loader* find_reboot() {
	list *p;
	if (g_embloader.menu && (p = list_first(g_embloader.menu->loaders))) do {
		LIST_DATA_DECLARE(item, p, embloader_loader*);
		if (!item) continue;
		if (embloader_loader_is_reboot(item)) return item;
	} while ((p = p->next));
	return NULL;
}

static embloader_loader* find_shutdown() {
	list *p;
	if (g_embloader.menu && (p = list_first(g_embloader.menu->loaders))) do {
		LIST_DATA_DECLARE(item, p, embloader_loader*);
		if (!item) continue;
		if (embloader_loader_is_shutdown(item)) return item;
	} while ((p = p->next));
	return NULL;
}

/**
 * @brief Load complete systemd-boot configuration from a filesystem root.
 *
 * Loads the main menu configuration from /loader/loader.conf and all boot
 * entries from /loader/entries/ directory. Integrates configuration values
 * into global embloader settings if not already configured.
 *
 * @param menu the sdboot menu structure to populate (must not be NULL)
 * @param dir the directory context for the filesystem root (must not be NULL)
 * @return EFI_SUCCESS on successful loading (tolerates missing files),
 *         EFI_INVALID_PARAMETER for invalid params, other EFI errors on failure
 */
EFI_STATUS sdboot_boot_load_root(sdboot_menu *menu, embloader_dir *dir) {
	EFI_STATUS status;
	if (!menu || !dir) return EFI_INVALID_PARAMETER;
	status = sdboot_boot_load_file(menu, dir, dir->root, "/loader/loader.conf", false);
	if (EFI_ERROR(status) && status != EFI_NOT_FOUND) return status;
	status = sdboot_boot_load_entries_folder(menu, dir, "/loader/entries");
	if (EFI_ERROR(status) && status != EFI_NOT_FOUND) return status;
	if (!confignode_path_lookup(g_embloader.config, "menu.timeout", false)) {
		log_debug("use timeout value %d from sdboot config", menu->menu.timeout);
		g_embloader.menu->timeout = menu->menu.timeout;
	}
	if (menu->menu.default_entry && !confignode_path_lookup(g_embloader.config, "menu.default", false)) {
		embloader_loader *l;
		char *entry = menu->menu.default_entry, *newent = NULL;
		log_debug("found default entry %s in sdboot config", entry);
		if (strcasecmp(entry, "auto-firmware") == 0 && (l = find_efisetup()))
			newent = l->name;
		else if (strcasecmp(entry, "auto-reboot") == 0 && (l = find_reboot()))
			newent = l->name;
		else if (strcasecmp(entry, "auto-poweroff") == 0 && (l = find_shutdown()))
			newent = l->name;
		if (!newent){
			newent = entry;
			log_debug("use default entry %s from sdboot config", entry);		
		}  else log_debug("use auto entry %s for %s", newent, entry);
		if (g_embloader.menu->default_entry)
			free(g_embloader.menu->default_entry);
		g_embloader.menu->default_entry = strdup(newent);
	}
	return EFI_SUCCESS;
}
