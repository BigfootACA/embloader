#ifndef READABLE_H
#define READABLE_H
#include <stddef.h>
#include <stdint.h>
extern const char*size_units_b[];
extern const char*size_units_ib[];
extern const char*size_units_ibs[];
extern const char*size_units_hz[];
extern const char*format_size_ex(char*buf,size_t len,uint64_t val,const char**units,size_t blk);
extern const char*format_size_float_ex(char*buf,size_t len,uint64_t val,const char**units,size_t blk,uint8_t dot);
#define format_size(buf,val) format_size_ex(buf,sizeof(buf),val,size_units_ib,1024)
#define format_size_hz(buf,val) format_size_ex(buf,sizeof(buf),val,size_units_hz,1000)
#define format_size_float(buf,val) format_size_float_ex(buf,sizeof(buf),val,size_units_ib,1024,2)
#define format_size_float_hz(buf,val) format_size_float_ex(buf,sizeof(buf),val,size_units_hz,1000,2)
#endif
