#ifndef FDT_UTILS_H
#define FDT_UTILS_H
#include <Uefi.h>
#include <libfdt.h>
#include "embloader.h"
extern int fdt_getprop_u8(fdt fdt, int off, const char* name, int id, uint8_t* val);
extern int fdt_getprop_u16(fdt fdt, int off, const char* name, int id, uint16_t* val);
extern int fdt_getprop_u32(fdt fdt, int off, const char* name, int id, uint32_t* val);
extern int fdt_getprop_u64(fdt fdt, int off, const char* name, int id, uint64_t* val);
extern int fdt_getprop_un(fdt fdt, int off, const char* name, int id, uintptr_t* val);
extern int32_t fdt_get_address_cells(fdt fdt, int off);
extern int32_t fdt_get_size_cells(fdt fdt, int off);
extern bool fdt_get_reg(fdt fdt, int off, int index, uint64_t* base, uint64_t* size);
extern bool fdt_get_ranges(fdt fdt, int off, int index, uint64_t* base, uint64_t* target, uint64_t* size);
extern bool fdt_get_memory(fdt fdt, int index, uint64_t* base, uint64_t* size);
extern bool fdt_get_initrd(fdt fdt, void** initrd, size_t* size);
extern bool fdt_is_enabled(fdt fdt, int off);
#endif
