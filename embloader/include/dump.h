#ifndef DUMP_H
#define DUMP_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
typedef struct mem_dump mem_dump;
struct mem_dump{
	uint8_t step;
	uint8_t addr_len;
	bool show_header;
	bool print_ascii;
	bool real_address;
	bool buffered;
	const char**table;
	void*data;
	char*buffer;
	size_t buf_size,buf_pos;
	int(*printf)(mem_dump*d,const char*fmt,...);
	int(*putchar)(mem_dump*d,char ch);
	int(*print)(mem_dump*d,const char*str);
	int(*write)(mem_dump*d,const char*str,size_t len);
	void(*finish)(mem_dump*d);
};
extern const char*unicode_fat_table_char[];
extern const char*unicode_table_char[];
extern const char*ascii_table_char[];
extern mem_dump mem_dump_def;
extern void mem_dump_with(void*addr,size_t len,mem_dump*dump);
#endif
