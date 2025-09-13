#include "../smbios.h"

static const char *cache_error_type[] = {
	"null", "other", "unknown", "none", "parity", "single_bit_ecc", "multi_bit_ecc"
};

static const char *cache_type[] = {
	"null", "other", "unknown", "instruction", "data", "unified"
};

static const char *cache_associativity[] = {
	"null", "other", "unknown", "direct_mapped", "2_way", "4_way", "fully", "8_way",
	"16_way", "12_way", "24_way", "32_way", "48_way", "64_way", "20_way"
};

void smbios_load_type7(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE7 *t = ptr.Type7;
	embloader_smbios_set_config_string(ctx, ptr, m, "socket_designation", t->SocketDesignation);
	confignode_path_set_int(m, "cache_configuration", t->CacheConfiguration);
	if (ctx->version >= SMBIOS_VER(3,1) && t->MaximumCacheSize == 0xFFFF) {
		confignode_path_set_int(m, "maximum_cache_size", t->MaximumCacheSize2);
	} else {
		confignode_path_set_int(m, "maximum_cache_size", t->MaximumCacheSize);
	}
	if (ctx->version >= SMBIOS_VER(3,1) && t->InstalledSize == 0xFFFF) {
		confignode_path_set_int(m, "installed_size", t->InstalledSize2);
	} else {
		confignode_path_set_int(m, "installed_size", t->InstalledSize);
	}
	confignode_path_set_bool(m, "sram_supported.other", t->SupportedSRAMType.Other);
	confignode_path_set_bool(m, "sram_supported.unknown", t->SupportedSRAMType.Unknown);
	confignode_path_set_bool(m, "sram_supported.non_burst", t->SupportedSRAMType.NonBurst);
	confignode_path_set_bool(m, "sram_supported.burst", t->SupportedSRAMType.Burst);
	confignode_path_set_bool(m, "sram_supported.pipeline_burst", t->SupportedSRAMType.PipelineBurst);
	confignode_path_set_bool(m, "sram_supported.synchronous", t->SupportedSRAMType.Synchronous);
	confignode_path_set_bool(m, "sram_supported.asynchronous", t->SupportedSRAMType.Asynchronous);
	confignode_path_set_bool(m, "sram_current.other", t->CurrentSRAMType.Other);
	confignode_path_set_bool(m, "sram_current.unknown", t->CurrentSRAMType.Unknown);
	confignode_path_set_bool(m, "sram_current.non_burst", t->CurrentSRAMType.NonBurst);
	confignode_path_set_bool(m, "sram_current.burst", t->CurrentSRAMType.Burst);
	confignode_path_set_bool(m, "sram_current.pipeline_burst", t->CurrentSRAMType.PipelineBurst);
	confignode_path_set_bool(m, "sram_current.synchronous", t->CurrentSRAMType.Synchronous);
	confignode_path_set_bool(m, "sram_current.asynchronous", t->CurrentSRAMType.Asynchronous);
	confignode_path_set_int(m, "cache_speed", t->CacheSpeed);
	confignode_path_set_string(m, "error_correction_type",
		t->ErrorCorrectionType < sizeof(cache_error_type)/sizeof(cache_error_type[0]) ?
		cache_error_type[t->ErrorCorrectionType] : "unknown");
	confignode_path_set_string(m, "system_cache_type",
		t->SystemCacheType < sizeof(cache_type)/sizeof(cache_type[0]) ?
		cache_type[t->SystemCacheType] : "unknown");
	confignode_path_set_string(m, "associativity",
		t->Associativity < sizeof(cache_associativity)/sizeof(cache_associativity[0]) ?
		cache_associativity[t->Associativity] : "unknown");
}
