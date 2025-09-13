#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/SmBios.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "str-utils.h"
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
	if(CalculateCheckSum8((void*)smbios+xs,smbios->EntryPointLength-xs)!=0){
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
	if(smbios->EntryPointLength<sizeof(SMBIOS_TABLE_3_0_ENTRY_POINT)){
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
	if(CalculateCheckSum8((void*)smbios,sizeof(SMBIOS_TABLE_3_0_ENTRY_POINT))!=0){
		log_warning("smbios3 checksum mismatch");
		return false;
	}
	return true;
}

void embloader_init_info_smbios(embloader_smbios*ctx){
	SMBIOS_TABLE_ENTRY_POINT*smbios2=NULL;
	SMBIOS_TABLE_3_0_ENTRY_POINT*smbios3=NULL;
	ctx->smbios2=NULL;
	ctx->smbios3=NULL;
	for(UINTN i=0;i<gST->NumberOfTableEntries;i++){
		EFI_GUID*g=&gST->ConfigurationTable[i].VendorGuid;
		void*t=gST->ConfigurationTable[i].VendorTable;
		if(memcmp(g,&gEfiSmbiosTableGuid,sizeof(EFI_GUID))==0)smbios2=t;
		if(memcmp(g,&gEfiSmbios3TableGuid,sizeof(EFI_GUID))==0)smbios3=t;
	}
	if(smbios2&&check_smbios2(smbios2))ctx->smbios2=smbios2;
	if(smbios3&&check_smbios3(smbios3))ctx->smbios3=smbios3;
	if(ctx->smbios2){
		ctx->major=ctx->smbios2->MajorVersion;
		ctx->minor=ctx->smbios2->MinorVersion;
	}
	if(ctx->smbios3){
		ctx->major=ctx->smbios3->MajorVersion;
		ctx->minor=ctx->smbios3->MinorVersion;
	}
	ctx->version=SMBIOS_VER(ctx->major,ctx->minor);
}

static bool smbios_get_range(embloader_smbios*ctx,UINTN*start,UINTN*end){
	if(!ctx||!start||!end)return false;
	if(ctx->smbios2)*start=ctx->smbios2->TableAddress,*end=*start+ctx->smbios2->TableLength;
	if(ctx->smbios3)*start=ctx->smbios3->TableAddress,*end=*start+ctx->smbios3->TableMaximumSize;
	if(!*start||!*end||*start>=*end)return false;
	return true;
}

static bool smbios_walk(embloader_smbios*ctx,SMBIOS_STRUCTURE_POINTER*p){
	UINTN start=0,end=0;
	if(!p||!smbios_get_range(ctx,&start,&end))return false;
	if(!p->Raw){
		p->Raw=(void*)start;
		if(p->Hdr->Length<sizeof(SMBIOS_STRUCTURE))return false;
		return true;
	}
	if(p->Hdr->Type==127)return false;
	if((UINTN)p->Raw+p->Hdr->Length>end)return false;
	if(p->Hdr->Length<sizeof(SMBIOS_STRUCTURE))return false;
	SMBIOS_STRUCTURE_POINTER next=*p;
	next.Raw+=p->Hdr->Length;
	do{
		for(;*next.Raw;next.Raw++);
		next.Raw++;
	}while(*next.Raw);
	next.Raw++;
	if((UINTN)next.Raw+p->Hdr->Length>end)return false;
	if(next.Hdr->Length<sizeof(SMBIOS_STRUCTURE))return false;
	*p=next;
	return true;
}

SMBIOS_STRUCTURE_POINTER embloader_get_smbios_by_type(
	embloader_smbios*ctx,
	uint8_t type,
	SMBIOS_HANDLE handle
){
	SMBIOS_STRUCTURE_POINTER smbios={},nil={};
	while(smbios_walk(ctx,&smbios)){
		if(smbios.Hdr->Type!=type)continue;
		if(handle==0xFFFF)return smbios;
		if(smbios.Hdr->Handle>handle)return smbios;
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
	UINTN start=0,end=0;
	if(id==0||!p.Raw||!smbios_get_range(ctx,&start,&end))return NULL;
	if(p.Hdr->Type==127)return NULL;
	if((UINTN)p.Raw+p.Hdr->Length>end)return NULL;
	if(p.Hdr->Length<sizeof(SMBIOS_STRUCTURE))return NULL;
	const char*str=(void*)p.Raw+p.Hdr->Length;
	for(SMBIOS_TABLE_STRING cur_id=1;*str&&cur_id<=id;cur_id++){
		if(cur_id==id)return str;
		for(;*str;str++);
		str++;
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
	memset(buff, 0, sizeof(buff));
	embloader_load_smbios_string(
		ctx, ptr, index, buff, sizeof(buff)
	);
	confignode_path_set_string(node, path, buff);
}

void embloader_smbios_set_config_guid(
	embloader_smbios *ctx,
	SMBIOS_STRUCTURE_POINTER ptr,
	confignode *node,
	const char *path,
	GUID *guid
) {
	char buff[256];
	memset(buff, 0, sizeof(buff));
	AsciiSPrint(buff, sizeof(buff), "%g", guid);
	confignode_path_set_string(node, path, buff);
}

bool embloader_load_smbios_string(
	embloader_smbios*ctx,
	SMBIOS_STRUCTURE_POINTER p,
	SMBIOS_TABLE_STRING id,
	char*buff,
	size_t size
){
	memset(buff,0,size);
	if(!ctx||!buff||size==0)return false;
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

void embloader_smbios_load_table(
	embloader_smbios *ctx,
	uint8_t type,
	const char *name,
	void (*load)(embloader_smbios *ctx, confignode *m, SMBIOS_STRUCTURE_POINTER ptr)
) {
	confignode *m;
	confignode *s = g_embloader.sysinfo;
	SMBIOS_STRUCTURE_POINTER ptr;
	SMBIOS_HANDLE handle = 0xFFFF;
	int i = 0;
	while (true) {
		ptr = embloader_get_smbios_by_type(ctx, type, handle);
		if (!ptr.Raw) return;
		if (!(m = confignode_new_map())) return;
		char buff[64];
		while (true) {
			snprintf(buff, sizeof(buff), "smbios.%s%d", name, i++);
			if (!confignode_path_lookup(s, buff, false)) break;
		}
		confignode_path_map_set(s, buff, m);
		confignode *hm = confignode_new_map();
		confignode_path_map_set(m, "header", hm);
		confignode_path_set_int(hm, "type", ptr.Hdr->Type);
		confignode_path_set_int(hm, "length", ptr.Hdr->Length);
		confignode_path_set_int(hm, "handle", ptr.Hdr->Handle);
		if (load) load(ctx, m, ptr);
		handle = ptr.Hdr->Handle;
	}
}
