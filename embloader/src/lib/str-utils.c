#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "str-utils.h"
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

bool stringn_is_true(const char *string, size_t len) {
	return
		strncasecmp(string, "y", len) == 0 ||
		strncasecmp(string, "ok", len) == 0 ||
		strncasecmp(string, "on", len) == 0 ||
		strncasecmp(string, "yes", len) == 0 ||
		strncasecmp(string, "true", len) == 0 ||
		strncasecmp(string, "always", len) == 0 ||
		strncasecmp(string, "enable", len) == 0 ||
		strncasecmp(string, "enabled", len) == 0;
}

bool stringn_is_false(const char *string, size_t len) {
	return
		strncasecmp(string, "n", len) == 0 ||
		strncasecmp(string, "no", len) == 0 ||
		strncasecmp(string, "off", len) == 0 ||
		strncasecmp(string, "false", len) == 0 ||
		strncasecmp(string, "never", len) == 0 ||
		strncasecmp(string, "disable", len) == 0 ||
		strncasecmp(string, "disabled", len) == 0;
}

bool string_is_true(const char *string) {
	return stringn_is_true(string, string ? strlen(string) : 0);
}

bool string_is_false(const char *string) {
	return stringn_is_false(string, string ? strlen(string) : 0);
}

bool str_match(const char *str, size_t len, const char *expected) {
	size_t expected_len = strlen(expected);
	return len == expected_len && strncmp(str, expected, len) == 0;
}

bool str_case_match(const char *str, size_t len, const char *expected) {
	size_t expected_len = strlen(expected);
	return len == expected_len && strncasecmp(str, expected, len) == 0;
}

bool startwith(const char*str,char c){
	if(!str)return false;
	return str[0]==c;
}

bool startwiths(const char*str,const char*s){
	if(!str||!s)return false;
	return strncmp(str,s,strlen(s))==0;
}

bool startnwith(const char*str,size_t len,char c){
	if(!str||len<=0)return false;
	return str[0]==c;
}

bool startnwiths(const char*str,size_t len,const char*s){
	if(!str||len<=0)return false;
	return strncmp(str,s,strnlen(s,len))==0;
}

bool endwith(const char*str,char c){
	if(!str)return false;
	size_t l=strlen(str);
	return l>0&&str[l-1]==c;
}

bool endwiths(const char*str,const char*s){
	if(!str||!s)return false;
	size_t ll=strlen(str),rl=strlen(s);
	if(rl==0)return true;
	if(rl>ll)return false;
	return strncmp(str+ll-rl,s,rl)==0;
}

bool endnwith(const char*str,size_t len,char c){
	if(!str||len<=0)return false;
	size_t l=strnlen(str,len);
	return l>0&&str[l-1]==c;
}

bool endnwiths(const char*str,size_t len,const char*s){
	if(!str||len<=0)return false;
	size_t ll=strnlen(str,len),rl=strlen(s);
	if(rl==0)return true;
	if(rl>ll)return false;
	return strncmp(str+ll-rl,s,rl)==0;
}

bool endwithsi(const char *str, size_t len, const char *suffix) {
	size_t suffix_len = strlen(suffix);
	return len >= suffix_len && strncasecmp(str + len - suffix_len, suffix, suffix_len) == 0;
}

bool removeend(char*str,char c){
	if(!str)return false;
	size_t l=strlen(str);
	bool r=(l>0&&str[l-1]==c);
	if(r)str[l-1]=0;
	return r;
}

bool removeends(char*str,const char*s){
	if(!str||!s)return false;
	size_t ll=strlen(str),rl=strlen(s);
	if(rl==0)return true;
	if(rl>ll)return false;
	bool r=strncmp(str+ll-rl,s,rl)==0;
	if(r)memset(str+ll-rl,0,rl);
	return r;
}

void trimleft(char*str){
	if(!str||!str[0])return;
	char*start=str;
	while(*start&&isspace(*start))start++;
	if(start!=str){
		if(!*start)str[0]=0;
		memmove(str,start,strlen(start)+1);
	}
}

void trimright(char*str){
	if(!str||!str[0])return;
	char*end=str+strlen(str);
	while(end>str&&isspace(end[-1]))end--;
	*end=0;
}

void trim(char*str){
	if(!str||!str[0])return;
	trimright(str);
	trimleft(str);
}

int vscprintf(char*buf,size_t size,const char*fmt,va_list va){
	size_t len=strnlen(buf,size);
	if(len>=size)return 0;
	return vsnprintf(buf+len,size-len,fmt,va);
}

int scprintf(char*buf,size_t size,const char*fmt,...){
	va_list va;
	va_start(va,fmt);
	int ret=vscprintf(buf,size,fmt,va);
	va_end(va);
	return ret;
}

list *string_to_list_by_space(const char *str) {
	if (!str) return NULL;
	list *result = NULL;
	const char *p = str;
	char *cur = malloc(strlen(str) + 1);
	if (!cur) return NULL;
	int item_pos = 0;
	bool in_single_quote = false;
	bool in_double_quote = false;
	bool esc = false;
	while (*p) {
		char c = *p;
		if (esc) {
			cur[item_pos++] = c, esc = false, p++;
			continue;
		}
		if (c == '\\' && !in_single_quote) {
			esc = true, p++;
			continue;
		}
		if (c == '\'' && !in_double_quote) {
			in_single_quote = !in_single_quote, p++;
			continue;
		}
		if (c == '"' && !in_single_quote) {
			in_double_quote = !in_double_quote, p++;
			continue;
		}
		if (isspace(c) && !in_single_quote && !in_double_quote) {
			if (item_pos > 0) {
				cur[item_pos] = 0;
				list_obj_add_new_strndup(&result, cur, item_pos);
				item_pos = 0;
			}
			while (*p && isspace(*p)) p++;
			continue;
		}
		cur[item_pos++] = c;
		p++;
	}
	if (item_pos > 0) {
		cur[item_pos] = 0;
		list_obj_add_new_strndup(&result, cur, item_pos);
	}
	free(cur);
	return result;
}

/**
 * @brief Compare two version strings with length limit
 *
 * This function implements a comprehensive version comparison algorithm similar
 * to RPM/Alpine version comparison. It handles:
 * - Numeric segments (compared numerically)
 * - Alphabetic segments (compared lexicographically)
 * - Special characters as separators
 * - Tilde (~) prefix means pre-release (lower priority)
 * - Leading zeros are ignored in numeric segments
 *
 * @param ver1 First version string to compare
 * @param ver2 Second version string to compare
 * @param len Maximum length to compare from ver1 (0 means unlimited)
 * @return < 0 if ver1 < ver2, 0 if ver1 == ver2, > 0 if ver1 > ver2
 */
int verncmp(const char *ver1, const char *ver2, size_t len) {
	if (!ver1 && !ver2) return 0;
	if (!ver1) return -1;
	if (!ver2) return 1;
	const char *p1 = ver1, *p2 = ver2;
	size_t pos1 = 0;
	while (*p1 || *p2) {
		if (len > 0 && pos1 >= len) break;
		while (*p1 && !isalnum(*p1) && *p1 != '~') p1++, pos1++;
		while (*p2 && !isalnum(*p2) && *p2 != '~') p2++;
		if (*p1 == '~' && *p2 != '~') return -1;
		if (*p1 != '~' && *p2 == '~') return 1;
		if (*p1 == '~' && *p2 == '~') {
			p1++, p2++, pos1++;
			continue;
		}
		if (!*p1 && !*p2) break;
		if (!*p1) return -1;
		if (!*p2) return 1;
		if (isdigit(*p1) && isdigit(*p2)) {
			while (*p1 == '0' && isdigit(*(p1 + 1))) p1++, pos1++;
			while (*p2 == '0' && isdigit(*(p2 + 1))) p2++;
			const char *num1_start = p1, *num2_start = p2;
			while (isdigit(*p1)) p1++, pos1++;
			while (isdigit(*p2)) p2++;
			size_t len1 = p1 - num1_start, len2 = p2 - num2_start;
			if (len1 != len2) return len1 < len2 ? -1 : 1;
			int cmp = strncmp(num1_start, num2_start, len1);
			if (cmp != 0) return cmp;
			continue;
		}
		if (isalpha(*p1) && isalpha(*p2)) {
			const char *alpha1_start = p1, *alpha2_start = p2;
			while (isalpha(*p1)) p1++, pos1++;
			while (isalpha(*p2)) p2++;
			size_t len1 = p1 - alpha1_start, len2 = p2 - alpha2_start;
			size_t min_len = len1 < len2 ? len1 : len2;
			int cmp = strncmp(alpha1_start, alpha2_start, min_len);
			if (cmp != 0) return cmp;
			if (len1 != len2) return len1 < len2 ? -1 : 1;
			continue;
		}
		if (isdigit(*p1)) return 1;
		if (isdigit(*p2)) return -1;
		p1++, p2++, pos1++;
	}
	return 0;
}

/**
 * @brief Compare two version strings (unlimited length)
 *
 * This is a convenience wrapper around verncmp() with no length limit.
 * It compares the full version strings using the same algorithm.
 *
 * @param ver1 First version string to compare
 * @param ver2 Second version string to compare
 * @return < 0 if ver1 < ver2, 0 if ver1 == ver2, > 0 if ver1 > ver2
 */
int vercmp(const char *ver1, const char *ver2) {
	return verncmp(ver1, ver2, 0);
}
