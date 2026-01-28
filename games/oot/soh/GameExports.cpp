/**
 * Game Export Interface for OoT (Ship of Harkinian)
 *
 * This file implements the standard game interface that allows the combo
 * launcher to load and run OoT as a shared library.
 */

#include "combo/GameExports.h"
#include "combo/ComboContextBridge.h"
#include "combo/CrossGameEntrance.h"
#include "combo/FrozenState.h"
#include "OTRGlobals.h"
#include "z64save.h"
#include <libultraship/bridge.h>
#include <cstring>
#include "soh/CrashHandlerExt.h"
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/controller/controldevice/controller/mapping/keyboard/KeyboardScancodes.h>

// External declarations from main.c and other C sources
extern "C" {
    void GameConsole_Init(void);
    void InitOTR(int argc, char* argv[]);
    void DeinitOTR(void);
    void OoT_Heaps_Alloc(void);
    void OoT_Heaps_Free(void);
    void Main(void* arg);
    void BootCommands_Init(void);
    void OTRAudio_Suspend(void);
    void OTRAudio_Resume(void);

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

    // Initialize OoT subsystems (matching what main() does)
    GameConsole_Init();
    InitOTR(argc, argv);
    CrashHandlerRegisterCallback(CrashHandler_PrintSohData);
    BootCommands_Init();
    OoT_Heaps_Alloc();

    return 0;
}

GAME_EXPORT void Game_Run(void) {
    // Run the main game loop
    Main(nullptr);
}

GAME_EXPORT void Game_Shutdown(void) {
    fprintf(stderr, "[OOT SHUTDOWN DEBUG] Game_Shutdown called\n");
    fflush(stderr);
    fprintf(stderr, "[OOT SHUTDOWN DEBUG] Calling DeinitOTR()...\n");
    fflush(stderr);
    DeinitOTR();
    fprintf(stderr, "[OOT SHUTDOWN DEBUG] DeinitOTR() complete\n");
    fflush(stderr);
    fprintf(stderr, "[OOT SHUTDOWN DEBUG] Calling OoT_Heaps_Free()...\n");
    fflush(stderr);
    OoT_Heaps_Free();
    fprintf(stderr, "[OOT SHUTDOWN DEBUG] Game_Shutdown complete\n");
    fflush(stderr);
}

GAME_EXPORT void Game_Pause(void) {
    sGamePaused = true;

    // Stop the OoT audio thread to prevent audio bleeding into the other game
    OTRAudio_Suspend();

    fprintf(stderr, "[OOT] Game_Pause: audio suspended\n");
    fflush(stderr);
}

GAME_EXPORT void Game_Resume(void) {
    // Restart the OoT audio thread
    OTRAudio_Resume();

    sGamePaused = false;

    fprintf(stderr, "[OOT] Game_Resume: audio resumed\n");
    fflush(stderr);
}

GAME_EXPORT void* Game_SaveState(size_t* outSize) {
    // Freeze state to the combo layer's FrozenStateManager
    // The returnEntrance will be set separately by the entrance hook
    Combo_FreezeState("oot", 0, &gSaveContext, sizeof(SaveContext));

    // The FrozenStateManager owns the buffer, so we don't return one
    // Callers should use Combo_GetOoTSaveContext() for read access
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
    if (Combo_RestoreState("oot", &gSaveContext, sizeof(SaveContext))) {
        return 0;
    }

    return -1;  // No state to restore
}

GAME_EXPORT const char* Game_GetName(void) {
    return "Ocarina of Time";
}

GAME_EXPORT const char* Game_GetId(void) {
    return "oot";
}

// ============================================================================
// Hot-swap support - F10 detection and cross-game entrance switching
// ============================================================================

// Track last F10 state to detect edge (press, not hold)
static bool sLastF10State = false;

/**
 * Check if F10 was pressed and request game switch if so.
 * Also checks for pending cross-game entrance switches.
 * Called from the game loop (OoT_Graph_ThreadEntry) each frame.
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
// Cross-game entrance hook - called from Entrance_OverrideNextIndex
// ============================================================================

/**
 * Check if an entrance is a cross-game entrance and handle the switch.
 * Called from the entrance override system when transitioning.
 *
 * @param entranceIndex The entrance being taken
 * @return The entrance to use (original if not cross-game)
 */
extern "C" uint16_t Combo_CheckEntranceSwitch(uint16_t entranceIndex) {
    fprintf(stderr, "[COMBO DEBUG] CheckEntranceSwitch called with entrance 0x%04X\n", entranceIndex);
    fflush(stderr);

    // Check if this entrance triggers a cross-game switch
    uint16_t result = Combo_CheckCrossGameEntrance("oot", entranceIndex);
    fprintf(stderr, "[COMBO DEBUG] CheckCrossGameEntrance returned 0x%04X\n", result);
    fflush(stderr);

    // If a cross-game switch was triggered, freeze our state
    if (Combo_IsCrossGameSwitch()) {
        fprintf(stderr, "[COMBO DEBUG] Cross-game switch detected!\n");
        fflush(stderr);

        // Get the return entrance from the pending switch
        uint16_t returnEntrance = Combo_GetSwitchReturnEntrance();
        fprintf(stderr, "[COMBO DEBUG] Return entrance: 0x%04X\n", returnEntrance);
        fflush(stderr);

        // Freeze OoT state with the return entrance
        fprintf(stderr, "[COMBO DEBUG] About to freeze state, gSaveContext=%p, size=%zu\n",
                (void*)&gSaveContext, sizeof(SaveContext));
        fflush(stderr);
        Combo_FreezeState("oot", returnEntrance, &gSaveContext, sizeof(SaveContext));
        fprintf(stderr, "[COMBO DEBUG] State frozen successfully\n");
        fflush(stderr);

        // Signal that we're ready to switch
        Combo_SignalReadyToSwitch();
        fprintf(stderr, "[COMBO DEBUG] Ready to switch signaled\n");
        fflush(stderr);
    }

    return result;
}
