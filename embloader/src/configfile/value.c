#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <string.h>
#include "internal.h"
#include "str-utils.h"

/**
 * @brief Get the raw value from a value-type config node.
 * For string values, the returned string is a duplicate that must be freed by
 * the caller.
 *
 * @param node the node to get value from
 * @param out pointer to confignode_value struct to receive the value
 * @return true on success, false if node is NULL, not a value type, or memory
 * allocation failed
 *
 */
bool confignode_value_get(confignode* node, confignode_value* out) {
	if (!node || node->type != CONFIGNODE_TYPE_VALUE || !out) return false;
	memcpy(out, &node->value, sizeof(confignode_value));
	if (out->type == VALUE_STRING)
		if (!(out->v.s = strdup(node->value.v.s))) return false;
	return true;
}

/**
 * @brief Get string representation of a value node with automatic type conversion.
 * Converts integers, floats, and booleans to their string representations.
 *
 * @param node the value node to convert to string
 * @param def default string to return if conversion fails (will be duplicated)
 * @param ok optional pointer to bool that receives success status
 * @return newly allocated string that must be freed by caller, or duplicate of
 * def on failure
 *
 */
char* confignode_value_get_string(confignode* node, const char* def, bool* ok) {
	char* ret = NULL;
	if (ok) *ok = false;
	if (!node || node->type != CONFIGNODE_TYPE_VALUE) goto fail;
	switch (node->value.type) {
		case VALUE_STRING:
			if (!node->value.v.s) goto fail;
			ret = strdup(node->value.v.s);
			break;
		case VALUE_INT:
			if (asprintf(&ret, "%" PRId64, node->value.v.i) < 0)
				goto fail;
			break;
		case VALUE_FLOAT:
			if (asprintf(&ret, "%f", node->value.v.f) < 0)
				goto fail;
			break;
		case VALUE_BOOL:
			ret = strdup(node->value.v.b ? "true" : "false");
			break;
		default:;
	}
	if (ret) {
		if (ok) *ok = true;
		return ret;
	}
fail:
	if (ret) free(ret);
	return def ? strdup(def) : NULL;
}

/**
 * @brief Get integer value from a value node with automatic type conversion.
 * Converts strings (decimal), floats (truncated), and booleans (1/0) to integers.
 *
 * @param node the value node to convert to integer
 * @param def default value to return if conversion fails
 * @param ok optional pointer to bool that receives success status
 * @return converted integer value, or def on failure
 *
 */
int64_t confignode_value_get_int(confignode* node, int64_t def, bool* ok) {
	if (ok) *ok = false;
	if (!node || node->type != CONFIGNODE_TYPE_VALUE) return def;
	switch (node->value.type) {
		case VALUE_INT:
			if (ok) *ok = true;
			return node->value.v.i;
		case VALUE_STRING: {
			char* endptr;
			int64_t ret = strtoll(node->value.v.s, &endptr, 10);
			if (endptr == node->value.v.s) return def;
			if (ok) *ok = true;
			return ret;
		}
		case VALUE_FLOAT:
			if (ok) *ok = true;
			return (int64_t) node->value.v.f;
		case VALUE_BOOL:
			if (ok) *ok = true;
			return node->value.v.b ? 1 : 0;
		default:
			return def;
	}
}

/**
 * @brief Get floating-point value from a value node with automatic type conversion.
 * Converts strings (using strtod), integers, and booleans (1.0/0.0) to doubles.
 *
 * @param node the value node to convert to float
 * @param def default value to return if conversion fails
 * @param ok optional pointer to bool that receives success status
 * @return converted double value, or def on failure
 *
 */
double confignode_value_get_float(confignode* node, double def, bool* ok) {
	if (ok) *ok = false;
	if (!node || node->type != CONFIGNODE_TYPE_VALUE) return def;
	switch (node->value.type) {
		case VALUE_FLOAT:
			if (ok) *ok = true;
			return node->value.v.f;
		case VALUE_STRING: {
			char* end = NULL;
			double ret = strtod(node->value.v.s, &end);
			if (end == node->value.v.s) return def;
			if (ok) *ok = true;
			return ret;
		}
		case VALUE_INT:
			if (ok) *ok = true;
			return (double) node->value.v.i;
		case VALUE_BOOL:
			if (ok) *ok = true;
			return node->value.v.b ? 1.0 : 0.0;
		default:
			return def;
	}
}

/**
 * @brief Get boolean value from a value node with automatic type conversion.
 * Converts strings ("true"/"yes"/"on" vs "false"/"no"/"off", case-insensitive),
 * integers (non-zero = true), and floats (non-zero = true) to booleans.
 *
 * @param node the value node to convert to boolean
 * @param def default value to return if conversion fails
 * @param ok optional pointer to bool that receives success status
 * @return converted boolean value, or def on failure
 *
 */
bool confignode_value_get_bool(confignode* node, bool def, bool* ok) {
	if (ok) *ok = false;
	if (!node || node->type != CONFIGNODE_TYPE_VALUE) return def;
	switch (node->value.type) {
		case VALUE_BOOL:
			if (ok) *ok = true;
			return node->value.v.b;
		case VALUE_STRING: {
			if (!node->value.v.s) return def;
			bool is_yes = string_is_true(node->value.v.s);
			bool is_no = string_is_false(node->value.v.s);
			if (!is_yes && !is_no) return def;
			else if (ok) *ok = true;
			return is_yes;
		}
		case VALUE_INT:
			if (ok) *ok = true;
			return node->value.v.i != 0;
		case VALUE_FLOAT:
			if (ok) *ok = true;
			return node->value.v.f != 0.0;
		default:
			return def;
	}
}

/**
 * @brief Create a new value-type config node with the specified value.
 *
 * @param value pointer to confignode_value struct containing the value to set
 * @return a new value-type config node, or NULL on allocation failure
 *
 */
confignode* confignode_new_value(const confignode_value* value) {
	confignode* n;
	n = confignode_new();
	if (!n) return NULL;
	n->type = CONFIGNODE_TYPE_VALUE;
	confignode_value_set(n, value);
	return n;
}

/**
 * @brief Set the value for a value-type config node.
 * If the node currently contains a string value, it will be freed.
 * For string values, a copy of the input string will be made.
 *
 * @param node the value-type node to set value for
 * @param value pointer to confignode_value struct containing the new value
 * @return true on success, false if node is NULL, not a value type, or
 * allocation failed
 *
 */
bool confignode_value_set(confignode* node, const confignode_value* value) {
	if (!node || node->type != CONFIGNODE_TYPE_VALUE || !value)
		return false;
	if (value->type == VALUE_STRING && !value->v.s) return false;
	if (node->value.type == VALUE_STRING && node->value.v.s)
		free(node->value.v.s);
	memcpy(&node->value, value, sizeof(confignode_value));
	if (value->type == VALUE_STRING)
		if (!(node->value.v.s = strdup(value->v.s))) return false;
	return true;
}
