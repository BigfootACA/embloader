#ifndef EFI_UTILS_H
#define EFI_UTILS_H
#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Guid/FileInfo.h>
#include <Guid/FileSystemInfo.h>
#include <stdbool.h>
#include <time.h>
extern EFI_HANDLE efi_get_parent_device(EFI_HANDLE handle);
extern EFI_LOADED_IMAGE_PROTOCOL* efi_get_loaded_image(void);
extern EFI_HANDLE efi_get_current_device(void);
extern void* efi_file_get_info_by(EFI_FILE_PROTOCOL* f, EFI_GUID* guid);
extern EFI_FILE_SYSTEM_INFO* efi_get_fs_info(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs);
extern EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* efi_get_current_fs(void);
extern bool efi_timestamp_to_time(time_t in, EFI_TIME* out);
extern bool efi_timeval_to_time(struct timeval* in, EFI_TIME* out);
extern bool efi_timespec_to_time(struct timespec* in, EFI_TIME* out);
extern void efi_table_calc_sum(void* table, size_t len);
extern char* efi_device_path_to_text(EFI_DEVICE_PATH_PROTOCOL* dp);
extern EFI_DEVICE_PATH_PROTOCOL* efi_device_path_from_handle(EFI_HANDLE hand);
extern char* efi_handle_to_device_path_text(EFI_HANDLE hand);
extern const char* efi_status_to_string(EFI_STATUS st);
extern const char* efi_status_to_short_string(EFI_STATUS st);
extern const char* efi_memory_type_to_string(EFI_MEMORY_TYPE type);
extern EFI_STATUS efi_wait_any_key(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *in);
extern EFI_STATUS efi_readline(
	EFI_SIMPLE_TEXT_INPUT_PROTOCOL *in,
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out,
	char *buffer,
	size_t len,
	time_t timeout
);
EFI_DEVICE_PATH_PROTOCOL* efi_device_path_append_filepath(
	EFI_DEVICE_PATH_PROTOCOL *dp,
	const char *path
);
#endif
