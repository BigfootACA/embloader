#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/Fdt.h>
#include <libfdt.h>
#include "log.h"
#include "embloader.h"
#include "linuxboot.h"
#include "efi-utils.h"
#include "efi-dt-fixup.h"

static EFI_STATUS efi_dtfixup(void *tree) {
	int ret;
	UINTN size = 0;
	EFI_STATUS status;
	void *buff = NULL;
	EFI_DT_FIXUP_PROTOCOL *dtfixup = NULL;
	status = gBS->LocateProtocol(&gEfiDtFixupProtocolGuid, NULL, (VOID**)&dtfixup);
	if (EFI_ERROR(status) || !dtfixup) {
		if (status != EFI_NOT_FOUND) log_warning(
			"failed to locate dtfixup protocol: %s",
			efi_status_to_string(status)
		);
		return status;
	}
	if (!(buff = AllocateZeroPool(SIZE_2MB))) {
		log_error("failed to allocate memory for dtfixup");
		return EFI_OUT_OF_RESOURCES;
	}
	if ((ret = fdt_open_into(tree, buff, SIZE_2MB)) != 0) {
		log_error("failed to open fdt: %s", fdt_strerror(ret));
		status = EFI_DEVICE_ERROR;
		goto done;
	}
	fdt_pack(buff);
	log_debug("applying efi dtfixup protocol");
	status = dtfixup->Fixup(
		dtfixup, buff, &size,
		EFI_DT_APPLY_FIXUPS | EFI_DT_RESERVE_MEMORY
	);
	if (status != EFI_BUFFER_TOO_SMALL) {
		log_warning(
			"efi dtfixup does not return buffer size: %s",
			efi_status_to_string(status)
		);
		goto done;
	}
	if (size > SIZE_2MB) {
		log_warning("efi dtfixup required too large buffer");
		status = EFI_BAD_BUFFER_SIZE;
		goto done;
	}
	status = dtfixup->Fixup(
		dtfixup, buff, &size,
		EFI_DT_APPLY_FIXUPS | EFI_DT_RESERVE_MEMORY
	);
	if (EFI_ERROR(status)) {
		log_warning(
			"efi dtfixup failed: %s",
			efi_status_to_string(status)
		);
		goto done;
	}
	if ((ret = fdt_check_header(tree)) != 0) {
		log_error("efi dtfixup produced invalid fdt: %s", fdt_strerror(ret));
		status = EFI_DEVICE_ERROR;
		goto done;
	}
	if ((ret = fdt_open_into(buff, tree, SIZE_2MB)) != 0) {
		log_error("efi dtfixup produced invalid fdt: %s", fdt_strerror(ret));
		status = EFI_DEVICE_ERROR;
		goto done;
	}
	log_info("efi dtfixup applied successfully");
done:
	if (buff) FreePool(buff);
	return status;
}

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
	if (!fdt || fdt_check_header(fdt) < 0) return EFI_INVALID_PARAMETER;
	void *copied = NULL;
	int size = fdt_totalsize(fdt);
	if (size <= 0 || size > SIZE_2MB) return EFI_INVALID_PARAMETER;
	if (!(copied = AllocateAlignedRuntimePages(
		EFI_SIZE_TO_PAGES(SIZE_2MB), SIZE_2MB
	))) {
		log_error("failed to allocate memory for fdt");
		return EFI_OUT_OF_RESOURCES;
	}
	memset(copied, 0, SIZE_2MB);
	memcpy(copied, fdt, size);
	if (confignode_path_get_bool(g_embloader.config, "efi.dtfixup", true, NULL))
		efi_dtfixup(copied);
	fdt_finish(copied);
	status = gBS->InstallConfigurationTable(&gFdtTableGuid, copied);
	if (EFI_ERROR(status)) {
		log_error(
			"failed to install fdt to system table: %s",
			efi_status_to_string(status)
		);
		gBS->FreePages((EFI_PHYSICAL_ADDRESS)(UINTN)copied, EFI_SIZE_TO_PAGES(size));
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
