#include "yaml.h"
#include "internal.h"
#include "str-utils.h"

static confignode* parse_yaml_node(yaml_parser_t* parser, yaml_event_t* event) {
	switch (event->type) {
		case YAML_SCALAR_EVENT: {
			char* value = (char*) event->data.scalar.value;
			char* tag = (char*) event->data.scalar.tag;
			if (tag && strcmp(tag, "tag:yaml.org,2002:bool") == 0) {
				return confignode_new_bool(
					string_is_true(value));
			} else if (tag && strcmp(tag, "tag:yaml.org,2002:int") == 0) {
				return confignode_new_int(
					strtoll(value, NULL, 10));
			} else if (tag && strcmp(tag, "tag:yaml.org,2002:float") == 0) {
				return confignode_new_float(strtod(value, NULL));
			} else if (tag && strcmp(tag, "tag:yaml.org,2002:null") == 0) {
				return confignode_new();
			} else if (
				strcmp(value, "null") == 0 ||
				strcmp(value, "~") == 0 ||
				strlen(value) == 0
			) {
				return confignode_new();
			} else if (string_is_true(value) || string_is_false(value)) {
				return confignode_new_bool(string_is_true(value));
			} else {
				char* end = NULL;
				long long int_val = strtoll(value, &end, 10);
				if (end && *end == 0)
					return confignode_new_int(int_val);
				double float_val = strtod(value, &end);
				if (end && *end == 0)
					return confignode_new_float(float_val);
				return confignode_new_string(value);
			}
		}
		case YAML_SEQUENCE_START_EVENT: {
			confignode* array = confignode_new_array();
			if (!array) return NULL;
			yaml_event_t child_event;
			while (true) {
				if (!yaml_parser_parse(parser, &child_event)) {
					confignode_clean(array);
					return NULL;
				}
				if (child_event.type == YAML_SEQUENCE_END_EVENT) {
					yaml_event_delete(&child_event);
					break;
				}
				confignode* child = parse_yaml_node(parser, &child_event);
				yaml_event_delete(&child_event);
				if (!child) {
					confignode_clean(array);
					return NULL;
				}
				if (!confignode_array_append(array, child)) {
					confignode_clean(child);
					confignode_clean(array);
					return NULL;
				}
			}
			return array;
		}
		case YAML_MAPPING_START_EVENT: {
			confignode* map = confignode_new_map();
			if (!map) return NULL;
			while (true) {
				yaml_event_t key_event;
				if (!yaml_parser_parse(parser, &key_event)) {
					confignode_clean(map);
					return NULL;
				}
				if (key_event.type == YAML_MAPPING_END_EVENT) {
					yaml_event_delete(&key_event);
					break;
				}
				if (key_event.type != YAML_SCALAR_EVENT) {
					yaml_event_delete(&key_event);
					confignode_clean(map);
					return NULL;
				}
				char* key = strdup((char*) key_event.data.scalar.value);
				yaml_event_delete(&key_event);
				if (!key) {
					confignode_clean(map);
					return NULL;
				}
				yaml_event_t value_event;
				if (!yaml_parser_parse(parser, &value_event)) {
					free(key);
					confignode_clean(map);
					return NULL;
				}
				confignode* value = parse_yaml_node(parser, &value_event);
				yaml_event_delete(&value_event);
				if (!value) {
					free(key);
					confignode_clean(map);
					return NULL;
				}
				if (!confignode_map_set(map, key, value)) {
					free(key);
					confignode_clean(value);
					confignode_clean(map);
					return NULL;
				}
				free(key);
			}
			return map;
		}
		default:
			return NULL;
	}
}

/**
 * @brief Load a confignode tree from a YAML string.
 * Parses the YAML string using libyaml and converts it to a confignode tree
 * structure. Supports all standard YAML data types including scalars,
 * sequences, and mappings.
 *
 * @param buff the YAML string to parse (must be null-terminated)
 * @return newly allocated confignode tree, or NULL on parse error or allocation
 * failure
 *
 */
confignode* configfile_yaml_load_string(const char* buff) {
	if (!buff) return NULL;
	yaml_parser_t parser;
	yaml_event_t event;
	confignode* root = NULL;
	if (!yaml_parser_initialize(&parser)) return NULL;
	yaml_parser_set_input_string(&parser, (const uint8_t*) buff, strlen(buff));
	if (!yaml_parser_parse(&parser, &event)) goto error;
	if (event.type != YAML_STREAM_START_EVENT) {
		yaml_event_delete(&event);
		goto error;
	}
	yaml_event_delete(&event);
	if (!yaml_parser_parse(&parser, &event)) goto error;
	if (event.type == YAML_STREAM_END_EVENT) {
		yaml_event_delete(&event);
		yaml_parser_delete(&parser);
		return confignode_new();
	}
	if (event.type != YAML_DOCUMENT_START_EVENT) {
		yaml_event_delete(&event);
		goto error;
	}
	yaml_event_delete(&event);
	while (true) {
		if (!yaml_parser_parse(&parser, &event)) goto error;
		if (event.type == YAML_DOCUMENT_END_EVENT) {
			yaml_event_delete(&event);
			break;
		}
		root = parse_yaml_node(&parser, &event);
		yaml_event_delete(&event);
		if (!root) goto error;
	}
	if (!yaml_parser_parse(&parser, &event)) goto error;
	if (event.type != YAML_STREAM_END_EVENT) {
		yaml_event_delete(&event);
		goto error;
	}
	yaml_event_delete(&event);
	yaml_parser_delete(&parser);
	return root;
error:
	if (root) confignode_clean(root);
	yaml_parser_delete(&parser);
	return NULL;
}

static bool emit_yaml_node(yaml_emitter_t* emitter, confignode* node) {
	list* p;
	if (!node) return false;
	yaml_event_t event;
	switch (node->type) {
		case CONFIGNODE_TYPE_NULL:
			yaml_scalar_event_initialize(
				&event, NULL, NULL,
				(yaml_char_t*) "null", 4,
				1, 0, YAML_PLAIN_SCALAR_STYLE
			);
			if (!yaml_emitter_emit(emitter, &event)) return false;
			break;
		case CONFIGNODE_TYPE_VALUE: switch (node->value.type) {
			case VALUE_STRING: {
				const char* str = node->value.v.s ?: "";
				yaml_scalar_event_initialize(
					&event, NULL, NULL,
					(yaml_char_t*) str, strlen(str),
					1, 1, YAML_PLAIN_SCALAR_STYLE
				);
				if (!yaml_emitter_emit(emitter, &event))
					return false;
				break;
			}
			case VALUE_INT: {
				char buf[32];
				snprintf(
					buf, sizeof(buf),
					"%" PRId64,
					node->value.v.i
				);
				yaml_scalar_event_initialize(
					&event, NULL, NULL,
					(yaml_char_t*) buf, strlen(buf),
					1, 0, YAML_PLAIN_SCALAR_STYLE
				);
				if (!yaml_emitter_emit(emitter, &event))
					return false;
				break;
			}
			case VALUE_FLOAT: {
				char buf[32];
				snprintf(
					buf, sizeof(buf),
					"%lf",
					node->value.v.f
				);
				yaml_scalar_event_initialize(
					&event, NULL, NULL,
					(yaml_char_t*) buf, strlen(buf),
					1, 0, YAML_PLAIN_SCALAR_STYLE
				);
				if (!yaml_emitter_emit(emitter, &event))
					return false;
				break;
			}
			case VALUE_BOOL: {
				const char* bool_str = node->value.v.b ? "true" : "false";
				yaml_scalar_event_initialize(
					&event, NULL, NULL,
					(yaml_char_t*) bool_str, strlen(bool_str),
					1, 0, YAML_PLAIN_SCALAR_STYLE
				);
				if (!yaml_emitter_emit(emitter, &event))
					return false;
				break;
			}
			default:
				return false;
		}break;
		case CONFIGNODE_TYPE_ARRAY:
			yaml_sequence_start_event_initialize(
				&event, NULL, NULL, 1, YAML_BLOCK_SEQUENCE_STYLE
			);
			if (!yaml_emitter_emit(emitter, &event)) return false;
			if ((p = list_first(node->items))) do {
				LIST_DATA_DECLARE(child, p, confignode*);
				if (child && !emit_yaml_node(emitter, child))
					return false;
			} while ((p = p->next));
			yaml_sequence_end_event_initialize(&event);
			if (!yaml_emitter_emit(emitter, &event)) return false;
			break;
		case CONFIGNODE_TYPE_MAP:
			yaml_mapping_start_event_initialize(
				&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE
			);
			if (!yaml_emitter_emit(emitter, &event)) return false;
			if ((p = list_first(node->items))) do {
				LIST_DATA_DECLARE(
					child, p, confignode*);
				if (child && child->key) {
					yaml_scalar_event_initialize(
						&event, NULL, NULL,
						(yaml_char_t*) child->key,
						strlen(child->key),
						1, 1, YAML_PLAIN_SCALAR_STYLE
					);
					if (!yaml_emitter_emit(emitter, &event))
						return false;
					if (!emit_yaml_node(emitter, child))
						return false;
				}
			} while ((p = p->next));
			yaml_mapping_end_event_initialize(&event);
			if (!yaml_emitter_emit(emitter, &event)) return false;
			break;
		default:
			return false;
	}
	return true;
}

/**
 * @brief Save a confignode tree to a YAML string.
 * Converts the confignode tree to YAML format using libyaml and returns it as a
 * string. Supports all confignode types including values, arrays, and maps.
 *
 * @param node the confignode tree to convert (can be NULL)
 * @return newly allocated YAML string that must be freed by caller, or NULL on
 * failure
 *
 */
char* configfile_yaml_save_string(confignode* node) {
	if (!node) return NULL;
	yaml_emitter_t emitter;
	yaml_event_t event;
	unsigned char output[1024 * 10];
	size_t size_written = 0;
	if (!yaml_emitter_initialize(&emitter)) return NULL;
	yaml_emitter_set_output_string(
		&emitter, output, sizeof(output), &size_written
	);
	yaml_emitter_set_encoding(&emitter, YAML_UTF8_ENCODING);
	yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
	if (!yaml_emitter_emit(&emitter, &event)) goto error;
	yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
	if (!yaml_emitter_emit(&emitter, &event)) goto error;
	if (!emit_yaml_node(&emitter, node)) goto error;
	yaml_document_end_event_initialize(&event, 1);
	if (!yaml_emitter_emit(&emitter, &event)) goto error;
	yaml_stream_end_event_initialize(&event);
	if (!yaml_emitter_emit(&emitter, &event)) goto error;
	yaml_emitter_delete(&emitter);
	if (size_written > 0) {
		char* result = malloc(size_written + 1);
		if (result) {
			memcpy(result, output, size_written);
			result[size_written] = 0;
			return result;
		}
	}
	return NULL;
error:
	yaml_emitter_delete(&emitter);
	return NULL;
}
