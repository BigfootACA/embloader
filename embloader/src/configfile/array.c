#include "internal.h"

/**
 * @brief Get a child node from an array-type config node by index.
 *
 * @param node the array-type node to get child from
 * @param index the index of the child node to retrieve
 * @return the child node at the specified index, or NULL if not found or
 * invalid parameters
 *
 */
confignode* confignode_array_get(confignode* node, size_t index) {
	if (!node || node->type != CONFIGNODE_TYPE_ARRAY) return NULL;
	list* p;
	if ((p = list_first(node->items))) do {
			LIST_DATA_DECLARE(n, p, confignode*);
			if (!n || n->parent != node || n->index != index)
				continue;
			return n;
		} while ((p = p->next));
	return NULL;
}

/**
 * @brief Get the number of elements in an array-type config node.
 *
 * @param node the array-type node to get length from
 * @return the number of elements in the array, or 0 if not an array or invalid
 *
 */
size_t confignode_array_len(confignode* node) {
	if (!node || node->type != CONFIGNODE_TYPE_ARRAY) return 0;
	ssize_t ret = list_count(node->items);
	return ret > 0 ? (size_t) ret : 0;
}

/**
 * @brief Create a new array-type config node.
 *
 * @return a new array-type config node, or NULL on allocation failure
 *
 */
confignode* confignode_new_array() {
	confignode* n;
	n = confignode_new();
	if (!n) return NULL;
	n->type = CONFIGNODE_TYPE_ARRAY;
	return n;
}

/**
 * @brief Append a child node to the end of an array-type config node.
 *
 * @param node the array-type node to append to
 * @param sub the child node to append
 * @return true on success, false if parameters are invalid or operation failed
 *
 */
bool confignode_array_append(confignode* node, confignode* sub) {
	if (!node || !sub || sub->parent) return false;
	if (node->type != CONFIGNODE_TYPE_ARRAY) return false;
	int st = list_obj_add_new(&node->items, sub);
	if (st != 0) return false;
	sub->parent = node;
	configfile_array_fixup(node);
	return true;
}

/**
 * @brief Fix up the index values of all child nodes in an array-type config node.
 * This function ensures that each child node has the correct index value
 * corresponding to its position in the array.
 *
 * @param node the array-type node to fix up
 *
 */
void configfile_array_fixup(confignode* node) {
	if (!node || node->type != CONFIGNODE_TYPE_ARRAY) return;
	size_t index = 0;
	list* p;
	if ((p = list_first(node->items))) do {
		LIST_DATA_DECLARE(n, p, confignode*);
		if (!n || n->parent != node) continue;
		n->index = index++;
	} while ((p = p->next));
}

/**
 * @brief Insert a child node at a specific index in an array-type config node.
 *
 * @param node the array-type node to insert into
 * @param index the index position to insert at
 * @param sub the child node to insert
 * @return true on success, false if parameters are invalid or operation failed
 *
 */
bool confignode_array_insert(confignode* node, size_t index, confignode* sub) {
	if (!node || !sub || sub->parent) return false;
	if (node->type != CONFIGNODE_TYPE_ARRAY) return false;
	list* p;
	int status = -1;
	if (index == 0) status = list_obj_insert_new(&node->items, sub);
	else if ((p = list_first(node->items))) do {
		LIST_DATA_DECLARE(n, p, confignode*);
		if (!n || n->parent != node || n->index != index) continue;
		status = list_insert_new(p, sub);
		break;
	} while ((p = p->next));
	if (status != 0) return false;
	sub->parent = node;
	configfile_array_fixup(node);
	return true;
}

/**
 * @brief Extend an array-type config node to ensure it has at least the specified
 * length. New null-type nodes will be created to fill any gaps.
 *
 * @param node the array-type node to extend
 * @param nlen the minimum length the array should have
 * @return true on success, false if parameters are invalid or operation failed
 *
 */
bool confignode_array_extend(confignode* node, size_t nlen) {
	if (!node || node->type != CONFIGNODE_TYPE_ARRAY) return false;
	size_t len = confignode_array_len(node);
	while (nlen > len) {
		for (size_t i = len; i < nlen; i++)
			if (!confignode_array_append(node, confignode_new()))
				return false;
		len = confignode_array_len(node);
	}
	configfile_array_fixup(node);
	return true;
}

/**
 * @brief Set a child node at a specific index in an array-type config node.
 * If a node already exists at the index, it will be replaced.
 *
 * @param node the array-type node to set child in
 * @param index the index position to set
 * @param sub the child node to set
 * @return true on success, false if parameters are invalid or operation failed
 *
 */
bool confignode_array_set(confignode* node, size_t index, confignode* sub) {
	if (!node || !sub || sub->parent) return false;
	if (node->type != CONFIGNODE_TYPE_ARRAY) return false;
	if (!confignode_array_extend(node, index + 1)) return false;
	if (index == confignode_array_len(node))
		return confignode_array_append(node, sub);
	confignode* n = confignode_array_get(node, index);
	if (n) return confignode_replace(&n, sub);
	return false;
}
