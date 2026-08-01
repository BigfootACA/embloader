#include<stdbool.h>
#include<stdint.h>
#include<stdlib.h>
#include"path.h"

static inline bool is_sep(char c){return c=='/'||c=='\\';}

size_t path_resolve(char*buff,size_t size,char sep,const char*path){
	size_t len=0;
	if(!buff||size==0||!path)return 0;
	if(sep==0)sep='/';
	memset(buff,0,size);
	size--;
	const char*cur=path;
	while(size>0&&*cur){
		if(is_sep(cur[0])){
			if(len>0&&buff[-1]!=sep)
				*(buff++)=sep,len++,size--;
			cur++;
			continue;
		}
		if(cur[0]=='.'&&(len<=0||is_sep(buff[-1]))){
			if(is_sep(cur[1])||!cur[1]){
				cur+=cur[1]?2:1;
				continue;
			}
			if(cur[1]=='.'&&(is_sep(cur[2])||!cur[2])){
				if(len>0){
					if(buff[-1]==sep)
						*(--buff)=0,len--,size++;
					while(len>0&&buff[-1]!=sep)
						*(--buff)=0,len--,size++;
				}
				cur+=cur[2]?3:2;
				continue;
			}
		}
		*(buff++)=*cur++,len++,size--;
	}
	return len;
}

size_t path_merge(char*buff,size_t size,char sep,const char*path1,const char*path2){
	if(!buff||size==0)return 0;
	memset(buff,0,size);
	if(!path1||!path2){
		if(path1)return path_resolve(buff,size,sep,path1);
		else if(path2)return path_resolve(buff,size,sep,path2);
		else return 0;
	}
	size_t len1=strlen(path1),len2=strlen(path2);
	if(len1>SIZE_MAX-len2-2)return 0;
	char*path=malloc(len1+len2+2);
	if(!path)return 0;
	memcpy(path,path1,len1);
	path[len1]='/';
	memcpy(path+len1+1,path2,len2+1);
	size_t ret=path_resolve(buff,size,sep,path);
	free(path);
	return ret;
}

const char*path_basename(char*buff,size_t size,const char*path){
	size_t cnt;
	const char*start,*end,*p;
	if(!buff||size<=0||!path)return NULL;
	memset(buff,0,size);
	start=path,end=start+strlen(path);
	while(end>start&&is_sep(end[-1]))end--;
	for(p=start;p<end;p++)if(is_sep(*p))start=p+1;
	cnt=MIN((size_t)(end-start),size-1);
	if(cnt>0)strncpy(buff,start,cnt);
	return buff;
}

const char*path_dirname(char*buff,size_t size,const char*path){
	size_t cnt;
	const char*start,*end;
	if(!buff||size<=0||!path)return NULL;
	memset(buff,0,size);
	start=path,end=start+strlen(path);
	while(end>start&&is_sep(end[-1]))end--;
	while(end>start&&!is_sep(end[-1]))end--;
	cnt=MIN((size_t)(end-start),size-1);
	if(cnt>0)strncpy(buff,start,cnt);
	return buff;
}
