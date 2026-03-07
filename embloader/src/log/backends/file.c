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
		efi_file_set_size(file, 0);
		file->SetPosition(file, 0);
	}
	free(path);
	ctx->file = file;
	return 0;
fail:
	if (path) free(path);
	return -1;
}

static int log_file_deinit(log_backend *backend) {
	struct log_file_ctx *ctx;
	if (!backend || !(ctx = backend->ctx)) return -1;
	if (ctx->file) ctx->file->Close(ctx->file);
	ctx->file = NULL;
	return 0;
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
				if (EFI_ERROR(st)) break;
				if (enc.out.dst_wrote == 0) break;
				wlen = enc.out.dst_wrote;
				ctx->file->Write(ctx->file, &wlen, buff);
				enc.in.src_ptr = enc.out.src_end;
				enc.in.src_size -= enc.out.src_used;
			}
		} else ctx->file->Write(ctx->file, &len, formatted);
		ctx->file->Flush(ctx->file);
		ret = 0;
	}
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
