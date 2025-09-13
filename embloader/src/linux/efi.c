#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/ReportStatusCodeLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "linuxboot.h"

static const union memmap_dp {
	struct {
		MEMMAP_DEVICE_PATH memmap;
		EFI_DEVICE_PATH_PROTOCOL end;
		char p0[
			32-
			sizeof(EFI_DEVICE_PATH_PROTOCOL)-
			sizeof(MEMMAP_DEVICE_PATH)
		];
	};
	EFI_DEVICE_PATH_PROTOCOL dp;
} __attribute__((packed)) memmap_dp_def = {
	.memmap = {
		.Header = {
			.Type = HARDWARE_DEVICE_PATH,
			.SubType = HW_MEMMAP_DP,
			.Length = {(UINT8)(sizeof(memmap_dp_def.memmap)), 0},
		},
	},
	.end = {
		.Type = END_DEVICE_PATH_TYPE,
		.SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE,
		.Length = { (UINT8)(sizeof(memmap_dp_def.end)), 0},
	}
};

/**
 * @brief Boot Linux kernel using EFI stub
 *
 * This function boots a Linux kernel image using the EFI stub loader method.
 * It creates a memory-mapped device path and starts the EFI image.
 *
 * @param img Pointer to the kernel image in memory
 * @param len Size of the kernel image in bytes
 * @param cmdline Kernel command line arguments (can be NULL)
 * @return EFI_STATUS no returns on successful boot, or appropriate error code on failure
 */
EFI_STATUS linux_boot_efi(void *img, size_t len, const char *cmdline) {
	union memmap_dp dp = memmap_dp_def;
	if (!img || len == 0) return EFI_INVALID_PARAMETER;
	dp.memmap.StartingAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)img;
	dp.memmap.EndingAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)img + len;
	return embloader_start_efi(&dp.dp, img, len, cmdline);
}
