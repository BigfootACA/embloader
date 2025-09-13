#include "../smbios.h"

void smbios_load_type39(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE39 *t = ptr.Type39;
	confignode_path_set_int(m, "power_unit_group", t->PowerUnitGroup);
	embloader_smbios_set_config_string(ctx, ptr, m, "location", t->Location);
	embloader_smbios_set_config_string(ctx, ptr, m, "device_name", t->DeviceName);
	embloader_smbios_set_config_string(ctx, ptr, m, "manufacturer", t->Manufacturer);
	embloader_smbios_set_config_string(ctx, ptr, m, "serial_number", t->SerialNumber);
	embloader_smbios_set_config_string(ctx, ptr, m, "asset_tag_number", t->AssetTagNumber);
	embloader_smbios_set_config_string(ctx, ptr, m, "model_part_number", t->ModelPartNumber);
	embloader_smbios_set_config_string(ctx, ptr, m, "revision_level", t->RevisionLevel);
	confignode_path_set_int(m, "max_power_capacity", t->MaxPowerCapacity);
	confignode_path_set_int(m, "power_supply_characteristics", *(UINT16*)&t->PowerSupplyCharacteristics);
	confignode_path_set_int(m, "input_voltage_probe_handle", t->InputVoltageProbeHandle);
	confignode_path_set_int(m, "cooling_device_handle", t->CoolingDeviceHandle);
	confignode_path_set_int(m, "input_current_probe_handle", t->InputCurrentProbeHandle);
}
