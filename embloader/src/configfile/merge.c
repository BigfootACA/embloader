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
					if (!i || i->parent != node) {
						confignode_clean(a);
						return NULL;
					}
					confignode* copy = confignode_copy(i);
					if (!copy) {
						confignode_clean(a);
						return NULL;
					}
					if (!confignode_array_append(a, copy)) {
						confignode_clean(copy);
						confignode_clean(a);
						return NULL;
					}
				} while ((p = p->next));
			return a;
		}
		case CONFIGNODE_TYPE_MAP: {
			confignode* m = confignode_new_map();
			if (!m) return NULL;
			list* p;
			if ((p = list_first(node->items))) do {
					LIST_DATA_DECLARE(i, p, confignode*);
					if (!i || i->parent != node || !i->key) {
						confignode_clean(m);
						return NULL;
					}
					confignode* copy = confignode_copy(i);
					if (!copy) {
						confignode_clean(m);
						return NULL;
					}
					if (!confignode_map_set(m, i->key, copy)) {
						confignode_clean(copy);
						confignode_clean(m);
						return NULL;
					}
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
	confignode* old = *node;
	confignode* parent = old->parent;
	list* found = NULL;
	list* p;
	if ((p = list_first(parent->items))) do {
		LIST_DATA_DECLARE(n, p, confignode*);
		if (n != old) continue;
		found = p;
		break;
	} while ((p = p->next));
	if (!found) return false;
	if (parent->type == CONFIGNODE_TYPE_MAP) {
		if (!old->key || !confignode_set_key(new, old->key)) return false;
		new->index = -1;
	} else {
		if (!confignode_set_key(new, NULL)) return false;
		new->index = old->index;
	}
	found->data = new;
	new->parent = parent;
	old->parent = NULL;
	confignode_clean(old);
	*node = new;
	return true;
}

static void confignode_reparent_children(confignode* node) {
	list* p;
	if ((p = list_first(node->items))) do {
		LIST_DATA_DECLARE(child, p, confignode*);
		if (child) child->parent = node;
	} while ((p = p->next));
}

static bool confignode_replace_contents(confignode* node, confignode* replacement) {
	if (!node || !replacement || replacement->parent) return false;
	confignode temporary = *node;
	*node = *replacement;
	*replacement = temporary;
	char* replacement_key = node->key;
	node->parent = temporary.parent;
	node->key = temporary.key;
	node->index = temporary.index;
	replacement->parent = NULL;
	replacement->key = replacement_key;
	replacement->index = -1;
	confignode_reparent_children(node);
	confignode_reparent_children(replacement);
	confignode_clean(replacement);
	return true;
}

static bool confignode_merge_in_place(confignode* node, confignode* new) {
	if (!node || !new) return false;
	if (node == new) return true;
	if (node->type != new->type || (
		node->type != CONFIGNODE_TYPE_MAP &&
		node->type != CONFIGNODE_TYPE_ARRAY
	)) {
		confignode* copy = confignode_copy(new);
		if (!copy) return false;
		bool result = confignode_replace_contents(node, copy);
		if (!result) confignode_clean(copy);
		return result;
	}
	list* p;
	if ((p = list_first(new->items))) do {
		LIST_DATA_DECLARE(n, p, confignode*);
		if (!n || n->parent != new) continue;
		bool r = false;
		if (node->type == CONFIGNODE_TYPE_ARRAY) {
			confignode* copy = confignode_copy(n);
			if (!copy) return false;
			r = confignode_array_append(node, copy);
			if (!r) confignode_clean(copy);
		} else if (node->type == CONFIGNODE_TYPE_MAP) {
			if (!n->key) return false;
			confignode* s = confignode_map_get(node, n->key);
			if (s) r = confignode_merge_in_place(s, n);
			else {
				confignode* copy = confignode_copy(n);
				if (!copy) return false;
				r = confignode_map_set(node, n->key, copy);
				if (!r) confignode_clean(copy);
			}
		}
		if (!r) return false;
	} while ((p = p->next));
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
	if (node == new) return true;
	confignode* working = confignode_copy(node);
	if (!working) return false;
	if (!confignode_merge_in_place(working, new)) {
		confignode_clean(working);
		return false;
	}
	bool result = confignode_replace_contents(node, working);
	if (!result) confignode_clean(working);
	return result;
}
