#include "../smbios.h"

void smbios_load_type44(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE44 *t = ptr.Type44;
	confignode_path_set_int(m, "ref_handle", t->RefHandle);
	confignode_path_set_int(m, "processor.length", t->ProcessorSpecificBlock.Length);
	confignode_path_set_int(m, "processor.arch_type", t->ProcessorSpecificBlock.ProcessorArchType);
}
