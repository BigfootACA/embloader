#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "embloader.h"
#include "efi-utils.h"
#include "encode.h"
#include "log.h"

/**
 * @brief Start an EFI executable image
 *
 * This function loads and starts an EFI executable image from either memory
 * or a device path, with optional command line arguments. It handles EFI
 * image loading, protocol setup, and execution.
 *
 * @param dp Device path protocol for loading from device (can be NULL if img provided)
 * @param img Pointer to image data in memory (can be NULL if dp provided)
 * @param len Size of image data in bytes (ignored if img is NULL)
 * @param cmdline Command line arguments for the executable (can be NULL)
 * @return EFI_STATUS No return on successful boot, or appropriate error code on failure
 */
EFI_STATUS embloader_start_efi(
	EFI_DEVICE_PATH_PROTOCOL *dp,
	void *img, size_t len,
	const char *cmdline
) {
	EFI_STATUS status;
	EFI_HANDLE image = NULL;
	EFI_LOADED_IMAGE_PROTOCOL *li = NULL;
	CHAR16 *options = NULL;
	char *s;
	if (img) log_info("load efi image at %p...", img);
	else if (dp && (s = efi_device_path_to_text(dp))) {
		log_info("load efi image from %s...", s);
		free(s);
	} else log_info("load efi image...");
	status = gBS->LoadImage(FALSE, gImageHandle, dp, img, len, &image);
	if (EFI_ERROR(status) || !image) {
		log_error("LoadImage failed: %s", efi_status_to_string(status));
		goto fail;
	}
	status = gBS->OpenProtocol(
		image, &gEfiLoadedImageProtocolGuid,
		(VOID**)&li, gImageHandle, NULL,
		EFI_OPEN_PROTOCOL_GET_PROTOCOL
	);
	if (EFI_ERROR(status) || !li) {
		log_error("open LoadedImage failed: %s", efi_status_to_string(status));
		goto fail;
	}
	if (li->ImageCodeType != EfiLoaderCode) {
		log_error("target image is not a UEFI application");
		status = EFI_UNSUPPORTED;
		goto fail;
	}
	if (cmdline && cmdline[0]) {
		if (!(options = encode_utf8_to_utf16(cmdline))) {
			log_error("failed to convert cmdline");
			goto fail;
		}
		li->LoadOptions = options;
		li->LoadOptionsSize = StrSize(options);
		log_info("use cmdline %s", cmdline);
	}
	log_info("start efi image...");
	status = gBS->StartImage(image, NULL, NULL);
	if (EFI_ERROR(status))
		log_error("StartImage failed: %s", efi_status_to_string(status));
fail:
	if (options) {
		if (li) li->LoadOptions = NULL, li->LoadOptionsSize = 0;
		free(options);
	}
	if (li) gBS->CloseProtocol(
		image, &gEfiLoadedImageProtocolGuid,
		gImageHandle, NULL
	);
	if (image) gBS->UnloadImage(image);
	return status;
}
