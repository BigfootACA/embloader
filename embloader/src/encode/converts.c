#include "internal.h"

struct encoding_convert encode_converts[] = {
	{ENC_UTF8,  ENC_UTF16, encode_utf8_to_utf16_convert},
	{ENC_UTF16, ENC_UTF8,  encode_utf16_to_utf8_convert},
	{ENC_NONE,  ENC_NONE,  NULL},
};
