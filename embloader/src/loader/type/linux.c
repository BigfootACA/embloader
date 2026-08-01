#include "../loader.h"
#include "linuxboot.h"
#include "log.h"

EFI_STATUS embloader_loader_boot_linux(embloader_loader* loader) {
	if (!loader || !loader->node) return EFI_INVALID_PARAMETER;
#if !defined(MDE_CPU_AARCH64)
	log_error("direct Linux Image boot is only supported on AArch64");
	return EFI_UNSUPPORTED;
#else
	linux_bootinfo* info = linux_bootinfo_parse(loader->node);
	if (!info) {
		log_error("failed to parse Linux boot information");
		return EFI_LOAD_ERROR;
	}
	info->bootargs_override = linux_prepare_bootargs_string(loader->bootargs);
	if (!info->bootargs_override) {
		linux_bootinfo_clean(info);
		log_error("failed to prepare Linux boot arguments");
		return EFI_OUT_OF_RESOURCES;
	}
	info->root = g_embloader.dir.root;
	linux_data* data = linux_data_load(info);
	linux_bootinfo_clean(info);
	if (!data) {
		log_error("failed to load Linux Image boot data");
		return EFI_LOAD_ERROR;
	}
	EFI_STATUS status = linux_boot_use_aarch64(data);
	linux_data_clean(data);
	return status;
#endif
}
