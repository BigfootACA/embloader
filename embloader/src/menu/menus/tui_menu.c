#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextOut.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include "embloader.h"
#include "bootmenu.h"
#include "encode.h"
#include "efi-utils.h"
#include "log.h"

struct tui_context {
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out;
	EFI_SIMPLE_TEXT_INPUT_PROTOCOL *in;
	UINTN col;
	UINTN row;
	embloader_menu *menu;
	int item_count;
	int selected_item;
	int scroll_offset;
	int def_num;
	embloader_loader *def_loader;
	embloader_loader **selected;
	UINTN menu_start_row;
	UINTN menu_end_row;
	UINTN max_visible_items;
	INTN timeout;
	uint64_t flags;
	bool have_timeout;
	bool is_scroll_up;
	bool is_scroll_down;
	int ansi_state;
	char ansi_sequence[8];
	int ansi_seq_len;
	bool esc_pending;
	bool force_redraw;
};

static void print_repeat(struct tui_context *ctx, UINTN count, CHAR16 ch) {
	UINTN remain = count, chunk;
	CHAR16 buff[256];
	while (remain > 0) {
		chunk = MIN(remain, (sizeof(buff) / sizeof(CHAR16)) - 1);
		SetMem16(buff, chunk * sizeof(CHAR16), ch);
		buff[chunk] = 0;
		remain -= chunk;
		ctx->out->OutputString(ctx->out, buff);
	}
}

static void set_clear_line(struct tui_context *ctx, UINTN col, UINTN row, UINTN attr) {
	ctx->out->SetCursorPosition(ctx->out, 0, row);
	ctx->out->SetAttribute(ctx->out, attr);
	print_repeat(ctx, ctx->col, L' ');
	ctx->out->SetCursorPosition(ctx->out, col, row);
}

static void write_at(struct tui_context *ctx, UINTN col, UINTN row, CHAR16 *str) {
	ctx->out->SetCursorPosition(ctx->out, col, row);
	ctx->out->OutputString(ctx->out, str);
}

static void write_utf8_at(struct tui_context *ctx, UINTN col, UINTN row, const char *str) {
	CHAR16 *utf16 = encode_utf8_to_utf16(str);
	if (!utf16) return;
	write_at(ctx, col, row, utf16);
	free(utf16);
}

static void printf_utf8_at(struct tui_context *ctx, UINTN col, UINTN row, const char *str, ...) {
	char *buffer = NULL;
	va_list args;
	va_start(args, str);
	int ret = vasprintf(&buffer, str, args);
	va_end(args);
	if (ret < 0 || !buffer) return;
	write_utf8_at(ctx, col, row, buffer);
	free(buffer);
}

static void reset_cursor(struct tui_context *ctx) {
	ctx->out->SetAttribute(ctx->out, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
	ctx->out->SetCursorPosition(ctx->out, 0, ctx->row - 1);
}

static void redraw_editor_dialog(
	struct tui_context *ctx,
	embloader_loader *item,
	char *buffer,
	size_t cursor_pos,
	size_t scroll_offset,
	UINTN dialog_col,
	UINTN dialog_row,
	UINTN dialog_width,
	UINTN dialog_height,
	UINTN content_width,
	UINTN content_start_row,
	UINTN content_end_row
) {
	UINTN i;
	size_t bootargs_len = strlen(buffer);
	UINTN current_row = content_start_row, wl;
	size_t cursor_screen_row = 0, cursor_screen_col = 0;
	size_t remaining, line_len, pos = scroll_offset * content_width;
	CHAR16 draw_buff[80], *p;
	if (dialog_width >= ARRAY_SIZE(draw_buff)) return;
	for (i = 0; i < dialog_height; i++)
		set_clear_line(ctx, dialog_col, dialog_row + i, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
	ctx->out->SetAttribute(ctx->out, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
	draw_buff[dialog_width] = 0;
	SetMem16(draw_buff, dialog_width * sizeof(CHAR16), L' ');
	draw_buff[0] = L'┌';
	for (i = 1; i < dialog_width - 1; i++) draw_buff[i] = L'─';
	draw_buff[dialog_width - 1] = L'┐';
	p = L"Edit Boot Arguments", wl = StrLen(p);
	i = (dialog_width - wl) / 2;
	CopyMem(&draw_buff[i], p, MIN(ARRAY_SIZE(draw_buff) - i, wl * sizeof(CHAR16)));
	write_at(ctx, dialog_col, dialog_row, draw_buff);
	SetMem16(draw_buff, dialog_width * sizeof(CHAR16), L' ');
	draw_buff[0] = L'│';
	wl = UnicodeSPrint(
		&draw_buff[2], (dialog_width - 3) * sizeof(CHAR16),
		L"Item: %a", item->title ? item->title : "(unnamed)"
	);
	draw_buff[wl + 2] = L' ';
	draw_buff[dialog_width - 1] = L'│';
	write_at(ctx, dialog_col, dialog_row + 1, draw_buff);
	SetMem16(draw_buff, dialog_width * sizeof(CHAR16), L' ');
	draw_buff[0] = L'├';
	for (i = 1; i < dialog_width - 1; i++) draw_buff[i] = L'─';
	draw_buff[dialog_width - 1] = L'┤';
	draw_buff[dialog_width] = 0;
	write_at(ctx, dialog_col, dialog_row + 2, draw_buff);
	while (current_row <= content_end_row) {
		SetMem16(draw_buff, dialog_width * sizeof(CHAR16), L' ');
		draw_buff[0] = L'│';
		if (pos < bootargs_len) {
			remaining = bootargs_len - pos;
			line_len = MIN(remaining, content_width);
			wl = 0;
			AsciiStrnToUnicodeStrS(
				buffer + pos, line_len,
				&draw_buff[2], content_width + 1, &wl
			);
			if (wl > 0 && draw_buff[wl + 2] == 0)
				draw_buff[wl + 2] = L' ';
			if (cursor_pos >= pos && cursor_pos <= pos + line_len) {
				cursor_screen_row = current_row;
				cursor_screen_col = dialog_col + 2 + (cursor_pos - pos);
			}
			pos += line_len;
		} else if (cursor_pos == bootargs_len && cursor_screen_row == 0) {
			cursor_screen_row = current_row;
			cursor_screen_col = dialog_col + 2;
		}
		draw_buff[dialog_width - 1] = L'│';
		draw_buff[dialog_width] = 0;
		write_at(ctx, dialog_col, current_row, draw_buff);
		current_row++;
	}
	SetMem16(draw_buff, dialog_width * sizeof(CHAR16), L' ');
	draw_buff[0] = L'└';
	for (i = 1; i < dialog_width - 1; i++) draw_buff[i] = L'─';
	draw_buff[dialog_width - 1] = L'┘';
	draw_buff[dialog_width] = 0;
	write_at(ctx, dialog_col, dialog_row + dialog_height - 1, draw_buff);
	if (cursor_screen_row > 0)
		ctx->out->SetCursorPosition(ctx->out, cursor_screen_col, cursor_screen_row);
}

static bool try_incremental_char_update(
	struct tui_context *ctx,
	CHAR16 ch,
	size_t cursor_pos,
	size_t scroll_offset,
	UINTN content_width,
	UINTN content_start_row,
	UINTN visible_lines,
	UINTN dialog_col
) {
	size_t cursor_line = cursor_pos / content_width;
	size_t cursor_col_in_line = cursor_pos % content_width;
	if (
		cursor_line >= scroll_offset &&
		cursor_line < scroll_offset + visible_lines
	) {
		size_t relative_line = cursor_line - scroll_offset;
		UINTN screen_row = content_start_row + relative_line;
		UINTN screen_col = dialog_col + 2 + cursor_col_in_line;
		if (screen_col < 99 && screen_row < 99) {
			write_at(ctx, screen_col, screen_row, (CHAR16[]){ch, 0});
			return true;
		}
	}
	return false;
}

static void show_editor_dialog(struct tui_context *ctx, embloader_loader *item) {
	EFI_INPUT_KEY key;
	EFI_STATUS status;
	size_t pos, cursor_screen_row, cursor_screen_col, line_len;
	size_t cursor_pos, len, old_len, old_lines, new_lines;
	size_t scroll_offset = 0, cursor_line, cursor_col_in_line, relative_line;
	bool done = false, esc_pending = false, need_redraw = false, cursor_only;
	UINTN screen_row, screen_col, current_row;
	UINTN dialog_width = MIN(ctx->col - 4, 76), dialog_height = 12;
	UINTN dialog_col = (ctx->col - dialog_width) / 2;
	UINTN dialog_row = (ctx->row - dialog_height) / 2;
	UINTN content_width = dialog_width - 4;
	UINTN content_start_row = dialog_row + 3;
	UINTN content_end_row = dialog_row + dialog_height - 2;
	UINTN visible_lines = content_end_row - content_start_row + 1;
	int ansi_state = STATE_NORMAL, ansi_seq_len = 0;
	char *buffer, ansi_sequence[8];
	if (!item || !item->editor) return;
	if (!(buffer = malloc(16384))) {
		log_error("failed to allocate memory for bootargs editor");
		return;
	}
	memset(buffer, 0, 16384);
	if (item->bootargs && item->bootargs[0])
		strncpy(buffer, item->bootargs, 16383);
	cursor_pos = strlen(buffer);
	old_len = cursor_pos;
	redraw_editor_dialog(
		ctx, item, buffer, cursor_pos, scroll_offset,
		dialog_col, dialog_row, dialog_width, dialog_height,
		content_width, content_start_row, content_end_row
	);
	ctx->out->EnableCursor(ctx->out, TRUE);
	while (!done) {
		memset(&key, 0, sizeof(key));
		gBS->Stall(50000);
		status = ctx->in->ReadKeyStroke(ctx->in, &key);
		if (status == EFI_NOT_READY) {
			if (esc_pending) {
				esc_pending = false;
				ansi_state = STATE_NORMAL;
				if (ansi_seq_len == 0) {
					done = true;
					need_redraw = false;
					continue;
				}
			}
			continue;
		}
		if (EFI_ERROR(status)) break;
		if (!efi_parse_ansi_sequence(
			&key, &ansi_state, ansi_sequence,
			&ansi_seq_len, &esc_pending
		)) continue;
		len = strlen(buffer);
		need_redraw = false, cursor_only = false;
		if (key.ScanCode == SCAN_ESC) {
			done = true, need_redraw = false;
		} else if (
			key.UnicodeChar == CHAR_LINEFEED ||
			key.UnicodeChar == CHAR_CARRIAGE_RETURN
		) {
			if (item->bootargs) free(item->bootargs);
			item->bootargs = strdup(buffer);
			ctx->flags |= EMBLOADER_FLAG_USERSELECT;
			ctx->flags |= EMBLOADER_FLAG_EDITED;
			*ctx->selected = item;
			done = true;
		} else if (key.ScanCode == SCAN_LEFT) {
			if (cursor_pos > 0) {
				cursor_pos--;
				cursor_only = true;
			}
		} else if (key.ScanCode == SCAN_RIGHT) {
			if (cursor_pos < len) {
				cursor_pos++;
				cursor_only = true;
			}
		} else if (key.ScanCode == SCAN_UP) {
			if (cursor_pos >= content_width) {
				cursor_pos -= content_width;
				cursor_only = true;
			} else if (cursor_pos > 0) {
				cursor_pos = 0;
				cursor_only = true;
			}
		} else if (key.ScanCode == SCAN_DOWN) {
			if (cursor_pos + content_width < len) {
				cursor_pos += content_width;
				cursor_only = true;
			} else if (cursor_pos < len) {
				cursor_pos = len;
				cursor_only = true;
			}
		} else if (key.ScanCode == SCAN_HOME) {
			cursor_pos = 0, cursor_only = true;
		} else if (key.ScanCode == SCAN_END) {
			cursor_pos = len, cursor_only = true;
		} else if (
			key.ScanCode == SCAN_DELETE ||
			key.UnicodeChar == 0x7F
		) {
			if (cursor_pos < len) {
				memmove(&buffer[cursor_pos], &buffer[cursor_pos + 1], len - cursor_pos);
				if (
					cursor_pos == len - 1 &&
					try_incremental_char_update(
						ctx, L' ', cursor_pos, scroll_offset,
						content_width, content_start_row, visible_lines, dialog_col
					)
				) len--, cursor_only = true;
				else need_redraw = true;
			}
		} else if (key.UnicodeChar == CHAR_BACKSPACE) {
			if (cursor_pos > 0) {
				memmove(&buffer[cursor_pos - 1], &buffer[cursor_pos], len - cursor_pos + 1);
				cursor_pos--;
				if (
					cursor_pos == len - 1 &&
					try_incremental_char_update(
						ctx, L' ', cursor_pos, scroll_offset,
						content_width, content_start_row, visible_lines, dialog_col
					)
				) len--, cursor_only = true;
				else need_redraw = true;
			}
		} else if (
			key.ScanCode == SCAN_NULL &&
			key.UnicodeChar <= 0xFF &&
			isprint((char)key.UnicodeChar)
		) {
			if (len < 16383) {
				memmove(&buffer[cursor_pos + 1], &buffer[cursor_pos], len - cursor_pos + 1);
				buffer[cursor_pos] = (char)key.UnicodeChar;
				if (
					cursor_pos == len &&
					try_incremental_char_update(
						ctx, key.UnicodeChar, cursor_pos, scroll_offset,
						content_width, content_start_row, visible_lines, dialog_col
					)
				) len++, cursor_only = true;
				else need_redraw = true;
				cursor_pos++;
			}
		}
		if (cursor_only) {
			cursor_line = cursor_pos / content_width;
			cursor_col_in_line = cursor_pos % content_width;
			if (cursor_line < scroll_offset) {
				scroll_offset = cursor_line;
				need_redraw = true, cursor_only = false;
			} else if (cursor_line >= scroll_offset + visible_lines) {
				scroll_offset = cursor_line - visible_lines + 1;
				need_redraw = true, cursor_only = false;
			} else {
				relative_line = cursor_line - scroll_offset;
				screen_row = content_start_row + relative_line;
				screen_col = dialog_col + 2 + cursor_col_in_line;
				if (screen_row <= content_end_row) {
					ctx->out->SetCursorPosition(ctx->out, screen_col, screen_row);
				} else need_redraw = true;
			}
		}
		if (need_redraw) {
			len = strlen(buffer);
			cursor_line = cursor_pos / content_width;
			if (cursor_line < scroll_offset) {
				scroll_offset = cursor_line;
			} else if (cursor_line >= scroll_offset + visible_lines) {
				scroll_offset = cursor_line - visible_lines + 1;
			}
			old_lines = (old_len + content_width - 1) / content_width;
			new_lines = (len + content_width - 1) / content_width;
			if (old_lines != new_lines || len == 0 || old_len == 0) {
				redraw_editor_dialog(
					ctx, item, buffer, cursor_pos, scroll_offset,
					dialog_col, dialog_row, dialog_width, dialog_height,
					content_width, content_start_row, content_end_row
				);
			} else {
				CHAR16 draw_buff[80];
				UINTN wl;
				ctx->out->SetAttribute(ctx->out, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
				current_row = content_start_row;
				pos = scroll_offset * content_width;
				cursor_screen_row = 0, cursor_screen_col = 0;
				while (current_row <= content_end_row && pos < len) {
					SetMem16(draw_buff, dialog_width * sizeof(CHAR16), L' ');
					draw_buff[0] = L'│';
					line_len = MIN(len - pos, content_width);
					wl = 0;
					AsciiStrnToUnicodeStrS(
						buffer + pos, line_len,
						&draw_buff[2], content_width + 1, &wl
					);
					if (wl > 0 && draw_buff[wl + 2] == 0)
						draw_buff[wl + 2] = L' ';
					draw_buff[dialog_width - 1] = L'│';
					cursor_line = cursor_pos / content_width;
					if (cursor_line >= scroll_offset && cursor_pos >= pos && cursor_pos <= pos + line_len) {
						cursor_screen_row = current_row;
						cursor_screen_col = dialog_col + 2 + (cursor_pos - pos);
					}
					draw_buff[dialog_width] = 0;
					write_at(ctx, dialog_col, current_row, draw_buff);
					pos += line_len, current_row++;
				}
				while (current_row <= content_end_row) {
					SetMem16(draw_buff, dialog_width * sizeof(CHAR16), L' ');
					draw_buff[0] = L'│';
					draw_buff[dialog_width - 1] = L'│';
					draw_buff[dialog_width] = 0;
					if (cursor_pos == len && cursor_screen_row == 0) {
						cursor_line = cursor_pos / content_width;
						if (cursor_line >= scroll_offset && cursor_line - scroll_offset == (size_t)(current_row - content_start_row)) {
							cursor_screen_row = current_row;
							cursor_screen_col = dialog_col + 2 + (cursor_pos % content_width);
						}
					}
					write_at(ctx, dialog_col, current_row, draw_buff);
					current_row++;
				}
				if (cursor_screen_row > 0)
					ctx->out->SetCursorPosition(ctx->out, cursor_screen_col, cursor_screen_row);
			}
			old_len = len;
		}
	}
	ctx->out->EnableCursor(ctx->out, FALSE);
	reset_cursor(ctx);
	free(buffer);
}

static void draw_top(struct tui_context *ctx) {
	set_clear_line(ctx, 0, 0, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLUE));
	write_at(ctx, 2, 0, L"Embedded Bootloader");
	UINTN row_start = 2;
	for (UINTN i = row_start; i < 5; i++)
		set_clear_line(ctx, 2, i, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
	if (g_embloader.device_name && g_embloader.device_name[0])
		printf_utf8_at(ctx, 2, row_start++, "Device: %s", g_embloader.device_name);
	if (g_embloader.menu->title && g_embloader.menu->title[0])
		printf_utf8_at(ctx, 2, row_start++, "Menu: %s", g_embloader.menu->title);
	if (row_start > 2) row_start++;
	ctx->menu_start_row = row_start;
}

static void draw_single_item(struct tui_context *ctx, int index, UINTN display_row, embloader_loader *item) {
	bool is_selected = index == ctx->selected_item;
	set_clear_line(ctx, 0, display_row, is_selected ?
		EFI_TEXT_ATTR(EFI_BLACK, EFI_LIGHTGRAY) :
		EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK)
	);
	printf_utf8_at(
		ctx, 2, display_row, "%c %d. %s",
		is_selected ? '>' : ' ',
		index,
		item->title ? item->title : "(unnamed)"
	);
}

static bool draw_menu_item(struct tui_context *ctx, int target) {
	list *p;
	int index = 0;
	UINTN display_row = ctx->menu_start_row;
	if ((p = list_first(ctx->menu->loaders))) do {
		LIST_DATA_DECLARE(item, p, embloader_loader*);
		if (!item) continue;
		index++;
		if (index < ctx->scroll_offset + 1) continue;
		if (index > ctx->scroll_offset + (int)ctx->max_visible_items) break;
		if (target == index) {
			draw_single_item(ctx, index, display_row, item);
			return true;
		}
		display_row++;
	} while ((p = p->next) && display_row <= ctx->menu_end_row);
	return false;
}

static void draw_menu_items(struct tui_context *ctx) {
	list *p;
	int index = 0;
	UINTN display_row = ctx->menu_start_row;
	ctx->max_visible_items = ctx->menu_end_row - ctx->menu_start_row + 1;
	if ((p = list_first(ctx->menu->loaders))) do {
		LIST_DATA_DECLARE(item, p, embloader_loader*);
		if (!item) continue;
		index++;
		if (index < ctx->scroll_offset + 1) continue;
		if (index > ctx->scroll_offset + (int)ctx->max_visible_items) break;
		draw_single_item(ctx, index, display_row, item);
		display_row++;
	} while ((p = p->next) && display_row <= ctx->menu_end_row);
	while (display_row <= ctx->menu_end_row)
		set_clear_line(ctx, 0, display_row++, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
	ctx->out->SetAttribute(ctx->out, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLACK));
	set_clear_line(ctx, 0, ctx->menu_start_row - 1, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
	set_clear_line(ctx, 0, ctx->menu_end_row + 1, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
	write_at(ctx, 4, ctx->menu_start_row - 1, ctx->is_scroll_up ? L"......" : L"      ");
	write_at(ctx, 4, ctx->menu_end_row + 1, ctx->is_scroll_down ? L"......" : L"      ");
}

static void draw_bottom(struct tui_context *ctx, bool update_layout) {
	UINTN row_start = ctx->row - 2;
	UINTN info_row;
	for (UINTN i = row_start; i > row_start - 6 && i > 0; i--)
		set_clear_line(ctx, 0, i, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
	row_start --;
	write_at(ctx, 2, row_start--, L"Arrow keys: Select | Enter: Boot | E: Edit bootargs | Esc: Exit");
	write_at(ctx, 2, row_start--, L"F: UEFI setup | B: Reboot | O: Shutdown");
	row_start --;
	info_row = row_start;
	if (ctx->timeout > 0) {
		ctx->out->SetAttribute(ctx->out, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLACK));
		printf_utf8_at(ctx, 2, info_row--, "Timeout: %d seconds", ctx->timeout / 1000);
	}
	if (ctx->def_loader) {
		ctx->out->SetAttribute(ctx->out, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLACK));
		printf_utf8_at(
			ctx, 2, info_row--, "Default: (%d) %s", ctx->def_num,
			ctx->def_loader->title ? ctx->def_loader->title : "(unnamed)"
		);
	}
	if (ctx->max_visible_items != 0 && ctx->item_count > (int)ctx->max_visible_items) {
		ctx->out->SetAttribute(ctx->out, EFI_TEXT_ATTR(EFI_CYAN, EFI_BLACK));
		UINTN disp_start = ctx->is_scroll_up ? ctx->scroll_offset + 1 : 1;
		UINTN disp_end = ctx->is_scroll_down ? ctx->scroll_offset + (int)ctx->max_visible_items : ctx->item_count;
		printf_utf8_at(ctx, 2, info_row--, "Items %d-%d of %d", disp_start, disp_end, ctx->item_count);
	}
	if (update_layout) ctx->menu_end_row = info_row - 1;
}

static void draw_once(struct tui_context *ctx) {
	if (!ctx->force_redraw) return;
	ctx->force_redraw = false;
	draw_top(ctx);
	draw_bottom(ctx, true);
	ctx->max_visible_items = ctx->menu_end_row - ctx->menu_start_row + 1;
	if (ctx->selected_item < ctx->scroll_offset + 1) {
		ctx->scroll_offset = ctx->selected_item - 1;
	} else if (ctx->selected_item > ctx->scroll_offset + (int)ctx->max_visible_items) {
		ctx->scroll_offset = ctx->selected_item - (int)ctx->max_visible_items;
	}
	if (ctx->scroll_offset < 0) ctx->scroll_offset = 0;
	if (ctx->scroll_offset > ctx->item_count - (int)ctx->max_visible_items) {
		ctx->scroll_offset = ctx->item_count - (int)ctx->max_visible_items;
	}
	if (ctx->scroll_offset < 0) ctx->scroll_offset = 0;
	ctx->is_scroll_up = ctx->scroll_offset > 0;
	ctx->is_scroll_down = ctx->scroll_offset + (int)ctx->max_visible_items < ctx->item_count;
	draw_menu_items(ctx);
	reset_cursor(ctx);
}

static embloader_loader* find_loader_index(struct tui_context *ctx, int target) {
	int index = 1;
	list *p;
	if (!ctx || !ctx->menu || !ctx->menu->loaders) return NULL;
	if ((p = list_first(ctx->menu->loaders))) do {
		LIST_DATA_DECLARE(item, p, embloader_loader*);
		if (!item) continue;
		if (target == index) return item;
		index++;
	} while ((p = p->next));
	return NULL;
}

static EFI_STATUS read_once(struct tui_context *ctx) {
	embloader_loader *item;
	EFI_STATUS status;
	EFI_INPUT_KEY key;
	memset(&key, 0, sizeof(key));
	status = ctx->in->ReadKeyStroke(ctx->in, &key);
	if (EFI_ERROR(status)) {
		if (status == EFI_NOT_READY && ctx->esc_pending) {
			ctx->esc_pending = false;
			ctx->ansi_state = STATE_NORMAL;
			if (ctx->ansi_seq_len == 0) return EFI_ABORTED;
		}
		return status;
	}
	if (!efi_parse_ansi_sequence(
		&key, &ctx->ansi_state, ctx->ansi_sequence,
		&ctx->ansi_seq_len, &ctx->esc_pending
	)) return EFI_NOT_READY;
	if (ctx->timeout >= 0) {
		ctx->timeout = -1;
		ctx->force_redraw = true;
		draw_once(ctx);
		reset_cursor(ctx);
	}
	int old_item = ctx->selected_item;
	if (
		key.UnicodeChar == '-' || key.UnicodeChar == '_' ||
		key.UnicodeChar == 'w' || key.UnicodeChar == 'a' ||
		key.ScanCode == SCAN_BRIGHTNESS_UP ||
		key.ScanCode == SCAN_VOLUME_UP ||
		key.ScanCode == SCAN_LEFT ||
		key.ScanCode == SCAN_UP
	) {
		ctx->selected_item = MAX(ctx->selected_item - 1, 1);
	} else if (
		key.UnicodeChar == '+' || key.UnicodeChar == '=' ||
		key.UnicodeChar == 's' || key.UnicodeChar == 'd' ||
		key.ScanCode == SCAN_BRIGHTNESS_DOWN ||
		key.ScanCode == SCAN_VOLUME_DOWN ||
		key.ScanCode == SCAN_RIGHT ||
		key.ScanCode == SCAN_DOWN
	) {
		ctx->selected_item = MIN(ctx->selected_item + 1, ctx->item_count);
	} else if (key.UnicodeChar == 'e' || key.UnicodeChar == 'E') {
		if ((item = find_loader_index(ctx, ctx->selected_item))) {
			show_editor_dialog(ctx, item);
			ctx->force_redraw = true;
			draw_once(ctx);
			return EFI_NOT_READY;
		}
	} else if (key.ScanCode == SCAN_PAGE_UP) {
		ctx->selected_item -= ctx->max_visible_items;
		ctx->selected_item = MAX(ctx->selected_item, 1);
	} else if (key.ScanCode == SCAN_PAGE_DOWN) {
		ctx->selected_item += ctx->max_visible_items;
		ctx->selected_item = MIN(ctx->selected_item, ctx->item_count);
	} else if (key.ScanCode == SCAN_HOME) {
		ctx->selected_item = 1;
	} else if (key.ScanCode == SCAN_END) {
		ctx->selected_item = ctx->item_count;
	} else if (key.UnicodeChar >= '1' && key.UnicodeChar <= '9') {
		int num = key.UnicodeChar - '0';
		ctx->selected_item = MIN(ctx->item_count, MAX(num, 1));
	} else if (
		key.UnicodeChar == 'F' ||
		key.UnicodeChar == 'f'
	) efi_reboot_to_setup();
	else if (
		key.UnicodeChar == 'B' ||
		key.UnicodeChar == 'b'
	) gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
	else if (
		key.UnicodeChar == 'O' ||
		key.UnicodeChar == 'o'
	) gRT->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, NULL);
	else if (
		key.ScanCode == SCAN_HIBERNATE ||
		key.ScanCode == SCAN_SUSPEND ||
		key.UnicodeChar == CHAR_LINEFEED ||
		key.UnicodeChar == CHAR_CARRIAGE_RETURN
	) {
		if ((item = find_loader_index(ctx, ctx->selected_item))) {
			ctx->flags |= EMBLOADER_FLAG_USERSELECT;
			*ctx->selected = item;
			return EFI_SUCCESS;
		}
	} else if (key.ScanCode == SCAN_ESC) {
		return EFI_ABORTED;
	}
	if (old_item != ctx->selected_item) {
		int old_scroll = ctx->scroll_offset;
		if (ctx->selected_item < ctx->scroll_offset + 1)
			ctx->scroll_offset = ctx->selected_item - 1;
		else if (ctx->selected_item > ctx->scroll_offset + (int)ctx->max_visible_items)
			ctx->scroll_offset = ctx->selected_item - (int)ctx->max_visible_items;
		if (ctx->scroll_offset < 0) ctx->scroll_offset = 0;
		if (ctx->scroll_offset > ctx->item_count - (int)ctx->max_visible_items)
			ctx->scroll_offset = ctx->item_count - (int)ctx->max_visible_items;
		if (ctx->scroll_offset < 0) ctx->scroll_offset = 0;
		if (old_scroll != ctx->scroll_offset) {
			ctx->is_scroll_up = ctx->scroll_offset > 0;
			ctx->is_scroll_down = ctx->scroll_offset + (int)ctx->max_visible_items < ctx->item_count;
			ctx->force_redraw = true;
		} else {
			draw_menu_item(ctx, old_item);
			draw_menu_item(ctx, ctx->selected_item);
			reset_cursor(ctx);
		}
	}
	return EFI_SUCCESS;
}

static bool filter_supports() {
	bool supports = true;
	if (confignode_path_get_bool(
		g_embloader.config,
		"menu.no-blacklist",
		false, NULL
	)) return true;
	char *bios_vendor = confignode_path_get_string(
		g_embloader.sysinfo,
		"smbios.bios0.vendor",
		NULL, NULL
	);
	if (bios_vendor) {
		if (strstr(bios_vendor, "Qualcomm")) {
			log_warning("disabled for Qualcomm UEFI");
			supports = false;
		}
		free(bios_vendor);
	}
	return supports;
}

/**
 * @brief Display a text-based user interface (TUI) menu for boot selection.
 * This function creates a more advanced console interface using EFI text output
 * protocols with screen clearing and cursor positioning.
 *
 * @param selected pointer to receive the selected loader entry
 * @return EFI_SUCCESS if a loader was selected,
 *         EFI_INVALID_PARAMETER for invalid parameters,
 *         EFI_UNSUPPORTED if screen too small or incomplete implementation,
 *         EFI_NOT_FOUND if no menu entries available,
 *         EFI_ABORTED if user cancelled the selection,
 *         other EFI errors on failure
 */
EFI_STATUS embloader_tui_menu_start(embloader_loader **selected, uint64_t *flags) {
	struct tui_context ctx;
	EFI_STATUS status;
	if (flags) *flags = 0;
	if (!g_embloader.menu || !selected) return EFI_INVALID_PARAMETER;
	if (!filter_supports()) {
		log_warning("this platform have bad console, skip TUI menu");
		return EFI_UNSUPPORTED;
	}
	memset(&ctx, 0, sizeof(ctx));
	ctx.out = gST->ConOut;
	ctx.in = gST->ConIn;
	ctx.menu = g_embloader.menu;
	ctx.selected = selected;
	ctx.ansi_state = STATE_NORMAL;
	ctx.ansi_seq_len = 0;
	ctx.esc_pending = false;
	*selected = NULL;
	ctx.out->ClearScreen(ctx.out);
	status = ctx.out->QueryMode(ctx.out, ctx.out->Mode->Mode, &ctx.col, &ctx.row);
	if (EFI_ERROR(status) || ctx.col < 40 || ctx.row < 20) {
		log_warning("text mode too small (%ux%u), skip drawing menu", ctx.col, ctx.row);
		return EFI_ERROR(status) ? status : EFI_UNSUPPORTED;
	}
	list *p;
	ctx.def_num = 1;
	if ((p = list_first(ctx.menu->loaders))) do {
		LIST_DATA_DECLARE(item, p, embloader_loader*);
		if (!item) continue;
		ctx.item_count++;
		if(ctx.item_count == 1) ctx.def_loader = item;
		if (
			ctx.menu->default_entry && item->name &&
			strcmp(ctx.menu->default_entry, item->name) == 0
		) {
			ctx.def_num = ctx.item_count;
			ctx.def_loader = item;
		}
	} while ((p = p->next));
	if (ctx.item_count == 0) return EFI_NOT_FOUND;
	ctx.selected_item = ctx.def_num;
	ctx.scroll_offset = 0;
	ctx.have_timeout = ctx.menu->timeout >= 0;
	ctx.timeout = ctx.menu->timeout * 1000;
	ctx.force_redraw = true;
	while (true) {
		draw_once(&ctx);
		int read_count = 0;
		while (true) {
			status = read_once(&ctx);
			if (*ctx.selected) {
				if (flags) *flags = ctx.flags;
				return EFI_SUCCESS;
			}
			if (!EFI_ERROR(status)) {
				read_count ++;
				if (read_count >= 8) break;
				continue;
			}
			if (status == EFI_NOT_READY) {
				if (read_count != 0) break;
				if (ctx.timeout == 0 && ctx.have_timeout) {
					*selected = ctx.def_loader;
					ctx.flags |= EMBLOADER_FLAG_AUTOBOOT;
					if (flags) *flags = ctx.flags;
					return EFI_SUCCESS;
				}
				gBS->Stall(100000);
				if (ctx.have_timeout && ctx.timeout > 0) {
					ctx.timeout -= 100;
					if (ctx.timeout % 1000 == 0) {
						draw_bottom(&ctx, false);
						ctx.out->SetCursorPosition(ctx.out, 0, ctx.row - 1);
						break;
					}
				}
				continue;
			}
			if (flags) *flags = ctx.flags;
			return status;
		}
	}
	if (flags) *flags = ctx.flags;
	return EFI_NOT_STARTED;
}
