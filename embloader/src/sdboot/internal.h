#ifndef SDBOOT_INTERNAL_H
#define SDBOOT_INTERNAL_H
#include "sdboot.h"

extern bool sdboot_parse_bool(
	const char *name,
	const char *str,
	size_t len,
	bool *out
);
extern bool sdboot_parse_int(
	const char *name,
	const char *str,
	size_t len,
	int *out
);
extern bool sdboot_parse_console_mode(
	const char *str,
	size_t len,
	sdboot_console_mode *out
);
extern bool sdboot_parse_secureboot_enroll(
	const char *str,
	size_t len,
	sdboot_secureboot_enroll *out
);
extern bool sdboot_parse_arch(
	const char *str,
	size_t len,
	embloader_arch *out
);
extern bool sdboot_parse_string(const char *str, size_t len, char **out);
extern bool sdboot_parse_string_list(const char *str, size_t len, list **out);
extern bool sdboot_parse_timeout(
	const char *str,
	size_t len,
	int *out
);
extern bool sdboot_boot_parse_menu_one(
	sdboot_boot_menu *menu,
	const char *key, size_t key_len,
	const char *value, size_t value_len
);
extern bool sdboot_boot_parse_loader_one(
	sdboot_boot_loader *loader,
	const char *key, size_t key_len,
	const char *value, size_t value_len
);
extern bool sdboot_boot_parse_line(
	const char *line,
	size_t len,
	sdboot_boot_menu *menu,
	sdboot_boot_loader *loader
);
extern bool sdboot_boot_parse_content(
	const char *fname,
	const char *content,
	size_t len,
	sdboot_boot_menu *menu,
	sdboot_boot_loader *loader
);
extern embloader_arch sdboot_get_current_arch();
extern EFI_STATUS sdboot_boot_load_file(
	sdboot_menu *menu,
	embloader_dir *dir,
	EFI_FILE_PROTOCOL *base,
	const char *fname,
	bool is_entry
);
extern EFI_STATUS sdboot_boot_load_entries_folder(
	sdboot_menu *menu,
	embloader_dir *dir,
	const char *folder
);
extern EFI_STATUS sdboot_boot_load_root(sdboot_menu *menu, embloader_dir *dir);
#endif
