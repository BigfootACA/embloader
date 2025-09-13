#include "../smbios.h"

void smbios_load_type42(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE42 *t = ptr.Type42;
	confignode_path_set_int(m, "interface_type", t->InterfaceType);
}
