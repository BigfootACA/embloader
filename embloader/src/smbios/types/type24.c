#include "../smbios.h"

void smbios_load_type24(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE24 *t = ptr.Type24;
	confignode_path_set_int(m, "hardware_security_settings", t->HardwareSecuritySettings);
}
