#include "global.h"

StackEntry sBootThreadInfo;
OSThread OoT_sIdleThread;
u8 sIdleThreadStack[0x400];
StackEntry sIdleThreadInfo;
u8 sBootThreadStack[0x400];

void cleararena(void) {
    // bzero(_dmadataSegmentStart, osMemSize - OS_K0_TO_PHYSICAL(_dmadataSegmentStart));
}

void OoT_bootproc(void) {
    OoT_StackCheck_Init(&sBootThreadInfo, sBootThreadStack, sBootThreadStack + sizeof(sBootThreadStack), 0, -1, "boot");

    osMemSize = osGetMemSize();
    cleararena();
    __osInitialize_common();
    __osInitialize_autodetect();

    OoT_gCartHandle = osCartRomInit();
    osDriveRomInit();
    isPrintfInit();
    Locale_Init();

    OoT_StackCheck_Init(&sIdleThreadInfo, sIdleThreadStack, sIdleThreadStack + sizeof(sIdleThreadStack), 0, 256, "idle");
    osCreateThread(&OoT_sIdleThread, 1, OoT_Idle_ThreadEntry, NULL, sIdleThreadStack + sizeof(sIdleThreadStack),
                   Z_PRIORITY_MAIN);
    osStartThread(&OoT_sIdleThread);
}
