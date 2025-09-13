#include "../smbios.h"

void smbios_load_type34(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE34 *t = ptr.Type34;
	embloader_smbios_set_config_string(ctx, ptr, m, "description", t->Description);
	confignode_path_set_int(m, "type", t->Type);
	confignode_path_set_int(m, "address", t->Address);
	confignode_path_set_int(m, "address_type", t->AddressType);
}
