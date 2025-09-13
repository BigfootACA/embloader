#include <Library/UefiBootServicesTableLib.h>
#include <Guid/Fdt.h>
#include <libfdt.h>
#include "log.h"
#include "embloader.h"
#include "linuxboot.h"
#include "efi-utils.h"

/**
 * @brief Install device tree into EFI configuration table
 *
 * This function copies the device tree to EFI memory and installs it
 * in the EFI configuration table for kernel access.
 *
 * @param fdt Pointer to the device tree structure
 * @return EFI_STATUS Returns EFI_SUCCESS on success, or appropriate error code on failure
 */
EFI_STATUS linux_install_fdt(fdt fdt) {
	EFI_STATUS status;
	if (!fdt) return EFI_INVALID_PARAMETER;
	void *copied = NULL;
	int size = fdt_totalsize(fdt);
	if (size <= 0) return EFI_INVALID_PARAMETER;
	status = gBS->AllocatePages(
		AllocateAnyPages,
		EfiACPIReclaimMemory,
		EFI_SIZE_TO_PAGES(size),
		(EFI_PHYSICAL_ADDRESS*) &copied
	);
	if (EFI_ERROR(status) || !copied) {
		log_error(
			"failed to allocate memory for fdt: %s",
			efi_status_to_string(status)
		);
		return EFI_OUT_OF_RESOURCES;
	}
	memcpy(copied, fdt, size);
	fdt_finish(copied);
	status = gBS->InstallConfigurationTable(&gFdtTableGuid, fdt);
	if (EFI_ERROR(status)) {
		log_error(
			"failed to install fdt to system table: %s",
			efi_status_to_string(status)
		);
		gBS->FreePages((EFI_PHYSICAL_ADDRESS)copied, EFI_SIZE_TO_PAGES(size));
		return status;
	}
	return EFI_SUCCESS;
}

/**
 * @brief Fetch existing device tree from EFI system table
 *
 * This function searches the EFI system table for an existing device tree
 * and copies it into local memory for further processing.
 *
 * @return EFI_STATUS Returns EFI_SUCCESS if found and copied,
 *         EFI_NOT_FOUND if not found,
 *         or appropriate error code on failure
 */
EFI_STATUS embloader_fetch_fdt() {
	int r;
	fdt nfdt, ofdt;
	if (g_embloader.fdt) return EFI_SUCCESS;
	for (UINTN i = 0; i < gST->NumberOfTableEntries; i++) {
		if (memcmp(
			&gST->ConfigurationTable[i].VendorGuid,
			&gFdtTableGuid, sizeof(EFI_GUID)
		) != 0) continue;
		ofdt = (fdt)gST->ConfigurationTable[i].VendorTable;
		if (!ofdt) continue;
		if ((r = fdt_check_header(ofdt)) != 0) {
			log_warning(
				"found invalid fdt in system table: %s",
				fdt_strerror(r)
			);
			continue;
		}
		if (fdt_totalsize(ofdt) > SIZE_2MB) {
			log_warning("found too large fdt in system table");
			continue;
		}
		if (!(nfdt = malloc(SIZE_2MB))) {
			log_error("failed to allocate memory for fdt copy");
			return EFI_OUT_OF_RESOURCES;
		}
		memset(nfdt, 0, SIZE_2MB);
		if ((r = fdt_open_into(ofdt, nfdt, SIZE_2MB)) != 0) {
			log_warning(
				"copy fdt from system table failed: %s",
				fdt_strerror(r)
			);
			free(nfdt);
			nfdt = NULL;
			continue;
		}
		g_embloader.fdt = nfdt;
		log_info("found existing fdt in system table at %p", ofdt);
		return EFI_SUCCESS;
	}
	log_debug("no existing fdt found in system table");
	return EFI_NOT_FOUND;
}

/**
 * @brief Prepare system for Linux boot
 *
 * This function prepares the system for Linux boot by loading the default
 * device tree, applying overlays, and setting up the boot environment.
 *
 * @return EFI_STATUS Returns EFI_SUCCESS on success, or appropriate error code on failure
 */
EFI_STATUS embloader_prepare_boot() {
	if (!linux_load_default_dtb())
		log_warning("load device tree failed");
	if (!g_embloader.fdt) embloader_fetch_fdt();
	if (!g_embloader.fdt) {
		log_warning("no device tree loaded");
		return EFI_LOAD_ERROR;
	}
	if (!linux_load_dtbos(g_embloader.dir.root, g_embloader.fdt, NULL))
		log_warning("no any device tree overlays applied");
	return linux_install_fdt(g_embloader.fdt);
}
