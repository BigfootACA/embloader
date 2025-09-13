#include "../smbios.h"

void smbios_load_type30(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE30 *t = ptr.Type30;
	embloader_smbios_set_config_string(ctx, ptr, m, "manufacturer_name", t->ManufacturerName);
	confignode_path_set_int(m, "connections", t->Connections);
}
