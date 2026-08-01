#include "../internal.h"
#include "embloader.h"
#include "efi-utils.h"
#include "file-utils.h"
#include "encode.h"
#include <unistd.h>

struct log_file_ctx {
	EFI_FILE_PROTOCOL *file;
	bool truncate;
	encoding encode;
};

static int log_file_write_all(
	EFI_FILE_PROTOCOL *file,
	const void *buffer,
	UINTN size
) {
	EFI_STATUS status;
	UINTN written = size;
	if (!file || (!buffer && size > 0)) return -1;
	if (size == 0) return 0;
	status = file->Write(file, &written, (void*) buffer);
	if (EFI_ERROR(status) || written != size) return -1;
	return 0;
}

static int log_file_init(log_backend *backend) {
	EFI_STATUS status;
	struct log_file_ctx *ctx;
	EFI_FILE_PROTOCOL *file = NULL;
	char *path = NULL, *encode = NULL;
	if (!backend || !(ctx = backend->ctx)) return -1;
	if (!g_embloader.dir.dir) {
		log_warning("no directory handle for log file");
		return -1;
	}
	if (!(path = confignode_path_get_string(backend->config, "path", NULL, NULL))) {
		log_warning("no path configured for log file backend");
		goto fail;
	}
	ctx->encode = ENC_UTF8;
	if ((encode = confignode_path_get_string(backend->config, "encode", NULL, NULL))){
		if(!encode_lookup(encode, &ctx->encode)) {
			log_warning("unknown encoding %s for log file backend", encode);
			free(encode);
			goto fail;
		}
		free(encode);
	}
	ctx->truncate = confignode_path_get_bool(backend->config, "truncate", false, NULL);
	status = efi_open(
		g_embloader.dir.dir, &file, path,
		EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
		0
	);
	if (EFI_ERROR(status)) {
		log_warning(
			"failed to open log file %s: %s",
			path, efi_status_to_string(status)
		);
		goto fail;
	}
	if (ctx->truncate) {
		status = efi_file_set_size(file, 0);
		if (!EFI_ERROR(status)) status = file->SetPosition(file, 0);
	} else status = file->SetPosition(file, MAX_UINT64);
	if (EFI_ERROR(status)) {
		log_warning(
			"failed to prepare log file %s: %s",
			path, efi_status_to_string(status)
		);
		goto fail;
	}
	free(path);
	ctx->file = file;
	return 0;
fail:
	if (file) file->Close(file);
	if (path) free(path);
	return -1;
}

static int log_file_deinit(log_backend *backend) {
	struct log_file_ctx *ctx;
	EFI_STATUS status = EFI_SUCCESS;
	if (!backend || !(ctx = backend->ctx)) return -1;
	if (ctx->file) status = ctx->file->Close(ctx->file);
	ctx->file = NULL;
	return EFI_ERROR(status) ? -1 : 0;
}

static int log_file_writer(log_backend *backend, log_item *item) {
	int ret = -1;
	char *format, *formatted;
	struct log_file_ctx *ctx;
	EFI_STATUS st;
	UINTN len, wlen;
	if (!backend || !item || !(ctx = backend->ctx)) return -1;
	if (!log_check_filter(item, backend->config)) return 0;
	format = confignode_path_get_string(backend->config, "format", NULL, NULL);
	formatted = log_formatter(item, format, 1);
	if (formatted && ctx->file) {
		len = strlen(formatted);
		if (ctx->encode != ENC_NONE && ctx->encode != ENC_UTF8) {
			char buff[256];
			encode_convert_ctx enc = {
				.in = {
					.src = ENC_UTF8,
					.dst = ctx->encode,
					.dst_ptr = buff,
					.dst_size = sizeof(buff),
					.src_ptr = formatted,
					.src_size = len,
				},
			};
			while (enc.in.src_size > 0) {
				st = encode_convert(&enc);
				if (
					enc.out.src_used == 0 || enc.out.dst_wrote == 0 ||
					enc.out.src_used > enc.in.src_size ||
					enc.out.dst_wrote > sizeof(buff)
				) goto done;
				if (
					(st == EFI_SUCCESS &&
					 enc.out.src_used != enc.in.src_size) ||
					(st == EFI_OUT_OF_RESOURCES &&
					 enc.out.src_used == enc.in.src_size) ||
					(st != EFI_SUCCESS && st != EFI_OUT_OF_RESOURCES)
				) goto done;
				wlen = enc.out.dst_wrote;
				if (log_file_write_all(ctx->file, buff, wlen) < 0)
					goto done;
				enc.in.src_ptr =
					(const UINT8*) enc.in.src_ptr + enc.out.src_used;
				enc.in.src_size -= enc.out.src_used;
			}
		} else if (log_file_write_all(ctx->file, formatted, len) < 0)
			goto done;
		st = ctx->file->Flush(ctx->file);
		if (EFI_ERROR(st)) goto done;
		ret = 0;
	}
done:
	if (formatted) free(formatted);
	if (format) free(format);
	return ret;
}

log_backend_base log_backend_file = {
	.name = "file",
	.init = log_file_init,
	.deinit = log_file_deinit,
	.write = log_file_writer,
	.ctx_size = sizeof(struct log_file_ctx),
};
