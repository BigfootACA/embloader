#include "../smbios.h"

void smbios_load_type43(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE43 *t = ptr.Type43;
	confignode_path_set_string_fmt(
		m, "vendor_id", "%02x%02x%02x%02x",
		t->VendorID[0], t->VendorID[1], t->VendorID[2], t->VendorID[3]
	);
	confignode_path_set_int(m, "major_spec_version", t->MajorSpecVersion);
	confignode_path_set_int(m, "minor_spec_version", t->MinorSpecVersion);
	confignode_path_set_int(m, "firmware_version1", t->FirmwareVersion1);
	confignode_path_set_int(m, "firmware_version2", t->FirmwareVersion2);
	embloader_smbios_set_config_string(ctx, ptr, m, "description", t->Description);
	confignode_path_set_int(m, "characteristics", *(UINT64*)&t->Characteristics);
	confignode_path_set_int(m, "oem_defined", t->OemDefined);
}
