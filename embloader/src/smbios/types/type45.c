#include "../smbios.h"

void smbios_load_type45(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE45 *t = ptr.Type45;
	embloader_smbios_set_config_string(ctx, ptr, m, "firmware.component_name", t->FirmwareComponentName);
	embloader_smbios_set_config_string(ctx, ptr, m, "firmware.version", t->FirmwareVersion);
	confignode_path_set_int(m, "firmware.version_format", t->FirmwareVersionFormat);
	embloader_smbios_set_config_string(ctx, ptr, m, "firmware.id", t->FirmwareId);
	confignode_path_set_int(m, "firmware.id_format", t->FirmwareIdFormat);
	embloader_smbios_set_config_string(ctx, ptr, m, "release_date", t->ReleaseDate);
	embloader_smbios_set_config_string(ctx, ptr, m, "manufacturer", t->Manufacturer);
	embloader_smbios_set_config_string(ctx, ptr, m, "lowest_supported_version", t->LowestSupportedVersion);
	confignode_path_set_int(m, "image_size", t->ImageSize);
	confignode_path_set_int(m, "state", t->State);
	confignode_path_set_int(m, "associated_component_count", t->AssociatedComponentCount);
}
