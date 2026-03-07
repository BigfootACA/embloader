#ifndef LOG_INTERNAL_H
#define LOG_INTERNAL_H
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "list.h"
#include "configfile.h"

typedef struct log_item log_item;
typedef struct log_backend log_backend;
typedef struct log_backend_base log_backend_base;

struct log_item {
	log_level level;
	EFI_TIME time;
	bool log_flushed;
	uint32_t size;
	int lineno;
	const char *tag;
	const char *file;
	const char *function;
	const char *content;
	char data[];
};

struct log_backend_base {
	const char *name;
	int (*init)(log_backend *backend);
	int (*deinit)(log_backend *backend);
	int (*write)(log_backend *backend, log_item *item);
	size_t ctx_size;
};

struct log_backend {
	char *name;
	log_backend_base *base;
	confignode *config;
	void *ctx;
};

extern log_item* log_item_create(
	log_level level,
	const char *tag,
	const char *file,
	const char *function,
	int lineno,
	const char *content
);
extern bool log_append(struct log_item *item);
extern void log_flush_all(bool force);
extern void log_flush_one_to(log_backend *backend, log_item *item, bool force);
extern void log_flush_fast();
extern const char *log_level_str(log_level level);
extern bool log_level_from_str(const char *str, log_level *level);
extern char* log_formatter(log_item *item, const char *format, int crlf);
extern bool log_check_filter(log_item *item, confignode *config);
extern log_backend* log_backend_create(
	log_backend_base *base,
	const char *name,
	confignode *config
);
extern log_backend* log_backend_create_from(confignode *config);
extern void log_backend_destroy(log_backend *backend);
extern int log_backend_write(log_backend *backend, log_item *item);
extern int log_backend_init(log_backend *backend);
extern int log_backend_deinit(log_backend *backend);
extern void log_backends_init(confignode *config);
extern log_backend_base log_backend_stdio;
extern log_backend_base log_backend_file;
extern log_backend_base *log_backend_bases[];
extern list *log_items;
extern list *log_backends;
extern size_t log_size_limit;
extern size_t log_size_cur;

#endif
