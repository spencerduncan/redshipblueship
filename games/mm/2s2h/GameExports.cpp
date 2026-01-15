/**
 * Game Export Interface for MM (2Ship2Harkinian)
 *
 * This file implements the standard game interface that allows the combo
 * launcher to load and run MM as a shared library.
 */

#include "combo/GameExports.h"
#include "combo/ComboContextBridge.h"
#include "combo/FrozenState.h"
#include "BenPort.h"
#include <libultraship/bridge/crashhandlerbridge.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/controller/controldevice/controller/mapping/keyboard/KeyboardScancodes.h>
#include <cstring>

// From main.c - need these includes for initialization
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
#include "z64save.h"
}

// External declarations from main.c
extern "C" {
    void InitOTR(void);
    void DeinitOTR(void);
    void Heaps_Alloc(void);
    void Heaps_Free(void);
    void CrashHandler_PrintExt(char* buffer, size_t* pos);
    void Graph_ThreadEntry(void* arg);

    // Additional init functions from main.c
    void Nmi_Init(void);
    void Fault_Init(void);
    void Check_RegionIsSupported(void);
    void Check_ExpansionPak(void);
    void Regs_Init(void);

    // Globals from main.c that need to be referenced
    extern s32 gScreenWidth;
    extern s32 gScreenHeight;
    extern uintptr_t gSystemHeap;
    extern OSMesgQueue sSerialEventQueue;
    extern OSMesg sSerialMsgBuf[1];
    extern OSMesgQueue sIrqMgrMsgQueue;
    extern OSMesg sIrqMgrMsgBuf[60];
    extern SchedContext gSchedContext;
    extern AudioMgr sAudioMgr;
    extern PadMgr gPadMgr;
    extern IrqMgr gIrqMgr;

    // Save context for state preservation
    extern SaveContext gSaveContext;
}

// Game state for pause/resume
static bool sGamePaused = false;
static int sArgc = 0;
static char** sArgv = nullptr;

GAME_EXPORT int Game_Init(int argc, char** argv) {
    // Store args for potential restart
    sArgc = argc;
    sArgv = argv;

    // Initialize MM subsystems (matching what SDL_main() does)
    InitOTR();
    CrashHandlerRegisterCallback(CrashHandler_PrintExt);
    Heaps_Alloc();

    // Additional initialization from main.c that happens before Graph_ThreadEntry
    gScreenWidth = SCREEN_WIDTH;
    gScreenHeight = SCREEN_HEIGHT;

    Nmi_Init();
    Fault_Init();
    Check_RegionIsSupported();
    Check_ExpansionPak();

    SystemHeap_Init((void*)gSystemHeap, SYSTEM_HEAP_SIZE);
    Regs_Init();

    // Set up message queues
    osCreateMesgQueue(&sSerialEventQueue, sSerialMsgBuf, ARRAY_COUNT(sSerialMsgBuf));
    osSetEventMesg(OS_EVENT_SI, &sSerialEventQueue, OS_MESG_PTR(NULL));
    osCreateMesgQueue(&sIrqMgrMsgQueue, sIrqMgrMsgBuf, ARRAY_COUNT(sIrqMgrMsgBuf));

    // Initialize PadMgr and AudioMgr - these are required for Graph_ThreadEntry
    // Note: Stack addresses are handled differently in shared library context
    PadMgr_Init(&sSerialEventQueue, &gIrqMgr, Z_THREAD_ID_PADMGR, Z_PRIORITY_PADMGR, NULL);
    AudioMgr_Init(&sAudioMgr, NULL, Z_PRIORITY_AUDIOMGR, Z_THREAD_ID_AUDIOMGR, &gSchedContext, &gIrqMgr);

    return 0;
}

GAME_EXPORT void Game_Run(void) {
    // Run the main game loop
    Graph_ThreadEntry(nullptr);
}

GAME_EXPORT void Game_Shutdown(void) {
    DeinitOTR();
    Heaps_Free();
}

GAME_EXPORT void Game_Pause(void) {
    // TODO: Implement pause logic
    // For now, this is a placeholder. Full implementation requires
    // modifying the game loop to check sGamePaused flag.
    sGamePaused = true;
}

GAME_EXPORT void Game_Resume(void) {
    sGamePaused = false;
}

GAME_EXPORT void* Game_SaveState(size_t* outSize) {
    // Freeze state to the combo layer's FrozenStateManager
    // The returnEntrance will be set separately by the entrance hook
    Combo_FreezeState("mm", 0, &gSaveContext, sizeof(SaveContext));

    // The FrozenStateManager owns the buffer, so we don't return one
    // Callers should use Combo_GetMMSaveContext() for read access
    *outSize = sizeof(SaveContext);
    return nullptr;
}

GAME_EXPORT int Game_LoadState(void* data, size_t size) {
    // If data is provided, use it directly (external state)
    if (data != nullptr && size == sizeof(SaveContext)) {
        memcpy(&gSaveContext, data, sizeof(SaveContext));
        return 0;
    }

    // Otherwise, try to restore from FrozenStateManager
    if (Combo_RestoreState("mm", &gSaveContext, sizeof(SaveContext))) {
        return 0;
    }

    return -1;  // No state to restore
}

GAME_EXPORT const char* Game_GetName(void) {
    return "Majora's Mask";
}

GAME_EXPORT const char* Game_GetId(void) {
    return "mm";
}

// ============================================================================
// Hot-swap support - F10 detection
// ============================================================================

// Track last F10 state to detect edge (press, not hold)
static bool sLastF10State = false;

/**
 * Check if F10 was pressed and request game switch if so.
 * Called from the game loop (Graph_ThreadEntry) each frame.
 * Returns true if a switch was requested (game should exit its loop).
 */
extern "C" bool Combo_CheckHotSwap(void) {
    auto context = Ship::Context::GetInstance();
    if (!context) {
        return Combo_IsGameSwitchRequested();
    }

    auto window = context->GetWindow();
    if (!window) {
        return Combo_IsGameSwitchRequested();
    }

    int32_t scancode = window->GetLastScancode();
    bool f10Pressed = (scancode == Ship::LUS_KB_F10);

    // Detect rising edge (just pressed, not held)
    if (f10Pressed && !sLastF10State) {
        Combo_RequestGameSwitch();
    }
    sLastF10State = f10Pressed;

    return Combo_IsGameSwitchRequested();
}
