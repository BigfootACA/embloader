#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <string.h>
#include "embloader.h"
#include "debugs.h"
#include "regexp.h"
#include "log.h"

static int compare_versions(const char *v1, const char *v2) {
	char *s1 = strdup(v1), *s2 = strdup(v2);
	char *saveptr1, *saveptr2;
	char *token1 = strtok_r(s1, ".", &saveptr1);
	char *token2 = strtok_r(s2, ".", &saveptr2);
	int result = 0;
	while (token1 && token2) {
		int n1 = atoi(token1);
		int n2 = atoi(token2);
		if (n1 != n2) {
			result = (n1 > n2) ? 1 : -1;
			break;
		}
		token1 = strtok_r(NULL, ".", &saveptr1);
		token2 = strtok_r(NULL, ".", &saveptr2);
	}
	if (result == 0) {
		if (token1) result = 1;
		else if (token2) result = -1;
	}
	free(s1);
	free(s2);
	return result;
}

bool embloader_try_match(confignode *node) {
	char *field_value = NULL;
	bool match = false;
	if (!node) return false;
	char buff[256];
	if (confignode_path_get(node, buff, sizeof(buff)))
		log_debug("match node %s", buff);
	if (!confignode_is_type(node, CONFIGNODE_TYPE_MAP)) return false;
	char *field = confignode_path_get_string(node, "field", NULL, NULL);
	char *oper = confignode_path_get_string(node, "oper", NULL, NULL);
	char *value = confignode_path_get_string(node, "value", NULL, NULL);
	if (!field || !oper || !value) {
		log_warning("missing field/oper/value");
		goto fail;
	}
	log_debug("try match: '%s' %s '%s'", field, oper, value);
	if (!g_embloader.sysinfo) goto fail;
	;
	if (!(field_value = confignode_path_get_string(
		g_embloader.sysinfo, field, NULL, NULL
	))){
		log_debug("field '%s' not found", field);
		goto fail;
	}
	log_debug("field '%s' value: '%s'", field, field_value);
	if (strcasecmp(oper, "equals") == 0) {
		match = (strcasecmp(field_value, value) == 0);
	} else if (strcasecmp(oper, "noteq") == 0) {
		match = (strcasecmp(field_value, value) != 0);
	} else if (strcasecmp(oper, "contains") == 0) {
		match = (strcasestr(field_value, value) != NULL);
	} else if (strcasecmp(oper, "regexp") == 0) {
		match = (regexp_match(value, field_value, REG_ICASE) == 1);
	} else if (strcasecmp(oper, "greater") == 0) {
		match = (strcasecmp(field_value, value) > 0);
	} else if (strcasecmp(oper, "less") == 0) {
		match = (strcasecmp(field_value, value) < 0);
	} else if (strcasecmp(oper, "greater-equals") == 0) {
		match = (strcasecmp(field_value, value) >= 0);
	} else if (strcasecmp(oper, "less-equals") == 0) {
		match = (strcasecmp(field_value, value) <= 0);
	} else if (strcasecmp(oper, "older") == 0) {
		match = (compare_versions(field_value, value) < 0);
	} else if (strcasecmp(oper, "newer") == 0) {
		match = (compare_versions(field_value, value) > 0);
	} else log_warning("unknown oper: %s", oper);
	log_debug("match result: %s", match ? "true" : "false");
fail:
	if (field) free(field);
	if (oper) free(oper);
	if (value) free(value);
	if (field_value) free(field_value);
	return match;
}

bool embloader_try_matches(confignode *node) {
	if (!confignode_is_type(node, CONFIGNODE_TYPE_ARRAY)) return false;
	confignode_foreach(sub, node)
		if (!embloader_try_match(sub.node))
			return false;
	return true;
}
