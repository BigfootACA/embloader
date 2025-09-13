#include "../smbios.h"

void smbios_load_type22(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE22 *t = ptr.Type22;
	embloader_smbios_set_config_string(ctx, ptr, m, "location", t->Location);
	embloader_smbios_set_config_string(ctx, ptr, m, "manufacturer", t->Manufacturer);
	embloader_smbios_set_config_string(ctx, ptr, m, "manufacturer_date", t->ManufactureDate);
	embloader_smbios_set_config_string(ctx, ptr, m, "serial_number", t->SerialNumber);
	embloader_smbios_set_config_string(ctx, ptr, m, "device_name", t->DeviceName);
	confignode_path_set_int(m, "device_chemistry", t->DeviceChemistry);
	confignode_path_set_int(m, "device_capacity", t->DeviceCapacity);
	confignode_path_set_int(m, "design_voltage", t->DesignVoltage);
	embloader_smbios_set_config_string(ctx, ptr, m, "sbds_version_number", t->SBDSVersionNumber);
	confignode_path_set_int(m, "maximum_error_in_battery_data", t->MaximumErrorInBatteryData);
	if (ctx->version >= SMBIOS_VER(2,2)) {
		confignode_path_set_int(m, "sbds_serial_number", t->SBDSSerialNumber);
		confignode_path_set_int(m, "sbds_manufacture_date", t->SBDSManufactureDate);
		embloader_smbios_set_config_string(ctx, ptr, m, "sbds_device_chemistry", t->SBDSDeviceChemistry);
		confignode_path_set_int(m, "design_capacity_multiplier", t->DesignCapacityMultiplier);
		confignode_path_set_int(m, "oem_specific", t->OEMSpecific);
	}
}
