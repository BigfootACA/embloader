#include "../smbios.h"

void smbios_load_type41(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE41 *t = ptr.Type41;
	embloader_smbios_set_config_string(ctx, ptr, m, "reference_designation", t->ReferenceDesignation);
	confignode_path_set_int(m, "device_type", t->DeviceType & 0x7F);
	confignode_path_set_bool(m, "device_status", (t->DeviceType & 0x80) != 0);
	confignode_path_set_int(m, "device_type_instance", t->DeviceTypeInstance);
	confignode_path_set_int(m, "segment_group_num", t->SegmentGroupNum);
	confignode_path_set_int(m, "bus_num", t->BusNum);
	confignode_path_set_int(m, "device_function_num", t->DevFuncNum);
}
