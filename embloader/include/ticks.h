#ifndef TICKS_H
#define TICKS_H
#include <stdint.h>
extern uint64_t ticks_usec(void);
extern uint64_t ticks_msec(void);
extern uint32_t ticks_msec_u32(void);
#endif
