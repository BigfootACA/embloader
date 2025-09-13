#include "../smbios.h"

void smbios_load_type25(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr) {
	SMBIOS_TABLE_TYPE25 *t = ptr.Type25;
	confignode_path_set_int(m, "next_scheduled_power_on_month", t->NextScheduledPowerOnMonth);
	confignode_path_set_int(m, "next_scheduled_power_on_day_of_month", t->NextScheduledPowerOnDayOfMonth);
	confignode_path_set_int(m, "next_scheduled_power_on_hour", t->NextScheduledPowerOnHour);
	confignode_path_set_int(m, "next_scheduled_power_on_minute", t->NextScheduledPowerOnMinute);
	confignode_path_set_int(m, "next_scheduled_power_on_second", t->NextScheduledPowerOnSecond);
}
