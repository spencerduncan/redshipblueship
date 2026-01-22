#ifndef SYSTEM_MALLOC_H
#define SYSTEM_MALLOC_H

#include "PR/ultratypes.h"
#include "os_malloc.h"

void* MM_SystemArena_Malloc(size_t size);
void* MM_SystemArena_MallocR(size_t size);
void* MM_SystemArena_Realloc(void* oldPtr, size_t newSize);
void MM_SystemArena_Free(void* ptr);
void* MM_SystemArena_Calloc(size_t num, size_t size);
void MM_SystemArena_GetSizes(size_t* maxFreeBlock, size_t* bytesFree, size_t* bytesAllocated);
u32 SystemArena_CheckArena(void);
void MM_SystemArena_Init(void* start, size_t size);
void MM_SystemArena_Cleanup(void);
u8 SystemArena_IsInitialized(void);

extern Arena MM_gSystemArena;

#endif
