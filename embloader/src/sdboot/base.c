#include "internal.h"
#include "log.h"

/**
 * @brief Get the current processor architecture
 *
 * Determines the architecture at compile time using EDK2 CPU macros.
 * Used for filtering architecture-specific boot entries.
 *
 * @return the current processor architecture enum value
 */
embloader_arch sdboot_get_current_arch() {
	#if defined(MDE_CPU_ARM)
	return ARCH_ARM;
	#elif defined(MDE_CPU_AARCH64)
	return ARCH_ARM64;
	#elif defined(MDE_CPU_X86)
	return ARCH_X86;
	#elif defined(MDE_CPU_X64)
	return ARCH_X86_64;
	#elif defined(MDE_CPU_RISCV64)
	return ARCH_RISCV64;
	#elif defined(MDE_CPU_LOONGARCH64)
	return ARCH_LOONGARCH64;
	#else
	#error unknown architecture
	#endif
}

/**
 * @brief Initialize a sdboot menu structure to default values
 *
 * Sets all fields to zero and initializes the nested menu structure.
 * Must be called before using a sdboot_menu structure.
 *
 * @param menu the menu structure to initialize (may be NULL - no-op)
 */
void sdboot_menu_init(sdboot_menu* menu) {
	if (!menu) return;
	memset(menu, 0, sizeof(sdboot_menu));
	sdboot_boot_menu_init(&menu->menu);
	menu->loaders = NULL;
}

/**
 * @brief Initialize a boot menu configuration structure to default values
 *
 * Sets timeout to 0, console mode to keep current, and all boolean
 * flags to false/off.
 *
 * @param menu the boot menu structure to initialize (may be NULL - no-op)
 */
void sdboot_boot_menu_init(sdboot_boot_menu* menu) {
	if (!menu) return;
	memset(menu, 0, sizeof(sdboot_boot_menu));
	menu->default_entry = NULL;
	menu->timeout = 0;
	menu->console_mode = MODE_KEEP;
	menu->editor = 0;
	menu->auto_entries = 0;
	menu->auto_firmware = 0;
	menu->auto_reboot = 0;
	menu->auto_poweroff = 0;
	menu->beep = 0;
	menu->secureboot_enroll = ENROLL_OFF;
	menu->reboot_for_bitlocker = 0;
}

/**
 * @brief Initialize a boot loader entry structure to default values
 *
 * Sets all string pointers to NULL, lists to NULL, and architecture
 * to the current system architecture.
 *
 * @param loader the boot loader structure to initialize (may be NULL - no-op)
 */
void sdboot_boot_loader_init(sdboot_boot_loader* loader) {
	if (!loader) return;
	memset(loader, 0, sizeof(sdboot_boot_loader));
	loader->name = NULL;
	loader->title = NULL;
	loader->version = NULL;
	loader->machine_id = NULL;
	loader->sort_key = NULL;
	loader->kernel = NULL;
	loader->efi = NULL;
	loader->initramfs = NULL;
	loader->options = NULL;
	loader->devicetree = NULL;
	loader->dtoverlay = NULL;
	loader->arch = sdboot_get_current_arch();
}

static int free_loader(void *ptr) {
	sdboot_boot_loader_free(ptr);
	return 0;
}

/**
 * @brief Clean up all resources owned by a sdboot menu structure
 *
 * Frees all allocated memory including loader list and nested structures.
 * The structure is reset to zero after cleanup.
 *
 * @param menu the menu structure to clean up (may be NULL - no-op)
 */
void sdboot_menu_clean(sdboot_menu* menu) {
	if (!menu) return;
	sdboot_boot_menu_clean(&menu->menu);
	list_free_all(menu->loaders, free_loader);
	memset(menu, 0, sizeof(sdboot_menu));
}

/**
 * @brief Clean up all resources owned by a boot menu configuration structure
 *
 * Frees the default entry string and resets structure to zero.
 *
 * @param menu the boot menu structure to clean up (may be NULL - no-op)
 */
void sdboot_boot_menu_clean(sdboot_boot_menu* menu) {
	if (!menu) return;
	free(menu->default_entry);
	memset(menu, 0, sizeof(sdboot_boot_menu));
}

/**
 * @brief Clean up all resources owned by a boot loader entry structure
 *
 * Frees all allocated strings and lists, but does not close EFI file handles.
 * The structure is reset to zero after cleanup.
 *
 * @param loader the boot loader structure to clean up (may be NULL - no-op)
 */
void sdboot_boot_loader_clean(sdboot_boot_loader* loader) {
	if (!loader) return;
	if (loader->root) loader->root = NULL;
	if (loader->name) free(loader->name);
	if (loader->title) free(loader->title);
	if (loader->version) free(loader->version);
	if (loader->machine_id) free(loader->machine_id);
	if (loader->sort_key) free(loader->sort_key);
	if (loader->kernel) free(loader->kernel);
	if (loader->efi) free(loader->efi);
	if (loader->initramfs) list_free_all_def(loader->initramfs);
	if (loader->options) list_free_all_def(loader->options);
	if (loader->devicetree) free(loader->devicetree);
	if (loader->dtoverlay) list_free_all_def(loader->dtoverlay);
	memset(loader, 0, sizeof(sdboot_boot_loader));
}

/**
 * @brief Clean up and free a sdboot menu structure
 *
 * Calls sdboot_menu_clean() to release resources, then frees the structure itself.
 *
 * @param menu the menu structure to free (may be NULL - no-op)
 */
void sdboot_menu_free(sdboot_menu* menu) {
	if (!menu) return;
	sdboot_menu_clean(menu);
	free(menu);
}

/**
 * @brief Clean up and free a boot menu configuration structure
 *
 * Calls sdboot_boot_menu_clean() to release resources, then frees the structure itself.
 *
 * @param menu the boot menu structure to free (may be NULL - no-op)
 */
void sdboot_boot_menu_free(sdboot_boot_menu* menu) {
	if (!menu) return;
	sdboot_boot_menu_clean(menu);
	free(menu);
}

/**
 * @brief Clean up and free a boot loader entry structure
 *
 * Calls sdboot_boot_loader_clean() to release resources, then frees the structure itself.
 *
 * @param loader the boot loader structure to free (may be NULL - no-op)
 */
void sdboot_boot_loader_free(sdboot_boot_loader* loader) {
	if (!loader) return;
	sdboot_boot_loader_clean(loader);
	free(loader);
}

/**
 * @brief Create and initialize a new sdboot menu structure
 *
 * Allocates memory and initializes the structure with default values.
 *
 * @return newly allocated and initialized sdboot menu, or NULL on allocation failure
 */
sdboot_menu* sdboot_menu_new() {
	sdboot_menu* menu = malloc(sizeof(sdboot_menu));
	if (!menu) return NULL;
	sdboot_menu_init(menu);
	return menu;
}

/**
 * @brief Create and initialize a new boot menu configuration structure
 *
 * Allocates memory and initializes the structure with default values.
 *
 * @return newly allocated and initialized boot menu, or NULL on allocation failure
 */
sdboot_boot_menu* sdboot_boot_menu_new() {
	sdboot_boot_menu* menu = malloc(sizeof(sdboot_boot_menu));
	if (!menu) return NULL;
	sdboot_boot_menu_init(menu);
	return menu;
}

/**
 * @brief Create and initialize a new boot loader entry structure
 *
 * Allocates memory and initializes the structure with default values.
 *
 * @return newly allocated and initialized boot loader, or NULL on allocation failure
 */
sdboot_boot_loader* sdboot_boot_loader_new() {
	sdboot_boot_loader* loader = malloc(sizeof(sdboot_boot_loader));
	if (!loader) return NULL;
	sdboot_boot_loader_init(loader);
	return loader;
}

/**
 * @brief Validate a boot loader entry configuration
 *
 * Checks for required fields, mutual exclusions, and logical consistency.
 * Logs specific warning messages for validation failures.
 *
 * @param loader the boot loader structure to validate (may be NULL)
 * @return true if loader is valid and usable, false otherwise
 */
bool sdboot_boot_loader_check(sdboot_boot_loader *loader) {
	if (!loader) return false;
	if (!loader->name) {
		log_warning("sdboot loader missing name");
		return false;
	}
	if (loader->kernel && loader->efi) {
		log_warning("sdboot loader %s has both kernel and efi", loader->name);
		return false;
	}
	if (!loader->kernel && !loader->efi) {
		log_warning("sdboot loader %s has neither kernel nor efi", loader->name);
		return false;
	}
	return true;
}
