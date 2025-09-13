#ifndef PATH_H
#define PATH_H
#include <string.h>
extern size_t path_resolve(char* buff, size_t size, char sep, const char* path);
extern size_t path_merge(
	char* buff,
	size_t size,
	char sep,
	const char* path1,
	const char* path2
);
extern const char* path_basename(char* buff, size_t size, const char* path);
extern const char* path_dirname(char* buff, size_t size, const char* path);
#endif
