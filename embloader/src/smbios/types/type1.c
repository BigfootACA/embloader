#include "../smbios.h"

void smbios_load_type1(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE1 *t = ptr.Type1;
	embloader_smbios_set_config_string(ctx, ptr, m, "manufacturer", t->Manufacturer);
	embloader_smbios_set_config_string(ctx, ptr, m, "product_name", t->ProductName);
	embloader_smbios_set_config_string(ctx, ptr, m, "version", t->Version);
	embloader_smbios_set_config_string(ctx, ptr, m, "serial_number", t->SerialNumber);
	embloader_smbios_set_config_guid(ctx, ptr, m, "uuid", &t->Uuid);
	confignode_path_set_bool(m, "wake_up_type", t->WakeUpType);
	embloader_smbios_set_config_string(ctx, ptr, m, "sku_number", t->SKUNumber);
	embloader_smbios_set_config_string(ctx, ptr, m, "family", t->Family);
}
