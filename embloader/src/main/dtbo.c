#include "configfile.h"
#include "efi-utils.h"
#include "file-utils.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static bool parse_overlay_params(const char* params, size_t len, confignode* overlay_node) {
	if (!params || !overlay_node || len == 0) return false;
	confignode* params_node = confignode_path_lookup(overlay_node, "params", true);
	if (!params_node) return false;
	if (!confignode_is_type(params_node, CONFIGNODE_TYPE_MAP)) {
		confignode *replacement = confignode_new_map();
		if (!replacement) return false;
		if (!confignode_replace(&params_node, replacement)) {
			confignode_clean(replacement);
			return false;
		}
	}
	const char* start = params;
	const char* end = params + len;
	while (start < end && isspace((unsigned char)*start)) start++;
	while (end > start && isspace((unsigned char)*(end - 1))) end--;
	while (start < end) {
		const char* param_start = start, *param_end = start;
		while (param_end < end && *param_end != ',' && *param_end != '=') param_end++;
		while (param_end > param_start && isspace((unsigned char)*(param_end - 1))) param_end--;
		if (param_end <= param_start) return false;
		char* param_name = strndup(param_start, param_end - param_start);
		if (!param_name) return false;
		start = param_end;
		while (start < end && isspace((unsigned char)*start)) start++;
		if (start < end && *start == '=') {
			start++;
			while (start < end && isspace((unsigned char)*start)) start++;
			const char* value_start = start;
			while (start < end && *start != ',') start++;
			const char* value_end = start;
			while (value_end > value_start && isspace((unsigned char)*(value_end - 1))) value_end--;
			char* value = strndup(value_start, value_end - value_start);
			if (!value || !confignode_path_set_string(
				params_node, param_name, value
			)) {
				free(value);
				free(param_name);
				return false;
			}
			free(value);
		} else if (!confignode_path_set_string(params_node, param_name, "")) {
			free(param_name);
			return false;
		}
		free(param_name);
		if (start < end && *start == ',') start++;
		while (start < end && isspace((unsigned char)*start)) start++;
	}
	return true;
}

static bool parse_dtoverlay_line(const char* line, size_t len, const char* profile, confignode* dtree_node) {
	bool ret = false;
	char* params_str = NULL, *overlay_name = NULL;
	confignode* overlay_node = NULL;
	if (!line || !dtree_node || len == 0) return false;
	while (len > 0 && isspace((unsigned char)*line)) line++, len--;
	if (len == 0 || *line == '#') return true;
	const char* dtoverlay_prefix = "dtoverlay";
	size_t prefix_len = strlen(dtoverlay_prefix);
	if (len < prefix_len || strncasecmp(line, dtoverlay_prefix, prefix_len) != 0) return true;
	line += prefix_len, len -= prefix_len;
	if (len > 0 && *line != '.' && *line != '=' && !isspace((unsigned char)*line))
		return true;
	char *cur_profile = NULL;
	if (len > 0 && *line == '.') {
		line++, len--;
		const char* profile_start = line;
		size_t profile_len = 0;
		while (
			profile_len < len && line[profile_len] != '=' &&
			!isspace((unsigned char)line[profile_len])
		) profile_len++;
		if (profile_len > 0) {
			cur_profile = strndup(profile_start, profile_len);
			if (!cur_profile) goto fail;
			line += profile_len, len -= profile_len;
		}
	}
	while (len > 0 && isspace((unsigned char)*line)) line++, len--;
	if (len == 0 || *line != '=') goto fail;
	line++, len--;
	while (len > 0 && isspace((unsigned char)*line)) line++, len--;
	const char* overlay_start = line, *params_start = NULL;
	size_t overlay_name_len = 0, params_len = 0;
	while (overlay_name_len < len && line[overlay_name_len] != ',') overlay_name_len++;
	if (overlay_name_len < len && line[overlay_name_len] == ',')
		params_start = line + overlay_name_len + 1, params_len = len - overlay_name_len - 1;
	while (
		overlay_name_len > 0 &&
		isspace((unsigned char)overlay_start[overlay_name_len - 1])
	) overlay_name_len--;
	if (overlay_name_len == 0) goto fail;
	if (!(overlay_name = strndup(overlay_start, overlay_name_len))) goto fail;
	confignode *existing = confignode_map_get(dtree_node, overlay_name);
	if (existing && confignode_is_type(existing, CONFIGNODE_TYPE_MAP))
		overlay_node = confignode_copy(existing);
	else overlay_node = confignode_new_map();
	if (!overlay_node) goto fail;
	if (!confignode_path_set_bool(overlay_node, "enabled", true)) goto fail;
	if (cur_profile) {
		if (!confignode_path_set_string(overlay_node, "profile", cur_profile)) goto fail;
	} else if (profile) {
		if (!confignode_path_set_string(overlay_node, "profile", profile)) goto fail;
	}
	if (params_start && params_len > 0) {
		params_str = strndup(params_start, params_len);
		if (!params_str || !parse_overlay_params(
			params_str, params_len, overlay_node
		)) goto fail;
	}
	if (existing) {
		if (!confignode_replace(&existing, overlay_node)) goto fail;
	} else if (!confignode_map_set(dtree_node, overlay_name, overlay_node))
		goto fail;
	overlay_node = NULL;
	ret = true;
fail:
	if (overlay_node) confignode_clean(overlay_node);
	if (cur_profile) free(cur_profile);
	if (overlay_name) free(overlay_name);
	if (params_str) free(params_str);
	return ret;
}

/**
 * @brief Load device tree overlay configuration from buffer
 * @param node Configuration root node
 * @param buffer Configuration file content
 * @return EFI status code
 */
EFI_STATUS embloader_load_dtbo_config(confignode *node, const char *buffer) {
	if (!node || !buffer) return EFI_INVALID_PARAMETER;
	const char* line_start = buffer, *cur = buffer;
	char *profile = NULL;
	confignode* dtree_node = confignode_path_lookup(
		node, "devicetree.overlays", true
	);
	if (!dtree_node) return EFI_OUT_OF_RESOURCES;
	if (!confignode_is_type(dtree_node, CONFIGNODE_TYPE_MAP)) {
		confignode *replacement = confignode_new_map();
		if (!replacement) return EFI_OUT_OF_RESOURCES;
		if (!confignode_replace(&dtree_node, replacement)) {
			confignode_clean(replacement);
			return EFI_OUT_OF_RESOURCES;
		}
	}
	int lineno = 0;
	while (*cur) {
		lineno++;
		while (*cur && *cur != '\n' && *cur != '\r') cur++;
		const char *trimmed = line_start;
		size_t len = cur - line_start;
		while (len > 0 && isspace((unsigned char)*trimmed)) trimmed++, len--;
		while (len > 0 && isspace((unsigned char)trimmed[len - 1])) len--;
		if (len > 2 && trimmed[0] == '[' && trimmed[len - 1] == ']') {
			if (profile) free(profile);
			profile = strndup(trimmed + 1, len - 2);
			if (!profile) return EFI_OUT_OF_RESOURCES;
		} else if (len > 0 && !parse_dtoverlay_line(trimmed, len, profile, dtree_node)) {
			log_warning("failed to parse dtoverlay line %d", lineno);
			log_debug("line %d: '%.*s'", lineno, (int)len, trimmed);
		}
		if (*cur) {
			cur++;
			while (*cur == '\n' || *cur == '\r') cur++;
		}
		line_start = cur;
	}
	if (profile) free(profile);
	return EFI_SUCCESS;
}

/**
 * @brief Load device tree overlay configuration from file
 * @param node Configuration root node
 * @param base Base file protocol
 * @param path File path to load
 * @return EFI status code
 */
EFI_STATUS embloader_load_dtbo_config_path(confignode *node, EFI_FILE_PROTOCOL *base, const char *path) {
	EFI_STATUS status;
	char *buff = NULL;
	size_t flen = 0;
	if (!base || !path) return EFI_INVALID_PARAMETER;
	log_info("loading dtoverlay config from file %s", path);
	status = efi_open(base, &base, path, EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR(status) || !base) {
		if (status != EFI_NOT_FOUND) log_error(
			"open dtoverlay config file %s failed: %s",
			path, efi_status_to_string(status)
		);
		return status;
	}
	status = efi_file_read_all(base, (void**) &buff, &flen);
	base->Close(base);
	if (EFI_ERROR(status)) {
		log_error(
			"failed to read file %s: %s",
			path, efi_status_to_string(status)
		);
		return status;
	}
	status = embloader_load_dtbo_config(node, buff);
	free(buff);
	return status;
}
