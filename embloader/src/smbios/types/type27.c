#include "../smbios.h"

void smbios_load_type27(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE27 *t = ptr.Type27;
	confignode_path_set_int(m, "temperature_probe_handle", t->TemperatureProbeHandle);
	confignode_path_set_int(m, "device_type_and_status", *(UINT8*)&t->DeviceTypeAndStatus);
	confignode_path_set_int(m, "cooling_unit_group", t->CoolingUnitGroup);
	confignode_path_set_int(m, "oem_defined", t->OEMDefined);
	confignode_path_set_int(m, "nominal_speed", t->NominalSpeed);
	embloader_smbios_set_config_string(ctx, ptr, m, "description", t->Description);
}
