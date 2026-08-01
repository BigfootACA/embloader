#include <inttypes.h>
#include "internal.h"

static bool confignode_path_parse_index(
	const char* start,
	const char* end,
	size_t* index
) {
	if (!start || !end || !index || start >= end) return false;
	size_t value = 0;
	for (const char* p = start; p < end; p++) {
		if (*p < '0' || *p > '9') return false;
		size_t digit = (size_t) (*p - '0');
		if (value > (SIZE_MAX - digit) / 10) return false;
		value = value * 10 + digit;
	}
	if (value > SIZE_MAX / 2) return false;
	*index = value;
	return true;
}

static bool confignode_path_validate(const char* path) {
	if (!path) return false;
	if (!path[0]) return true;
	if (path[0] == '[') {
		const char* close = strchr(path + 1, ']');
		size_t index;
		if (!close || !confignode_path_parse_index(path + 1, close, &index))
			return false;
		const char* next = close + 1;
		if (!*next) return true;
		if (*next == '.') {
			next++;
			if (!*next) return false;
		} else if (*next != '[') return false;
		return confignode_path_validate(next);
	}
	const char* delim = strpbrk(path, ".[" );
	if (!delim) return true;
	if (delim == path) return false;
	const char* next = *delim == '[' ? delim : delim + 1;
	return *next && confignode_path_validate(next);
}

static bool confignode_path_require_type(
	confignode** node,
	confignode_type type,
	bool create
) {
	if ((*node)->type == type) return true;
	if (!create || !(*node)->parent) return false;
	confignode* replacement = type == CONFIGNODE_TYPE_ARRAY ?
		confignode_new_array() : confignode_new_map();
	if (!replacement) return false;
	if (!confignode_replace(node, replacement)) {
		confignode_clean(replacement);
		return false;
	}
	return true;
}

/**
 * @brief Look up a config node by path string, optionally creating missing nodes.
 * Path format: "key1.key2[0].key3" where keys access map children and [index]
 * accesses array children.
 *
 * @param node the root node to start lookup from
 * @param path the path string to look up
 * @param create if true, missing nodes will be created; if false, returns NULL
 * for missing nodes
 * @return the node at the specified path, or NULL if not found/creation failed
 *
 */
confignode* confignode_path_lookup(confignode* node, const char* path, bool create) {
	if (!node || !confignode_path_validate(path)) return NULL;
	if (!path[0]) return node;
	const char* next_path = NULL;
	confignode* next;
	if (path[0] == '[') {
		const char* close = strchr(path + 1, ']');
		size_t index;
		if (!close || !confignode_path_parse_index(path + 1, close, &index))
			return NULL;
		next_path = close + 1;
		if (*next_path == '.') {
			next_path++;
			if (!*next_path) return NULL;
		}
		else if (*next_path && *next_path != '[') return NULL;
		if (!*next_path) next_path = NULL;
		if (!confignode_path_require_type(
			&node, CONFIGNODE_TYPE_ARRAY, create
		)) return NULL;
		next = confignode_array_get(node, index);
		if (!next) {
			if (!create || index == SIZE_MAX) return NULL;
			if (!confignode_array_extend(node, index + 1)) return NULL;
			next = confignode_array_get(node, index);
			if (!next) return NULL;
		}
	} else {
		const char* delim = strpbrk(path, ".[" );
		size_t key_len = delim ? (size_t) (delim - path) : strlen(path);
		if (key_len == 0) return NULL;
		char* key = strndup(path, key_len);
		if (!key) return NULL;
		if (delim) {
			next_path = *delim == '[' ? delim : delim + 1;
			if (!*next_path) {
				free(key);
				return NULL;
			}
		}
		if (!confignode_path_require_type(
			&node, CONFIGNODE_TYPE_MAP, create
		)) {
			free(key);
			return NULL;
		}
		next = confignode_map_get(node, key);
		if (!next && create) {
			next = confignode_new();
			if (!next || !confignode_map_set(node, key, next)) {
				if (next) confignode_clean(next);
				next = NULL;
			}
		}
		free(key);
		if (!next) return NULL;
	}
	return next_path ?
		confignode_path_lookup(next, next_path, create) : next;
}

static void confignode_path_get_rec(confignode* node, char* buff, size_t size) {
	if (!node->parent) return;
	confignode_path_get_rec(node->parent, buff, size);
	if (node->parent->type == CONFIGNODE_TYPE_MAP) {
		if (*buff) strlcat(buff, ".", size);
		strlcat(buff, node->key, size);
	}
	if (node->parent->type == CONFIGNODE_TYPE_ARRAY) {
		char fmt[32];
		memset(fmt, 0, sizeof(fmt));
		snprintf(fmt, sizeof(fmt), "[%" PRId64 "]", (int64_t) node->index);
		strlcat(buff, fmt, size);
	}
}

/**
 * @brief Get the full path string for a config node.
 * The path will be in the format "key1.key2[0].key3" representing the node's
 * location in the tree.
 *
 * @param node the node to get path for
 * @param buff buffer to store the path string
 * @param size size of the buffer
 * @return true on success, false if parameters are invalid
 *
 */
bool confignode_path_get(confignode* node, char* buff, size_t size) {
	if (!node || !buff || size <= 0) return false;
	memset(buff, 0, size);
	confignode_path_get_rec(node, buff, size);
	return true;
}

/**
 * @brief Set a value at the specified path in a config node tree.
 * Missing intermediate nodes will be created as needed.
 *
 * @param node the root node to start from
 * @param path the path string where to set the value
 * @param value the value to set, or NULL to create a null-type node
 * @return true on success, false if operation failed
 *
 */
bool confignode_path_set_value(confignode* node, const char* path, const confignode_value* value) {
	confignode* n = confignode_path_lookup(node, path, true);
	if (!n) return false;
	if (n->type == CONFIGNODE_TYPE_VALUE && value)
		return confignode_value_set(n, value);
	confignode* replacement = value ?
		confignode_new_value(value) : confignode_new();
	if (!replacement) return false;
	if (!confignode_replace(&n, replacement)) {
		confignode_clean(replacement);
		return false;
	}
	return true;
}

/**
 * @brief Get the value at the specified path in the configuration tree.
 * The value is copied to the provided structure if found.
 *
 * @param node the root node to start path lookup from
 * @param path the path string pointing to the value (e.g.,
 * "config.section.value")
 * @param value pointer to structure to receive the value (can be NULL to just
 * check existence)
 * @return true if the path exists and points to a value, false otherwise
 *
 */
bool confignode_path_get_value(confignode* node, const char* path, confignode_value* value) {
	confignode* n = confignode_path_lookup(node, path, false);
	if (!n) return false;
	return confignode_value_get(n, value);
}
