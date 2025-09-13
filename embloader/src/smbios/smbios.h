
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
	SMBIOS_TABLE_ENTRY_POINT*smbios2;
	SMBIOS_TABLE_3_0_ENTRY_POINT*smbios3;
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
extern void embloader_smbios_load_table(
	embloader_smbios *ctx,
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
#define SMBIOS_GET(_ptr,_type,_field,_def)({\
	SMBIOS_STRUCTURE_POINTER _p=(_ptr);\
	UINTN _min_len=OFFSET_OF(SMBIOS_TABLE_TYPE##_type,_field)+sizeof(_p.Type##_type->_field);\
	_p.Hdr->Length>_min_len?_p.Type##_type->_field:(_def);\
})
#define SMBIOS_VARX(_name,_ptr,_type,_field,_def)\
	typeof((_ptr).Type##_type->_field) _name=SMBIOS_GET(_ptr,_type,_field,_def)
#define SMBIOS_VAR(_ptr,_type,_field,_def) SMBIOS_VARX(_field,_ptr,_type,_field,_def)

#endif
