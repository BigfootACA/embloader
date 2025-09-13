#ifndef EMBLOADER_H
#define EMBLOADER_H
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/DevicePath.h>
#include "configfile.h"
#include "list.h"
typedef void* fdt;
typedef struct sdboot_menu sdboot_menu;
typedef struct embloader_menu embloader_menu;
typedef struct embloader_dir embloader_dir;
typedef struct embloader embloader;
struct embloader_dir {
	EFI_HANDLE handle;
	EFI_FILE_PROTOCOL *root;
	EFI_FILE_PROTOCOL *dir;
	EFI_DEVICE_PATH_PROTOCOL *dp;
};
struct embloader {
	embloader_dir dir;
	confignode *config;
	confignode *sysinfo;
	char *ktype;
	char *device_name;
	list *profiles;
	fdt fdt;
	embloader_menu *menu;
	sdboot_menu *sdboot;
};
extern embloader g_embloader;
extern void find_embloader_folder(embloader_dir *dir);
extern bool embloader_init();
extern bool embloader_load_configs();
extern bool embloader_load_config_one(const char *name);
extern bool embloader_load_smbios();
extern bool embloader_try_match(confignode *node);
extern bool embloader_try_matches(confignode *node);
extern bool embloader_choose_device();
extern bool embloader_is_efisetup_supported();
extern void embloader_load_ktype();
extern list* embloader_resolve_path(const char *path);
extern list* embloader_dt_get_dtb_id();
extern list* embloader_dt_get_dtbo_dir();
extern list* embloader_dt_get_default_dtb();
extern EFI_STATUS embloader_load_dtbo_config(confignode *node, const char *buffer);
extern EFI_STATUS embloader_load_dtbo_config_path(confignode *node, EFI_FILE_PROTOCOL *base, const char *path);
extern EFI_STATUS embloader_load_cmdline_config(confignode *node, const char *buffer);
extern EFI_STATUS embloader_load_cmdline_config_path(confignode *node, EFI_FILE_PROTOCOL *base, const char *path);
extern EFI_STATUS embloader_install_fdt(void *fdt);
extern EFI_STATUS embloader_prepare_boot();
extern EFI_STATUS embloader_fetch_fdt();
extern EFI_STATUS embloader_start_efi(
	EFI_DEVICE_PATH_PROTOCOL *dp,
	void *img, size_t len,
	const char *cmdline
);
#endif
