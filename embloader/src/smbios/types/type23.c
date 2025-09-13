#include "../smbios.h"

void smbios_load_type23(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE23 *t = ptr.Type23;
	confignode_path_set_int(m, "capabilities", t->Capabilities);
	confignode_path_set_int(m, "reset_count", t->ResetCount);
	confignode_path_set_int(m, "reset_limit", t->ResetLimit);
	confignode_path_set_int(m, "timer_interval", t->TimerInterval);
	confignode_path_set_int(m, "timeout", t->Timeout);
}
