#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Guid/GlobalVariable.h>
#include "../loader.h"
#include "log.h"

/**
 * @brief Check if EFI setup (firmware setup) is supported.
 * This function checks the EFI OsIndicationsSupported variable to determine
 * if the firmware supports booting to firmware setup interface.
 *
 * @return true if EFI setup is supported, false otherwise
 */
bool embloader_is_efisetup_supported() {
	UINT64 osind = 0;
	UINTN size = sizeof(osind);
	EFI_STATUS status;
	status = gRT->GetVariable(
		EFI_OS_INDICATIONS_SUPPORT_VARIABLE_NAME,
		&gEfiGlobalVariableGuid,
		NULL, &size, &osind
	);
	if (EFI_ERROR(status)) return false;
	return (osind & EFI_OS_INDICATIONS_BOOT_TO_FW_UI) != 0;
}

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
	UINT64 osind = 0;
	UINTN size = sizeof(osind);
	EFI_STATUS status;
	if (!embloader_is_efisetup_supported()) {
		log_error("EFI setup not supported");
		return EFI_UNSUPPORTED;
	}
	gRT->GetVariable(
		EFI_OS_INDICATIONS_VARIABLE_NAME,
		&gEfiGlobalVariableGuid,
		NULL, &size, &osind
	);
	osind |= EFI_OS_INDICATIONS_BOOT_TO_FW_UI;
	status = gRT->SetVariable(
		EFI_OS_INDICATIONS_VARIABLE_NAME,
		&gEfiGlobalVariableGuid,
		EFI_VARIABLE_NON_VOLATILE |
		EFI_VARIABLE_BOOTSERVICE_ACCESS |
		EFI_VARIABLE_RUNTIME_ACCESS,
		sizeof(osind), &osind
	);
	if (EFI_ERROR(status)) return status;
	log_info("Rebooting to EFI setup ...");
	gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
	gBS->Stall(500000);
	log_warning("ResetSystem returned unexpectedly");
	return EFI_NOT_FOUND;
}
