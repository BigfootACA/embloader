#include "internal.h"

/**
 * @brief Get the type of a config node.
 *
 * @param node the node to get type
 * @return the type of the config node
 *
 */
confignode_type confignode_get_type(confignode* node) {
	if (!node) return CONFIGNODE_TYPE_NULL;
	return node->type;
}

/**
 * @brief Check if a config node is of a specific type.
 *
 * @param node the node to check (may be NULL)
 * @param type the type to compare against
 * @return true if node exists and has the specified type, false otherwise
 */
bool confignode_is_type(confignode* node, confignode_type type) {
	if (!node) return false;
	return node->type == type;
}

/**
 * @brief Get the key name of a config node.
 *
 * @param node the node to get key from (may be NULL)
 * @return the key string, or NULL if node is NULL or has no key
 */
const char* confignode_get_key(confignode* node) {
	if (!node) return NULL;
	return node->key;
}

/**
 * @brief Get the array index of a config node.
 * Only meaningful for nodes that are array elements.
 *
 * @param node the node to get index from (may be NULL)
 * @return the array index, or -1 if node is NULL or not an array element
 */
int64_t confignode_get_index(confignode* node) {
	if (!node) return -1;
	return node->index;
}

/**
 * @brief Check if a config node is empty.
 * For containers (maps/arrays), checks if they have no items.
 * For values, checks if they contain zero/empty values.
 *
 * @param node the node to check (may be NULL)
 * @return true if node is NULL or contains empty/zero values, false otherwise
 */
bool confignode_is_empty(confignode* node) {
	if (!node) return true;
	if (node->type == CONFIGNODE_TYPE_MAP || node->type == CONFIGNODE_TYPE_ARRAY)
		return !node->items || list_count(node->items) == 0;
	if (node->type == CONFIGNODE_TYPE_VALUE) switch (node->value.type) {
		case VALUE_STRING:
			return !node->value.v.s || !node->value.v.s[0];
		case VALUE_INT:
			return node->value.v.i == 0;
		case VALUE_FLOAT:
			return node->value.v.f == 0.0;
		case VALUE_BOOL:
			return !node->value.v.b;
	}
	return true;
}

/**
 * @brief Get the parent node of a config node.
 *
 * @param node the node to get parent from
 * @return the parent node, or NULL if node is NULL or has no parent
 *
 */
confignode* confignode_parent(confignode* node) {
	return node ? node->parent : NULL;
}

/**
 * @brief Get the root node of a config node tree.
 *
 * @param node the node to get root from
 * @return the root node of the tree, or the node itself if it's already root
 *
 */
confignode* confignode_root(confignode* node) {
	confignode* n = node;
	while (n && n->parent) n = n->parent;
	return n;
}

/**
 * @brief Create a new config node with default values.
 *
 * @return a new config node with type CONFIGNODE_TYPE_NULL, or NULL on
 * allocation failure
 *
 */
confignode* confignode_new() {
	confignode* n;
	n = malloc(sizeof(confignode));
	if (!n) return NULL;
	memset(n, 0, sizeof(confignode));
	n->index = -1;
	n->type = CONFIGNODE_TYPE_NULL;
	return n;
}

/**
 * @brief Set the key name for a config node.
 *
 * @param node the node to set key for
 * @param key the key string to set, or NULL to clear the key
 * @return TRUE on success, false if node is NULL or memory allocation failed
 *
 */
bool confignode_set_key(confignode* node, const char* key) {
	if (!node) return false;
	if (node->key) free(node->key);
	node->key = NULL;
	if (!key) return true;
	node->key = strdup(key);
	return node->key != NULL;
}

/**
 * @brief Clean up and free a config node and all its children recursively.
 * This function removes the node from its parent, frees all allocated memory,
 * and recursively cleans up all child nodes.
 *
 * @param node the node to clean up and free
 *
 */
void confignode_clean(confignode* node) {
	if (!node) return;
	if (node->parent) {
		list_obj_del_data(&node->parent->items, node, NULL);
		configfile_array_fixup(node->parent);
	}
	if (node->key) free(node->key);
	if (node->value.type == VALUE_STRING && node->value.v.s)
		free(node->value.v.s);
	if (node->items) {
		list *p, *n;
		if ((p = list_first(node->items))) do {
			n = p->next;
			LIST_DATA_DECLARE(i, p, confignode*);
			if (!i || i->parent != node) continue;
			confignode_clean(i);
		} while ((p = n));
		list_free_all(node->items, NULL);
	}
	memset(node, 0, sizeof(confignode));
	free(node);
}
