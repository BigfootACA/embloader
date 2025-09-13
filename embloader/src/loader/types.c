#include "loader.h"

struct loader_func embloader_loader_funcs[] = {
	{ "efisetup",  LOADER_EFISETUP,  embloader_loader_boot_efisetup },
	{ "reboot",    LOADER_REBOOT,    embloader_loader_boot_reboot },
	{ "efi",       LOADER_EFI,       embloader_loader_boot_efi },
	{ "linux-efi", LOADER_LINUX_EFI, embloader_loader_boot_linux_efi },
	{ "linux",     LOADER_LINUX,     embloader_loader_boot_linux },
	{ "sdboot",      LOADER_SDBOOT,      embloader_loader_boot_sdboot },
	{ NULL, LOADER_NONE, NULL }
};
