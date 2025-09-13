#include "internal.h"
#include "debugs.h"

/**
 * @brief Create a deep copy of a config node and all its children.
 *
 * @param node the node to copy
 * @return a new config node that is a deep copy of the original, or NULL on
 * allocation failure
 *
 */
confignode* confignode_copy(confignode* node) {
	if (!node) return NULL;
	switch (node->type) {
		case CONFIGNODE_TYPE_NULL:
			return confignode_new();
		case CONFIGNODE_TYPE_VALUE:
			return confignode_new_value(&node->value);
		case CONFIGNODE_TYPE_ARRAY: {
			confignode* a = confignode_new_array();
			if (!a) return NULL;
			list* p;
			if ((p = list_first(node->items))) do {
					LIST_DATA_DECLARE(i, p, confignode*);
					confignode* copy = confignode_copy(i);
					if (!copy) {
						confignode_clean(a);
						return NULL;
					}
					copy->parent = a;
					list_obj_add_new(&a->items, copy);
				} while ((p = p->next));
			configfile_array_fixup(a);
			return a;
		}
		case CONFIGNODE_TYPE_MAP: {
			confignode* m = confignode_new_map();
			if (!m) return NULL;
			list* p;
			if ((p = list_first(node->items))) do {
					LIST_DATA_DECLARE(i, p, confignode*);
					confignode* copy = confignode_copy(i);
					if (!copy) {
						confignode_clean(m);
						return NULL;
					}
					confignode_set_key(copy, i->key);
					copy->parent = m;
					list_obj_add_new(&m->items, copy);
				} while ((p = p->next));
			return m;
		}
		default:
			return NULL;
	}
}

/**
 * @brief Replace a config node with a new node, maintaining parent-child
 * relationships. The old node will be cleaned up and freed.
 *
 * @param node pointer to the node pointer to be replaced
 * @param new the new node to replace with
 * @return true on success, false if parameters are invalid or operation failed
 *
 */
bool confignode_replace(confignode** node, confignode* new) {
	if (!node || !*node || !new) return false;
	if (!(*node)->parent || new->parent) return false;
	if (
		(*node)->parent->type != CONFIGNODE_TYPE_MAP &&
		(*node)->parent->type != CONFIGNODE_TYPE_ARRAY
	) return false;
	bool found = false;
	list* p;
	if ((p = list_first((*node)->parent->items))) do {
		LIST_DATA_DECLARE(n, p, confignode*);
		if (n != (*node)) continue;
		p->data = new;
		found = true;
		break;
	} while ((p = p->next));
	if (!found) return false;
	if ((*node)->parent->type == CONFIGNODE_TYPE_MAP)
		confignode_set_key(new, (*node)->key);
	if ((*node)->parent->type == CONFIGNODE_TYPE_ARRAY)
		new->index = (*node)->index;
	new->parent = (*node)->parent;
	(*node)->parent = NULL;
	confignode_clean(*node);
	*node = new;
	return true;
}

/**
 * @brief Merge a config node tree into another config node tree.
 * For maps, child nodes with the same keys will be recursively merged.
 * For arrays, child nodes will be appended.
 * For other types, the target node will be replaced.
 *
 * @param node the target node to merge into
 * @param new the source node to merge from
 * @return true on success, false if parameters are invalid or operation failed
 *
 */
bool confignode_merge(confignode* node, confignode* new) {
	if (!node || !new) return false;
	if (
		node->type != CONFIGNODE_TYPE_MAP &&
		node->type != CONFIGNODE_TYPE_ARRAY
	) return confignode_replace(&node, confignode_copy(new));
	list* p;
	if ((p = list_first(new->items))) do {
		LIST_DATA_DECLARE(n, p, confignode*);
		if (!n || n->parent != new) continue;
		bool r = false;
		if (node->type == CONFIGNODE_TYPE_ARRAY) {
			r = confignode_array_append(node, confignode_copy(n));
		} else if (node->type == CONFIGNODE_TYPE_MAP) {
			confignode* s = confignode_map_get(node, n->key);
			if (s) r = confignode_merge(s, n);
			else r = confignode_map_set(node, n->key, confignode_copy(n));
		}
		if (!r) return false;
	} while ((p = p->next));
	return true;
}
