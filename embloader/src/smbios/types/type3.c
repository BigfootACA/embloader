#include "../smbios.h"

static const char *chassis_type[] = {
	"null",
	"type_other",
	"type_unknown",
	"type_desk_top",
	"type_low_profile_desktop",
	"type_pizza_box",
	"type_mini_tower",
	"type_tower",
	"type_portable",
	"type_lap_top",
	"type_notebook",
	"type_hand_held",
	"type_docking_station",
	"type_all_in_one",
	"type_sub_notebook",
	"type_space_saving",
	"type_lunch_box",
	"type_main_server_chassis",
	"type_expansion_chassis",
	"type_sub_chassis",
	"type_bus_expansion_chassis",
	"type_peripheral_chassis",
	"type_raid_chassis",
	"type_rack_mount_chassis",
	"type_sealed_case_pc",
	"multi_system_chassis",
	"compact_pci",
	"advanced_tca",
	"blade",
	"blade_enclosure",
	"tablet",
	"convertible",
	"detachable",
	"io_t_gateway",
	"embedded_pc",
	"mini_pc",
	"stick_pc",
};

static const char *chassis_state[] = {
	"other",
	"unknown",
	"safe",
	"warning",
	"critical",
	"non-recoverable",
};

static const char *chassis_security_state[] = {
	"other",
	"unknown",
	"none",
	"external_interface_locked_out",
	"external_interface_locked_enabled",
};

static const char *chassis_state_to_string(UINT8 state) {
	if (state < 1 || state > sizeof(chassis_state) / sizeof(chassis_state[0]))
		return "unknown";
	return chassis_state[state - 1];
}

void smbios_load_type3(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE3 *t = ptr.Type3;
	embloader_smbios_set_config_string(ctx, ptr, m, "manufacturer", t->Manufacturer);
	// Bit 7 reports whether a chassis lock is present.
	UINT8 chassis_type_value = t->Type & 0x7F;
	confignode_path_set_string(m, "type", chassis_type_value < sizeof(chassis_type)/sizeof(chassis_type[0]) ? chassis_type[chassis_type_value] : "unknown");
	confignode_path_set_bool(m, "lock_present", (t->Type & 0x80) != 0);
	embloader_smbios_set_config_string(ctx, ptr, m, "version", t->Version);
	embloader_smbios_set_config_string(ctx, ptr, m, "serial_number", t->SerialNumber);
	embloader_smbios_set_config_string(ctx, ptr, m, "asset_tag", t->AssetTag);
	confignode_path_set_string(m, "bootup_state", chassis_state_to_string(t->BootupState));
	confignode_path_set_string(m, "power_supply_state", chassis_state_to_string(t->PowerSupplyState));
	confignode_path_set_string(m, "thermal_state", chassis_state_to_string(t->ThermalState));
	confignode_path_set_string(m, "security_status", t->SecurityStatus >= 1 && t->SecurityStatus <= 5 ? chassis_security_state[t->SecurityStatus-1] : "unknown");
	confignode_path_set_int(m, "height", t->Height);
}
