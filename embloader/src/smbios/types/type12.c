#include "../smbios.h"

void smbios_load_type12(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE12 *t = ptr.Type12;
	confignode_path_set_int(m, "count", t->StringCount);
}
