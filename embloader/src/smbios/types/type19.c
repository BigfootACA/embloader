#include "../smbios.h"

void smbios_load_type19(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE19 *t = ptr.Type19;
	confignode_path_set_int(m, "starting_address", t->StartingAddress);
	confignode_path_set_int(m, "ending_address", t->EndingAddress);
	confignode_path_set_int(m, "memory_array_handle", t->MemoryArrayHandle);
	confignode_path_set_int(m, "partition_width", t->PartitionWidth);
	if (ctx->version >= SMBIOS_VER(2,7)) {
		confignode_path_set_int(m, "extended_starting_address", t->ExtendedStartingAddress);
		confignode_path_set_int(m, "extended_ending_address", t->ExtendedEndingAddress);
	}
}
