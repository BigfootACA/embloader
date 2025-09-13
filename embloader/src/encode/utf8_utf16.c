#include "internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Convert UTF-8 encoded data to UTF-16.
 * This function converts UTF-8 byte sequences to UTF-16 characters, handling
 * 1-3 byte UTF-8 sequences. Supports character transfers and invalid character
 * handling.
 *
 * @param ctx encoding conversion context with UTF-8 source and UTF-16 destination
 * @return EFI_SUCCESS on success, EFI_INVALID_PARAMETER for invalid context,
 *         EFI_OUT_OF_RESOURCES if destination buffer too small,
 *         EFI_WARN_UNKNOWN_GLYPH if invalid sequences found
 */
EFI_STATUS encode_utf8_to_utf16_convert(encode_convert_ctx* ctx) {
	EFI_STATUS st, ret = EFI_SUCCESS;
	if (!ctx || !ctx->in.src_ptr || !ctx->in.dst_ptr)
		return EFI_INVALID_PARAMETER;
	const uint8_t* src = (const uint8_t*) ctx->in.src_ptr;
	size_t src_size = ctx->in.src_size;
	CHAR16* dst = (CHAR16*) ctx->in.dst_ptr;
	size_t dst_size = ctx->in.dst_size / sizeof(CHAR16);
	size_t i = 0, j = 0;
	while (i < src_size && j + 1 < dst_size && src[i]) {
		if (ctx->in.transfers) {
			size_t si = i, dj = j * sizeof(CHAR16);
			st = encode_proc_transfer(ctx, &si, &dj);
			if (st == EFI_OUT_OF_RESOURCES) {
				ret = EFI_OUT_OF_RESOURCES;
				break;
			} else if (st == EFI_SUCCESS) {
				i = si, j = dj / sizeof(CHAR16);
				continue;
			}
		}
		uint8_t c = src[i];
		if (c < 0x80) {
			dst[j++] = c;
			i++;
		} else if ((c & 0xE0) == 0xC0 && i + 1 < src_size) {
			dst[j] = ((src[i] & 0x1F) << 6) | (src[i + 1] & 0x3F);
			j++, i += 2;
		} else if ((c & 0xF0) == 0xE0 && i + 2 < src_size) {
			dst[j] =
				((src[i] & 0x0F) << 12) |
				((src[i + 1] & 0x3F) << 6) |
				(src[i + 2] & 0x3F);
			j++, i += 3;
		} else if (ctx->in.allow_invalid) {
			ctx->out.have_invalid = true;
			dst[j++] = '?';
			i++;
		} else {
			ctx->out.have_invalid = true;
			ret = EFI_WARN_UNKNOWN_GLYPH;
			break;
		}
	}
	dst[j] = 0;
	ctx->out.src_used = i;
	ctx->out.dst_wrote = j * sizeof(CHAR16);
	ctx->out.src_end = (void*) (src + i);
	ctx->out.dst_end = (void*) (dst + j);
	return ret;
}

/**
 * @brief Convert UTF-16 encoded data to UTF-8.
 * This function converts UTF-16 characters to UTF-8 byte sequences, handling
 * characters that require 1-3 bytes in UTF-8. Supports character transfers and
 * invalid character handling including surrogate pairs.
 *
 * @param ctx encoding conversion context with UTF-16 source and UTF-8 destination
 * @return EFI_SUCCESS on success, EFI_OUT_OF_RESOURCES if destination buffer too small,
 *         EFI_WARN_UNKNOWN_GLYPH if invalid characters (like surrogates) found
 */
EFI_STATUS encode_utf16_to_utf8_convert(encode_convert_ctx* ctx) {
	EFI_STATUS st, ret = EFI_SUCCESS;
	const CHAR16* src = (const CHAR16*) ctx->in.src_ptr;
	size_t src_size = ctx->in.src_size / sizeof(CHAR16);
	uint8_t* dst = (uint8_t*) ctx->in.dst_ptr;
	size_t dst_size = ctx->in.dst_size;
	size_t i = 0, j = 0;
	while (i < src_size && j + 1 < dst_size && src[i]) {
		if (ctx->in.transfers) {
			size_t si = i * sizeof(CHAR16), dj = j;
			st = encode_proc_transfer(ctx, &si, &dj);
			if (st == EFI_OUT_OF_RESOURCES) {
				ret = EFI_OUT_OF_RESOURCES;
				break;
			} else if (st == EFI_SUCCESS) {
				i = si / sizeof(CHAR16), j = dj;
				continue;
			}
		}
		CHAR16 wc = src[i];
		if (wc < 0x80) {
			if (j + 1 > dst_size) break;
			dst[j++] = (uint8_t) wc;
		} else if (wc < 0x800) {
			if (j + 2 > dst_size) break;
			dst[j++] = 0xC0 | (wc >> 6);
			dst[j++] = 0x80 | (wc & 0x3F);
		} else if (wc >= 0xD800 && wc <= 0xDFFF) {
			ctx->out.have_invalid = true;
			if (ctx->in.allow_invalid) {
				dst[j++] = '?';
			} else {
				ret = EFI_WARN_UNKNOWN_GLYPH;
				break;
			}
		} else {
			if (j + 3 > dst_size) break;
			dst[j++] = 0xE0 | (wc >> 12);
			dst[j++] = 0x80 | ((wc >> 6) & 0x3F);
			dst[j++] = 0x80 | (wc & 0x3F);
		}
		i++;
	}
	if (j < dst_size) dst[j] = 0;
	ctx->out.src_used = i * sizeof(CHAR16);
	ctx->out.dst_wrote = j;
	ctx->out.src_end = (void*) (src + i);
	ctx->out.dst_end = (void*) (dst + j);
	return ret;
}
