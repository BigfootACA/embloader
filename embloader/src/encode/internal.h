#ifndef EMBLOADER_ENCODE_INTERNAL_H
#define EMBLOADER_ENCODE_INTERNAL_H
#include "encode.h"
struct encoding_convert {
	encoding src;
	encoding dst;
	EFI_STATUS (*convert)(encode_convert_ctx* ctx);
};
extern struct encoding_convert encode_converts[];
extern EFI_STATUS encode_utf8_to_utf16_convert(encode_convert_ctx* ctx);
extern EFI_STATUS encode_utf16_to_utf8_convert(encode_convert_ctx* ctx);
extern EFI_STATUS encode_proc_transfer(encode_convert_ctx* ctx, size_t* s, size_t* d);
extern EFI_STATUS encode_copy_convert(encode_convert_ctx* ctx);
extern EFI_STATUS encode_center_convert(encode_convert_ctx* ctx);
#endif
