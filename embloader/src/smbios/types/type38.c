#include "../smbios.h"

void smbios_load_type38(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE38 *t = ptr.Type38;
	confignode_path_set_int(m, "interface_type", t->InterfaceType);
	confignode_path_set_int(m, "ipmi_specification_revision", t->IPMISpecificationRevision);
	confignode_path_set_int(m, "i2c_slave_address", t->I2CSlaveAddress);
	confignode_path_set_int(m, "nv_storage_device_address", t->NVStorageDeviceAddress);
	confignode_path_set_int(m, "base_address", t->BaseAddress);
	confignode_path_set_int(m, "base_address_modifier", t->BaseAddressModifier_InterruptInfo);
	confignode_path_set_int(m, "interrupt_number", t->InterruptNumber);
}
