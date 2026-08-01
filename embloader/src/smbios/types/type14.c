#include "../smbios.h"
#include <string.h>

void smbios_load_type14(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE14 *t = ptr.Type14;
	embloader_smbios_set_config_string(ctx, ptr, m, "name", t->GroupName);
	size_t offset = OFFSET_OF(SMBIOS_TABLE_TYPE14, Group);
	size_t available = t->Hdr.Length > offset ? t->Hdr.Length - offset : 0;
	size_t group_count = available / sizeof(GROUP_STRUCT);
	confignode_path_set_int(m, "count", group_count);
	confignode *p = confignode_new_array();
	confignode_path_map_set(m, "items", p);
	for (size_t i = 0; i < group_count; ++i) {
		GROUP_STRUCT group;
		memcpy(&group, ptr.Raw + offset + i * sizeof(group), sizeof(group));
		confignode *v = confignode_new_map();
		confignode_array_append(p, v);
		confignode_path_set_int(v, "type", group.ItemType);
		confignode_path_set_int(v, "handle", group.ItemHandle);
	}
}
