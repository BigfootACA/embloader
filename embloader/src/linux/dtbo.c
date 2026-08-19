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
 * @brief Get DTBO on error behavior from configuration
 *
 * This function reads the "devicetree.dtbo-on-error" configuration value to determine
 * how the system should behave when applying a device tree overlay (DTBO) fails.
 *
 * @return enum embloader_dtbo_on_error The configured behavior for DTBO application errors
 */
enum embloader_dtbo_on_error linux_get_dtbo_on_error() {
	char *value = confignode_path_get_string(
		g_embloader.config,
		"devicetree.dtbo-on-error", "ignore", NULL
	);
	enum embloader_dtbo_on_error result = DTBO_ERROR_FAILURE;
	if (value) {
		if (strcasecmp(value, "failure") == 0)
			result = DTBO_ERROR_FAILURE;
		else if (strcasecmp(value, "ignore") == 0)
			result = DTBO_ERROR_IGNORE;
		else if (strcasecmp(value, "needone") == 0)
			result = DTBO_ERROR_NEEDONE;
		else if (strcasecmp(value, "revert") == 0)
			result = DTBO_ERROR_REVERT;
		else log_warning("unknown dtbo-on-error value %s", value);
		free(value);
	}
	return result;
}

/**
 * @brief Get device tree overlay directory paths
 *
 * This function retrieves the device tree overlay directory path from the system
 * configuration and resolves it to actual directory paths using path variables.
 *
 * @return list* List of resolved DTBO directory paths, or NULL if no DTBO directory configured
 */
list* embloader_dt_get_dtbo_dir(embloader_loader *loader) {
	char *dtbo_dir = confignode_path_get_string(
		g_embloader.config, "devicetree.dtbo-dir", NULL, NULL
	);
	if (!dtbo_dir) return NULL;
	list *ret = embloader_resolve_path(loader, dtbo_dir, NULL);
	free(dtbo_dir);
	return ret;
}

static bool dtbo_matches_active_profile(confignode *node) {
	confignode *profile_node = confignode_path_lookup(node, "profile", false);
	if (!profile_node) return true;
	if (!confignode_is_type(profile_node, CONFIGNODE_TYPE_VALUE)) return false;
	bool ok = false;
	char *profile = confignode_value_get_string(profile_node, NULL, &ok);
	if (!ok || !profile) return false;
	bool matches = !profile[0] ||
		list_search_string(g_embloader.profiles, profile) != NULL;
	free(profile);
	return matches;
}

static bool apply_dtbo_libufdt(const char *dtbo_name, fdt base, fdt dtbo) {
	int ret = fdt_pack(base);
	if (ret != 0) {
		log_warning(
			"pack base tree failed for %s: %s",
			dtbo_name, fdt_strerror(ret)
		);
		return false;
	}
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
	if (new_size <= 0 || new_size > SIZE_2MB) {
		log_warning(
			"overlay new dtb too large: %d bytes",
			new_size
		);
		free(nfdt);
		return false;
	}
	memset(base, 0, SIZE_2MB);
	ret = fdt_open_into(nfdt, base, SIZE_2MB);
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
	void *working = NULL;
	int ret;
	if (!dtbo_name || !base || !dtbo) return false;
	if (fdt_check_header(base) != 0 || fdt_check_header(dtbo) != 0)
		return false;
	char *method = confignode_path_get_string(
		g_embloader.config,
		"devicetree.dtbo-method", "libufdt", NULL
	);
	if (!method) return false;
	working = malloc(SIZE_2MB);
	if (!working) {
		free(method);
		return false;
	}
	memset(working, 0, SIZE_2MB);
	ret = fdt_open_into(base, working, SIZE_2MB);
	if (ret != 0) {
		log_warning(
			"copy base tree failed for %s: %s",
			dtbo_name, fdt_strerror(ret)
		);
		goto done;
	}
	if (strcasecmp(method, "libufdt") == 0)
		result = apply_dtbo_libufdt(dtbo_name, working, dtbo);
	else if (strcasecmp(method, "libfdt") == 0)
		result = apply_dtbo_libfdt(dtbo_name, working, dtbo);
	else log_warning("unknown dtbo method %s", method);
	if (result && fdt_check_header(working) != 0) {
		log_warning("overlay %s produced an invalid tree", dtbo_name);
		result = false;
	}
	if (result) memcpy(base, working, SIZE_2MB);
done:
	free(working);
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
	bool ret = do_load_dtbo_dirs(fdt, base, dtbo_xpath, params, dtbo_dir);
	free(dtbo_xpath);
	return ret;
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
 * @param on_error Behavior to follow if an overlay fails to apply
 * @return int Returns the number of successfully applied overlays,
 *             -1 if overlays fail under the configured error policy,
 *             0 if no overlays configured
 */
int linux_load_dtbos(embloader_loader *loader, EFI_FILE_PROTOCOL *base, fdt fdt, list *alt_dir, enum embloader_dtbo_on_error on_error) {
	if (!fdt) return -1;
	int applied_count = 0;
	bool is_empty = true;
	list *dtbo_dir = embloader_dt_get_dtbo_dir(loader);
	if (!dtbo_dir) log_warning(
		"no dtbo directory configured; only absolute overlay paths can be used"
	);
	list *x = list_duplicate_chars(alt_dir, NULL);
	if (x && list_obj_add(&dtbo_dir, x) != 0) {
		list_free_all_def(x);
		list_free_all_def(dtbo_dir);
		return -1;
	}
	list_reverse(dtbo_dir);
	confignode_path_foreach(iter, g_embloader.config, "devicetree.overlays") {
		if (!confignode_path_get_bool(iter.node, "enabled", true, NULL)) continue;
		if (!dtbo_matches_active_profile(iter.node)) continue;
		is_empty = false;
		if (linux_load_dtbo(base, fdt, iter.node, dtbo_dir)) applied_count++;
		else if (on_error == DTBO_ERROR_FAILURE || on_error == DTBO_ERROR_REVERT) {
			applied_count = -1;
			break;
		}
	}
	if (!is_empty && applied_count == 0 && on_error == DTBO_ERROR_NEEDONE)
		applied_count = -1;
	list_free_all_def(dtbo_dir);
	return applied_count;
}

static EFI_STATUS prepare_dtoverlay_fdt(embloader_loader *loader, linux_data *data) {
	bool depends_on_loader = false;
	if (data->fdt) return EFI_SUCCESS;
	if (linux_load_default_dtb(loader, &depends_on_loader)) return EFI_SUCCESS;
	if (depends_on_loader) return EFI_NOT_FOUND;
	if (g_embloader.fdt) return EFI_SUCCESS;
	EFI_STATUS status = embloader_fetch_fdt();
	return status == EFI_NOT_FOUND ? EFI_SUCCESS : status;
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
	int applied_count = 0, failed_count = 0, ret;
	enum embloader_dtbo_on_error on_error = linux_get_dtbo_on_error();
	if (!data || !info) return EFI_INVALID_PARAMETER;
	if (
		!info->dtoverlay && !confignode_path_lookup(
			g_embloader.config, "devicetree.overlays", false
		)
	) return EFI_SUCCESS;
	EFI_STATUS status = prepare_dtoverlay_fdt(data->loader, data);
	if (EFI_ERROR(status)) return status;
	if (!data->fdt && !g_embloader.fdt) {
		log_warning("no device tree loaded, skip apply dtbo");
		return EFI_SUCCESS;
	}
	int fdt_size = 0;
	void *fdt_backup = NULL;
	void *xfdt = data->fdt ?: g_embloader.fdt;
	if (on_error == DTBO_ERROR_REVERT && xfdt) {
		fdt_size = fdt_totalsize(xfdt);
		if (fdt_size <= 0 || fdt_size > SIZE_2MB) {
			log_error("bad current fdt");
			return EFI_DEVICE_ERROR;
		}
		if (!(fdt_backup = malloc(fdt_size))) {
			log_error("failed to allocate memory for fdt backup");
			return EFI_OUT_OF_RESOURCES;
		}
		memcpy(fdt_backup, xfdt, fdt_size);
	}
	list *dtbo_dir = embloader_dt_get_dtbo_dir(data->loader);
	ret = linux_load_dtbos(data->loader, info->root, xfdt, NULL, on_error);
	if (ret < 0){
		if (on_error == DTBO_ERROR_FAILURE) {
			list_free_all_def(dtbo_dir);
			if (fdt_backup) free(fdt_backup);
			return EFI_LOAD_ERROR;
		}
		if (on_error == DTBO_ERROR_REVERT && fdt_backup) {
			list_free_all_def(dtbo_dir);
			log_error("failed to apply dtbos, reverting to original device tree");
			memcpy(xfdt, fdt_backup, fdt_size);
			free(fdt_backup);
			return EFI_SUCCESS;
		}
		if (on_error == DTBO_ERROR_NEEDONE) failed_count++;
	}
	if (ret > 0) applied_count += ret;
	list *p;
	if ((p = list_first(info->dtoverlay))) do {
		LIST_DATA_DECLARE(dtbo, p, linux_overlay*);
		if (!dtbo || !dtbo->path) continue;
		log_debug("try apply dtbo %s", dtbo->path);
		if (do_load_dtbo_dirs(
			xfdt, info->root, dtbo->path,
			dtbo->params, dtbo_dir
		)) {
			applied_count++;
			continue;
		}
		failed_count++;
		if (on_error == DTBO_ERROR_FAILURE) {
			log_error("dtbo %s failed, aborting boot", dtbo->path);
			list_free_all_def(dtbo_dir);
			if (fdt_backup) free(fdt_backup);
			return EFI_LOAD_ERROR;
		}
		if (on_error == DTBO_ERROR_REVERT && fdt_backup) {
			log_error("dtbo %s failed, reverting to original device tree", dtbo->path);
			memcpy(xfdt, fdt_backup, fdt_size);
			applied_count = 0;
			failed_count = 0;
			break;
		}
		log_warning("dtbo %s failed, continuing", dtbo->path);
	} while ((p = p->next));
	list_free_all_def(dtbo_dir);
	if (fdt_backup) free(fdt_backup);
	if (on_error == DTBO_ERROR_NEEDONE && applied_count == 0 && failed_count > 0) {
		log_error("no device tree overlays applied, aborting boot");
		return EFI_LOAD_ERROR;
	}
	if (applied_count > 0)
		log_info("applied %d device tree overlay(s)", applied_count);
	else
		log_warning("no device tree overlays applied");
	return EFI_SUCCESS;
}
