#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Guid/GlobalVariable.h>
#include "../loader.h"
#include "efi-utils.h"

/**
 * @brief Boot to EFI firmware setup interface.
 * This function sets the OsIndications variable to request boot to firmware
 * setup on next reboot, then performs a cold reset to enter setup mode.
 *
 * @param loader pointer to the loader configuration (unused but required for interface)
 * @return EFI_UNSUPPORTED if EFI setup not supported, error from SetVariable,
 *         or EFI_NOT_FOUND if reset unexpectedly returned
 */
EFI_STATUS embloader_loader_boot_efisetup(embloader_loader *loader) {
	return efi_reboot_to_setup();
}
