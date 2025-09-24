#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <Library/BaseLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include "encode.h"
#include "variables.h"

EFI_STATUS efivar_set_raw(
	const EFI_GUID *vendor,
	const char *name,
	const void *buf,
	size_t size,
	uint32_t flags
) {
	EFI_STATUS status;
	CHAR16 *wname = NULL;
	if (!vendor || !name) return EFI_INVALID_PARAMETER;
	flags |= EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS;
	if (!(wname = encode_utf8_to_utf16(name))) return EFI_OUT_OF_RESOURCES;
	status = gRT->SetVariable(wname, (EFI_GUID *) vendor, flags, size, (void *) buf);
	free(wname);
	return status;
}

EFI_STATUS efivar_set_str16(
	const EFI_GUID *vendor,
	const char *name,
	const CHAR16 *value,
	uint32_t flags
) {
	if (!vendor || !name) return EFI_INVALID_PARAMETER;
	return efivar_set_raw(vendor, name, value, value ? StrSize(value) : 0, flags);
}

EFI_STATUS efivar_set_str8(
	const EFI_GUID *vendor,
	const char *name,
	const char *value,
	uint32_t flags
) {
	if (!vendor || !name) return EFI_INVALID_PARAMETER;
	return efivar_set_raw(vendor, name, value, value ? AsciiStrSize(value) : 0, flags);
}

EFI_STATUS efivar_set_str16_from_str8(
	const EFI_GUID *vendor,
	const char *name,
	const char *value,
	uint32_t flags
) {
	CHAR16 *wvalue = NULL;
	EFI_STATUS status;
	if (!vendor || !name) return EFI_INVALID_PARAMETER;
	if (value && !(wvalue = encode_utf8_to_utf16(value))) return EFI_OUT_OF_RESOURCES;
	status = efivar_set_str16(vendor, name, wvalue, flags);
	if (wvalue) free(wvalue);
	return status;
}

EFI_STATUS efivar_set_str8_from_str16(
	const EFI_GUID *vendor,
	const char *name,
	const CHAR16 *value,
	uint32_t flags
) {
	char *svalue = NULL;
	EFI_STATUS status;
	if (!vendor || !name) return EFI_INVALID_PARAMETER;
	if (value && !(svalue = encode_utf16_to_utf8(value))) return EFI_OUT_OF_RESOURCES;
	status = efivar_set_str8(vendor, name, svalue, flags);
	if (svalue) free(svalue);
	return status;
}

EFI_STATUS efivar_set_vfmt8(
	const EFI_GUID *vendor,
	const char *name,
	uint32_t flags,
	const char *fmt,
	va_list args
) {
	char *buf = NULL;
	if (!vendor || !name || !fmt) return EFI_INVALID_PARAMETER;
	int ret = vasprintf(&buf, fmt, args);
	if (ret < 0 || !buf) return EFI_OUT_OF_RESOURCES;
	EFI_STATUS status = efivar_set_str8(vendor, name, buf, flags);
	free(buf);
	return status;
}

EFI_STATUS efivar_set_vfmt16(
	const EFI_GUID *vendor,
	const char *name,
	uint32_t flags,
	const char *fmt,
	va_list args
) {
	char *buf = NULL;
	if (!vendor || !name || !fmt) return EFI_INVALID_PARAMETER;
	int ret = vasprintf(&buf, fmt, args);
	if (ret < 0 || !buf) return EFI_OUT_OF_RESOURCES;
	EFI_STATUS status = efivar_set_str16_from_str8(vendor, name, buf, flags);
	free(buf);
	return status;
}

EFI_STATUS efivar_set_fmt8(
	const EFI_GUID *vendor,
	const char *name,
	uint32_t flags,
	const char *fmt,
	...
) {
	EFI_STATUS status;
	va_list args;
	if (!vendor || !name || !fmt) return EFI_INVALID_PARAMETER;
	va_start(args, fmt);
	status = efivar_set_vfmt8(vendor, name, flags, fmt, args);
	va_end(args);
	return status;
}

EFI_STATUS efivar_set_fmt16(
	const EFI_GUID *vendor,
	const char *name,
	uint32_t flags,
	const char *fmt,
	...
) {
	EFI_STATUS status;
	va_list args;
	if (!vendor || !name || !fmt) return EFI_INVALID_PARAMETER;
	va_start(args, fmt);
	status = efivar_set_vfmt16(vendor, name, flags, fmt, args);
	va_end(args);
	return status;
}

EFI_STATUS efivar_set_uint32_le(
	const EFI_GUID *vendor,
	const char *name,
	uint32_t value,
	uint32_t flags
) {
	uint8_t buf[4];
	if (!vendor || !name) return EFI_INVALID_PARAMETER;
	buf[0] = (uint8_t)(value >> 0U & 0xFF);
	buf[1] = (uint8_t)(value >> 8U & 0xFF);
	buf[2] = (uint8_t)(value >> 16U & 0xFF);
	buf[3] = (uint8_t)(value >> 24U & 0xFF);
	return efivar_set_raw(vendor, name, buf, sizeof(buf), flags);
}

EFI_STATUS efivar_set_uint64_le(
	const EFI_GUID *vendor,
	const char *name,
	uint64_t value,
	uint32_t flags
) {
	uint8_t buf[8];
	if (!vendor || !name) return EFI_INVALID_PARAMETER;
	buf[0] = (uint8_t)(value >> 0U & 0xFF);
	buf[1] = (uint8_t)(value >> 8U & 0xFF);
	buf[2] = (uint8_t)(value >> 16U & 0xFF);
	buf[3] = (uint8_t)(value >> 24U & 0xFF);
	buf[4] = (uint8_t)(value >> 32U & 0xFF);
	buf[5] = (uint8_t)(value >> 40U & 0xFF);
	buf[6] = (uint8_t)(value >> 48U & 0xFF);
	buf[7] = (uint8_t)(value >> 56U & 0xFF);
	return efivar_set_raw(vendor, name, buf, sizeof(buf), flags);
}

EFI_STATUS efivar_get_raw(
	const EFI_GUID *vendor,
	const char *name,
	void **ret_data,
	size_t *ret_size
) {
	UINTN size = 0;
	void *buf = NULL;
	CHAR16 *wname = NULL;
	EFI_STATUS status = EFI_SUCCESS;
	if (!vendor || !name) return EFI_INVALID_PARAMETER;
	if (!(wname = encode_utf8_to_utf16(name))) return EFI_OUT_OF_RESOURCES;
	status = gRT->GetVariable(wname, (EFI_GUID *) vendor, NULL, &size, NULL);
	if (status != EFI_BUFFER_TOO_SMALL) {
		if (!EFI_ERROR(status)) status = EFI_BAD_BUFFER_SIZE;
		goto done;
	}
	if (!(buf = malloc(size + 2))) return EFI_OUT_OF_RESOURCES;
	memset(buf, 0, size + 2);
	status = gRT->GetVariable(wname, (EFI_GUID *) vendor, NULL, &size, buf);
	if (EFI_ERROR(status)) goto done;
	if (ret_data) *ret_data = buf, buf = NULL;
	if (ret_size) *ret_size = size;
done:
	if (buf) free(buf);
	if (wname) free(wname);
	return status;
}

EFI_STATUS efivar_get_str16(
	const EFI_GUID *vendor,
	const char *name,
	CHAR16 **ret
) {
	CHAR16 *buf = NULL;
	EFI_STATUS status;
	size_t size;
	if (!vendor || !name) return EFI_INVALID_PARAMETER;
	status = efivar_get_raw(vendor, name, (void**) &buf, &size);
	if (EFI_ERROR(status)) return status;
	if ((size % sizeof(CHAR16)) != 0) {
		free(buf);
		return EFI_INVALID_PARAMETER;
	}
	if (!ret) {
		free(buf);
		return EFI_SUCCESS;
	}
	*ret = buf;
	return EFI_SUCCESS;
}

EFI_STATUS efivar_get_str8(
	const EFI_GUID *vendor,
	const char *name,
	char **ret
) {
	char *buf = NULL;
	EFI_STATUS status;
	size_t size;
	if (!vendor || !name) return EFI_INVALID_PARAMETER;
	status = efivar_get_raw(vendor, name, (void**) &buf, &size);
	if (EFI_ERROR(status)) return status;
	if (!ret) {
		free(buf);
		return EFI_SUCCESS;
	}
	*ret = buf;
	return EFI_SUCCESS;
}

EFI_STATUS efivar_get_str16_to_str8(
	const EFI_GUID *vendor,
	const char *name,
	char **ret
) {
	CHAR16 *wbuf = NULL;
	char *buf = NULL;
	EFI_STATUS status;
	if (!vendor || !name) return EFI_INVALID_PARAMETER;
	status = efivar_get_str16(vendor, name, &wbuf);
	if (EFI_ERROR(status)) return status;
	if (!ret) {
		free(wbuf);
		return EFI_SUCCESS;
	}
	if (!(buf = encode_utf16_to_utf8(wbuf))) {
		free(wbuf);
		return EFI_OUT_OF_RESOURCES;
	}
	free(wbuf);
	*ret = buf;
	return EFI_SUCCESS;
}

EFI_STATUS efivar_get_str8_to_str16(
	const EFI_GUID *vendor,
	const char *name,
	CHAR16 **ret
) {
	char *buf = NULL;
	CHAR16 *wbuf = NULL;
	EFI_STATUS status;
	if (!vendor || !name) return EFI_INVALID_PARAMETER;
	status = efivar_get_str8(vendor, name, &buf);
	if (EFI_ERROR(status)) return status;
	if (!ret) {
		free(buf);
		return EFI_SUCCESS;
	}
	if (!(wbuf = encode_utf8_to_utf16(buf))) {
		free(buf);
		return EFI_OUT_OF_RESOURCES;
	}
	free(buf);
	*ret = wbuf;
	return EFI_SUCCESS;
}

EFI_STATUS efivar_get_str16_to_uint64(
	const EFI_GUID *vendor,
	const char *name,
	uint64_t *ret
) {
	char *buf = NULL;
	EFI_STATUS status;
	if (!vendor || !name) return EFI_INVALID_PARAMETER;
	status = efivar_get_str16_to_str8(vendor, name, &buf);
	if (EFI_ERROR(status)) return status;
	char *end = NULL;
	unsigned long long val = strtoull(buf, &end, 0);
	if (end == buf || *end != '\0') {
		free(buf);
		return EFI_INVALID_PARAMETER;
	}
	free(buf);
	if (ret) *ret = val;
	return EFI_SUCCESS;
}

EFI_STATUS efivar_get_str8_to_uint64(
	const EFI_GUID *vendor,
	const char *name,
	uint64_t *ret
) {
	char *buf = NULL;
	EFI_STATUS status;
	if (!vendor || !name) return EFI_INVALID_PARAMETER;
	status = efivar_get_str8(vendor, name, &buf);
	if (EFI_ERROR(status)) return status;
	char *end = NULL;
	unsigned long long val = strtoull(buf, &end, 0);
	if (end == buf || *end != '\0') {
		free(buf);
		return EFI_INVALID_PARAMETER;
	}
	free(buf);
	if (ret) *ret = val;
	return EFI_SUCCESS;
}

EFI_STATUS efivar_get_uint32_le(
	const EFI_GUID *vendor,
	const char *name,
	uint32_t *ret
) {
	size_t size;
	EFI_STATUS status;
	uint8_t *buf = NULL;
	if (!vendor || !name) return EFI_INVALID_PARAMETER;
	status = efivar_get_raw(vendor, name, (void**) &buf, &size);
	if (EFI_ERROR(status)) return status;
	if (size != sizeof(uint32_t)) {
		if (buf) free(buf);
		return EFI_BUFFER_TOO_SMALL;
	}
	if (ret) *ret =
		(uint32_t) buf[0] << 0U |
		(uint32_t) buf[1] << 8U |
		(uint32_t) buf[2] << 16U |
		(uint32_t) buf[3] << 24U;
	return EFI_SUCCESS;
}

EFI_STATUS efivar_get_uint64_le(
	const EFI_GUID *vendor,
	const char *name,
	uint64_t *ret
) {
	uint8_t *buf = NULL;
	size_t size;
	EFI_STATUS status;
	if (!vendor || !name) return EFI_INVALID_PARAMETER;
	status = efivar_get_raw(vendor, name, (void**) &buf, &size);
	if (EFI_ERROR(status)) return status;
	if (size != sizeof(uint64_t)) {
		if (buf) free(buf);
		return EFI_BUFFER_TOO_SMALL;
	}
	if (ret) *ret =
		(uint64_t) buf[0] << 0U |
		(uint64_t) buf[1] << 8U |
		(uint64_t) buf[2] << 16U |
		(uint64_t) buf[3] << 24U |
		(uint64_t) buf[4] << 32U |
		(uint64_t) buf[5] << 40U |
		(uint64_t) buf[6] << 48U |
		(uint64_t) buf[7] << 56U;
	return EFI_SUCCESS;
}

EFI_STATUS efivar_unset(
	const EFI_GUID *vendor,
	const char *name,
	uint32_t flags
) {
	EFI_STATUS status;
	if (!vendor || !name) return EFI_INVALID_PARAMETER;
	status = efivar_get_raw(vendor, name, NULL, NULL);
	if (!EFI_ERROR(status))
		status = efivar_set_raw(vendor, name, NULL, 0, flags);
	return status;
}
