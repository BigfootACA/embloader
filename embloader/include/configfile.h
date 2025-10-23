/**
 * @file configfile.h
 * @brief Configuration file parsing and manipulation library
 *
 * Provides hierarchical configuration tree data structures and operations
 * for parsing, manipulating, and serializing configuration data in multiple
 * formats (JSON, YAML). Supports path-based access and type-safe operations.
 */

#ifndef CONFIGFILE_H
#define CONFIGFILE_H
#include <Protocol/SimpleFileSystem.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "list.h"

typedef struct confignode confignode;

/** Supported configuration file formats */
typedef enum configfile_type {
	CONFIGFILE_TYPE_UNKNOWN = 0,
	CONFIGFILE_TYPE_JSON,
	CONFIGFILE_TYPE_YAML,
	CONFIGFILE_TYPE_CONF,
} configfile_type;

/** Types of configuration nodes */
typedef enum confignode_type {
	CONFIGNODE_TYPE_NULL = 0, ///< Empty/null node
	CONFIGNODE_TYPE_VALUE,    ///< Leaf node containing a value
	CONFIGNODE_TYPE_ARRAY,    ///< Array container node
	CONFIGNODE_TYPE_MAP,      ///< Map/dictionary container node
} confignode_type;

/** Value types for VALUE nodes */
typedef enum value_type {
	VALUE_STRING, ///< String value
	VALUE_INT,    ///< 64-bit integer value
	VALUE_FLOAT,  ///< Double precision floating point value
	VALUE_BOOL,   ///< Boolean value
} value_type;

/** Value container for configuration node values */
typedef struct confignode_value {
	value_type type; ///< Type of the stored value
	union {
		char* s;   ///< String value (dynamically allocated)
		int64_t i; ///< Integer value
		double f;  ///< Float value
		bool b;    ///< Boolean value
	} v;
	size_t len;        ///< Length of the value
} confignode_value;

/** Iterator structure for traversing child nodes of MAP or ARRAY nodes */
typedef struct confignode_iter {
	void* cur;        ///< Internal iterator state (list position)
	confignode* root; ///< Root node being iterated over
	confignode* node; ///< Current node in iteration (NULL when done)
} confignode_iter;

// Configuration file I/O functions

/** Load configuration from string buffer */
extern confignode* configfile_load_string(
	configfile_type type,
	const char* buff
);

/** Save configuration to string buffer (caller must free result) */
extern char* configfile_save_string(configfile_type type, confignode* node);

/** Load configuration from EFI file */
extern confignode* configfile_load_efi_file(configfile_type type, EFI_FILE_PROTOCOL* file);

/** Load configuration from EFI file at specified path */
confignode* configfile_load_efi_file_path(configfile_type type, EFI_FILE_PROTOCOL* base, const char* path);

/** Print configuration tree using custom print function */
extern void confignode_print(confignode* node, int (*print)(const char*));

/** Guess configuration file type from filename extension */
extern configfile_type configfile_type_from_ext(const char* ext);

/** Guess configuration file type from filename */
extern configfile_type configfile_type_guess(const char* filename);

// Node type and navigation functions

/** Get the type of a configuration node */
extern confignode_type confignode_get_type(confignode* node);

/** Check if node is of specified type */
extern bool confignode_is_type(confignode* node, confignode_type type);

/** Get key name of node (for MAP children) */
extern const char* confignode_get_key(confignode* node);

/** Get array index of node (for ARRAY children) */
extern int64_t confignode_get_index(confignode* node);

/** Check if node is empty */
extern bool confignode_is_empty(confignode* node);

/** Get parent node (NULL if root node) */
extern confignode* confignode_parent(confignode* node);

/** Get root node of the tree */
extern confignode* confignode_root(confignode* node);

// Map operations

/** Get child node by key from a MAP node */
extern confignode* confignode_map_get(confignode* node, const char* key);

/** Set/add child node with key in a MAP node */
extern bool confignode_map_set(confignode* node, const char* key, confignode* sub);

// Array operations

/** Get child node by index from an ARRAY node */
extern confignode* confignode_array_get(confignode* node, size_t index);

/** Set child node at index in an ARRAY node (extends array if needed) */
extern bool confignode_array_set(confignode* node, size_t index, confignode* sub);

/** Append child node to end of ARRAY node */
extern bool confignode_array_append(confignode* node, confignode* sub);

/** Insert child node at specific index in ARRAY node */
extern bool confignode_array_insert(confignode* node, size_t index, confignode* sub);

/** Extend ARRAY node to minimum length (fills with NULL nodes) */
extern bool confignode_array_extend(confignode* node, size_t nlen);

/** Get number of elements in ARRAY node */
extern size_t confignode_array_len(confignode* node);

// Value operations

/** Get value from VALUE node (caller must free string values) */
extern bool confignode_value_get(confignode* node, confignode_value* out);

/** Get string representation of value (with default and success flag) */
extern char* confignode_value_get_string(confignode* node, const char* def, bool* ok);

/** Get list of strings from VALUE node or ARRAY/MAP of VALUE nodes */
extern list* confignode_value_get_string_or_list_to_list(confignode* node, bool* ok);

/** Get concatenated string from VALUE node or ARRAY/MAP of VALUE nodes */
extern char* confignode_value_get_string_or_list(confignode* node, char* sep, bool* ok);

/** Get integer value with type conversion and default */
extern int64_t confignode_value_get_int(confignode* node, int64_t def, bool* ok);

/** Get float value with type conversion and default */
extern double confignode_value_get_float(confignode* node, double def, bool* ok);

/** Get boolean value with type conversion and default */
extern bool confignode_value_get_bool(confignode* node, bool def, bool* ok);

/** Set value in VALUE node */
extern bool confignode_value_set(
	confignode* node,
	const confignode_value* value
);

/** Set string value (convenience function) */
extern bool confignode_value_set_string(confignode* node, const char* s);

/** Set formatted string value (convenience function) */
extern bool confignode_value_set_string_fmt(confignode* node, const char* fmt, ...);

/** Set integer value (convenience function) */
extern bool confignode_value_set_int(confignode* node, int64_t i);

/** Set float value (convenience function) */
extern bool confignode_value_set_float(confignode* node, double f);

/** Set boolean value (convenience function) */
extern bool confignode_value_set_bool(confignode* node, bool b);

// Node creation functions

/** Create new NULL-type node */
extern confignode* confignode_new();

/** Create new MAP-type node */
extern confignode* confignode_new_map();

/** Create new ARRAY-type node */
extern confignode* confignode_new_array();

/** Create new VALUE-type node with specified value */
extern confignode* confignode_new_value(const confignode_value* value);

/** Create new VALUE-type node with string value (convenience) */
extern confignode* confignode_new_string(const char* s);

/** Create new VALUE-type node with formatted string value (convenience) */
extern confignode* confignode_new_string_fmt(const char* s, ...);

/** Create new VALUE-type node with integer value (convenience) */
extern confignode* confignode_new_int(int64_t i);

/** Create new VALUE-type node with float value (convenience) */
extern confignode* confignode_new_float(double f);

/** Create new VALUE-type node with boolean value (convenience) */
extern confignode* confignode_new_bool(bool b);

// Tree manipulation functions

/** Recursively free node and all children */
extern void confignode_clean(confignode* node);

/** Create deep copy of node tree */
extern confignode* confignode_copy(confignode* node);

/** Merge source tree into target tree */
extern bool confignode_merge(confignode* node, confignode* new);

/** Replace node with new node (updates parent references) */
extern bool confignode_replace(confignode** node, confignode* new);

// Path-based access functions

/** Look up node by path string (e.g. "root.array[0].value") */
extern confignode* confignode_path_lookup(confignode* node, const char* path, bool create);

/** Get path string for node location in tree */
extern bool confignode_path_get(confignode* node, char* buff, size_t size);

/** Set value at path (creates intermediate nodes as needed) */
extern bool confignode_path_set_value(
	confignode* node,
	const char* path,
	const confignode_value* value
);

/** Set string value at path (convenience function) */
extern bool confignode_path_set_string(
	confignode* node,
	const char* path,
	const char* s
);

/** Set formatted string value at path (convenience function) */
extern bool confignode_path_set_string_fmt(
	confignode* node,
	const char* path,
	const char* fmt,
	...
);

/** Set integer value at path (convenience function) */
extern bool confignode_path_set_int(confignode* node, const char* path, int64_t i);

/** Set float value at path (convenience function) */
extern bool confignode_path_set_float(confignode* node, const char* path, double f);

/** Set boolean value at path (convenience function) */
extern bool confignode_path_set_bool(confignode* node, const char* path, bool b);

/** Get value at path (copies value to output structure) */
extern bool confignode_path_get_value(
	confignode* node,
	const char* path,
	confignode_value* value
);

/** Get string value at path with default fallback */
extern char* confignode_path_get_string(
	confignode* node,
	const char* path,
	const char* def,
	bool* ok
);

/** Get list of strings from VALUE node or ARRAY/MAP of VALUE nodes at path */
extern list* confignode_path_get_string_or_list_to_list(
	confignode* node,
	const char* path,
	bool* ok
);

/** Get concatenated string from VALUE node or ARRAY/MAP of VALUE nodes at path */
extern char* confignode_path_get_string_or_list(
	confignode* node,
	const char* path,
	char* sep,
	bool* ok
);

/** Get integer value at path with default fallback */
extern int64_t confignode_path_get_int(
	confignode* node,
	const char* path,
	int64_t def,
	bool* ok
);

/** Get float value at path with default fallback */
extern double confignode_path_get_float(
	confignode* node,
	const char* path,
	double def,
	bool* ok
);

/** Get boolean value at path with default fallback */
extern bool confignode_path_get_bool(
	confignode* node,
	const char* path,
	bool def,
	bool* ok
);

/** Check if node at path is of specified type */
extern bool confignode_path_is_type(
	confignode* node,
	const char* path,
	confignode_type type
);

/** Append node to array at path (creates array if needed) */
extern bool confignode_path_array_append(
	confignode* node,
	const char* path,
	confignode* sub
);

/** Set node at array index via path (extends array if needed) */
extern bool confignode_path_array_set(
	confignode* node,
	const char* path,
	size_t index,
	confignode* sub
);

/** Insert node at array index via path (shifts existing elements) */
extern bool confignode_path_array_insert(
	confignode* node,
	const char* path,
	size_t index,
	confignode* sub
);

/** Get array length at path (returns 0 if not array) */
extern size_t confignode_path_array_len(confignode* node, const char* path);

/** Set map entry at path (creates intermediate nodes as needed) */
extern bool confignode_path_map_set(confignode* node, const char* path, confignode* sub);

// Iterator functions

/** Start iteration over child nodes at specified path */
extern bool confignode_path_iter_start(
	confignode_iter* iter,
	confignode* node,
	const char* path
);

/** Start iteration over child nodes of MAP or ARRAY node */
extern bool confignode_iter_start(confignode_iter* iter, confignode* node);

/** Advance iterator to next child node */
extern bool confignode_iter_next(confignode_iter* iter);

/** Reset iterator to beginning of collection */
extern void confignode_iter_reset(confignode_iter* iter);

// Iterator convenience macros

/** Iterate over all child nodes of a MAP or ARRAY node */
#define confignode_foreach(_iter, _node) \
	confignode_iter _iter; \
	for (confignode_iter_start(&_iter, _node); \
		confignode_iter_next(&_iter);)

/** Iterate over all child nodes at the specified path */
#define confignode_path_foreach(_iter, _node, _path) \
	confignode_iter _iter; \
	for (confignode_path_iter_start(&_iter, _node, _path); \
		confignode_iter_next(&_iter);)

#endif
