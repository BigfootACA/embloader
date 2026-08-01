
#ifndef EMBLOADER_SMBIOS_H
#define EMBLOADER_SMBIOS_H
#include <IndustryStandard/SmBios.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "configfile.h"
typedef struct embloader_smbios embloader_smbios;
#define SMBIOS_VER(maj,min) (((maj)<<16)|(min))
struct smbios_table_load {
	uint8_t type;
	const char *name;
	void (*load)(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
};
struct embloader_smbios{
	UINT16 major,minor;
	UINT32 version;
	bool config_error;
	SMBIOS_TABLE_ENTRY_POINT*smbios2;
	SMBIOS_TABLE_3_0_ENTRY_POINT*smbios3;
	UINTN table_start;
	UINTN table_end;
	UINT8*active_record;
	UINT8*active_copy;
};
extern void embloader_init_info_smbios(embloader_smbios*ctx);
extern SMBIOS_STRUCTURE_POINTER embloader_get_smbios_by_type(
	embloader_smbios*ctx,
	uint8_t type,
	SMBIOS_HANDLE handle
);
extern SMBIOS_STRUCTURE_POINTER embloader_get_smbios_by_handle(
	embloader_smbios*ctx,
	SMBIOS_HANDLE handle
);
extern void embloader_smbios_set_config_string(
	embloader_smbios *ctx,
	SMBIOS_STRUCTURE_POINTER ptr,
	confignode *node,
	const char *path,
	SMBIOS_TABLE_STRING index
);
extern void embloader_smbios_set_config_guid(
	embloader_smbios *ctx,
	SMBIOS_STRUCTURE_POINTER ptr,
	confignode *node,
	const char *path,
	GUID *guid
);
extern const char*embloader_get_smbios_string(
	embloader_smbios*ctx,
	SMBIOS_STRUCTURE_POINTER p,
	SMBIOS_TABLE_STRING id
);
extern bool embloader_load_smbios_string(
	embloader_smbios*ctx,
	SMBIOS_STRUCTURE_POINTER p,
	SMBIOS_TABLE_STRING id,
	char*buff,
	size_t size
);
extern bool embloader_smbios_config_set_int(
	embloader_smbios *ctx,
	confignode *node,
	const char *path,
	int64_t value
);
extern bool embloader_smbios_config_set_string(
	embloader_smbios *ctx,
	confignode *node,
	const char *path,
	const char *value
);
extern bool embloader_smbios_config_set_string_fmt(
	embloader_smbios *ctx,
	confignode *node,
	const char *path,
	const char *format,
	...
);
extern bool embloader_smbios_config_set_bool(
	embloader_smbios *ctx,
	confignode *node,
	const char *path,
	bool value
);
extern bool embloader_smbios_config_attach(
	embloader_smbios *ctx,
	confignode *node,
	const char *path,
	confignode *sub
);
extern bool embloader_smbios_config_append(
	embloader_smbios *ctx,
	confignode *array,
	confignode *sub
);
extern bool embloader_smbios_load_table(
	embloader_smbios *ctx,
	confignode *root,
	uint8_t type,
	const char *name,
	void (*load)(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr)
);
extern bool embloader_load_smbios();
extern struct smbios_table_load smbios_table_loads[];
extern void smbios_load_type0(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type1(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type2(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type3(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type4(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type5(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type6(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type7(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type8(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type9(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type10(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type11(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type12(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type13(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type14(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type15(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type16(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type17(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type18(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type19(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type20(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type21(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type22(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type23(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type24(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type25(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type26(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type27(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type28(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type29(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type30(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type31(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type32(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type33(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type34(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type35(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type36(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type37(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type38(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type39(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type40(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type41(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type42(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type43(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type44(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type45(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);
extern void smbios_load_type46(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr);

#ifndef EMBLOADER_SMBIOS_CONFIG_IMPL
#define confignode_path_set_int(node, path, value) \
	embloader_smbios_config_set_int(ctx, node, path, value)
#define confignode_path_set_string(node, path, value) \
	embloader_smbios_config_set_string(ctx, node, path, value)
#define confignode_path_set_string_fmt(node, path, ...) \
	embloader_smbios_config_set_string_fmt(ctx, node, path, __VA_ARGS__)
#define confignode_path_set_bool(node, path, value) \
	embloader_smbios_config_set_bool(ctx, node, path, value)
#define confignode_path_map_set(node, path, sub) \
	embloader_smbios_config_attach(ctx, node, path, sub)
#define confignode_array_append(node, sub) \
	embloader_smbios_config_append(ctx, node, sub)
#endif

#endif
