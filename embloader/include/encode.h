#ifndef ENCODE_H
#define ENCODE_H
#include <Uefi.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum encoding {
	ENC_NONE,   ///< No encoding specified
	ENC_UTF8,   ///< UTF-8 encoding
	ENC_UTF16,  ///< UTF-16 encoding
} encoding;

/** Character transfer rule for encoding conversion. */
typedef struct convert_transfer {
	const void* match;      ///< Pattern to match in source data in source encoding
	size_t match_size;      ///< Size of match pattern in bytes
	const void* replace;    ///< Replacement data in destination encoding
	size_t replace_size;    ///< Size of replacement data in bytes
} convert_transfer;

/** Encoding conversion context structure. */
typedef struct encode_convert_ctx {
	struct {
		encoding src;                   ///< Source encoding format
		encoding dst;                   ///< Destination encoding format
		const void* src_ptr;           ///< Pointer to source data
		void* dst_ptr;                 ///< Pointer to destination buffer
		size_t src_size;               ///< Size of source data in bytes
		size_t dst_size;               ///< Size of destination buffer in bytes
		convert_transfer** transfers;  ///< Array of character transfer rules (optional)
		bool allow_invalid;            ///< Whether to allow invalid character replacement
	} in;
	struct {
		void* src_end;      ///< Pointer to end of processed source data
		void* dst_end;      ///< Pointer to end of written destination data
		size_t src_used;    ///< Number of source bytes processed
		size_t dst_wrote;   ///< Number of destination bytes written
		bool have_invalid;  ///< Whether invalid characters were encountered
	} out;
} encode_convert_ctx;

/** Main encoding conversion function */
extern EFI_STATUS encode_convert(encode_convert_ctx* ctx);

/** Convert UTF-8 string to UTF-16 (allocates memory) */
extern CHAR16* encode_utf8_to_utf16(const char *utf8);

/** Convert UTF-16 string to UTF-8 (allocates memory) */
extern char* encode_utf16_to_utf8(const CHAR16 *utf16);
#endif
