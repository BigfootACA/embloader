#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "dump.h"
#include "print.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

const char* unicode_fat_table_char[] = {
	"═", "║",
	"╔", "╦", "╗",
	"╠", "╬", "╣",
	"╚", "╩", "╝",
};

const char* unicode_table_char[] = {
	"─", "│",
	"┌", "┬", "┐",
	"├", "┼", "┤",
	"└", "┴", "┘",
};

const char* ascii_table_char[] = {
	"-", "|",
	"+", "+", "+",
	"+", "+", "+",
	"+", "+", "+",
};

static void dump_finish(mem_dump* dump) {
	if (!dump || !dump->write) return;
	if (!dump->buffered || !dump->buffer) return;
	if (dump->buf_pos > 0) {
		dump->buffer[dump->buf_pos + 1] = 0;
		dump->write(dump, dump->buffer, dump->buf_pos + 1);
		dump->buf_pos = 0;
	}
	free(dump->buffer);
	dump->buffer = NULL;
	dump->buf_size = 0;
}

static int dump_print(mem_dump* dump, const char* str) {
	void* buf;
	size_t len, size, i;
	if (!dump || !dump->write || !str) return -1;
	if ((len = strlen(str)) <= 0) return 0;
	if (!dump->buffered) return dump->write(dump, str, len);
	if (len + dump->buf_pos >= dump->buf_size || !dump->buffer) {
		size = dump->buf_size;
		while (len + dump->buf_pos >= size) size += 4096;
		buf = dump->buffer ? realloc(dump->buffer, size) : malloc(size);
		if (!buf) return -1;
		memset(buf, 0, size);
		dump->buf_size = size;
		dump->buffer = buf;
	}
	for (i = 0; i < len; i++) {
		dump->buffer[dump->buf_pos] = str[i];
		if (str[i] == '\n') {
			dump->buffer[dump->buf_pos + 1] = 0;
			dump->write(dump, dump->buffer, dump->buf_pos + 1);
			dump->buf_pos = 0;
		} else dump->buf_pos++;
	}
	return 0;
}

static int dump_putchar(mem_dump* dump, const char ch) {
	char buff[2] = {ch, 0};
	if (!dump || !dump->print) return -1;
	return dump->print(dump, buff);
}

static int dump_printf(mem_dump* dump, const char* fmt, ...) {
	int r;
	va_list va;
	bool alloc = false;
	char buff[256];
	char* str = buff;
	if (!dump || !dump->print) return -1;
	va_start(va, fmt);
	r = vsnprintf(buff, sizeof(buff) - 1, fmt, va);
	if (r >= 0 && (size_t) r >= sizeof(buff)) {
		alloc = true, str = NULL;
		r = vasprintf(&str, fmt, va);
		if (r < 0 || !str) return -1;
	}
	va_end(va);
	if (!str) return 0;
	dump->print(dump, str);
	if (alloc) free(str);
	return 0;
}

static int dump_stdio_write(mem_dump* dump, const char* str, size_t len) {
	(void) dump;
	out_print(str);
	return 0;
}

mem_dump mem_dump_def = {
	.step = 0x8,
	.addr_len = 4,
	.table = unicode_table_char,
	.write = dump_stdio_write,
	.show_header = true,
	.print_ascii = true,
};

void mem_dump_with(void* addr, size_t len, mem_dump* dump) {
	char* u = addr;
	char buff[256];
	size_t i, a, b, s = 0;
	if (!dump) dump = &mem_dump_def;
	if (!dump->write || dump->step <= 0 || len <= 0) return;
	if (!dump->finish) dump->finish = dump_finish;
	if (!dump->putchar) dump->putchar = dump_putchar;
	if (!dump->printf) dump->printf = dump_printf;
	if (!dump->print) dump->print = dump_print;
	if (dump->show_header) {
		if (dump->addr_len > 0)
			for (i = 0; i < (size_t) dump->addr_len + 1; i++)
				dump->putchar(dump, ' ');
		if (dump->table) dump->print(dump, "  ");
		for (i = 0; i < (size_t) dump->step; i++)
			dump->printf(dump, "%02zX ", i);
		if (dump->print_ascii) {
			if (dump->table) dump->print(dump, "  ");
			dump->print(dump, "ASCII");
		}
		dump->putchar(dump, '\n');
	}
	if (dump->table) {
		if (dump->addr_len > 0)
			for (i = 0; i < (size_t) dump->addr_len + 1; i++)
				dump->putchar(dump, ' ');
		dump->print(dump, dump->table[2]);
		for (i = 0; i < (size_t) dump->step * 3 + 1; i++)
			dump->print(dump, dump->table[0]);
		if (dump->print_ascii) {
			dump->print(dump, dump->table[3]);
			for (i = 0; i < (size_t) dump->step + 2; i++)
				dump->print(dump, dump->table[0]);
		}
		dump->print(dump, dump->table[4]);
	}
	for (a = 0; a < len; a++) {
		if (a % dump->step == 0) {
			if (dump->table && a != 0) {
				dump->printf(dump, dump->table[1]);
				dump->putchar(dump, ' ');
			}
			if (dump->print_ascii) {
				for (b = a - dump->step; b < a; b++)
					dump->putchar(
						dump,
						isprint(u[b]) ? u[b] : '.'
					);
				if (dump->table && a != 0) {
					dump->putchar(dump, ' ');
					dump->printf(dump, dump->table[1]);
					dump->putchar(dump, ' ');
				}
			}
			if (dump->table || a != 0) dump->putchar(dump, '\n');
			if (dump->addr_len > 0) {
				snprintf(
					buff, sizeof(buff),
					"%%0%dzX ", dump->addr_len
				);
				dump->printf(
					dump,buff,
					a + (dump->real_address ? (size_t) addr : 0)
				);
			}
			if (dump->table) {
				dump->printf(dump, dump->table[1]);
				dump->putchar(dump, ' ');
			}
			s = a;
		}
		dump->printf(dump, "%02X ", (unsigned char) (u[a] % 0xFF));
	}
	if (s != len)
		for (b = 0; b < s + dump->step - len; b++)
			dump->print(dump, "   ");
	if (dump->table) {
		dump->print(dump, dump->table[1]);
		dump->putchar(dump, ' ');
	}
	if (dump->print_ascii) {
		for (b = s; b < len; b++)
			dump->printf(dump, "%c", isprint(u[b]) ? u[b] : '.');
		for (b = 0; b < s + dump->step - len; b++)
			dump->putchar(dump, ' ');
		if (dump->table) {
			dump->putchar(dump, ' ');
			dump->print(dump, dump->table[1]);
		}
	}
	if (dump->table) dump->putchar(dump, '\n');
	if (dump->addr_len > 0)
		for (i = 0; i < (size_t) dump->addr_len + 1; i++)
			dump->putchar(dump, ' ');
	if (dump->table) {
		dump->print(dump, dump->table[8]);
		for (i = 0; i < (size_t) dump->step * 3 + 1; i++)
			dump->print(dump, dump->table[0]);
		if (dump->print_ascii) {
			dump->print(dump, dump->table[9]);
			for (i = 0; i < (size_t) dump->step + 2; i++)
				dump->print(dump, dump->table[0]);
		}
		dump->print(dump, dump->table[10]);
		dump->putchar(dump, '\n');
	}
}
