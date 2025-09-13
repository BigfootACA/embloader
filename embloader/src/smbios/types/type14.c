#include "../smbios.h"

void smbios_load_type14(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE14 *t = ptr.Type14;
	embloader_smbios_set_config_string(ctx, ptr, m, "name", t->GroupName);
	int group_count = (t->Hdr.Length - sizeof(SMBIOS_STRUCTURE) - sizeof(SMBIOS_TABLE_STRING)) / sizeof(GROUP_STRUCT);
	confignode_path_set_int(m, "count", group_count);
	confignode *p = confignode_new_array();
	confignode_path_map_set(m, "items", p);
	for (int i = 0; i < group_count; ++i) {
		confignode *v = confignode_new_map();
		confignode_array_append(p, v);
		confignode_path_set_int(v, "type", t->Group[i].ItemType);
		confignode_path_set_int(v, "handle", t->Group[i].ItemHandle);
	}
}
