#include "../loader.h"
#include "sdboot.h"
#include "log.h"

/**
 * @brief Boot using systemd-boot compatible loader.
 * This function boots a loader entry using the SDBOOT boot system, which
 * provides systemd-boot compatibility for loading EFI applications and kernels.
 *
 * @param loader pointer to the loader configuration with SDBOOT boot loader data
 * @return EFI_SUCCESS on successful boot,
 *         EFI_NOT_READY if SDBOOT not initialized,
 *         EFI_INVALID_PARAMETER for invalid parameters,
 *         or error from SDBOOT boot
 */
EFI_STATUS embloader_loader_boot_sdboot(embloader_loader *loader) {
	if (!g_embloader.sdboot) return EFI_NOT_READY;
	if (!loader || loader->type != LOADER_SDBOOT || !loader->extra)
		return EFI_INVALID_PARAMETER;
	sdboot_boot_loader *l = loader->extra;
	log_info(
		"booting sdboot loader %s ...",
		loader->title ?: (loader->name ?: "(unnamed)")
	);
	return sdboot_boot_loader_invoke(g_embloader.sdboot, l);
}
