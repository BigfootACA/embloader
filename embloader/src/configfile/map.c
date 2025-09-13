#include <string.h>
#include "internal.h"
#include "debugs.h"

/**
 * @brief Get a child node from a map-type config node by key name.
 *
 * @param node the map-type node to get child from
 * @param key the key name of the child node to retrieve
 * @return the child node with the specified key, or NULL if not found or
 * invalid parameters
 *
 */
confignode* confignode_map_get(confignode* node, const char* key) {
	if (!node || node->type != CONFIGNODE_TYPE_MAP) return NULL;
	list* p;
	if ((p = list_first(node->items))) do {
		LIST_DATA_DECLARE(n, p, confignode*);
		if (!n || n->parent != node || !n->key) continue;
		if (strcmp(key, n->key) != 0) continue;
		return n;
	} while ((p = p->next));
	return NULL;
}

/**
 * @brief Create a new map-type config node.
 *
 * @return a new map-type config node, or NULL on allocation failure
 *
 */
confignode* confignode_new_map() {
	confignode* n;
	n = confignode_new();
	if (!n) return NULL;
	n->type = CONFIGNODE_TYPE_MAP;
	return n;
}

/**
 * @brief Set a child node in a map-type config node with the specified key.
 * If a node with the same key already exists, it will be replaced.
 *
 * @param node the map-type node to set child in
 * @param key the key name for the child node
 * @param sub the child node to set
 * @return true on success, false if parameters are invalid or operation failed
 *
 */
bool confignode_map_set(confignode* node, const char* key, confignode* sub) {
	if (!node || !key || !sub || sub->parent) return false;
	if (node->type != CONFIGNODE_TYPE_MAP) return false;
	list* p;
	bool found = false;
	if ((p = list_first(node->items))) do {
		LIST_DATA_DECLARE(n, p, confignode*);
		if (!n || n->parent != node) continue;
		if (strcmp(key, n->key) != 0) continue;
		confignode_clean(n);
		p->data = sub;
		found = true;
		break;
	} while ((p = p->next));
	if (!found) {
		int st = list_obj_add_new(&node->items, sub);
		if (st != 0) return false;
	}
	confignode_set_key(sub, key);
	sub->parent = node;
	return true;
}

/**
 * @brief Set a child node at the specified path in the configuration tree,
 * replacing any existing node at that path.
 *
 * @param node the root node to start path lookup from
 * @param path the path string (e.g., "config.section.key")
 * @param sub the child node to set at the path
 * @return true on success, false if parameters are invalid or operation failed
 *
 */
bool confignode_path_map_set(confignode* node, const char* path, confignode* sub) {
	if (!node || !sub || sub->parent) return false;
	confignode* n = confignode_path_lookup(node, path, true);
	if (!n) return false;
	return confignode_replace(&n, sub);
}
