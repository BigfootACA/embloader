#include "embloader.h"
#include "log.h"

static bool embloader_match_device(confignode *node) {
	if (!node) return false;
	bool ok = false;
	char *name = confignode_path_get_string(node, "name", NULL, &ok);
	list *profiles_new = NULL;
	if (!ok || !name) goto fail;
	confignode *match = confignode_path_lookup(node, "match", false);
	if (match && !embloader_try_matches(match)) goto fail;
	confignode *profiles = confignode_path_lookup(node, "profiles", false);
	confignode_foreach(profile, profiles) {
		if (!confignode_is_type(profile.node, CONFIGNODE_TYPE_VALUE)) continue;
		char *v = confignode_value_get_string(profile.node, NULL, &ok);
		if (!ok || !v) {
			free(v);
			goto fail;
		}
		if (list_obj_add_new(&profiles_new, v) != 0) {
			free(v);
			goto fail;
		}
		log_debug("Pick profile %s", v);
	}
	char *name_old = g_embloader.device_name;
	list *profiles_old = g_embloader.profiles;
	g_embloader.device_name = name;
	g_embloader.profiles = profiles_new;
	if (name_old) free(name_old);
	if (profiles_old) list_free_all_def(profiles_old);
	log_info("Found matched device: %s", name);
	return true;
fail:
	free(name);
	if (profiles_new) list_free_all_def(profiles_new);
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
