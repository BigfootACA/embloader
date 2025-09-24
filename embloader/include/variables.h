#ifndef VARIABLES_H
#define VARIABLES_H
#include <Uefi.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
extern EFI_STATUS efivar_set_raw(
	const EFI_GUID *vendor,
	const char *name,
	const void *buf,
	size_t size,
	uint32_t flags
);
extern EFI_STATUS efivar_set_str16(
	const EFI_GUID *vendor,
	const char *name,
	const CHAR16 *value,
	uint32_t flags
);
extern EFI_STATUS efivar_set_str8(
	const EFI_GUID *vendor,
	const char *name,
	const char *value,
	uint32_t flags
);
extern EFI_STATUS efivar_set_str16_from_str8(
	const EFI_GUID *vendor,
	const char *name,
	const char *value,
	uint32_t flags
);
extern EFI_STATUS efivar_set_str8_from_str16(
	const EFI_GUID *vendor,
	const char *name,
	const CHAR16 *value,
	uint32_t flags
);
extern EFI_STATUS efivar_set_vfmt8(
	const EFI_GUID *vendor,
	const char *name,
	uint32_t flags,
	const char *fmt,
	va_list args
);
extern EFI_STATUS efivar_set_vfmt16(
	const EFI_GUID *vendor,
	const char *name,
	uint32_t flags,
	const char *fmt,
	va_list args
);
extern EFI_STATUS efivar_set_fmt8(
	const EFI_GUID *vendor,
	const char *name,
	uint32_t flags,
	const char *fmt,
	...
);
extern EFI_STATUS efivar_set_fmt16(
	const EFI_GUID *vendor,
	const char *name,
	uint32_t flags,
	const char *fmt,
	...
);
extern EFI_STATUS efivar_set_uint32_le(
	const EFI_GUID *vendor,
	const char *name,
	uint32_t value,
	uint32_t flags
);
extern EFI_STATUS efivar_set_uint64_le(
	const EFI_GUID *vendor,
	const char *name,
	uint64_t value,
	uint32_t flags
);
extern EFI_STATUS efivar_get_raw(
	const EFI_GUID *vendor,
	const char *name,
	void **ret_data,
	size_t *ret_size
);
extern EFI_STATUS efivar_get_str16(
	const EFI_GUID *vendor,
	const char *name,
	CHAR16 **ret
);
extern EFI_STATUS efivar_get_str8(
	const EFI_GUID *vendor,
	const char *name,
	char **ret
);
extern EFI_STATUS efivar_get_str16_to_str8(
	const EFI_GUID *vendor,
	const char *name,
	char **ret
);
extern EFI_STATUS efivar_get_str8_to_str16(
	const EFI_GUID *vendor,
	const char *name,
	CHAR16 **ret
);
extern EFI_STATUS efivar_get_str16_to_uint64(
	const EFI_GUID *vendor,
	const char *name,
	uint64_t *ret
);
extern EFI_STATUS efivar_get_str8_to_uint64(
	const EFI_GUID *vendor,
	const char *name,
	uint64_t *ret
);
extern EFI_STATUS efivar_get_uint32_le(
	const EFI_GUID *vendor,
	const char *name,
	uint32_t *ret
);
extern EFI_STATUS efivar_get_uint64_le(
	const EFI_GUID *vendor,
	const char *name,
	uint64_t *ret
);
extern EFI_STATUS efivar_unset(
	const EFI_GUID *vendor,
	const char *name,
	uint32_t flags
);
#endif
