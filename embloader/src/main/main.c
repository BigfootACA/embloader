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

EFI_STATUS EFIAPI efi_main(
	EFI_HANDLE ImageHandle,
	EFI_SYSTEM_TABLE *SystemTable
){
	log_info("embloader (Embedded Bootloader) version " EMBLOADER_VERSION);
	log_debug("function efi_main at %p", efi_main);
	embloader_init();
	g_embloader.start_time = ticks_usec();
	find_embloader_folder(&g_embloader.dir);
	if (g_embloader.dir.dir && !embloader_load_configs())
		log_warning("no config files loaded");
	if (confignode_path_get_bool(g_embloader.config, "log.print-config", true, NULL)) {
		log_debug("Final configuration:");
		confignode_print(g_embloader.config, config_print);
	}
	log_info("parsing smbios");
	embloader_load_smbios();
	if (confignode_path_get_bool(g_embloader.config, "log.print-sysinfo", false, NULL)) {
		log_debug("system information:");
		confignode_print(g_embloader.sysinfo, sysinfo_print);
	}
	if (!embloader_choose_device())
		log_warning("no matched device found in config");
	embloader_load_menu();
	sdboot_boot_load_menu();
	EFI_STATUS status = embloader_show_menu();
	log_info("exiting embloader");
	return status;
}
