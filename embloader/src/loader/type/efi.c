#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include "efi-utils.h"
#include "../loader.h"
#include "encode.h"
#include "log.h"

static EFI_DEVICE_PATH_PROTOCOL *find_path(embloader_loader *loader) {
	char *val;
	EFI_DEVICE_PATH_PROTOCOL *ret = NULL;
	if (!loader || !loader->node) return NULL;
	if ((val = confignode_path_get_string(
		loader->node, "device-path", NULL, NULL
	))) {
		CHAR16 *c;
		if ((c = encode_utf8_to_utf16(val))) {
			ret = ConvertTextToDevicePath(c);
			free(c);
		}
		if (!ret) log_warning("invalid device-path: %s", val);
		free(val);
		return ret;
	}
	if ((val = confignode_path_get_string(
		loader->node, "path", NULL, NULL
	))) {
		ret = efi_device_path_append_filepath(g_embloader.dir.dp, val);
		free(val);
		return ret;
	}
	return NULL;
}

/**
 * @brief Boot an EFI application with optional command line arguments.
 * This function loads and launches an EFI application specified by device path
 * or file path in the loader configuration, with optional command line parameters.
 *
 * @param loader pointer to the loader configuration containing EFI app path and cmdline
 * @return EFI_SUCCESS on successful launch, EFI_INVALID_PARAMETER for invalid
 *         parameters, EFI_LOAD_ERROR if path resolution fails, or error from EFI launch
 */
EFI_STATUS embloader_loader_boot_efi(embloader_loader *loader) {
	if (!loader || !loader->node) return EFI_INVALID_PARAMETER;
	char *cmdline = loader->bootargs ? strdup(loader->bootargs) :
		confignode_path_get_string(loader->node, "cmdline", NULL, NULL);
	EFI_DEVICE_PATH_PROTOCOL *dp = find_path(loader);
	if (!dp) return EFI_LOAD_ERROR;
	char *dpt = efi_device_path_to_text(dp);
	log_info(
		"Launching EFI application from %s%s%s...",
		dpt ?: "(unknown)",
		cmdline ? " with " : "",
		cmdline ?: ""
	);
	if (dpt) free(dpt);
	EFI_STATUS status = embloader_start_efi(dp, NULL, 0, cmdline);
	if (cmdline) free(cmdline);
	FreePool(dp);
	return status;
}
