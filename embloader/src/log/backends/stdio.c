#include "../internal.h"
#include <unistd.h>

static int log_stdio_write_all(int fd, const char *buffer, size_t size) {
	size_t offset = 0;
	ssize_t written;
	if (fd < 0 || (!buffer && size > 0)) return -1;
	while (offset < size) {
		written = write(fd, buffer + offset, size - offset);
		if (written <= 0 || (size_t) written > size - offset) return -1;
		offset += (size_t) written;
	}
	return 0;
}

static int log_stdio_writer(log_backend *backend, log_item *item) {
	int ret = -1, fd = 1;
	char *format, *output, *formatted;
	if (!backend || !item) return -1;
	if (!log_check_filter(item, backend->config)) return 0;
	format = confignode_path_get_string(backend->config, "format", NULL, NULL);
	output = confignode_path_get_string(backend->config, "output", NULL, NULL);
	formatted = log_formatter(item, format, 1);
	if (output) {
		if (strcasecmp(output, "stdout") == 0) fd = 1;
		else if (strcasecmp(output, "stderr") == 0) fd = 2;
		else fd = -1;
	}
	if (formatted && fd > 0)
		ret = log_stdio_write_all(fd, formatted, strlen(formatted));
	if (formatted) free(formatted);
	if (format) free(format);
	if (output) free(output);
	return ret;
}

log_backend_base log_backend_stdio = {
	.name = "stdio",
	.write = log_stdio_writer,
};
