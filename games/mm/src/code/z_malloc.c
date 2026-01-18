#include "z64malloc.h"

#include "os_malloc.h"
#include <string.h>

Arena MM_sZeldaArena;

void* MM_ZeldaArena_Malloc(size_t size) {
    void* ptr = __osMalloc(&MM_sZeldaArena, size);

    return ptr;
}

void* MM_ZeldaArena_MallocR(size_t size) {
    void* ptr = __osMallocR(&MM_sZeldaArena, size);

    return ptr;
}

void* MM_ZeldaArena_Realloc(void* ptr, size_t newSize) {
    ptr = __osRealloc(&MM_sZeldaArena, ptr, newSize);
    return ptr;
}

void MM_ZeldaArena_Free(void* ptr) {
    __osFree(&MM_sZeldaArena, ptr);
}

void* MM_ZeldaArena_Calloc(size_t num, size_t size) {
    void* ptr;
    size_t totalSize = num * size;

    ptr = __osMalloc(&MM_sZeldaArena, totalSize);
    if (ptr != NULL) {
        memset(ptr, 0, totalSize);
    }

    return ptr;
}

void MM_ZeldaArena_GetSizes(size_t* outMaxFree, size_t* outFree, size_t* outAlloc) {
    __osGetSizes(&MM_sZeldaArena, outMaxFree, outFree, outAlloc);
}

s32 MM_ZeldaArena_Check(void) {
    return __osCheckArena(&MM_sZeldaArena);
}

void MM_ZeldaArena_Init(void* start, size_t size) {
    __osMallocInit(&MM_sZeldaArena, start, size);
}

void MM_ZeldaArena_Cleanup(void) {
    __osMallocCleanup(&MM_sZeldaArena);
}

u8 ZeldaArena_IsInitialized(void) {
    return __osMallocIsInitalized(&MM_sZeldaArena);
}
