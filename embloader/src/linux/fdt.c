#include <stdio.h>
#include "linuxboot.h"
#include "libfdt.h"
#include "fdt-utils.h"
#include "log.h"

static int find_chosen_node(fdt fdt) {
	int node = fdt_path_offset(fdt, "/chosen");
	if (node == -FDT_ERR_NOTFOUND) node = fdt_add_subnode(fdt, 0, "chosen");
	return node;
}

int linux_fdt_fill_bootargs(fdt fdt, const char* bootargs) {
	int node;
	if (!fdt || !bootargs) return -FDT_ERR_BADVALUE;
	node = find_chosen_node(fdt);
	if (node < 0) return node;
	return fdt_setprop_string(fdt, node, "bootargs", bootargs);
}

int linux_fdt_fill_initrd(fdt fdt, VOID* initrd_addr, size_t initrd_size) {
	UINT64 start, end;
	int node, ret;
	if (!fdt) return -FDT_ERR_BADVALUE;
	if (!initrd_addr || initrd_size == 0) {
		node = fdt_path_offset(fdt, "/chosen");
		if (node == -FDT_ERR_NOTFOUND) return 0;
		if (node < 0) return node;
		ret = fdt_delprop(fdt, node, "linux,initrd-start");
		if (ret != 0 && ret != -FDT_ERR_NOTFOUND) return ret;
		ret = fdt_delprop(fdt, node, "linux,initrd-end");
		return ret == -FDT_ERR_NOTFOUND ? 0 : ret;
	}
	start = (UINTN) initrd_addr;
	if (initrd_size > MAX_UINT64 - start) return -FDT_ERR_BADVALUE;
	end = start + initrd_size;
	node = find_chosen_node(fdt);
	if (node < 0) return node;
	ret = fdt_setprop_uc(fdt, node, "linux,initrd-start", start, false);
	if (ret != 0) return ret;
	return fdt_setprop_uc(fdt, node, "linux,initrd-end", end, false);
}

static int delete_memory_nodes(fdt fdt) {
	int node, ret;
	int length;
	const char* type;

	for (;;) {
		for (
			node = fdt_first_subnode(fdt, 0); node >= 0;
			node = fdt_next_subnode(fdt, node)
		) {
			type = fdt_getprop(fdt, node, "device_type", &length);
			if (type && length == sizeof("memory") && memcmp(type, "memory", sizeof("memory")) == 0)
				break;
		}
		if (node == -FDT_ERR_NOTFOUND) return 0;
		if (node < 0) return node;
		ret = fdt_del_node(fdt, node);
		if (ret != 0) return ret;
	}
}

int linux_fdt_fill_memory(fdt fdt, struct linux_memory* memory, size_t count) {
	char name[32];
	int node, ret;
	if (!fdt || !memory || count == 0) return -FDT_ERR_BADVALUE;
	ret = delete_memory_nodes(fdt);
	if (ret != 0) return ret;
	snprintf(name, sizeof(name), "memory@%llx", memory[0].address);
	node = fdt_add_subnode(fdt, 0, name);
	if (node < 0) return node;
	ret = fdt_setprop_string(fdt, node, "device_type", "memory");
	if (ret != 0) goto fail;
	for (size_t i = 0; i < count; i++) {
		ret = fdt_appendprop_uc(fdt, node, "reg", memory[i].address, false);
		if (ret != 0) goto fail;
		ret = fdt_appendprop_uc(fdt, node, "reg", memory[i].size, true);
		if (ret != 0) goto fail;
	}
	return 0;

fail:
	log_warning("failed to set device tree memory map: %s", fdt_strerror(ret));
	fdt_del_node(fdt, node);
	return ret;
}
