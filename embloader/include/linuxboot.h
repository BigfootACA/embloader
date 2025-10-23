#ifndef LINUX_BOOT_H
#define LINUX_BOOT_H
#include "embloader.h"

typedef struct linux_data linux_data;
typedef struct linux_bootinfo linux_bootinfo;
typedef struct linux_overlay linux_overlay;

struct linux_overlay {
	char *path;
	confignode *params;
};

struct linux_bootinfo {
	EFI_FILE_PROTOCOL *root;
	char *kernel;
	list *initramfs;
	list *bootargs;
	char *bootargs_override;
	char *devicetree;
	list *dtoverlay;
};

struct linux_data {
	void *kernel;
	size_t kernel_size;
	void *initramfs;
	size_t initramfs_size;
	fdt fdt;
	char *bootargs;
};

extern linux_bootinfo* linux_bootinfo_parse(confignode *node);
extern void linux_bootinfo_clean(linux_bootinfo *info);
extern fdt linux_try_load_dtb(EFI_FILE_PROTOCOL *base, const char *dtb, bool grow);
extern bool linux_load_default_dtb();
extern bool linux_apply_dtbo(const char *dtbo_name, fdt base, fdt dtbo);
extern bool linux_load_dtbo(EFI_FILE_PROTOCOL *base, fdt fdt, confignode *node, list *dtbo_dir);
extern bool linux_load_dtbos(EFI_FILE_PROTOCOL *base, fdt fdt, list *alt_dir);
extern bool linux_dtbo_write_overrides(fdt fdt, confignode *overrides);
extern char* linux_prepare_bootargs(list *def_bootargs);
extern char* linux_bootinfo_prepare_bootargs(linux_bootinfo *info);
extern EFI_STATUS linux_load_kernel(linux_data *data, linux_bootinfo *info);
extern EFI_STATUS linux_load_initramfs(linux_data *data, linux_bootinfo *info);
extern EFI_STATUS linux_load_bootargs(linux_data *data, linux_bootinfo *info);
extern EFI_STATUS linux_load_devicetree(linux_data *data, linux_bootinfo *info);
extern EFI_STATUS linux_load_dtoverlay(linux_data *data, linux_bootinfo *info);
extern EFI_STATUS linux_initramfs_register(const void *ptr, size_t size, EFI_HANDLE *hand);
extern EFI_STATUS linux_initramfs_unregister(EFI_HANDLE hand);
extern EFI_STATUS linux_boot_efi(void *img, size_t len, const char *cmdline);
extern EFI_STATUS linux_boot_use_efi(linux_data *data);
extern linux_data* linux_data_load(linux_bootinfo *info);
extern void linux_data_clean(linux_data *data);
extern EFI_STATUS linux_install_fdt(fdt fdt);
#endif
