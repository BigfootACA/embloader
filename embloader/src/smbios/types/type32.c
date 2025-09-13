#include "../smbios.h"

void smbios_load_type32(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE32 *t = ptr.Type32;
	confignode_path_set_int(m, "boot_status", *(UINT8*)&t->BootStatus);
}
