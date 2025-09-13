#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "embloader.h"
#include <string.h>
#include <stdlib.h>
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

static char* replace_variable(const char *str, const char *var, const char *value) {
	if (!str || !var || !value) return NULL;
	char *var_pattern = NULL;
	int ret = asprintf(&var_pattern, "{%s}", var);
	if (ret == -1 || !var_pattern) return NULL;
	char *pos = strstr(str, var_pattern);
	char *pattern = var_pattern;
	if (!pos) {
		free(var_pattern);
		return strdup(str);
	}
	size_t prefix_len = pos - str;
	size_t suffix_len = strlen(pos + strlen(pattern));
	size_t new_len = prefix_len + strlen(value) + suffix_len + 1;
	char *result = malloc(new_len);
	strncpy(result, str, prefix_len);
	result[prefix_len] = 0;
	strcat(result, value);
	strcat(result, pos + strlen(pattern));
	free(var_pattern);
	return result;
}

static void resolve_path_recursive(list **result, const char *path, int var_index) {
	if (!variable_getters[var_index].name) {
		list_obj_add_new_strdup(result, path);
		return;
	}
	const char *var_name = variable_getters[var_index].name;
	list *values = variable_getters[var_index].getter();
	if (!values) {
		resolve_path_recursive(result, path, var_index + 1);
		return;
	}
	list *v;
	if ((v = list_first(values))) do {
		LIST_DATA_DECLARE(value, v, char*);
		if (!value) continue;
		char *new_path = replace_variable(path, var_name, value);
		if (new_path) {
			resolve_path_recursive(result, new_path, var_index + 1);
			free(new_path);
		}
	} while ((v = v->next));
	list_free_all_def(values);
}

list* embloader_resolve_path(const char *path) {
	if (!path) return NULL;
	list *result = NULL;
	resolve_path_recursive(&result, path, 0);
	return result;
}
