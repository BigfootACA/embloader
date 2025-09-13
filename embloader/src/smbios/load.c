#include <stdio.h>
#include <string.h>
#include "smbios.h"
#include "debugs.h"
#include "embloader.h"

bool embloader_load_smbios() {
	embloader_smbios smbios;
	memset(&smbios, 0, sizeof(smbios));
	embloader_init_info_smbios(&smbios);
	if (!smbios.smbios2 && !smbios.smbios3) return false;
	confignode_path_set_int(g_embloader.sysinfo, "smbios.major", smbios.major);
	confignode_path_set_int(g_embloader.sysinfo, "smbios.minor", smbios.minor);
	confignode_path_set_string_fmt(g_embloader.sysinfo, "smbios.version", "%u.%u", smbios.major, smbios.minor);
	for (size_t i = 0; smbios_table_loads[i].load != NULL; i++) {
		embloader_smbios_load_table(
			&smbios,
			smbios_table_loads[i].type,
			smbios_table_loads[i].name,
			smbios_table_loads[i].load
		);
	}
	return true;
}
