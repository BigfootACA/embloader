#ifndef LOG_H
#define LOG_H
#include <stdarg.h>

typedef enum log_level {
	LOG_DEBUG,
	LOG_INFO,
	LOG_WARNING,
	LOG_ERROR
} log_level;

extern void log_print(log_level level, const char *tag, const char *str);
extern void log_vprintf(log_level level, const char *tag, const char *fmt, va_list args);
extern void log_printf(log_level level, const char *tag, const char *fmt, ...);

#ifndef LOG_TAG
#define LOG_TAG __func__
#endif

#define log_debug(...) log_printf(LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define log_info(...) log_printf(LOG_INFO, LOG_TAG, __VA_ARGS__)
#define log_warning(...) log_printf(LOG_WARNING, LOG_TAG, __VA_ARGS__)
#define log_error(...) log_printf(LOG_ERROR, LOG_TAG, __VA_ARGS__)

#endif
