#ifndef PR_OS_INTERNAL_REG_H
#define PR_OS_INTERNAL_REG_H

#include "ultratypes.h"
#include "os_exception.h"


u32 __osGetCause(void);
void __osSetCause(u32);
u32 __osGetCompare(void);
void __osSetCompare(u32 value);
u32 __osGetConfig(void);
void __osSetConfig(u32);
u32 __osGetSR(void);
void __osSetSR(u32 value);
OSIntMask MM___osDisableInt(void);
void MM___osRestoreInt(OSIntMask im);
u32 __osGetWatchLo(void);
void __osSetWatchLo(u32 value);

u32 MM___osSetFpcCsr(u32 value);
u32 MM___osGetFpcCsr(void);

#endif
