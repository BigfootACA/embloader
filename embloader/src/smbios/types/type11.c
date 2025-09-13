#include "../smbios.h"

void smbios_load_type11(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE11 *t = ptr.Type11;
	confignode_path_set_int(m, "count", t->StringCount);
}
