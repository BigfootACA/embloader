#include "menu.h"

EFI_STATUS embloader_hii_menu_start(embloader_loader **selected) {
	if (!selected) return EFI_INVALID_PARAMETER;
	*selected = NULL;
	return EFI_UNSUPPORTED;
}
