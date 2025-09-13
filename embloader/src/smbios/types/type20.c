#include "../smbios.h"

void smbios_load_type20(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE20 *t = ptr.Type20;
	confignode_path_set_int(m, "starting_address", t->StartingAddress);
	confignode_path_set_int(m, "ending_address", t->EndingAddress);
	confignode_path_set_int(m, "memory_device_handle", t->MemoryDeviceHandle);
	confignode_path_set_int(m, "memory_array_mapped_address_handle", t->MemoryArrayMappedAddressHandle);
	confignode_path_set_int(m, "partition_row_position", t->PartitionRowPosition);
	confignode_path_set_int(m, "interleave_position", t->InterleavePosition);
	confignode_path_set_int(m, "interleaved_data_depth", t->InterleavedDataDepth);
	if (ctx->version >= SMBIOS_VER(2,7)) {
		confignode_path_set_int(m, "extended_starting_address", t->ExtendedStartingAddress);
		confignode_path_set_int(m, "extended_ending_address", t->ExtendedEndingAddress);
	}
}
