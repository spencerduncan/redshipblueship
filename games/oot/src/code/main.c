#ifdef _WIN32
#include <Windows.h>
#include <locale.h>
#endif

#include "global.h"
#include "vt.h"
#include "stdio.h"
#include <soh/Enhancements/bootcommands.h>
#include "soh/OTRGlobals.h"

#include <libultraship/bridge.h>
#include "soh/CrashHandlerExt.h"

s32 OoT_gScreenWidth = SCREEN_WIDTH;
s32 OoT_gScreenHeight = SCREEN_HEIGHT;
size_t OoT_gSystemHeapSize = 0;

PreNmiBuff* gAppNmiBufferPtr;
SchedContext OoT_gSchedContext;
PadMgr OoT_gPadMgr;
IrqMgr OoT_gIrqMgr;
uintptr_t OoT_gSegments[NUM_SEGMENTS];
OSThread sGraphThread;
u8 sGraphStack[0x1800];
u8 sSchedStack[0x600];
u8 sAudioStack[0x800];
u8 sPadMgrStack[0x500];
u8 sIrqMgrStack[0x500];
StackEntry OoT_sGraphStackInfo;
StackEntry OoT_sSchedStackInfo;
StackEntry OoT_sAudioStackInfo;
StackEntry OoT_sPadMgrStackInfo;
StackEntry OoT_sIrqMgrStackInfo;
AudioMgr gAudioMgr;
OSMesgQueue sSiIntMsgQ;
OSMesg sSiIntMsgBuf[1];

void Main_LogSystemHeap(void) {
    osSyncPrintf(VT_FGCOL(GREEN));
    // "System heap size% 08x (% dKB) Start address% 08x"
    osSyncPrintf("システムヒープサイズ %08x(%dKB) 開始アドレス %08x\n", OoT_gSystemHeapSize, OoT_gSystemHeapSize / 1024,
                 OoT_gSystemHeap);
    osSyncPrintf(VT_RST);
}

/* In single-exe mode, rsbs/src/main.cpp provides the entry point */
#ifndef RSBS_SINGLE_EXECUTABLE
#ifdef _WIN32
int OoT_SDL_main(int argc, char* argv[]) {
    AllocConsole();
    (void)freopen("CONIN$", "r", stdin);
    (void)freopen("CONOUT$", "w", stdout);
    (void)freopen("CONOUT$", "w", stderr);
#ifndef _DEBUG
    ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif
    // Allow non-ascii characters for Windows
    setlocale(LC_ALL, ".UTF8");

#else //_WIN32
int main(int argc, char* argv[]) {
#endif
    GameConsole_Init();
    InitOTR(argc, argv);
    // TODO: Was moved to below InitOTR because it requires window to be setup. But will be late to catch crashes.
    CrashHandlerRegisterCallback(CrashHandler_PrintSohData);
    BootCommands_Init();

    OoT_Heaps_Alloc();
    Main(0);
    DeinitOTR();
    OoT_Heaps_Free();
    return 0;
}
#endif /* RSBS_SINGLE_EXECUTABLE */

void Main(void* arg) {
    IrqMgrClient irqClient;
    OSMesgQueue irqMgrMsgQ;
    OSMesg irqMgrMsgBuf[60];
    uintptr_t sysHeap;
    uintptr_t fb;
    void* debugHeap;
    size_t debugHeapSize;
    s16* msg;

    osSyncPrintf("mainproc 実行開始\n"); // "Start running"
    OoT_gScreenWidth = SCREEN_WIDTH;
    OoT_gScreenHeight = SCREEN_HEIGHT;
    gAppNmiBufferPtr = (PreNmiBuff*)osAppNmiBuffer;
    PreNmiBuff_Init(gAppNmiBufferPtr);
    OoT_Fault_Init();
    OoT_SysCfb_Init(0);
    sysHeap = (uintptr_t)OoT_gSystemHeap;
    fb = SysCfb_GetFbPtr(0);
    OoT_gSystemHeapSize = 1024 * 1024 * 4;
    // "System heap initalization"
    osSyncPrintf("システムヒープ初期化 %08x-%08x %08x\n", sysHeap, fb, OoT_gSystemHeapSize);
    OoT_SystemHeap_Init((void*)sysHeap, OoT_gSystemHeapSize); // initializes the system heap
    if (osMemSize >= 0x800000) {
        debugHeap = (void*)SysCfb_GetFbEnd();
        debugHeapSize = (0x80600000 - (uintptr_t)debugHeap);
    } else {
        debugHeapSize = 0x400;
        debugHeap = SYSTEM_ARENA_MALLOC_DEBUG(debugHeapSize);
    }

    debugHeapSize = 1024 * 64;

    osSyncPrintf("debug_InitArena(%08x, %08x)\n", debugHeap, debugHeapSize);
    DebugArena_Init(debugHeap, debugHeapSize);
    func_800636C0();

    R_ENABLE_ARENA_DBG = 0;

    osCreateMesgQueue(&sSiIntMsgQ, sSiIntMsgBuf, 1);
    osSetEventMesg(5, &sSiIntMsgQ, OS_MESG_PTR(NULL));

    Main_LogSystemHeap();

    osCreateMesgQueue(&irqMgrMsgQ, irqMgrMsgBuf, 0x3C);
    OoT_StackCheck_Init(&OoT_sIrqMgrStackInfo, sIrqMgrStack, sIrqMgrStack + sizeof(sIrqMgrStack), 0, 0x100, "irqmgr");
    OoT_IrqMgr_Init(&OoT_gIrqMgr, &OoT_sGraphStackInfo, Z_PRIORITY_IRQMGR, 1);

    osSyncPrintf("タスクスケジューラの初期化\n"); // "Initialize the task scheduler"
    OoT_StackCheck_Init(&OoT_sSchedStackInfo, sSchedStack, sSchedStack + sizeof(sSchedStack), 0, 0x100, "sched");
    OoT_Sched_Init(&OoT_gSchedContext, &sAudioStack, Z_PRIORITY_SCHED, D_80013960, 1, &OoT_gIrqMgr);

    OoT_IrqMgr_AddClient(&OoT_gIrqMgr, &irqClient, &irqMgrMsgQ);

    OoT_StackCheck_Init(&OoT_sAudioStackInfo, sAudioStack, sAudioStack + sizeof(sAudioStack), 0, 0x100, "audio");
    OoT_AudioMgr_Init(&gAudioMgr, sAudioStack + sizeof(sAudioStack), Z_PRIORITY_AUDIOMGR, 0xA, &OoT_gSchedContext, &OoT_gIrqMgr);

    OoT_StackCheck_Init(&OoT_sPadMgrStackInfo, sPadMgrStack, sPadMgrStack + sizeof(sPadMgrStack), 0, 0x100, "padmgr");
    OoT_PadMgr_Init(&OoT_gPadMgr, &sSiIntMsgQ, &OoT_gIrqMgr, 7, Z_PRIORITY_PADMGR, &sIrqMgrStack);

    OoT_AudioMgr_Unlock(&gAudioMgr);

    OoT_StackCheck_Init(&OoT_sGraphStackInfo, sGraphStack, sGraphStack + sizeof(sGraphStack), 0, 0x100, "graph");
    osCreateThread(&sGraphThread, 4, OoT_Graph_ThreadEntry, arg, sGraphStack + sizeof(sGraphStack), Z_PRIORITY_GRAPH);
    osStartThread(&sGraphThread);
    osSetThreadPri(0, Z_PRIORITY_SCHED);

    OoT_Graph_ThreadEntry(0);

    while (true) {
        msg = NULL;
        osRecvMesg(&irqMgrMsgQ, (OSMesg*)&msg, OS_MESG_BLOCK);
        if (msg == NULL) {
            break;
        }
        if (*msg == OS_SC_PRE_NMI_MSG) {
            osSyncPrintf("main.c: リセットされたみたいだよ\n"); // "Looks like it's been reset"
            PreNmiBuff_SetReset(gAppNmiBufferPtr);
        }
    }

    osSyncPrintf("mainproc 後始末\n"); // "Cleanup"
    osDestroyThread(&sGraphThread);
    func_800FBFD8();
    osSyncPrintf("mainproc 実行終了\n"); // "End of execution"
}
