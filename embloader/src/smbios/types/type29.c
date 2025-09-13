#include "../smbios.h"

void smbios_load_type29(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE29 *t = ptr.Type29;
	embloader_smbios_set_config_string(ctx, ptr, m, "description", t->Description);
	confignode_path_set_int(m, "probe_site", t->LocationAndStatus.ElectricalCurrentProbeSite);
	confignode_path_set_int(m, "probe_status", t->LocationAndStatus.ElectricalCurrentProbeStatus);
	confignode_path_set_int(m, "maximum_value", t->MaximumValue);
	confignode_path_set_int(m, "minimum_value", t->MinimumValue);
	confignode_path_set_int(m, "resolution", t->Resolution);
	confignode_path_set_int(m, "tolerance", t->Tolerance);
	confignode_path_set_int(m, "accuracy", t->Accuracy);
	confignode_path_set_int(m, "oem_defined", t->OEMDefined);
	confignode_path_set_int(m, "nominal_value", t->NominalValue);
}
