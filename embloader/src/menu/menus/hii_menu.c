#include "menu.h"

EFI_STATUS embloader_hii_menu_start(embloader_loader **selected, uint64_t *flags) {
	if (flags) *flags = 0;
	if (!selected) return EFI_INVALID_PARAMETER;
	*selected = NULL;
	return EFI_UNSUPPORTED;
}
