#include <inttypes.h>
#include "internal.h"

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
	if (!node || !path) return NULL;
	if (!path[0]) return node;
	char *np, *nstr, *npath = NULL;
	confignode* next = NULL;
	size_t len = strlen(path);
	if (!(np = strdup(path))) return NULL;
	if (path[0] == '[') {
		size_t index = 0;
		char* end = NULL;
		if (!(nstr = memchr(np + 1, ']', len - 1)))
			goto cleanup;
		*nstr = 0, npath = nstr + 1;
		long long temp = strtoll(np + 1, &end, 10);
		if (temp < 0 || end != nstr) goto cleanup;
		index = (size_t) temp;
		if (node->type != CONFIGNODE_TYPE_ARRAY) {
			if (!create || !node->parent) goto cleanup;
			confignode_replace(&node, confignode_new_array());
			if (node->type != CONFIGNODE_TYPE_ARRAY) goto cleanup;
		}
		if (!confignode_array_extend(node, index + 1)) goto cleanup;
		if (!(next = confignode_array_get(node, index))) {
			if (!create) goto cleanup;
			next = confignode_new();
			if (!confignode_array_set(node, index, next)) goto cleanup;
		}
	} else {
		if ((nstr = strpbrk(np, ".["))) *nstr = 0, npath = nstr + 1;
		if (node->type != CONFIGNODE_TYPE_MAP) {
			if (!create || !node->parent) goto cleanup;
			confignode_replace(&node, confignode_new_map());
			if (node->type != CONFIGNODE_TYPE_MAP) goto cleanup;
		}
		if (!(next = confignode_map_get(node, np))) {
			if (!create) goto cleanup;
			next = confignode_new();
			if (!confignode_map_set(node, np, next)) goto cleanup;
		}
	}
	if (npath) next = confignode_path_lookup(next, npath, create);
cleanup:
	free(np);
	return next;
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
	if (!value) return confignode_replace(&n, confignode_new());
	return n->type == CONFIGNODE_TYPE_VALUE ?
		confignode_value_set(n, value) :
		confignode_replace(&n, confignode_new_value(value));
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
