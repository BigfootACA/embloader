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
 * @return true on successful parsing, false on error (logs warning for unknown keys)
 */
bool sdboot_boot_parse_menu_one(
	sdboot_boot_menu *menu,
	const char *key, size_t key_len,
	const char *value, size_t value_len
) {
	if (str_case_match(key, key_len, "default"))
		return sdboot_parse_string(value, value_len, &menu->default_entry);
	if (str_case_match(key, key_len, "timeout"))
		return sdboot_parse_int("timeout", value, value_len, &menu->timeout);
	if (str_case_match(key, key_len, "console-mode"))
		return sdboot_parse_console_mode(value, value_len, &menu->console_mode);
	if (str_case_match(key, key_len, "editor"))
		return sdboot_parse_bool("editor", value, value_len, &menu->editor);
	if (str_case_match(key, key_len, "auto-entries"))
		return sdboot_parse_bool("auto-entries", value, value_len, &menu->auto_entries);
	if (str_case_match(key, key_len, "auto-firmware"))
		return sdboot_parse_bool("auto-firmware", value, value_len, &menu->auto_firmware);
	if (str_case_match(key, key_len, "auto-reboot"))
		return sdboot_parse_bool("auto-reboot", value, value_len, &menu->auto_reboot);
	if (str_case_match(key, key_len, "auto-poweroff"))
		return sdboot_parse_bool("auto-poweroff", value, value_len, &menu->auto_poweroff);
	if (str_case_match(key, key_len, "beep"))
		return sdboot_parse_bool("beep", value, value_len, &menu->beep);
	if (str_case_match(key, key_len, "secureboot-enroll"))
		return sdboot_parse_secureboot_enroll(value, value_len, &menu->secureboot_enroll);
	if (str_case_match(key, key_len, "reboot-for-bitlocker"))
		return sdboot_parse_bool("reboot-for-bitlocker", value, value_len, &menu->reboot_for_bitlocker);
	log_warning("unknown boot menu option: %.*s", (int)key_len, key);
	return true;
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
 * @return true on successful parsing, false on error (logs warning for unknown keys)
 */
bool sdboot_boot_parse_loader_one(
	sdboot_boot_loader *loader,
	const char *key, size_t key_len,
	const char *value, size_t value_len
) {
	if (str_case_match(key, key_len, "title"))
		return sdboot_parse_string(value, value_len, &loader->title);
	if (str_case_match(key, key_len, "version"))
		return sdboot_parse_string(value, value_len, &loader->version);
	if (str_case_match(key, key_len, "machine-id"))
		return sdboot_parse_string(value, value_len, &loader->machine_id);
	if (str_case_match(key, key_len, "sort-key"))
		return sdboot_parse_string(value, value_len, &loader->sort_key);
	if (str_case_match(key, key_len, "linux"))
		return sdboot_parse_string(value, value_len, &loader->kernel);
	if (str_case_match(key, key_len, "initrd"))
		return sdboot_parse_string_list(value, value_len, &loader->initramfs);
	if (str_case_match(key, key_len, "efi"))
		return sdboot_parse_string(value, value_len, &loader->efi);
	if (str_case_match(key, key_len, "options"))
		return sdboot_parse_string_list(value, value_len, &loader->options);
	if (str_case_match(key, key_len, "devicetree"))
		return sdboot_parse_string(value, value_len, &loader->devicetree);
	if (str_case_match(key, key_len, "devicetree-overlay"))
		return sdboot_parse_string_list(value, value_len, &loader->dtoverlay);
	if (str_case_match(key, key_len, "architecture"))
		return sdboot_parse_arch(value, value_len, &loader->arch);
	log_warning("unknown boot loader entry option: %.*s", (int)key_len, key);
	return true;
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
 * @return true on successful parsing or comment/empty line, false on parse error
 */
bool sdboot_boot_parse_line(
	const char *line,
	size_t len,
	sdboot_boot_menu *menu,
	sdboot_boot_loader *loader
) {
	char *p;
	bool ret = false;
	if (!line) return false;
	if (len == 0) return true;
	if ((!menu && !loader) || (menu && loader)) return false;
	const char *orig_line = line;
	size_t orig_len = len;
	while (len > 0 && isspace(line[len - 1])) len--;
	while (len > 0 && isspace(line[0])) line++, len--;
	if (len == 0 || line[0] == '#') return true;
	const char *sep = line;
	for (; sep < line + len && !isspace(*sep); sep++);
	if (sep >= line + len) return false;
	const char *key = line, *value = sep;
	while (value < line + len && isspace(*value)) value++;
	if (value >= line + len) return false;
	size_t key_len = sep - line, value_len = line + len - value;
	if ((p = memchr(value, '#', value_len))) value_len = p - value;
	while (value_len > 0 && isspace(value[value_len - 1])) value_len--;
	if (key_len == 0 || value_len == 0) return false;
	if (menu) ret = sdboot_boot_parse_menu_one(menu, key, key_len, value, value_len);
	if (loader) ret = sdboot_boot_parse_loader_one(loader, key, key_len, value, value_len);
	if (!ret) log_warning("failed to apply config line: '%.*s'", (int)orig_len, orig_line);
	return ret;
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
 * @return true if at least one line was successfully parsed, false on complete failure
 */
bool sdboot_boot_parse_content(
	const char *fname,
	const char *content,
	size_t len,
	sdboot_boot_menu *menu,
	sdboot_boot_loader *loader
) {
	if (!fname || !content || len == 0) return false;
	if ((!menu && !loader) || (menu && loader)) return false;
	size_t off = 0;
	int line = 0;
	bool have_success = false;
	while (off < len) {
		line++;
		size_t llen = 0;
		const char *line_start = content + off;
		while (off + llen < len && content[off + llen] != '\n') llen++;
		if (!sdboot_boot_parse_line(line_start, llen, menu, loader)) {
			log_warning("failed to parse sdboot config %s at line %d", fname, line);
			log_debug(
				"line %d: \"%.*s\" (%llu chars)",
				line, (int)llen, line_start, (unsigned long long)llen
			);
		} else have_success = true;
		off += llen;
		if (off < len && content[off] == '\n') off++;
	}
	return have_success;
}
