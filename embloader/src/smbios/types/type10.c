#include "../smbios.h"
#include <string.h>

void smbios_load_type10(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE10 *t = ptr.Type10;
	size_t offset = OFFSET_OF(SMBIOS_TABLE_TYPE10, Device);
	size_t available = t->Hdr.Length > offset ? t->Hdr.Length - offset : 0;
	size_t device_count = available / sizeof(DEVICE_STRUCT);
	confignode_path_set_int(m, "device_count", device_count);
	confignode *device_array = confignode_new_array();
	confignode_path_map_set(m, "devices", device_array);
	for (size_t i = 0; i < device_count; ++i) {
		DEVICE_STRUCT entry;
		memcpy(&entry, ptr.Raw + offset + i * sizeof(entry), sizeof(entry));
		confignode *device = confignode_new_map();
		confignode_array_append(device_array, device);
		confignode_path_set_int(device, "type", entry.DeviceType & 0x7F);
		confignode_path_set_bool(device, "enabled", (entry.DeviceType & 0x80) != 0);
		embloader_smbios_set_config_string(ctx, ptr, device, "description", entry.DescriptionString);
	}
}
