#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Guid/GlobalVariable.h>
#include "../loader.h"
#include "log.h"

/**
 * @brief Check if a loader is a reboot.
 *
 * @param loader the loader to check (must not be NULL)
 * @return true if the loader, false otherwise
 */
bool embloader_loader_is_reboot(embloader_loader *loader) {
	if (!loader || !loader->node) return false;
	char *type = confignode_path_get_string(loader->node, "type", NULL, NULL);
	if (!type) return false;
	bool ret = false;
	if (strcasecmp(type, "reboot") == 0) ret = true;
	else if (strcasecmp(type, "reboot-cold") == 0) ret = true;
	else if (strcasecmp(type, "reboot-warm") == 0) ret = true;
	else if (strcasecmp(type, "reboot-with") == 0) ret = true;
	free(type);
	return ret;
}

/**
 * @brief Check if a loader is a shutdown/poweroff.
 *
 * @param loader the loader to check (must not be NULL)
 * @return true if the loader, false otherwise
 */
bool embloader_loader_is_shutdown(embloader_loader *loader) {
	if (!loader || !loader->node) return false;
	char *type = confignode_path_get_string(loader->node, "type", NULL, NULL);
	if (!type) return false;
	bool ret = false;
	if (strcasecmp(type, "shutdown") == 0) ret = true;
	else if (strcasecmp(type, "poweroff") == 0) ret = true;
	free(type);
	return ret;
}

/**
 * @brief Perform system reboot or shutdown operation.
 * This function handles various system reset operations including cold/warm reboot,
 * shutdown, and platform-specific resets based on the configured mode.
 *
 * @param loader pointer to the loader configuration containing reboot mode and data
 * @return EFI_SUCCESS (though function typically doesn't return),
 *         EFI_INVALID_PARAMETER for invalid parameters or unknown mode
 */
EFI_STATUS embloader_loader_boot_reboot(embloader_loader *loader) {
	if (!loader || !loader->node) return EFI_INVALID_PARAMETER;
	char *mode = confignode_path_get_string(loader->node, "mode", "reboot", NULL);
	if (!mode) return EFI_INVALID_PARAMETER;
	char *data = confignode_path_get_string(loader->node, "data", NULL, NULL);
	EFI_RESET_TYPE type = EfiResetCold;
	if (strcasecmp(mode, "reboot") == 0) {
		log_info("Rebooting...");
		type = EfiResetCold;
	} else if (strcasecmp(mode, "reboot-cold") == 0) {
		log_info("Cold rebooting...");
		type = EfiResetCold;
	} else if (strcasecmp(mode, "reboot-warm") == 0) {
		log_info("Warm rebooting...");
		type = EfiResetWarm;
	} else if (strcasecmp(mode, "shutdown") == 0) {
		log_info("Shutting down...");
		type = EfiResetShutdown;
	} else if (strcasecmp(mode, "poweroff") == 0) {
		log_info("Shutting down...");
		type = EfiResetShutdown;
	} else if (strcasecmp(mode, "reboot-with") == 0) {
		log_info("Rebooting with '%s'...", data);
		type = EfiResetPlatformSpecific;
	} else {
		log_error("unknown reboot mode: %s", mode);
		free(mode);
		if (data) free(data);
		return EFI_INVALID_PARAMETER;
	}
	gRT->ResetSystem(type, EFI_SUCCESS, data ? (UINTN)strlen(data) : 0, data);
	gBS->Stall(500000);
	log_warning("ResetSystem returned unexpectedly");
	if (data) free(data);
	free(mode);
	return EFI_NOT_FOUND;
}
