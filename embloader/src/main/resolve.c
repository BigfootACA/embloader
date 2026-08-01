#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "embloader.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

static list* ktype_getter() {
	list *ret = NULL;
	if (g_embloader.ktype)
		list_obj_add_new_strdup(&ret, g_embloader.ktype);
	return ret;
}

static list* profile_getter() {
	list *ret = NULL;
	list *p;
	if ((p = list_first(g_embloader.profiles))) do {
		LIST_DATA_DECLARE(profile, p, char*);
		if (!profile) continue;
		list_obj_add_new_strdup(&ret, profile);
	} while ((p = p->next));
	return ret;
}

static struct variable_getter {
	const char *name;
	list* (*getter)();
} variable_getters[] = {
	{"ktype", ktype_getter},
	{"profile", profile_getter},
	{"dtb-id", embloader_dt_get_dtb_id},
	{NULL, NULL}
};

static char* replace_variable(
	const char *str,
	const char *var,
	const char *value,
	bool *replaced
) {
	if (!str || !var || !value) return NULL;
	if (replaced) *replaced = false;
	size_t var_len = strlen(var);
	if (var_len > SIZE_MAX - 3) return NULL;
	size_t pattern_len = var_len + 2;
	char *pattern = malloc(pattern_len + 1);
	if (!pattern) return NULL;
	pattern[0] = '{';
	memcpy(pattern + 1, var, var_len);
	pattern[pattern_len - 1] = '}';
	pattern[pattern_len] = 0;
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

static bool resolve_path_recursive(list **result, const char *path, int var_index) {
	if (!variable_getters[var_index].name) {
		return list_obj_add_new_strdup(result, path) == 0;
	}
	const char *var_name = variable_getters[var_index].name;
	list *values = variable_getters[var_index].getter();
	if (!values) {
		return resolve_path_recursive(result, path, var_index + 1);
	}
	bool handled = false, success = true;
	list *v;
	if ((v = list_first(values))) do {
		LIST_DATA_DECLARE(value, v, char*);
		if (!value) continue;
		bool replaced = false;
		char *new_path = replace_variable(path, var_name, value, &replaced);
		if (!new_path) {
			success = false;
			break;
		}
		handled = true;
		success = resolve_path_recursive(result, new_path, var_index + 1);
		free(new_path);
		if (!success || !replaced) break;
	} while ((v = v->next));
	list_free_all_def(values);
	if (success && !handled)
		success = resolve_path_recursive(result, path, var_index + 1);
	return success;
}

list* embloader_resolve_path(const char *path) {
	if (!path) return NULL;
	list *result = NULL;
	if (!resolve_path_recursive(&result, path, 0)) {
		list_free_all_def(result);
		return NULL;
	}
	return result;
}
