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
 * @brief Get the size of an EFI file
 *
 * This function retrieves the file size by querying the file information.
 *
 * @param file Pointer to the opened EFI_FILE_PROTOCOL
 * @param size Pointer to store the file size
 * @return EFI_SUCCESS on success, error status on failure
 */
EFI_STATUS efi_file_get_size(EFI_FILE_PROTOCOL* file, size_t* size) {
	EFI_STATUS status;
	EFI_FILE_INFO* file_info = NULL;
	UINTN info_size = 0;
	if (!file || !size) return EFI_INVALID_PARAMETER;
	*size = 0;
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
	*size = (size_t) file_info->FileSize;
	FreePool(file_info);
	return EFI_SUCCESS;
}

/**
 * @brief Read file data in chunks from a specific offset
 *
 * This function reads file data in chunks to avoid memory limitations.
 * It preserves the original file position after reading.
 *
 * @param file Pointer to the opened EFI_FILE_PROTOCOL
 * @param offset Starting offset in the file
 * @param buffer Buffer to store the read data
 * @param size Number of bytes to read
 * @return EFI_SUCCESS on success, error status on failure
 */
EFI_STATUS efi_file_chunked_read(EFI_FILE_PROTOCOL* file, size_t offset, void* buffer, size_t size) {
	EFI_STATUS status;
	UINTN read_size;
	UINT64 read_pos = 0, old_pos = 0;
	if (!file || !buffer) return EFI_INVALID_PARAMETER;
	if (size == 0) return EFI_SUCCESS;
	status = file->GetPosition(file, &old_pos);
	if (EFI_ERROR(status)) old_pos = 0;
	status = file->SetPosition(file, offset);
	if (EFI_ERROR(status)) return status;
	while (read_pos < size) {
		read_size = MIN(size - read_pos, SIZE_1MB);
		status = file->Read(file, &read_size, (UINT8*)buffer + read_pos);
		if (EFI_ERROR(status)) break;
		if (read_size == 0) {
			status = EFI_END_OF_FILE;
			break;
		}
		read_pos += read_size;
	}
	file->SetPosition(file, old_pos);
	return status;
}

/**
 * @brief Read entire file content into a null-terminated string
 *
 * This function reads the complete file content and adds a null terminator.
 * The caller is responsible for freeing the allocated memory using free().
 *
 * @param file Pointer to opened EFI_FILE_PROTOCOL
 * @param out Pointer to store the allocated buffer containing file content
 * @param flen Optional pointer to store the file length (excluding null terminator)
 * @return EFI_SUCCESS on success, error status on failure
 */
EFI_STATUS efi_file_read_all(EFI_FILE_PROTOCOL* file, void** out, size_t *flen) {
	EFI_STATUS status;
	size_t file_size = 0;
	char* buffer = NULL;
	if (!file || !out) return EFI_INVALID_PARAMETER;
	if (flen) *flen = 0;
	*out = NULL;
	status = efi_file_get_size(file, &file_size);
	if (EFI_ERROR(status)) return status;
	if (file_size == 0) {
		buffer = malloc(1);
		if (buffer) buffer[0] = 0;
		*out = buffer;
		if (flen) *flen = 0;
		return EFI_SUCCESS;
	}
	buffer = malloc(file_size + 1);
	if (!buffer) return EFI_OUT_OF_RESOURCES;
	status = efi_file_chunked_read(file, 0, buffer, file_size);
	if (EFI_ERROR(status)) {
		free(buffer);
		return status;
	}
	buffer[file_size] = 0;
	*out = buffer;
	if (flen) *flen = file_size;
	return EFI_SUCCESS;
}

/**
 * @brief Read entire file content using EFI page allocation
 *
 * This function reads file content into EFI-allocated pages, suitable for
 * large files or when page-aligned memory is required. The caller must
 * free the memory using FreePages().
 *
 * @param file Pointer to opened EFI_FILE_PROTOCOL
 * @param out Pointer to store the allocated buffer containing file content
 * @param flen Optional pointer to store the file length (excluding null terminator)
 * @return EFI_SUCCESS on success, error status on failure
 */
EFI_STATUS efi_file_read_pages(EFI_FILE_PROTOCOL* file, void** out, size_t *flen) {
	UINTN pages;
	EFI_STATUS status;
	size_t file_size = 0;
	char* buffer = NULL;
	if (!file || !out) return EFI_INVALID_PARAMETER;
	if (flen) *flen = 0;
	*out = NULL;
	status = efi_file_get_size(file, &file_size);
	if (EFI_ERROR(status)) return status;
	if (file_size == 0) return EFI_END_OF_FILE;
	pages = EFI_SIZE_TO_PAGES(file_size);
	buffer = AllocatePages(pages);
	if (!buffer) return EFI_OUT_OF_RESOURCES;
	status = efi_file_chunked_read(file, 0, buffer, file_size);
	if (EFI_ERROR(status)) {
		FreePages(buffer, pages);
		return status;
	}
	*out = buffer;
	if (flen) *flen = file_size;
	return EFI_SUCCESS;
}

/**
 * @brief Write data to file
 *
 * This function writes data to a file starting from the beginning.
 * The file must be opened with write permissions.
 *
 * @param file Pointer to opened EFI_FILE_PROTOCOL (must be opened for writing)
 * @param data Pointer to data to write
 * @param len Length of data to write
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

/**
 * @brief Check if a folder exists
 *
 * This function checks whether a directory exists at the specified path.
 *
 * @param root Pointer to the root EFI_FILE_PROTOCOL
 * @param path Path to the folder to check
 * @return true if folder exists, false otherwise
 */
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

/**
 * @brief Check if a file exists
 *
 * This function checks whether a file exists at the specified path.
 *
 * @param root Pointer to the root EFI_FILE_PROTOCOL
 * @param path Path to the file to check
 * @return true if file exists, false otherwise
 */
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

/**
 * @brief Open a file or directory using UTF-8 path
 *
 * This function opens a file or directory, converting the UTF-8 path
 * to UTF-16 and replacing forward slashes with backslashes for EFI compatibility.
 *
 * @param root Pointer to the root EFI_FILE_PROTOCOL
 * @param file Pointer to store the opened file handle
 * @param path UTF-8 encoded file path
 * @param mode File open mode (EFI_FILE_MODE_READ, etc.)
 * @param attr File attributes
 * @return EFI_SUCCESS on success, error status on failure
 */
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

/**
 * @brief Open a file and read its entire content
 *
 * This is a convenience function that opens a file and reads its entire
 * content in one operation. The caller is responsible for freeing the
 * allocated memory using free().
 *
 * @param file Pointer to the root EFI_FILE_PROTOCOL
 * @param path Path to the file to open and read
 * @param out Pointer to store the allocated buffer containing file content
 * @param flen Optional pointer to store the file length
 * @return EFI_SUCCESS on success, error status on failure
 */
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

/**
 * @brief Open a file and read its entire content using EFI page allocation
 *
 * This is a convenience function that opens a file and reads its entire
 * content using EFI page allocation in one operation. This is suitable for
 * large files or when page-aligned memory is required. The caller is
 * responsible for freeing the allocated memory using FreePages().
 *
 * @param file Pointer to the root EFI_FILE_PROTOCOL
 * @param path Path to the file to open and read
 * @param out Pointer to store the allocated buffer containing file content
 * @param flen Optional pointer to store the file length (excluding null terminator)
 * @return EFI_SUCCESS on success, error status on failure
 */
EFI_STATUS efi_file_open_read_pages(
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
	status = efi_file_read_pages(f, out, flen);
	f->Close(f);
	return status;
}
