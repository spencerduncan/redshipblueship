#include "global.h"
#include "os_malloc.h"
#include <string.h>

Arena MM_gSystemArena;

void* MM_SystemArena_Malloc(size_t size) {
    return __osMalloc(&MM_gSystemArena, size);
}

void* MM_SystemArena_MallocR(size_t size) {
    return __osMallocR(&MM_gSystemArena, size);
}

void* MM_SystemArena_Realloc(void* oldPtr, size_t newSize) {
    return __osRealloc(&MM_gSystemArena, oldPtr, newSize);
}

void MM_SystemArena_Free(void* ptr) {
    __osFree(&MM_gSystemArena, ptr);
}

void* MM_SystemArena_Calloc(size_t num, size_t size) {
    void* ptr;
    size_t totalSize = num * size;

    ptr = __osMalloc(&MM_gSystemArena, totalSize);
    if (ptr != NULL) {
        memset(ptr, 0, totalSize);
    }
    return ptr;
}

void MM_SystemArena_GetSizes(size_t* maxFreeBlock, size_t* bytesFree, size_t* bytesAllocated) {
    __osGetSizes(&MM_gSystemArena, maxFreeBlock, bytesFree, bytesAllocated);
}

u32 SystemArena_CheckArena(void) {
    return __osCheckArena(&MM_gSystemArena);
}

void MM_SystemArena_Init(void* start, size_t size) {
    __osMallocInit(&MM_gSystemArena, start, size);
}

void MM_SystemArena_Cleanup(void) {
    __osMallocCleanup(&MM_gSystemArena);
}

u8 SystemArena_IsInitialized(void) {
    return __osMallocIsInitalized(&MM_gSystemArena);
}
