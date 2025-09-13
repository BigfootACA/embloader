#include "../smbios.h"

void smbios_load_type36(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE36 *t = ptr.Type36;
	confignode_path_set_int(m, "lower_threshold_non_critical", t->LowerThresholdNonCritical);
	confignode_path_set_int(m, "upper_threshold_non_critical", t->UpperThresholdNonCritical);
	confignode_path_set_int(m, "lower_threshold_critical", t->LowerThresholdCritical);
	confignode_path_set_int(m, "upper_threshold_critical", t->UpperThresholdCritical);
	confignode_path_set_int(m, "lower_threshold_non_recoverable", t->LowerThresholdNonRecoverable);
	confignode_path_set_int(m, "upper_threshold_non_recoverable", t->UpperThresholdNonRecoverable);
}
