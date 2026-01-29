#include "global.h"
#include "os_malloc.h"
#include <string.h>

Arena MM_gSystemArena;

void* MM_SystemArena_Malloc(size_t size) {
    return MM___osMalloc(&MM_gSystemArena, size);
}

void* MM_SystemArena_MallocR(size_t size) {
    return MM___osMallocR(&MM_gSystemArena, size);
}

void* MM_SystemArena_Realloc(void* oldPtr, size_t newSize) {
    return MM___osRealloc(&MM_gSystemArena, oldPtr, newSize);
}

void MM_SystemArena_Free(void* ptr) {
    MM___osFree(&MM_gSystemArena, ptr);
}

void* MM_SystemArena_Calloc(size_t num, size_t size) {
    void* ptr;
    size_t totalSize = num * size;

    ptr = MM___osMalloc(&MM_gSystemArena, totalSize);
    if (ptr != NULL) {
        memset(ptr, 0, totalSize);
    }
    return ptr;
}

void MM_SystemArena_GetSizes(size_t* maxFreeBlock, size_t* bytesFree, size_t* bytesAllocated) {
    __osGetSizes(&MM_gSystemArena, maxFreeBlock, bytesFree, bytesAllocated);
}

u32 SystemArena_CheckArena(void) {
    return MM___osCheckArena(&MM_gSystemArena);
}

void MM_SystemArena_Init(void* start, size_t size) {
    MM___osMallocInit(&MM_gSystemArena, start, size);
}

void MM_SystemArena_Cleanup(void) {
    MM___osMallocCleanup(&MM_gSystemArena);
}

u8 SystemArena_IsInitialized(void) {
    return __osMallocIsInitalized(&MM_gSystemArena);
}
