#ifndef BOOTMENU_H
#define BOOTMENU_H
#include "embloader.h"
#define EMBLOADER_FLAG_AUTOBOOT     (1 << 0)
#define EMBLOADER_FLAG_USERSELECT   (1 << 1)
#define EMBLOADER_FLAG_EDITED       (1 << 2)
typedef enum embloader_arch embloader_arch;
typedef enum embloader_loader_type embloader_loader_type;
typedef struct embloader_loader embloader_loader;
typedef struct embloader_menu embloader_menu;
enum embloader_loader_type {
	LOADER_NONE,
	LOADER_EFISETUP,
	LOADER_REBOOT,
	LOADER_EFI,
	LOADER_LINUX_EFI,
	LOADER_LINUX,
	LOADER_SDBOOT,
};
enum embloader_arch {
	ARCH_UNKNOWN = 0,
	ARCH_ARM,
	ARCH_ARM64,
	ARCH_X86,
	ARCH_X86_64,
	ARCH_RISCV64,
	ARCH_LOONGARCH64,
};
struct embloader_menu {
	char *title;
	int timeout;
	bool save_default;
	char *default_entry;
	list *loaders;
};
struct embloader_loader {
	char *name;
	char *title;
	embloader_loader_type type;
	bool complete;
	int64_t priority;
	confignode *node;
	void *extra;
};
extern void embloader_load_loader(confignode *node);
extern EFI_STATUS embloader_loader_boot(embloader_loader *loader);
extern embloader_loader *embloader_find_loader(const char *name);
extern void embloader_load_menu();
extern void embloader_sort_menu_loaders();
extern embloader_loader_type embloader_menu_get_loader_type(const char *type_str);
extern bool embloader_menu_is_complete();
extern EFI_STATUS embloader_menu_start(embloader_loader **selected, uint64_t *flags);
extern EFI_STATUS embloader_show_menu();
extern bool embloader_loader_is_reboot(embloader_loader *loader);
extern bool embloader_loader_is_shutdown(embloader_loader *loader);
#endif
