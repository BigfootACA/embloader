#include "configfile.h"
#include "efi-utils.h"
#include "file-utils.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static bool parse_cmdline_line(const char* line, size_t len, const char* profile, confignode* array_node) {
	if (!line || !array_node || len == 0) return false;
	while (len > 0 && isspace(*line)) line++, len--;
	if (len == 0 || *line == '#') return true;
	const char* start = line;
	char *param;
	while (start < line + len) {
		while (start < line + len && isspace(*start)) start++;
		if (start >= line + len) break;
		const char* end = start;
		while (end < line + len && !isspace(*end)) end++;
		if (end > start && (param = strndup(start, end - start))) {
			confignode_array_append(array_node, confignode_new_string(param));
			free(param);
		}
		start = end;
	}
	return true;
}

/**
 * @brief Load command line configuration from buffer
 * @param node Configuration root node
 * @param buffer Configuration file content
 * @return EFI status code
 */
EFI_STATUS embloader_load_cmdline_config(confignode *node, const char *buffer) {
	if (!node || !buffer) return EFI_INVALID_PARAMETER;
	const char* line_start = buffer, *cur = buffer;
	char *profile = NULL;
	confignode* dtree_node = confignode_path_lookup(
		node, "menu.bootargs", true
	);
	if (!dtree_node) return EFI_OUT_OF_RESOURCES;
	if (!confignode_is_type(dtree_node, CONFIGNODE_TYPE_ARRAY))
		confignode_replace(&dtree_node, confignode_new_array());
	int lineno = 0;
	while (*cur) {
		while (*cur && *cur != '\n' && *cur != '\r') cur++;
		size_t len = cur - line_start;
		if (len > 0 && !parse_cmdline_line(line_start, len, profile, dtree_node)) {
			log_warning("failed to parse cmdline line %d", lineno + 1);
			log_debug("line %d: '%.*s'", lineno, (int)len, line_start);
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
 * @brief Load command line configuration from file
 * @param node Configuration root node
 * @param base Base file protocol
 * @param path File path to load
 * @return EFI status code
 */
EFI_STATUS embloader_load_cmdline_config_path(confignode *node, EFI_FILE_PROTOCOL *base, const char *path) {
	EFI_STATUS status;
	char *buff = NULL;
	size_t flen = 0;
	if (!base || !path) return EFI_INVALID_PARAMETER;
	log_info("loading cmdline config from file %s", path);
	status = efi_open(base, &base, path, EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR(status) || !base) {
		if (status != EFI_NOT_FOUND) log_error(
			"open cmdline config file %s failed: %s",
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
	status = embloader_load_cmdline_config(node, buff);
	free(buff);
	return status;
}
