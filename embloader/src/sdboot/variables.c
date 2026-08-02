#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fnmatch.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/DevicePath.h>
#include "bootmenu.h"
#include "efi-utils.h"
#include "embloader.h"
#include "encode.h"
#include "internal.h"
#include "ticks.h"
#include "variables.h"

#define EFI_LOADER_FEATURE_CONFIG_TIMEOUT          (UINT64_C(1) << 0)
#define EFI_LOADER_FEATURE_CONFIG_TIMEOUT_ONE_SHOT (UINT64_C(1) << 1)
#define EFI_LOADER_FEATURE_ENTRY_DEFAULT           (UINT64_C(1) << 2)
#define EFI_LOADER_FEATURE_ENTRY_ONESHOT           (UINT64_C(1) << 3)
#define EFI_LOADER_FEATURE_SORT_KEY                (UINT64_C(1) << 8)
#define EFI_LOADER_FEATURE_SAVED_ENTRY             (UINT64_C(1) << 9)
#define EFI_LOADER_FEATURE_DEVICETREE              (UINT64_C(1) << 10)
#define EFI_LOADER_FEATURE_MENU_DISABLE            (UINT64_C(1) << 13)

static char *entry_last_booted;
static embloader_loader *entry_oneshot_selected;
static bool save_last_booted;
static bool menu_variables_initialized;

static void sdboot_record_error(EFI_STATUS *result, EFI_STATUS status) {
	if (result && !EFI_ERROR(*result) && EFI_ERROR(status)) *result = status;
}

static EFI_STATUS sdboot_set_time(const char *name, uint64_t usec) {
	if (!name) return EFI_INVALID_PARAMETER;
	if (usec == 0) usec = ticks_usec();
	if (usec == 0) return EFI_SUCCESS;
	return efivar_set_fmt16(&gLoaderGuid, name, 0, "%" PRIu64, usec);
}

static bool sdboot_variable_is_missing(const char *name) {
	return EFI_ERROR(efivar_get_raw(&gLoaderGuid, name, NULL, NULL));
}

static bool sdboot_get_partition_uuid(EFI_HANDLE handle, char uuid[37]) {
	EFI_DEVICE_PATH_PROTOCOL *dp = efi_device_path_from_handle(handle);
	if (!dp || !uuid) return false;
	while (dp) {
		UINTN length = (UINTN)dp->Length[0] | (UINTN)dp->Length[1] << 8;
		if (length < sizeof(*dp)) return false;
		if (
			dp->Type == MEDIA_DEVICE_PATH &&
			dp->SubType == MEDIA_HARDDRIVE_DP &&
			length >= sizeof(HARDDRIVE_DEVICE_PATH)
		) {
			HARDDRIVE_DEVICE_PATH *hd = (HARDDRIVE_DEVICE_PATH *)dp;
			if (hd->SignatureType == SIGNATURE_TYPE_GUID) {
				EFI_GUID guid;
				memcpy(&guid, hd->Signature, sizeof(guid));
				AsciiSPrint(uuid, 37, "%g", &guid);
				return true;
			}
		}
		dp = efi_device_path_next(dp);
	}
	return false;
}

static EFI_STATUS sdboot_export_common_variables(void) {
	EFI_STATUS result = EFI_SUCCESS, status;
	EFI_LOADED_IMAGE_PROTOCOL *loaded_image = efi_get_loaded_image();
	if (
		loaded_image && loaded_image->DeviceHandle &&
		sdboot_variable_is_missing("LoaderDevicePartUUID")
	) {
		char uuid[37];
		if (sdboot_get_partition_uuid(loaded_image->DeviceHandle, uuid)) {
			status = efivar_set_str16_from_str8(
				&gLoaderGuid, "LoaderDevicePartUUID", uuid, 0
			);
			sdboot_record_error(&result, status);
		}
	}
	if (
		loaded_image && loaded_image->FilePath &&
		sdboot_variable_is_missing("LoaderImageIdentifier")
	) {
		char *path = efi_device_path_to_text(loaded_image->FilePath);
		if (path) {
			status = efivar_set_str16_from_str8(
				&gLoaderGuid, "LoaderImageIdentifier", path, 0
			);
			sdboot_record_error(&result, status);
			free(path);
		}
	}
	if (
		gST && gST->FirmwareVendor &&
		sdboot_variable_is_missing("LoaderFirmwareInfo")
	) {
		char *vendor = encode_utf16_to_utf8(gST->FirmwareVendor);
		if (!vendor) {
			sdboot_record_error(&result, EFI_OUT_OF_RESOURCES);
		} else {
			status = efivar_set_fmt16(
				&gLoaderGuid, "LoaderFirmwareInfo", 0,
				"%s %u.%02u", vendor,
				gST->FirmwareRevision >> 16,
				gST->FirmwareRevision & 0xffff
			);
			sdboot_record_error(&result, status);
			free(vendor);
		}
	}
	if (gST && sdboot_variable_is_missing("LoaderFirmwareType")) {
		status = efivar_set_fmt16(
			&gLoaderGuid, "LoaderFirmwareType", 0,
			"UEFI %u.%02u", gST->Hdr.Revision >> 16,
			gST->Hdr.Revision & 0xffff
		);
		sdboot_record_error(&result, status);
	}
	return result;
}

EFI_STATUS sdboot_init_loader_variables(void) {
	static const uint64_t loader_features =
		EFI_LOADER_FEATURE_CONFIG_TIMEOUT |
		EFI_LOADER_FEATURE_CONFIG_TIMEOUT_ONE_SHOT |
		EFI_LOADER_FEATURE_ENTRY_DEFAULT |
		EFI_LOADER_FEATURE_ENTRY_ONESHOT |
		EFI_LOADER_FEATURE_SORT_KEY |
		EFI_LOADER_FEATURE_SAVED_ENTRY |
		EFI_LOADER_FEATURE_DEVICETREE |
		EFI_LOADER_FEATURE_MENU_DISABLE;
	EFI_STATUS result = sdboot_export_common_variables(), record;
	record = sdboot_set_time("LoaderTimeInitUSec", g_embloader.start_time);
	sdboot_record_error(&result, record);
	record = efivar_set_str16(&gLoaderGuid, "LoaderInfo", L"embloader " EMBLOADER_VERSION, 0);
	sdboot_record_error(&result, record);
	record = efivar_set_uint64_le(&gLoaderGuid, "LoaderFeatures", loader_features, 0);
	sdboot_record_error(&result, record);
	return result;
}

static EFI_STATUS sdboot_var_get_timeout(
	const char *name,
	int *out,
	bool *ret_disabled
) {
	EFI_STATUS status;
	char *value = NULL;
	char *end = NULL;
	unsigned long long timeout;
	if (!name || !out) return EFI_INVALID_PARAMETER;
	if (ret_disabled) *ret_disabled = false;
	status = efivar_get_str16_to_str8(&gLoaderGuid, name, &value);
	if (EFI_ERROR(status)) return status;
	if (strcasecmp(value, "menu-force") == 0) {
		*out = -1;
		goto done;
	}
	if (
		strcasecmp(value, "menu-hidden") == 0 ||
		strcasecmp(value, "menu-disabled") == 0
	) {
		*out = 0;
		if (ret_disabled)
			*ret_disabled = strcasecmp(value, "menu-disabled") == 0;
		goto done;
	}
	if (!value[0]) goto invalid;
	for (const char *p = value; *p; p++)
		if (*p < '0' || *p > '9') goto invalid;
	errno = 0;
	timeout = strtoull(value, &end, 10);
	if (errno == ERANGE || !end || *end != 0) goto invalid;
	if (timeout >= UINT32_MAX)
		*out = -1;
	else if (timeout > INT_MAX)
		*out = INT_MAX;
	else
		*out = (int) timeout;
done:
	free(value);
	return EFI_SUCCESS;
invalid:
	free(value);
	return EFI_INVALID_PARAMETER;
}

static bool sdboot_loader_is_bootable(const embloader_loader *loader) {
	if (!loader) return false;
	switch (loader->type) {
		case LOADER_EFI:
		case LOADER_LINUX_EFI:
		case LOADER_LINUX:
		case LOADER_SDBOOT:
			return true;
		default:
			return false;
	}
}

static char *sdboot_loader_entry_id(const embloader_loader *loader) {
	char *id = NULL;
	if (!loader || !loader->name) return NULL;
	if (loader->type == LOADER_SDBOOT) {
		if (asprintf(&id, "%s.conf", loader->name) < 0) return NULL;
		return id;
	}
	if (loader->type == LOADER_EFISETUP && strcmp(loader->name, "sdboot-auto-firmware") == 0)
		return strdup("auto-reboot-to-firmware-setup");
	if (loader->type == LOADER_REBOOT && strcmp(loader->name, "sdboot-auto-shutdown") == 0)
		return strdup("auto-poweroff");
	if (loader->type == LOADER_REBOOT && strcmp(loader->name, "sdboot-auto-reboot") == 0)
		return strdup("auto-reboot");
	return strdup(loader->name);
}

static EFI_STATUS sdboot_find_loader(
	const char *pattern,
	bool allow_internal_name,
	embloader_loader **ret
) {
	list *p;
	if (!ret) return EFI_INVALID_PARAMETER;
	*ret = NULL;
	if (!pattern) return EFI_SUCCESS;
	if (!g_embloader.menu) return EFI_NOT_READY;
	if ((p = list_first(g_embloader.menu->loaders))) do {
		LIST_DATA_DECLARE(loader, p, embloader_loader*);
		if (!loader || !loader->name) continue;
		if (allow_internal_name && fnmatch(pattern, loader->name, FNM_CASEFOLD) == 0) {
			*ret = loader;
			return EFI_SUCCESS;
		}
		char *id = sdboot_loader_entry_id(loader);
		if (!id) return EFI_OUT_OF_RESOURCES;
		bool matches = fnmatch(pattern, id, FNM_CASEFOLD) == 0;
		free(id);
		if (matches) {
			*ret = loader;
			return EFI_SUCCESS;
		}
	} while ((p = p->next));
	return EFI_SUCCESS;
}

static EFI_STATUS sdboot_entry_id_seen_before(
	list *entry,
	const char *entry_id,
	bool *ret
) {
	list *p;
	if (!entry || !entry_id || !ret || !g_embloader.menu)
		return EFI_INVALID_PARAMETER;
	*ret = false;
	if ((p = list_first(g_embloader.menu->loaders))) do {
		if (p == entry) break;
		LIST_DATA_DECLARE(loader, p, embloader_loader*);
		if (!loader || !loader->name) continue;
		char *id = sdboot_loader_entry_id(loader);
		if (!id) return EFI_OUT_OF_RESOURCES;
		bool matches = strcasecmp(entry_id, id) == 0;
		free(id);
		if (matches) {
			*ret = true;
			break;
		}
	} while ((p = p->next));
	return EFI_SUCCESS;
}

static EFI_STATUS sdboot_export_entries(void) {
	EFI_STATUS status;
	CHAR16 *entries = NULL;
	size_t size = 0;
	list *p;
	if (!g_embloader.menu) return EFI_NOT_READY;
	if ((p = list_first(g_embloader.menu->loaders))) do {
		LIST_DATA_DECLARE(loader, p, embloader_loader*);
		if (!loader || !loader->name) continue;
		char *entry_id = sdboot_loader_entry_id(loader);
		if (!entry_id) {
			free(entries);
			return EFI_OUT_OF_RESOURCES;
		}
		bool duplicate;
		status = sdboot_entry_id_seen_before(p, entry_id, &duplicate);
		if (EFI_ERROR(status)) {
			free(entry_id);
			free(entries);
			return status;
		}
		if (duplicate) {
			free(entry_id);
			continue;
		}
		CHAR16 *id = encode_utf8_to_utf16(entry_id);
		free(entry_id);
		if (!id) {
			free(entries);
			return EFI_OUT_OF_RESOURCES;
		}
		size_t id_size = StrSize(id);
		if (id_size > SIZE_MAX - size) {
			free(id);
			free(entries);
			return EFI_OUT_OF_RESOURCES;
		}
		CHAR16 *replacement = realloc(entries, size + id_size);
		if (!replacement) {
			free(id);
			free(entries);
			return EFI_OUT_OF_RESOURCES;
		}
		entries = replacement;
		memcpy((uint8_t *)entries + size, id, id_size);
		size += id_size;
		free(id);
	} while ((p = p->next));
	if (entries) {
		status = efivar_set_raw(
			&gLoaderGuid, "LoaderEntries", entries, size, 0
		);
		free(entries);
		return status;
	}
	status = efivar_unset(&gLoaderGuid, "LoaderEntries", 0);
	return status == EFI_NOT_FOUND ? EFI_SUCCESS : status;
}

static EFI_STATUS sdboot_read_entry_variable(const char *name, char **ret) {
	if (!name || !ret) return EFI_INVALID_PARAMETER;
	*ret = NULL;
	return efivar_get_str16_to_str8(&gLoaderGuid, name, ret);
}

EFI_STATUS sdboot_init_menu_variables(void) {
	EFI_STATUS result = EFI_SUCCESS, status;
	embloader_menu *menu = g_embloader.menu;
	char *entry_default = NULL, *entry_oneshot = NULL, *last_booted = NULL;
	embloader_loader *oneshot_selected = NULL, *selected = NULL;
	bool disabled, selection_ready = true;
	bool use_saved_config, use_saved_efivar;
	int timeout;
	if (!menu) return EFI_NOT_READY;
	free(entry_last_booted);
	entry_last_booted = NULL;
	entry_oneshot_selected = NULL;
	save_last_booted = false;
	menu_variables_initialized = false;

	status = sdboot_var_get_timeout("LoaderConfigTimeout", &timeout, &disabled);
	if (!EFI_ERROR(status)) {
		menu->timeout = timeout;
		menu->disabled = disabled;
	}
	else if (status != EFI_NOT_FOUND) sdboot_record_error(&result, status);

	status = sdboot_var_get_timeout("LoaderConfigTimeoutOneShot", &timeout, NULL);
	if (!EFI_ERROR(status)) {
		menu->timeout = timeout <= 0 ? -1 : timeout;
		menu->disabled = false;
		status = efivar_unset(
			&gLoaderGuid, "LoaderConfigTimeoutOneShot",
			EFI_VARIABLE_NON_VOLATILE
		);
		if (status != EFI_NOT_FOUND) sdboot_record_error(&result, status);
	} else if (status != EFI_NOT_FOUND) sdboot_record_error(&result, status);

	status = sdboot_read_entry_variable("LoaderEntryOneShot", &entry_oneshot);
	if (!EFI_ERROR(status)) {
		status = efivar_unset(
			&gLoaderGuid, "LoaderEntryOneShot",
			EFI_VARIABLE_NON_VOLATILE
		);
		if (status != EFI_NOT_FOUND) sdboot_record_error(&result, status);
	} else if (status != EFI_NOT_FOUND) sdboot_record_error(&result, status);

	status = sdboot_read_entry_variable("LoaderEntryDefault", &entry_default);
	if (EFI_ERROR(status)) {
		if (status != EFI_NOT_FOUND) sdboot_record_error(&result, status);
		free(entry_default);
		entry_default = NULL;
	}
	use_saved_config = menu->default_entry && strcasecmp(menu->default_entry, "@saved") == 0;
	use_saved_efivar = entry_default && strcasecmp(entry_default, "@saved") == 0;
	if (use_saved_config || use_saved_efivar) {
		status = sdboot_read_entry_variable(
			"LoaderEntryLastBooted", &last_booted
		);
		if (EFI_ERROR(status)) {
			if (status != EFI_NOT_FOUND) sdboot_record_error(&result, status);
			free(last_booted);
			last_booted = NULL;
		}
	}
	if (entry_oneshot) {
		status = sdboot_find_loader(entry_oneshot, false, &selected);
		if (EFI_ERROR(status)) goto selection_failed;
		oneshot_selected = selected;
	}
	if (!selected && entry_default) {
		status = sdboot_find_loader(
			use_saved_efivar ? last_booted : entry_default,
			false,
			&selected
		);
		if (EFI_ERROR(status)) goto selection_failed;
	}
	if (!selected && menu->default_entry) {
		status = sdboot_find_loader(
			use_saved_config ? last_booted : menu->default_entry,
			!use_saved_config,
			&selected
		);
		if (EFI_ERROR(status)) goto selection_failed;
	}
	if (selected) {
		char *replacement = strdup(selected->name);
		if (!replacement) {
			sdboot_record_error(&result, EFI_OUT_OF_RESOURCES);
			selection_ready = false;
		} else {
			free(menu->default_entry);
			menu->default_entry = replacement;
			if (selected == oneshot_selected)
				entry_oneshot_selected = selected;
		}
	}
	goto selection_done;
selection_failed:
	sdboot_record_error(&result, status);
	selection_ready = false;
selection_done:
	save_last_booted = use_saved_efivar || (!entry_default && use_saved_config);
	if (selection_ready) {
		entry_last_booted = last_booted;
		last_booted = NULL;
		menu_variables_initialized = true;
	}
	free(last_booted);
	free(entry_oneshot);
	free(entry_default);
	sdboot_record_error(&result, sdboot_export_entries());
	return result;
}

EFI_STATUS sdboot_set_menu_time(void) {
	return sdboot_set_time("LoaderTimeMenuUSec", 0);
}

EFI_STATUS sdboot_set_exec_time(void) {
	return sdboot_set_time("LoaderTimeExecUSec", 0);
}

EFI_STATUS sdboot_set_selected_entry(const embloader_loader *loader) {
	EFI_STATUS result, status;
	char *entry_id, *replacement;
	bool oneshot;
	if (!loader || !loader->name) return EFI_INVALID_PARAMETER;
	oneshot = loader == entry_oneshot_selected;
	if (oneshot) entry_oneshot_selected = NULL;
	if (!sdboot_loader_is_bootable(loader)) return EFI_SUCCESS;
	entry_id = sdboot_loader_entry_id(loader);
	if (!entry_id) return EFI_OUT_OF_RESOURCES;
	result = efivar_set_str16_from_str8(
		&gLoaderGuid, "LoaderEntrySelected", entry_id, 0
	);
	if (!menu_variables_initialized) goto done;
	if (oneshot) goto done;
	if (!save_last_booted) {
		status = efivar_unset(
			&gLoaderGuid, "LoaderEntryLastBooted",
			EFI_VARIABLE_NON_VOLATILE
		);
		if (status == EFI_NOT_FOUND) status = EFI_SUCCESS;
		if (!EFI_ERROR(status)) {
			free(entry_last_booted);
			entry_last_booted = NULL;
		}
		sdboot_record_error(&result, status);
		goto done;
	}
	if (entry_last_booted && strcasecmp(entry_last_booted, entry_id) == 0) goto done;
	replacement = strdup(entry_id);
	if (!replacement) {
		sdboot_record_error(&result, EFI_OUT_OF_RESOURCES);
		goto done;
	}
	status = efivar_set_str16_from_str8(
		&gLoaderGuid, "LoaderEntryLastBooted", entry_id,
		EFI_VARIABLE_NON_VOLATILE
	);
	if (!EFI_ERROR(status)) {
		free(entry_last_booted);
		entry_last_booted = replacement;
		replacement = NULL;
	}
	free(replacement);
	sdboot_record_error(&result, status);
done:
	free(entry_id);
	return result;
}
