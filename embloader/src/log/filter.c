#include "internal.h"
#include "regexp.h"

static log_level confignode_path_get_log_level(
	confignode* node,
	const char* path,
	log_level def,
	bool* ok
) {
	bool vok = false;
	if (ok) *ok = false;
	if (!node || !path) return def;
	char *str = confignode_path_get_string(node, path, NULL, &vok);
	if (!str) return def;
	log_level level;
	if (!log_level_from_str(str, &level)) {
		char buff[256];
		confignode_path_get(node, buff, sizeof(buff));
		log_warning(
			"invalid log level string '%s' at path '%s%s%s'",
			str, buff, buff[0] ? "." : "", path
		);
		level = def;
	}
	if (ok) *ok = vok;
	free(str);
	return level;
}

static bool log_check_regex_filter(const char *value, confignode *config) {
	if (!value) return true;
	if (confignode_is_type(config, CONFIGNODE_TYPE_ARRAY)){
		confignode_foreach(iter, config) {
			if (!iter.node) continue;
			if (!log_check_regex_filter(value, iter.node)) return false;
		}
		return true;
	}
	if (!confignode_is_type(config, CONFIGNODE_TYPE_VALUE)) return true;
	char *pattern = confignode_value_get_string(config, NULL, NULL);
	if (!pattern) return true;
	int ret = regexp_match(pattern, value, 0);
	free(pattern);
	return ret != 1;
}

static bool log_check_regex_filter_path(const char *value, confignode* config, const char* path) {
	confignode* n = confignode_path_lookup(config, path, false);
	if (!n) return true;
	return log_check_regex_filter(value, n);
}

/**
 * @brief Check whether a log item passes the filter criteria of a config node.
 * Evaluates min-level, max-level, and regex filters on tag, file, function,
 * content, and line fields. If no config is provided, the item passes.
 *
 * @param item   the log item to check (must not be NULL)
 * @param config configuration node containing filter rules (may be NULL)
 * @return true if the item passes all filters, false otherwise
 */
bool log_check_filter(log_item *item, confignode *config) {
	bool ok = false;
	log_level level = LOG_DEBUG;
	if (!item) return false;
	if (!config) return true;
	level = confignode_path_get_log_level(config, "min-level", LOG_DEBUG, &ok);
	if (ok && level > item->level) return false;
	level = confignode_path_get_log_level(config, "max-level", LOG_ERROR, &ok);
	if (ok && level < item->level) return false;
	if (!log_check_regex_filter_path(item->tag, config, "tag")) return false;
	if (!log_check_regex_filter_path(item->file, config, "file")) return false;
	if (!log_check_regex_filter_path(item->function, config, "function")) return false;
	if (!log_check_regex_filter_path(item->content, config, "content")) return false;
	if (confignode_path_lookup(config, "line", false)) {
		char buff[1024];
		memset(buff, 0, sizeof(buff));
		snprintf(buff, sizeof(buff) - 1, "%s:%d", item->file, item->lineno);
		if (!log_check_regex_filter(buff, confignode_path_lookup(config, "line", false)))
			return false;
	}
	return true;
}
