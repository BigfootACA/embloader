#include <ctype.h>
#include "str-utils.h"
#include "internal.h"
#include "log.h"

/**
 * @brief Parse a single boot menu configuration key-value pair.
 *
 * Processes systemd-boot compatible menu configuration options like
 * "default", "timeout", "console-mode", "editor", etc.
 *
 * @param menu the boot menu structure to update (must not be NULL)
 * @param key the configuration key string (must not be NULL)
 * @param key_len the length of the key string
 * @param value the configuration value string (must not be NULL)
 * @param value_len the length of the value string
 * @return detailed parse status (unknown keys remain successful)
 */
sdboot_parse_status sdboot_boot_parse_menu_one(
	sdboot_boot_menu *menu,
	const char *key, size_t key_len,
	const char *value, size_t value_len
) {
	if (str_case_match(key, key_len, "default"))
		return sdboot_parse_string_status(
			value, value_len, &menu->default_entry
		);
	if (str_case_match(key, key_len, "timeout"))
		return sdboot_parse_timeout_status(value, value_len, &menu->timeout);
	if (str_case_match(key, key_len, "console-mode"))
		return sdboot_parse_console_mode(
			value, value_len, &menu->console_mode
		) ? SDBOOT_PARSE_SUCCESS : SDBOOT_PARSE_INVALID;
	if (str_case_match(key, key_len, "editor"))
		return sdboot_parse_bool(
			"editor", value, value_len, &menu->editor
		) ? SDBOOT_PARSE_SUCCESS : SDBOOT_PARSE_INVALID;
	if (str_case_match(key, key_len, "auto-entries"))
		return sdboot_parse_bool(
			"auto-entries", value, value_len, &menu->auto_entries
		) ? SDBOOT_PARSE_SUCCESS : SDBOOT_PARSE_INVALID;
	if (str_case_match(key, key_len, "auto-firmware"))
		return sdboot_parse_bool(
			"auto-firmware", value, value_len, &menu->auto_firmware
		) ? SDBOOT_PARSE_SUCCESS : SDBOOT_PARSE_INVALID;
	if (str_case_match(key, key_len, "auto-reboot"))
		return sdboot_parse_bool(
			"auto-reboot", value, value_len, &menu->auto_reboot
		) ? SDBOOT_PARSE_SUCCESS : SDBOOT_PARSE_INVALID;
	if (str_case_match(key, key_len, "auto-poweroff"))
		return sdboot_parse_bool(
			"auto-poweroff", value, value_len, &menu->auto_poweroff
		) ? SDBOOT_PARSE_SUCCESS : SDBOOT_PARSE_INVALID;
	if (str_case_match(key, key_len, "beep"))
		return sdboot_parse_bool(
			"beep", value, value_len, &menu->beep
		) ? SDBOOT_PARSE_SUCCESS : SDBOOT_PARSE_INVALID;
	if (str_case_match(key, key_len, "secureboot-enroll"))
		return sdboot_parse_secureboot_enroll(
			value, value_len, &menu->secureboot_enroll
		) ? SDBOOT_PARSE_SUCCESS : SDBOOT_PARSE_INVALID;
	if (str_case_match(key, key_len, "reboot-for-bitlocker"))
		return sdboot_parse_bool(
			"reboot-for-bitlocker", value, value_len,
			&menu->reboot_for_bitlocker
		) ? SDBOOT_PARSE_SUCCESS : SDBOOT_PARSE_INVALID;
	log_warning("unknown boot menu option: %.*s", (int)key_len, key);
	return SDBOOT_PARSE_SUCCESS;
}

/**
 * @brief Parse a single boot loader entry configuration key-value pair.
 *
 * Processes systemd-boot compatible loader entry options like
 * "title", "linux", "initrd", "options", "devicetree", etc.
 *
 * @param loader the boot loader structure to update (must not be NULL)
 * @param key the configuration key string (must not be NULL)
 * @param key_len the length of the key string
 * @param value the configuration value string (must not be NULL)
 * @param value_len the length of the value string
 * @return detailed parse status (unknown keys remain successful)
 */
sdboot_parse_status sdboot_boot_parse_loader_one(
	sdboot_boot_loader *loader,
	const char *key, size_t key_len,
	const char *value, size_t value_len
) {
	if (str_case_match(key, key_len, "title"))
		return sdboot_parse_string_status(value, value_len, &loader->title);
	if (str_case_match(key, key_len, "version"))
		return sdboot_parse_string_status(value, value_len, &loader->version);
	if (str_case_match(key, key_len, "machine-id"))
		return sdboot_parse_string_status(
			value, value_len, &loader->machine_id
		);
	if (str_case_match(key, key_len, "sort-key"))
		return sdboot_parse_string_status(value, value_len, &loader->sort_key);
	if (str_case_match(key, key_len, "linux"))
		return sdboot_parse_string_status(value, value_len, &loader->kernel);
	if (str_case_match(key, key_len, "initrd"))
		return sdboot_parse_string_list_status(
			value, value_len, &loader->initramfs
		);
	if (str_case_match(key, key_len, "efi"))
		return sdboot_parse_string_status(value, value_len, &loader->efi);
	if (str_case_match(key, key_len, "options"))
		return sdboot_parse_string_list_status(
			value, value_len, &loader->options
		);
	if (str_case_match(key, key_len, "devicetree"))
		return sdboot_parse_string_status(
			value, value_len, &loader->devicetree
		);
	if (str_case_match(key, key_len, "devicetree-overlay"))
		return sdboot_parse_string_list_status(
			value, value_len, &loader->dtoverlay
		);
	if (str_case_match(key, key_len, "architecture"))
		return sdboot_parse_arch(
			value, value_len, &loader->arch
		) ? SDBOOT_PARSE_SUCCESS : SDBOOT_PARSE_INVALID;
	log_warning("unknown boot loader entry option: %.*s", (int)key_len, key);
	return SDBOOT_PARSE_SUCCESS;
}

/**
 * @brief Parse a single configuration line from a systemd-boot config file.
 *
 * Handles key-value parsing, comment stripping, whitespace normalization,
 * and delegates to appropriate menu or loader parsing functions.
 *
 * @param line the input line to parse (must not be NULL)
 * @param len the length of the input line
 * @param menu the menu structure to update (exactly one of menu/loader must be NULL)
 * @param loader the loader structure to update (exactly one of menu/loader must be NULL)
 * @return detailed parse status; comments and empty lines are successful
 */
sdboot_parse_status sdboot_boot_parse_line(
	const char *line,
	size_t len,
	sdboot_boot_menu *menu,
	sdboot_boot_loader *loader
) {
	char *p;
	sdboot_parse_status ret = SDBOOT_PARSE_INVALID;
	if (!line) return SDBOOT_PARSE_INVALID;
	if (len == 0) return SDBOOT_PARSE_SUCCESS;
	if ((!menu && !loader) || (menu && loader)) return SDBOOT_PARSE_INVALID;
	const char *orig_line = line;
	size_t orig_len = len;
	while (len > 0 && isspace((unsigned char)line[len - 1])) len--;
	while (len > 0 && isspace((unsigned char)line[0])) line++, len--;
	if (len == 0 || line[0] == '#') return SDBOOT_PARSE_SUCCESS;
	const char *sep = line;
	for (; sep < line + len && !isspace((unsigned char)*sep); sep++);
	if (sep >= line + len) return SDBOOT_PARSE_INVALID;
	const char *key = line, *value = sep;
	while (value < line + len && isspace((unsigned char)*value)) value++;
	if (value >= line + len) return SDBOOT_PARSE_INVALID;
	size_t key_len = sep - line, value_len = line + len - value;
	if ((p = memchr(value, '#', value_len))) value_len = p - value;
	while (
		value_len > 0 &&
		isspace((unsigned char)value[value_len - 1])
	) value_len--;
	if (key_len == 0 || value_len == 0) return SDBOOT_PARSE_INVALID;
	if (menu) ret = sdboot_boot_parse_menu_one(menu, key, key_len, value, value_len);
	if (loader) ret = sdboot_boot_parse_loader_one(loader, key, key_len, value, value_len);
	if (ret != SDBOOT_PARSE_SUCCESS) log_warning(
		"failed to apply config line: '%.*s'", (int)orig_len, orig_line
	);
	return ret;
}

static EFI_STATUS sdboot_boot_parse_content_into(
	const char *fname,
	const char *content,
	size_t len,
	sdboot_boot_menu *menu,
	sdboot_boot_loader *loader
) {
	size_t off = 0;
	int line = 0;
	bool have_success = false;
	while (off < len) {
		line++;
		size_t llen = 0;
		const char *line_start = content + off;
		while (off + llen < len && content[off + llen] != '\n') llen++;
		sdboot_parse_status result = sdboot_boot_parse_line(
			line_start, llen, menu, loader
		);
		if (result == SDBOOT_PARSE_OUT_OF_RESOURCES)
			return EFI_OUT_OF_RESOURCES;
		if (result == SDBOOT_PARSE_INVALID) {
			log_warning(
				"failed to parse sdboot config %s at line %d", fname, line
			);
			log_debug(
				"line %d: \"%.*s\" (%llu chars)",
				line, (int)llen, line_start, (unsigned long long)llen
			);
		} else have_success = true;
		off += llen;
		if (off < len && content[off] == '\n') off++;
	}
	return have_success ? EFI_SUCCESS : EFI_LOAD_ERROR;
}

/**
 * @brief Parse complete systemd-boot configuration file content.
 *
 * Processes entire file content line by line, handling various line endings
 * and providing detailed error reporting with line numbers.
 *
 * @param fname filename for error reporting (must not be NULL)
 * @param content the file content to parse (must not be NULL)
 * @param len the length of the content (must be > 0)
 * @param menu the menu structure to update (exactly one of menu/loader must be NULL)
 * @param loader the loader structure to update (exactly one of menu/loader must be NULL)
 * @return EFI_SUCCESS if any line is accepted, or the relevant EFI error status
 */
EFI_STATUS sdboot_boot_parse_content(
	const char *fname,
	const char *content,
	size_t len,
	sdboot_boot_menu *menu,
	sdboot_boot_loader *loader
) {
	if (!fname || !content) return EFI_INVALID_PARAMETER;
	if (len == 0) return EFI_LOAD_ERROR;
	if ((!menu && !loader) || (menu && loader)) return EFI_INVALID_PARAMETER;
	if (loader)
		return sdboot_boot_parse_content_into(
			fname, content, len, NULL, loader
		);
	sdboot_boot_menu staged = *menu;
	staged.default_entry = NULL;
	if (menu->default_entry) {
		staged.default_entry = strdup(menu->default_entry);
		if (!staged.default_entry) return EFI_OUT_OF_RESOURCES;
	}
	EFI_STATUS status = sdboot_boot_parse_content_into(
		fname, content, len, &staged, NULL
	);
	if (EFI_ERROR(status)) {
		free(staged.default_entry);
		return status;
	}
	free(menu->default_entry);
	*menu = staged;
	return EFI_SUCCESS;
}
