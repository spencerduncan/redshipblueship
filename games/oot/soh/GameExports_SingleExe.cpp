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
#include "integration_test_hooks.h"
#include "context.h"
#include "entrance.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

// External declarations from main.c and other C sources
extern "C" {
    void GameConsole_Init(void);
    void InitOTR(int argc, char* argv[]);
    void DeinitOTR(void);
    void OoT_Heaps_Alloc(void);
    void OoT_Heaps_Free(void);
    void Main(void* arg);
    void BootCommands_Init(void);

    // Audio cleanup for suspend (issue #160)
    void OoT_Audio_PreNMI(void);
    extern s32 gAudioContextInitalized;
    void Audio_InitMesgQueues(void);
}

// Game state
static int sArgc = 0;
static char** sArgv = nullptr;

// ============================================================================
// Integration Test Hooks
// ============================================================================

/**
 * Register integration test hooks for OoT.
 * Called after OoT is initialized when integration test mode is active.
 */
static void OoT_RegisterIntegrationTestHooks(void) {
    if (!IntegrationTest_IsActive()) {
        return;
    }

    IntegrationTestMode mode = IntegrationTest_GetMode();

    // Only register hooks for OoT boot test
    if (mode == INT_TEST_BOOT_OOT) {
        fprintf(stderr, "[OoT] Registering integration test hooks for boot detection\n");
        fflush(stderr);

        // Register hook for title screen init
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnZTitleInit>(
            [](void* gameState) {
                fprintf(stderr, "[OoT-INT-TEST] OnZTitleInit hook fired!\n");
                fflush(stderr);
                IntegrationTest_SignalBootComplete(GAME_OOT, "title screen init");
            }
        );

        // Register hook for file select presentation
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPresentFileSelect>(
            []() {
                fprintf(stderr, "[OoT-INT-TEST] OnPresentFileSelect hook fired!\n");
                fflush(stderr);
                IntegrationTest_SignalBootComplete(GAME_OOT, "file select presented");
            }
        );

        fprintf(stderr, "[OoT] Integration test hooks registered\n");
        fflush(stderr);
    }
}

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

    // Register integration test hooks if in integration test mode
    OoT_RegisterIntegrationTestHooks();

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
 * Suspend OoT for a game switch (issue #160).
 * Stops audio to prevent interference with MM, keeps libultraship context alive.
 */
void OoT_Game_Suspend(void) {
    fprintf(stderr, "[OoT] Game_Suspend called\n");
    fflush(stderr);

    // Stop OoT audio playback to prevent interference with MM (issue #160).
    // OoT_Audio_PreNMI triggers the audio reset path which stops all sequences
    // and puts the audio system into a quiescent state.
    //
    // Note: No race with the audio thread here — in the SoH port, the audio
    // thread is not a real OS thread (osCreateThread is commented out in
    // audioMgr.c). Audio is processed synchronously from the game loop, which
    // has already returned from Main() before suspend is called.
    fprintf(stderr, "[OoT] Stopping audio via PreNMI path...\n");
    fflush(stderr);
    OoT_Audio_PreNMI();

    // Mark audio as uninitialized so re-init works on resume
    gAudioContextInitalized = false;

    fprintf(stderr, "[OoT] Game_Suspend complete\n");
    fflush(stderr);
}

/**
 * Resume OoT after being suspended for a game switch (issue #160, #170).
 * - Restores frozen OoT SaveContext so gameplay state survives the MM
 *   round-trip (MM scribbles over the unified gSaveContext storage while
 *   it is active — see src/common/unified_save.c).
 * - Reinitializes audio message queues for clean state.
 */
void OoT_Game_Resume(void) {
    fprintf(stderr, "[OoT] Game_Resume called\n");
    fflush(stderr);

    // Restore the frozen OoT SaveContext captured before we left for MM (#170).
    // Only restores when a frozen state exists — first boot of OoT skips this.
    if (Context_HasFrozenState(GAME_OOT)) {
        fprintf(stderr, "[OoT] Restoring frozen SaveContext on resume\n");
        fflush(stderr);
        Context_RestoreState(GAME_OOT, &gSaveContext, sizeof(gSaveContext));

        // Prefer an explicit startup entrance (set by main.cpp for this
        // switch); fall back to the return entrance recorded at freeze time.
        uint16_t startup = Combo_GetStartupEntrance();
        uint16_t returnEntrance = Context_GetFrozenReturnEntrance(GAME_OOT);
        uint16_t targetEntrance = startup != 0 ? startup : returnEntrance;
        if (targetEntrance != 0) {
            gSaveContext.entranceIndex = targetEntrance;
            fprintf(stderr, "[OoT] Resume entrance: 0x%04X (startup=%u)\n",
                    targetEntrance, startup != 0);
        }
    }

    // Reinitialize audio message queues for clean state (issue #160).
    // The audio context's queue pointers may be stale after suspend.
    fprintf(stderr, "[OoT] Reinitializing audio message queues...\n");
    fflush(stderr);
    Audio_InitMesgQueues();

    fprintf(stderr, "[OoT] Game_Resume complete\n");
    fflush(stderr);
}

/**
 * Full shutdown (final exit, no game switch coming).
 */
void OoT_Game_Shutdown(void) {
    fprintf(stderr, "[OoT] Game_Shutdown called\n");
    fflush(stderr);
    gAudioContextInitalized = false;
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
 *
 * Dispatches against the currently active game so both OoT→MM and MM→OoT
 * work (issue #170). In single-exe mode this symbol is shared between both
 * games' translation units; the MM code path calls into this exact function
 * via the `Combo_CheckEntranceSwitch` extern, so we must freeze the right
 * side's SaveContext depending on who's running.
 *
 * Called from OoT's z_play.c / randomizer_entrance.c and from MM's
 * z_play.c / z_player.c.
 */
extern "C" uint16_t Combo_CheckEntranceSwitch(uint16_t entranceIndex) {
    // If a cross-game switch is already queued (e.g. re-entrant call from a
    // subsequent transition frame), skip to avoid re-freezing with mutated
    // state (death sequences, mid-transition saves) — the main loop will
    // process the existing pending switch on the next iteration.
    if (Combo_IsCrossGameSwitch()) {
        return entranceIndex;
    }

    // Resolve which game is running so we pick the correct entrance table
    // and freeze the correct SaveContext interpretation. Default to OoT when
    // the current-game tracker hasn't been populated yet (early boot).
    GameId currentGame = Context_GetCurrentGame();
    const char* gameId = (currentGame == GAME_MM) ? "mm" : "oot";

    uint16_t result = Combo_CheckCrossGameEntrance(gameId, entranceIndex);

    if (Combo_IsCrossGameSwitch()) {
        fprintf(stderr, "[COMBO] Cross-game switch (%s)! entrance=0x%04X\n",
                gameId, entranceIndex);

        uint16_t returnEntrance = Combo_GetSwitchReturnEntrance();
        // sizeof(gSaveContext) in this TU is OoT's SaveContext layout, but the
        // underlying unified storage is identical regardless of caller and
        // Context_FreezeState clamps to the per-game N64 size anyway.
        Combo_FreezeState(gameId, returnEntrance, &gSaveContext, sizeof(gSaveContext));
        Combo_SignalReadyToSwitch();
    }

    return result;
}

#endif /* RSBS_SINGLE_EXECUTABLE */

