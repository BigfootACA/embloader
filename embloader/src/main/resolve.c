#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "embloader.h"
#include "bootmenu.h"
#include "sdboot.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

static bool ktype_getter(embloader_loader *loader, list **values) {
	if (!values) return false;
	*values = NULL;
	if (!g_embloader.ktype) return true;
	return list_obj_add_new_strdup(values, g_embloader.ktype) == 0;
}

static bool profile_getter(embloader_loader *loader, list **values) {
	if (!values) return false;
	*values = NULL;
	list *p;
	if ((p = list_first(g_embloader.profiles))) do {
		LIST_DATA_DECLARE(profile, p, char*);
		if (!profile) continue;
		if (list_obj_add_new_strdup(values, profile) != 0) {
			if (*values) list_free_all_def(*values);
			*values = NULL;
			return false;
		}
	} while ((p = p->next));
	return true;
}

static bool version_getter(embloader_loader *loader, list **values) {
	char *s;
	if (!loader || !values) return false;
	*values = NULL;
	if (loader->type == LOADER_SDBOOT && loader->extra) {
		sdboot_boot_loader *item = loader->extra;
		if (item->version) return list_obj_add_new_strdup(values, item->version) == 0;
	}
	if ((s = confignode_path_get_string(loader->node, "version", NULL, NULL))) {
		if (list_obj_add_new(values, s) == 0) return true;
		free(s);
		return false;
	}
	return true;
}

static struct variable_getter {
	const char *name;
	bool (*getter)(embloader_loader *loader, list **values);
	bool depends_on_loader;
} variable_getters[] = {
	{"version", version_getter, true},
	{"ktype", ktype_getter, false},
	{"profile", profile_getter, false},
	{"dtb-id", embloader_dt_get_dtb_id, false},
	{NULL, NULL, false}
};

static char* variable_pattern(const char *name) {
	if (!name) return NULL;
	size_t name_len = strlen(name);
	if (name_len > SIZE_MAX - 3) return NULL;
	size_t pattern_len = name_len + 2;
	char *pattern = malloc(pattern_len + 1);
	if (!pattern) return NULL;
	pattern[0] = '{';
	memcpy(pattern + 1, name, name_len);
	pattern[pattern_len - 1] = '}';
	pattern[pattern_len] = 0;
	return pattern;
}

static bool path_contains_variable(const char *path, const char *name) {
	if (!path || !name) return false;
	char *pattern = variable_pattern(name);
	if (!pattern) return false;
	bool found = strstr(path, pattern) != NULL;
	free(pattern);
	return found;
}

static bool path_depends_on_loader(const char *path) {
	int i;
	if (!path) return false;
	for (i = 0; variable_getters[i].name; i++) if (
		variable_getters[i].depends_on_loader &&
		path_contains_variable(path, variable_getters[i].name)
	) return true;
	return false;
}

static char* replace_variable(
	embloader_loader *loader,
	const char *str,
	const char *var,
	const char *value,
	bool *replaced
) {
	if (!str || !var || !value) return NULL;
	if (replaced) *replaced = false;
	char *pattern = variable_pattern(var);
	if (!pattern) return NULL;
	size_t pattern_len = strlen(pattern);
	const char *pos = strstr(str, pattern);
	if (!pos) {
		free(pattern);
		return strdup(str);
	}
	size_t count = 0;
	const char *scan = str;
	while ((pos = strstr(scan, pattern))) {
		count++;
		scan = pos + pattern_len;
	}
	size_t str_len = strlen(str), value_len = strlen(value), result_len = str_len;
	if (value_len >= pattern_len) {
		size_t growth = value_len - pattern_len;
		if (growth && count > (SIZE_MAX - result_len) / growth) {
			free(pattern);
			return NULL;
		}
		result_len += count * growth;
	} else {
		result_len -= count * (pattern_len - value_len);
	}
	if (result_len == SIZE_MAX) {
		free(pattern);
		return NULL;
	}
	char *result = malloc(result_len + 1);
	if (!result) {
		free(pattern);
		return NULL;
	}
	char *out = result;
	scan = str;
	while ((pos = strstr(scan, pattern))) {
		size_t prefix_len = pos - scan;
		memcpy(out, scan, prefix_len);
		out += prefix_len;
		memcpy(out, value, value_len);
		out += value_len;
		scan = pos + pattern_len;
	}
	size_t suffix_len = strlen(scan);
	memcpy(out, scan, suffix_len + 1);
	free(pattern);
	if (replaced) *replaced = true;
	return result;
}

static bool resolve_path_recursive(embloader_loader *loader, list **result, const char *path, int var_index) {
	if (!variable_getters[var_index].name) {
		return list_obj_add_new_strdup(result, path) == 0;
	}
	const char *var_name = variable_getters[var_index].name;
	list *values = NULL;
	if (!variable_getters[var_index].getter(loader, &values)) {
		if (values) list_free_all_def(values);
		return false;
	}
	if (!values) {
		return resolve_path_recursive(loader, result, path, var_index + 1);
	}
	bool handled = false, success = true;
	list *v;
	if ((v = list_first(values))) do {
		LIST_DATA_DECLARE(value, v, char*);
		if (!value) continue;
		bool replaced = false;
		char *new_path = replace_variable(loader, path, var_name, value, &replaced);
		if (!new_path) {
			success = false;
			break;
		}
		handled = true;
		success = resolve_path_recursive(loader, result, new_path, var_index + 1);
		free(new_path);
		if (!success || !replaced) break;
	} while ((v = v->next));
	list_free_all_def(values);
	if (success && !handled)
		success = resolve_path_recursive(loader, result, path, var_index + 1);
	return success;
}

list* embloader_resolve_path(embloader_loader *loader, const char *path, bool *depends_on_loader) {
	if (depends_on_loader) *depends_on_loader = false;
	if (!path) return NULL;
	if (depends_on_loader) *depends_on_loader = path_depends_on_loader(path);
	list *result = NULL;
	if (!resolve_path_recursive(loader, &result, path, 0)) {
		list_free_all_def(result);
		return NULL;
	}
	return result;
}
