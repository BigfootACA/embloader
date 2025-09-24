#include <stdbool.h>
#include "configfile.h"
#include "embloader.h"
#include "vm.h"
#if defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#endif

static bool cpuid_in_hypervisor(void) {
#if defined(__i386__) || defined(__x86_64__)
        unsigned eax, ebx, ecx, edx;
        if (__get_cpuid(1, &eax, &ebx, &ecx, &edx) == 0)
                return false;
        if (((~ecx) & 0x80000000U) == 0)
                return true;
#endif
        return false;
}

static bool smbios_in_hypervisor(void) {
        return confignode_path_get_bool(
		g_embloader.config,
		"smbios.bios0.characteristics.virtual_machine_supported",
		false, NULL
	);
}

bool vm_in_hypervisor(void) {
        static int cache = -1;
        if (cache >= 0)
                return cache;

        cache = cpuid_in_hypervisor() || smbios_in_hypervisor();
        return cache;
}
