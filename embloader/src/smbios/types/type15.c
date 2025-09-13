#include "../smbios.h"

void smbios_load_type15(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE15 *t = ptr.Type15;
	confignode_path_set_int(m, "log_area_length", t->LogAreaLength);
	confignode_path_set_int(m, "log_header_start_offset", t->LogHeaderStartOffset);
	confignode_path_set_int(m, "log_data_start_offset", t->LogDataStartOffset);
	confignode_path_set_int(m, "access_method", t->AccessMethod);
	confignode_path_set_int(m, "log_status", t->LogStatus);
	confignode_path_set_int(m, "log_change_token", t->LogChangeToken);
	confignode_path_set_int(m, "access_method_address", t->AccessMethodAddress);
	confignode_path_set_int(m, "log_header_format", t->LogHeaderFormat);
	confignode_path_set_int(m, "number_of_supported_log_type_descriptors", t->NumberOfSupportedLogTypeDescriptors);
	confignode_path_set_int(m, "length_of_log_type_descriptor", t->LengthOfLogTypeDescriptor);
	confignode *descriptors_array = confignode_new_array();
	confignode_path_map_set(m, "event_log_type_descriptors", descriptors_array);
	for (int i = 0; i < t->NumberOfSupportedLogTypeDescriptors; ++i) {
		confignode *descriptor = confignode_new_map();
		confignode_array_append(descriptors_array, descriptor);
		confignode_path_set_int(descriptor, "log_type", t->EventLogTypeDescriptors[i].LogType);
		confignode_path_set_int(descriptor, "data_format_type", t->EventLogTypeDescriptors[i].DataFormatType);
	}
}
