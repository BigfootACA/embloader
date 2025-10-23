#include <stdio.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextOut.h>
#include "bootmenu.h"
#include "embloader.h"
#include "efi-utils.h"
#include "str-utils.h"

static void show_ask_editor(embloader_loader *item, uint64_t *flags) {
	EFI_STATUS status;
	char edit_input[8], *buffer;
	if (!item->editor) return;
	while (true) {
		printf("Edit bootargs? (y/n): ");
		fflush(stdout);
		memset(edit_input, 0, sizeof(edit_input));
		status = efi_readline(
			gST->ConIn, gST->ConOut,
			edit_input, sizeof(edit_input),
			g_embloader.menu->timeout
		);
		if (EFI_ERROR(status)) return;
		if (string_is_true(edit_input)) break;
		if (string_is_false(edit_input)) return;
		printf("Invalid choice '%s'\n", edit_input);
		fflush(stdout);
	}
	if (flags) *flags |= EMBLOADER_FLAG_EDITED;
	if (!(buffer = malloc(16384))) return;
	memset(buffer, 0, 16384);
	if (item->bootargs && item->bootargs[0])
		strncpy(buffer, item->bootargs, 16383);
	printf("Edit bootargs: ");
	fflush(stdout);
	status = efi_readline(
		gST->ConIn, gST->ConOut,
		buffer, 16384, INT32_MAX
	);
	if (EFI_ERROR(status)) {
		free(buffer);
		return;
	}
	if (item->bootargs) free(item->bootargs);
	item->bootargs = buffer;
}

/**
 * @brief Display a simple text-based boot menu and handle user selection.
 * This function presents a console-based menu interface using printf for output
 * and keyboard input for selection. Displays available loaders and allows
 * user to select one for booting.
 *
 * @param selected pointer to receive the selected loader entry
 * @return EFI_SUCCESS if a loader was selected,
 *         EFI_INVALID_PARAMETER for invalid parameters,
 *         EFI_NOT_FOUND if no menu entries available,
 *         EFI_ABORTED if user cancelled the selection,
 *         other EFI errors on failure
 */
EFI_STATUS embloader_text_menu_start(embloader_loader **selected, uint64_t *flags) {
	embloader_menu *menu = g_embloader.menu;
	if (flags) *flags = 0;
	if (!menu || !selected) return EFI_INVALID_PARAMETER;
	*selected = NULL;
	printf("\n");
	printf("========================================\n");
	printf("= Embedded Bootloader menu\n");
	if (menu->title && menu->title[0])
		printf("= Menu:    %s\n", menu->title);
	if (g_embloader.device_name && g_embloader.device_name[0])
		printf("= Device:  %s\n", g_embloader.device_name);
	printf("========================================\n");
	list *p;
	int index = 1;
	int def_num = 1;
	embloader_loader *def_loader = NULL;
	if ((p = list_first(menu->loaders))) do {
		LIST_DATA_DECLARE(item, p, embloader_loader*);
		if (!item) continue;
		if (index == 1) def_loader = item;
		if (
			menu->default_entry && item->name &&
			strcmp(menu->default_entry, item->name) == 0
		) def_num = index, def_loader = item;
		printf("  %d. %s\n", index++, item->title ? item->title : "(unnamed)");
	} while ((p = p->next));
	printf("========================================\n");
	printf("Menu timeout: %d seconds\n", menu->timeout);
	if (def_loader) printf(
		"Default option: (%d) %s\n",
		def_num, def_loader->title ? def_loader->title : "(unnamed)"
	);
	while (true) {
		printf("Select an option (1-%d) or press Enter to boot default: ", index - 1);
		fflush(stdout);
		char input[16];
		memset(input, 0, sizeof(input));
		EFI_STATUS status = efi_readline(
			gST->ConIn, gST->ConOut,
			input, sizeof(input),
			menu->timeout
		);
		if ((
			status == EFI_TIMEOUT ||
			(status == EFI_SUCCESS && !input[0])
		) && def_loader) {
			printf(
				"Booting default option (%d) %s\n\n",
				def_num, def_loader->title ? def_loader->title : "(unnamed)"
			);
			*selected = def_loader;
			if (flags) *flags |= EMBLOADER_FLAG_AUTOBOOT;
			return EFI_SUCCESS;
		}
		if (EFI_ERROR(status)) return status;
		char *end = NULL;
		long choice = strtol(input, &end, 10);
		if (!end || *end != 0 || choice <= 0 || choice >= index) {
			printf("Invalid choice '%s'\n", input);
			continue;
		}
		int idx = 1;
		if ((p = list_first(menu->loaders))) do {
			LIST_DATA_DECLARE(item, p, embloader_loader*);
			if (!item || idx++ != choice)  continue;
			printf(
				"Choose option (%ld) %s\n\n",
				choice, item->title ? item->title : "(unnamed)"
			);
			if (flags) *flags |= EMBLOADER_FLAG_USERSELECT;
			show_ask_editor(item, flags);
			*selected = item;
			return EFI_SUCCESS;
		} while ((p = p->next));
	}
	return EFI_NOT_STARTED;
}
