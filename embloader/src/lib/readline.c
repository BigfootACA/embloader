#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include "efi-utils.h"
#include <stdint.h>
#include <string.h>
#include <ctype.h>

EFI_STATUS efi_wait_any_key(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *in) {
	EFI_INPUT_KEY key;
	EFI_STATUS status;
	if (!in) return EFI_INVALID_PARAMETER;
	do {
		status = in->ReadKeyStroke(in, &key);
	} while (!EFI_ERROR(status));
	while (true) {
		memset(&key, 0, sizeof(key));
		status = in->ReadKeyStroke(in, &key);
		if (EFI_ERROR(status)) {
			if (status == EFI_NOT_READY) {
				gBS->Stall(50000);
				continue;
			}
			return status;
		}
		break;
	}
	return EFI_SUCCESS;
}

EFI_STATUS efi_readline(
	EFI_SIMPLE_TEXT_INPUT_PROTOCOL *in,
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out,
	char *buffer,
	size_t size,
	time_t timeout
) {
	EFI_INPUT_KEY key;
	EFI_STATUS status;
	if (!in || !buffer || size == 0) return EFI_INVALID_PARAMETER;
	memset(buffer, 0, size);
	uint64_t times = 0;
	do {
		status = in->ReadKeyStroke(in, &key);
	} while (!EFI_ERROR(status));
	while (true) {
		memset(&key, 0, sizeof(key));
		if (timeout > 0 && times > timeout * 1000) {
			out->OutputString(out, L"\r\nTimeout\r\n");
			return EFI_TIMEOUT;
		}
		gBS->Stall(50000);
		times += 50;
		status = in->ReadKeyStroke(in, &key);
		if (EFI_ERROR(status)) {
			if (status == EFI_NOT_READY) continue;
			CHAR16 err_msg[64];
			UnicodeSPrint(err_msg, sizeof(err_msg),L"failed to read key: %r\r\n", status);
			out->OutputString(out, err_msg);
			return status;
		}
		times = UINT64_MAX;
		size_t len = strlen(buffer);
		if (key.ScanCode == SCAN_ESC) {
			memset(buffer, 0, len);
			out->OutputString(out, L"\r\n");
			return EFI_ABORTED;
		} else if (
			key.UnicodeChar == CHAR_LINEFEED ||
			key.UnicodeChar == CHAR_CARRIAGE_RETURN
		) {
			out->OutputString(out, L"\r\n");
			break;
		} else if (
			key.ScanCode == SCAN_DELETE ||
			key.UnicodeChar == 0x7F ||
			key.UnicodeChar == CHAR_BACKSPACE
		) {
			size_t len = strlen(buffer);
			if (len > 0) {
				buffer[len - 1] = 0;
				out->OutputString(out, L"\b \b");
			}
		} else if (
			key.ScanCode == SCAN_NULL &&
			key.UnicodeChar <= 0xFF &&
			isprint((char) key.UnicodeChar)
		) {
			if (len + 2 < size) {
				buffer[len] = (char)key.UnicodeChar;
				buffer[len + 1] = 0;
				CHAR16 wch[2] = { key.UnicodeChar, 0 };
				out->OutputString(out, wch);
			}
		}
	}
	return EFI_SUCCESS;
}
