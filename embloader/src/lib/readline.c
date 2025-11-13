#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include "efi-utils.h"
#include "encode.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

/**
 * @brief Wait for any key press from the user
 *
 * This function clears any pending key strokes and waits for a new key press.
 * It blocks until a key is pressed.
 *
 * @param in Pointer to EFI_SIMPLE_TEXT_INPUT_PROTOCOL
 * @return EFI_SUCCESS on success, error status on failure
 */
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

/**
 * @brief Parse ANSI escape sequences and convert to EFI scan codes
 *
 * This function processes ANSI escape sequences (CSI and SS3 sequences) and converts
 * them to EFI scan codes. It maintains state across multiple calls to handle multi-byte
 * sequences. Supports arrow keys, function keys, editing keys, etc.
 *
 * @param key Pointer to EFI_INPUT_KEY to process and potentially modify
 * @param ansi_state Pointer to current parser state (STATE_NORMAL, STATE_ESC, STATE_CSI)
 * @param ansi_sequence Buffer containing accumulated sequence characters
 * @param ansi_seq_len Pointer to current sequence length
 * @param esc_pending Pointer to flag indicating if ESC is pending confirmation
 * @return true if key was processed and should be handled, false if more input needed
 */
bool efi_parse_ansi_sequence(
	EFI_INPUT_KEY *key,
	int *ansi_state,
	char *ansi_sequence,
	int *ansi_seq_len,
	bool *esc_pending
) {
	if (key->ScanCode == SCAN_ESC && key->UnicodeChar == 0) {
		*ansi_state = STATE_ESC;
		*ansi_seq_len = 0;
		memset(ansi_sequence, 0, 8);
		*esc_pending = true;
		return false;
	}
	if (*ansi_state == STATE_NORMAL) return true;
	if (key->ScanCode != SCAN_NULL || key->UnicodeChar == 0) {
		*ansi_state = STATE_NORMAL;
		return false;
	}
	if (*ansi_state == STATE_ESC) switch (key->UnicodeChar) {
		case '[':
			*ansi_state = STATE_CSI;
			return false;
		case 'O':
			*ansi_state = STATE_CSI;
			ansi_sequence[0] = 'O';
			*ansi_seq_len = 1;
			return false;
		default:
			*ansi_state = STATE_NORMAL;
			return false;
	}
	if (*ansi_state != STATE_CSI || *ansi_seq_len >= 7) {
		*ansi_state = STATE_NORMAL;
		return false;
	}
	ansi_sequence[(*ansi_seq_len)++] = (char)key->UnicodeChar;
	if (*ansi_seq_len == 1 && ansi_sequence[0] != 'O') {
		switch (ansi_sequence[0]) {
			case 'A': key->ScanCode = SCAN_UP; break;
			case 'B': key->ScanCode = SCAN_DOWN; break;
			case 'C': key->ScanCode = SCAN_RIGHT; break;
			case 'D': key->ScanCode = SCAN_LEFT; break;
			case 'H': key->ScanCode = SCAN_HOME; break;
			case 'F': key->ScanCode = SCAN_END; break;
			default: return false;
		}
		key->UnicodeChar = 0;
		*ansi_state = STATE_NORMAL;
		*esc_pending = false;
		return true;
	} else if (*ansi_seq_len == 2 && ansi_sequence[1] == '~') {
		*ansi_state = STATE_NORMAL;
		*esc_pending = false;
		switch (ansi_sequence[0]) {
			case '1': key->ScanCode = SCAN_HOME; break;
			case '2': key->ScanCode = SCAN_INSERT; break;
			case '3': key->ScanCode = SCAN_DELETE; break;
			case '4': key->ScanCode = SCAN_END; break;
			case '5': key->ScanCode = SCAN_PAGE_UP; break;
			case '6': key->ScanCode = SCAN_PAGE_DOWN; break;
			case '7': key->ScanCode = SCAN_HOME; break;
			case '8': key->ScanCode = SCAN_END; break;
			default: return false;
		}
		key->UnicodeChar = 0;
		return true;
	} else if (*ansi_seq_len == 3 && ansi_sequence[0] == '1' && ansi_sequence[2] == '~') {
		*ansi_state = STATE_NORMAL;
		*esc_pending = false;
		switch (ansi_sequence[1]) {
			case '0': key->ScanCode = SCAN_F10; break;
			case '1': key->ScanCode = SCAN_F1; break;
			case '2': key->ScanCode = SCAN_F2; break;
			case '3': key->ScanCode = SCAN_F3; break;
			case '4': key->ScanCode = SCAN_F4; break;
			case '5': key->ScanCode = SCAN_F5; break;
			case '7': key->ScanCode = SCAN_F6; break;
			case '8': key->ScanCode = SCAN_F7; break;
			case '9': key->ScanCode = SCAN_F8; break;
			default: return false;
		}
		key->UnicodeChar = 0;
		return true;
	} else if (*ansi_seq_len == 3 && ansi_sequence[0] == '2' && ansi_sequence[2] == '~') {
		*ansi_state = STATE_NORMAL;
		*esc_pending = false;
		switch (ansi_sequence[1]) {
			case '0': key->ScanCode = SCAN_F9; break;
			case '1': key->ScanCode = SCAN_F10; break;
			case '3': key->ScanCode = SCAN_F11; break;
			case '4': key->ScanCode = SCAN_F12; break;
			default: return false;
		}
		key->UnicodeChar = 0;
		return true;
	} else if (*ansi_seq_len == 2 && ansi_sequence[0] == 'O') {
		*ansi_state = STATE_NORMAL;
		*esc_pending = false;
		switch (ansi_sequence[1]) {
			case 'A': key->ScanCode = SCAN_UP; break;
			case 'B': key->ScanCode = SCAN_DOWN; break;
			case 'C': key->ScanCode = SCAN_RIGHT; break;
			case 'D': key->ScanCode = SCAN_LEFT; break;
			case 'H': key->ScanCode = SCAN_HOME; break;
			case 'F': key->ScanCode = SCAN_END; break;
			case 'P': key->ScanCode = SCAN_F1; break;
			case 'Q': key->ScanCode = SCAN_F2; break;
			case 'R': key->ScanCode = SCAN_F3; break;
			case 'S': key->ScanCode = SCAN_F4; break;
			default: return false;
		}
		key->UnicodeChar = 0;
		return true;
	} else if (*ansi_seq_len >= 6) {
		*ansi_state = STATE_NORMAL;
		return false;
	}
	return false;
}

/**
 * @brief Read a line of text input from the user with editing support
 *
 * This function provides an interactive line editing interface with support for:
 * - Backspace/Delete to remove characters at cursor position
 * - Left/Right arrow keys to move cursor
 * - Character insertion at cursor position
 * - Enter to submit the input
 * - ESC to cancel and clear the buffer
 * - Timeout for automatic submission
 * - Pre-filled initial text display
 *
 * @param in Pointer to EFI_SIMPLE_TEXT_INPUT_PROTOCOL for keyboard input
 * @param out Pointer to EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL for text output
 * @param buffer Buffer containing initial text and receiving edited text (null-terminated)
 * @param size Size of the buffer in bytes
 * @param timeout Timeout in seconds (negative for infinite, 0 for immediate return on timeout)
 * @return EFI_SUCCESS on normal input completion,
 *         EFI_TIMEOUT if timeout expired,
 *         EFI_ABORTED if user pressed ESC,
 *         EFI_INVALID_PARAMETER for invalid parameters,
 *         other EFI errors on failure
 */
EFI_STATUS efi_readline(
	EFI_SIMPLE_TEXT_INPUT_PROTOCOL *in,
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out,
	char *buffer,
	size_t size,
	time_t timeout
) {
	EFI_INPUT_KEY key;
	EFI_STATUS status = EFI_NOT_READY;
	CHAR16 *str, wch[2] = {0}, msgbuf[64];
	if (!in || !buffer || size == 0) return EFI_INVALID_PARAMETER;
	size_t i, len = strlen(buffer), cursor_pos = len;
	if (buffer[0] && (str = encode_utf8_to_utf16(buffer))) {
		out->OutputString(out, str);
		free(str);
	}
	uint64_t times = 0;
	int ansi_state = STATE_NORMAL;
	char ansi_sequence[8];
	int ansi_seq_len = 0;
	bool esc_pending = false;
	while (true) {
		memset(&key, 0, sizeof(key));
		gBS->Stall(50000);
		times += 50;
		status = in->ReadKeyStroke(in, &key);
		if (status == EFI_NOT_READY) {
			if (esc_pending) {
				esc_pending = false;
				ansi_state = STATE_NORMAL;
				status = EFI_SUCCESS;
				if (ansi_seq_len == 0) goto quit;
				continue;
			} else {
				if (timeout >= 0 && times > timeout * 1000) {
					out->OutputString(out, L"\r\n");
					if (timeout != 0) out->OutputString(out, L"Timeout\r\n");
					return EFI_TIMEOUT;
				}
				continue;
			}
		}
		if (EFI_ERROR(status)) {
			UnicodeSPrint(msgbuf, sizeof(msgbuf), L"failed to read key: %r\r\n", status);
			out->OutputString(out, msgbuf);
			return status;
		}
		len = strlen(buffer);
		if (!efi_parse_ansi_sequence(
			&key, &ansi_state, ansi_sequence,
			&ansi_seq_len, &esc_pending
		)) continue;
		if (key.ScanCode == SCAN_ESC) goto quit;
		else if (
			key.UnicodeChar == CHAR_LINEFEED ||
			key.UnicodeChar == CHAR_CARRIAGE_RETURN
		) {
			out->OutputString(out, L"\r\n");
			break;
		} else if (key.ScanCode == SCAN_LEFT) {
			if (cursor_pos > 0) {
				cursor_pos--;
				out->OutputString(out, L"\b");
			}
		} else if (key.ScanCode == SCAN_RIGHT) {
			if (cursor_pos < len) {
				wch[0] = (CHAR16)buffer[cursor_pos];
				out->OutputString(out, wch);
				cursor_pos++;
			}
		} else if (key.ScanCode == SCAN_HOME) {
			while (cursor_pos > 0) {
				cursor_pos--;
				out->OutputString(out, L"\b");
			}
		} else if (key.ScanCode == SCAN_END) {
			while (cursor_pos < len) {
				wch[0] = (CHAR16)buffer[cursor_pos];
				out->OutputString(out, wch);
				cursor_pos++;
			}
		} else if (
			key.ScanCode == SCAN_DELETE ||
			key.UnicodeChar == 0x7F
		) {
			if (cursor_pos < len) {
				memmove(&buffer[cursor_pos], &buffer[cursor_pos + 1], len - cursor_pos);
				len--;
				if ((str = encode_utf8_to_utf16(&buffer[cursor_pos]))) {
					out->OutputString(out, str);
					free(str);
				}
				out->OutputString(out, L" ");
				for (i = cursor_pos; i <= len; i++)
					out->OutputString(out, L"\b");
			}
		} else if (key.UnicodeChar == CHAR_BACKSPACE) {
			if (cursor_pos > 0) {
				memmove(&buffer[cursor_pos - 1], &buffer[cursor_pos], len - cursor_pos + 1);
				cursor_pos--, len--;
				out->OutputString(out, L"\b");
				if ((str = encode_utf8_to_utf16(&buffer[cursor_pos]))) {
					out->OutputString(out, str);
					free(str);
				}
				out->OutputString(out, L" ");
				for (i = cursor_pos; i <= len; i++)
					out->OutputString(out, L"\b");
			}
		} else if (
			key.ScanCode == SCAN_NULL &&
			key.UnicodeChar <= 0xFF &&
			isprint((char) key.UnicodeChar)
		) {
			if (len + 1 < size) {
				timeout = INT32_MAX;
				memmove(&buffer[cursor_pos + 1], &buffer[cursor_pos], len - cursor_pos + 1);
				buffer[cursor_pos] = (char)key.UnicodeChar;
				len++;
				if ((str = encode_utf8_to_utf16(&buffer[cursor_pos]))) {
					out->OutputString(out, str);
					free(str);
				}
				cursor_pos++;
				for (i = cursor_pos; i < len; i++)
					out->OutputString(out, L"\b");
			}
		}
	}
	return EFI_SUCCESS;
quit:
	for (i = cursor_pos; i < len; i++)
		out->OutputString(out, L" ");
	for (i = 0; i < len; i++)
		out->OutputString(out, L"\b \b");
	memset(buffer, 0, len);
	out->OutputString(out, L"\r\n");
	return EFI_ABORTED;
}
