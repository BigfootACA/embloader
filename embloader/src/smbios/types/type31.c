#include "../smbios.h"

void smbios_load_type31(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE31 *t = ptr.Type31;
	confignode_path_set_int(m, "checksum", t->Checksum);
	confignode_path_set_int(m, "bis_entry16", t->BisEntry16);
	confignode_path_set_int(m, "bis_entry32", t->BisEntry32);
}
