#include <string.h>
#include "internal.h"

/**
 * @brief Convert a json_object to a confignode tree.
 * Recursively processes all JSON types including objects, arrays, and
 * primitives. Memory management: the returned confignode tree is independent of
 * the json_object.
 *
 * @param obj the json_object to convert (must not be NULL)
 * @return newly allocated confignode tree, or NULL on allocation failure
 *
 */
confignode* confignode_from_json(json_object* obj) {
	confignode* m;
	json_object_iter iter;
	switch (json_object_get_type(obj)) {
		case json_type_null:
			return confignode_new();
		case json_type_object: {
			if (!(m = confignode_new_map())) return NULL;
			json_object_object_foreachC(obj, iter) {
				confignode* sub = confignode_from_json(iter.val);
				if (
					!sub ||
					!confignode_set_key(sub, iter.key) ||
					!confignode_map_set(m, iter.key, sub)
				) {
					confignode_clean(m);
					return NULL;
				}
			}
			return m;
		}
		case json_type_array: {
			if (!(m = confignode_new_array())) return NULL;
			for (size_t i = 0; i < json_object_array_length(obj); i++) {
				json_object* subobj = json_object_array_get_idx(obj, i);
				confignode* sub = confignode_from_json(subobj);
				if (!sub || !confignode_array_append(m, sub)) {
					confignode_clean(m);
					return NULL;
				}
			}
			return m;
		}
		case json_type_boolean:
			return confignode_new_bool(json_object_get_boolean(obj));
		case json_type_double:
			return confignode_new_float(json_object_get_double(obj));
		case json_type_int:
			return confignode_new_int(json_object_get_int64(obj));
		case json_type_string:
			return confignode_new_string(json_object_get_string(obj));
		default:
			return NULL;
	}
}

/**
 * @brief Convert a confignode tree to a json_object.
 * Recursively processes all confignode types and creates corresponding JSON
 * structures. Memory management: caller is responsible for calling
 * json_object_put() on the result.
 *
 * @param node the confignode to convert (can be NULL)
 * @return newly allocated json_object, or NULL on allocation failure or invalid
 * input
 *
 */
json_object* confignode_to_json(confignode* node) {
	if (!node) return NULL;
	switch (node->type) {
		case CONFIGNODE_TYPE_NULL:
			return json_object_new_null();
		case CONFIGNODE_TYPE_VALUE: switch (node->value.type) {
			case VALUE_STRING:
				return json_object_new_string(
					node->value.v.s ? node->value.v.s : ""
				);
			case VALUE_INT:
				return json_object_new_int64(node->value.v.i);
			case VALUE_FLOAT:
				return json_object_new_double(node->value.v.f);
			case VALUE_BOOL:
				return json_object_new_boolean(node->value.v.b);
			default:
				return NULL;
		}
		case CONFIGNODE_TYPE_ARRAY: {
			json_object* arr = json_object_new_array();
			if (!arr) return NULL;
			list* p;
			if ((p = list_first(node->items))) do {
				LIST_DATA_DECLARE(i, p, confignode*);
				if (!i) continue;
				json_object* sub = confignode_to_json(i);
				if (!sub) {
					json_object_put(arr);
					return NULL;
				}
				if (json_object_array_add(arr, sub) != 0) {
					json_object_put(sub);
					json_object_put(arr);
					return NULL;
				}
			} while ((p = p->next));
			return arr;
		}
		case CONFIGNODE_TYPE_MAP: {
			json_object* obj = json_object_new_object();
			if (!obj) return NULL;
			list* p;
			if ((p = list_first(node->items))) do {
				LIST_DATA_DECLARE(i, p, confignode*);
				if (!i || !i->key) continue;
				json_object* sub = confignode_to_json(i);
				if (!sub) {
					json_object_put(obj);
					return NULL;
				}
				if (json_object_object_add(obj, i->key, sub) != 0) {
					json_object_put(sub);
					json_object_put(obj);
					return NULL;
				}
			} while ((p = p->next));
			return obj;
		}
		default:
			return NULL;
	}
}

/**
 * @brief Load a confignode tree from a JSON string.
 * Parses the JSON string and converts it to a confignode tree structure.
 * This is a convenience function that combines json_tokener_parse and
 * confignode_from_json.
 *
 * @param buff the JSON string to parse (must be null-terminated)
 * @return newly allocated confignode tree, or NULL on parse error or allocation
 * failure
 *
 */
confignode* configfile_json_load_string(const char* buff) {
	json_object* obj = json_tokener_parse(buff);
	if (!obj) return NULL;
	confignode* node = confignode_from_json(obj);
	json_object_put(obj);
	return node;
}

/**
 * @brief Save a confignode tree to a JSON string.
 * Converts the confignode tree to JSON format and returns it as a string.
 * This is a convenience function that combines confignode_to_json and
 * json_object_to_json_string.
 *
 * @param node the confignode tree to convert (can be NULL)
 * @return newly allocated JSON string that must be freed by caller, or NULL on
 * failure
 *
 */
char* configfile_json_save_string(confignode* node) {
	json_object* obj = confignode_to_json(node);
	if (!obj) return NULL;
	const char* oj = json_object_to_json_string(obj);
	char* ret = oj ? strdup(oj) : NULL;
	json_object_put(obj);
	return ret;
}
