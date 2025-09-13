#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "list.h"

struct log_item {
	log_level level;
	const char *str;
};

static list *log_items = NULL;

const char *log_level_str(log_level level) {
	switch (level) {
		case LOG_DEBUG: return "DEBUG";
		case LOG_INFO: return "INFO";
		case LOG_WARNING: return "WARNING";
		case LOG_ERROR: return "ERROR";
		default: return "UNKNOWN";
	}
}

void log_print(log_level level, const char *tag, const char *str){
	if (!str) return;
	struct log_item item = {
		.level = level,
		.str = str,
	};
	list_obj_add_new_dup(&log_items, &item, sizeof(item));
	const char *level_str = log_level_str(level);
	printf("[%s] %s: %s\n", level_str, tag, str);
}

void log_vprintf(log_level level, const char *tag, const char *fmt, va_list args){
	char *ptr = NULL;
	if (vasprintf(&ptr, fmt, args) < 0) return;
	log_print(level, tag, ptr);
	free(ptr);
}

void log_printf(log_level level, const char *tag, const char *fmt, ...){
	va_list args;
	va_start(args, fmt);
	log_vprintf(level, tag, fmt, args);
	va_end(args);
}
