#include "internal.h"

/**
 * @brief Start iteration over child nodes at a specific path.
 * This is a convenience function that combines path lookup with iterator
 * initialization. Only works for MAP and ARRAY nodes that have child elements
 * to iterate over.
 *
 * @param iter the iterator structure to initialize
 * @param node the root node to start path lookup from
 * @param path the path string pointing to the node to iterate over
 * @return true if iteration started successfully, false if path not found or
 * node type invalid
 *
 */
bool confignode_path_iter_start(
	confignode_iter* iter,
	confignode* node,
	const char* path
) {
	if (!iter || !node) return false;
	memset(iter, 0, sizeof(confignode_iter));
	confignode* n = confignode_path_lookup(node, path, false);
	if (!n) return false;
	return confignode_iter_start(iter, n);
}

/**
 * @brief Start iteration at a path and return an iterator by value.
 * This is a convenience function that combines path lookup with iterator
 * initialization and returns the iterator by value. Useful for inline iterator
 * initialization in for loops or variable declarations. Only works for MAP and
 * ARRAY nodes at the specified path.
 *
 * @param node the root node to start path lookup from
 * @param path the path string pointing to the node to iterate over
 * @return initialized iterator structure positioned at the start
 *
 */
confignode_iter confignode_path_iter_start_ret(
	confignode* node,
	const char* path
) {
	confignode_iter iter = {};
	confignode_path_iter_start(&iter, node, path);
	return iter;
}

/**
 * @brief Start iteration over child nodes of a MAP or ARRAY node.
 * Initializes the iterator and positions it at the first child node.
 * Only MAP and ARRAY nodes can be iterated over as they contain child elements.
 *
 * @param iter the iterator structure to initialize
 * @param node the MAP or ARRAY node to iterate over
 * @return true if iteration started successfully and first element found, false
 * otherwise
 *
 */
bool confignode_iter_start(confignode_iter* iter, confignode* node) {
	if (!iter || !node) return false;
	if (
		node->type != CONFIGNODE_TYPE_MAP &&
		node->type != CONFIGNODE_TYPE_ARRAY
	) return false;
	iter->root = node, iter->cur = NULL, iter->node = NULL;
	iter->index = -1, iter->name = NULL;
	return confignode_iter_next(iter);
}

/**
 * @brief Start iteration and return an iterator by value.
 * This is a convenience function that initializes an iterator structure and
 * returns it by value. Useful for inline iterator initialization in for loops
 * or variable declarations. Only works for MAP and ARRAY nodes.
 *
 * @param node the MAP or ARRAY node to iterate over
 * @return initialized iterator structure positioned at the start
 *
 */
confignode_iter confignode_iter_start_ret(confignode* node) {
	confignode_iter iter = {};
	confignode_iter_start(&iter, node);
	return iter;
}

/**
 * @brief Advance the iterator to the next child node.
 * Moves the iterator to the next valid child node in the collection.
 * Skips over any invalid or orphaned nodes in the internal list structure.
 *
 * @param iter the iterator to advance
 * @return true if next element found and iterator positioned, false if no more
 * elements
 *
 */
bool confignode_iter_next(confignode_iter* iter) {
	if (!iter || !iter->root) return false;
	do {
		list* next = iter->cur ?
			((list*) iter->cur)->next :
			list_first(iter->root->items);
		if (!next) goto end;
		iter->cur = next;
		iter->node = LIST_DATA(iter->cur, confignode*);
	} while (!iter->node);
	if (!iter->node) goto end;
	iter->index++;
	iter->name = iter->node->key ? iter->node->key : NULL;
	return true;
end:
	iter->index = -1;
	iter->name = NULL;
	iter->node = NULL;
	return false;
}

/**
 * @brief Reset the iterator to the beginning of the collection.
 * Resets the iterator state and positions it at the first child node again.
 * This allows restarting iteration without reinitializing the iterator.
 *
 * @param iter the iterator to reset
 *
 */
void confignode_iter_reset(confignode_iter* iter) {
	if (!iter || !iter->root) return;
	iter->cur = NULL, iter->node = NULL;
}
