#include "configfile.h"
#include "efi-utils.h"
#include "file-utils.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef enum dtoverlay_parse_result {
	DTOVERLAY_PARSE_IGNORED,
	DTOVERLAY_PARSE_SUCCESS,
	DTOVERLAY_PARSE_SYNTAX_ERROR,
	DTOVERLAY_PARSE_WRITE_ERROR,
} dtoverlay_parse_result;

static dtoverlay_parse_result parse_overlay_params(
	const char* params,
	size_t len,
	confignode* overlay_node
) {
	if (!params || !overlay_node) return DTOVERLAY_PARSE_WRITE_ERROR;
	if (len == 0) return DTOVERLAY_PARSE_SYNTAX_ERROR;
	confignode* params_node = confignode_path_lookup(overlay_node, "params", true);
	if (!params_node) return DTOVERLAY_PARSE_WRITE_ERROR;
	if (!confignode_is_type(params_node, CONFIGNODE_TYPE_MAP)) {
		confignode *replacement = confignode_new_map();
		if (!replacement) return DTOVERLAY_PARSE_WRITE_ERROR;
		if (!confignode_replace(&params_node, replacement)) {
			confignode_clean(replacement);
			return DTOVERLAY_PARSE_WRITE_ERROR;
		}
	}
	const char* start = params;
	const char* end = params + len;
	while (start < end && isspace((unsigned char)*start)) start++;
	while (end > start && isspace((unsigned char)*(end - 1))) end--;
	if (start == end) return DTOVERLAY_PARSE_SYNTAX_ERROR;
	while (start < end) {
		const char* param_start = start, *param_end = start;
		while (param_end < end && *param_end != ',' && *param_end != '=') param_end++;
		while (param_end > param_start && isspace((unsigned char)*(param_end - 1))) param_end--;
		if (param_end <= param_start) return DTOVERLAY_PARSE_SYNTAX_ERROR;
		char* param_name = strndup(param_start, param_end - param_start);
		if (!param_name) return DTOVERLAY_PARSE_WRITE_ERROR;
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
				return DTOVERLAY_PARSE_WRITE_ERROR;
			}
			free(value);
		} else if (!confignode_path_set_string(params_node, param_name, "")) {
			free(param_name);
			return DTOVERLAY_PARSE_WRITE_ERROR;
		}
		free(param_name);
		if (start < end && *start == ',') {
			start++;
			while (start < end && isspace((unsigned char)*start)) start++;
			if (start == end) return DTOVERLAY_PARSE_SYNTAX_ERROR;
		}
		while (start < end && isspace((unsigned char)*start)) start++;
	}
	return DTOVERLAY_PARSE_SUCCESS;
}

static dtoverlay_parse_result parse_dtoverlay_line(
	const char* line,
	size_t len,
	const char* profile,
	confignode* dtree_node
) {
	dtoverlay_parse_result ret = DTOVERLAY_PARSE_WRITE_ERROR;
	char* overlay_name = NULL;
	confignode* overlay_node = NULL;
	if (!line || !dtree_node) return DTOVERLAY_PARSE_WRITE_ERROR;
	while (len > 0 && isspace((unsigned char)*line)) line++, len--;
	if (len == 0 || *line == '#') return DTOVERLAY_PARSE_IGNORED;
	const char* dtoverlay_prefix = "dtoverlay";
	size_t prefix_len = strlen(dtoverlay_prefix);
	if (
		len < prefix_len ||
		strncasecmp(line, dtoverlay_prefix, prefix_len) != 0
	) return DTOVERLAY_PARSE_IGNORED;
	line += prefix_len, len -= prefix_len;
	if (len > 0 && *line != '.' && *line != '=' && !isspace((unsigned char)*line))
		return DTOVERLAY_PARSE_IGNORED;
	char *cur_profile = NULL;
	if (len > 0 && *line == '.') {
		line++, len--;
		const char* profile_start = line;
		size_t profile_len = 0;
		while (
			profile_len < len && line[profile_len] != '=' &&
			!isspace((unsigned char)line[profile_len])
		) profile_len++;
		if (profile_len == 0) {
			ret = DTOVERLAY_PARSE_SYNTAX_ERROR;
			goto fail;
		}
		cur_profile = strndup(profile_start, profile_len);
		if (!cur_profile) goto fail;
		line += profile_len, len -= profile_len;
	}
	while (len > 0 && isspace((unsigned char)*line)) line++, len--;
	if (len == 0 || *line != '=') {
		ret = DTOVERLAY_PARSE_SYNTAX_ERROR;
		goto fail;
	}
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
	if (overlay_name_len == 0) {
		ret = DTOVERLAY_PARSE_SYNTAX_ERROR;
		goto fail;
	}
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
	if (params_start) {
		dtoverlay_parse_result params_result = parse_overlay_params(
			params_start, params_len, overlay_node
		);
		if (params_result != DTOVERLAY_PARSE_SUCCESS) {
			ret = params_result;
			goto fail;
		}
	}
	if (existing) {
		if (!confignode_replace(&existing, overlay_node)) goto fail;
	} else if (!confignode_map_set(dtree_node, overlay_name, overlay_node))
		goto fail;
	overlay_node = NULL;
	ret = DTOVERLAY_PARSE_SUCCESS;
fail:
	if (overlay_node) confignode_clean(overlay_node);
	if (cur_profile) free(cur_profile);
	if (overlay_name) free(overlay_name);
	return ret;
}

/**
 * @brief Load device tree overlay configuration from buffer
 * @param node Configuration root node
 * @param buffer Configuration file content
 * @return EFI status code
 */
EFI_STATUS embloader_load_dtbo_config(confignode *node, const char *buffer) {
	if (!node || !buffer || !confignode_is_type(
		node, CONFIGNODE_TYPE_MAP
	)) return EFI_INVALID_PARAMETER;
	const char* line_start = buffer, *cur = buffer;
	char *profile = NULL;
	EFI_STATUS status = EFI_OUT_OF_RESOURCES;
	confignode *old_dtree = confignode_map_get(node, "devicetree");
	confignode *dtree_node = old_dtree && confignode_is_type(
		old_dtree, CONFIGNODE_TYPE_MAP
	) ? confignode_copy(old_dtree) : confignode_new_map();
	if (!dtree_node) return EFI_OUT_OF_RESOURCES;
	confignode *overlays = confignode_map_get(dtree_node, "overlays");
	if (!overlays || !confignode_is_type(overlays, CONFIGNODE_TYPE_MAP)) {
		confignode *replacement = confignode_new_map();
		if (!replacement || !confignode_map_set(
			dtree_node, "overlays", replacement
		)) {
			if (replacement) confignode_clean(replacement);
			goto fail;
		}
		overlays = replacement;
	}
	int lineno = 0;
	while (*cur) {
		lineno++;
		while (*cur && *cur != '\n' && *cur != '\r') cur++;
		const char *trimmed = line_start;
		size_t len = cur - line_start;
		while (len > 0 && isspace((unsigned char)*trimmed)) trimmed++, len--;
		while (len > 0 && isspace((unsigned char)trimmed[len - 1])) len--;
		if (len > 0 && trimmed[0] == '[') {
			if (len <= 2 || trimmed[len - 1] != ']') {
				status = EFI_COMPROMISED_DATA;
				goto parse_fail;
			}
			char *new_profile = strndup(trimmed + 1, len - 2);
			if (!new_profile) goto parse_fail;
			if (profile) free(profile);
			profile = new_profile;
		} else if (len > 0) {
			dtoverlay_parse_result result = parse_dtoverlay_line(
				trimmed, len, profile, overlays
			);
			if (result == DTOVERLAY_PARSE_SYNTAX_ERROR) {
				status = EFI_COMPROMISED_DATA;
				goto parse_fail;
			}
			if (result == DTOVERLAY_PARSE_WRITE_ERROR) goto parse_fail;
		}
		if (*cur) {
			cur++;
			while (*cur == '\n' || *cur == '\r') cur++;
		}
		line_start = cur;
		continue;
parse_fail:
		if (status == EFI_COMPROMISED_DATA)
			log_warning("invalid dtoverlay syntax on line %d", lineno);
		else log_error("failed to store dtoverlay from line %d", lineno);
		log_debug("line %d: '%.*s'", lineno, (int)len, trimmed);
		goto fail;
	}
	if (old_dtree) {
		if (!confignode_replace(&old_dtree, dtree_node)) goto fail;
	} else if (!confignode_map_set(node, "devicetree", dtree_node)) goto fail;
	dtree_node = NULL;
	if (profile) free(profile);
	return EFI_SUCCESS;
fail:
	if (profile) free(profile);
	confignode_clean(dtree_node);
	return status;
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
