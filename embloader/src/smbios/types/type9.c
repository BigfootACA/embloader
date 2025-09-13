#include "../smbios.h"

static const char *slot_type_to_string(int v) {
	switch (v) {
		case 0x01: return "other";
		case 0x02: return "unknown";
		case 0x03: return "isa";
		case 0x04: return "mca";
		case 0x05: return "eisa";
		case 0x06: return "pci";
		case 0x07: return "pcmcia";
		case 0x08: return "vl_vesa";
		case 0x09: return "proprietary";
		case 0x0A: return "processor_card_slot";
		case 0x0B: return "proprietary_memory_card_slot";
		case 0x0C: return "io_riser_card_slot";
		case 0x0D: return "nubus";
		case 0x0E: return "pci_66mhz_capable";
		case 0x0F: return "agp";
		case 0x10: return "agp_2x";
		case 0x11: return "agp_4x";
		case 0x12: return "pci_x";
		case 0x13: return "agp_8x";
		case 0xA5: return "pci_express";
		case 0xA6: return "pci_express_x1";
		case 0xA7: return "pci_express_x2";
		case 0xA8: return "pci_express_x4";
		case 0xA9: return "pci_express_x8";
		case 0xAA: return "pci_express_x16";
		case 0xAB: return "pci_express_gen2";
		case 0xAC: return "pci_express_gen2_x1";
		case 0xAD: return "pci_express_gen2_x2";
		case 0xAE: return "pci_express_gen2_x4";
		case 0xAF: return "pci_express_gen2_x8";
		case 0xB0: return "pci_express_gen2_x16";
		case 0xB1: return "pci_express_gen3";
		case 0xB2: return "pci_express_gen3_x1";
		case 0xB3: return "pci_express_gen3_x2";
		case 0xB4: return "pci_express_gen3_x4";
		case 0xB5: return "pci_express_gen3_x8";
		case 0xB6: return "pci_express_gen3_x16";
		case 0xB8: return "pci_express_gen4";
		case 0xB9: return "pci_express_gen4_x1";
		case 0xBA: return "pci_express_gen4_x2";
		case 0xBB: return "pci_express_gen4_x4";
		case 0xBC: return "pci_express_gen4_x8";
		case 0xBD: return "pci_express_gen4_x16";
		case 0xBE: return "pci_express_gen5";
		case 0xBF: return "pci_express_gen5_x1";
		case 0xC0: return "pci_express_gen5_x2";
		case 0xC1: return "pci_express_gen5_x4";
		case 0xC2: return "pci_express_gen5_x8";
		case 0xC3: return "pci_express_gen5_x16";
		default: return "unknown";
	}
}

static const char *slot_usage_to_string(int v) {
	switch (v) {
		case 0x01: return "other";
		case 0x02: return "unknown";
		case 0x03: return "available";
		case 0x04: return "in_use";
		case 0x05: return "unavailable";
		default: return "unknown";
	}
}

static const char *slot_length_to_string(int v) {
	switch (v) {
		case 0x01: return "other";
		case 0x02: return "unknown";
		case 0x03: return "short";
		case 0x04: return "long";
		default: return "unknown";
	}
}

void smbios_load_type9(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE9 *t = ptr.Type9;
	embloader_smbios_set_config_string(ctx, ptr, m, "slot_designation", t->SlotDesignation);
	confignode_path_set_string(m, "slot_type", slot_type_to_string(t->SlotType));
	confignode_path_set_int(m, "slot_data_bus_width", t->SlotDataBusWidth);
	confignode_path_set_string(m, "current_usage", slot_usage_to_string(t->CurrentUsage));
	confignode_path_set_string(m, "slot_length", slot_length_to_string(t->SlotLength));
	confignode_path_set_int(m, "slot_id", t->SlotID);
	confignode *cm = confignode_new_map();
	confignode_path_map_set(m, "characteristics", cm);
	confignode_path_set_bool(cm, "characteristics_unknown", t->SlotCharacteristics1.CharacteristicsUnknown);
	confignode_path_set_bool(cm, "provides_50_volts", t->SlotCharacteristics1.Provides50Volts);
	confignode_path_set_bool(cm, "provides_33_volts", t->SlotCharacteristics1.Provides33Volts);
	confignode_path_set_bool(cm, "shared_slot", t->SlotCharacteristics1.SharedSlot);
	confignode_path_set_bool(cm, "pc_card_16_supported", t->SlotCharacteristics1.PcCard16Supported);
	confignode_path_set_bool(cm, "cardbus_supported", t->SlotCharacteristics1.CardBusSupported);
	confignode_path_set_bool(cm, "zoom_video_supported", t->SlotCharacteristics1.ZoomVideoSupported);
	confignode_path_set_bool(cm, "modem_ring_resume_supported", t->SlotCharacteristics1.ModemRingResumeSupported);
	confignode_path_set_bool(cm, "pme_signal_supported", t->SlotCharacteristics2.PmeSignalSupported);
	confignode_path_set_bool(cm, "hot_plug_devices_supported", t->SlotCharacteristics2.HotPlugDevicesSupported);
	confignode_path_set_bool(cm, "smbus_signal_supported", t->SlotCharacteristics2.SmbusSignalSupported);
	confignode_path_set_bool(cm, "bifurcation_supported", t->SlotCharacteristics2.BifurcationSupported);
	confignode_path_set_bool(cm, "async_surprise_removal", t->SlotCharacteristics2.AsyncSurpriseRemoval);
	confignode_path_set_bool(cm, "flexbus_slot_cxl10_capable", t->SlotCharacteristics2.FlexbusSlotCxl10Capable);
	confignode_path_set_bool(cm, "flexbus_slot_cxl20_capable", t->SlotCharacteristics2.FlexbusSlotCxl20Capable);
	if (ctx->version >= SMBIOS_VER(3,7)) {
		confignode_path_set_bool(cm, "flexbus_slot_cxl30_capable", t->SlotCharacteristics2.FlexbusSlotCxl30Capable);
	}
	if (ctx->version >= SMBIOS_VER(2,6)) {
		confignode_path_set_int(m, "segment_group_num", t->SegmentGroupNum);
		confignode_path_set_int(m, "bus_num", t->BusNum);
		confignode_path_set_int(m, "dev_func_num", t->DevFuncNum);
	}
	if (ctx->version >= SMBIOS_VER(3,2)) {
		confignode_path_set_int(m, "data_bus_width", t->DataBusWidth);
		confignode_path_set_int(m, "peer_grouping_count", t->PeerGroupingCount);
		confignode *peer_array = confignode_new_array();
		confignode_path_map_set(m, "peer_groups", peer_array);
		for (int i = 0; i < t->PeerGroupingCount && i < 1; ++i) {
			confignode *peer = confignode_new_map();
			confignode_array_append(peer_array, peer);
			confignode_path_set_int(peer, "segment_group_num", t->PeerGroups[i].SegmentGroupNum);
			confignode_path_set_int(peer, "bus_num", t->PeerGroups[i].BusNum);
			confignode_path_set_int(peer, "dev_func_num", t->PeerGroups[i].DevFuncNum);
			confignode_path_set_int(peer, "data_bus_width", t->PeerGroups[i].DataBusWidth);
		}
	}
}
