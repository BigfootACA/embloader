#include "linuxboot.h"
#include "log.h"
#include "efi-utils.h"

/**
 * @brief Boot Linux using EFI stub loader
 *
 * This function boots a Linux kernel using the EFI stub loader method.
 * It handles device tree installation, initramfs registration, and kernel execution.
 *
 * @param data Pointer to linux_data structure containing kernel, initramfs, and boot parameters
 * @return EFI_STATUS Returns EFI_SUCCESS on successful boot, or appropriate error code on failure
 */
EFI_STATUS linux_boot_use_efi(linux_data *data) {
	EFI_STATUS status;
	EFI_HANDLE initrd_hand = NULL;
	if (!data) return EFI_INVALID_PARAMETER;
	if (data->fdt) linux_install_fdt(data->fdt);
	else if(!g_embloader.fdt) embloader_prepare_boot();
	if (data->initramfs && data->initramfs_size > 0) {
		status = linux_initramfs_register(
			data->initramfs,
			data->initramfs_size,
			&initrd_hand
		);
		if (EFI_ERROR(status)) {
			log_error(
				"register initramfs failed: %s",
				efi_status_to_string(status)
			);
			return status;
		}
	}
	status = linux_boot_efi(data->kernel, data->kernel_size, data->bootargs);
	if (initrd_hand) linux_initramfs_unregister(initrd_hand);
	if (EFI_ERROR(status)) {
		log_error("boot kernel failed: %s", efi_status_to_string(status));
		return status;
	}
	return EFI_SUCCESS;
}
