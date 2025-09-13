#include "../smbios.h"

void smbios_load_type46(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE46 *t = ptr.Type46;
	confignode_path_set_int(m, "string_property_id", t->StringPropertyId);
	embloader_smbios_set_config_string(ctx, ptr, m, "string_property_value", t->StringPropertyValue);
	confignode_path_set_int(m, "parent_handle", t->ParentHandle);
}
