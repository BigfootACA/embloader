#include <libfdt.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "fdt-utils.h"

int fdt_getprop_u8(fdt fdt, int off, const char* name, int id, uint8_t* val) {
	int length = 0;
	const uint8_t* prop;
	if (val) *val = 0;
	if (!fdt) return -FDT_ERR_NOTFOUND;
	prop = fdt_getprop(fdt, off, name, &length);
	if (length < 0) return length;
	if (length % sizeof(uint8_t) != 0) return -FDT_ERR_BADVALUE;
	if (id < 0 || id >= length / (int) sizeof(uint8_t)) return -FDT_ERR_BADOFFSET;
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
	if (id < 0 || id >= length / (int) sizeof(uint16_t)) return -FDT_ERR_BADOFFSET;
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
	if (id < 0 || id >= length / (int) sizeof(uint32_t)) return -FDT_ERR_BADOFFSET;
	if (val) *val = fdt32_to_cpu(prop[id]);
	return 0;
}

int fdt_getprop_u64(fdt fdt, int off, const char* name, int id, uint64_t* val) {
	int length = 0;
	const uint8_t* prop;
	fdt64_t raw;
	if (val) *val = 0;
	if (!fdt) return -FDT_ERR_NOTFOUND;
	prop = fdt_getprop(fdt, off, name, &length);
	if (length < 0) return length;
	if (length % sizeof(uint64_t) != 0) return -FDT_ERR_BADVALUE;
	if (id < 0 || id >= length / (int) sizeof(uint64_t)) return -FDT_ERR_BADOFFSET;
	memcpy(&raw,prop+(size_t)id*sizeof(raw),sizeof(raw));
	if (val) *val = fdt64_to_cpu(raw);
	return 0;
}

int fdt_getprop_un(fdt fdt, int off, const char* name, int id, uintptr_t* val) {
	if (sizeof(uintptr_t) == sizeof(uint32_t))
		return fdt_getprop_u32(fdt, off, name, id, (uint32_t*) val);
	if (sizeof(uintptr_t) == sizeof(uint64_t))
		return fdt_getprop_u64(fdt, off, name, id, (uint64_t*) val);
	return -FDT_ERR_BADVALUE;
}

int fdt_getprop_uc(fdt fdt, int off, const char* name, int id, uint64_t* val, bool size) {
	if (val) *val = 0;
	int p = off == 0 ? 0 : fdt_parent_offset(fdt, off);
	int32_t cells = size ? fdt_get_size_cells(fdt, p): fdt_get_address_cells(fdt, p);
	if (cells == 1) {
		uint32_t value;
		int ret = fdt_getprop_u32(fdt, off, name, id, &value);
		if (ret == 0 && val) *val = value;
		return ret;
	}
	if (cells == 2)
		return fdt_getprop_u64(fdt, off, name, id, (uint64_t*) val);
	return -FDT_ERR_BADVALUE;
}

int fdt_setprop_uc(fdt fdt, int off, const char* name, uint64_t val, bool size) {
	int p = off == 0 ? 0 : fdt_parent_offset(fdt, off);
	int32_t cells = size ? fdt_get_size_cells(fdt, p): fdt_get_address_cells(fdt, p);
	if (cells == 1 && val <= MAX_UINT32)
		return fdt_setprop_u32(fdt, off, name, val);
	if (cells == 2)
		return fdt_setprop_u64(fdt, off, name, val);
	return -FDT_ERR_BADVALUE;
}

int fdt_appendprop_uc(fdt fdt, int off, const char* name, uint64_t val, bool size) {
	int p = off == 0 ? 0 : fdt_parent_offset(fdt, off);
	int32_t cells = size ? fdt_get_size_cells(fdt, p): fdt_get_address_cells(fdt, p);
	if (cells == 1 && val <= MAX_UINT32)
		return fdt_appendprop_u32(fdt, off, name, val);
	if (cells == 2)
		return fdt_appendprop_u64(fdt, off, name, val);
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

static bool fdt_valid_cells(int32_t cells) {
	return cells>=0&&cells<=2;
}

bool fdt_get_reg(fdt fdt, int off, int index, uint64_t* base, uint64_t* size) {
	const uint8_t* prop;
	const uint8_t* cursor;
	int parent,len=0;
	int32_t ac,sc;
	size_t cells,item,offset;
	if (base) *base = 0;
	if (size) *size = 0;
	if (!fdt || off < 0 || index < 0) return false;

	parent=fdt_parent_offset(fdt,off);
	if(parent<0)return false;
	ac=fdt_get_address_cells(fdt,parent);
	sc=fdt_get_size_cells(fdt,parent);
	if(!fdt_valid_cells(ac)||!fdt_valid_cells(sc))return false;
	cells=(size_t)ac+(size_t)sc;
	if(cells==0)return false;
	item=cells*sizeof(fdt32_t);

	prop = fdt_getprop(fdt, off, "reg", &len);
	if(!prop||len<0||(size_t)len<item||(size_t)len%item!=0)return false;
	if((size_t)index>=(size_t)len/item)return false;
	offset=(size_t)index*item;
	cursor=prop+offset;

	if(base&&!fdt_apply_cells(ac,base,cursor))return false;
	cursor+=(size_t)ac*sizeof(fdt32_t);

	if(size&&!fdt_apply_cells(sc,size,cursor))return false;

	return true;
}

bool fdt_get_ranges(fdt fdt, int off, int index, uint64_t* base, uint64_t* target, uint64_t* size) {
	const uint8_t* prop;
	const uint8_t* cursor;
	int parent;
	int32_t pc,ac,sc;
	int len=0;
	size_t cells,item,offset;
	if (base) *base = 0;
	if (target) *target = 0;
	if (size) *size = 0;
	if (!fdt || off < 0 || index < 0) return false;

	parent=fdt_parent_offset(fdt,off);
	if(parent<0)return false;
	ac=fdt_get_address_cells(fdt,off);
	sc=fdt_get_size_cells(fdt,off);
	pc=fdt_get_address_cells(fdt,parent);
	if(
		!fdt_valid_cells(ac)||!fdt_valid_cells(sc)||
		!fdt_valid_cells(pc)
	)return false;
	cells=(size_t)ac+(size_t)pc+(size_t)sc;
	if(cells==0)return false;
	item=cells*sizeof(fdt32_t);

	prop = fdt_getprop(fdt, off, "ranges", &len);
	if(!prop||len<0||(size_t)len<item||(size_t)len%item!=0)return false;
	if((size_t)index>=(size_t)len/item)return false;
	offset=(size_t)index*item;
	cursor=prop+offset;

	if(base&&!fdt_apply_cells(ac,base,cursor))return false;
	cursor+=(size_t)ac*sizeof(fdt32_t);

	if(target&&!fdt_apply_cells(pc,target,cursor))return false;
	cursor+=(size_t)pc*sizeof(fdt32_t);

	if(size&&!fdt_apply_cells(sc,size,cursor))return false;

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
	int32_t cells;
	uint64_t start = 0, end = 0;
	const void* data = NULL;
	if (initrd) *initrd = NULL;
	if (size) *size = 0;
	if (!fdt) return false;
	node = fdt_path_offset(fdt, "/chosen");
	if (node < 0) return false;
	cells = fdt_get_address_cells(fdt, node);
	if (cells != 1 && cells != 2) return false;
	data = fdt_getprop(fdt, node, "linux,initrd-start", &l);
	if (l != cells * (int) sizeof(fdt32_t)) return false;
	if (!fdt_apply_cells(cells, &start, data)) return false;
	data = fdt_getprop(fdt, node, "linux,initrd-end", &l);
	if (l != cells * (int) sizeof(fdt32_t)) return false;
	if (!fdt_apply_cells(cells, &end, data)) return false;
	if (end <= start || start > UINTPTR_MAX || end - start > SIZE_MAX)
		return false;
	if (initrd) *initrd = (void*) (uintptr_t) start;
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
