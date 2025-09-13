#include<stdbool.h>
#include"path.h"

static inline bool is_sep(char c){return c=='/'||c=='\\';}

size_t path_resolve(char*buff,size_t size,char sep,const char*path){
	size_t len=0;
	if(!buff||size<=0||!path)return 0;
	if(sep==0)sep='/';
	memset(buff,0,size);
	for(;size>0&&*path;path++){
		if(is_sep(path[0])){
			if(len>0&&buff[-1]!=sep)
				*(buff++)='/',len++,size--;
			continue;
		}
		if(path[0]=='.'&&(len<=0||is_sep(buff[-1]))){
			if(is_sep(path[1])||!path[1]){
				path++;
				continue;
			}
			if(path[1]=='.'&&(is_sep(path[2])||!path[2])){
				if(len>0){
					if(buff[-1]==sep)
						*(--buff)=0,len--,size++;
					while(len>0&&buff[-1]!=sep)
						*(--buff)=0,len--,size++;
				}
				path+=2;
				continue;
			}
		}
		*(buff++)=*path,len++,size--;
	}
	return len;
}

size_t path_merge(char*buff,size_t size,char sep,const char*path1,const char*path2){
	char path[8192];
	if(!buff||size<=0)return 0;
	if(!path1||!path2){
		if(path1)return path_resolve(buff,size,sep,path1);
		else if(path2)return path_resolve(buff,size,sep,path2);
		else return 0;
	}
	memset(path,0,sizeof(path));
	strlcat(path,path1,sizeof(path)-1);
	strlcat(path,"/",sizeof(path)-1);
	strlcat(path,path2,sizeof(path)-1);
	return path_resolve(buff,size,sep,path);
}

const char*path_basename(char*buff,size_t size,const char*path){
	size_t cnt;
	const char*start,*end,*p;
	if(!buff||size<=0||!path)return NULL;
	memset(buff,0,size);
	start=path,end=start+strlen(path);
	while(end>start&&is_sep(end[-1]))end--;
	for(p=start;p<end;p++)if(is_sep(*p))start=p+1;
	cnt=MIN((size_t)(end-start),size);
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
	cnt=MIN((size_t)(end-start),size);
	if(cnt>0)strncpy(buff,start,cnt);
	return buff;
}
