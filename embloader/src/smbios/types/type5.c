#include "../smbios.h"

static const char *memory_error_detect_method[] = {
	"other", "unknown", "none", "parity", "single_bit_ecc", "multi_bit_ecc", "crc"
};
static const char *memory_support_interleave_type[] = {
	"other", "unknown", "none", "interleave_2_way", "interleave_4_way", "interleave_8_way"
};

void smbios_load_type5(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE5 *t = ptr.Type5;
	confignode_path_set_string(m, "error_detect_method",
		t->ErrDetectMethod < sizeof(memory_error_detect_method)/sizeof(memory_error_detect_method[0]) ?
		memory_error_detect_method[t->ErrDetectMethod] : "unknown");
	confignode_path_set_string(m, "support_interleave",
		t->SupportInterleave < sizeof(memory_support_interleave_type)/sizeof(memory_support_interleave_type[0]) ?
		memory_support_interleave_type[t->SupportInterleave] : "unknown");
	confignode_path_set_string(m, "current_interleave",
		t->CurrentInterleave < sizeof(memory_support_interleave_type)/sizeof(memory_support_interleave_type[0]) ?
		memory_support_interleave_type[t->CurrentInterleave] : "unknown");
	confignode_path_set_int(m, "max_memory_module_size", t->MaxMemoryModuleSize);
	confignode_path_set_int(m, "support_memory_type", t->SupportMemoryType);
	confignode_path_set_int(m, "memory_module_voltage", t->MemoryModuleVoltage);
	confignode_path_set_int(m, "associated_memory_slot_num", t->AssociatedMemorySlotNum);
}
