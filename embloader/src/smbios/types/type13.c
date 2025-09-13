#include "../smbios.h"

void smbios_load_type13(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE13 *t = ptr.Type13;
	confignode_path_set_int(m, "installable_languages", t->InstallableLanguages);
	confignode_path_set_int(m, "flags", t->Flags);
	embloader_smbios_set_config_string(ctx, ptr, m, "current_languages", t->CurrentLanguages);
}
