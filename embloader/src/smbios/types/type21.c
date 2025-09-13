#include "../smbios.h"

void smbios_load_type21(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE21 *t = ptr.Type21;
	confignode_path_set_int(m, "type", t->Type);
	confignode_path_set_int(m, "interface", t->Interface);
	confignode_path_set_int(m, "number_of_buttons", t->NumberOfButtons);
}
