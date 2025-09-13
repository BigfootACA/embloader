#ifndef CONFIGFILE_INTERNAL_H
#define CONFIGFILE_INTERNAL_H
#include <stdbool.h>
#include "configfile.h"
#include "list.h"
#include "json.h"
#include "yaml.h"

struct configfile {
	configfile_type type;
	confignode* root;
};

struct confignode {
	confignode* parent;
	char* key;
	ssize_t index;
	confignode_type type;
	list* items;
	confignode_value value;
};

extern void configfile_array_fixup(confignode* node);
extern bool confignode_set_key(confignode* node, const char* key);
extern confignode* confignode_from_json(json_object* obj);
extern json_object* confignode_to_json(confignode* node);
extern confignode* configfile_json_load_string(const char* buff);
extern char* configfile_json_save_string(confignode* node);
extern confignode* configfile_yaml_load_string(const char* buff);
extern char* configfile_yaml_save_string(confignode* node);
#endif
