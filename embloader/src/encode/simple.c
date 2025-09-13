#include <Library/BaseLib.h>
#include "encode.h"
#include <string.h>
#include <stdlib.h>

/**
 * @brief Simple UTF-8 to UTF-16 string conversion.
 * This is a convenience function that allocates memory and converts a null-terminated
 * UTF-8 string to a null-terminated UTF-16 string. Invalid characters are replaced
 * with '?'.
 *
 * @param utf8 null-terminated UTF-8 input string
 * @return newly allocated UTF-16 string (caller must free), or NULL on error
 */
CHAR16* encode_utf8_to_utf16(const char *utf8) {
	if (!utf8) return NULL;
	encode_convert_ctx ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.in.src = ENC_UTF8;
	ctx.in.dst = ENC_UTF16;
	ctx.in.src_ptr = utf8;
	ctx.in.src_size = strlen(utf8);
	ctx.in.dst_size = ctx.in.src_size * 2 + 2;
	ctx.in.allow_invalid = true;
	ctx.in.dst_ptr = malloc(ctx.in.dst_size);
	if (!ctx.in.dst_ptr) return NULL;
	memset(ctx.in.dst_ptr, 0, ctx.in.dst_size);
	if (encode_convert(&ctx) != EFI_SUCCESS){
		free(ctx.in.dst_ptr);
		return NULL;
	}
	return ctx.in.dst_ptr;
}

/**
 * @brief Simple UTF-16 to UTF-8 string conversion.
 * This is a convenience function that allocates memory and converts a null-terminated
 * UTF-16 string to a null-terminated UTF-8 string. Invalid characters are replaced
 * with '?'.
 *
 * @param utf16 null-terminated UTF-16 input string
 * @return newly allocated UTF-8 string (caller must free), or NULL on error
 */
char* encode_utf16_to_utf8(const CHAR16 *utf16) {
	if (!utf16) return NULL;
	encode_convert_ctx ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.in.src = ENC_UTF16;
	ctx.in.dst = ENC_UTF8;
	ctx.in.src_ptr = utf16;
	ctx.in.src_size = 0;
	ctx.in.src_size = StrSize(utf16);
	ctx.in.dst_size = ctx.in.src_size;
	ctx.in.allow_invalid = true;
	ctx.in.dst_ptr = malloc(ctx.in.dst_size);
	if (!ctx.in.dst_ptr) return NULL;
	memset(ctx.in.dst_ptr, 0, ctx.in.dst_size);
	if (encode_convert(&ctx) != EFI_SUCCESS){
		free(ctx.in.dst_ptr);
		return NULL;
	}
	return ctx.in.dst_ptr;
}
