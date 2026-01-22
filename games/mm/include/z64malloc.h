#ifndef Z64MALLOC_H
#define Z64MALLOC_H

#include "ultra64.h"

void* MM_ZeldaArena_Malloc(size_t size);
void* MM_ZeldaArena_MallocR(size_t size);
void* MM_ZeldaArena_Realloc(void* ptr, size_t newSize);
void MM_ZeldaArena_Free(void* ptr);
void* MM_ZeldaArena_Calloc(size_t num, size_t size);
void MM_ZeldaArena_GetSizes(size_t* outMaxFree, size_t* outFree, size_t* outAlloc);
s32 MM_ZeldaArena_Check(void);
void MM_ZeldaArena_Init(void* start, size_t size);
void MM_ZeldaArena_Cleanup(void);
u8 ZeldaArena_IsInitialized(void);

#endif
