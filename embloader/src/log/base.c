#include "internal.h"
#include "embloader.h"
#include <Library/UefiRuntimeServicesTableLib.h>

/**
 * @brief Convert a log level enum value to its string representation.
 *
 * @param level the log level to convert
 * @return static string
 */
const char *log_level_str(log_level level) {
	switch (level) {
		case LOG_DEBUG: return "DEBUG";
		case LOG_INFO: return "INFO";
		case LOG_WARNING: return "WARNING";
		case LOG_ERROR: return "ERROR";
		default: return "UNKNOWN";
	}
}

/**
 * @brief Parse a log level from a string representation.
 * Accepts case-insensitive names and numeric values.
 *
 * @param str   the string to parse
 * @param level pointer to store the parsed log level
 * @return true if the string was recognized and level was set, false otherwise
 */
bool log_level_from_str(const char *str, log_level *level) {
	if (!str || !level) return false;
	if (strcasecmp(str, "debug") == 0) *level = LOG_DEBUG;
	else if (strcasecmp(str, "dbg") == 0) *level = LOG_DEBUG;
	else if (strcasecmp(str, "info") == 0) *level = LOG_INFO;
	else if (strcasecmp(str, "warn") == 0) *level = LOG_WARNING;
	else if (strcasecmp(str, "warning") == 0) *level = LOG_WARNING;
	else if (strcasecmp(str, "err") == 0) *level = LOG_ERROR;
	else if (strcasecmp(str, "error") == 0) *level = LOG_ERROR;
	else if (strcmp(str, "0") == 0) *level = LOG_DEBUG;
	else if (strcmp(str, "1") == 0) *level = LOG_INFO;
	else if (strcmp(str, "2") == 0) *level = LOG_WARNING;
	else if (strcmp(str, "3") == 0) *level = LOG_ERROR;
	else return false;
	return true;
}

/**
 * @brief Create a new log item with all associated metadata.
 * All string fields (tag, file, function, content) are copied into a single
 * contiguous allocation using a flexible array member, avoiding fragmentation.
 *
 * @param level    log severity level
 * @param tag      log tag string (may be NULL)
 * @param file     source file name (may be NULL)
 * @param function source function name (may be NULL)
 * @param lineno   source line number
 * @param content  log message content (must not be NULL)
 * @return newly allocated log_item (caller must free), or NULL on failure
 */
log_item* log_item_create(
	log_level level,
	const char *tag,
	const char *file,
	const char *function,
	int lineno,
	const char *content
) {
	size_t tag_len = 0, file_len = 0, function_len = 0, content_len = 0;
	size_t total_len, cur_off = 0;
	if (!content) return NULL;
	if (tag) tag_len = strlen(tag);
	if (file) file_len = strlen(file);
	if (function) function_len = strlen(function);
	if (content) content_len = strlen(content);
	total_len = sizeof(log_item) + tag_len + file_len + function_len + content_len + 4;
	if (total_len >= UINT32_MAX) return NULL;
	log_item *item = malloc(total_len);
	if (!item) return NULL;
	memset(item, 0, total_len);
	item->size = total_len;
	item->level = level;
	item->lineno = lineno;
	gRT->GetTime(&item->time, NULL);
	if (tag) {
		item->tag = &item->data[cur_off];
		memcpy(&item->data[cur_off], tag, tag_len);
		cur_off += tag_len + 1;
	}
	if (file) {
		item->file = &item->data[cur_off];
		memcpy(&item->data[cur_off], file, file_len);
		cur_off += file_len + 1;
	}
	if (function) {
		item->function = &item->data[cur_off];
		memcpy(&item->data[cur_off], function, function_len);
		cur_off += function_len + 1;
	}
	if (content) {
		item->content = &item->data[cur_off];
		memcpy(&item->data[cur_off], content, content_len);
		cur_off += content_len + 1;
	}
	return item;
}

/**
 * @brief Initialize the log subsystem.
 * Creates default backends from configuration, sets up log size limits,
 * and flushes any buffered log items to newly created backends.
 */
void log_init() {
	confignode *r = g_embloader.config;
	if (confignode_path_get_bool(r, "log.defaults", true, NULL))
		log_backend_create(&log_backend_stdio, NULL, NULL);
	confignode *backends = confignode_path_lookup(r, "log.backends", false);
	if (backends) log_backends_init(backends);
	log_size_limit = confignode_path_get_int(r, "log.size-limit", log_size_limit, NULL);
	log_info("log system initialized");
}
