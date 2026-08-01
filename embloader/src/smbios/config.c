#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#define EMBLOADER_SMBIOS_CONFIG_IMPL
#include "smbios.h"

static bool embloader_smbios_config_result(
	embloader_smbios *ctx,
	bool result
) {
	if (!result && ctx) ctx->config_error = true;
	return result;
}

bool embloader_smbios_config_set_int(
	embloader_smbios *ctx,
	confignode *node,
	const char *path,
	int64_t value
) {
	if (!ctx || ctx->config_error) return false;
	return embloader_smbios_config_result(
		ctx, confignode_path_set_int(node, path, value)
	);
}

bool embloader_smbios_config_set_string(
	embloader_smbios *ctx,
	confignode *node,
	const char *path,
	const char *value
) {
	if (!ctx || ctx->config_error) return false;
	return embloader_smbios_config_result(
		ctx, confignode_path_set_string(node, path, value)
	);
}

bool embloader_smbios_config_set_string_fmt(
	embloader_smbios *ctx,
	confignode *node,
	const char *path,
	const char *format,
	...
) {
	bool result;
	char *value = NULL;
	va_list args;
	if (!ctx || ctx->config_error || !format) return false;
	va_start(args, format);
	result = vasprintf(&value, format, args) >= 0 && value;
	va_end(args);
	if (result)
		result = confignode_path_set_string(node, path, value);
	free(value);
	return embloader_smbios_config_result(ctx, result);
}

bool embloader_smbios_config_set_bool(
	embloader_smbios *ctx,
	confignode *node,
	const char *path,
	bool value
) {
	if (!ctx || ctx->config_error) return false;
	return embloader_smbios_config_result(
		ctx, confignode_path_set_bool(node, path, value)
	);
}

bool embloader_smbios_config_attach(
	embloader_smbios *ctx,
	confignode *node,
	const char *path,
	confignode *sub
) {
	bool result = false;
	if (ctx && !ctx->config_error && sub)
		result = confignode_path_map_set(node, path, sub);
	if (!result) {
		if (sub) confignode_clean(sub);
		if (ctx) ctx->config_error = true;
	}
	return result;
}

bool embloader_smbios_config_append(
	embloader_smbios *ctx,
	confignode *array,
	confignode *sub
) {
	bool result = false;
	if (ctx && !ctx->config_error && sub)
		result = confignode_array_append(array, sub);
	if (!result) {
		if (sub) confignode_clean(sub);
		if (ctx) ctx->config_error = true;
	}
	return result;
}
