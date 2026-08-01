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
	EFI_STATUS status, unregister_status = EFI_SUCCESS;
	EFI_HANDLE initrd_hand = NULL;
	if (!data) return EFI_INVALID_PARAMETER;
	if (data->fdt || g_embloader.fdt) {
		status = linux_install_fdt(data->fdt ?: g_embloader.fdt);
		if (EFI_ERROR(status)) {
			log_error("install device tree failed: %s", efi_status_to_string(status));
			return status;
		}
	} else {
		status = embloader_prepare_boot();
		if (EFI_ERROR(status)) log_warning(
			"prepare device tree failed: %s; continuing without one",
			efi_status_to_string(status)
		);
	}
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
	if (initrd_hand) {
		unregister_status = linux_initramfs_unregister(initrd_hand);
		if (EFI_ERROR(unregister_status)) {
			log_error(
				"unregister initramfs failed: %s",
				efi_status_to_string(unregister_status)
			);
			/* The installed protocol may still reference this buffer. */
			data->initramfs = NULL;
			data->initramfs_size = 0;
		}
	}
	if (EFI_ERROR(status)) {
		log_error("boot kernel failed: %s", efi_status_to_string(status));
		return status;
	}
	return unregister_status;
}
