#include "../smbios.h"

static const char *baseboard_type[] = {
	"null",
	"unknown",
	"other",
	"server_blade",
	"connectivity_switch",
	"system_management_module",
	"processor_module",
	"io_module",
	"memory_module",
	"daughter_board",
	"mother_board",
	"processor_memory_module",
	"processor_io_module",
	"interconnect_board",
};

void smbios_load_type2(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE2 *t = ptr.Type2;
	embloader_smbios_set_config_string(ctx, ptr, m, "manufacturer", t->Manufacturer);
	embloader_smbios_set_config_string(ctx, ptr, m, "product", t->ProductName);
	embloader_smbios_set_config_string(ctx, ptr, m, "version", t->Version);
	embloader_smbios_set_config_string(ctx, ptr, m, "serial_number", t->SerialNumber);
	embloader_smbios_set_config_string(ctx, ptr, m, "asset_tag", t->AssetTag);
	BASE_BOARD_FEATURE_FLAGS *f = &t->FeatureFlag;
	confignode_path_set_bool(m, "motherboard", f->Motherboard);
	confignode_path_set_bool(m, "requires_daughter_card", f->RequiresDaughterCard);
	confignode_path_set_bool(m, "removable", f->Removable);
	confignode_path_set_bool(m, "replaceable", f->Replaceable);
	confignode_path_set_bool(m, "hot_swappable", f->HotSwappable);
	embloader_smbios_set_config_string(ctx, ptr, m, "location_in_chassis", t->LocationInChassis);
	confignode_path_set_int(m, "chassis_handle", t->ChassisHandle);
	confignode_path_set_string(m, "board_type", t->BoardType < 0xd ? baseboard_type[t->BoardType] : "unknown");
}
