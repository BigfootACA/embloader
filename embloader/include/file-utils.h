#ifndef FILE_UTILS_H
#define FILE_UTILS_H
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>
#include <Guid/FileSystemInfo.h>
#include <stdbool.h>
#include <stddef.h>

extern EFI_STATUS efi_file_get_info_by(EFI_FILE_PROTOCOL* file, EFI_GUID *guid, VOID** info, UINTN *info_size);
extern EFI_STATUS efi_file_get_info(EFI_FILE_PROTOCOL* file, EFI_FILE_INFO** info);
extern EFI_STATUS efi_get_fs_info(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs, EFI_FILE_SYSTEM_INFO** info);
extern EFI_STATUS efi_file_get_size(EFI_FILE_PROTOCOL* file, size_t* size);
extern EFI_STATUS efi_file_set_size(EFI_FILE_PROTOCOL* file, size_t size);
extern EFI_STATUS efi_file_chunked_read(EFI_FILE_PROTOCOL* file, size_t offset, void* buffer, size_t size);
extern EFI_STATUS efi_file_read_all(EFI_FILE_PROTOCOL* file, void** out, size_t *flen);
extern EFI_STATUS efi_file_read_pages(EFI_FILE_PROTOCOL* file, void** out, size_t *flen);
extern bool efi_file_write_all(EFI_FILE_PROTOCOL* file, const void* data, size_t len);
extern bool efi_folder_exists(EFI_FILE_PROTOCOL* root, const char* path);
extern bool efi_file_exists(EFI_FILE_PROTOCOL* root, const char* path);
extern EFI_STATUS efi_open(
	EFI_FILE_PROTOCOL* root,
	EFI_FILE_PROTOCOL** file,
	const char* path,
	UINT64 mode,
	UINT64 attr
);
extern EFI_STATUS efi_file_open_read_all(
	EFI_FILE_PROTOCOL* file,
	const char* path,
	void** out,
	size_t *flen
);
extern EFI_STATUS efi_file_open_read_pages(
	EFI_FILE_PROTOCOL* file,
	const char* path,
	void** out,
	size_t *flen
);

#endif
