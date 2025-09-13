#include "../smbios.h"

static const char *port_connector_type_to_string(int v) {
	switch (v) {
		case 0x00: return "none";
		case 0x01: return "centronics";
		case 0x02: return "mini_centronics";
		case 0x03: return "proprietary";
		case 0x04: return "db25_male";
		case 0x05: return "db25_female";
		case 0x06: return "db15_male";
		case 0x07: return "db15_female";
		case 0x08: return "db9_male";
		case 0x09: return "db9_female";
		case 0x0A: return "rj11";
		case 0x0B: return "rj45";
		case 0x0C: return "50pin_mini_scsi";
		case 0x0D: return "mini_din";
		case 0x0E: return "micro_din";
		case 0x0F: return "ps2";
		case 0x10: return "infrared";
		case 0x11: return "hp_hil";
		case 0x12: return "usb";
		case 0x13: return "ssa_scsi";
		case 0x14: return "circular_din8_male";
		case 0x15: return "circular_din8_female";
		case 0x16: return "onboard_ide";
		case 0x17: return "onboard_floppy";
		case 0x18: return "9pin_dual_inline";
		case 0x19: return "25pin_dual_inline";
		case 0x1A: return "50pin_dual_inline";
		case 0x1B: return "68pin_dual_inline";
		case 0x1C: return "onboard_sound_input";
		case 0x1D: return "mini_centronics_type14";
		case 0x1E: return "mini_centronics_type26";
		case 0x1F: return "headphone_mini_jack";
		case 0x20: return "bnc";
		case 0x21: return "1394";
		case 0x22: return "sas_sata";
		case 0x23: return "usb_type_c";
		case 0xFF: return "other";
		default: return "unknown";
	}
}

static const char *port_type_to_string(int v) {
	switch (v) {
		case 0x00: return "none";
		case 0x01: return "parallel_xt_at_compatible";
		case 0x02: return "parallel_port_ps2";
		case 0x03: return "parallel_port_ecp";
		case 0x04: return "parallel_port_epp";
		case 0x05: return "parallel_port_ecp_epp";
		case 0x06: return "serial_xt_at_compatible";
		case 0x07: return "serial_16450_compatible";
		case 0x08: return "serial_16550_compatible";
		case 0x09: return "serial_16550a_compatible";
		case 0x0A: return "scsi";
		case 0x0B: return "midi";
		case 0x0C: return "joy_stick";
		case 0x0D: return "keyboard";
		case 0x0E: return "mouse";
		case 0x0F: return "ssa_scsi";
		case 0x10: return "usb";
		case 0x11: return "firewire";
		case 0x12: return "pcmcia_type_i";
		case 0x13: return "pcmcia_type_ii";
		case 0x14: return "pcmcia_type_iii";
		case 0x15: return "cardbus";
		case 0x16: return "access_bus_port";
		case 0x17: return "scsi_ii";
		case 0x18: return "scsi_wide";
		case 0x19: return "pc98";
		case 0x1A: return "pc98_hireso";
		case 0x1B: return "pch98";
		case 0x1C: return "video_port";
		case 0x1D: return "audio_port";
		case 0x1E: return "modem_port";
		case 0x1F: return "network_port";
		case 0x20: return "sata";
		case 0x21: return "sas";
		case 0x22: return "mfdp";
		case 0x23: return "thunderbolt";
		case 0xFF: return "other";
		default: return "unknown";
	}
}

void smbios_load_type8(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE8 *t = ptr.Type8;
	embloader_smbios_set_config_string(ctx, ptr, m, "internal_reference_designator", t->InternalReferenceDesignator);
	confignode_path_set_string(m, "internal_connector_type", port_connector_type_to_string(t->InternalConnectorType));
	embloader_smbios_set_config_string(ctx, ptr, m, "external_reference_designator", t->ExternalReferenceDesignator);
	confignode_path_set_string(m, "external_connector_type", port_connector_type_to_string(t->ExternalConnectorType));
	confignode_path_set_string(m, "port_type", port_type_to_string(t->PortType));
}
