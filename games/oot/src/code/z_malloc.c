#include "global.h"
#include <string.h>

#define LOG_SEVERITY_NOLOG 0
#define LOG_SEVERITY_ERROR 2
#define LOG_SEVERITY_VERBOSE 3

s32 gZeldaArenaLogSeverity = LOG_SEVERITY_ERROR;
Arena OoT_sZeldaArena;

void ZeldaArena_CheckPointer(void* ptr, size_t size, const char* name, const char* action) {
    if (ptr == NULL) {
        if (gZeldaArenaLogSeverity >= LOG_SEVERITY_ERROR) {
            // "%s: %u bytes %s failed\n"
            osSyncPrintf("%s: %u バイトの%sに失敗しました\n", name, size, action);
            __osDisplayArena(&OoT_sZeldaArena);
        }
    } else if (gZeldaArenaLogSeverity >= LOG_SEVERITY_VERBOSE) {
        // "%s: %u bytes %s succeeded\n"
        osSyncPrintf("%s: %u バイトの%sに成功しました\n", name, size, action);
    }
}

void* OoT_ZeldaArena_Malloc(size_t size) {
    void* ptr = __osMalloc(&OoT_sZeldaArena, size);

    ZeldaArena_CheckPointer(ptr, size, "zelda_malloc", "確保"); // "Secure"
    return ptr;
}

void* ZeldaArena_MallocDebug(size_t size, const char* file, s32 line) {
    void* ptr = __osMallocDebug(&OoT_sZeldaArena, size, file, line);

    ZeldaArena_CheckPointer(ptr, size, "zelda_malloc_DEBUG", "確保"); // "Secure"
    return ptr;
}

void* OoT_ZeldaArena_MallocR(size_t size) {
    void* ptr = __osMallocR(&OoT_sZeldaArena, size);

    ZeldaArena_CheckPointer(ptr, size, "zelda_malloc_r", "確保"); // "Secure"
    return ptr;
}

void* ZeldaArena_MallocRDebug(size_t size, const char* file, s32 line) {
    void* ptr = __osMallocRDebug(&OoT_sZeldaArena, size, file, line);

    ZeldaArena_CheckPointer(ptr, size, "zelda_malloc_r_DEBUG", "確保"); // "Secure"
    return ptr;
}

void* OoT_ZeldaArena_Realloc(void* ptr, size_t newSize) {
    ptr = __osRealloc(&OoT_sZeldaArena, ptr, newSize);
    ZeldaArena_CheckPointer(ptr, newSize, "zelda_realloc", "再確保"); // "Re-securing"
    return ptr;
}

void* ZeldaArena_ReallocDebug(void* ptr, size_t newSize, const char* file, s32 line) {
    ptr = __osReallocDebug(&OoT_sZeldaArena, ptr, newSize, file, line);
    ZeldaArena_CheckPointer(ptr, newSize, "zelda_realloc_DEBUG", "再確保"); // "Re-securing"
    return ptr;
}

void OoT_ZeldaArena_Free(void* ptr) {
    __osFree(&OoT_sZeldaArena, ptr);
}

void ZeldaArena_FreeDebug(void* ptr, const char* file, s32 line) {
    __osFreeDebug(&OoT_sZeldaArena, ptr, file, line);
}

void* OoT_ZeldaArena_Calloc(size_t num, size_t size) {
    void* ret;
    size_t n = num * size;

    ret = __osMalloc(&OoT_sZeldaArena, n);
    if (ret != NULL) {
        memset(ret, 0, n);
    }

    ZeldaArena_CheckPointer(ret, n, "zelda_calloc", "確保");
    return ret;
}

void ZeldaArena_Display() {
    osSyncPrintf("ゼルダヒープ表示\n"); // "Zelda heap display"
    __osDisplayArena(&OoT_sZeldaArena);
}

void OoT_ZeldaArena_GetSizes(u32* outMaxFree, u32* outFree, u32* outAlloc) {
    ArenaImpl_GetSizes(&OoT_sZeldaArena, outMaxFree, outFree, outAlloc);
}

void OoT_ZeldaArena_Check() {
    __osCheckArena(&OoT_sZeldaArena);
}

void OoT_ZeldaArena_Init(void* start, size_t size) {
    gZeldaArenaLogSeverity = LOG_SEVERITY_NOLOG;
    __osMallocInit(&OoT_sZeldaArena, start, size);
}

void OoT_ZeldaArena_Cleanup() {
    gZeldaArenaLogSeverity = LOG_SEVERITY_NOLOG;
    __osMallocCleanup(&OoT_sZeldaArena);
}

u8 ZeldaArena_IsInitalized() {
    return __osMallocIsInitialized(&OoT_sZeldaArena);
}
