#ifndef MAIN_H
#define MAIN_H

#include "ultra64.h"
#include "scheduler.h"
#include "padmgr.h"

extern s32 MM_gScreenWidth;
extern s32 MM_gScreenHeight;
extern size_t MM_gSystemHeapSize;

extern uintptr_t MM_gSegments[NUM_SEGMENTS];
extern SchedContext MM_gSchedContext;
extern OSThread gGraphThread;
extern PadMgr MM_gPadMgr;

void Main(void* arg);

#define SEGMENTED_TO_K0(addr) (addr) // (void*)((MM_gSegments[SEGMENT_NUMBER(addr)] + K0BASE) + SEGMENT_OFFSET(addr))

#endif
