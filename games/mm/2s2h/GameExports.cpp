/**
 * Game Export Interface for MM (2Ship2Harkinian)
 *
 * This file implements the standard game interface that allows the combo
 * launcher to load and run MM as a shared library.
 */

#include "combo/GameExports.h"
#include "combo/ComboContextBridge.h"
#include "combo/CrossGameEntrance.h"
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
    void MM_Heaps_Alloc(void);
    void MM_Heaps_Free(void);
    void CrashHandler_PrintExt(char* buffer, size_t* pos);
    void MM_Graph_ThreadEntry(void* arg);

    // Additional init functions from main.c
    void Nmi_Init(void);
    void MM_Fault_Init(void);
    void Check_RegionIsSupported(void);
    void Check_ExpansionPak(void);
    void Regs_Init(void);

    // Globals from main.c that need to be referenced
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

    // Save context for state preservation
    extern SaveContext gSaveContext;
}

// Game state for pause/resume
static bool sGamePaused = false;
static int sArgc = 0;
static char** sArgv = nullptr;

GAME_EXPORT int Game_Init(int argc, char** argv) {
    fprintf(stderr, "[MM INIT DEBUG] Game_Init called, argc=%d\n", argc);
    fflush(stderr);

    // Store args for potential restart
    sArgc = argc;
    sArgv = argv;

    // Initialize MM subsystems (matching what MM_SDL_main() does)
    fprintf(stderr, "[MM INIT DEBUG] About to call InitOTR()...\n");
    fflush(stderr);
    InitOTR();
    fprintf(stderr, "[MM INIT DEBUG] InitOTR() complete\n");
    fflush(stderr);

    fprintf(stderr, "[MM INIT DEBUG] Registering crash handler...\n");
    fflush(stderr);
    CrashHandlerRegisterCallback(CrashHandler_PrintExt);

    fprintf(stderr, "[MM INIT DEBUG] Allocating heaps...\n");
    fflush(stderr);
    MM_Heaps_Alloc();
    fprintf(stderr, "[MM INIT DEBUG] Heaps allocated\n");
    fflush(stderr);

    // Additional initialization from main.c that happens before MM_Graph_ThreadEntry
    MM_gScreenWidth = SCREEN_WIDTH;
    MM_gScreenHeight = SCREEN_HEIGHT;

    fprintf(stderr, "[MM INIT DEBUG] Calling Nmi_Init()...\n");
    fflush(stderr);
    Nmi_Init();

    fprintf(stderr, "[MM INIT DEBUG] Calling MM_Fault_Init()...\n");
    fflush(stderr);
    MM_Fault_Init();

    fprintf(stderr, "[MM INIT DEBUG] Calling Check_RegionIsSupported()...\n");
    fflush(stderr);
    Check_RegionIsSupported();

    fprintf(stderr, "[MM INIT DEBUG] Calling Check_ExpansionPak()...\n");
    fflush(stderr);
    Check_ExpansionPak();

    fprintf(stderr, "[MM INIT DEBUG] Calling MM_SystemHeap_Init()...\n");
    fflush(stderr);
    MM_SystemHeap_Init((void*)MM_gSystemHeap, SYSTEM_HEAP_SIZE);

    fprintf(stderr, "[MM INIT DEBUG] Calling Regs_Init()...\n");
    fflush(stderr);
    Regs_Init();

    // Set up message queues
    fprintf(stderr, "[MM INIT DEBUG] Setting up message queues...\n");
    fflush(stderr);
    osCreateMesgQueue(&sSerialEventQueue, sSerialMsgBuf, ARRAY_COUNT(sSerialMsgBuf));
    osSetEventMesg(OS_EVENT_SI, &sSerialEventQueue, OS_MESG_PTR(NULL));
    osCreateMesgQueue(&sIrqMgrMsgQueue, sIrqMgrMsgBuf, ARRAY_COUNT(sIrqMgrMsgBuf));

    // Initialize PadMgr and AudioMgr - these are required for MM_Graph_ThreadEntry
    // Note: Stack addresses are handled differently in shared library context
    fprintf(stderr, "[MM INIT DEBUG] Calling MM_PadMgr_Init()...\n");
    fflush(stderr);
    MM_PadMgr_Init(&sSerialEventQueue, &MM_gIrqMgr, Z_THREAD_ID_PADMGR, Z_PRIORITY_PADMGR, NULL);

    fprintf(stderr, "[MM INIT DEBUG] Calling MM_AudioMgr_Init()...\n");
    fflush(stderr);
    MM_AudioMgr_Init(&sAudioMgr, NULL, Z_PRIORITY_AUDIOMGR, Z_THREAD_ID_AUDIOMGR, &MM_gSchedContext, &MM_gIrqMgr);

    fprintf(stderr, "[MM INIT DEBUG] Game_Init complete, returning 0\n");
    fflush(stderr);
    return 0;
}

GAME_EXPORT void Game_Run(void) {
    // Run the main game loop
    MM_Graph_ThreadEntry(nullptr);
}

GAME_EXPORT void Game_Shutdown(void) {
    DeinitOTR();
    MM_Heaps_Free();
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
 * Also checks for pending cross-game entrance switches.
 * Called from the game loop (MM_Graph_ThreadEntry) each frame.
 * Returns true if a switch was requested (game should exit its loop).
 */
extern "C" bool Combo_CheckHotSwap(void) {
    // Check for pending cross-game entrance switch first
    if (Combo_IsCrossGameSwitch()) {
        return true;
    }

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

// ============================================================================
// Cross-game entrance hook - called from MM's entrance handling
// ============================================================================

/**
 * Check if an entrance is a cross-game entrance and handle the switch.
 * Called from the entrance system when transitioning.
 *
 * @param entranceIndex The entrance being taken
 * @return The entrance to use (original if not cross-game)
 */
extern "C" uint16_t Combo_CheckEntranceSwitch(uint16_t entranceIndex) {
    // Check if this entrance triggers a cross-game switch
    uint16_t result = Combo_CheckCrossGameEntrance("mm", entranceIndex);

    // If a cross-game switch was triggered, freeze our state
    if (Combo_IsCrossGameSwitch()) {
        // Get the return entrance from the pending switch
        uint16_t returnEntrance = Combo_GetSwitchReturnEntrance();

        // Freeze MM state with the return entrance
        Combo_FreezeState("mm", returnEntrance, &gSaveContext, sizeof(SaveContext));

        // Signal that we're ready to switch
        Combo_SignalReadyToSwitch();
    }

    return result;
}
