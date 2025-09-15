[Defines]
  PLATFORM_NAME                  = embloader
  PLATFORM_GUID                  = 35C91970-52DF-4E5F-9E39-209D15EF9EE6
  PLATFORM_VERSION               = 1.02
  DSC_SPECIFICATION              = 0x00010006
  SUPPORTED_ARCHITECTURES        = IA32|X64|ARM|AARCH64|RISCV64|LOONGARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT

!include MdePkg/MdeLibs.dsc.inc

[Packages]
  MdePkg/MdePkg.dec

[LibraryClasses]
  embloader|embloader/src/main/embloader.inf
  embloader_lib|embloader/src/lib/lib.inf
  embloader_encode|embloader/src/encode/encode.inf
  embloader_configfile|embloader/src/configfile/configfile.inf
  embloader_smbios|embloader/src/smbios/smbios.inf
  embloader_menu|embloader/src/menu/menu.inf
  embloader_loader|embloader/src/loader/loader.inf
  embloader_linux|embloader/src/linux/linux.inf
  embloader_sdboot|embloader/src/sdboot/sdboot.inf
  embloader_resource|embloader/src/resource/resource.inf
  jsonc|embloader/ext/jsonc.inf
  libyaml|embloader/ext/libyaml.inf
  newlib|embloader/ext/newlib.inf
  libfdt|embloader/ext/libfdt.inf
  libufdt|embloader/ext/libufdt.inf
  nanosvg|embloader/ext/nanosvg.inf
  stb|embloader/ext/stb.inf
  regexp|embloader/ext/regexp.inf
  lvgl|embloader/ext/lvgl.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  DebugLib|MdePkg/Library/UefiDebugLibConOut/UefiDebugLibConOut.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  PcdLib|MdePkg/Library/DxePcdLib/DxePcdLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf

[Components]
  embloader/src/main/embloader.inf

[BuildOptions]
  GCC:*_*_*_CC_FLAGS = -D_HAVE_LONG_DOUBLE -U_FORTIFY_SOURCE -Wno-char-subscripts
  GCC:RELEASE_*_*_CC_FLAGS = -DEDK2_TARGET_RELEASE
  GCC:DEBUG_*_*_CC_FLAGS = -DEDK2_TARGET_DEBUG
