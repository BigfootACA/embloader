#include "embloader.h"
#include "file-utils.h"
#include "efi-utils.h"
#include "log.h"
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePathToText.h>
#include <stdbool.h>
#include <string.h>

static bool embloader_check_fs(embloader_dir *dir) {
	EFI_STATUS status;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
	if (!dir->handle) return false;
	dir->root = NULL;
	dir->dir = NULL;
	status = gBS->HandleProtocol(
		dir->handle,
		&gEfiSimpleFileSystemProtocolGuid,
		(VOID**)&fs
	);
	if (EFI_ERROR(status) || !fs) return false;
	status = fs->OpenVolume(fs, &dir->root);
	if (EFI_ERROR(status) || !dir->root) return false;
	status = efi_open(
		dir->root, &dir->dir,
		"embloader",
		EFI_FILE_MODE_READ,
		EFI_FILE_DIRECTORY
	);
	if (EFI_ERROR(status) || !dir->dir) {
		if (status != EFI_NOT_FOUND)
			log_debug("open embloader folder failed: %s", efi_status_to_string(status));
		dir->root->Close(dir->root);
		dir->root = NULL;
		return false;
	}
	dir->dp = efi_device_path_from_handle(dir->handle);
	char *dp = efi_device_path_to_text(dir->dp);
	if (dp) {
		log_info("Found embloader folder in %s", dp);
		free(dp);
	} else log_info("Found embloader folder (handle: %p)", dir->handle);
	return true;
}

static bool find_by_image_handler(embloader_dir *dir) {
	EFI_LOADED_IMAGE_PROTOCOL *li;
	li = efi_get_loaded_image();
	if (!li) return false;
	dir->handle = li->DeviceHandle;
	return embloader_check_fs(dir);
}

static bool find_by_scan(embloader_dir *dir) {
	EFI_STATUS status;
	EFI_HANDLE *handles = NULL;
	UINTN handle_count = 0;
	status = gBS->LocateHandleBuffer(
		ByProtocol,
		&gEfiSimpleFileSystemProtocolGuid,
		NULL, &handle_count, &handles
	);
	if (EFI_ERROR(status) || handle_count == 0) return NULL;
	for (UINTN i = 0; i < handle_count; i++) {
		dir->handle = handles[i];
		if (embloader_check_fs(dir)) break;
	}
	if (handles) FreePool(handles);
	return dir->dir != NULL;
}

void find_embloader_folder(embloader_dir *dir) {
	memset(dir, 0, sizeof(embloader_dir));
	if (find_by_image_handler(dir)) return;
	if (find_by_scan(dir)) return;
	memset(dir, 0, sizeof(embloader_dir));
	log_warning("embloader folder not found");
}
