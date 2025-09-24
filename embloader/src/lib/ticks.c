#include <Library/UefiBootServicesTableLib.h>
#include "ticks.h"

#if defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#include "vm.h"

static inline uint64_t ticks_read_arch(void) {
	if (vm_in_hypervisor())
		return 0;
	return __builtin_ia32_rdtsc();
}

static uint64_t ticks_freq_arch(void) {
	unsigned max_leaf, ebx, ecx, edx;
	if (__get_cpuid(0, &max_leaf, &ebx, &ecx, &edx) == 0) return 0;
	if (
		max_leaf < 0x15 ||
		ebx != signature_INTEL_ebx ||
		ecx != signature_INTEL_ecx ||
		edx != signature_INTEL_edx
	) return 0;
	unsigned denominator, numerator, crystal_hz;
	__cpuid(0x15, denominator, numerator, crystal_hz, edx);
	if (denominator == 0 || numerator == 0)
		return 0;
	uint64_t freq = crystal_hz;
	if (crystal_hz == 0) {
		if (max_leaf < 0x16) return 0;
		unsigned core_mhz;
		__cpuid(0x16, core_mhz, ebx, ecx, edx);
		freq = core_mhz * 1000ULL * 1000ULL * denominator / numerator;
	}
	return freq * numerator / denominator;
}

#elif defined(__aarch64__)

static inline uint64_t ticks_read_arch(void) {
	uint64_t val;
	asm volatile("mrs %0, cntvct_el0" : "=r"(val));
	return val;
}

static inline uint64_t ticks_freq_arch(void) {
	uint64_t freq;
	asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
	return freq;
}

#else

static inline uint64_t ticks_read_arch(void) {
	return 0;
}

static inline uint64_t ticks_freq_arch(void) {
	return 0;
}

#endif

static uint64_t ticks_freq(void) {
	static uint64_t cache = 0;
	if (cache != 0)
		return cache;
	cache = ticks_freq_arch();
	if (cache != 0)
		return cache;
	uint64_t ticks_start = ticks_read_arch();
	gBS->Stall(1000);
	uint64_t ticks_end = ticks_read_arch();
	if (ticks_end < ticks_start)
		return 0;
	cache = (ticks_end - ticks_start) * 1000UL;
	return cache;
}

uint64_t ticks_usec(void) {
	uint64_t ticks = ticks_read_arch();
	if (ticks == 0)
		return 0;
	uint64_t freq = ticks_freq();
	if (freq == 0)
		return 0;
	return 1000UL * 1000UL * ticks / freq;
}

uint64_t ticks_msec(void) {
	return ticks_usec() / 1000UL;
}

uint32_t ticks_msec_u32(void) {
	return (uint32_t) ticks_msec();
}
