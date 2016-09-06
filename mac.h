#ifndef _MC_MAC_H
#define _MC_MAC_H

#include <limits.h>

enum {
	AC_M_RESERVED = INT_MIN,
	AC_INVALID = INT_MIN + 1
};

uint16_t decode_ID(uint16_t);
uint16_t decode_Mode_A(uint16_t);
int32_t  decode_AC(uint16_t raw, bool M);

#endif
