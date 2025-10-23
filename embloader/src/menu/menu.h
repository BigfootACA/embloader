#ifndef MENU_H
#define MENU_H
#include "bootmenu.h"
extern EFI_STATUS embloader_text_menu_start(embloader_loader **selected, uint64_t *flags);
extern EFI_STATUS embloader_tui_menu_start(embloader_loader **selected, uint64_t *flags);
extern EFI_STATUS embloader_hii_menu_start(embloader_loader **selected, uint64_t *flags);
extern EFI_STATUS embloader_gui_menu_start(embloader_loader **selected, uint64_t *flags);
#endif
