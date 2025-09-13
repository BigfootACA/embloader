#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "internal.h"
#include "str-utils.h"
#include "log.h"

static bool conf_parse_value(const char* valstr, confignode_value* value) {
	if (!valstr || !value) return false;
	char* end = NULL;
	int64_t ival = strtoll(valstr, &end, 0);
	if (!end || *end == 0) {
		value->type = VALUE_INT;
		value->v.i = ival;
		value->len = sizeof(int64_t);
		return true;
	}
	double fval = strtod(valstr, &end);
	if (!end || *end == 0) {
		value->type = VALUE_FLOAT;
		value->v.f = fval;
		value->len = sizeof(double);
		return true;
	}
	if (string_is_true(valstr)) {
		value->type = VALUE_BOOL;
		value->v.b = true;
		value->len = sizeof(bool);
		return true;
	} else if (string_is_false(valstr)) {
		value->type = VALUE_BOOL;
		value->v.b = false;
		value->len = sizeof(bool);
		return true;
	}
	value->type = VALUE_STRING;
	value->v.s = strdup(valstr);
	if (!value->v.s) return false;
	value->len = strlen(valstr);
	return true;
}

static bool conf_parse_line(confignode* node, const char* line, size_t len) {
	if (!node || !line || len == 0) return false;
	while (len > 0 && isspace(line[len - 1])) len--;
	while (len > 0 && isspace(*line)) line++, len--;
	if (len == 0 || *line == '#') return true;
	const char* sep = memchr(line, '=', len);
	if (!sep) return false;
	const char *key = line, *val = sep + 1;
	size_t keylen = sep - line, vallen = len - keylen - 1;
	while (keylen > 0 && isspace(key[keylen - 1])) keylen--;
	while (vallen > 0 && isspace(*val)) val++, vallen--;
	if (!key || !val || keylen == 0 || vallen == 0) return false;
	if (val[0] == '"' && vallen >= 2 && val[vallen - 1] == '"') {
		val++, vallen -= 2;
	}
	char* keystr = strndup(key, keylen);
	if (!keystr) return false;
	char* valstr = strndup(val, vallen);
	if (!valstr) {
		free(keystr);
		return false;
	}
	confignode_value value = {0};
	if (!conf_parse_value(valstr, &value)) {
		free(keystr);
		free(valstr);
		return false;
	}
	bool result = confignode_path_set_value(node, keystr, &value);
	free(keystr);
	free(valstr);
	return result;
}

/**
 * @brief Parse configuration from string buffer in conf format.
 * Supports key-value pairs with nested paths and array indices.
 * Example: "menu.timeout = 30" or "test.array[0].val = value"
 *
 * @param buff string buffer containing configuration data
 * @return newly created root config node, or NULL on error
 */
confignode* configfile_conf_load_string(const char* buff) {
	if (!buff) return NULL;
	confignode* root = confignode_new_map();
	if (!root) return NULL;
	int lineno = 0;
	const char* line_start = buff, * cur = buff;
	while (*cur) {
		lineno++;
		while (*cur && *cur != '\n' && *cur != '\r') cur++;
		size_t line_len = cur - line_start;
		if (line_len > 0 && !conf_parse_line(root, line_start, line_len)) {
			log_warning("failed to parse config line %d", lineno);
			log_debug("line %d: '%.*s'", lineno, (int)line_len, line_start);
		}
		while (*cur == '\n' || *cur == '\r') cur++;
		line_start = cur;
	}
	return root;
}

static bool conf_append_string(char** result, size_t* size, size_t* pos, const char* str) {
	if (!result || !size || !pos || !str) return false;
	size_t len = strlen(str);
	size_t needed = *pos + len + 1;
	if (needed > *size) {
		size_t new_size = *size * 2;
		if (new_size < needed) new_size = needed + 1024;
		char* new_buf = realloc(*result, new_size);
		if (!new_buf) return false;
		*result = new_buf;
		*size = new_size;
	}
	memcpy(*result + *pos, str, len);
	*pos += len;
	(*result)[*pos] = 0;
	return true;
}

static bool conf_save_node_recursive(confignode* node, const char* prefix, char** result, size_t* size, size_t* pos) {
	if (!node) return true;
	confignode_type type = confignode_get_type(node);
	if (type == CONFIGNODE_TYPE_VALUE) {
		confignode_value value;
		if (!confignode_value_get(node, &value)) return false;
		if (!conf_append_string(result, size, pos, prefix)) return false;
		if (!conf_append_string(result, size, pos, " = ")) return false;
		char temp[64];
		switch (value.type) {
			case VALUE_STRING:
				if (!conf_append_string(result, size, pos, "\"")) return false;
				if (!conf_append_string(result, size, pos, value.v.s)) return false;
				if (!conf_append_string(result, size, pos, "\"")) return false;
				break;
			case VALUE_INT:
				snprintf(temp, sizeof(temp), "%lld", (long long)value.v.i);
				if (!conf_append_string(result, size, pos, temp)) return false;
				break;
			case VALUE_FLOAT:
				snprintf(temp, sizeof(temp), "%.6g", value.v.f);
				if (!conf_append_string(result, size, pos, temp)) return false;
				break;
			case VALUE_BOOL:
				if (!conf_append_string(result, size, pos, value.v.b ? "true" : "false")) return false;
				break;
		}
		if (!conf_append_string(result, size, pos, "\n")) return false;
		if (value.type == VALUE_STRING) free(value.v.s);
		return true;
	}
	if (type == CONFIGNODE_TYPE_MAP) {
		list* p;
		if ((p = list_first(node->items))) do {
			LIST_DATA_DECLARE(child, p, confignode*);
			if (!child || !child->key) continue;
			char* new_prefix;
			if (prefix && prefix[0]) {
				size_t prefix_len = strlen(prefix);
				size_t key_len = strlen(child->key);
				new_prefix = malloc(prefix_len + key_len + 2);
				if (!new_prefix) return false;
				strcpy(new_prefix, prefix);
				strcat(new_prefix, ".");
				strcat(new_prefix, child->key);
			} else {
				new_prefix = strdup(child->key);
				if (!new_prefix) return false;
			}
			bool result_ok = conf_save_node_recursive(child, new_prefix, result, size, pos);
			free(new_prefix);
			if (!result_ok) return false;
		} while ((p = p->next));
		return true;
	}
	if (type == CONFIGNODE_TYPE_ARRAY) {
		list* p;
		if ((p = list_first(node->items))) do {
			LIST_DATA_DECLARE(child, p, confignode*);
			if (!child) continue;
			char* new_prefix;
			size_t prefix_len = prefix ? strlen(prefix) : 0;
			new_prefix = malloc(prefix_len + 20);
			if (!new_prefix) return false;
			if (!prefix || !prefix[0]) sprintf(new_prefix, "[%lld]", (long long)child->index);
			else sprintf(new_prefix, "%s[%lld]", prefix, (long long)child->index);
			bool result_ok = conf_save_node_recursive(child, new_prefix, result, size, pos);
			free(new_prefix);
			if (!result_ok) return false;
		} while ((p = p->next));
		return true;
	}
	return true;
}

/**
 * @brief Save configuration node tree to string in conf format.
 * Converts the config tree back to key=value format with nested paths.
 *
 * @param node root config node to save
 * @return newly allocated string containing configuration, or NULL on error
 */
char* configfile_conf_save_string(confignode* node) {
	if (!node) return NULL;
	size_t size = 1024, pos = 0;
	char* result = malloc(size);
	if (!result) return NULL;
	result[0] = 0;
	if (!conf_save_node_recursive(node, "", &result, &size, &pos)) {
		free(result);
		return NULL;
	}
	return result;
}
