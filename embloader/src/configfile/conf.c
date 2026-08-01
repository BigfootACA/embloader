#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "internal.h"
#include "str-utils.h"
#include "log.h"

static bool conf_parse_value(const char* valstr, confignode_value* value) {
	if (!valstr || !value) return false;
	char* end = NULL;
	const char* digits = valstr;
	if (*digits == '+' || *digits == '-') digits++;
	int int_base = digits[0] == '0' &&
		(digits[1] == 'x' || digits[1] == 'X') ? 16 : 10;
	errno = 0;
	int64_t ival = strtoll(valstr, &end, int_base);
	if (end != valstr && *end == 0) {
		if (errno == ERANGE) return false;
		value->type = VALUE_INT;
		value->v.i = ival;
		value->len = sizeof(int64_t);
		return true;
	}
	errno = 0;
	double fval = strtod(valstr, &end);
	if (end != valstr && *end == 0) {
		if (errno == ERANGE && (fval == 0.0 || !isfinite(fval)))
			return false;
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
	while (len > 0 && isspace((unsigned char)line[len - 1])) len--;
	while (len > 0 && isspace((unsigned char)*line)) line++, len--;
	if (len == 0 || *line == '#') return true;
	const char* sep = memchr(line, '=', len);
	if (!sep) return false;
	const char *key = line, *val = sep + 1;
	size_t keylen = sep - line, vallen = len - keylen - 1;
	while (keylen > 0 && isspace((unsigned char)key[keylen - 1])) keylen--;
	while (vallen > 0 && isspace((unsigned char)*val)) val++, vallen--;
	if (!key || !val || keylen == 0 || vallen == 0) return false;
	bool quoted = val[0] == '"';
	if (quoted) {
		if (vallen < 2 || val[vallen - 1] != '"') return false;
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
	if (quoted) {
		value.type = VALUE_STRING;
		value.v.s = strdup(valstr);
		value.len = vallen;
	}
	if ((quoted && !value.v.s) || (!quoted && !conf_parse_value(valstr, &value))) {
		free(keystr);
		free(valstr);
		return false;
	}
	bool result = confignode_path_set_value(node, keystr, &value);
	if (value.type == VALUE_STRING) free(value.v.s);
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
			int print_len = line_len > INT_MAX ? INT_MAX : (int) line_len;
			log_debug("line %d: '%.*s'", lineno, print_len, line_start);
			confignode_clean(root);
			return NULL;
		}
		while (*cur == '\n' || *cur == '\r') cur++;
		line_start = cur;
	}
	return root;
}

static bool conf_append_string(char** result, size_t* size, size_t* pos, const char* str) {
	if (!result || !size || !pos || !str) return false;
	size_t len = strlen(str);
	if (*pos == SIZE_MAX || len > SIZE_MAX - *pos - 1) return false;
	size_t needed = *pos + len + 1;
	if (needed > *size) {
		size_t new_size = *size ? *size : 1;
		while (new_size < needed) {
			if (new_size > SIZE_MAX / 2) {
				new_size = needed;
				break;
			}
			new_size *= 2;
		}
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

static bool conf_key_is_serializable(const char* key, bool root) {
	if (!key || !key[0] || strpbrk(key, ".=[\r\n")) return false;
	size_t len = strlen(key);
	if (
		isspace((unsigned char)key[0]) || isspace((unsigned char)key[len - 1]) ||
		(root && key[0] == '#')
	) return false;
	return true;
}

static bool conf_save_node_recursive(confignode* node, const char* prefix, char** result, size_t* size, size_t* pos) {
	if (!node || !prefix) return false;
	confignode_type type = confignode_get_type(node);
	if (type == CONFIGNODE_TYPE_VALUE) {
		confignode_value value;
		if (!confignode_value_get(node, &value)) return false;
		bool result_ok = conf_append_string(result, size, pos, prefix) &&
			conf_append_string(result, size, pos, " = ");
		char temp[64];
		if (result_ok) switch (value.type) {
			case VALUE_STRING:
				if (!value.v.s || strpbrk(value.v.s, "\r\n")) result_ok = false;
				else result_ok =
					conf_append_string(result, size, pos, "\"") &&
					conf_append_string(result, size, pos, value.v.s) &&
					conf_append_string(result, size, pos, "\"");
				break;
			case VALUE_INT:
				result_ok = snprintf(
					temp, sizeof(temp), "%lld", (long long) value.v.i
				) > 0 && conf_append_string(result, size, pos, temp);
				break;
			case VALUE_FLOAT: {
				int len = snprintf(
					temp, sizeof(temp), "%.*g", DBL_DECIMAL_DIG, value.v.f
				);
				result_ok = len > 0 && (size_t) len < sizeof(temp);
				if (
					result_ok && isfinite(value.v.f) &&
					!strpbrk(temp, ".eE")
				) {
					if ((size_t) len > sizeof(temp) - 3) result_ok = false;
					else memcpy(temp + len, ".0", 3);
				}
				if (result_ok)
					result_ok = conf_append_string(result, size, pos, temp);
				break;
			}
			case VALUE_BOOL:
				result_ok = conf_append_string(
					result, size, pos, value.v.b ? "true" : "false"
				);
				break;
			default:
				result_ok = false;
				break;
		}
		if (result_ok) result_ok = conf_append_string(result, size, pos, "\n");
		if (value.type == VALUE_STRING) free(value.v.s);
		return result_ok;
	}
	if (type == CONFIGNODE_TYPE_MAP) {
		if (!node->items) return !prefix[0];
		list* p;
		if ((p = list_first(node->items))) do {
			LIST_DATA_DECLARE(child, p, confignode*);
			if (!child || child->parent != node || !child->key) return false;
			if (!conf_key_is_serializable(child->key, !prefix || !prefix[0]))
				return false;
			char* new_prefix;
			if (prefix && prefix[0]) {
				size_t prefix_len = strlen(prefix);
				size_t key_len = strlen(child->key);
				if (prefix_len > SIZE_MAX - key_len - 2) return false;
				new_prefix = malloc(prefix_len + key_len + 2);
				if (!new_prefix) return false;
				memcpy(new_prefix, prefix, prefix_len);
				new_prefix[prefix_len] = '.';
				memcpy(new_prefix + prefix_len + 1, child->key, key_len + 1);
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
		if (!node->items) return false;
		list* p;
		if ((p = list_first(node->items))) do {
			LIST_DATA_DECLARE(child, p, confignode*);
			if (!child || child->parent != node) return false;
			if (child->index < 0) return false;
			char* new_prefix;
			size_t prefix_len = prefix ? strlen(prefix) : 0;
			char index[32];
			int index_len = snprintf(
				index, sizeof(index), "[%lld]", (long long)child->index
			);
			if (index_len <= 0 || (size_t)index_len >= sizeof(index)) return false;
			if (prefix_len > SIZE_MAX - (size_t)index_len - 1) return false;
			new_prefix = malloc(prefix_len + (size_t)index_len + 1);
			if (!new_prefix) return false;
			if (prefix_len > 0) memcpy(new_prefix, prefix, prefix_len);
			memcpy(new_prefix + prefix_len, index, (size_t)index_len + 1);
			bool result_ok = conf_save_node_recursive(child, new_prefix, result, size, pos);
			free(new_prefix);
			if (!result_ok) return false;
		} while ((p = p->next));
		return true;
	}
	return false;
}

/**
 * @brief Save configuration node tree to string in conf format.
 * Converts the config tree back to key=value format with nested paths.
 *
 * @param node root config node to save
 * @return newly allocated string containing configuration, or NULL on error
 */
char* configfile_conf_save_string(confignode* node) {
	if (!node || confignode_get_type(node) != CONFIGNODE_TYPE_MAP) return NULL;
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
