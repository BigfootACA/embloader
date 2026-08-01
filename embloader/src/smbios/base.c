#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/SmBios.h>
#include <inttypes.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "str-utils.h"
#define EMBLOADER_SMBIOS_CONFIG_IMPL
#include "smbios.h"
#include "log.h"
#include "embloader.h"
#include "debugs.h"

static bool check_smbios2(SMBIOS_TABLE_ENTRY_POINT*smbios){
	if(!smbios)return false;
	if(memcmp(
		smbios->AnchorString,SMBIOS_ANCHOR_STRING,
		sizeof(smbios->AnchorString))!=0
	){
		log_warning("smbios2 magic mismatch");
		return false;
	}
	if(
		smbios->EntryPointLength!=0x1E&&
		smbios->EntryPointLength!=sizeof(SMBIOS_TABLE_ENTRY_POINT)
	){
		log_info(
			"smbios2 entry point length mismatch %u",
			smbios->EntryPointLength
		);
		return false;
	}
	if(smbios->MajorVersion<2){
		log_info("smbios2 major invalid %u",smbios->MajorVersion);
		return false;
	}
	if(memcmp(
		smbios->IntermediateAnchorString,"_DMI_",
		sizeof(smbios->IntermediateAnchorString)
	)!=0){
		log_warning("smbios2 intermediate magic mismatch");
		return false;
	}
	if(CalculateCheckSum8((void*)smbios,smbios->EntryPointLength)!=0){
		log_warning("smbios2 checksum mismatch");
		return false;
	}
	UINTN xs=OFFSET_OF(SMBIOS_TABLE_ENTRY_POINT,IntermediateAnchorString);
	if(CalculateCheckSum8((UINT8*)smbios+xs,smbios->EntryPointLength-xs)!=0){
		log_warning("smbios2 intermediate checksum mismatch");
		return false;
	}
	return true;
}

static bool check_smbios3(SMBIOS_TABLE_3_0_ENTRY_POINT*smbios){
	if(!smbios)return false;
	if(memcmp(smbios->AnchorString,SMBIOS_3_0_ANCHOR_STRING,sizeof(smbios->AnchorString))!=0){
		log_warning("smbios3 magic mismatch");
		return false;
	}
	if(smbios->EntryPointLength!=sizeof(SMBIOS_TABLE_3_0_ENTRY_POINT)){
		log_info(
			"smbios3 entry point length mismatch %u",
			smbios->EntryPointLength
		);
		return false;
	}
	if(smbios->MajorVersion<3){
		log_info("smbios3 major invalid %u",smbios->MajorVersion);
		return false;
	}
	if(CalculateCheckSum8((void*)smbios,smbios->EntryPointLength)!=0){
		log_warning("smbios3 checksum mismatch");
		return false;
	}
	return true;
}

static bool smbios_set_range(
	embloader_smbios *ctx,
	UINT64 address,
	UINT64 size
){
	UINT64 max_uintn=(UINT64)((UINTN)-1);
	if(
		!ctx||address==0||size<sizeof(SMBIOS_STRUCTURE)||
		address>max_uintn||size>max_uintn-address
	){
		log_warning(
			"invalid smbios table range 0x%" PRIx64 "+0x%" PRIx64,
			(uint64_t)address,(uint64_t)size
		);
		return false;
	}
	ctx->table_start=(UINTN)address;
	ctx->table_end=ctx->table_start+(UINTN)size;
	return true;
}

void embloader_init_info_smbios(embloader_smbios*ctx){
	SMBIOS_TABLE_ENTRY_POINT*smbios2=NULL;
	SMBIOS_TABLE_3_0_ENTRY_POINT*smbios3=NULL;
	if(!ctx)return;
	ctx->major=0;
	ctx->minor=0;
	ctx->version=0;
	ctx->smbios2=NULL;
	ctx->smbios3=NULL;
	ctx->table_start=0;
	ctx->table_end=0;
	ctx->active_record=NULL;
	ctx->active_copy=NULL;
	for(UINTN i=0;i<gST->NumberOfTableEntries;i++){
		EFI_GUID*g=&gST->ConfigurationTable[i].VendorGuid;
		void*t=gST->ConfigurationTable[i].VendorTable;
		if(memcmp(g,&gEfiSmbiosTableGuid,sizeof(EFI_GUID))==0)smbios2=t;
		if(memcmp(g,&gEfiSmbios3TableGuid,sizeof(EFI_GUID))==0)smbios3=t;
	}
	if(
		smbios2&&check_smbios2(smbios2)&&
		smbios_set_range(ctx,smbios2->TableAddress,smbios2->TableLength)
	){
		ctx->smbios2=smbios2;
		ctx->major=ctx->smbios2->MajorVersion;
		ctx->minor=ctx->smbios2->MinorVersion;
	}
	if(
		smbios3&&check_smbios3(smbios3)&&
		smbios_set_range(ctx,smbios3->TableAddress,smbios3->TableMaximumSize)
	){
		ctx->smbios3=smbios3;
		ctx->major=ctx->smbios3->MajorVersion;
		ctx->minor=ctx->smbios3->MinorVersion;
	}
	ctx->version=SMBIOS_VER(ctx->major,ctx->minor);
}

static bool smbios_get_range(embloader_smbios*ctx,UINTN*start,UINTN*end){
	if(!ctx||!start||!end)return false;
	*start=ctx->table_start;
	*end=ctx->table_end;
	if(!*start||*start>=*end||*end-*start<sizeof(SMBIOS_STRUCTURE))return false;
	return true;
}

static bool smbios_structure_end(
	embloader_smbios*ctx,
	UINT8*raw,
	UINT8**next
){
	UINTN start=0,end=0;
	UINTN address=(UINTN)raw;
	if(!raw||!smbios_get_range(ctx,&start,&end))return false;
	if(address<start||address>=end)return false;
	if(end-address<sizeof(SMBIOS_STRUCTURE))return false;
	SMBIOS_STRUCTURE*header=(SMBIOS_STRUCTURE*)raw;
	if(
		header->Length<sizeof(SMBIOS_STRUCTURE)||
		header->Length>end-address
	)return false;
	UINT8*str=raw+header->Length;
	while((UINTN)str<end){
		UINT8*nul=memchr(str,0,end-(UINTN)str);
		if(!nul||(UINTN)nul>=end-1)return false;
		if(nul[1]==0){
			if(next)*next=nul+2;
			return true;
		}
		str=nul+1;
	}
	return false;
}

static bool smbios_walk(embloader_smbios*ctx,SMBIOS_STRUCTURE_POINTER*p){
	UINTN start=0,end=0;
	UINT8*raw;
	if(!p||!smbios_get_range(ctx,&start,&end))return false;
	if(!p->Raw)raw=(UINT8*)start;
	else{
		if(!smbios_structure_end(ctx,p->Raw,&raw))return false;
		if(p->Hdr->Type==127||(UINTN)raw>=end)return false;
	}
	if(!smbios_structure_end(ctx,raw,NULL))return false;
	p->Raw=raw;
	return true;
}

SMBIOS_STRUCTURE_POINTER embloader_get_smbios_by_type(
	embloader_smbios*ctx,
	uint8_t type,
	SMBIOS_HANDLE handle
){
	SMBIOS_STRUCTURE_POINTER smbios={},nil={};
	bool past_handle=handle==0xFFFF;
	while(smbios_walk(ctx,&smbios)){
		if(!past_handle){
			if(smbios.Hdr->Handle==handle)past_handle=true;
			continue;
		}
		if(smbios.Hdr->Type==type)return smbios;
	}
	return nil;
}

SMBIOS_STRUCTURE_POINTER embloader_get_smbios_by_handle(
	embloader_smbios*ctx,
	SMBIOS_HANDLE handle
){
	SMBIOS_STRUCTURE_POINTER smbios={},nil={};
	while(smbios_walk(ctx,&smbios))
		if(smbios.Hdr->Handle==handle)return smbios;
	return nil;
}

const char*embloader_get_smbios_string(
	embloader_smbios*ctx,
	SMBIOS_STRUCTURE_POINTER p,
	SMBIOS_TABLE_STRING id
){
	UINT8*record_end=NULL;
	if(ctx&&p.Raw==ctx->active_copy)p.Raw=ctx->active_record;
	if(id==0||!p.Raw||!smbios_structure_end(ctx,p.Raw,&record_end))return NULL;
	if(p.Hdr->Type==127)return NULL;
	const char*str=(const char*)p.Raw+p.Hdr->Length;
	for(unsigned int cur_id=1;(UINTN)str<(UINTN)record_end&&*str;cur_id++){
		const char*nul=memchr(str,0,(UINTN)record_end-(UINTN)str);
		if(!nul)return NULL;
		if(cur_id==id)return str;
		str=nul+1;
	}
	return NULL;
}

void embloader_smbios_set_config_string(
	embloader_smbios *ctx,
	SMBIOS_STRUCTURE_POINTER ptr,
	confignode *node,
	const char *path,
	SMBIOS_TABLE_STRING index
) {
	char buff[256];
	if (!ctx || ctx->config_error) return;
	memset(buff, 0, sizeof(buff));
	embloader_load_smbios_string(
		ctx, ptr, index, buff, sizeof(buff)
	);
	embloader_smbios_config_set_string(ctx, node, path, buff);
}

void embloader_smbios_set_config_guid(
	embloader_smbios *ctx,
	SMBIOS_STRUCTURE_POINTER ptr,
	confignode *node,
	const char *path,
	GUID *guid
) {
	char buff[256];
	GUID aligned;
	if (!ctx || ctx->config_error) return;
	UINTN start=(UINTN)ptr.Raw,address=(UINTN)guid;
	if(!ptr.Raw||!guid||address<start)return;
	UINTN offset=address-start;
	if(offset>ptr.Hdr->Length||ptr.Hdr->Length-offset<sizeof(aligned))return;
	memcpy(&aligned,guid,sizeof(aligned));
	memset(buff, 0, sizeof(buff));
	AsciiSPrint(buff, sizeof(buff), "%g", &aligned);
	embloader_smbios_config_set_string(ctx, node, path, buff);
}

bool embloader_load_smbios_string(
	embloader_smbios*ctx,
	SMBIOS_STRUCTURE_POINTER p,
	SMBIOS_TABLE_STRING id,
	char*buff,
	size_t size
){
	if(!ctx||!buff||size==0)return false;
	memset(buff,0,size);
	const char*v=embloader_get_smbios_string(ctx,p,id);
	if(
		!v||!v[0]||
		strcasestr(v,"unknown")||
		strcasestr(v,"to be filled by")
	)return false;
	strncpy(buff,v,size-1);
	trim(buff);
	return true;
}

bool embloader_smbios_load_table(
	embloader_smbios *ctx,
	confignode *root,
	uint8_t type,
	const char *name,
	void (*load)(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr)
) {
	confignode *m, *hm;
	SMBIOS_STRUCTURE_POINTER ptr;
	SMBIOS_HANDLE handle = 0xFFFF;
	int i = 0;
	if (!ctx || !root || !name) return false;
	while (true) {
		ptr = embloader_get_smbios_by_type(ctx, type, handle);
		if (!ptr.Raw) return true;
		ctx->config_error = false;
		if (!(m = confignode_new_map())) return false;
		char buff[64];
		while (true) {
			snprintf(buff, sizeof(buff), "%s%d", name, i++);
			if (!confignode_path_lookup(root, buff, false)) break;
		}
		hm = confignode_new_map();
		embloader_smbios_config_attach(ctx, m, "header", hm);
		embloader_smbios_config_set_int(ctx, hm, "type", ptr.Hdr->Type);
		embloader_smbios_config_set_int(ctx, hm, "length", ptr.Hdr->Length);
		embloader_smbios_config_set_int(ctx, hm, "handle", ptr.Hdr->Handle);
		if (load && !ctx->config_error) {
			union{
				UINT64 align;
				UINT8 raw[256];
			}formatted={};
			memcpy(formatted.raw,ptr.Raw,ptr.Hdr->Length);
			SMBIOS_STRUCTURE_POINTER copy={.Raw=formatted.raw};
			ctx->active_record=ptr.Raw;
			ctx->active_copy=copy.Raw;
			load(ctx,m,copy);
			ctx->active_record=NULL;
			ctx->active_copy=NULL;
		}
		if (ctx->config_error) {
			confignode_clean(m);
			return false;
		}
		if (!embloader_smbios_config_attach(ctx, root, buff, m))
			return false;
		handle = ptr.Hdr->Handle;
	}
}
