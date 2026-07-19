#ifndef PR_OS_AI_H
#define PR_OS_AI_H

#include <libultraship/libultra/os.h>

// RSBS: revived from the `#if 0` block below (the gu.h C4013 class).
// Definition: games/mm/src/code/stubs.c (returns the port's fixed 32006 Hz).
s32 MM_osAiSetFrequency(u32 frequency);

#if 0

#include "ultratypes.h"


u32 MM_osAiGetLength(void);
s32 MM_osAiSetFrequency(u32 frequency);
s32 MM_osAiSetNextBuffer(void* buf, u32 size);

#endif // 0

#endif
