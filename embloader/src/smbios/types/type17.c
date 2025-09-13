#include "../smbios.h"

static const char *memory_form_factor[] = {
	"null", "other", "unknown", "simm", "sip", "chip", "dip", "zip", "proprietary_card",
	"dimm", "tsop", "row_of_chips", "rimm", "sodimm", "srimm", "fb_dimm", "die", "camm"
};

static const char *memory_device_type[] = {
	"null", "other", "unknown", "dram", "edram", "vram", "sram", "ram", "rom", "flash",
	"eeprom", "feprom", "eprom", "cdram", "3dram", "sdram", "sgram", "rdram", "ddr",
	"ddr2", "ddr2_fb_dimm", "reserved1", "reserved2", "reserved3", "ddr3", "fbd2",
	"ddr4", "lpddr", "lpddr2", "lpddr3", "lpddr4", "logical_non_volatile_device",
	"hbm", "hbm2", "ddr5", "lpddr5", "hbm3"
};

static const char *memory_technology[] = {
	"null", "other", "unknown", "dram", "nvdimm_n", "nvdimm_f", "nvdimm_p", "intel_optane_persistent_memory"
};

void smbios_load_type17(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE17 *t = ptr.Type17;
	confignode_path_set_int(m, "memory_array_handle", t->MemoryArrayHandle);
	confignode_path_set_int(m, "memory_error_information_handle", t->MemoryErrorInformationHandle);
	confignode_path_set_int(m, "total_width", t->TotalWidth);
	confignode_path_set_int(m, "data_width", t->DataWidth);
	if (ctx->version >= SMBIOS_VER(2,7) && t->Size == 0x7FFF) {
		confignode_path_set_int(m, "size", t->ExtendedSize);
	} else {
		confignode_path_set_int(m, "size", t->Size);
	}
	confignode_path_set_string(m, "form_factor",
		t->FormFactor < sizeof(memory_form_factor)/sizeof(memory_form_factor[0]) ?
		memory_form_factor[t->FormFactor] : "unknown");
	confignode_path_set_int(m, "device_set", t->DeviceSet);
	embloader_smbios_set_config_string(ctx, ptr, m, "device_locator", t->DeviceLocator);
	embloader_smbios_set_config_string(ctx, ptr, m, "bank_locator", t->BankLocator);
	confignode_path_set_string(m, "memory_type",
		t->MemoryType < sizeof(memory_device_type)/sizeof(memory_device_type[0]) ?
		memory_device_type[t->MemoryType] : "unknown");
	confignode_path_set_bool(m, "type_detail.other", t->TypeDetail.Other);
	confignode_path_set_bool(m, "type_detail.unknown", t->TypeDetail.Unknown);
	confignode_path_set_bool(m, "type_detail.fast_paged", t->TypeDetail.FastPaged);
	confignode_path_set_bool(m, "type_detail.static_column", t->TypeDetail.StaticColumn);
	confignode_path_set_bool(m, "type_detail.pseudo_static", t->TypeDetail.PseudoStatic);
	confignode_path_set_bool(m, "type_detail.rambus", t->TypeDetail.Rambus);
	confignode_path_set_bool(m, "type_detail.synchronous", t->TypeDetail.Synchronous);
	confignode_path_set_bool(m, "type_detail.cmos", t->TypeDetail.Cmos);
	confignode_path_set_bool(m, "type_detail.edo", t->TypeDetail.Edo);
	confignode_path_set_bool(m, "type_detail.window_dram", t->TypeDetail.WindowDram);
	confignode_path_set_bool(m, "type_detail.cache_dram", t->TypeDetail.CacheDram);
	confignode_path_set_bool(m, "type_detail.nonvolatile", t->TypeDetail.Nonvolatile);
	confignode_path_set_bool(m, "type_detail.registered", t->TypeDetail.Registered);
	confignode_path_set_bool(m, "type_detail.unbuffered", t->TypeDetail.Unbuffered);
	confignode_path_set_bool(m, "type_detail.lr_dimm", t->TypeDetail.LrDimm);
	confignode_path_set_int(m, "speed", t->Speed);
	embloader_smbios_set_config_string(ctx, ptr, m, "manufacturer", t->Manufacturer);
	embloader_smbios_set_config_string(ctx, ptr, m, "serial_number", t->SerialNumber);
	embloader_smbios_set_config_string(ctx, ptr, m, "asset_tag", t->AssetTag);
	embloader_smbios_set_config_string(ctx, ptr, m, "part_number", t->PartNumber);
	if (ctx->version >= SMBIOS_VER(2,6)) {
		confignode_path_set_int(m, "attributes", t->Attributes);
	}
	if (ctx->version >= SMBIOS_VER(2,7)) {
		// ExtendedSize is handled above in the Size field logic
		confignode_path_set_int(m, "configured_memory_clock_speed", t->ConfiguredMemoryClockSpeed);
	}
	if (ctx->version >= SMBIOS_VER(2,8)) {
		confignode_path_set_int(m, "minimum_voltage", t->MinimumVoltage);
		confignode_path_set_int(m, "maximum_voltage", t->MaximumVoltage);
		confignode_path_set_int(m, "configured_voltage", t->ConfiguredVoltage);
	}
	if (ctx->version >= SMBIOS_VER(3,2)) {
		confignode_path_set_string(m, "memory_technology",
			t->MemoryTechnology < sizeof(memory_technology)/sizeof(memory_technology[0]) ?
			memory_technology[t->MemoryTechnology] : "unknown");
		confignode_path_set_bool(m, "operating_mode_other", t->MemoryOperatingModeCapability.Bits.Other);
		confignode_path_set_bool(m, "operating_mode_unknown", t->MemoryOperatingModeCapability.Bits.Unknown);
		confignode_path_set_bool(m, "operating_mode_volatile_memory", t->MemoryOperatingModeCapability.Bits.VolatileMemory);
		confignode_path_set_bool(m, "operating_mode_byte_accessible_persistent_memory", t->MemoryOperatingModeCapability.Bits.ByteAccessiblePersistentMemory);
		confignode_path_set_bool(m, "operating_mode_block_accessible_persistent_memory", t->MemoryOperatingModeCapability.Bits.BlockAccessiblePersistentMemory);
		embloader_smbios_set_config_string(ctx, ptr, m, "firmware_version", t->FirmwareVersion);
		confignode_path_set_int(m, "module_manufacturer_id", t->ModuleManufacturerID);
		confignode_path_set_int(m, "module_product_id", t->ModuleProductID);
		confignode_path_set_int(m, "memory_subsystem_controller_manufacturer_id", t->MemorySubsystemControllerManufacturerID);
		confignode_path_set_int(m, "memory_subsystem_controller_product_id", t->MemorySubsystemControllerProductID);
		confignode_path_set_int(m, "non_volatile_size", t->NonVolatileSize);
		confignode_path_set_int(m, "volatile_size", t->VolatileSize);
		confignode_path_set_int(m, "cache_size", t->CacheSize);
		confignode_path_set_int(m, "logical_size", t->LogicalSize);
	}
	if (ctx->version >= SMBIOS_VER(3,3)) {
		confignode_path_set_int(m, "extended_speed", t->ExtendedSpeed);
		confignode_path_set_int(m, "extended_configured_memory_speed", t->ExtendedConfiguredMemorySpeed);
	}
	if (ctx->version >= SMBIOS_VER(3,7)) {
		confignode_path_set_int(m, "pmic0_manufacturer_id", t->Pmic0ManufacturerID);
		confignode_path_set_int(m, "pmic0_revision_number", t->Pmic0RevisionNumber);
		confignode_path_set_int(m, "rcd_manufacturer_id", t->RcdManufacturerID);
		confignode_path_set_int(m, "rcd_revision_number", t->RcdRevisionNumber);
	}
}
