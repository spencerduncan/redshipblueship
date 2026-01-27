#include "ultra64.h"
#include "libc/stdlib.h"
#include "libc/string.h"
#include <stdio.h>

#if 0
void* MM_proutSprintf(void* dst, const char* fmt, size_t size) {
    return (void*)((uintptr_t)memcpy(dst, fmt, size) + size);
}

int MM_vsprintf(char* dst, char* fmt, va_list args) {
    int ans = MM__Printf(MM_proutSprintf, dst, fmt, args);
    if (ans > -1) {
        dst[ans] = 0;
    }
    return ans;
}

int MM_sprintf(char* dst, const char* fmt, ...) {
    int ans;
    va_list args;
    va_start(args, fmt);

    ans = MM__Printf(&MM_proutSprintf, dst, fmt, args);
    if (ans > -1) {
        dst[ans] = 0;
    }

    va_end(args);

    return ans;
}
#endif
