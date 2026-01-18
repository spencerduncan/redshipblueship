#include "global.h"
#include <string.h>

#define LOG_SEVERITY_NOLOG 0
#define LOG_SEVERITY_ERROR 2
#define LOG_SEVERITY_VERBOSE 3

s32 gSystemArenaLogSeverity = LOG_SEVERITY_NOLOG;
Arena OoT_gSystemArena;

void SystemArena_CheckPointer(void* ptr, size_t size, const char* name, const char* action) {
    if (ptr == NULL) {
        if (gSystemArenaLogSeverity >= LOG_SEVERITY_ERROR) {
            // "%s: %u bytes %s failed\n"
            osSyncPrintf("%s: %u バイトの%sに失敗しました\n", name, size, action);
            __osDisplayArena(&OoT_gSystemArena);
            return;
        }
    } else if (gSystemArenaLogSeverity >= LOG_SEVERITY_VERBOSE) {
        // "%s: %u bytes %s succeeded\n"
        osSyncPrintf("%s: %u バイトの%sに成功しました\n", name, size, action);
    }
}

void* OoT_SystemArena_Malloc(size_t size) {
    void* ptr = __osMalloc(&OoT_gSystemArena, size);

    SystemArena_CheckPointer(ptr, size, "malloc", "確保"); // "Secure"
    return ptr;
}

void* SystemArena_MallocDebug(size_t size, const char* file, s32 line) {
    void* ptr = __osMallocDebug(&OoT_gSystemArena, size, file, line);

    SystemArena_CheckPointer(ptr, size, "malloc_DEBUG", "確保"); // "Secure"
    return ptr;
}

void* OoT_SystemArena_MallocR(size_t size) {
    void* ptr = __osMallocR(&OoT_gSystemArena, size);

    SystemArena_CheckPointer(ptr, size, "malloc_r", "確保"); // "Secure"
    return ptr;
}

void* SystemArena_MallocRDebug(size_t size, const char* file, s32 line) {
    void* ptr = __osMallocRDebug(&OoT_gSystemArena, size, file, line);

    SystemArena_CheckPointer(ptr, size, "malloc_r_DEBUG", "確保"); // "Secure"
    return ptr;
}

void* OoT_SystemArena_Realloc(void* ptr, size_t newSize) {
    ptr = __osRealloc(&OoT_gSystemArena, ptr, newSize);
    SystemArena_CheckPointer(ptr, newSize, "realloc", "再確保"); // "Re-securing"
    return ptr;
}

void* SystemArena_ReallocDebug(void* ptr, size_t newSize, const char* file, s32 line) {
    ptr = __osReallocDebug(&OoT_gSystemArena, ptr, newSize, file, line);
    SystemArena_CheckPointer(ptr, newSize, "realloc_DEBUG", "再確保"); // "Re-securing"
    return ptr;
}

void OoT_SystemArena_Free(void* ptr) {
    __osFree(&OoT_gSystemArena, ptr);
}

void SystemArena_FreeDebug(void* ptr, const char* file, s32 line) {
    __osFreeDebug(&OoT_gSystemArena, ptr, file, line);
}

void* OoT_SystemArena_Calloc(size_t num, size_t size) {
    void* ret;
    size_t n = num * size;

    ret = __osMalloc(&OoT_gSystemArena, n);
    if (ret != NULL) {
        memset(ret, 0, n);
    }

    SystemArena_CheckPointer(ret, n, "calloc", "確保");
    return ret;
}

void SystemArena_Display(void) {
    osSyncPrintf("システムヒープ表示\n"); // "System heap display"
    __osDisplayArena(&OoT_gSystemArena);
}

void OoT_SystemArena_GetSizes(u32* outMaxFree, u32* outFree, u32* outAlloc) {
    ArenaImpl_GetSizes(&OoT_gSystemArena, outMaxFree, outFree, outAlloc);
}

void SystemArena_Check(void) {
    __osCheckArena(&OoT_gSystemArena);
}

void OoT_SystemArena_Init(void* start, size_t size) {
    gSystemArenaLogSeverity = LOG_SEVERITY_NOLOG;
    __osMallocInit(&OoT_gSystemArena, start, size);
}

void OoT_SystemArena_Cleanup(void) {
    gSystemArenaLogSeverity = LOG_SEVERITY_NOLOG;
    __osMallocCleanup(&OoT_gSystemArena);
}

u8 SystemArena_IsInitalized(void) {
    return __osMallocIsInitialized(&OoT_gSystemArena);
}
