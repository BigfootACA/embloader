#include <Library/BaseLib.h>
#include "file-utils.h"
#include "embloader.h"
#include "log.h"

bool embloader_load_config_one(const char *name) {
	if (!g_embloader.dir.dir) return false;
	if (!efi_file_exists(g_embloader.dir.dir, name)) return false;
	configfile_type type;
	type = configfile_type_guess(name);
	if (type == CONFIGFILE_TYPE_UNKNOWN) {
		log_warning("unknown config file type for %s", name);
		return false;
	}
	confignode *newcfg = configfile_load_efi_file_path(type, g_embloader.dir.dir, name);
	if (!newcfg) {
		log_warning("Failed to parse config file %s", name);
		return false;
	}
	if (!confignode_merge(g_embloader.config, newcfg))
		log_warning("Failed to merge config file %s", name);
	confignode_clean(newcfg);
	log_info("Loaded config file %s", name);
	return true;
}

bool embloader_load_configs() {
	int cnt = 0;
	if (embloader_load_config_one("config.static.yaml")) cnt++;
	if (embloader_load_config_one("config.static.yml")) cnt++;
	if (embloader_load_config_one("config.static.json")) cnt++;
	if (embloader_load_config_one("config.static.conf")) cnt++;
	if (!EFI_ERROR(embloader_load_dtbo_config_path(g_embloader.config, g_embloader.dir.dir, "dtbo.txt"))) cnt++;
	if (!EFI_ERROR(embloader_load_cmdline_config_path(g_embloader.config, g_embloader.dir.dir, "cmdline.txt"))) cnt++;
	if (embloader_load_config_one("config.dynamic.yaml")) cnt++;
	if (embloader_load_config_one("config.dynamic.yml")) cnt++;
	if (embloader_load_config_one("config.dynamic.json")) cnt++;
	if (embloader_load_config_one("config.dynamic.conf")) cnt++;
	if (cnt > 0) log_info("Loaded %d configuration files", cnt);
	else log_warning("No configuration files loaded");
	return cnt > 0;
}

bool embloader_init() {
	memset(&g_embloader, 0, sizeof(g_embloader));
	g_embloader.sysinfo = confignode_new_map();
	if (!g_embloader.sysinfo) return false;
	g_embloader.config = confignode_new_map();
	if (!g_embloader.config) return false;
	return true;
}

void embloader_load_ktype() {
	list *p;
	if (g_embloader.ktype) free(g_embloader.ktype);
	confignode *np = confignode_map_get(g_embloader.config, "profiles");
	if (!np) return;
	if ((p = list_first(g_embloader.profiles))) do {
		LIST_DATA_DECLARE(profile, p, char*);
		if (!profile) continue;
		confignode *cp = confignode_map_get(np, profile);
		if (!cp) continue;
		char *ktype = confignode_path_get_string(cp, "ktype", NULL, NULL);
		if (!ktype) continue;
		if (g_embloader.ktype) free(g_embloader.ktype);
		g_embloader.ktype = ktype;
		if (g_embloader.ktype) log_debug(
			"use ktype %s from profile %s",
			g_embloader.ktype, profile
		);
		return;
	} while ((p = p->next));
}
