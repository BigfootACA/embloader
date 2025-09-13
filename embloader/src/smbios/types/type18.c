#include "../smbios.h"

void smbios_load_type18(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE18 *t = ptr.Type18;
	confignode_path_set_int(m, "error_type", t->ErrorType);
	confignode_path_set_int(m, "error_granularity", t->ErrorGranularity);
	confignode_path_set_int(m, "error_operation", t->ErrorOperation);
	confignode_path_set_int(m, "vendor_syndrome", t->VendorSyndrome);
	confignode_path_set_int(m, "memory_array_error_address", t->MemoryArrayErrorAddress);
	confignode_path_set_int(m, "device_error_address", t->DeviceErrorAddress);
	confignode_path_set_int(m, "error_resolution", t->ErrorResolution);
}
