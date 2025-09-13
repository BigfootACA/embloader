#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <Uefi.h>
#include "embloader.h"
#include "linuxboot.h"
#include "log.h"
#include "str-utils.h"
#include <stdio.h>
#include <libfdt.h>
#include <ufdt_overlay.h>

/**
 * @brief Get device tree overlay directory paths
 *
 * This function retrieves the device tree overlay directory path from the system
 * configuration and resolves it to actual directory paths using path variables.
 *
 * @return list* List of resolved DTBO directory paths, or NULL if no DTBO directory configured
 */
list* embloader_dt_get_dtbo_dir() {
	char *dtbo_dir = confignode_path_get_string(
		g_embloader.config, "devicetree.dtbo-dir", NULL, NULL
	);
	if (!dtbo_dir) return NULL;
	list *ret = embloader_resolve_path(dtbo_dir);
	free(dtbo_dir);
	return ret;
}

static bool apply_dtbo_libufdt(const char *dtbo_name, fdt base, fdt dtbo) {
	fdt_pack(base);
	int dtbo_size = fdt_totalsize(dtbo);
	int base_size = fdt_totalsize(base);
	struct fdt_header* nfdt = ufdt_apply_overlay(
		base, base_size, dtbo, dtbo_size
	);
	if (!nfdt) {
		log_warning(
			"apply dtbo use libufdt %s failed",
			dtbo_name
		);
		return false;
	}
	int new_size = fdt_totalsize(nfdt);
	if (new_size > SIZE_2MB) {
		log_warning(
			"overlay new dtb too large: %d bytes",
			new_size
		);
		free(nfdt);
		return false;
	}
	memset(base, 0, SIZE_2MB);
	int ret = fdt_open_into(nfdt, base, SIZE_2MB);
	free(nfdt);
	if (ret != 0) {
		log_warning(
			"reopen tree into buffer failed for %s: %s",
			dtbo_name, fdt_strerror(ret)
		);
		return false;
	}
	return true;
}

static bool apply_dtbo_libfdt(const char *dtbo_name, fdt base, fdt dtbo) {
	int ret = fdt_overlay_apply(base, dtbo);
	if (ret != 0) {
		log_warning(
			"apply dtbo use libfdt %s failed: %s",
			dtbo_name, fdt_strerror(ret)
		);
		return false;
	}
	return true;
}

/**
 * @brief Apply a device tree overlay using the configured method
 *
 * This function applies a device tree overlay to the base device tree using
 * the method specified in the configuration (libufdt or libfdt).
 *
 * @param dtbo_name Name of the device tree overlay (for logging purposes)
 * @param base Base device tree to apply the overlay to
 * @param dtbo Device tree overlay to apply
 * @return bool Returns true if overlay was successfully applied, false otherwise
 */
bool linux_apply_dtbo(const char *dtbo_name, fdt base, fdt dtbo) {
	bool result = false;
	char *method = confignode_path_get_string(
		g_embloader.config,
		"devicetree.dtbo-method", "libufdt", NULL
	);
	if (!method) return false;
	if (strcasecmp(method, "libufdt") == 0)
		result = apply_dtbo_libufdt(dtbo_name, base, dtbo);
	else if (strcasecmp(method, "libfdt") == 0)
		result = apply_dtbo_libfdt(dtbo_name, base, dtbo);
	else log_warning("unknown dtbo method %s", method);
	free(method);
	return result;
}

static bool do_load_dtbo(
	fdt fdt,
	EFI_FILE_PROTOCOL *base,
	const char *path,
	confignode *params
) {
	void *dtbo;
	if (!fdt || !base || !path) return false;
	if (!(dtbo = linux_try_load_dtb(base, path, false))) return false;
	log_info("pick dtbo from %s", path);
	if (params && !linux_dtbo_write_overrides(dtbo, params)) {
		log_warning("apply overrides for dtbo %s failed", path);
		free(dtbo);
		return false;
	}
	bool res = linux_apply_dtbo(path, fdt, dtbo);
	free(dtbo);
	if (!res) log_warning("apply dtbo %s failed", path);
	else log_info("applied dtbo %s successfully", path);
	return res;
}

static bool do_load_dtbo_dirs(
	fdt fdt,
	EFI_FILE_PROTOCOL *base,
	const char *path,
	confignode *params,
	list *dirs
) {
	list *p;
	if (path[0] == '/' || path[0] == '\\')
		return do_load_dtbo(fdt, base, path, params);
	if ((p = list_first(dirs))) do {
		LIST_DATA_DECLARE(dir, p, char*);
		if (!dir) continue;
		char *dtbo_path = NULL;
		const char *sep = endwith(dir, '/') ? "" : "/";
		if (asprintf(&dtbo_path, "%s%s%s.dtbo", dir, sep, path) < 0) continue;
		if (!dtbo_path) continue;
		if (!do_load_dtbo(fdt, base, dtbo_path, params)) {
			free(dtbo_path);
			continue;
		}
		free(dtbo_path);
		log_info("applied dtbo %s successfully", path);
		return true;
	} while ((p = p->next));
	log_warning("no applicable dtbo %s found", path);
	return false;
}

/**
 * @brief Load and apply a device tree overlay
 *
 * This function loads a device tree overlay (DTBO) from the specified directories
 * and applies it to the base device tree. It handles parameter substitution and
 * overlay merging.
 *
 * @param base EFI file protocol for file access
 * @param fdt Base device tree to apply the overlay to
 * @param node Configuration node containing overlay information
 * @param dtbo_dir List of directories to search for the overlay file
 * @return bool Returns true if overlay was successfully applied, false otherwise
 */
bool linux_load_dtbo(EFI_FILE_PROTOCOL *base, fdt fdt, confignode *node, list *dtbo_dir) {
	const char *dtbo_name;
	char *dtbo_xpath = NULL;
	if (!fdt) return false;
	if (!confignode_is_type(node, CONFIGNODE_TYPE_MAP)) return false;
	if (!(dtbo_name = confignode_get_key(node))) return false;
	if (!confignode_path_get_bool(node, "enabled", true, NULL)) return false;
	confignode *params = confignode_map_get(node, "params");
	dtbo_xpath = confignode_path_get_string(node, "path", dtbo_name, NULL);
	if (!dtbo_xpath) return false;
	log_debug("try apply dtbo %s", dtbo_name);
	return do_load_dtbo_dirs(fdt, base, dtbo_xpath, params, dtbo_dir);
}

/**
 * @brief Load and apply multiple device tree overlays
 *
 * This function loads and applies all configured device tree overlays
 * from the system configuration and alternative directories.
 *
 * @param base EFI file protocol for file access
 * @param fdt Base device tree to apply overlays to
 * @param alt_dir List of alternative directories to search for overlays
 * @return bool Returns true if at least one overlay was successfully applied, false otherwise
 */
bool linux_load_dtbos(EFI_FILE_PROTOCOL *base, fdt fdt, list *alt_dir) {
	if (!fdt) return false;
	bool success = false, is_empty = true;
	list *dtbo_dir = embloader_dt_get_dtbo_dir();
	if (!dtbo_dir) {
		log_warning("no dtbo directory configured, skip apply dtbo");
		return true;
	}
	list *x = list_duplicate_chars(alt_dir, NULL);
	if (x) list_obj_add(&dtbo_dir, x);
	list_reverse(dtbo_dir);
	confignode_path_foreach(iter, g_embloader.config, "devicetree.overlays") {
		is_empty = false;
		if (linux_load_dtbo(base, fdt, iter.node, dtbo_dir)) success = true;
	}
	list_free_all_def(dtbo_dir);
	return is_empty || success;
}

/**
 * @brief Load and apply device tree overlays for Linux boot
 *
 * This function loads and applies device tree overlays during Linux boot process.
 * It processes both configured overlays and overlays specified in boot information.
 *
 * @param data Linux boot data containing device tree information
 * @param info Linux boot information containing overlay specifications
 * @return EFI_STATUS Returns EFI_SUCCESS if overlays were applied successfully,
 *                    EFI_INVALID_PARAMETER for invalid parameters,
 *                    EFI_LOAD_ERROR if no overlays could be applied
 */
EFI_STATUS linux_load_dtoverlay(linux_data *data, linux_bootinfo *info) {
	bool have_success = false;
	if (!data || !info) return EFI_INVALID_PARAMETER;
	if (!info->dtoverlay) return EFI_SUCCESS;
	if (!data->fdt && !g_embloader.fdt)
		embloader_fetch_fdt();
	if (!data->fdt && !g_embloader.fdt) {
		log_warning("no device tree loaded, skip apply dtbo");
		return EFI_SUCCESS;
	}
	void *xfdt = data->fdt ?: g_embloader.fdt;
	list *dtbo_dir = embloader_dt_get_dtbo_dir();
	if (dtbo_dir && linux_load_dtbos(info->root, xfdt, dtbo_dir)) have_success = true;
	list *p;
	if ((p = list_first(info->dtoverlay))) do {
		LIST_DATA_DECLARE(dtbo, p, linux_overlay*);
		if (!dtbo || !dtbo->path) continue;
		log_debug("try apply dtbo %s", dtbo->path);
		if (do_load_dtbo_dirs(
			xfdt, info->root, dtbo->path,
			dtbo->params, dtbo_dir
		)) have_success = true;
	} while ((p = p->next));
	list_free_all_def(dtbo_dir);
	if (!have_success) log_warning("no any device tree overlays applied");
	return have_success ? EFI_SUCCESS : EFI_LOAD_ERROR;
}
