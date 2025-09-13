#include "embloader.h"
#include "log.h"

static bool embloader_match_device(confignode *node) {
	if (!node) return false;
	char *name = confignode_path_get_string(node, "name", NULL, NULL);
	if (!name) return false;
	confignode *match = confignode_path_lookup(node, "match", false);
	if (match && !embloader_try_matches(match)) goto fail;
	log_info("Found matched device: %s", name);
	if (g_embloader.device_name)
		free(g_embloader.device_name);
	g_embloader.device_name = name;
	confignode *profiles = confignode_path_lookup(node, "profiles", false);
	confignode_foreach(profile, profiles) {
		char *v = confignode_value_get_string(profile.node, NULL, NULL);
		if (!v) continue;
		log_debug("Pick profile %s", v);
		list_obj_add_new(&g_embloader.profiles, v);
	}
	return true;
fail:
	if (name) free(name);
	return false;
}

bool embloader_choose_device() {
	confignode *devices = confignode_path_lookup(
		g_embloader.config, "devices", false
	);
	if (!devices) return false;
	confignode_foreach(dev, devices)
		if (embloader_match_device(dev.node)) return true;
	return false;
}
