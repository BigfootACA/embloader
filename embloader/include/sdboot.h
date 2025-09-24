#ifndef SDBOOT_H
#define SDBOOT_H
#include "bootmenu.h"
typedef struct sdboot_menu sdboot_menu;
typedef struct sdboot_boot_menu sdboot_boot_menu;
typedef struct sdboot_boot_loader sdboot_boot_loader;
typedef enum sdboot_console_mode sdboot_console_mode;
typedef enum sdboot_secureboot_enroll sdboot_secureboot_enroll;

#define LOADER_GUID { 0x4a67b082, 0x0a4c, 0x41cf, { 0xb6, 0xc7, 0x44, 0x0b, 0x29, 0xbb, 0x8c, 0x4f } }

/**
 * Console display modes for boot menu
 */
enum sdboot_console_mode {
	MODE_UEFI_80_25,     ///< 80x25 text mode
	MODE_UEFI_80_50,     ///< 80x50 text mode
	MODE_FIRMWARE_FIRST, ///< Use firmware default mode
	MODE_AUTO,           ///< Auto-detect best mode
	MODE_MAX,            ///< Maximum available mode
	MODE_KEEP,           ///< Keep current mode
};

/**
 * SecureBoot key enrollment options
 */
enum sdboot_secureboot_enroll {
	ENROLL_OFF,      ///< Disable enrollment
	ENROLL_MANUAL,   ///< Manual enrollment only
	ENROLL_IF_SAFE,  ///< Enroll if safe to do so
	ENROLL_FORCE,    ///< Force enrollment
};

/**
 * Boot menu configuration settings
 * Contains global settings that affect the boot menu behavior
 */
struct sdboot_boot_menu {
	char *default_entry;                        ///< Default entry to boot
	int timeout;                                ///< Menu timeout in seconds
	sdboot_console_mode console_mode;           ///< Console display mode (*unsupported)
	bool editor;                                ///< Enable boot entry editor (*unsupported)
	bool auto_entries;                          ///< Auto-discover entries
	bool auto_firmware;                         ///< Auto-detect firmware
	bool auto_reboot;                           ///< Auto-reboot on failure
	bool auto_poweroff;                         ///< Auto-poweroff on shutdown
	bool beep;                                  ///< Enable beep sounds (*unsupported)
	sdboot_secureboot_enroll secureboot_enroll; ///< SecureBoot enrollment mode (*unsupported)
	bool reboot_for_bitlocker;                  ///< Reboot for BitLocker (*unsupported)
};

/**
 * Boot loader entry configuration
 * Represents a single bootable entry with all its parameters
 */
struct sdboot_boot_loader {
	EFI_HANDLE root_handle;  ///< Handle of device containing the config file
	EFI_FILE_PROTOCOL *root; ///< Root directory of the config file
	char *name;              ///< Entry name (from filename)
	char *title;             ///< Display title for menu
	char *version;           ///< Version string (as ktype)
	char *machine_id;        ///< Machine ID (*unsupported)
	char *sort_key;          ///< Sort key for ordering (*unsupported)
	char *kernel;            ///< Linux EFI kernel to boot
	char *efi;               ///< EFI application to start
	list *initramfs;         ///< List of initramfs files
	list *options;           ///< List of boot arguments/options
	char *devicetree;        ///< Device tree blob file
	list *dtoverlay;         ///< List of device tree overlays
	embloader_arch arch;     ///< Target EFI architecture
};

/**
 * Complete menu structure
 * Contains menu configuration and list of available boot entries
 */
struct sdboot_menu {
	sdboot_boot_menu menu; ///< Global menu settings
	list *loaders;       ///< List of boot loader entries
};

// Menu loading and management functions

/** Load boot menu from configuration files */
extern EFI_STATUS sdboot_boot_load_menu();

// Structure initialization functions

/** Initialize menu structure to defaults */
extern void sdboot_menu_init(sdboot_menu* menu);

/** Initialize boot menu structure to defaults */
extern void sdboot_boot_menu_init(sdboot_boot_menu* menu);

/** Initialize boot loader structure to defaults */
extern void sdboot_boot_loader_init(sdboot_boot_loader* loader);

// Structure cleanup functions

/** Clean up menu structure (free allocated memory) */
extern void sdboot_menu_clean(sdboot_menu* menu);

/** Clean up boot menu structure (free allocated memory) */
extern void sdboot_boot_menu_clean(sdboot_boot_menu* menu);

/** Clean up boot loader structure (free allocated memory) */
extern void sdboot_boot_loader_clean(sdboot_boot_loader* loader);

// Structure allocation and deallocation functions

/** Free menu structure and its memory */
extern void sdboot_menu_free(sdboot_menu* menu);

/** Free boot menu structure and its memory */
extern void sdboot_boot_menu_free(sdboot_boot_menu* menu);

/** Free boot loader structure and its memory */
extern void sdboot_boot_loader_free(sdboot_boot_loader* loader);

// Structure creation functions

/** Create new menu structure */
extern sdboot_menu* sdboot_menu_new();

/** Create new boot menu structure */
extern sdboot_boot_menu* sdboot_boot_menu_new();

/** Create new boot loader structure */
extern sdboot_boot_loader* sdboot_boot_loader_new();

// Boot loader operations

/** Check if boot loader configuration is valid */
extern bool sdboot_boot_loader_check(sdboot_boot_loader *loader);

/** Execute/invoke the specified boot loader entry */
extern EFI_STATUS sdboot_boot_loader_invoke(sdboot_menu *menu, sdboot_boot_loader *loader);

extern EFI_GUID gLoaderGuid;
#endif
