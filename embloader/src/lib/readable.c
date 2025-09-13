#include"readable.h"
#include"str-utils.h"
#include<string.h>
#include<stdio.h>
#include<inttypes.h>

const char*size_units_b[]={"B","KB","MB","GB","TB","PB","EB","ZB","YB",NULL};
const char*size_units_ib[]={"B","KiB","MiB","GiB","TiB","PiB","EiB","ZiB","YiB",NULL};
const char*size_units_ibs[]={"B/s","KiB/s","MiB/s","GiB/s","TiB/s","PiB/s","EiB/s","ZiB/s","YiB/s",NULL};
const char*size_units_hz[]={"Hz","KHz","MHz","GHz","THz","PHz","EHz","ZHz","YHz",NULL};

const char*format_size_ex(
	char*buf,size_t len,
	uint64_t val,const char**units,
	size_t blk
){
	int unit=0;
	if(!buf||len<=0||!units)return NULL;
	memset(buf,0,len);
	if(val==0)return strncpy(buf,"0",len);
	while((val>=blk)&&units[unit+1])val/=blk,unit++;
	snprintf(buf,len-1,"%" PRIu64 " %s",val,units[unit]);
	return buf;
}

const char*format_size_float_ex(
	char*buf,size_t len,
	uint64_t val,const char**units,
	size_t blk,uint8_t dot
){
	int unit=0;
	uint8_t ncnt=1,append,pdot;
	uint64_t left,right,pd=10,xright;
	if(!buf||len<=0||!units)return NULL;
	if(dot==0)return format_size_ex(buf,len,val,units,blk);
	for(pdot=dot;pdot>0;pdot--)pd*=10;
	memset(buf,0,len);
	if(val==0)return strncpy(buf,"0",len);
	while((val>=blk*blk)&&units[unit+1])
		val/=blk,unit++;
	left=val,right=0;
	if(val>=blk&&units[unit+1])
		left=val/blk,right=(val%blk)*pd/blk,unit++;
	if(right%10>=5)right+=10;
	right/=10;
	while(right>0&&(right%10)==0)right/=10;
	for(xright=right;xright>10;xright/=10)ncnt++;
	scprintf(buf,len,"%llu",left);
	if(dot>0)scprintf(buf,len,".%llu",right);
	if(dot>ncnt)for(append=0;append<dot-ncnt;append++)
		strlcat(buf,"0",len);
	strlcat(buf,units[unit],len);
	return buf;
}
