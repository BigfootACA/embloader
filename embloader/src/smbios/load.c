#include <stdio.h>
#include <string.h>
#include "smbios.h"
#include "debugs.h"
#include "embloader.h"

bool embloader_load_smbios() {
	embloader_smbios smbios, *ctx = &smbios;
	confignode *root = NULL;
	memset(ctx, 0, sizeof(*ctx));
	embloader_init_info_smbios(ctx);
	if (!ctx->smbios2 && !ctx->smbios3) return false;
	if (!(root = confignode_new_map())) return false;
	confignode_path_set_int(root, "major", ctx->major);
	confignode_path_set_int(root, "minor", ctx->minor);
	confignode_path_set_string_fmt(root, "version", "%u.%u", ctx->major, ctx->minor);
	if (ctx->config_error) goto fail;
	for (size_t i = 0; smbios_table_loads[i].load != NULL; i++) {
		if (!embloader_smbios_load_table(
			ctx,
			root,
			smbios_table_loads[i].type,
			smbios_table_loads[i].name,
			smbios_table_loads[i].load
		)) goto fail;
	}
	if (!confignode_map_set(g_embloader.sysinfo, "smbios", root))
		goto fail;
	return true;
fail:
	if (root) confignode_clean(root);
	return false;
}
