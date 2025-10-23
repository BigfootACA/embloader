#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "internal.h"
#include <stdarg.h>

/**
 * @brief Create a new value-type config node with a string value.
 * Convenience function for creating string value nodes.
 *
 * @param s the string value to set
 * @return a new value-type config node, or NULL on allocation failure
 *
 */

confignode* confignode_new_string(const char* s) {
	confignode_value v = {
		.type = VALUE_STRING,
		.v = {.s = (char*) s},
		.len = s ? strlen(s) : 0,
	};
	return confignode_new_value(&v);
}

/**
 * @brief Create a new value-type config node with a formatted string value.
 * Convenience function for creating string value nodes using printf-style
 * formatting.
 *
 * @param s the printf-style format string
 * @param ... additional arguments for formatting
 * @return a new value-type config node, or NULL on allocation failure
 *
 */
confignode* confignode_new_string_fmt(const char* s, ...) {
	if (!s) return NULL;
	char* buf = NULL;
	va_list va;
	va_start(va, s);
	int len = vasprintf(&buf, s, va);
	va_end(va);
	if (len < 0 || !buf) return NULL;
	confignode* n = confignode_new_string(buf);
	free(buf);
	return n;
}

/**
 * @brief Create a new value-type config node with an integer value.
 * Convenience function for creating integer value nodes.
 *
 * @param i the integer value to set
 * @return a new value-type config node, or NULL on allocation failure
 *
 */
confignode* confignode_new_int(int64_t i) {
	confignode_value v = {
		.type = VALUE_INT,
		.v = {.i = i},
		.len = sizeof(int64_t),
	};
	return confignode_new_value(&v);
}

/**
 * @brief Create a new value-type config node with a floating-point value.
 * Convenience function for creating float value nodes.
 *
 * @param f the double value to set
 * @return a new value-type config node, or NULL on allocation failure
 *
 */
confignode* confignode_new_float(double f) {
	confignode_value v = {
		.type = VALUE_FLOAT,
		.v = {.f = f},
		.len = sizeof(double),
	};
	return confignode_new_value(&v);
}

/**
 * @brief Create a new value-type config node with a boolean value.
 * Convenience function for creating boolean value nodes.
 *
 * @param b the boolean value to set
 * @return a new value-type config node, or NULL on allocation failure
 *
 */
confignode* confignode_new_bool(bool b) {
	confignode_value v = {
		.type = VALUE_BOOL,
		.v = {.b = b},
		.len = sizeof(bool),
	};
	return confignode_new_value(&v);
}

/**
 * @brief Set a string value in a value-type config node.
 * Convenience function for setting string values.
 *
 * @param node the value-type node to set value for
 * @param s the string value to set
 * @return true on success, false if operation failed
 *
 */
bool confignode_value_set_string(confignode* node, const char* s) {
	confignode_value v = {
		.type = VALUE_STRING,
		.v = {.s = (char*) s},
		.len = s ? strlen(s) : 0,
	};
	return confignode_value_set(node, &v);
}

/**
 * @brief Set a string value with len in a value-type config node.
 * Convenience function for setting string values.
 *
 * @param node the value-type node to set value for
 * @param s the string value to set
 * @param len the length of the string value
 * @return true on success, false if operation failed
 *
 */
bool confignode_value_set_stringn(confignode* node, const char* s, size_t len) {
	confignode_value v = {
		.type = VALUE_STRING,
		.v = {.s = (char*) s},
		.len = s ? len : 0,
	};
	return confignode_value_set(node, &v);
}

/**
 * @brief Set a formatted string value in a value-type config node.
 * Convenience function for setting string values using printf-style formatting.
 *
 * @param node the value-type node to set value for
 * @param fmt the printf-style format string
 * @param ... additional arguments for formatting
 * @return true on success, false if operation failed
 *
 */
bool confignode_value_set_string_fmt(confignode* node, const char* fmt, ...) {
	if (!fmt) return false;
	char* buf = NULL;
	va_list va;
	va_start(va, fmt);
	int len = vasprintf(&buf, fmt, va);
	va_end(va);
	if (len < 0 || !buf) return false;
	bool res = confignode_value_set_string(node, buf);
	free(buf);
	return res;
}

/**
 * @brief Set an integer value in a value-type config node.
 * Convenience function for setting integer values.
 *
 * @param node the value-type node to set value for
 * @param i the integer value to set
 * @return true on success, false if operation failed
 *
 */
bool confignode_value_set_int(confignode* node, int64_t i) {
	confignode_value v = {
		.type = VALUE_INT,
		.v = {.i = i},
		.len = sizeof(int64_t),
	};
	return confignode_value_set(node, &v);
}

/**
 * @brief Set a floating-point value in a value-type config node.
 * Convenience function for setting float values.
 *
 * @param node the value-type node to set value for
 * @param f the double value to set
 * @return true on success, false if operation failed
 *
 */
bool confignode_value_set_float(confignode* node, double f) {
	confignode_value v = {
		.type = VALUE_FLOAT,
		.v = {.f = f},
		.len = sizeof(double),
	};
	return confignode_value_set(node, &v);
}

/**
 * @brief Set a boolean value in a value-type config node.
 * Convenience function for setting boolean values.
 *
 * @param node the value-type node to set value for
 * @param b the boolean value to set
 * @return true on success, false if operation failed
 *
 */
bool confignode_value_set_bool(confignode* node, bool b) {
	confignode_value v = {
		.type = VALUE_BOOL,
		.v = {.b = b},
		.len = sizeof(bool),
	};
	return confignode_value_set(node, &v);
}

/**
 * @brief Set a string value at the specified path in the configuration tree.
 * Creates intermediate nodes as needed if the path doesn't exist.
 *
 * @param node the root node to start path lookup from
 * @param path the path string (e.g., "config.section.value")
 * @param s the string value to set
 * @return true on success, false on failure
 *
 */
bool confignode_path_set_string(
	confignode* node,
	const char* path,
	const char* s
) {
	confignode_value v = {
		.type = VALUE_STRING,
		.v = {.s = (char*) s},
		.len = s ? strlen(s) : 0,
	};
	return confignode_path_set_value(node, path, &v);
}

/**
 * @brief Set a formatted string value at the specified path in the configuration tree.
 * Creates intermediate nodes as needed if the path doesn't exist.
 * Uses printf-style formatting to create the string value.
 *
 * @param node the root node to start path lookup from
 * @param path the path string (e.g., "config.section.value")
 * @param fmt the printf-style format string
 * @param ... additional arguments for formatting
 * @return true on success, false on failure
 *
 */
bool confignode_path_set_string_fmt(
	confignode* node,
	const char* path,
	const char* fmt,
	...
) {
	if (!path) return false;
	char* buf = NULL;
	va_list va;
	va_start(va, fmt);
	int len = vasprintf(&buf, fmt, va);
	va_end(va);
	if (len < 0 || !buf) return false;
	bool res = confignode_path_set_string(node, path, buf);
	free(buf);
	return res;
}

/**
 * @brief Set an integer value at the specified path in the configuration tree.
 * Creates intermediate nodes as needed if the path doesn't exist.
 *
 * @param node the root node to start path lookup from
 * @param path the path string (e.g., "config.section.count")
 * @param i the integer value to set
 * @return true on success, false on failure
 *
 */
bool confignode_path_set_int(confignode* node, const char* path, int64_t i) {
	confignode_value v = {
		.type = VALUE_INT,
		.v = {.i = i},
		.len = sizeof(int64_t),
	};
	return confignode_path_set_value(node, path, &v);
}

/**
 * @brief Set a floating-point value at the specified path in the configuration tree.
 * Creates intermediate nodes as needed if the path doesn't exist.
 *
 * @param node the root node to start path lookup from
 * @param path the path string (e.g., "config.section.ratio")
 * @param f the double value to set
 * @return true on success, false on failure
 *
 */
bool confignode_path_set_float(confignode* node, const char* path, double f) {
	confignode_value v = {
		.type = VALUE_FLOAT,
		.v = {.f = f},
		.len = sizeof(double),
	};
	return confignode_path_set_value(node, path, &v);
}

/**
 * @brief Set a boolean value at the specified path in the configuration tree.
 * Creates intermediate nodes as needed if the path doesn't exist.
 *
 * @param node the root node to start path lookup from
 * @param path the path string (e.g., "config.section.enabled")
 * @param b the boolean value to set
 * @return true on success, false on failure
 *
 */
bool confignode_path_set_bool(confignode* node, const char* path, bool b) {
	confignode_value v = {
		.type = VALUE_BOOL,
		.v = {.b = b},
		.len = sizeof(bool),
	};
	return confignode_path_set_value(node, path, &v);
}

/**
 * @brief Append a node to an array at the specified path.
 * Creates the array and intermediate nodes as needed if the path doesn't exist.
 * If the target node exists but is not an array, it will be replaced with a new
 * array.
 *
 * @param node the root node to start path lookup from
 * @param path the path string pointing to the array (e.g., "config.items")
 * @param sub the node to append to the array
 * @return true on success, false on failure
 *
 */
bool confignode_path_array_append(
	confignode* node,
	const char* path,
	confignode* sub
) {
	confignode* n = confignode_path_lookup(node, path, true);
	if (!n) return false;
	if (n->type != CONFIGNODE_TYPE_ARRAY) {
		if (!n->parent) return false;
		confignode_replace(&n, confignode_new_array());
		if (n->type != CONFIGNODE_TYPE_ARRAY) return false;
	}
	return confignode_array_append(n, sub);
}

/**
 * @brief Set a node at a specific index in an array at the specified path.
 * Creates the array and intermediate nodes as needed if the path doesn't exist.
 * If the target node exists but is not an array, it will be replaced with a new
 * array. Extends the array if the index is beyond the current length.
 *
 * @param node the root node to start path lookup from
 * @param path the path string pointing to the array (e.g., "config.items")
 * @param index the array index to set
 * @param sub the node to set at the specified index
 * @return true on success, false on failure
 *
 */
bool confignode_path_array_set(
	confignode* node,
	const char* path,
	size_t index,
	confignode* sub
) {
	confignode* n = confignode_path_lookup(node, path, true);
	if (!n) return false;
	if (n->type != CONFIGNODE_TYPE_ARRAY) {
		if (!n->parent) return false;
		confignode_replace(&n, confignode_new_array());
		if (n->type != CONFIGNODE_TYPE_ARRAY) return false;
	}
	return confignode_array_set(n, index, sub);
}

/**
 * @brief Insert a node at a specific index in an array at the specified path.
 * Creates the array and intermediate nodes as needed if the path doesn't exist.
 * If the target node exists but is not an array, it will be replaced with a new
 * array. Shifts existing elements to make room for the new element.
 *
 * @param node the root node to start path lookup from
 * @param path the path string pointing to the array (e.g., "config.items")
 * @param index the array index to insert at
 * @param sub the node to insert at the specified index
 * @return true on success, false on failure
 *
 */
bool confignode_path_array_insert(
	confignode* node,
	const char* path,
	size_t index,
	confignode* sub
) {
	confignode* n = confignode_path_lookup(node, path, true);
	if (!n) return false;
	if (n->type != CONFIGNODE_TYPE_ARRAY) {
		if (!n->parent) return false;
		confignode_replace(&n, confignode_new_array());
		if (n->type != CONFIGNODE_TYPE_ARRAY) return false;
	}
	return confignode_array_insert(n, index, sub);
}

/**
 * @brief Get the length of an array at the specified path.
 * Returns 0 if the path doesn't exist or doesn't point to an array.
 *
 * @param node the root node to start path lookup from
 * @param path the path string pointing to the array (e.g., "config.items")
 * @return the number of elements in the array, or 0 if not found or not an array
 *
 */
size_t confignode_path_array_len(confignode* node, const char* path) {
	confignode* n = confignode_path_lookup(node, path, false);
	if (!n || n->type != CONFIGNODE_TYPE_ARRAY) return 0;
	return confignode_array_len(n);
}

/**
 * @brief Get a string value at the specified path with default fallback.
 * Performs type conversion if the value exists but is not a string.
 *
 * @param node the root node to start path lookup from
 * @param path the path string pointing to the value
 * @param def the default value to return if path doesn't exist
 * @param ok optional pointer to receive success status
 * @return newly allocated string (must be freed), or copy of default value
 *
 */
char* confignode_path_get_string(
	confignode* node,
	const char* path,
	const char* def,
	bool* ok
) {
	if (ok) *ok = false;
	confignode* n = confignode_path_lookup(node, path, false);
	if (!n) return def ? strdup(def) : NULL;
	return confignode_value_get_string(n, def, ok);
}

/**
 * @brief Get an integer value at the specified path with default fallback.
 * Performs type conversion if the value exists but is not an integer.
 *
 * @param node the root node to start path lookup from
 * @param path the path string pointing to the value
 * @param def the default value to return if path doesn't exist or conversion
 * fails
 * @param ok optional pointer to receive success status
 * @return the integer value, or the default value
 *
 */
int64_t confignode_path_get_int(
	confignode* node,
	const char* path,
	int64_t def,
	bool* ok
) {
	if (ok) *ok = false;
	confignode* n = confignode_path_lookup(node, path, false);
	if (!n) return def;
	return confignode_value_get_int(n, def, ok);
}

/**
 * @brief Get a floating-point value at the specified path with default fallback.
 * Performs type conversion if the value exists but is not a float.
 *
 * @param node the root node to start path lookup from
 * @param path the path string pointing to the value
 * @param def the default value to return if path doesn't exist or conversion
 * fails
 * @param ok optional pointer to receive success status
 * @return the double value, or the default value
 *
 */
double confignode_path_get_float(
	confignode* node,
	const char* path,
	double def,
	bool* ok
) {
	if (ok) *ok = false;
	confignode* n = confignode_path_lookup(node, path, false);
	if (!n) return def;
	return confignode_value_get_float(n, def, ok);
}

/**
 * @brief Get a boolean value at the specified path with default fallback.
 * Performs type conversion if the value exists but is not a boolean.
 *
 * @param node the root node to start path lookup from
 * @param path the path string pointing to the value
 * @param def the default value to return if path doesn't exist or conversion
 * fails
 * @param ok optional pointer to receive success status
 * @return the boolean value, or the default value
 *
 */
bool confignode_path_get_bool(
	confignode* node,
	const char* path,
	bool def,
	bool* ok
) {
	if (ok) *ok = false;
	confignode* n = confignode_path_lookup(node, path, false);
	if (!n) return def;
	return confignode_value_get_bool(n, def, ok);
}

/**
 * @brief Get string list from a value node or array/map of values at path.
 * This is a path-based wrapper around confignode_value_get_string_or_list_to_list().
 * For VALUE nodes, returns a single-element list containing the string value.
 * For ARRAY or MAP nodes, returns a list of strings from all child value nodes.
 * The returned list and its string elements must be freed by the caller using
 * list_free_all_def().
 *
 * @param node the root node to start path lookup from
 * @param path the path string pointing to the node
 * @param ok optional pointer to bool that receives success status
 * @return newly allocated list of strings, or NULL on failure or path not found
 *         Caller is responsible for freeing the list and its contents
 *
 */
list* confignode_path_get_string_or_list_to_list(
	confignode* node,
	const char* path,
	bool* ok
) {
	if (ok) *ok = false;
	confignode* n = confignode_path_lookup(node, path, false);
	return n ? confignode_value_get_string_or_list_to_list(n, ok) : NULL;
}

/**
 * @brief Get concatenated string from a value node or array/map of values at path.
 * This is a path-based wrapper around confignode_value_get_string_or_list().
 * For VALUE nodes, returns the string representation of the value.
 * For ARRAY or MAP nodes, returns all child values concatenated with the
 * specified separator. Empty arrays/maps return NULL with success status.
 *
 * @param node the root node to start path lookup from
 * @param path the path string pointing to the node
 * @param sep the separator string to use between elements
 * @param ok optional pointer to bool that receives success status
 * @return newly allocated concatenated string that must be freed by caller,
 *         or NULL on failure, path not found, or empty list
 *
 */
char* confignode_path_get_string_or_list(
	confignode* node,
	const char* path,
	char* sep,
	bool* ok
) {
	if (ok) *ok = false;
	confignode* n = confignode_path_lookup(node, path, false);
	if (!n) return NULL;
	return confignode_value_get_string_or_list(n, sep, ok);
}
