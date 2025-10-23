#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseLib.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextOut.h>
#include <stdio.h>
#include <stdarg.h>
#include "embloader.h"
#include "bootmenu.h"
#include "encode.h"
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
};

static void set_clear_line(struct tui_context *ctx, UINTN col, UINTN row, UINTN attr) {
	ctx->out->SetCursorPosition(ctx->out, 0, row);
	ctx->out->SetAttribute(ctx->out, attr);
	for (UINTN i = 0; i < ctx->col; i++) ctx->out->OutputString(ctx->out, L" ");
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
	ctx->menu_start_row = row_start + 1;
}

static void draw_menu_items(struct tui_context *ctx) {
	list *p;
	UINTN display_row = ctx->menu_start_row;
	int index = 0;
	ctx->max_visible_items = ctx->menu_end_row - ctx->menu_start_row + 1;
	if ((p = list_first(ctx->menu->loaders))) do {
		LIST_DATA_DECLARE(item, p, embloader_loader*);
		if (!item) continue;
		index ++;
		if (index < ctx->scroll_offset + 1) continue;
		if (index > ctx->scroll_offset + (int)ctx->max_visible_items) break;
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
		display_row++;
	} while ((p = p->next) && display_row <= ctx->menu_end_row);
	while (display_row <= ctx->menu_end_row)
		set_clear_line(ctx, 0, display_row ++, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
	ctx->out->SetAttribute(ctx->out, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLACK));
	set_clear_line(ctx, 0, ctx->menu_start_row - 1, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
	set_clear_line(ctx, 0, ctx->menu_end_row + 1, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
	write_at(ctx, 4, ctx->menu_start_row - 1, ctx->is_scroll_up ? L"......" : L"      ");
	write_at(ctx, 4, ctx->menu_end_row + 1, ctx->is_scroll_down ? L"......" : L"      ");
}

static void draw_bottom(struct tui_context *ctx) {
	UINTN row_start = ctx->row - 2;
	for (UINTN i = row_start; i > row_start - 6 && i > 0; i--)
		set_clear_line(ctx, 0, i, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
	row_start --;
	write_at(ctx, 2, row_start, L"Use arrow keys to select, Enter to boot, Esc to exit");
	row_start --;
	if (ctx->timeout > 0) {
		ctx->out->SetAttribute(ctx->out, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLACK));
		printf_utf8_at(ctx, 2, row_start, "Timeout: %d seconds", ctx->timeout / 1000);
		row_start --;
	}
	if (ctx->def_loader) {
		ctx->out->SetAttribute(ctx->out, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLACK));
		printf_utf8_at(
			ctx, 2, row_start, "Default: (%d) %s", ctx->def_num,
			ctx->def_loader->title ? ctx->def_loader->title : "(unnamed)"
		);
		row_start --;
	}
	if (ctx->max_visible_items != 0 && ctx->item_count > (int)ctx->max_visible_items) {
		ctx->out->SetAttribute(ctx->out, EFI_TEXT_ATTR(EFI_CYAN, EFI_BLACK));
		UINTN disp_start = ctx->is_scroll_up ? ctx->scroll_offset + 1 : 1;
		UINTN disp_end = ctx->is_scroll_down ? ctx->scroll_offset + (int)ctx->max_visible_items : ctx->item_count;
		printf_utf8_at(ctx, 2, row_start, "Items %d-%d of %d", disp_start, disp_end, ctx->item_count);
		row_start --;
	}
	ctx->menu_end_row = row_start - 2;
}

static void draw_once(struct tui_context *ctx) {
	draw_top(ctx);
	draw_bottom(ctx);
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
	ctx->out->SetAttribute(ctx->out, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
	ctx->out->SetCursorPosition(ctx->out, 0, ctx->row - 1);
}

static EFI_STATUS read_once(struct tui_context *ctx) {
	EFI_STATUS status;
	EFI_INPUT_KEY key;
	memset(&key, 0, sizeof(key));
	status = ctx->in->ReadKeyStroke(ctx->in, &key);
	if (EFI_ERROR(status)) return status;
	ctx->timeout = -1;
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
		key.ScanCode == SCAN_HIBERNATE ||
		key.ScanCode == SCAN_SUSPEND ||
		key.UnicodeChar == CHAR_LINEFEED ||
		key.UnicodeChar == CHAR_CARRIAGE_RETURN
	) {
		int index = 1;
		list *p;
		if ((p = list_first(ctx->menu->loaders))) do {
			LIST_DATA_DECLARE(item, p, embloader_loader*);
			if (!item) continue;
			if (index == ctx->selected_item) {
				*ctx->selected = item;
				return EFI_SUCCESS;
			}
			index++;
		} while ((p = p->next));
	} else if (key.ScanCode == SCAN_ESC) {
		return EFI_ABORTED;
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
				if (ctx.have_timeout && ctx.timeout > 0) ctx.timeout -= 100;
				if (ctx.timeout % 1000 == 0) break;
				continue;
			}
			if (flags) *flags = ctx.flags;
			return status;
		}
	}
	if (flags) *flags = ctx.flags;
	return EFI_NOT_STARTED;
}
