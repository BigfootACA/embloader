#include "../smbios.h"

void smbios_load_type40(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE40 *t = ptr.Type40;
	confignode_path_set_int(m, "number_of_additional_information_entries", t->NumberOfAdditionalInformationEntries);
}
