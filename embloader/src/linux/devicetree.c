#include <Uefi.h>
#include <inttypes.h>
#include "linuxboot.h"
#include "embloader.h"
#include "log.h"
#include "efi-utils.h"
#include "file-utils.h"
#include "readable.h"
#include <libfdt.h>

/**
 * @brief Get device tree blob IDs from active profiles
 *
 * This function retrieves device tree blob identifiers from all active profiles
 * in the system configuration, which are used for DTB selection and matching.
 *
 * @return list* List of DTB ID strings, or NULL if no profiles or DTB IDs found
 */
list* embloader_dt_get_dtb_id() {
	list *p, *ret = NULL;
	confignode *np = confignode_map_get(g_embloader.config, "profiles");
	if (!np) return NULL;
	if ((p = list_first(g_embloader.profiles))) do {
		LIST_DATA_DECLARE(profile, p, char*);
		if (!profile) continue;
		confignode *cp = confignode_map_get(np, profile);
		if (!cp) continue;
		char *id = confignode_path_get_string(cp, "dtb-id", NULL, NULL);
		if (!id) continue;
		list_obj_add_new(&ret, id);
	} while ((p = p->next));
	return ret;
}

/**
 * @brief Get default device tree blob path list
 *
 * This function retrieves the default device tree blob path from the system
 * configuration and resolves it to actual file paths using path variables.
 *
 * @return list* List of resolved DTB file paths, or NULL if no default DTB configured
 */
list* embloader_dt_get_default_dtb() {
	char *def_dtb = confignode_path_get_string(
		g_embloader.config, "devicetree.default-dtb", NULL, NULL
	);
	if (!def_dtb) return NULL;
	list *ret = embloader_resolve_path(def_dtb);
	free(def_dtb);
	return ret;
}

/**
 * @brief Try to load a device tree blob from file
 *
 * This function attempts to load a device tree blob (DTB) from the specified file path.
 * It validates the DTB structure and optionally grows the DTB size for modifications.
 *
 * @param dtb Path to the device tree blob file
 * @param grow Whether to allocate extra space for DTB modifications
 * @return fdt Pointer to loaded device tree structure, or NULL on failure
 */
fdt linux_try_load_dtb(EFI_FILE_PROTOCOL *base, const char *dtb, bool grow) {
	EFI_STATUS status;
	EFI_FILE_PROTOCOL *fp = NULL;
	void *data = NULL;
	fdt fdt = NULL, rfdt = NULL;
	size_t len = 0;
	char buf[64];
	int ret;
	if (!dtb || !base) return NULL;
	status = efi_open(
		base, &fp, dtb,
		EFI_FILE_MODE_READ, 0
	);
	if (EFI_ERROR(status) || !fp) {
		if (status == EFI_NOT_FOUND) log_debug("dtb %s not found", dtb);
		else log_warning("open dtb %s failed: %s", dtb, efi_status_to_string(status));
		return NULL;
	}
	log_debug("try load dtb %s", dtb);
	status = efi_file_read_all(fp, &data, &len);
	fp->Close(fp);
	fp = NULL;
	if (EFI_ERROR(status) || !data || len == 0) {
		log_warning("read dtb %s failed: %s", dtb, efi_status_to_string(status));
		goto fail;
	}
	format_size_float(buf, len);
	log_info("read dtb %s size %s (%" PRId64 " bytes)", dtb, buf, len);
	if (len > SIZE_2MB) {
		log_warning("dtb %s too large: %s", dtb, buf);
		log_warning("dtb size limit is 2MiB");
		goto fail;
	}
	if ((ret = fdt_check_header(data)) < 0) {
		log_warning("invalid dtb header %s: %s", dtb, fdt_strerror(ret));
		goto fail;
	}
	if (fdt_totalsize(data) > len) {
		log_warning("incomplete dtb data %s", dtb);
		goto fail;
	}
	if (grow) {
		if (!(fdt = malloc(SIZE_2MB))) {
			log_warning("alloc dtb buffer failed");
			goto fail;
		}
		if ((ret = fdt_open_into(data, fdt, SIZE_2MB)) < 0) {
			log_warning("fdt_open_into failed: %s", fdt_strerror(ret));
			goto fail;
		}
		free(data);
		rfdt = fdt;
	} else rfdt = data;
	log_info("load dtb %s successfully", dtb);
	const char *str;
	int root = fdt_path_offset(rfdt, "/");
	if ((str = fdt_stringlist_get(rfdt, root, "model", 0, NULL)))
		log_info("dtb model: %s", str);
	if ((str = fdt_stringlist_get(rfdt, root, "compatible", 0, NULL)))
		log_info("dtb compatible: %s", str);
	return rfdt;
fail:
	if (fp) fp->Close(fp);
	if (data) free(data);
	if (fdt) free(fdt);
	return NULL;
}

/**
 * @brief Load the default device tree blob
 *
 * This function attempts to load the default device tree blob based on
 * the system configuration and detected hardware profile.
 *
 * @return bool Returns true if default DTB was successfully loaded, false otherwise
 */
bool linux_load_default_dtb() {
	list *p;
	list* dtbs = embloader_dt_get_default_dtb();
	list_reverse(dtbs);
	if ((p = list_first(dtbs))) do {
		LIST_DATA_DECLARE(dtb, p, char*);
		if (!dtb) continue;
		log_debug("default dtb %s", dtb);
		fdt fdt = linux_try_load_dtb(g_embloader.dir.root, dtb, true);
		if (!fdt) continue;
		if (g_embloader.fdt)
			free(g_embloader.fdt);
		g_embloader.fdt = fdt;
		list_free_all_def(dtbs);
		return true;
	} while ((p = p->next));
	list_free_all_def(dtbs);
	log_warning("no valid dtb found");
	return false;
}

/**
 * @brief Load device tree for Linux boot
 *
 * This function loads the appropriate device tree for Linux boot, either from
 * the boot configuration or using the system default. It handles DTB loading
 * and overlay application.
 *
 * @param data Pointer to linux_data structure to store the device tree
 * @param info Pointer to linux_bootinfo structure containing device tree configuration
 * @return EFI_STATUS Returns EFI_SUCCESS on success, or appropriate error code on failure
 */
EFI_STATUS linux_load_devicetree(linux_data *data, linux_bootinfo *info) {
	if (!data || !info) return EFI_INVALID_PARAMETER;
	if (!info->devicetree) return EFI_SUCCESS;
	fdt fdt = linux_try_load_dtb(info->root, info->devicetree, true);
	if (!fdt) {
		log_warning("load dtb %s failed", info->devicetree);
		return EFI_NOT_FOUND;
	}
	data->fdt = fdt;
	return EFI_SUCCESS;
}
