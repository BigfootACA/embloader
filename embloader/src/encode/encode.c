#include "internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Process character transfers during encoding conversion.
 * This function checks if any of the configured character transfer rules
 * match at the current position and applies the replacement if found.
 *
 * @param ctx encoding conversion context containing transfer rules
 * @param s pointer to current source position (updated on match)
 * @param d pointer to current destination position (updated on match)
 * @return EFI_SUCCESS if transfer was applied, EFI_NOT_FOUND if no match,
 *         EFI_OUT_OF_RESOURCES if buffer space insufficient
 */
EFI_STATUS encode_proc_transfer(encode_convert_ctx* ctx, size_t* s, size_t* d) {
	const convert_transfer* tr;
	uint8_t* dst = (uint8_t*) ctx->in.dst_ptr;
	const uint8_t* src = (const uint8_t*) ctx->in.src_ptr;
	for (int t = 0; (tr = ctx->in.transfers[t]); t++) if (
		tr->match_size > 0 &&
		tr->replace_size > 0 &&
		*s + tr->match_size <= ctx->in.src_size &&
		memcmp(src + *s, tr->match, tr->match_size) == 0
	) {
		if (!tr->replace || tr->replace_size == 0)
			return EFI_OUT_OF_RESOURCES;
		if (*d + tr->replace_size > ctx->in.dst_size)
			return EFI_OUT_OF_RESOURCES;
		memcpy(dst + *d, tr->replace, tr->replace_size);
		*s += tr->match_size, *d += tr->replace_size;
		return EFI_SUCCESS;
	}
	return EFI_NOT_FOUND;
}

/**
 * @brief Copy and convert data with optional character transfers.
 * This function performs byte-by-byte copying while applying any configured
 * character transfer rules. If no transfers are configured, performs direct copy.
 *
 * @param ctx encoding conversion context with source and destination buffers
 * @return EFI_SUCCESS on success, error status on failure
 */
EFI_STATUS encode_copy_convert(encode_convert_ctx* ctx) {
	size_t i = 0, j = 0;
	EFI_STATUS st, ret = EFI_SUCCESS;
	if (ctx->in.transfers) {
		uint8_t* dst = (uint8_t*) ctx->in.dst_ptr;
		const uint8_t* src = (const uint8_t*) ctx->in.src_ptr;
		while (
			i < ctx->in.src_size &&
			j < ctx->in.dst_size &&
			!EFI_ERROR(ret)
		) {
			st = encode_proc_transfer(ctx, &i, &j);
			if (st == EFI_OUT_OF_RESOURCES) break;
			else if (st == EFI_NOT_FOUND)
				dst[j++] = src[i++];
			else if (st == EFI_SUCCESS)
				continue;
			else ret = st;
		}
	} else {
		size_t size = MIN(ctx->in.src_size, ctx->in.dst_size);
		memcpy(ctx->in.dst_ptr, ctx->in.src_ptr, size);
		i = size, j = size;
	}
	ctx->out.src_used = i;
	ctx->out.dst_wrote = j;
	ctx->out.src_end = (void*) ctx->in.src_ptr + i;
	ctx->out.dst_end = (void*) ctx->in.dst_ptr + j;
	return ret;
}

/**
 * @brief Convert between encodings using UTF-8 as intermediate format.
 * This function performs two-stage conversion: source -> UTF-8 -> destination.
 * Used when there's no direct converter between source and destination encodings.
 *
 * @param ctx encoding conversion context (transfers not supported)
 * @return EFI_SUCCESS on success, EFI_UNSUPPORTED if transfers are configured,
 *         EFI_OUT_OF_RESOURCES if memory allocation fails, other errors from conversion
 */
EFI_STATUS encode_center_convert(encode_convert_ctx* ctx) {
	if (ctx->in.transfers) return EFI_UNSUPPORTED;
	size_t tmp_buf_size = ctx->in.src_size * 4;
	uint8_t* tmp_buf = (uint8_t*) malloc(tmp_buf_size);
	if (!tmp_buf) return EFI_OUT_OF_RESOURCES;
	encode_convert_ctx src_ctx = *ctx;
	src_ctx.in.dst = ENC_UTF8;
	src_ctx.in.dst_ptr = tmp_buf;
	src_ctx.in.dst_size = tmp_buf_size;
	memset(&src_ctx.out, 0, sizeof(src_ctx.out));
	EFI_STATUS ret = encode_convert(&src_ctx);
	if (ret != EFI_SUCCESS && ret != EFI_WARN_UNKNOWN_GLYPH) {
		free(tmp_buf);
		return ret;
	}
	encode_convert_ctx dst_ctx = *ctx;
	dst_ctx.in.src = ENC_UTF8;
	dst_ctx.in.src_ptr = tmp_buf;
	dst_ctx.in.src_size = src_ctx.out.dst_wrote;
	memset(&dst_ctx.out, 0, sizeof(dst_ctx.out));
	ret = encode_convert(&dst_ctx);
	ctx->out = dst_ctx.out;
	ctx->out.src_used = src_ctx.out.src_used;
	ctx->out.have_invalid =
		src_ctx.out.have_invalid ||
		dst_ctx.out.have_invalid;
	free(tmp_buf);
	return ret;
}

/**
 * @brief Main encoding conversion function.
 * This function dispatches to appropriate conversion routines based on source
 * and destination encodings. Supports direct conversion, copy conversion, and
 * two-stage conversion via UTF-8.
 *
 * @param ctx encoding conversion context containing source/destination info
 * @return EFI_SUCCESS on successful conversion, EFI_INVALID_PARAMETER for invalid
 *         parameters, EFI_UNSUPPORTED for unsupported encoding combinations
 */
EFI_STATUS encode_convert(encode_convert_ctx* ctx) {
	if (!ctx || !ctx->in.src_ptr || !ctx->in.dst_ptr)
		return EFI_INVALID_PARAMETER;
	if (ctx->in.src == ENC_NONE || ctx->in.dst == ENC_NONE)
		return EFI_INVALID_PARAMETER;
	memset(&ctx->out, 0, sizeof(ctx->out));
	if (ctx->in.src == ctx->in.dst) return encode_copy_convert(ctx);
	for (int i = 0; encode_converts[i].src != ENC_NONE; i++) if (
		ctx->in.src == encode_converts[i].src &&
		ctx->in.dst == encode_converts[i].dst
	) return encode_converts[i].convert(ctx);
	if (ctx->in.src != ENC_UTF8 && ctx->in.dst != ENC_UTF8)
		return encode_center_convert(ctx);
	return EFI_UNSUPPORTED;
}

