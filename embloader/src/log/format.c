#include "internal.h"

#define DEFAULT_LOG_FORMAT "[%l] %t: %m"

enum log_format_specifier {
	LOG_FMT_STRING,
	LOG_FMT_UINT,
	LOG_FMT_SINT,
	LOG_FMT_LOG_LEVEL,
};

struct log_formatter {
	char tag;
	bool ref;
	enum log_format_specifier type;
	size_t off;
	size_t len;
	void *data;
};

static struct log_formatter default_formatters[] = {
	{ .tag = '%', .type = LOG_FMT_STRING,    .ref = false,  .data = "%"                                                           },
	{ .tag = 'l', .type = LOG_FMT_LOG_LEVEL, .ref = true,   .off = offsetof(log_item, level),           .len = sizeof(log_level), },
	{ .tag = 't', .type = LOG_FMT_STRING,    .ref = true,   .off = offsetof(log_item, tag),                                       },
	{ .tag = 'f', .type = LOG_FMT_STRING,    .ref = true,   .off = offsetof(log_item, file),                                      },
	{ .tag = 'F', .type = LOG_FMT_STRING,    .ref = true,   .off = offsetof(log_item, function),                                  },
	{ .tag = 'L', .type = LOG_FMT_SINT,      .ref = true,   .off = offsetof(log_item, lineno),          .len = sizeof(int),       },
	{ .tag = 'm', .type = LOG_FMT_STRING,    .ref = true,   .off = offsetof(log_item, content),                                   },
	{ .tag = 'Y', .type = LOG_FMT_UINT,      .ref = true,   .off = offsetof(log_item, time.Year),       .len = sizeof(UINT16),    },
	{ .tag = 'M', .type = LOG_FMT_UINT,      .ref = true,   .off = offsetof(log_item, time.Month),      .len = sizeof(UINT8),     },
	{ .tag = 'D', .type = LOG_FMT_UINT,      .ref = true,   .off = offsetof(log_item, time.Day),        .len = sizeof(UINT8),     },
	{ .tag = 'h', .type = LOG_FMT_UINT,      .ref = true,   .off = offsetof(log_item, time.Hour),       .len = sizeof(UINT8),     },
	{ .tag = 'i', .type = LOG_FMT_UINT,      .ref = true,   .off = offsetof(log_item, time.Minute),     .len = sizeof(UINT8),     },
	{ .tag = 's', .type = LOG_FMT_UINT,      .ref = true,   .off = offsetof(log_item, time.Second),     .len = sizeof(UINT8),     },
	{ .tag = 'S', .type = LOG_FMT_UINT,      .ref = true,   .off = offsetof(log_item, time.Nanosecond), .len = sizeof(UINT32),    },
	{}
};

static const char *resolve_specifier(
	struct log_formatter *fmt,
	log_item *item,
	char *num_buf,
	size_t num_buf_size
) {
	void *ptr = fmt->ref ? (void *)item + fmt->off : &fmt->data;
	union{
		UINT64 u;
		INT64 s;
	} i;
	switch (fmt->type) {
		case LOG_FMT_STRING:
			return *(const char **)ptr ?: "(null)";
		case LOG_FMT_UINT:
			switch (fmt->len) {
				case sizeof(UINT8): i.u = *(UINT8 *)ptr; break;
				case sizeof(UINT16): i.u = *(UINT16 *)ptr; break;
				case sizeof(UINT32): i.u = *(UINT32 *)ptr; break;
				case sizeof(UINT64): i.u = *(UINT64 *)ptr; break;
				default: i.u = 0; break;
			}
			snprintf(num_buf, num_buf_size, "%llu", i.u);
			return num_buf;
		case LOG_FMT_SINT:
			switch (fmt->len) {
				case sizeof(INT8): i.s = *(INT8 *)ptr; break;
				case sizeof(INT16): i.s = *(INT16 *)ptr; break;
				case sizeof(INT32): i.s = *(INT32 *)ptr; break;
				case sizeof(INT64): i.s = *(INT64 *)ptr; break;
				default: i.s = 0; break;
			}
			snprintf(num_buf, num_buf_size, "%lld", i.s);
			return num_buf;
		case LOG_FMT_LOG_LEVEL:
			return log_level_str(*(log_level *)ptr);
		default:
			return "";
	}
}

static const char *apply_specifier(log_item *item, char spec, char *num_buf, size_t num_buf_size) {
	struct log_formatter *f;
	for (f = default_formatters; f->tag; f++) {
		if (f->tag != spec) continue;
		return resolve_specifier(f, item, num_buf, num_buf_size);
	}
	return NULL;
}

/**
 * @brief Format a log item into a string using a printf-like format specifier.
 * Supported format specifiers:
 *   %%  literal percent sign
 *   %l  log level (DEBUG, INFO, WARNING, ERROR)
 *   %t  tag
 *   %f  source file name
 *   %F  source function name
 *   %L  source line number
 *   %m  log message content
 *   %Y  year, %M month, %D day, %h hour, %i minute, %s second, %S nanosecond
 *
 * If format is NULL, the default format "[%l] %t: %m" is used.
 *
 * @param item   the log item to format (must not be NULL)
 * @param format format string with % specifiers (may be NULL for default)
 * @param crlf   line ending mode: 0 = none, 1 = LF, >1 = CRLF
 * @return newly allocated formatted string (caller must free), or NULL on failure
 */
char* log_formatter(log_item *item, const char *format, int crlf) {
	size_t cap = 256, len = 0, vlen;
	char *result, *tmp, num_buf[32];
	const char *p, *value;
	if (!item) return NULL;
	if (!format) format = DEFAULT_LOG_FORMAT;
	p = format;
	if (!(result = malloc(cap))) return NULL;
	result[0] = 0;
	while (*p) if (*p == '%' && *(p + 1)) {
		p++;
		if (!(value = apply_specifier(item, *p, num_buf, sizeof(num_buf)))) {
			num_buf[0] = '%';
			num_buf[1] = *p;
			num_buf[2] = 0;
			value = num_buf;
		}
		vlen = strlen(value);
		while (len + vlen >= cap) {
			cap *= 2;
			if (!(tmp = realloc(result, cap))) goto fail;
			result = tmp;
		}
		memcpy(result + len, value, vlen);
		len += vlen;
		result[len] = 0;
		p++;
	} else {
		if (len + 1 >= cap) {
			cap *= 2;
			if (!(tmp = realloc(result, cap))) goto fail;
			result = tmp;
		}
		result[len++] = *p++;
		result[len] = 0;
	}
	if (crlf > 0) {
		if (len + 2 >= cap) {
			cap += 2;
			if (!(tmp = realloc(result, cap))) goto fail;
			result = tmp;
		}
		if (crlf > 1)
			result[len++] = '\r';
		result[len++] = '\n';
		result[len] = 0;
	}
	return result;
fail:
	if (result) free(result);
	return NULL;
}
