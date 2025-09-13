#include "../smbios.h"

void smbios_load_type35(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE35 *t = ptr.Type35;
	embloader_smbios_set_config_string(ctx, ptr, m, "description", t->Description);
	confignode_path_set_int(m, "management_device_handle", t->ManagementDeviceHandle);
	confignode_path_set_int(m, "component_handle", t->ComponentHandle);
	confignode_path_set_int(m, "threshold_handle", t->ThresholdHandle);
}
