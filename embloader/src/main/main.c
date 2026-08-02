#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/SimpleFileSystem.h>
#include "embloader.h"
#include "bootmenu.h"
#include "sdboot.h"
#include "configfile.h"
#include "efi-utils.h"
#include "ticks.h"
#include "log.h"

embloader g_embloader = {};

static int sysinfo_print(const char *str) {
	log_print(LOG_DEBUG, "sysinfo", str);
	return 0;
}

static int config_print(const char *str) {
	log_print(LOG_DEBUG, "config", str);
	return 0;
}

#ifndef EMBLOADER_VERSION
#error EMBLOADER_VERSION is not set
#endif

static const char *bootloader_info =
	"#### LoaderInfo: systemd 999." EMBLOADER_VERSION " (embloader " EMBLOADER_VERSION ") ####\n\0"
	"#### EmbloaderVersion: embloader " EMBLOADER_VERSION " ####\n";

EFI_STATUS EFIAPI efi_main(
	EFI_HANDLE ImageHandle,
	EFI_SYSTEM_TABLE *SystemTable
){
	uint64_t init_usec = ticks_usec();
	EFI_STATUS status;
	if (!strstr(bootloader_info, "####")) return EFI_LOAD_ERROR;
	log_info("embloader (Embedded Bootloader) version " EMBLOADER_VERSION);
	log_debug("function efi_main at %p", efi_main);
	if (!embloader_init()) {
		log_error("failed to initialize embloader");
		return EFI_OUT_OF_RESOURCES;
	}
	g_embloader.start_time = init_usec;
	find_embloader_folder(&g_embloader.dir);
	status = sdboot_init_loader_variables();
	if (EFI_ERROR(status)) log_warning(
		"failed to export boot loader variables: %s",
		efi_status_to_string(status)
	);
	if (g_embloader.dir.dir && !embloader_load_configs())
		log_warning("no config files loaded");
	if (confignode_path_get_bool(g_embloader.config, "log.print-config", true, NULL)) {
		log_debug("Final configuration:");
		confignode_print(g_embloader.config, config_print);
	}
	log_init();
	log_info("parsing smbios");
	embloader_load_smbios();
	if (confignode_path_get_bool(g_embloader.config, "log.print-sysinfo", false, NULL)) {
		log_debug("system information:");
		confignode_print(g_embloader.sysinfo, sysinfo_print);
	}
	if (!embloader_choose_device())
		log_warning("no matched device found in config");
	gBS->SetWatchdogTimer(0, 0, 0, NULL);
	embloader_load_menu();
	status = sdboot_boot_load_menu();
	if (
		EFI_ERROR(status) &&
		status != EFI_NOT_FOUND && status != EFI_UNSUPPORTED
	) {
		log_error(
			"failed to load systemd-boot menu: %s",
			efi_status_to_string(status)
		);
		return status;
	}
	status = sdboot_init_menu_variables();
	if (EFI_ERROR(status)) log_warning(
		"failed to initialize boot loader menu variables: %s",
		efi_status_to_string(status)
	);
	status = embloader_show_menu(
		sdboot_set_menu_time,
		sdboot_set_selected_entry
	);
	log_info("exiting embloader");
	return status;
}
