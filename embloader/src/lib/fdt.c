#include <libfdt.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <libfdt.h>
#include "fdt-utils.h"

int fdt_getprop_u8(fdt fdt, int off, const char* name, int id, uint8_t* val) {
	int length = 0;
	const uint8_t* prop;
	if (val) *val = 0;
	if (!fdt) return -FDT_ERR_NOTFOUND;
	prop = fdt_getprop(fdt, off, name, &length);
	if (length < 0) return length;
	if (length % sizeof(uint8_t) != 0) return -FDT_ERR_BADVALUE;
	if (id > length / (int) sizeof(uint8_t)) return -FDT_ERR_BADOFFSET;
	if (val) *val = prop[id];
	return 0;
}

int fdt_getprop_u16(fdt fdt, int off, const char* name, int id, uint16_t* val) {
	int length = 0;
	const uint16_t* prop;
	if (val) *val = 0;
	if (!fdt) return -FDT_ERR_NOTFOUND;
	prop = fdt_getprop(fdt, off, name, &length);
	if (length < 0) return length;
	if (length % sizeof(uint16_t) != 0) return -FDT_ERR_BADVALUE;
	if (id > length / (int) sizeof(uint16_t)) return -FDT_ERR_BADOFFSET;
	if (val) *val = fdt16_to_cpu(prop[id]);
	return 0;
}

int fdt_getprop_u32(fdt fdt, int off, const char* name, int id, uint32_t* val) {
	int length = 0;
	const uint32_t* prop;
	if (val) *val = 0;
	if (!fdt) return -FDT_ERR_NOTFOUND;
	prop = fdt_getprop(fdt, off, name, &length);
	if (length < 0) return length;
	if (length % sizeof(uint32_t) != 0) return -FDT_ERR_BADVALUE;
	if (id > length / (int) sizeof(uint32_t)) return -FDT_ERR_BADOFFSET;
	if (val) *val = fdt32_to_cpu(prop[id]);
	return 0;
}

int fdt_getprop_u64(fdt fdt, int off, const char* name, int id, uint64_t* val) {
	int length = 0;
	const uint64_t* prop;
	if (val) *val = 0;
	if (!fdt) return -FDT_ERR_NOTFOUND;
	prop = fdt_getprop(fdt, off, name, &length);
	if (length < 0) return length;
	if (length % sizeof(uint64_t) != 0) return -FDT_ERR_BADVALUE;
	if (id > length / (int) sizeof(uint64_t)) return -FDT_ERR_BADOFFSET;
	if (val) *val = fdt64_to_cpu(prop[id]);
	return 0;
}

int fdt_getprop_un(fdt fdt, int off, const char* name, int id, uintptr_t* val) {
	if (sizeof(uintptr_t) == sizeof(uint32_t))
		return fdt_getprop_u32(fdt, off, name, id, (uint32_t*) val);
	if (sizeof(uintptr_t) == sizeof(uint64_t))
		return fdt_getprop_u64(fdt, off, name, id, (uint64_t*) val);
	return -FDT_ERR_BADVALUE;
}

int32_t fdt_get_address_cells(fdt fdt, int off) {
	int parent = off;
	const int32_t* prop;
	int32_t length = 0, ret = -1;
	if (!fdt) return -FDT_ERR_NOTFOUND;
	do {
		prop = fdt_getprop(fdt, parent, "#address-cells", &length);
		parent = fdt_parent_offset(fdt, parent);
	} while (!prop && parent >= 0);
	if (length == 4) ret = fdt32_to_cpu(*prop);
	else if (length < 0)
		ret = length;
	return ret;
}

int32_t fdt_get_size_cells(fdt fdt, int off) {
	int parent = off;
	const int32_t* prop;
	int32_t length = 0, ret = -1;
	if (!fdt) return -FDT_ERR_NOTFOUND;
	do {
		prop = fdt_getprop(fdt, parent, "#size-cells", &length);
		parent = fdt_parent_offset(fdt, parent);
	} while (!prop && parent >= 0);
	if (length == 4) ret = fdt32_to_cpu(*prop);
	else if (length < 0)
		ret = length;
	return ret;
}

static bool fdt_apply_cells(int32_t cell, uint64_t* out, const void* prop) {
	union {
		fdt32_t f32;
		fdt64_t f64;
	} v;
	switch (cell) {
		case 0:
			*out = 0;
			return true;
		case 1:
			memcpy(&v.f32, prop, sizeof(v.f32));
			*out = fdt32_to_cpu(v.f32);
			return true;
		case 2:
			memcpy(&v.f64, prop, sizeof(v.f64));
			*out = fdt64_to_cpu(v.f64);
			return true;
		default:
			return false;
	}
}

bool fdt_get_reg(fdt fdt, int off, int index, uint64_t* base, uint64_t* size) {
	const void* prop;
	int32_t ac = -1, sc = -1, item, len = 0;
	if (base) *base = 0;
	if (size) *size = 0;
	if (!fdt || off < 0 || index < 0) return false;

	// load address cells and size cells
	if (ac <= 0) ac = fdt_get_address_cells(fdt, off);
	if (sc <= 0) sc = fdt_get_size_cells(fdt, off);

	prop = fdt_getprop(fdt, off, "reg", &len);
	item = (ac + sc) * sizeof(int32_t);
	if (item <= 0) return false;
	if (len < item) return false;

	// in range
	if (index >= len / item) return false;
	prop += item * index;

	// calc memory base
	if (base && !fdt_apply_cells(ac, base, prop)) return false;
	prop += ac * sizeof(fdt32_t);

	// calc memory size
	if (size && !fdt_apply_cells(sc, size, prop)) return false;
	prop += sc * sizeof(fdt32_t);

	return true;
}

bool fdt_get_ranges(fdt fdt, int off, int index, uint64_t* base, uint64_t* target, uint64_t* size) {
	const void* prop;
	int parent;
	int32_t pc = -1, ac = -1, sc = -1, item, len = 0;
	if (base) *base = 0;
	if (target) *target = 0;
	if (size) *size = 0;
	if (!fdt || off < 0 || index < 0) return false;

	// load address cells and size cells
	if (ac <= 0) ac = fdt_get_address_cells(fdt, off);
	if (sc <= 0) sc = fdt_get_size_cells(fdt, off);
	if ((parent = fdt_parent_offset(fdt, off)) >= 0)
		pc = fdt_get_address_cells(fdt, parent);

	prop = fdt_getprop(fdt, off, "ranges", &len);
	item = (pc + ac + sc) * sizeof(int32_t);
	if (item <= 0) return false;
	if (len < item) return false;

	// in range
	if (index >= len / item) return false;
	prop += item * index;

	// calc range base
	if (base && !fdt_apply_cells(ac, base, prop)) return false;
	prop += ac * sizeof(fdt32_t);

	// calc range target
	if (target && !fdt_apply_cells(pc, target, prop)) return false;
	prop += pc * sizeof(fdt32_t);

	// calc range size
	if (size && !fdt_apply_cells(sc, size, prop)) return false;
	prop += sc * sizeof(fdt32_t);

	return true;
}

bool fdt_get_memory(fdt fdt, int index, uint64_t* base, uint64_t* size) {
	if (!fdt) return false;
	int mem = fdt_path_offset(fdt, "/memory");
	if (mem < 0) return false;
	return fdt_get_reg(fdt, mem, index, base, size);
}

bool fdt_get_initrd(fdt fdt, void** initrd, size_t* size) {
	int node, l;
	size_t start, end;
	const void* data = NULL;
	if (!fdt) return false;
	node = fdt_path_offset(fdt, "/chosen");
	if (node < 0) return false;
	data = fdt_getprop(fdt, node, "linux,initrd-start", &l);
	if (l != sizeof(void*)) return false;
	start = *(size_t*) data;
	data = fdt_getprop(fdt, node, "linux,initrd-end", &l);
	if (l != sizeof(void*)) return false;
	end = *(size_t*) data;
	if (start <= end) return false;
	if (initrd) *initrd = (void*) start;
	if (size) *size = (size_t) (end - start);
	return true;
}

bool fdt_is_enabled(fdt fdt, int off) {
	int len;
	const char* str;
	char buffer[64];
	if (!fdt || off < 0) return false;
	if (!(str = fdt_stringlist_get(fdt, off, "status", 0, &len)))
		return true;
	memset(buffer, 0, sizeof(buffer));
	strncpy(buffer, str, MIN(sizeof(buffer) - 1, (size_t) len));
	return strcasecmp(buffer, "ok") == 0 || strcasecmp(buffer, "okay") == 0;
}
