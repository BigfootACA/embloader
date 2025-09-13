#ifndef EMBLOADER_LOADER_H
#define EMBLOADER_LOADER_H
#include "bootmenu.h"
struct loader_func {
	const char *name;
	embloader_loader_type type;
	EFI_STATUS (*func)(embloader_loader *loader);
};
extern struct loader_func embloader_loader_funcs[];
extern EFI_STATUS embloader_loader_boot_efisetup(embloader_loader *loader);
extern EFI_STATUS embloader_loader_boot_reboot(embloader_loader *loader);
extern EFI_STATUS embloader_loader_boot_efi(embloader_loader *loader);
extern EFI_STATUS embloader_loader_boot_linux_efi(embloader_loader *loader);
extern EFI_STATUS embloader_loader_boot_linux(embloader_loader *loader);
extern EFI_STATUS embloader_loader_boot_sdboot(embloader_loader *loader);
#endif
