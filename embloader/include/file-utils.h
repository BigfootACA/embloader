#ifndef FILE_UTILS_H
#define FILE_UTILS_H
#include <Protocol/SimpleFileSystem.h>
#include <stdbool.h>
#include <stddef.h>

extern EFI_STATUS efi_file_get_size(EFI_FILE_PROTOCOL* file, size_t* size);
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
