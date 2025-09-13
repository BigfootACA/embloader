#include "efi-utils.h"

const char* efi_status_to_string(EFI_STATUS st) {
	switch (st) {
		case EFI_SUCCESS: return "Success";
		case EFI_WARN_UNKNOWN_GLYPH: return "Unknown Glyph";
		case EFI_WARN_DELETE_FAILURE: return "Delete Failure";
		case EFI_WARN_WRITE_FAILURE: return "Write Failure";
		case EFI_WARN_BUFFER_TOO_SMALL: return "Buffer Too Small";
		case EFI_WARN_STALE_DATA: return "Stale Data";
		case EFI_LOAD_ERROR: return "Load Error";
		case EFI_INVALID_PARAMETER: return "Invalid Parameter";
		case EFI_UNSUPPORTED: return "Unsupported";
		case EFI_BAD_BUFFER_SIZE: return "Bad Buffer Size";
		case EFI_BUFFER_TOO_SMALL: return "Buffer Too Small";
		case EFI_NOT_READY: return "Not Ready";
		case EFI_DEVICE_ERROR: return "Device Error";
		case EFI_WRITE_PROTECTED: return "Write Protected";
		case EFI_OUT_OF_RESOURCES: return "Out of Resources";
		case EFI_VOLUME_CORRUPTED: return "Volume Corrupt";
		case EFI_VOLUME_FULL: return "Volume Full";
		case EFI_NO_MEDIA: return "No Media";
		case EFI_MEDIA_CHANGED: return "Media changed";
		case EFI_NOT_FOUND: return "Not Found";
		case EFI_ACCESS_DENIED: return "Access Denied";
		case EFI_NO_RESPONSE: return "No Response";
		case EFI_NO_MAPPING: return "No mapping";
		case EFI_TIMEOUT: return "Time out";
		case EFI_NOT_STARTED: return "Not started";
		case EFI_ALREADY_STARTED: return "Already started";
		case EFI_ABORTED: return "Aborted";
		case EFI_ICMP_ERROR: return "ICMP Error";
		case EFI_TFTP_ERROR: return "TFTP Error";
		case EFI_PROTOCOL_ERROR: return "Protocol Error";
		case EFI_INCOMPATIBLE_VERSION: return "Incompatible Version";
		case EFI_SECURITY_VIOLATION: return "Security Violation";
		case EFI_CRC_ERROR: return "CRC Error";
		case EFI_END_OF_MEDIA: return "End of Media";
		case EFI_END_OF_FILE: return "End of File";
		case EFI_INVALID_LANGUAGE: return "Invalid Language";
		case EFI_COMPROMISED_DATA: return "Compromised Data";
		default: return "Unknown";
	}
}

const char* efi_status_to_short_string(EFI_STATUS st) {
	switch (st) {
		case EFI_SUCCESS: return "success";
		case EFI_WARN_UNKNOWN_GLYPH: return "warn-unknown-glyph";
		case EFI_WARN_DELETE_FAILURE: return "warn-delete-failure";
		case EFI_WARN_WRITE_FAILURE: return "warn-write-failure";
		case EFI_WARN_BUFFER_TOO_SMALL: return "warn-buffer-too-small";
		case EFI_WARN_STALE_DATA: return "warn-stale-data";
		case EFI_LOAD_ERROR: return "load-error";
		case EFI_INVALID_PARAMETER: return "invalid-parameter";
		case EFI_UNSUPPORTED: return "unsupported";
		case EFI_BAD_BUFFER_SIZE: return "bad-buffer-size";
		case EFI_BUFFER_TOO_SMALL: return "buffer-too-small";
		case EFI_NOT_READY: return "not-ready";
		case EFI_DEVICE_ERROR: return "device-error";
		case EFI_WRITE_PROTECTED: return "write-protected";
		case EFI_OUT_OF_RESOURCES: return "out-of-resources";
		case EFI_VOLUME_CORRUPTED: return "volume-corrupted";
		case EFI_VOLUME_FULL: return "volume-full";
		case EFI_NO_MEDIA: return "no-media";
		case EFI_MEDIA_CHANGED: return "media-changed";
		case EFI_NOT_FOUND: return "not-found";
		case EFI_ACCESS_DENIED: return "access-denied";
		case EFI_NO_RESPONSE: return "no-response";
		case EFI_NO_MAPPING: return "no-mapping";
		case EFI_TIMEOUT: return "timeout";
		case EFI_NOT_STARTED: return "not-started";
		case EFI_ALREADY_STARTED: return "already-started";
		case EFI_ABORTED: return "aborted";
		case EFI_ICMP_ERROR: return "icmp-error";
		case EFI_TFTP_ERROR: return "tftp-error";
		case EFI_PROTOCOL_ERROR: return "protocol-error";
		case EFI_INCOMPATIBLE_VERSION: return "incompatible-version";
		case EFI_SECURITY_VIOLATION: return "security-violation";
		case EFI_CRC_ERROR: return "crc-error";
		case EFI_END_OF_MEDIA: return "end-of-media";
		case EFI_END_OF_FILE: return "end-of-file";
		case EFI_INVALID_LANGUAGE: return "invalid-language";
		case EFI_COMPROMISED_DATA: return "compromised-data";
		default: return "unknown";
	}
}

const char* efi_memory_type_to_string(EFI_MEMORY_TYPE type) {
	switch (type) {
		case EfiReservedMemoryType: return "Reserved";
		case EfiLoaderCode: return "Loader Code";
		case EfiLoaderData: return "Loader Data";
		case EfiBootServicesCode: return "Boot Services Code";
		case EfiBootServicesData: return "Boot Services Data";
		case EfiRuntimeServicesCode: return "Runtime Services Code";
		case EfiRuntimeServicesData: return "Runtime Services Data";
		case EfiConventionalMemory: return "Conventional";
		case EfiUnusableMemory: return "Unusable";
		case EfiACPIReclaimMemory: return "ACPI Reclaim";
		case EfiACPIMemoryNVS: return "ACPI Memory NVS";
		case EfiMemoryMappedIO: return "MMIO";
		case EfiMemoryMappedIOPortSpace: return "MMIO Port Space";
		case EfiPalCode: return "Pal Code";
		case EfiPersistentMemory: return "Persistent";
		case EfiMaxMemoryType: return "Max";
		default: return "Unknown";
	}
}
