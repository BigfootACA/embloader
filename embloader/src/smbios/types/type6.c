#include "../smbios.h"

void smbios_load_type6(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE6 *t = ptr.Type6;
	confignode_path_set_int(m, "error_status", t->ErrorStatus);
	confignode_path_set_int(m, "current_speed", t->CurrentSpeed);
}
