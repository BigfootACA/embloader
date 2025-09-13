#include "../smbios.h"

static const char *memory_array_location[] = {
	"null", "other", "unknown", "system_board", "isa_addon_card", "eisa_addon_card",
	"pci_addon_card", "mca_addon_card", "pcmcia_addon_card", "proprietary_addon_card", "nubus"
};

static const char *memory_array_use[] = {
	"null", "other", "unknown", "system_memory", "video_memory", "flash_memory", "non_volatile_ram", "cache_memory"
};

static const char *memory_error_correction[] = {
	"null", "other", "unknown", "none", "parity", "single_bit_ecc", "multi_bit_ecc", "crc"
};

void smbios_load_type16(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE16 *t = ptr.Type16;
	confignode_path_set_string(m, "location",
		t->Location < sizeof(memory_array_location)/sizeof(memory_array_location[0]) ?
		memory_array_location[t->Location] : "unknown");
	confignode_path_set_string(m, "use",
		t->Use < sizeof(memory_array_use)/sizeof(memory_array_use[0]) ?
		memory_array_use[t->Use] : "unknown");
	confignode_path_set_string(m, "memory_error_correction",
		t->MemoryErrorCorrection < sizeof(memory_error_correction)/sizeof(memory_error_correction[0]) ?
		memory_error_correction[t->MemoryErrorCorrection] : "unknown");
	if (ctx->version >= SMBIOS_VER(2,7) && t->MaximumCapacity == 0x80000000) {
		confignode_path_set_int(m, "maximum_capacity", t->ExtendedMaximumCapacity);
	} else {
		confignode_path_set_int(m, "maximum_capacity", t->MaximumCapacity);
	}
	confignode_path_set_int(m, "memory_error_information_handle", t->MemoryErrorInformationHandle);
	confignode_path_set_int(m, "number_of_memory_devices", t->NumberOfMemoryDevices);
}
