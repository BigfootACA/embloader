#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <Guid/FileInfo.h>
#include <stdlib.h>
#include <string.h>
#include "file-utils.h"
#include "efi-utils.h"
#include "encode.h"
#include "log.h"

/**
 * Read entire file content into a null-terminated string.
 *
 * @param file pointer to opened EFI_FILE_PROTOCOL
 */
EFI_STATUS efi_file_read_all(EFI_FILE_PROTOCOL* file, void** out, size_t *flen) {
	EFI_STATUS status;
	EFI_FILE_INFO* file_info = NULL;
	UINTN info_size = 0;
	UINTN file_size = 0;
	char* buffer = NULL;
	UINTN read_size;
	if (!file || !out) return EFI_INVALID_PARAMETER;
	if (flen) *flen = 0;
	*out = NULL;
	status = file->GetInfo(file, &gEfiFileInfoGuid, &info_size, NULL);
	if (status != EFI_BUFFER_TOO_SMALL)
		return EFI_ERROR(status) ? status : EFI_INVALID_PARAMETER;
	file_info = AllocatePool(info_size);
	if (!file_info) return EFI_OUT_OF_RESOURCES;
	status = file->GetInfo(file, &gEfiFileInfoGuid, &info_size, file_info);
	if (EFI_ERROR(status)) {
		FreePool(file_info);
		return status;
	}
	file_size = (UINTN) file_info->FileSize;
	FreePool(file_info);
	if (file_size == 0) {
		buffer = malloc(1);
		if (buffer) buffer[0] = 0;
		*out = buffer;
		if (flen) *flen = 0;
		return EFI_SUCCESS;
	}
	buffer = malloc(file_size + 1);
	if (!buffer) return EFI_OUT_OF_RESOURCES;
	status = file->SetPosition(file, 0);
	if (EFI_ERROR(status)) {
		free(buffer);
		return status;
	}
	read_size = file_size;
	status = file->Read(file, &read_size, buffer);
	if (EFI_ERROR(status) || read_size != file_size) {
		free(buffer);
		return EFI_ERROR(status) ? status : EFI_DEVICE_ERROR;
	}
	buffer[file_size] = 0;
	*out = buffer;
	if (flen) *flen = file_size;
	return EFI_SUCCESS;
}

/**
 * Write data to file.
 *
 * @param file pointer to opened EFI_FILE_PROTOCOL (must be opened for writing)
 * @param data pointer to data to write
 * @param len length of data to write
 * @return true on success, false on error
 */
bool efi_file_write_all(EFI_FILE_PROTOCOL* file, const void* data, size_t len) {
	if (!file || !data) return false;
	EFI_STATUS status;
	UINTN write_size = len;
	status = file->SetPosition(file, 0);
	if (EFI_ERROR(status)) return false;
	status = file->Write(file, &write_size, (VOID*) data);
	if (EFI_ERROR(status) || write_size != len) return false;
	status = file->Flush(file);
	if (EFI_ERROR(status)) return false;
	return true;
}

bool efi_folder_exists(EFI_FILE_PROTOCOL* root, const char* path) {
	EFI_STATUS status;
	EFI_FILE_PROTOCOL* dir = NULL;
	if (!path) return false;
	status = efi_open(
		root, &dir, path,
		EFI_FILE_MODE_READ,
		EFI_FILE_DIRECTORY
	);
	if (EFI_ERROR(status) || !dir) {
		if (status != EFI_NOT_FOUND) log_warning(
			"open folder %s failed: %s",
			path, efi_status_to_string(status)
		);
		return false;
	}
	dir->Close(dir);
	return true;
}

bool efi_file_exists(EFI_FILE_PROTOCOL* root, const char* path) {
	EFI_STATUS status;
	EFI_FILE_PROTOCOL* dir = NULL;
	if (!path) return false;
	status = efi_open(
		root, &dir, path,
		EFI_FILE_MODE_READ, 0
	);
	if (EFI_ERROR(status) || !dir) {
		if (status != EFI_NOT_FOUND) log_warning(
			"open file %s failed: %s",
			path, efi_status_to_string(status)
		);
		return false;
	}
	dir->Close(dir);
	return true;
}

EFI_STATUS efi_open(
	EFI_FILE_PROTOCOL* root,
	EFI_FILE_PROTOCOL** file,
	const char* path,
	UINT64 mode,
	UINT64 attr
) {
	if (!root || !file || !path)
		return EFI_INVALID_PARAMETER;
	EFI_STATUS status;
	CHAR16* wpath = encode_utf8_to_utf16(path);
	if (!wpath) return EFI_OUT_OF_RESOURCES;
	for (UINTN i = 0; wpath[i]; i++)
		if (wpath[i] == L'/')
			wpath[i] = L'\\';
	status = root->Open(root, file, wpath, mode, attr);
	free(wpath);
	return status;
}

EFI_STATUS efi_file_open_read_all(
	EFI_FILE_PROTOCOL* file,
	const char* path,
	void** out,
	size_t *flen
) {
	EFI_STATUS status;
	EFI_FILE_PROTOCOL* f = NULL;
	if (!file || !path || !out)
		return EFI_INVALID_PARAMETER;
	status = efi_open(
		file, &f, path,
		EFI_FILE_MODE_READ, 0
	);
	if (EFI_ERROR(status) || !f) {
		log_warning(
			"open file %s failed: %s", path,
			efi_status_to_string(status)
		);
		return status;
	}
	status = efi_file_read_all(f, out, flen);
	f->Close(f);
	return status;
}
