#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include "internal.h"
#include "log.h"
#include "str-utils.h"

/**
 * @brief Parse a boolean value from a string.
 *
 * Accepts common boolean representations like "true", "false", "yes", "no",
 * "on", "off" (case insensitive).
 *
 * @param name the name of the configuration field for error logging
 * @param str the input string to parse (must not be NULL)
 * @param len the length of the input string
 * @param out pointer to store the parsed boolean value (must not be NULL)
 * @return true on successful parsing, false on invalid input
 */
bool sdboot_parse_bool(
	const char *name,
	const char *str,
	size_t len,
	bool *out
) {
	if (!name || !str || len == 0 || !out) return false;
	if (stringn_is_true(str, len) || str_match(str, len, "1")) {
		*out = true;
		return true;
	}
	if (stringn_is_false(str, len) || str_match(str, len, "0")) {
		*out = false;
		return true;
	}
	log_warning(
		"invalid boolean value for '%s': %.*s",
		name, (int)len, str
	);
	return false;
}

/**
 * @brief Parse an integer value from a string.
 *
 * Supports decimal, hexadecimal (0x prefix), and octal (0 prefix) formats.
 *
 * @param name the name of the configuration field for error logging
 * @param str the input string to parse (must not be NULL)
 * @param len the length of the input string
 * @param out pointer to store the parsed integer value (must not be NULL)
 * @return parse status distinguishing invalid input from allocation failure
 */
sdboot_parse_status sdboot_parse_int_status(
	const char *name,
	const char *str,
	size_t len,
	int *out
) {
	if (!name || !str || len == 0 || !out) return SDBOOT_PARSE_INVALID;
	char *value = strndup(str, len);
	if (!value) return SDBOOT_PARSE_OUT_OF_RESOURCES;
	char *end = NULL;
	errno = 0;
	long v = strtol(value, &end, 0);
	bool valid =
		end != value && (size_t)(end - value) == len && *end == 0 &&
		errno != ERANGE && v >= INT_MIN && v <= INT_MAX;
	free(value);
	if (!valid) {
		log_warning(
			"invalid integer value for '%s': %.*s",
			name, (int)len, str
		);
		return SDBOOT_PARSE_INVALID;
	}
	*out = (int)v;
	return SDBOOT_PARSE_SUCCESS;
}

bool sdboot_parse_int(
	const char *name,
	const char *str,
	size_t len,
	int *out
) {
	return sdboot_parse_int_status(name, str, len, out) == SDBOOT_PARSE_SUCCESS;
}

/**
 * @brief Parse a console mode value from a string.
 *
 * Supports numeric modes (0, 1, 2) and named modes (auto, max, keep).
 *
 * @param str the input string to parse (must not be NULL)
 * @param len the length of the input string
 * @param out pointer to store the parsed console mode (must not be NULL)
 * @return true on successful parsing, false on invalid input
 */
bool sdboot_parse_console_mode(
	const char *str,
	size_t len,
	sdboot_console_mode *out
) {
	if (!str || len == 0 || !out) return false;
	if (str_match(str, len, "0"))
		*out = MODE_UEFI_80_25;
	else if (str_match(str, len, "1"))
		*out = MODE_UEFI_80_50;
	else if (str_match(str, len, "2"))
		*out = MODE_FIRMWARE_FIRST;
	else if (str_case_match(str, len, "auto"))
		*out = MODE_AUTO;
	else if (str_case_match(str, len, "max"))
		*out = MODE_MAX;
	else if (str_case_match(str, len, "keep"))
		*out = MODE_KEEP;
	else {
		log_warning(
			"invalid console mode: %.*s",
			(int)len, str
		);
		return false;
	}
	return true;
}

/**
 * @brief Parse SecureBoot enrollment policy from a string.
 *
 * Accepts values: "off", "manual", "if-safe", "force" (case insensitive).
 * Used for systemd-boot compatible SecureBoot key enrollment configuration.
 *
 * @param str the input string to parse (must not be NULL)
 * @param len the length of the input string
 * @param out pointer to store the parsed enrollment policy (must not be NULL)
 * @return true on successful parsing, false on invalid input
 */
bool sdboot_parse_secureboot_enroll(
	const char *str,
	size_t len,
	sdboot_secureboot_enroll *out
) {
	if (!str || len == 0 || !out) return false;
	if (str_case_match(str, len, "off"))
		*out = ENROLL_OFF;
	else if (str_case_match(str, len, "manual"))
		*out = ENROLL_MANUAL;
	else if (str_case_match(str, len, "if-safe"))
		*out = ENROLL_IF_SAFE;
	else if (str_case_match(str, len, "force"))
		*out = ENROLL_FORCE;
	else {
		log_warning(
			"invalid secureboot enroll mode: %.*s",
			(int)len, str
		);
		return false;
	}
	return true;
}

/**
 * @brief Parse processor architecture from a string.
 *
 * Supports: "x64" (x86_64), "aa64" (ARM64), "arm", "ia32" (x86),
 * "riscv64", "loongarch64" (case insensitive).
 * Used for systemd-boot compatible architecture specification.
 *
 * @param str the input string to parse (must not be NULL)
 * @param len the length of the input string
 * @param out pointer to store the parsed architecture (must not be NULL)
 * @return true on successful parsing, false on invalid input
 */
bool sdboot_parse_arch(
	const char *str,
	size_t len,
	embloader_arch *out
) {
	if (!str || len == 0 || !out) return false;
	if (str_case_match(str, len, "x64"))
		*out = ARCH_X86_64;
	else if (str_case_match(str, len, "aa64"))
		*out = ARCH_ARM64;
	else if (str_case_match(str, len, "arm"))
		*out = ARCH_ARM;
	else if (str_case_match(str, len, "ia32"))
		*out = ARCH_X86;
	else if (str_case_match(str, len, "riscv64"))
		*out = ARCH_RISCV64;
	else if (str_case_match(str, len, "loongarch64"))
		*out = ARCH_LOONGARCH64;
	else {
		log_warning(
			"invalid architecture: %.*s",
			(int)len, str
		);
		return false;
	}
	return true;
}

/**
 * @brief Parse and duplicate a string value
 *
 * Creates a null-terminated copy of the input string.
 * Memory management: caller is responsible for freeing the returned string.
 *
 * @param str the input string to parse (must not be NULL)
 * @param len the length of the input string (must be > 0)
 * @param out pointer to store the duplicated string (must not be NULL)
 * @return parse status distinguishing invalid input from allocation failure
 */
sdboot_parse_status sdboot_parse_string_status(
	const char *str,
	size_t len,
	char **out
) {
	if (!str || len == 0 || !out) return SDBOOT_PARSE_INVALID;
	char *value = strndup(str, len);
	if (!value) return SDBOOT_PARSE_OUT_OF_RESOURCES;
	free(*out);
	*out = value;
	return SDBOOT_PARSE_SUCCESS;
}

bool sdboot_parse_string(const char *str, size_t len, char **out) {
	return sdboot_parse_string_status(str, len, out) == SDBOOT_PARSE_SUCCESS;
}

/**
 * @brief Parse a whitespace-separated list of strings
 *
 * Splits input string on any whitespace characters (spaces, tabs, etc.)
 * and creates a list of duplicated string tokens.
 * Memory management: caller is responsible for freeing the list and its contents.
 *
 * @param str the input string to parse (must not be NULL)
 * @param len the length of the input string (must be > 0)
 * @param out pointer to store the created list (must not be NULL)
 * @return parse status distinguishing empty input from allocation failure
 */
sdboot_parse_status sdboot_parse_string_list_status(
	const char *str,
	size_t len,
	list **out
) {
	if (!str || len == 0 || !out) return SDBOOT_PARSE_INVALID;
	size_t off = 0;
	int token_count = 0;
	list *values = NULL;
	while (off < len) {
		while (off < len && isspace((unsigned char)str[off])) off++;
		if (off >= len) break;
		const char *token_start = str + off;
		size_t token_len = 0;
		while (
			off + token_len < len &&
			!isspace((unsigned char)str[off + token_len])
		)
			token_len++;
		if (token_len > 0) {
			int result = list_obj_add_new_strndup(
				&values, token_start, token_len
			);
			if (result != 0) {
				list_free_all_def(values);
				return SDBOOT_PARSE_OUT_OF_RESOURCES;
			}
			token_count++;
		}
		off += token_len;
	}
	if (token_count == 0) return SDBOOT_PARSE_INVALID;
	if (list_obj_add(out, values) != 0) {
		list_free_all_def(values);
		return SDBOOT_PARSE_INVALID;
	}
	return SDBOOT_PARSE_SUCCESS;
}

bool sdboot_parse_string_list(const char *str, size_t len, list **out) {
	return sdboot_parse_string_list_status(str, len, out) == SDBOOT_PARSE_SUCCESS;
}

/**
 * @brief Parse timeout value from a string.
 *
 * Supports integer seconds and special values:
 * "menu-force" (-1), "menu-hidden" (0), "menu-disabled" (0).
 *
 * @param str the input string to parse (must not be NULL)
 * @param len the length of the input string
 * @param out pointer to store the parsed timeout value (must not be NULL)
 * @return parse status distinguishing invalid input from allocation failure
 */
sdboot_parse_status sdboot_parse_timeout_status(
	const char *str,
	size_t len,
	int *out
) {
	sdboot_parse_status status;
	if (str_case_match(str, len, "menu-force"))
		*out = -1;
	else if (str_case_match(str, len, "menu-hidden"))
		*out = 0;
	else if (str_case_match(str, len, "menu-disabled"))
		*out = 0;
	else {
		status = sdboot_parse_int_status("timeout", str, len, out);
		if (status != SDBOOT_PARSE_SUCCESS) return status;
		if (*out < 0) {
			log_warning(
				"invalid integer value for 'timeout': %.*s",
				(int)len, str
			);
			return SDBOOT_PARSE_INVALID;
		}
	}
	return SDBOOT_PARSE_SUCCESS;
}

bool sdboot_parse_timeout(
	const char *str,
	size_t len,
	int *out
) {
	return sdboot_parse_timeout_status(str, len, out) == SDBOOT_PARSE_SUCCESS;
}
