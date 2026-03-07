#include "internal.h"

/**
 * @brief Submit a log message as a pre-formatted string.
 * Creates a log item, appends it to the log store, and triggers a fast flush
 * to all backends.
 *
 * @param level    log severity level
 * @param tag      log tag string (may be NULL)
 * @param file     source file name (may be NULL)
 * @param function source function name (may be NULL)
 * @param lineno   source line number
 * @param content  log message content (must not be NULL)
 */
void log_base_print(
	log_level level, 
	const char *tag,
	const char *file,
	const char *function,
	int lineno,
	const char *content
) {
	log_item *item = NULL;
	if (!content) return;
	if (!(item = log_item_create(level, tag, file, function, lineno, content))) goto fail;
	if (!log_append(item)) goto fail;
	log_flush_fast();
	return;
fail:
	if (item) free(item);
}

/**
 * @brief Submit a formatted log message using a va_list.
 * Formats the message with vasprintf, then delegates to log_base_print.
 *
 * @param level    log severity level
 * @param tag      log tag string (may be NULL)
 * @param file     source file name (may be NULL)
 * @param function source function name (may be NULL)
 * @param lineno   source line number
 * @param fmt      printf-style format string
 * @param args     va_list of arguments for the format string
 */
void log_base_vprintf(
	log_level level, 
	const char *tag,
	const char *file,
	const char *function,
	int lineno,
	const char *fmt,
	va_list args
) {
	char *ptr = NULL;
	if (vasprintf(&ptr, fmt, args) < 0) return;
	log_base_print(level, tag, file, function, lineno, ptr);
	free(ptr);
}

/**
 * @brief Submit a formatted log message with variadic arguments.
 * Convenience wrapper around log_base_vprintf.
 *
 * @param level    log severity level
 * @param tag      log tag string (may be NULL)
 * @param file     source file name (may be NULL)
 * @param function source function name (may be NULL)
 * @param lineno   source line number
 * @param fmt      printf-style format string
 * @param ...      arguments for the format string
 */
void log_base_printf(
	log_level level,
	const char *tag,
	const char *file,
	const char *function,
	int lineno,
	const char *fmt,
	...
) {
	va_list args;
	va_start(args, fmt);
	log_base_vprintf(level, tag, file, function, lineno, fmt, args);
	va_end(args);
}
