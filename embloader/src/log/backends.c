#include "internal.h"

log_backend_base *log_backend_bases[] = {
	&log_backend_stdio,
	&log_backend_file,
	NULL
};
