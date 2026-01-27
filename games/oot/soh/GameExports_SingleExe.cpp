/**
 * Game Entry Points for OoT (Ship of Harkinian) - Single Executable Build
 *
 * This file provides the OoT_Game_* functions expected by the redship
 * main.cpp for single-executable builds.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>
#include <cstring>
#include "OTRGlobals.h"
#include "soh/CrashHandlerExt.h"
#include <libultraship/bridge.h>

#include "game_lifecycle.h"

// External declarations from main.c and other C sources
extern "C" {
    void GameConsole_Init(void);
    void InitOTR(int argc, char* argv[]);
    void DeinitOTR(void);
    void OoT_Heaps_Alloc(void);
    void OoT_Heaps_Free(void);
    void Main(void* arg);
    void BootCommands_Init(void);
}

// Game state
static int sArgc = 0;
static char** sArgv = nullptr;

extern "C" {

int OoT_Game_Init(int argc, char** argv) {
    fprintf(stderr, "[OoT] Game_Init called, argc=%d\n", argc);
    fflush(stderr);

    // Store args for potential restart
    sArgc = argc;
    sArgv = argv;

    // Initialize OoT subsystems (matching what main() does)
    fprintf(stderr, "[OoT] Calling GameConsole_Init()...\n");
    fflush(stderr);
    GameConsole_Init();

    fprintf(stderr, "[OoT] Calling InitOTR()...\n");
    fflush(stderr);
    InitOTR(argc, argv);

    fprintf(stderr, "[OoT] Registering crash handler...\n");
    fflush(stderr);
    CrashHandlerRegisterCallback(CrashHandler_PrintSohData);

    fprintf(stderr, "[OoT] Calling BootCommands_Init()...\n");
    fflush(stderr);
    BootCommands_Init();

    fprintf(stderr, "[OoT] Calling OoT_Heaps_Alloc()...\n");
    fflush(stderr);
    OoT_Heaps_Alloc();

    fprintf(stderr, "[OoT] Game_Init complete\n");
    fflush(stderr);
    return 0;
}

void OoT_Game_Run(void) {
    fprintf(stderr, "[OoT] Game_Run called, entering Main()\n");
    fflush(stderr);
    // Run the main game loop
    Main(nullptr);
    fprintf(stderr, "[OoT] Main() returned\n");
    fflush(stderr);
}

/**
 * Suspend OoT for a game switch (preserves libultraship context).
 * MM needs the shared context (spdlog, SDL, resource manager) to remain alive.
 */
void OoT_Game_Suspend(void) {
    fprintf(stderr, "[OoT] Game_Suspend called (keeping libultraship context alive)\n");
    fflush(stderr);
    // Don't call DeinitOTR() — the shared context must survive for MM
    // Don't free heaps — may need them when switching back
}

/**
 * Resume OoT after being suspended for a game switch.
 * Ship::Context is still alive — just need to restore OoT-specific state.
 */
void OoT_Game_Resume(void) {
    fprintf(stderr, "[OoT] Game_Resume called\n");
    fflush(stderr);
    // TODO: Restore audio, reload OoT-specific resources if needed
}

/**
 * Full shutdown (final exit, no game switch coming).
 */
void OoT_Game_Shutdown(void) {
    fprintf(stderr, "[OoT] Game_Shutdown called\n");
    fflush(stderr);
    DeinitOTR();
    OoT_Heaps_Free();
    fprintf(stderr, "[OoT] Game_Shutdown complete\n");
    fflush(stderr);
}

const char* OoT_Game_GetName(void) {
    return "Ocarina of Time";
}

const char* OoT_Game_GetId(void) {
    return "oot";
}

} // extern "C"

// ============================================================================
// GameOps registration
// ============================================================================

static GameOps sOoTOps = {
    "oot",
    "Ocarina of Time",
    OoT_Game_Init,
    OoT_Game_Run,
    OoT_Game_Suspend,
    OoT_Game_Resume,
    OoT_Game_Shutdown
};

extern "C" GameOps* OoT_GetGameOps(void) {
    return &sOoTOps;
}

// ============================================================================
// Cross-game entrance hooks (single-exe mode)
// These were in GameExports.cpp but guarded out by #ifndef RSBS_SINGLE_EXECUTABLE.
// In single-exe mode, these are the REAL implementations called by game code.
// ============================================================================

// Cross-game entrance API (from src/common/)
extern "C" {
    uint16_t Combo_CheckCrossGameEntrance(const char* gameId, uint16_t entrance);
    bool Combo_IsCrossGameSwitch(void);
    uint16_t Combo_GetSwitchReturnEntrance(void);
    void Combo_FreezeState(const char* gameId, uint16_t returnEntrance,
                           const void* saveCtx, size_t saveCtxSize);
    void Combo_SignalReadyToSwitch(void);
    void Combo_RequestGameSwitch(void);
    bool Combo_IsGameSwitchRequested(void);
    void Combo_ClearGameSwitchRequest(void);
}

// OoT's SaveContext (type defined via OTRGlobals.h -> z64save.h)
extern "C" SaveContext gSaveContext;

static bool sLastF10State = false;

/**
 * Check if F10 was pressed and request game switch.
 * Also checks for pending cross-game entrance switches.
 * Called from the OoT game loop (graph.c) each frame.
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

    if (f10Pressed && !sLastF10State) {
        Combo_RequestGameSwitch();
    }
    sLastF10State = f10Pressed;

    return Combo_IsGameSwitchRequested();
}

/**
 * Check if an entrance triggers a cross-game switch.
 * Called from randomizer_entrance.c Entrance_OverrideNextIndex().
 */
extern "C" uint16_t Combo_CheckEntranceSwitch(uint16_t entranceIndex) {
    // Check if this entrance triggers a cross-game switch
    uint16_t result = Combo_CheckCrossGameEntrance("oot", entranceIndex);

    // If a cross-game switch was triggered, freeze our state
    if (Combo_IsCrossGameSwitch()) {
        fprintf(stderr, "[COMBO] Cross-game switch! entrance=0x%04X\n", entranceIndex);

        uint16_t returnEntrance = Combo_GetSwitchReturnEntrance();
        Combo_FreezeState("oot", returnEntrance, &gSaveContext, sizeof(gSaveContext));
        Combo_SignalReadyToSwitch();
    }

    return result;
}

#endif /* RSBS_SINGLE_EXECUTABLE */

