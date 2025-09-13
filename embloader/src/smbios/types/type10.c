#include "../smbios.h"

void smbios_load_type10(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE10 *t = ptr.Type10;
	int device_count = (t->Hdr.Length - sizeof(SMBIOS_STRUCTURE)) / sizeof(DEVICE_STRUCT);
	confignode_path_set_int(m, "device_count", device_count);
	confignode *device_array = confignode_new_array();
	confignode_path_map_set(m, "devices", device_array);
	for (int i = 0; i < device_count; ++i) {
		confignode *device = confignode_new_map();
		confignode_array_append(device_array, device);
		confignode_path_set_int(device, "type", t->Device[i].DeviceType & 0x7F);
		confignode_path_set_bool(device, "enabled", (t->Device[i].DeviceType & 0x80) != 0);
		embloader_smbios_set_config_string(ctx, ptr, device, "description", t->Device[i].DescriptionString);
	}
}
