#ifdef _WIN32
#include <Windows.h>
#include <stdio.h>
#include <locale.h>
#endif

#include "audiomgr.h"
#include "fault.h"
#include "idle.h"
#include "irqmgr.h"
#include "padmgr.h"
#include "scheduler.h"
#include "CIC6105.h"
#include "stack.h"
#include "stackcheck.h"
#include "BenPort.h"
#include <libultraship/bridge/crashhandlerbridge.h>

// Variables are put before most headers as a hacky way to bypass bss reordering
OSMesgQueue sSerialEventQueue;
OSMesg sSerialMsgBuf[1];
uintptr_t MM_gSegments[NUM_SEGMENTS];
SchedContext MM_gSchedContext;
IrqMgrClient sIrqClient;
OSMesgQueue sIrqMgrMsgQueue;
OSMesg sIrqMgrMsgBuf[60];
OSThread gGraphThread;
STACK(sGraphStack, 0x1800);
STACK(sSchedStack, 0x600);
STACK(sAudioStack, 0x800);
STACK(sPadMgrStack, 0x500);
StackEntry MM_sGraphStackInfo;
StackEntry MM_sSchedStackInfo;
StackEntry MM_sAudioStackInfo;
StackEntry MM_sPadMgrStackInfo;
AudioMgr sAudioMgr;
static s32 sBssPad;
PadMgr MM_gPadMgr;

#include "main.h"
#include "buffers.h"
#include "global.h"
#include "system_heap.h"
#include "z64thread.h"

s32 MM_gScreenWidth = SCREEN_WIDTH;
s32 MM_gScreenHeight = SCREEN_HEIGHT;
size_t MM_gSystemHeapSize = 0;

void InitOTR();
void MM_Heaps_Free(void);
#ifdef __GNUC__
#define MM_SDL_main main
#endif

void MM_SDL_main(int argc, char** argv /* void* arg*/) {
    intptr_t fb;
    intptr_t sysHeap;
    s32 exit;
    s16* msg;

// Attach console for windows so we can conditionally display it when running the extractor
#ifdef _WIN32
    AllocConsole();
    (void)freopen("CONIN$", "r", stdin);
    (void)freopen("CONOUT$", "w", stdout);
    (void)freopen("CONOUT$", "w", stderr);
#ifndef _DEBUG
    ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif
    // Allow non-ascii characters for Windows
    setlocale(LC_ALL, ".UTF8");
#endif // _WIN32

    InitOTR();
    CrashHandlerRegisterCallback(CrashHandler_PrintExt);
    MM_Heaps_Alloc();

    MM_gScreenWidth = SCREEN_WIDTH;
    MM_gScreenHeight = SCREEN_HEIGHT;

    Nmi_Init();
    MM_Fault_Init();
    Check_RegionIsSupported();
    Check_ExpansionPak();
    sysHeap = MM_gSystemHeap;
    // fb = FRAMEBUFFERS_START_ADDR;
    // MM_gSystemHeapSize = fb - sysHeap;
    MM_SystemHeap_Init(sysHeap, SYSTEM_HEAP_SIZE);

    Regs_Init();

    R_ENABLE_ARENA_DBG = 0;

    osCreateMesgQueue(&sSerialEventQueue, sSerialMsgBuf, ARRAY_COUNT(sSerialMsgBuf));
    osSetEventMesg(OS_EVENT_SI, &sSerialEventQueue, OS_MESG_PTR(NULL));

    osCreateMesgQueue(&sIrqMgrMsgQueue, sIrqMgrMsgBuf, ARRAY_COUNT(sIrqMgrMsgBuf));
    MM_PadMgr_Init(&sSerialEventQueue, &MM_gIrqMgr, Z_THREAD_ID_PADMGR, Z_PRIORITY_PADMGR, STACK_TOP(sPadMgrStack));

    MM_AudioMgr_Init(&sAudioMgr, STACK_TOP(sAudioStack), Z_PRIORITY_AUDIOMGR, Z_THREAD_ID_AUDIOMGR, &MM_gSchedContext,
                  &MM_gIrqMgr);
#if 0
    MM_StackCheck_Init(&MM_sSchedStackInfo, sSchedStack, STACK_TOP(sSchedStack), 0, 0x100, "sched");
    MM_Sched_Init(&MM_gSchedContext, STACK_TOP(sSchedStack), Z_PRIORITY_SCHED, gViConfigModeType, 1, &MM_gIrqMgr);

    CIC6105_AddRomInfoFaultPage();

    MM_IrqMgr_AddClient(&MM_gIrqMgr, &sIrqClient, &sIrqMgrMsgQueue);

    MM_StackCheck_Init(&MM_sAudioStackInfo, sAudioStack, STACK_TOP(sAudioStack), 0, 0x100, "audio");
    MM_AudioMgr_Init(&sAudioMgr, STACK_TOP(sAudioStack), Z_PRIORITY_AUDIOMGR, Z_THREAD_ID_AUDIOMGR, &MM_gSchedContext,
                  &MM_gIrqMgr);

    MM_StackCheck_Init(&MM_sPadMgrStackInfo, sPadMgrStack, STACK_TOP(sPadMgrStack), 0, 0x100, "padmgr");

    MM_AudioMgr_Unlock(&sAudioMgr);
    MM_StackCheck_Init(&MM_sGraphStackInfo, sGraphStack, STACK_TOP(sGraphStack), 0, 0x100, "graph");
    osCreateThread(&gGraphThread, Z_THREAD_ID_GRAPH, MM_Graph_ThreadEntry, NULL, STACK_TOP(sGraphStack), Z_PRIORITY_GRAPH);
    osStartThread(&gGraphThread);
#endif

    MM_Graph_ThreadEntry(0);

    exit = false;

    while (!exit) {
        msg = NULL;
        osRecvMesg(&sIrqMgrMsgQueue, (OSMesg*)&msg, OS_MESG_BLOCK);
        if (msg == NULL) {
            break;
        }

        switch (*msg) {
            case OS_SC_PRE_NMI_MSG:
                Nmi_SetPrenmiStart();
                break;

            case OS_SC_NMI_MSG:
                exit = true;
                break;
        }
    }

    MM_IrqMgr_RemoveClient(&MM_gIrqMgr, &sIrqClient);
    osDestroyThread(&gGraphThread);

    DeinitOTR();

#ifdef _WIN32
    FreeConsole();
#endif
    MM_Heaps_Free();
}
