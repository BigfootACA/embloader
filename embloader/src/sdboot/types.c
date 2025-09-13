#include <ctype.h>
#include "sdboot.h"
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
	if (stringn_is_true(str, len)) {
		*out = true;
		return true;
	}
	if (stringn_is_false(str, len)) {
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
 * @return true on successful parsing, false on invalid input or overflow
 */
bool sdboot_parse_int(
	const char *name,
	const char *str,
	size_t len,
	int *out
) {
	char *end = NULL;
	long v = strtol(str, &end, 0);
	if (!end || end == str || (size_t)(end - str) != len) {
		log_warning(
			"invalid integer value for '%s': %.*s",
			name, (int)len, str
		);
		return false;
	}
	*out = (int)v;
	return true;
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
 * @return true on successful allocation, false on memory allocation failure
 */
bool sdboot_parse_string(const char *str, size_t len, char **out) {
	if (!str || len == 0 || !out) return false;
	*out = strndup(str, len);
	return *out != NULL;
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
 * @return true if at least one token was parsed, false on error or empty input
 */
bool sdboot_parse_string_list(const char *str, size_t len, list **out) {
	if (!str || len == 0 || !out) return false;
	size_t off = 0;
	int token_count = 0;
	while (off < len) {
		while (off < len && isspace(str[off])) off++;
		if (off >= len) break;
		const char *token_start = str + off;
		size_t token_len = 0;
		while (off + token_len < len && !isspace(str[off + token_len]))
			token_len++;
		if (token_len > 0) {
			int result = list_obj_add_new_strndup(out, token_start, token_len);
			if (result != 0) return false;
			token_count++;
		}
		off += token_len;
	}
	return token_count > 0;
}
