#ifndef STR_UTILS_H
#define STR_UTILS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include "list.h"
extern bool string_is_true(const char *string);
extern bool string_is_false(const char *string);
extern bool stringn_is_true(const char *string, size_t len);
extern bool stringn_is_false(const char *string, size_t len);
extern bool str_match(const char *str, size_t len, const char *expected);
extern bool str_case_match(const char *str, size_t len, const char *expected);
extern bool startwith(const char*str,char c);
extern bool startwiths(const char*str,const char*s);
extern bool startnwith(const char*str,size_t len,char c);
extern bool startnwiths(const char*str,size_t len,const char*s);
extern bool endwith(const char*str,char c);
extern bool endwiths(const char*str,const char*s);
extern bool endnwith(const char*str,size_t len,char c);
extern bool endnwiths(const char*str,size_t len,const char*s);
extern bool endwithsi(const char *str, size_t len, const char *suffix);
extern bool removeend(char*str,char c);
extern bool removeends(char*str,const char*s);
extern void trimleft(char*str);
extern void trimright(char*str);
extern void trim(char*str);
extern int vscprintf(char*buf,size_t size,const char*fmt,va_list va);
extern int scprintf(char*buf,size_t size,const char*fmt,...);
extern list *string_to_list_by_space(const char *str);
#endif
