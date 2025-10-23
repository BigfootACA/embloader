#include <stdio.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextOut.h>
#include "bootmenu.h"
#include "embloader.h"
#include "efi-utils.h"

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
EFI_STATUS embloader_text_menu_start(embloader_loader **selected) {
	embloader_menu *menu = g_embloader.menu;
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
			*selected = item;
			return EFI_SUCCESS;
		} while ((p = p->next));
	}
	return EFI_NOT_STARTED;
}
