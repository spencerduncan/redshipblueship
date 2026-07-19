#ifndef PR_OS_LIBC_H
#define PR_OS_LIBC_H

#include "libc/stdarg.h"
#include <string.h>

// RSBS: revived from the `#if 0` block below (the gu.h C4013 class).
// Definition: games/mm/src/boot/O2/sprintf.c.
int MM_sprintf(char* dst, const char* fmt, ...);

// 2S2H This file might just outright get removed soon. Ifdeffing for now
#if 0
//void bcopy(void* __src, void* __dest, int __n);
//int bcmp(void* __s1, void* __s2, int __n);
//void bzero(void* begin, int length);

// s32 MM_vsprintf(char* dst, char* fmt, va_list args);
int MM_sprintf(char* dst, const char* fmt, ...);
void osSyncPrintf(const char* fmt, ...);
#endif

#endif