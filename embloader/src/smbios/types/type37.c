#include "../smbios.h"

void smbios_load_type37(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE37 *t = ptr.Type37;
	confignode_path_set_int(m, "channel_type", t->ChannelType);
	confignode_path_set_int(m, "maximum_channel_load", t->MaximumChannelLoad);
	confignode_path_set_int(m, "memory_device_count", t->MemoryDeviceCount);
}
