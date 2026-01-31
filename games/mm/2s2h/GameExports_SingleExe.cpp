/**
 * Game Entry Points for MM (2Ship2Harkinian) - Single Executable Build
 *
 * This file provides the MM_Game_* functions expected by the redship
 * main.cpp for single-executable builds.
 *
 * In single-exe mode, the libultraship context is shared with OoT.
 * MM skips InitOTR() since OoT already initialized the shared context.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>
#include <cstring>
#include <cassert>

#include <ship/Context.h>
#include <ship/window/Window.h>

#include "game_lifecycle.h"

#include "game_lifecycle.h"

// From main.c headers
extern "C" {
#include "audiomgr.h"
#include "fault.h"
#include "idle.h"
#include "irqmgr.h"
#include "padmgr.h"
#include "scheduler.h"
#include "stack.h"
#include "system_heap.h"
#include "z64thread.h"
#include "global.h"
}

// External declarations from main.c
extern "C" {
    void MM_Heaps_Alloc(void);
    void MM_Heaps_Free(void);
    void MM_Graph_ThreadEntry(void* arg);

    // Additional init functions from main.c
    void Nmi_Init(void);
    void MM_Fault_Init(void);
    void Check_RegionIsSupported(void);
    void Check_ExpansionPak(void);
    void Regs_Init(void);

    // Audio reset for cross-game switch (issue #157)
    extern s32 gAudioCtxInitalized;
    void AudioThread_InitMesgQueues(void);

    // Globals from main.c
    extern s32 MM_gScreenWidth;
    extern s32 MM_gScreenHeight;
    extern uintptr_t MM_gSystemHeap;
    extern OSMesgQueue sSerialEventQueue;
    extern OSMesg sSerialMsgBuf[1];
    extern OSMesgQueue sIrqMgrMsgQueue;
    extern OSMesg sIrqMgrMsgBuf[60];
    extern SchedContext MM_gSchedContext;
    extern AudioMgr sAudioMgr;
    extern PadMgr MM_gPadMgr;
    extern IrqMgr MM_gIrqMgr;

}

// Track if MM has been initialized (for re-entry after game switch)
static bool sMMInitialized = false;

/**
 * Verify the shared Ship::Context from OoT is ready for MM's graph thread.
 * MM_Graph_ThreadEntry calls WindowIsRunning(), GfxDebuggerIsDebugging(),
 * WindowGetWidth/Height/AspectRatio() every frame — all route through
 * Ship::Context::GetInstance(). OoT initializes this singleton; MM reuses it.
 */
static bool VerifySharedContext(void) {
    auto ctx = Ship::Context::GetInstance();
    if (!ctx) {
        fprintf(stderr, "[MM] ERROR: Ship::Context is null — OoT must init first\n");
        return false;
    }
    if (!ctx->GetWindow()) {
        fprintf(stderr, "[MM] ERROR: Ship::Context has no Window\n");
        return false;
    }
    if (!ctx->GetGfxDebugger()) {
        fprintf(stderr, "[MM] WARNING: No GfxDebugger — debug calls may crash\n");
    }
    fprintf(stderr, "[MM] Shared Ship::Context verified OK\n");
    return true;
}

extern "C" {

int MM_Game_Init(int argc, char** argv) {
    fprintf(stderr, "[MM] Game_Init called, argc=%d\n", argc);
    fflush(stderr);

    // In single-exe mode, skip InitOTR() - OoT already initialized libultraship.
    // Verify the shared context is ready for MM bridge functions (issue #158).
    if (!VerifySharedContext()) {
        fprintf(stderr, "[MM] FATAL: Cannot start without Ship::Context\n");
        return -1;
    }

    fprintf(stderr, "[MM] Allocating heaps...\n");
    fflush(stderr);
    MM_Heaps_Alloc();

    // Set screen dimensions
    MM_gScreenWidth = SCREEN_WIDTH;
    MM_gScreenHeight = SCREEN_HEIGHT;

    fprintf(stderr, "[MM] Calling Nmi_Init()...\n");
    fflush(stderr);
    Nmi_Init();

    fprintf(stderr, "[MM] Calling MM_Fault_Init()...\n");
    fflush(stderr);
    MM_Fault_Init();

    fprintf(stderr, "[MM] Calling Check_RegionIsSupported()...\n");
    fflush(stderr);
    Check_RegionIsSupported();

    fprintf(stderr, "[MM] Calling Check_ExpansionPak()...\n");
    fflush(stderr);
    Check_ExpansionPak();

    fprintf(stderr, "[MM] Calling MM_SystemHeap_Init()...\n");
    fflush(stderr);
    MM_SystemHeap_Init((void*)MM_gSystemHeap, SYSTEM_HEAP_SIZE);

    fprintf(stderr, "[MM] Calling Regs_Init()...\n");
    fflush(stderr);
    Regs_Init();

    // Set up message queues
    fprintf(stderr, "[MM] Setting up message queues...\n");
    fflush(stderr);
    MM_osCreateMesgQueue(&sSerialEventQueue, sSerialMsgBuf, ARRAY_COUNT(sSerialMsgBuf));
    MM_osSetEventMesg(OS_EVENT_SI, &sSerialEventQueue, OS_MESG_PTR(NULL));
    MM_osCreateMesgQueue(&sIrqMgrMsgQueue, sIrqMgrMsgBuf, ARRAY_COUNT(sIrqMgrMsgBuf));

    // Initialize PadMgr and AudioMgr
    fprintf(stderr, "[MM] Calling MM_PadMgr_Init()...\n");
    fflush(stderr);
    MM_PadMgr_Init(&sSerialEventQueue, &MM_gIrqMgr, Z_THREAD_ID_PADMGR, Z_PRIORITY_PADMGR, NULL);

    // Ensure audio message queues are initialized before AudioMgr uses them.
    // On re-entry after a game switch, gAudioCtx may have stale queue pointers
    // from a previous session. Explicitly reinitialize them (issue #157).
    fprintf(stderr, "[MM] Reinitializing audio message queues...\n");
    fflush(stderr);
    gAudioCtxInitalized = false;
    AudioThread_InitMesgQueues();

    fprintf(stderr, "[MM] Calling MM_AudioMgr_Init()...\n");
    fflush(stderr);
    MM_AudioMgr_Init(&sAudioMgr, NULL, Z_PRIORITY_AUDIOMGR, Z_THREAD_ID_AUDIOMGR, &MM_gSchedContext, &MM_gIrqMgr);

    sMMInitialized = true;

    fprintf(stderr, "[MM] Game_Init complete\n");
    fflush(stderr);
    return 0;
}

void MM_Game_Run(void) {
    fprintf(stderr, "[MM] Game_Run called, entering MM_Graph_ThreadEntry()\n");
    fflush(stderr);
    // Assert context still valid before graph loop (issue #158)
    assert(Ship::Context::GetInstance() != nullptr &&
           "Ship::Context must be valid when MM graph thread starts");
    assert(Ship::Context::GetInstance()->GetWindow() != nullptr &&
           "Window must be valid when MM graph thread starts");

    // Run the main game loop
    MM_Graph_ThreadEntry(nullptr);
    fprintf(stderr, "[MM] MM_Graph_ThreadEntry() returned\n");
    fflush(stderr);
}

void MM_Game_Suspend(void) {
    fprintf(stderr, "[MM] Game_Suspend called\n");
    fflush(stderr);
    // TODO: Stop MM audio playback
}

void MM_Game_Resume(void) {
    fprintf(stderr, "[MM] Game_Resume called\n");
    fflush(stderr);
    // TODO: Restore MM audio, reload MM-specific resources if needed
}

void MM_Game_Shutdown(void) {
    fprintf(stderr, "[MM] Game_Shutdown called\n");
    fflush(stderr);
    // Don't call DeinitOTR() - the shared context stays alive
    // Reset audio state so re-init works after game switch (issue #157)
    gAudioCtxInitalized = false;
    MM_Heaps_Free();
    sMMInitialized = false;
    fprintf(stderr, "[MM] Game_Shutdown complete\n");
    fflush(stderr);
}

const char* MM_Game_GetName(void) {
    return "Majora's Mask";
}

const char* MM_Game_GetId(void) {
    return "mm";
}

} // extern "C"

// ============================================================================
// GameOps registration
// ============================================================================

static GameOps sMMOps = {
    "mm",
    "Majora's Mask",
    MM_Game_Init,
    MM_Game_Run,
    MM_Game_Suspend,
    MM_Game_Resume,
    MM_Game_Shutdown
};

extern "C" GameOps* MM_GetGameOps(void) {
    return &sMMOps;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
