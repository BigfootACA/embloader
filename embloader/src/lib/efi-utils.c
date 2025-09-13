#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/DevicePathToText.h>
#include "efi-utils.h"
#include "encode.h"
#include "log.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

EFI_LOADED_IMAGE_PROTOCOL* efi_get_loaded_image(void) {
	EFI_STATUS st;
	EFI_LOADED_IMAGE_PROTOCOL* li = NULL;
	st = gBS->HandleProtocol(
		gImageHandle, &gEfiLoadedImageProtocolGuid, (void**) &li);
	if (EFI_ERROR(st)) return NULL;
	return li;
}

EFI_HANDLE efi_get_current_device(void) {
	EFI_LOADED_IMAGE_PROTOCOL* li = efi_get_loaded_image();
	return li ? li->DeviceHandle : NULL;
}

EFI_DEVICE_PATH_PROTOCOL* efi_device_path_next(EFI_DEVICE_PATH_PROTOCOL* dp) {
	if (!dp || dp->Length == 0) return NULL;
	EFI_DEVICE_PATH_PROTOCOL* next_dp =
		(void*) dp + *(UINT16*) dp->Length;
	if (next_dp->Length == 0) return NULL;
	return next_dp;
}

size_t efi_device_path_len(EFI_DEVICE_PATH_PROTOCOL* dp) {
	size_t dp_len = 0;
	if (dp) do {
		dp_len += *(UINT16*) dp->Length;
		if (dp->Type == 0x7f) break;
	} while ((dp = efi_device_path_next(dp)));
	return dp_len;
}

size_t efi_device_path_count(EFI_DEVICE_PATH_PROTOCOL* dp) {
	size_t cnt = 0;
	if (dp) do {
		if (dp->Type == 0x7f) break;
		cnt++;
	} while ((dp = efi_device_path_next(dp)));
	return cnt;
}

EFI_HANDLE efi_get_parent_device(EFI_HANDLE handle) {
	EFI_STATUS st;
	EFI_DEVICE_PATH_PROTOCOL* dp = NULL;
	if (!handle) return NULL;
	st = gBS->HandleProtocol(handle, &gEfiDevicePathProtocolGuid, (void**) &dp);
	if (EFI_ERROR(st) || !dp) return NULL;
	size_t dp_len = 0, cnt = 0;
	dp_len = efi_device_path_len(dp);
	cnt = efi_device_path_count(dp);
	if (cnt <= 1) return NULL;
	EFI_DEVICE_PATH_PROTOCOL* par_dp = NULL;
	if (!(par_dp = malloc(dp_len))) return NULL;
	memcpy(par_dp, dp, dp_len);
	EFI_DEVICE_PATH_PROTOCOL *last_dp = NULL, *next_dp = NULL;
	last_dp = par_dp;
	while (true) {
		next_dp = efi_device_path_next(last_dp);
		if (!next_dp || next_dp->Type == 0x7f) {
			last_dp->Type = 0x7f;
			last_dp->SubType = 0xff;
			*((UINT16*) last_dp->Length) = sizeof(EFI_DEVICE_PATH_PROTOCOL);
			break;
		}
		last_dp = next_dp;
	}
	EFI_HANDLE parent = NULL;
	EFI_DEVICE_PATH_PROTOCOL* loc_dp = par_dp;
	st = gBS->LocateDevicePath(
		&gEfiDevicePathProtocolGuid, (void*) &loc_dp, &parent
	);
	free(par_dp);
	if (EFI_ERROR(st) || !parent) return NULL;
	if (parent == handle) return NULL;
	return parent;
}

void* efi_file_get_info_by(EFI_FILE_PROTOCOL* f, EFI_GUID* guid) {
	void* info = NULL;
	UINTN size = 0;
	EFI_STATUS st;
	if (!f || !guid) return NULL;
	st = f->GetInfo(f, guid, &size, NULL);
	if (st != EFI_BUFFER_TOO_SMALL || size == 0) return NULL;
	info = malloc(size);
	if (!info) return NULL;
	st = f->GetInfo(f, guid, &size, info);
	if (EFI_ERROR(st)) {
		free(info);
		return NULL;
	}
	return info;
}

EFI_FILE_SYSTEM_INFO* efi_get_fs_info(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs) {
	EFI_STATUS st;
	EFI_FILE_PROTOCOL* root = NULL;
	EFI_FILE_SYSTEM_INFO* info = NULL;
	if (!fs) return NULL;
	st = fs->OpenVolume(fs, &root);
	if (EFI_ERROR(st) || !root) return NULL;
	info = efi_file_get_info_by(root, &gEfiFileSystemInfoGuid);
	if (root) root->Close(root);
	return info;
}

EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* efi_get_current_fs(void) {
	EFI_STATUS st;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs = NULL;
	EFI_HANDLE dev = efi_get_current_device();
	if (!dev) return NULL;
	st = gBS->HandleProtocol(dev, &gEfiSimpleFileSystemProtocolGuid, (void**) &fs);
	if (EFI_ERROR(st) || !fs) return NULL;
	return fs;
}

bool efi_timestamp_to_time(time_t in, EFI_TIME* out) {
	struct tm* tm = localtime(&in);
	if (!tm || !out) return false;
	memset(out, 0, sizeof(EFI_TIME));
	out->Year = 1900 + tm->tm_year;
	out->Month = tm->tm_mon + 1;
	out->Day = tm->tm_mday;
	out->Hour = tm->tm_hour;
	out->Minute = tm->tm_min;
	out->Second = tm->tm_sec;
	out->Daylight = (tm->tm_isdst > 0 ? 2 : 0);
	return true;
}

bool efi_timeval_to_time(struct timeval* in, EFI_TIME* out) {
	bool r;
	if (!in || !out) return false;
	r = efi_timestamp_to_time(in->tv_sec, out);
	if (r) out->Nanosecond = in->tv_usec * 1000;
	return r;
}

bool efi_timespec_to_time(struct timespec* in, EFI_TIME* out) {
	bool r;
	if (!in || !out) return false;
	r = efi_timestamp_to_time(in->tv_sec, out);
	if (r) out->Nanosecond = in->tv_nsec;
	return r;
}

void efi_table_calc_sum(void* table, size_t len) {
	uint32_t sum;
	EFI_TABLE_HEADER* hdr = table;
	if (!table || len < sizeof(EFI_TABLE_HEADER)) return;
	hdr->CRC32 = 0;
	extern uint32_t s_crc32(void* buffer, size_t length);
	sum = s_crc32(hdr, len);
	hdr->CRC32 = sum;
}

char* efi_handle_to_device_path_text(EFI_HANDLE hand) {
	EFI_DEVICE_PATH_PROTOCOL *dp = NULL;
	dp = efi_device_path_from_handle(hand);
	if (!dp) return NULL;
	return efi_device_path_to_text(dp);
}

char* efi_device_path_to_text(EFI_DEVICE_PATH_PROTOCOL* dp) {
	EFI_STATUS status;
	EFI_DEVICE_PATH_TO_TEXT_PROTOCOL *dpt = NULL;
	if (!dp) return NULL;
	status = gBS->LocateProtocol(
		&gEfiDevicePathToTextProtocolGuid,
		NULL, (VOID**) &dpt
	);
	if (EFI_ERROR(status) || !dpt) return NULL;
	CHAR16 *wtdp = dpt->ConvertDevicePathToText(
		dp, FALSE, FALSE
	);
	if (!wtdp) return NULL;
	char *tdp = encode_utf16_to_utf8(wtdp);
	FreePool(wtdp);
	return tdp;
}

EFI_DEVICE_PATH_PROTOCOL* efi_device_path_from_handle(EFI_HANDLE hand) {
	EFI_STATUS status;
	EFI_DEVICE_PATH_PROTOCOL *dp = NULL;
	if (!hand) return NULL;
	status = gBS->HandleProtocol(
		hand,
		&gEfiDevicePathProtocolGuid,
		(VOID**) &dp
	);
	if (EFI_ERROR(status) || !dp) return NULL;
	return dp;
}

EFI_DEVICE_PATH_PROTOCOL* efi_device_path_append_filepath(
	EFI_DEVICE_PATH_PROTOCOL *dp,
	const char *path
) {
	EFI_DEVICE_PATH_PROTOCOL *ret;
	FILEPATH_DEVICE_PATH *fdp;
	if (!dp || !path) return NULL;
	size_t plen = strlen(path);
	size_t len = sizeof(FILEPATH_DEVICE_PATH) +
		((plen + 3) * sizeof(CHAR16));
	if (!(fdp = malloc(len))) return NULL;
	memset(fdp, 0, len);
	fdp->Header.Type = MEDIA_DEVICE_PATH;
	fdp->Header.SubType = MEDIA_FILEPATH_DP;
	fdp->Header.Length[0] = (UINT8)(len & 0xFF);
	fdp->Header.Length[1] = (UINT8)((len >> 8) & 0xFF);
	CHAR16 *buf = fdp->PathName;
	if (path[0] != '/' && path[0] != '\\') *buf++ = L'/';
	AsciiStrToUnicodeStrS(path, buf, len + 1);
	for (size_t i = 0; i < plen; i++)
		if (buf[i] == L'/') buf[i] = L'\\';
	ret = AppendDevicePathNode(dp, (EFI_DEVICE_PATH_PROTOCOL*)fdp);
	free(fdp);
	if (!ret) log_warning("append filepath %s failed", path);
	return ret;
}
