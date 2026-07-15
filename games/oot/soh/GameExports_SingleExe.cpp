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
#include <ship/Context.h>
#include "z64save.h"

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

    // OoT's SaveContext (type from z64save.h).
    // Declared here so OoT_Game_Resume() can restore it on return from MM (#170).
    extern SaveContext gSaveContext;
}

// Archive hot-swap cycle helpers (#263). Defined in
// src/common/tests/test_archive_hotswap.c (compiled into redship_common via
// test_runner.cpp); resolved at final link. Record this OoT arrival, query the
// RSS bound, and read the target arrival count for the cycle-complete check.
extern "C" {
    int ArchiveHotswap_RecordArrival(void);
    int ArchiveHotswap_RssExceeded(void);
    int ArchiveHotswap_TargetArrivals(void);
}

// Game state
static int sArgc = 0;
static char** sArgv = nullptr;

// Integration test hook frame counter (reset each time hooks are registered)
static int sOoTGameStateMainFrameCount = 0;

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
    } else if (mode == INT_TEST_SWITCH_OOT_HMS_TO_MM) {
        // T1 (#260): Boot OoT, programmatically trigger the Happy Mask Shop
        // entrance, assert the cross-game switch resolves to MM Clock Tower
        // Interior. Leg 1 of the test passes when routing is verified here;
        // final pass is signaled from the MM-side hook after MM stabilizes
        // post-switch.
        fprintf(stderr, "[OoT] Registering integration test hooks for HMS->MM switch (T1)\n");
        fflush(stderr);

        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPresentFileSelect>(
            []() {
                fprintf(stderr, "[OoT-INT-TEST] File select reached; triggering HMS entrance 0x%04X\n",
                        OOT_ENTR_HAPPY_MASK_SHOP);
                fflush(stderr);

                // Same call OoT's z_play.c makes when the player walks into the
                // Happy Mask Shop door — minus the freeze, which T3 covers.
                Combo_CheckCrossGameEntrance("oot", OOT_ENTR_HAPPY_MASK_SHOP);

                if (!Combo_IsCrossGameSwitch()) {
                    fprintf(stderr, "[OoT-INT-TEST] FAIL: HMS entrance did not register a cross-game switch\n");
                    fflush(stderr);
                    IntegrationTest_RequestExit();
                    return;
                }

                const char* target = Combo_GetSwitchTargetGameId();
                uint16_t targetEntrance = Combo_GetSwitchTargetEntrance();

                if (!target || strcmp(target, "mm") != 0) {
                    fprintf(stderr, "[OoT-INT-TEST] FAIL: target should be 'mm', got '%s'\n",
                            target ? target : "(null)");
                    fflush(stderr);
                    IntegrationTest_RequestExit();
                    return;
                }

                if (targetEntrance != MM_ENTR_CLOCK_TOWER_INTERIOR_1) {
                    fprintf(stderr,
                            "[OoT-INT-TEST] FAIL: target entrance should be 0x%04X (Clock Tower Interior), got 0x%04X\n",
                            MM_ENTR_CLOCK_TOWER_INTERIOR_1, targetEntrance);
                    fflush(stderr);
                    IntegrationTest_RequestExit();
                    return;
                }

                fprintf(stderr,
                        "[OoT-INT-TEST] PASS leg 1: HMS routes to MM 0x%04X; main loop will run the switch\n",
                        targetEntrance);
                fflush(stderr);
                // Intentionally NOT signaling boot complete here. The main loop
                // will see the pending cross-game switch on Combo_CheckHotSwap
                // and hand off to MM. The MM-side hook signals the final pass.
            }
        );

        fprintf(stderr, "[OoT] HMS->MM switch hooks registered\n");
        fflush(stderr);
    } else if (mode == INT_TEST_SWITCH_MM_CLOCKTOWN_SOUTH_TO_OOT) {
        // T2 (#261) leg 2: OoT has been booted via the cross-game switch from
        // MM's SCT-south trigger. Reaching this hook means MM's freeze + main
        // loop's hand-off + OoT_Game_Init all succeeded end-to-end. Signal pass
        // once OoT's graph thread is running steady frames.
        fprintf(stderr, "[OoT] Registering integration test hooks for SCT-south->OoT switch completion (T2)\n");
        fflush(stderr);

        sOoTGameStateMainFrameCount = 0;

        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>(
            []() {
                sOoTGameStateMainFrameCount++;
                if (sOoTGameStateMainFrameCount >= 10) {
                    fprintf(stderr,
                            "[OoT-INT-TEST] OoT stable after SCT-south->OoT switch (frame %d)\n",
                            sOoTGameStateMainFrameCount);
                    fflush(stderr);
                    IntegrationTest_SignalBootComplete(GAME_OOT, "OoT stable after SCT-south->OoT switch");
                }
            }
        );

        fprintf(stderr, "[OoT] SCT-south->OoT switch hooks registered\n");
        fflush(stderr);
    } else if (mode == INT_TEST_ARCHIVE_HOTSWAP_CYCLE) {
        // T4 (#263): drive >=3 OoT<->MM archive hot-swaps and assert a healthy
        // runtime. OoT boots first, so OoT is arrivals #1 and #3 of the
        // OoT->MM->OoT->MM cycle (4 arrivals == 3 transitions).
        //
        // Registration runs from OoT_Game_Init, which fires only on OoT's FIRST
        // entry — later OoT arrivals come back through OoT_Game_Resume (see
        // GameRunner_SwitchTo: a suspended game is resumed, not re-init'd), so
        // this hook is registered exactly once and the persistent frame counter
        // is NOT reset per arrival. The hook therefore re-arms itself: it fires
        // ~10 stable frames after each (re)entry, then resets the counter so the
        // next arrival reached via resume is detected the same way.
        fprintf(stderr, "[OoT] Registering integration test hooks for archive-hotswap cycle (T4)\n");
        fflush(stderr);

        sOoTGameStateMainFrameCount = 0;

        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>(
            []() {
                // Fire once per arrival, ~10 stable frames after (re)entry, then
                // re-arm for the next OoT arrival (reached via OoT_Game_Resume).
                // (#344) Both games' frame loops fire this shared hook storage;
                // only count OoT's own frames so MM frames can't record a
                // bogus OoT arrival.
                if (Context_GetCurrentGame() != GAME_OOT) {
                    sOoTGameStateMainFrameCount = 0;
                    return;
                }
                sOoTGameStateMainFrameCount++;
                if (sOoTGameStateMainFrameCount < 10) {
                    return;
                }
                sOoTGameStateMainFrameCount = 0;

                int n = ArchiveHotswap_RecordArrival();
                fprintf(stderr, "[OoT-INT-TEST] OoT stable; archive-hotswap arrival #%d of %d\n",
                        n, ArchiveHotswap_TargetArrivals());
                fflush(stderr);

                if (ArchiveHotswap_RssExceeded()) {
                    // Steady-state RSS blew the bound — the #154 per-switch leak
                    // regression. Fail fast: RequestExit does NOT set the pass
                    // flag, so the run returns non-zero. Combo_RequestGameSwitch()
                    // right after unblocks the main loop promptly (known fix).
                    fprintf(stderr, "[OoT-INT-TEST] FAIL: steady-state RSS bound exceeded after %d arrivals\n", n);
                    fflush(stderr);
                    IntegrationTest_RequestExit();
                    Combo_RequestGameSwitch();
                } else if (n >= ArchiveHotswap_TargetArrivals()) {
                    // Target arrivals reached with a healthy runtime — PASS.
                    // SignalBootComplete sets the pass flag and requests the
                    // switch that unblocks the main loop.
                    fprintf(stderr, "[OoT-INT-TEST] archive-hotswap cycle complete after %d arrivals\n", n);
                    fflush(stderr);
                    IntegrationTest_SignalBootComplete(GAME_OOT, "archive-hotswap cycle complete");
                } else {
                    // Keep the cycle going: re-trigger the OoT->MM switch via the
                    // Happy Mask Shop entrance — same call the T1 branch makes.
                    fprintf(stderr, "[OoT-INT-TEST] re-triggering HMS entrance 0x%04X to continue cycle\n",
                            OOT_ENTR_HAPPY_MASK_SHOP);
                    fflush(stderr);
                    Combo_CheckCrossGameEntrance("oot", OOT_ENTR_HAPPY_MASK_SHOP);
                }
            }
        );

        fprintf(stderr, "[OoT] archive-hotswap cycle hooks registered\n");
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
        // Use Combo_HasStartupEntrance rather than (entrance != 0) — entrance
        // 0x0000 is the real id for Kokiri Forest from Deku Tree, so a legit
        // restore to 0 must not be silently dropped. The frozen return
        // entrance is always trustworthy here because we already checked
        // Context_HasFrozenState above.
        bool hasStartup = Combo_HasStartupEntrance();
        uint16_t targetEntrance = hasStartup
            ? Combo_GetStartupEntrance()
            : Context_GetFrozenReturnEntrance(GAME_OOT);
        gSaveContext.entranceIndex = targetEntrance;
        fprintf(stderr, "[OoT] Resume entrance: 0x%04X (startup=%u)\n",
                targetEntrance, hasStartup);
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
    // Capture the pending state up front. If a cross-game switch is already
    // queued when we're called (e.g. a subsequent transition frame after the
    // initial trigger), we still let Combo_CheckCrossGameEntrance run so the
    // return value and any pending-switch updates match the pre-#170 contract
    // exactly — but we suppress the freeze + signal step below, so we don't
    // capture mutated state from a death sequence or mid-transition save.
    // The main loop will process the queued switch on the next iteration.
    bool wasAlreadyPending = Combo_IsCrossGameSwitch();

    // Resolve which game is running so we pick the correct entrance table
    // and freeze the correct SaveContext interpretation. Default to OoT when
    // the current-game tracker hasn't been populated yet (early boot).
    GameId currentGame = Context_GetCurrentGame();
    const char* gameId = (currentGame == GAME_MM) ? "mm" : "oot";

    uint16_t result = Combo_CheckCrossGameEntrance(gameId, entranceIndex);

    if (Combo_IsCrossGameSwitch() && !wasAlreadyPending) {
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

