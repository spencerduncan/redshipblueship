#if 0
#include "prevent_bss_reordering.h"
#include "carthandle.h"
#include "idle.h"
#include "stack.h"
#include "stackcheck.h"
#include "CIC6105.h"
#include "z64thread.h"

StackEntry sBootStackInfo;
OSThread MM_sIdleThread;
STACK(sIdleStack, 0x400);
StackEntry sIdleStackInfo;
STACK(sBootStack, 0x400);

void MM_bootproc(void) {
    MM_StackCheck_Init(&sBootStackInfo, sBootStack, STACK_TOP(sBootStack), 0, -1, "boot");
    MM_osMemSize = MM_osGetMemSize();
    CIC6105_Init();
    osInitialize();
    osUnmapTLBAll();
    MM_gCartHandle = MM_osCartRomInit();
    MM_StackCheck_Init(&sIdleStackInfo, sIdleStack, STACK_TOP(sIdleStack), 0, 0x100, "idle");
    MM_osCreateThread(&MM_sIdleThread, Z_THREAD_ID_IDLE, MM_Idle_ThreadEntry, NULL, STACK_TOP(sIdleStack), Z_PRIORITY_IDLE);
    MM_osStartThread(&MM_sIdleThread);
}
#endif