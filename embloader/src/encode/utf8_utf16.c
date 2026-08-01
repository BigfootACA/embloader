#include "internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Convert UTF-8 encoded data to UTF-16.
 * This function converts UTF-8 byte sequences to UTF-16 characters, handling
 * 1-4 byte UTF-8 sequences. Supports character transfers and invalid character
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
	if (dst_size == 0) {
		ctx->out.src_end = (void*) src;
		ctx->out.dst_end = (void*) dst;
		return EFI_OUT_OF_RESOURCES;
	}
	size_t dst_capacity = dst_size - 1;
	while (i < src_size && src[i]) {
		if (ctx->in.transfers) {
			encode_convert_ctx transfer_ctx = *ctx;
			transfer_ctx.in.dst_size = dst_capacity * sizeof(CHAR16);
			size_t si = i, dj = j * sizeof(CHAR16);
			st = encode_proc_transfer(&transfer_ctx, &si, &dj);
			if (st == EFI_OUT_OF_RESOURCES) {
				ret = EFI_OUT_OF_RESOURCES;
				break;
			} else if (st == EFI_SUCCESS) {
				if (si <= i || dj % sizeof(CHAR16) != 0) {
					ret = EFI_INVALID_PARAMETER;
					break;
				}
				i = si, j = dj / sizeof(CHAR16);
				continue;
			} else if (st != EFI_NOT_FOUND) {
				ret = st;
				break;
			}
		}
		uint8_t c = src[i];
		uint32_t codepoint = 0;
		size_t consumed = 1;
		bool valid = true;
		if (c < 0x80) codepoint = c;
		else if (
			c >= 0xC2 && c <= 0xDF &&
			src_size - i >= 2 &&
			(src[i + 1] & 0xC0) == 0x80
		) {
			codepoint =
				((uint32_t) (c & 0x1F) << 6) |
				(uint32_t) (src[i + 1] & 0x3F);
			consumed = 2;
		} else if (
			c >= 0xE0 && c <= 0xEF &&
			src_size - i >= 3 &&
			(src[i + 1] & 0xC0) == 0x80 &&
			(src[i + 2] & 0xC0) == 0x80 &&
			(c != 0xE0 || src[i + 1] >= 0xA0) &&
			(c != 0xED || src[i + 1] <= 0x9F)
		) {
			codepoint =
				((uint32_t) (c & 0x0F) << 12) |
				((uint32_t) (src[i + 1] & 0x3F) << 6) |
				(uint32_t) (src[i + 2] & 0x3F);
			consumed = 3;
		} else if (
			c >= 0xF0 && c <= 0xF4 &&
			src_size - i >= 4 &&
			(src[i + 1] & 0xC0) == 0x80 &&
			(src[i + 2] & 0xC0) == 0x80 &&
			(src[i + 3] & 0xC0) == 0x80 &&
			(c != 0xF0 || src[i + 1] >= 0x90) &&
			(c != 0xF4 || src[i + 1] <= 0x8F)
		) {
			codepoint =
				((uint32_t) (c & 0x07) << 18) |
				((uint32_t) (src[i + 1] & 0x3F) << 12) |
				((uint32_t) (src[i + 2] & 0x3F) << 6) |
				(uint32_t) (src[i + 3] & 0x3F);
			consumed = 4;
		} else valid = false;
		if (!valid) {
			ctx->out.have_invalid = true;
			if (!ctx->in.allow_invalid) {
				ret = EFI_WARN_UNKNOWN_GLYPH;
				break;
			}
			codepoint = '?';
			consumed = 1;
		}
		size_t needed = codepoint > 0xFFFF ? 2 : 1;
		if (needed > dst_capacity - j) {
			ret = EFI_OUT_OF_RESOURCES;
			break;
		}
		if (needed == 1) dst[j++] = (CHAR16) codepoint;
		else {
			codepoint -= 0x10000;
			dst[j++] = (CHAR16) (0xD800 | (codepoint >> 10));
			dst[j++] = (CHAR16) (0xDC00 | (codepoint & 0x3FF));
		}
		i += consumed;
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
	if (!ctx || !ctx->in.src_ptr || !ctx->in.dst_ptr)
		return EFI_INVALID_PARAMETER;
	const CHAR16* src = (const CHAR16*) ctx->in.src_ptr;
	const uint8_t* src_bytes = (const uint8_t*) ctx->in.src_ptr;
	size_t src_size = ctx->in.src_size;
	uint8_t* dst = (uint8_t*) ctx->in.dst_ptr;
	size_t dst_size = ctx->in.dst_size;
	size_t i = 0, j = 0;
	if (dst_size == 0) {
		ctx->out.src_end = (void*) src;
		ctx->out.dst_end = (void*) dst;
		return EFI_OUT_OF_RESOURCES;
	}
	size_t dst_capacity = dst_size - 1;
	bool terminated = false;
	while (src_size - i >= sizeof(CHAR16)) {
		CHAR16 wc;
		memcpy(&wc, src_bytes + i, sizeof(wc));
		if (wc == 0) {
			terminated = true;
			break;
		}
		if (ctx->in.transfers) {
			encode_convert_ctx transfer_ctx = *ctx;
			transfer_ctx.in.dst_size = dst_capacity;
			size_t si = i, dj = j;
			st = encode_proc_transfer(&transfer_ctx, &si, &dj);
			if (st == EFI_OUT_OF_RESOURCES) {
				ret = EFI_OUT_OF_RESOURCES;
				break;
			} else if (st == EFI_SUCCESS) {
				if (si <= i || si % sizeof(CHAR16) != 0) {
					ret = EFI_INVALID_PARAMETER;
					break;
				}
				i = si, j = dj;
				continue;
			} else if (st != EFI_NOT_FOUND) {
				ret = st;
				break;
			}
		}
		uint32_t codepoint = wc;
		size_t consumed = sizeof(CHAR16);
		bool valid = true;
		if (wc >= 0xD800 && wc <= 0xDBFF) {
			CHAR16 low = 0;
			if (src_size - i >= 2 * sizeof(CHAR16))
				memcpy(&low, src_bytes + i + sizeof(CHAR16), sizeof(low));
			if (low >= 0xDC00 && low <= 0xDFFF) {
				codepoint = 0x10000 +
					(((uint32_t) wc - 0xD800) << 10) +
					((uint32_t) low - 0xDC00);
				consumed = 2 * sizeof(CHAR16);
			} else valid = false;
		} else if (wc >= 0xDC00 && wc <= 0xDFFF) valid = false;
		if (!valid) {
			ctx->out.have_invalid = true;
			if (!ctx->in.allow_invalid) {
				ret = EFI_WARN_UNKNOWN_GLYPH;
				break;
			}
			codepoint = '?';
			consumed = sizeof(CHAR16);
		}
		size_t needed = codepoint < 0x80 ? 1 :
			codepoint < 0x800 ? 2 : codepoint < 0x10000 ? 3 : 4;
		if (needed > dst_capacity - j) {
			ret = EFI_OUT_OF_RESOURCES;
			break;
		}
		if (needed == 1) dst[j++] = (uint8_t) codepoint;
		else if (needed == 2) {
			dst[j++] = (uint8_t) (0xC0 | (codepoint >> 6));
			dst[j++] = (uint8_t) (0x80 | (codepoint & 0x3F));
		} else if (needed == 3) {
			dst[j++] = (uint8_t) (0xE0 | (codepoint >> 12));
			dst[j++] = (uint8_t) (0x80 | ((codepoint >> 6) & 0x3F));
			dst[j++] = (uint8_t) (0x80 | (codepoint & 0x3F));
		} else {
			dst[j++] = (uint8_t) (0xF0 | (codepoint >> 18));
			dst[j++] = (uint8_t) (0x80 | ((codepoint >> 12) & 0x3F));
			dst[j++] = (uint8_t) (0x80 | ((codepoint >> 6) & 0x3F));
			dst[j++] = (uint8_t) (0x80 | (codepoint & 0x3F));
		}
		i += consumed;
	}
	if (!terminated && ret == EFI_SUCCESS && i < src_size) {
		ctx->out.have_invalid = true;
		if (!ctx->in.allow_invalid) ret = EFI_WARN_UNKNOWN_GLYPH;
		else if (j >= dst_capacity) ret = EFI_OUT_OF_RESOURCES;
		else dst[j++] = '?', i = src_size;
	}
	dst[j] = 0;
	ctx->out.src_used = i;
	ctx->out.dst_wrote = j;
	ctx->out.src_end = (void*) (src_bytes + i);
	ctx->out.dst_end = (void*) (dst + j);
	return ret;
}
