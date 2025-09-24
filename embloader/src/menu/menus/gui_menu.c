#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "menu.h"
#include "lvgl.h"
#include "log.h"
#include "ticks.h"
#include "efi-utils.h"
#include "efi-console-control.h"
#include "file-utils.h"
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SerialIo.h>
#include <stdio.h>

extern const char mouse_cursor_icon[];
extern const uint64_t mouse_cursor_icon_size;
extern lv_image_decoder_t* lv_nanosvg_init();

struct gui_menu_ctx {
	volatile bool running;
	int timeout;
	embloader_loader **selected;
	lv_obj_t *timeout_box;
	lv_obj_t *timeout_field;
	EFI_STATUS ret;
	lv_obj_t *box_menu;
	lv_obj_t *lbl_menu;
	lv_obj_t *box_device;
	lv_obj_t *lbl_device;
	lv_obj_t *lst_items;
	lv_obj_t *box_default;
	lv_obj_t *lbl_default;
	lv_obj_t *box_timeout;
	lv_obj_t *lbl_timeout;
	lv_obj_t *btn_boot;
	lv_group_t *group;
	lv_obj_t *cursor;
	list *items;
	EFI_HANDLE gop_handle;
	lv_timer_t *timer;
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
	EFI_SERIAL_IO_PROTOCOL *serial;
};

struct gui_menu_item {
	int index;
	struct gui_menu_ctx *ctx;
	embloader_loader *loader;
	lv_obj_t *menu;
	char **attrs;
	char txt_index[32];
};

static lv_img_dsc_t img_mouse = {
	.header.magic = LV_IMAGE_HEADER_MAGIC,
	.header.cf = LV_COLOR_FORMAT_ARGB8888,
	.header.flags = 0,
	.header.w = 13,
	.header.h = 21,
	.header.stride = 14 * sizeof(lv_color32_t),
	.data_size = 0,
	.data = (uint8_t*) mouse_cursor_icon,
};

static EFI_STATUS gui_init(struct gui_menu_ctx *ctx) {
	if (!lv_is_initialized()) {
		lv_uefi_init(gImageHandle, gST);
		lv_init();
		lv_nanosvg_init();
		if (!lv_is_initialized()) {
			log_warning("lvgl init failed");
			return EFI_UNSUPPORTED;
		}
	}
	EFI_STATUS status;
	lv_display_t* display;
	lv_indev_t* indev = NULL;
	ctx->gop_handle = lv_uefi_display_get_active();
	if (!ctx->gop_handle) ctx->gop_handle = lv_uefi_display_get_any();
	if (!ctx->gop_handle) {
		log_warning("no display found for lvgl");
		return EFI_UNSUPPORTED;
	}
	status = gBS->HandleProtocol(
		ctx->gop_handle, &gEfiGraphicsOutputProtocolGuid, (void**)&ctx->gop
	);
	if (EFI_ERROR(status) || !ctx->gop) {
		log_warning("couldn't get GOP protocol: %s", efi_status_to_string(status));
		return EFI_UNSUPPORTED;
	}
	display = lv_uefi_display_create(ctx->gop_handle);
	lv_display_set_default(display);
	ctx->group = lv_group_create();
	lv_group_set_default(ctx->group);
	ctx->cursor = lv_image_create(lv_layer_top());
	img_mouse.data_size = mouse_cursor_icon_size;
	lv_image_set_src(ctx->cursor, &img_mouse);
	if ((indev = lv_uefi_simple_text_input_indev_create())) {
		lv_uefi_simple_text_input_indev_add_all(indev);
		lv_indev_set_group(indev, ctx->group);
	}
	if ((indev = lv_uefi_simple_pointer_indev_create(NULL))) {
		lv_uefi_simple_pointer_indev_add_all(indev);
		lv_indev_set_cursor(indev, ctx->cursor);
		lv_indev_set_group(indev, ctx->group);
	}
	if ((indev = lv_uefi_absolute_pointer_indev_create(NULL))) {
		lv_uefi_absolute_pointer_indev_add_all(indev);
		lv_indev_set_cursor(indev, ctx->cursor);
		lv_indev_set_group(indev, ctx->group);
	}
	return EFI_SUCCESS;
}

static bool is_class_type(
	struct gui_menu_ctx *ctx,
	lv_obj_t *obj,
	const lv_obj_class_t *cls
) {
	if (lv_obj_has_class(obj, cls)) return true;
	const char *name =lv_obj_get_name(obj);
	if (!name) log_warning("object %p is not of class %s", obj, cls->name);
	else log_warning("object %s is not of class %s", name, cls->name);
	ctx->running = false;
	return false;
}

static void update_fields(struct gui_menu_ctx *ctx) {
	embloader_menu *menu = g_embloader.menu;
	if (ctx->box_menu) lv_obj_set_flag(
		ctx->box_menu,
		LV_OBJ_FLAG_HIDDEN,
		menu->title == NULL
	);
	if (ctx->box_device) lv_obj_set_flag(
		ctx->box_device,
		LV_OBJ_FLAG_HIDDEN,
		g_embloader.device_name == NULL
	);
	if (ctx->box_timeout) lv_obj_set_flag(
		ctx->box_timeout,
		LV_OBJ_FLAG_HIDDEN,
		ctx->timeout <= 0
	);
	if (ctx->box_default) lv_obj_set_flag(
		ctx->box_default,
		LV_OBJ_FLAG_HIDDEN,
		menu->default_entry == NULL
	);
	if (ctx->lbl_menu) lv_label_set_text(
		ctx->lbl_menu,
		menu->title ?: ""
	);
	if (ctx->lbl_timeout) lv_label_set_text_fmt(
		ctx->lbl_timeout, "%d",
		ctx->timeout > 0 ? ctx->timeout : 0
	);
	if (ctx->lbl_device) lv_label_set_text(
		ctx->lbl_device,
		g_embloader.device_name ?: ""
	);
	embloader_loader *loader = embloader_find_loader(menu->default_entry);
	if (ctx->lbl_default && loader) lv_label_set_text(
		ctx->lbl_default,
		loader->title ?: (loader->name ?: menu->default_entry)
	);
}

static void printf_serial(struct gui_menu_ctx *ctx, const char *fmt, ...) {
	char *buf = NULL;
	va_list args;
	if (!ctx || !ctx->serial || !fmt) return;
	va_start(args, fmt);
	int ret = vasprintf(&buf, fmt, args);
	va_end(args);
	if (ret < 0 || !buf) return;
	UINTN len = (UINTN) ret;
	ctx->serial->Write(ctx->serial, &len, buf);
	free(buf);
}

static void boot_item(struct gui_menu_ctx *ctx) {
	if (!ctx) return;
	if (*(ctx->selected)) {
		printf_serial(
			ctx, "Booting entry %s (%s)\r\n",
			(*ctx->selected)->title ?: "null",
			(*ctx->selected)->name ?: "null"
		);
		ctx->ret = EFI_SUCCESS;
	} else {
		printf_serial(ctx, "Cancel boot\r\n");
		ctx->ret = EFI_ABORTED;
	}
	ctx->running = false;
}

static void clear_timeout(struct gui_menu_ctx *ctx) {
	if (!ctx) return;
	if (ctx->timer) {
		lv_timer_delete(ctx->timer);
		ctx->timer = NULL;
		printf_serial(ctx, "Timer cleared\r\n");
	}
	ctx->timeout = -1;
	update_fields(ctx);
}

static void item_event_cb(lv_event_t *e) {
	list *p;
	struct gui_menu_item *item = lv_event_get_user_data(e);
	if (!item) return;
	lv_event_code_t code = lv_event_get_code(e);
	if (code == LV_EVENT_CLICKED) {
		clear_timeout(item->ctx);
		bool checked = lv_obj_has_state(item->menu, LV_STATE_CHECKED);
		if (checked) {
			if ((p = list_first(item->ctx->items))) do {
				LIST_DATA_DECLARE(it, p, struct gui_menu_item*);
				if (it && it != item) lv_obj_remove_state(it->menu, LV_STATE_CHECKED);
			} while ((p = p->next));
			*(item->ctx->selected) = item->loader;
		} else {
			*(item->ctx->selected) = NULL;
		}
		printf_serial(
			item->ctx, "%s entry %d. %s (%s)\r\n",
			checked ? "Selected" : "Deselected",
			item->index,
			item->loader->title ?: "null",
			item->loader->name ?: "null"
		);
		if (item->ctx->btn_boot) {
			lv_obj_set_state(item->ctx->btn_boot, LV_STATE_DISABLED, !checked);
			if (checked) lv_group_focus_obj(item->ctx->btn_boot);
		}
	} else if (code == LV_EVENT_KEY) {
		clear_timeout(item->ctx);
	}
}

static void boot_event_cb(lv_event_t *e) {
	struct gui_menu_ctx *ctx = lv_event_get_user_data(e);
	if (!ctx) return;
	lv_event_code_t code = lv_event_get_code(e);
	if (code == LV_EVENT_CLICKED) {
		clear_timeout(ctx);
		boot_item(ctx);
	} else if (code == LV_EVENT_KEY) {
		clear_timeout(ctx);
	}
}

static void timer_cb(lv_timer_t *timer) {
	struct gui_menu_ctx *ctx = timer->user_data;
	if (!ctx || !ctx->running) return;
	if (ctx->timeout < 0) return;
	ctx->timeout--;
	printf_serial(ctx, "Timeout: %d seconds\r\n", ctx->timeout);
	update_fields(ctx);
	if (ctx->timeout > 0) return;
	embloader_loader *loader;
	if ((loader = embloader_find_loader(g_embloader.menu->default_entry))) {
		printf_serial(
			ctx, "Auto selected entry %s (%s)\r\n",
			loader->title ?: "null",
			loader->name ?: "null"
		);
		*(ctx->selected) = loader;
	}
	boot_item(ctx);
}

static bool draw_menu_main(struct gui_menu_ctx *ctx) {
	lv_obj_t *menu_main = lv_xml_create(NULL, "menu-main", NULL);
	if (!menu_main) {
		log_warning("failed to create menu-main");
		return false;
	}
	lv_screen_load(menu_main);
	ctx->box_menu = lv_obj_find_by_name(menu_main, "box_menu");
	ctx->lbl_menu = lv_obj_find_by_name(menu_main, "lbl_menu");
	ctx->box_device = lv_obj_find_by_name(menu_main, "box_device");
	ctx->lbl_device = lv_obj_find_by_name(menu_main, "lbl_device");
	ctx->lst_items = lv_obj_find_by_name(menu_main, "items");
	ctx->box_default = lv_obj_find_by_name(menu_main, "box_default");
	ctx->lbl_default = lv_obj_find_by_name(menu_main, "lbl_default");
	ctx->box_timeout = lv_obj_find_by_name(menu_main, "box_timeout");
	ctx->lbl_timeout = lv_obj_find_by_name(menu_main, "lbl_timeout");
	ctx->btn_boot = lv_obj_find_by_name(menu_main, "btn_boot");
	if (!is_class_type(ctx, ctx->lbl_menu, &lv_label_class)) return false;
	if (!is_class_type(ctx, ctx->lbl_device, &lv_label_class)) return false;
	if (!is_class_type(ctx, ctx->lbl_timeout, &lv_label_class)) return false;
	if (!is_class_type(ctx, ctx->lbl_default, &lv_label_class)) return false;
	if (!is_class_type(ctx, ctx->btn_boot, &lv_button_class)) return false;
	if (!ctx->lst_items) {
		log_warning("failed to find lst_items box");
		return false;
	}
	if (ctx->btn_boot) lv_obj_add_event_cb(
		ctx->btn_boot, boot_event_cb,
		LV_EVENT_ALL, ctx
	);
	update_fields(ctx);
	if (g_embloader.menu->title) printf_serial(
		ctx, "Menu: %s\r\n",
		g_embloader.menu->title
	);
	if (g_embloader.device_name) printf_serial(
		ctx, "Device: %s\r\n",
		g_embloader.device_name
	);
	if (g_embloader.menu->timeout > 0) printf_serial(
		ctx, "Timeout: %d seconds\r\n",
		ctx->timeout
	);
	if (g_embloader.menu->default_entry) printf_serial(
		ctx, "Default entry: %s\r\n",
		g_embloader.menu->default_entry
	);
	return true;
}

static bool draw_menu_item(
	struct gui_menu_ctx *ctx,
	struct embloader_loader *loader,
	int index
) {
	struct gui_menu_item *item = NULL;
	if (!(item = malloc(sizeof(struct gui_menu_item)))) {
		log_warning("failed to allocate menu item");
		goto fail;
	}
	memset(item, 0, sizeof(struct gui_menu_item));
	item->ctx = ctx;
	item->loader = loader;
	item->index = index;
	snprintf(item->txt_index, sizeof(item->txt_index), "%d", index);
	const char * attrs [] = {
		"name", loader->name ?: "",
		"title", loader->title ?: "",
		"index", item->txt_index,
		NULL, NULL
	};
	if (!(item->menu = lv_xml_create(ctx->lst_items, "menu-item", attrs))) {
		log_warning("failed to create menu-item");
		goto fail;
	}
	lv_group_add_obj(ctx->group, item->menu);
	if (g_embloader.menu->default_entry && strcmp(
		loader->name, g_embloader.menu->default_entry
	) == 0) {
		lv_obj_add_state(item->menu, LV_STATE_CHECKED);
		*(ctx->selected) = loader;
		if (ctx->btn_boot) lv_obj_set_state(ctx->btn_boot, LV_STATE_DISABLED, false);
		lv_group_focus_obj(item->menu);
	}
	lv_obj_add_event_cb(item->menu, item_event_cb, LV_EVENT_ALL, item);
	list_obj_add_new(&ctx->items, item);
	printf_serial(
		ctx, "Entry %d. %s (%s)\r\n",
		index,
		loader->title ?: "null",
		loader->name ?: "null"
	);
	return true;
fail:
	if (item) free(item);
	return false;
}

static bool draw_menu_items(struct gui_menu_ctx *ctx) {
	if (!ctx->lst_items) {
		log_warning("failed to find items box");
		return false;
	}
	list *p;
	int index = 1;
	lv_group_remove_obj(ctx->btn_boot);
	if ((p = list_first(g_embloader.menu->loaders))) do {
		LIST_DATA_DECLARE(loader, p, embloader_loader*);
		if (!loader) continue;
		if (!draw_menu_item(ctx, loader, index++)) return false;
	} while ((p = p->next));
	lv_group_add_obj(ctx->group, ctx->btn_boot);
	return true;
}

static bool load_xml_from(const char *name, const char *key, const char *def) {
	bool ret = false;
	char *path = NULL, *buffer = NULL;
	if (!(path = confignode_path_get_string(
		g_embloader.config, key, def, NULL
	))) {
		log_warning("no XML path configured for %s", name);
		goto done;
	}
	size_t len = 0;
	EFI_STATUS status;
	status = efi_file_open_read_all(
		g_embloader.dir.dir, path,
		(void**)&buffer, &len
	);
	if (EFI_ERROR(status) || !buffer || len == 0) {
		log_warning(
			"failed to read XML file %s for %s: %s",
			path, name, efi_status_to_string(status)
		);
		goto done;
	}
	if (lv_xml_component_register_from_data(name, buffer) != LV_RESULT_OK) {
		log_warning("failed to load XML file %s for %s", path, name);
		goto done;
	}
	log_debug("loaded XML layout file %s for %s", path, name);
	ret = true;
done:
	if (buffer) free(buffer);
	if (path) free(path);
	return ret;
}

static void serial_alt_init(struct gui_menu_ctx *ctx) {
	EFI_STATUS status;
	status = gBS->LocateProtocol(
		&gEfiSerialIoProtocolGuid,
		NULL, (void**)&ctx->serial
	);
	if (EFI_ERROR(status) || !ctx->serial)
		log_warning("no serial console found: %s", efi_status_to_string(status));
	printf_serial(ctx, "\r\nEmbedded Boot Loader GUI Menu Serial Alternative\r\n");
}

static void set_graphics_mode(bool enable) {
        EFI_CONSOLE_CONTROL_PROTOCOL *ctrl = NULL;
        EFI_CONSOLE_CONTROL_SCREEN_MODE new, old;
        BOOLEAN uga_exists, stdin_locked;
        EFI_STATUS status;
        status = gBS->LocateProtocol(&gEfiConsoleControlProtocolGuid, NULL, (void **) &ctrl);
        if (EFI_ERROR(status)) return;
        status = ctrl->GetMode(ctrl, &old, &uga_exists, &stdin_locked);
        if (EFI_ERROR(status)) return;
        new = enable ? EfiConsoleControlScreenGraphics : EfiConsoleControlScreenText;
        if (new == old) return;
        status = ctrl->SetMode(ctrl, new);
        gST->ConOut->EnableCursor(gST->ConOut, !enable);
}

static void uefi_delay(uint32_t ms) {
	if (ms == 0) return;
	gBS->Stall(ms * 1000);
}

/**
 * @brief Start the graphical user interface menu for boot loader selection
 *
 * This function initializes and displays a GUI menu using LVGL for boot loader
 * selection. It provides an interactive interface with mouse support, timeout
 * functionality, and serial console alternative output.
 *
 * @param selected Pointer to store the selected boot loader entry
 * @return EFI_STATUS Returns EFI_SUCCESS if a loader was selected successfully,
 *                    EFI_INVALID_PARAMETER for invalid parameters,
 *                    EFI_UNSUPPORTED if GUI initialization failed,
 *                    EFI_ABORTED if user cancelled the selection
 */
EFI_STATUS embloader_gui_menu_start(embloader_loader **selected) {
	EFI_GRAPHICS_OUTPUT_BLT_PIXEL px;
	struct gui_menu_ctx ctx;
	EFI_STATUS status;
	if (!selected) return EFI_INVALID_PARAMETER;
	memset(&ctx, 0, sizeof(ctx));
	ctx.selected = selected;
	ctx.ret = EFI_UNSUPPORTED;
	*selected = NULL;
	ctx.timeout = g_embloader.menu->timeout;
	status = gui_init(&ctx);
	if (EFI_ERROR(status)) return status;
	if (ticks_usec() > 0)
		lv_tick_set_cb(ticks_msec_u32);
	lv_delay_set_cb(uefi_delay);
	serial_alt_init(&ctx);
	log_info("lvgl gui menu started");
	ctx.running = true;
	set_graphics_mode(true);
	if (!load_xml_from("menu-main", "menu.gui.xml-main", "menu-main.xml")) goto done;
	if (!load_xml_from("menu-item", "menu.gui.xml-item", "menu-item.xml")) goto done;
	if (!draw_menu_main(&ctx)) goto done;
	if (!draw_menu_items(&ctx)) goto done;
	if (ctx.timeout >= 0 && g_embloader.menu->default_entry)
		ctx.timer = lv_timer_create(timer_cb, 1000, &ctx);
	while (ctx.running) {
		gBS->Stall(10000);
		lv_tick_inc(10);
		lv_timer_handler();
	}
done:
	lv_deinit();
	set_graphics_mode(false);
	memset(&px, 0, sizeof(px));
	ctx.gop->Blt(
		ctx.gop, &px, EfiBltVideoFill, 0, 0, 0, 0,
		ctx.gop->Mode->Info->HorizontalResolution,
		ctx.gop->Mode->Info->VerticalResolution, 0
	);
	return ctx.ret;
}
