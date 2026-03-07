#ifndef LOG_H
#define LOG_H
#include <stdarg.h>

typedef enum log_level {
	LOG_DEBUG,
	LOG_INFO,
	LOG_WARNING,
	LOG_ERROR
} log_level;

extern void log_init();
extern void log_base_print(
	log_level level, 
	const char *tag,
	const char *file,
	const char *function,
	int lineno,
	const char *content
);
	
extern void log_base_vprintf(
	log_level level, 
	const char *tag,
	const char *file,
	const char *function,
	int lineno,
	const char *fmt,
	va_list args
);
extern void log_base_printf(
	log_level level,
	const char *tag,
	const char *file,
	const char *function,
	int lineno,
	const char *fmt,
	...
);

#ifndef LOG_TAG
#define LOG_TAG __func__
#endif

#define log_print(level, tag, str) log_base_print(level, tag, __FILE__, __func__, __LINE__, str)
#define log_vprintf(level, tag, fmt, args) log_base_vprintf(level, tag, __FILE__, __func__, __LINE__, fmt, args)
#define log_printf(level, tag, ...) log_base_printf(level, tag, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_printf(LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define log_info(...) log_printf(LOG_INFO, LOG_TAG, __VA_ARGS__)
#define log_warning(...) log_printf(LOG_WARNING, LOG_TAG, __VA_ARGS__)
#define log_error(...) log_printf(LOG_ERROR, LOG_TAG, __VA_ARGS__)

#endif
