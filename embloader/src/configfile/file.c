#include <Protocol/SimpleFileSystem.h>
#include "internal.h"
#include "file-utils.h"
#include "efi-utils.h"
#include "log.h"

/**
 * @brief Load a confignode tree from a string buffer in the specified format.
 * This function dispatches to the appropriate format-specific loader based on
 * the type parameter. Currently supports JSON and YAML formats.
 *
 * @param type the format type of the configuration data
 * @param buff the configuration data as a null-terminated string
 * @return newly allocated confignode tree, or NULL on parse error, unsupported
 * format, or allocation failure
 *
 */
confignode* configfile_load_string(configfile_type type, const char* buff) {
	switch (type) {
		case CONFIGFILE_TYPE_JSON:
			return configfile_json_load_string(buff);
		case CONFIGFILE_TYPE_YAML:
			return configfile_yaml_load_string(buff);
		default:
			return NULL;
	}
	return NULL;
}

/**
 * @brief Save a confignode tree to a string buffer in the specified format.
 * This function dispatches to the appropriate format-specific serializer based
 * on the type parameter. Currently supports JSON and YAML formats.
 *
 * @param type the format type for the output
 * @param node the confignode tree to serialize
 * @return newly allocated string containing the serialized data that must be
 * freed by caller, or NULL on failure
 *
 */
char* configfile_save_string(configfile_type type, confignode* node) {
	switch (type) {
		case CONFIGFILE_TYPE_JSON:
			return configfile_json_save_string(node);
		case CONFIGFILE_TYPE_YAML:
			return configfile_yaml_save_string(node);
		default:
			return NULL;
	}
}

/**
 * @brief Print a confignode tree to output using a custom print function.
 * This function converts the confignode tree to a JSON object and prints it as
 * a formatted string using the provided print function. Primarily useful for
 * debugging purposes where you need to control the output destination (e.g.,
 * console, log file, etc.).
 *
 * @param node the confignode tree to print
 * @param print function pointer to handle the actual output (e.g., printf,
 * custom logger)
 *
 */
void confignode_print(confignode* node, int (*print)(const char*)) {
	if (!node) return;
	json_object* obj = confignode_to_json(node);
	if (!obj) return;
	const char* oj = json_object_to_json_string_ext(
		obj, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
	if (oj) print(oj);
	json_object_put(obj);
}

/**
 * @brief Load a confignode tree from an EFI file in the specified format.
 * This function reads the entire contents of an EFI file into memory and then
 * parses it using the appropriate format-specific loader. The file data is
 * automatically freed after parsing.
 *
 * The function supports all formats that configfile_load_string supports,
 * including JSON and YAML. This is a convenience function that combines
 * file I/O with configuration parsing for UEFI environments.
 *
 * @param type the format type of the configuration file
 * @param file pointer to an opened EFI_FILE_PROTOCOL for reading
 * @return newly allocated confignode tree, or NULL on file read error, parse
 * error, or allocation failure
 *
 * @see configfile_load_string() for string-based parsing
 * @see efi_file_read_all() for the underlying file reading operation
 *
 */
confignode* configfile_load_efi_file(configfile_type type, EFI_FILE_PROTOCOL* file) {
	if (!file) return NULL;
	char* data = NULL;
	EFI_STATUS status = efi_file_read_all(file, (void**)&data, NULL);
	if (EFI_ERROR(status) || !data) return NULL;
	confignode* node = configfile_load_string(type, data);
	free(data);
	return node;
}

/**
 * @brief Load a confignode tree from an EFI file at a specified path.
 * This function opens the specified file path relative to the provided base
 * directory, reads its contents, and parses it into a confignode tree using
 * the specified format type. The file is automatically closed after reading.
 *
 * @param type the format type of the configuration file
 * @param base pointer to an opened EFI_FILE_PROTOCOL representing the base
 * directory
 * @param path the relative path to the configuration file within the base
 * directory
 * @return newly allocated confignode tree, or NULL on file open/read error,
 * parse error, or allocation failure
 *
 * @see configfile_load_efi_file() for file reading and parsing
 * @see efi_open() for the underlying file opening operation
 *
 */
confignode* configfile_load_efi_file_path(
	configfile_type type,
	EFI_FILE_PROTOCOL* base,
	const char* path
) {
	if (!base || !path) return NULL;
	EFI_FILE_PROTOCOL* file;
	EFI_STATUS status = efi_open(
		base, &file, path,
		EFI_FILE_MODE_READ, 0
	);
	if (EFI_ERROR(status) || !file) {
		if (status != EFI_NOT_FOUND) log_warning(
			"open config file %s failed: %s",
			path, efi_status_to_string(status)
		);
		return NULL;
	}
	confignode* node = configfile_load_efi_file(type, file);
	file->Close(file);
	return node;
}

/**
 * @brief Get a copy of the value from a value-type config node.
 * For string values, the returned string is a duplicate that must be freed by
 * the caller.
 *
 * @param node the node to get value from
 * @param out pointer to confignode_value struct to receive the value
 * @return true on success, false if node is NULL, not a value type, or memory
 * allocation failed
 *
 */
configfile_type configfile_type_from_ext(const char* ext) {
	if (!ext) return CONFIGFILE_TYPE_UNKNOWN;
	if (strcasecmp(ext, "json") == 0 || strcasecmp(ext, "jsn") == 0)
		return CONFIGFILE_TYPE_JSON;
	if (strcasecmp(ext, "yaml") == 0 || strcasecmp(ext, "yml") == 0)
		return CONFIGFILE_TYPE_YAML;
	if (strcasecmp(ext, "conf") == 0)
		return CONFIGFILE_TYPE_CONF;
	return CONFIGFILE_TYPE_UNKNOWN;
}

/**
 * @brief Guess the configuration file type based on its filename extension.
 * Recognizes .json, .jsn, .yaml, .yml, and .conf extensions (case-insensitive).
 *
 * @param filename the name of the file to check
 * @return the guessed configfile_type, or CONFIGFILE_TYPE_UNKNOWN if the
 * extension is unrecognized or filename is NULL
 *
 */
configfile_type configfile_type_guess(const char* filename) {
	if (!filename) return CONFIGFILE_TYPE_UNKNOWN;
	const char* ext = strrchr(filename, '.');
	if (!ext || ext == filename) return CONFIGFILE_TYPE_UNKNOWN;
	ext++;
	return configfile_type_from_ext(ext);
}
