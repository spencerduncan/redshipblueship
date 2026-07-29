/**
 * Game Entry Points for OoT (Ship of Harkinian) - Single Executable Build
 *
 * This file provides the OoT_Game_* functions expected by the redship
 * main.cpp for single-executable builds.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>
#include <cstring>
#include <cmath>
#include <chrono>
#include "OTRGlobals.h"
#include "soh/CrashHandlerExt.h"
#include <libultraship/bridge.h>
#include <ship/Context.h>
#include "z64save.h"

#include "game_lifecycle.h"
#include "integration_test_hooks.h"
#include "context.h"
#include "save.h" // RsbsSave_SetActiveSlot — publish the slot MM will save into
#include "shared_items.h"
#include "shared_resources.h" // Shared cross-game rupees/hearts (#525)
#include "foreign_items.h"    // OoT_ForeignItem_Give (Lane C1 redemption)
#include "entrance.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Notification/Notification.h" // Lane 6 (#494): the foreign-arrival toast
// SET_NEXT_GAMESTATE for the gameplay round-trip driver. Must come after
// GameInteractor.h (-> z64.h): macros.h declares `extern GraphicsContext*`
// and needs the type defined first.
#include "macros.h"

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
// Retire the graph coroutine on suspend (games/oot/src/code/graph.c) —
// re-entry after a switch re-inits the system arena under any suspended
// gamestate, so the frame loop must cold-start instead of resuming.
void OoT_Graph_ResetRunFrameContext(void);
// Wait for the OTR audio std::thread to finish any in-flight buffer before
// the switch hot-swaps resource archives (OTRGlobals.cpp).
void OoT_Audio_DrainForSuspend(void);
extern s32 gAudioContextInitalized;
void Audio_InitMesgQueues(void);
// Restart the sound system on resume (code_800EC960.c) — suspend's
// PreNMI halts the sequence players, and OoT_AudioMgr_Init's bring-up
// (OoT_Audio_Init + OoT_Audio_InitSound) is behind a static
// hasInitialized guard that never re-runs (audioMgr.c).
void OoT_Audio_InitSound(void);
// Clears the PreNMI resetTimer latch (code_800E4FE0.c) — while it is
// nonzero every sequence start is silently dropped.
void OoT_Audio_ResumeFromPreNMI(void);

// OoT's SaveContext (type from z64save.h).
// Declared here so OoT_Game_Resume() can restore it on return from MM (#170).
extern SaveContext gSaveContext;
}

// The cross-game shadow buffers and unified gSaveContext storage are sized at
// OOT_SAVE_CONTEXT_SIZE (src/common/game.h), which src/common code cannot
// derive from sizeof(SaveContext) because it never includes z64save.h. This TU
// can, so it enforces the capacity here: if SoH's ship.* extension grows past
// the capacity, the build fails instead of freeze/restore silently truncating
// OoT save state on every cross-game switch.
static_assert(sizeof(SaveContext) <= OOT_SAVE_CONTEXT_SIZE,
              "OOT_SAVE_CONTEXT_SIZE (src/common/game.h) is smaller than SoH's runtime SaveContext; "
              "raise the capacity or cross-game freeze/restore will truncate save state");

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
// Gameplay round-trip repro (INT_TEST_GAMEPLAY_ROUNDTRIP) — OoT side
// ============================================================================

// Symbols the gameplay driver borrows from OoT proper. All C linkage:
// OoT_Sram_InitDebugSave / OoT_Play_Init are decomp code (z_sram.c/z_play.c),
// OoT_gGameState / OoT_gPlayState are set by game.c / z_play.c.
extern "C" {
void OoT_Sram_InitDebugSave(void);
void OoT_Play_Init(GameState* thisx);
extern GameState* OoT_gGameState;
extern PlayState* OoT_gPlayState;
// Camera constant registers live in gGameInfo, which Main() re-mints (zeroed)
// on every OoT entry; the seeding is re-armed from func_800636C0. See
// games/oot/src/code/z_camera.c.
bool OoT_Camera_RegsSeeded(void);
}

// Phase-local OoT driver state. sGpArrivalPhase records which phase's
// expected scene has been confirmed by OnSceneInit; gameplay frames are only
// counted while the arrival matches the live phase, so fade-out frames after
// firing a door and pre-arrival frames never count.
static GameplayPhase sGpArrivalPhase = GP_PHASE_DONE;
static GameplayPhase sGpPlayerLastPhase = GP_PHASE_DONE;
static GameplayPhase sGpWatchdogLastPhase = GP_PHASE_DONE;
static int sGpFramesInPhase = 0;
static int sGpSceneInits = 0;
// Wall-clock phase watchdog (#376 item 4). The budget is seconds, not frames:
// a frame budget could not fire before the CTest/`timeout` wall clock under
// llvmpipe, so the diagnostic dump never emitted. sGpWatchdogPhaseStart is
// re-based whenever the OoT-owned phase advances; a phase that makes no
// progress for sGpWatchdogBudgetSecs fails the run with state.
static std::chrono::steady_clock::time_point sGpWatchdogPhaseStart{};
static int sGpWatchdogBudgetSecs = 0;
static bool sGpWatchdogFired = false;
// Door-actor presence check (bug 1a): baseline door count captured in the
// boot-phase scene, compared on the return leg when the scene matches. -1 =
// no baseline captured.
static int sGpDoorBaseline = -1;
static int16_t sGpDoorBaselineScene = -1;
// Camera-follow assert (bug 1b): snapshot of the active camera + player taken
// once the arrival settles; the driver then force-marches the player and
// fails if the camera's eye+at never move while the player does.
static Vec3f sGpCamStartEye;
static Vec3f sGpCamStartAt;
static Vec3f sGpCamStartPlayer;
static int sGpCamProbeArmed = 0;

static float GpVecDist(const Vec3f* a, const Vec3f* b) {
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    float dz = a->z - b->z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

/**
 * The production "walk through a door" trio (z_player.c exit handling /
 * debugconsole `entrance` command): the play update loop consumes it on the
 * next frame, runs the real transition state machine — including
 * Combo_CheckEntranceSwitch on the cross-game path, which freezes the live
 * SaveContext — and re-enters OoT_Play_Init for same-game targets.
 */
// #380: the env-side pre-filter in src/common/integration_test_hooks.cpp keeps a
// literal copy of this bound (kOoTEntranceMax) because src/common cannot include
// OoT headers. Lock the two at compile time so that literal cannot silently drift
// out of sync with the real gEntranceTable size.
static_assert(ENTR_MAX == 0x0614, "ENTR_MAX moved; sync kOoTEntranceMax in integration_test_hooks.cpp");

static void GpFireOoTDoor(uint16_t entrance, const char* what) {
    PlayState* play = OoT_gPlayState;
    if (play == NULL) {
        IntegrationTest_GameplayFail("no PlayState when firing a door transition");
        return;
    }
    // #380: nextEntranceIndex is consumed as a raw linear index into
    // gEntranceTable[ENTR_MAX] (games/oot/include/variables.h) with no bound
    // check downstream. This is the consumption-time re-check the env filter's
    // comment promises: an out-of-range id (e.g. RSBS_GP_WARP_ENTRANCE against a
    // shrunken table) fails the test loudly instead of reading gEntranceTable out
    // of bounds — the OOB-read crash class Test_StartupEntrance guards.
    if (entrance >= ENTR_MAX) {
        fprintf(stderr, "[GP-TEST] refusing %s: entrance 0x%04X is out of range (ENTR_MAX 0x%04X)\n", what, entrance,
                (unsigned)ENTR_MAX);
        fflush(stderr);
        IntegrationTest_GameplayFail("door-transition entrance index >= ENTR_MAX");
        return;
    }
    fprintf(stderr, "[GP-TEST] firing %s: entrance 0x%04X (from scene %d, entrance 0x%04X)\n", what, entrance,
            play->sceneNum, (uint16_t)gSaveContext.entranceIndex);
    fflush(stderr);
    play->nextEntranceIndex = entrance;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
}

/**
 * Author a debug save in-place and enter Play directly at the configured
 * boot entrance. Mirrors the operator's map-select flow — Select_LoadGame
 * (z_select.c) and the boot branch of Enhancements/Warping.cpp Warp() — so
 * the test runs on the same kind of full-inventory save + Play_Init entry the
 * manual repro uses.
 */
static void GpInjectDebugSaveAndEnterPlay(GameState* gameState, const char* from) {
    const GameplayTestConfig* cfg = IntegrationTest_GetGameplayConfig();
    if (gameState == NULL) {
        IntegrationTest_GameplayFail("no GameState available for debug-save injection");
        return;
    }
    fprintf(stderr, "[GP-TEST] injecting debug save at %s; entering Play at entrance 0x%04X\n", from,
            cfg->bootEntrance);
    fflush(stderr);

    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.fileNum = 0xFE; // temporary file so InitDebugSave respects the debug-save-file option
    OoT_Sram_InitDebugSave();
    gSaveContext.magicFillTarget = gSaveContext.magic;
    gSaveContext.magic = 0;
    gSaveContext.magicCapacity = 0;
    gSaveContext.magicLevel = gSaveContext.magic;
    gSaveContext.fileNum = 0xFF;
    gSaveContext.sceneSetupIndex = 0;
    gSaveContext.cutsceneIndex = 0;
    // child + noon => the populated Market Day of the crash logs. With
    // RSBS_GP_BOOT_AGE=adult the save boots adult instead, so the run
    // exercises the forced-child-on-return swap in OoT_Game_Resume (the
    // return-leg assert requires child either way).
    gSaveContext.linkAge = cfg->bootAdult ? LINK_AGE_ADULT : LINK_AGE_CHILD;
    gSaveContext.nightFlag = 0;
    gSaveContext.dayTime = 0x8000;
    gSaveContext.skyboxTime = 0x8000;
    gSaveContext.entranceIndex = cfg->bootEntrance;
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.showTitleCard = true;
    gSaveContext.respawnFlag = 0;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = ENTR_LOAD_OPENING;

    gameState->running = false;
    SET_NEXT_GAMESTATE(gameState, OoT_Play_Init, PlayState);
    GameInteractor_ExecuteOnLoadGame(gSaveContext.fileNum);
    IntegrationTest_SetGameplayPhase(GP_PHASE_OOT_PRE);
}

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
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnZTitleInit>([](void* gameState) {
            fprintf(stderr, "[OoT-INT-TEST] OnZTitleInit hook fired!\n");
            fflush(stderr);
            IntegrationTest_SignalBootComplete(GAME_OOT, "title screen init");
        });

        // Register hook for file select presentation
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPresentFileSelect>([]() {
            fprintf(stderr, "[OoT-INT-TEST] OnPresentFileSelect hook fired!\n");
            fflush(stderr);
            IntegrationTest_SignalBootComplete(GAME_OOT, "file select presented");
        });

        fprintf(stderr, "[OoT] Integration test hooks registered\n");
        fflush(stderr);
    } else if (mode == INT_TEST_SWITCH_OOT_HMS_TO_MM) {
        // T1 (#260): Boot OoT, programmatically trigger the Happy Mask Shop
        // entrance, assert the cross-game switch resolves to MM South Clock
        // Town (the tower-exit arrival). Leg 1 of the test passes when
        // routing is verified here; final pass is signaled from the MM-side
        // hook after MM stabilizes post-switch.
        fprintf(stderr, "[OoT] Registering integration test hooks for HMS->MM switch (T1)\n");
        fflush(stderr);

        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPresentFileSelect>([]() {
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
                fprintf(stderr, "[OoT-INT-TEST] FAIL: target should be 'mm', got '%s'\n", target ? target : "(null)");
                fflush(stderr);
                IntegrationTest_RequestExit();
                return;
            }

            if (targetEntrance != MM_ENTR_SOUTH_CLOCK_TOWN_0) {
                fprintf(stderr,
                        "[OoT-INT-TEST] FAIL: target entrance should be 0x%04X (South Clock Town tower exit), "
                        "got 0x%04X\n",
                        MM_ENTR_SOUTH_CLOCK_TOWN_0, targetEntrance);
                fflush(stderr);
                IntegrationTest_RequestExit();
                return;
            }

            fprintf(stderr, "[OoT-INT-TEST] PASS leg 1: HMS routes to MM 0x%04X; main loop will run the switch\n",
                    targetEntrance);
            fflush(stderr);
            // Intentionally NOT signaling boot complete here. The main loop
            // will see the pending cross-game switch on Combo_CheckHotSwap
            // and hand off to MM. The MM-side hook signals the final pass.
        });

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

        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>([]() {
            sOoTGameStateMainFrameCount++;
            if (sOoTGameStateMainFrameCount >= 10) {
                fprintf(stderr, "[OoT-INT-TEST] OoT stable after SCT-south->OoT switch (frame %d)\n",
                        sOoTGameStateMainFrameCount);
                fflush(stderr);
                IntegrationTest_SignalBootComplete(GAME_OOT, "OoT stable after SCT-south->OoT switch");
            }
        });

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

        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>([]() {
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
            fprintf(stderr, "[OoT-INT-TEST] OoT stable; archive-hotswap arrival #%d of %d\n", n,
                    ArchiveHotswap_TargetArrivals());
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
        });

        fprintf(stderr, "[OoT] archive-hotswap cycle hooks registered\n");
        fflush(stderr);
    } else if (mode == INT_TEST_GAMEPLAY_ROUNDTRIP) {
        // Full operator-repro loop: debug save -> live gameplay -> Happy Mask
        // Shop door (production Combo_CheckEntranceSwitch path, WITH the
        // SaveContext freeze) -> MM Clock Tower -> SCT-south exit -> OoT
        // RESUME leg (the crash surface of the 2026-07 logs) -> debug warp ->
        // final door transition. The MM half lives in
        // games/mm/2s2h/GameExports_SingleExe.cpp; the phase machine is shared
        // via integration_test_hooks.h.
        fprintf(stderr, "[OoT] Registering gameplay round-trip hooks\n");
        fflush(stderr);

        sGpArrivalPhase = GP_PHASE_DONE;
        sGpPlayerLastPhase = GP_PHASE_DONE;
        sGpWatchdogLastPhase = GP_PHASE_DONE;
        sGpFramesInPhase = 0;
        sGpSceneInits = 0;
        // Wall-clock budget per OoT-owned phase, sized well under the CTest
        // TIMEOUT so the watchdog's diagnostic dump fires BEFORE the hard kill
        // (#376 item 4). The timer is re-based below whenever the phase
        // advances, so only a genuinely wedged phase reaches the budget.
        sGpWatchdogPhaseStart = std::chrono::steady_clock::now();
        sGpWatchdogBudgetSecs = IntegrationTest_GetGameplayConfig()->watchdogSecs;
        sGpWatchdogFired = false;

        // Boot injection — whichever of these fires first wins; both are
        // no-ops once the phase machine has left GP_PHASE_BOOT. The title
        // hook receives its gamestate (TitleContext, whose first member is
        // the GameState); the file-select hook has no argument, so it uses
        // the OoT_gGameState global (same pattern as debugconsole.cpp).
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnZTitleUpdate>([](void* gameState) {
            if (IntegrationTest_GetGameplayPhase() != GP_PHASE_BOOT) {
                return;
            }
            GpInjectDebugSaveAndEnterPlay((GameState*)gameState, "title screen");
        });
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPresentFileSelect>([]() {
            if (IntegrationTest_GetGameplayPhase() != GP_PHASE_BOOT) {
                return;
            }
            GpInjectDebugSaveAndEnterPlay(OoT_gGameState, "file select");
        });

        // Arrival tracking + entrance verification. Fires from the scene
        // build inside OoT_Play_Init — i.e. AFTER the startup-entrance
        // consumption at the top of Play_Init, so entranceIndex is final.
        // The return-leg check is the #356 regression predicate: an MM
        // entrance id (0xC010) surviving into OoT would land here as a
        // mismatch (or crash first, which the CI wrapper reports with logs).
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t sceneNum) {
            if (Context_GetCurrentGame() == GAME_MM) {
                return; // shared hook storage guard (#344)
            }
            sGpSceneInits++;
            GameplayPhase phase = IntegrationTest_GetGameplayPhase();
            const GameplayTestConfig* cfg = IntegrationTest_GetGameplayConfig();
            uint16_t entrance = (uint16_t)gSaveContext.entranceIndex;
            uint16_t expected;
            const char* what;
            switch (phase) {
                case GP_PHASE_OOT_PRE:
                    expected = cfg->bootEntrance;
                    what = "boot";
                    break;
                case GP_PHASE_OOT_RETURN:
                    expected = OOT_ENTR_MARKET_FROM_MASK_SHOP;
                    what = "return leg";
                    break;
                case GP_PHASE_OOT_WARP:
                    expected = cfg->warpEntrance;
                    what = "warp";
                    break;
                case GP_PHASE_OOT_EXIT:
                    expected = cfg->exitEntrance;
                    what = "exit door";
                    break;
                default:
                    return; // MM-owned or completed phase: not an arrival we track
            }
            fprintf(stderr, "[GP-TEST] OoT scene init #%d: scene %d, entrance 0x%04X (%s)\n", sGpSceneInits, sceneNum,
                    entrance, what);
            fflush(stderr);
            if (entrance != expected) {
                char msg[160];
                snprintf(msg, sizeof(msg), "%s arrived at entrance 0x%04X, expected 0x%04X — entrance corruption?",
                         what, entrance, expected);
                IntegrationTest_GameplayFail(msg);
                return;
            }
            // Hard-assert the forced-child-on-return contract (operator
            // decision: the MM trip is child-canon; OoT_Game_Resume
            // forces linkAge child after restoring the frozen save). A
            // regression passes the entrance check above but lands here
            // as LINK_AGE_ADULT.
            if (phase == GP_PHASE_OOT_RETURN && gSaveContext.linkAge != LINK_AGE_CHILD) {
                char ageMsg[128];
                snprintf(ageMsg, sizeof(ageMsg),
                         "return leg arrived as linkAge=%d, expected LINK_AGE_CHILD (%d) — "
                         "force-child-on-return regressed",
                         (int)gSaveContext.linkAge, (int)LINK_AGE_CHILD);
                IntegrationTest_GameplayFail(ageMsg);
                return;
            }
            // Demo-state leakage asserts (the bug 1a/1b/1c common-cause
            // class): every cross-game arrival and post-return load must be a
            // plain gameplay spawn. A title/attract gameMode, a live cutscene
            // index, a cutscene scene layer, or a queued nextCutsceneIndex
            // here means frozen-blob or title-chain state escaped the
            // consumption-point neutralization.
            if (phase == GP_PHASE_OOT_RETURN || phase == GP_PHASE_OOT_WARP) {
                // The camera constants must be seeded on every arrival. Main()
                // re-mints gGameInfo (zeroing the REG backing store) on each
                // OoT entry, while the seeding sat behind a once-per-process
                // latch — so every return leg used to run the camera on
                // all-zero constants: it translated with Link but never
                // reoriented to follow him, and Player's stick-to-world yaw
                // (derived from the camera) went with it.
                if (!OoT_Camera_RegsSeeded()) {
                    IntegrationTest_GameplayFail("camera constant registers are zeroed on arrival — the "
                                                 "once-only OREG seeding lost its resume inverse");
                    return;
                }
                if (gSaveContext.gameMode != GAMEMODE_NORMAL || gSaveContext.cutsceneIndex >= 0xFFF0 ||
                    gSaveContext.sceneSetupIndex >= 4 || gSaveContext.nextCutsceneIndex != 0xFFEF) {
                    char stateMsg[192];
                    snprintf(stateMsg, sizeof(stateMsg),
                             "%s arrived with demo state: gameMode=%d cutsceneIndex=0x%04X sceneSetupIndex=%d "
                             "nextCutsceneIndex=0x%04X — title/cutscene state leaked into a gameplay spawn",
                             what, (int)gSaveContext.gameMode, (uint16_t)gSaveContext.cutsceneIndex,
                             (int)gSaveContext.sceneSetupIndex, (uint16_t)gSaveContext.nextCutsceneIndex);
                    IntegrationTest_GameplayFail(stateMsg);
                    return;
                }
            }
            sGpArrivalPhase = phase;
        });

        // Per-frame gameplay driver: counts live-gameplay frames (player
        // actor updating in the arrived scene) and fires the next action
        // when the window completes.
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>([]() {
            if (Context_GetCurrentGame() == GAME_MM) {
                return; // shared hook storage guard (#344)
            }
            if (OoT_gPlayState == NULL) {
                return;
            }
            GameplayPhase phase = IntegrationTest_GetGameplayPhase();
            if (phase != sGpPlayerLastPhase) {
                sGpPlayerLastPhase = phase;
                sGpFramesInPhase = 0;
                sGpCamProbeArmed = 0;
            }
            if (sGpArrivalPhase != phase) {
                return; // fade-out after firing a door, or not yet in the expected scene
            }
            const GameplayTestConfig* cfg = IntegrationTest_GetGameplayConfig();
            PlayState* play = OoT_gPlayState;
            sGpFramesInPhase++;

            // Door-actor presence (bug 1a): by frame 30 the arrival scene's
            // transition actors have spawned. Baseline in the boot phase;
            // compare on the return leg when the scene matches (default route:
            // both are Market via 0x01D1). Fewer doors than first boot means
            // the switch poisoned the cached transition-actor list.
            if (sGpFramesInPhase == 30) {
                int doorCount = play->actorCtx.actorLists[ACTORCAT_DOOR].length;
                if (phase == GP_PHASE_OOT_PRE) {
                    sGpDoorBaseline = doorCount;
                    sGpDoorBaselineScene = play->sceneNum;
                    fprintf(stderr, "[GP-TEST] door baseline: %d door actors in scene %d\n", doorCount,
                            (int)play->sceneNum);
                    fflush(stderr);
                } else if (phase == GP_PHASE_OOT_RETURN && sGpDoorBaseline >= 0 &&
                           play->sceneNum == sGpDoorBaselineScene) {
                    fprintf(stderr, "[GP-TEST] door check: %d door actors in scene %d (baseline %d)\n", doorCount,
                            (int)play->sceneNum, sGpDoorBaseline);
                    fflush(stderr);
                    if (doorCount < sGpDoorBaseline) {
                        char doorMsg[160];
                        snprintf(doorMsg, sizeof(doorMsg),
                                 "return leg scene %d has %d door actors, first boot had %d — transition "
                                 "actors lost across the switch (bug 1a)",
                                 (int)play->sceneNum, doorCount, sGpDoorBaseline);
                        IntegrationTest_GameplayFail(doorMsg);
                        return;
                    }
                }
            }

            // Camera-follow assert (bug 1b), WARP phase only: frames 20..39
            // arm a snapshot of the active camera + player once the frame is
            // settled plain gameplay; the driver then force-marches the player
            // (+4/frame — the harness has no input injection, so Link never
            // moves on his own); frame 80 requires the camera to have tracked.
            // WARP-only because a march in the return-leg Market crosses a
            // scene-exit trigger and aborts the phase (found the hard way);
            // the operator's camera bug lives in warp-class arrivals (Hyrule
            // Field 0x00CD). If geometry walls the march in, playerDist stays
            // small and the assert self-vacuouses (lost coverage, never a
            // false positive). Disable with RSBS_GP_CAMERA_ASSERT=0.
            if (cfg->cameraAssert && cfg->framesPerPhase >= 90 && phase == GP_PHASE_OOT_WARP) {
                Actor* playerActor = play->actorCtx.actorLists[ACTORCAT_PLAYER].head;
                Camera* cam = play->cameraPtrs[play->activeCamera];
                if (playerActor != NULL && cam != NULL) {
                    // Arm anywhere in frames 20..39 (transitions can settle a
                    // few frames late); log the gate state if it never opens —
                    // silent non-coverage is how vacuous asserts are born.
                    if (!sGpCamProbeArmed && sGpFramesInPhase >= 20 && sGpFramesInPhase < 40 &&
                        gSaveContext.gameMode == GAMEMODE_NORMAL && play->csCtx.state == CS_STATE_IDLE &&
                        play->transitionMode == TRANS_MODE_OFF) {
                        sGpCamStartEye = cam->eye;
                        sGpCamStartAt = cam->at;
                        sGpCamStartPlayer = playerActor->world.pos;
                        sGpCamProbeArmed = 1;
                    }
                    if (!sGpCamProbeArmed && sGpFramesInPhase == 40) {
                        fprintf(stderr,
                                "[GP-TEST] WARNING: camera probe never armed (gameMode=%d csState=%d "
                                "transitionMode=%d) — camera-follow assert skipped this phase\n",
                                (int)gSaveContext.gameMode, (int)play->csCtx.state, (int)play->transitionMode);
                        fflush(stderr);
                    }
                    if (sGpCamProbeArmed && sGpFramesInPhase < 80) {
                        playerActor->world.pos.x += 4.0f;
                    }
                    if (sGpCamProbeArmed && sGpFramesInPhase == 80) {
                        float playerDist = GpVecDist(&playerActor->world.pos, &sGpCamStartPlayer);
                        // eye and at MUST be judged separately. A camera
                        // bolted in place that merely rotates to keep Link in
                        // frame — the degenerate state Camera_Update falls
                        // into when it skips the setting/mode engine — moves
                        // `at` by roughly the player's displacement while
                        // `eye` never moves at all. Summing the two hides
                        // exactly the failure this assert exists to catch.
                        float eyeDist = GpVecDist(&cam->eye, &sGpCamStartEye);
                        float atDist = GpVecDist(&cam->at, &sGpCamStartAt);
                        fprintf(stderr,
                                "[GP-TEST] camera-follow: player moved %.1f, camera eye moved %.1f, at moved %.1f "
                                "(status=%d setting=%d mode=%d)\n",
                                playerDist, eyeDist, atDist, (int)cam->status, (int)cam->setting, (int)cam->mode);
                        fflush(stderr);
                        sGpCamProbeArmed = 0;
                        if (playerDist > 80.0f && eyeDist < 5.0f) {
                            char camMsg[224];
                            snprintf(camMsg, sizeof(camMsg),
                                     "camera did not follow: player moved %.1f but camera eye moved %.1f "
                                     "(at moved %.1f, status=%d setting=%d mode=%d) — camera is anchored",
                                     playerDist, eyeDist, atDist, (int)cam->status, (int)cam->setting, (int)cam->mode);
                            IntegrationTest_GameplayFail(camMsg);
                            return;
                        }
                    }
                }
            }

            // The warp phase gets its own (usually longer) budget so time-
            // dependent faults inside the warp target can soak.
            {
                int phaseBudget = (phase == GP_PHASE_OOT_WARP) ? cfg->warpFrames : cfg->framesPerPhase;
                if (sGpFramesInPhase < phaseBudget) {
                    return;
                }
            }
            switch (phase) {
                case GP_PHASE_OOT_PRE:
                    GpFireOoTDoor(OOT_ENTR_HAPPY_MASK_SHOP, "Happy Mask Shop door");
                    IntegrationTest_SetGameplayPhase(GP_PHASE_MM_STABILIZE);
                    break;
                case GP_PHASE_OOT_RETURN:
                    IntegrationTest_GameplayRecordCycle();
                    if (IntegrationTest_GameplayCyclesDone() < cfg->cycles) {
                        GpFireOoTDoor(OOT_ENTR_HAPPY_MASK_SHOP, "Happy Mask Shop door (next round trip)");
                        IntegrationTest_SetGameplayPhase(GP_PHASE_MM_STABILIZE);
                    } else {
                        GpFireOoTDoor(cfg->warpEntrance, "post-return debug warp");
                        IntegrationTest_SetGameplayPhase(GP_PHASE_OOT_WARP);
                    }
                    break;
                case GP_PHASE_OOT_WARP:
                    GpFireOoTDoor(cfg->exitEntrance, "final door transition");
                    IntegrationTest_SetGameplayPhase(GP_PHASE_OOT_EXIT);
                    break;
                case GP_PHASE_OOT_EXIT:
                    fprintf(stderr,
                            "[GP-TEST] PASS: %d round trip(s), warp, and door transition survived "
                            "%d live frames per phase\n",
                            IntegrationTest_GameplayCyclesDone(), cfg->framesPerPhase);
                    fflush(stderr);
                    IntegrationTest_SetGameplayPhase(GP_PHASE_DONE);
                    IntegrationTest_SignalBootComplete(GAME_OOT, "gameplay round-trip complete");
                    break;
                default:
                    break;
            }
        });

        // OoT-side watchdog: fail loudly (with state) instead of timing out
        // silently if an OoT-owned phase stops making progress. Budgeted in
        // WALL-CLOCK seconds (#376 item 4) so the diagnostic below is emitted
        // before the CTest/`timeout` kill — a frame budget lost that race.
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>([]() {
            if (Context_GetCurrentGame() == GAME_MM) {
                return;
            }
            GameplayPhase phase = IntegrationTest_GetGameplayPhase();
            switch (phase) {
                case GP_PHASE_BOOT:
                case GP_PHASE_OOT_PRE:
                case GP_PHASE_OOT_RETURN:
                case GP_PHASE_OOT_WARP:
                case GP_PHASE_OOT_EXIT:
                    break;
                default:
                    // Not an OoT-owned phase (MM is driving, or the run is
                    // done): re-base so the timer only measures the current
                    // OoT phase's stall, never the MM leg.
                    sGpWatchdogPhaseStart = std::chrono::steady_clock::now();
                    sGpWatchdogLastPhase = phase;
                    return;
            }
            std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
            if (phase != sGpWatchdogLastPhase) {
                // The phase advanced — progress. Re-base the stall timer.
                sGpWatchdogLastPhase = phase;
                sGpWatchdogPhaseStart = now;
            }
            double elapsedSecs = std::chrono::duration<double>(now - sGpWatchdogPhaseStart).count();
            if (!sGpWatchdogFired && IntegrationTest_GameplayWatchdogExpired(elapsedSecs, sGpWatchdogBudgetSecs)) {
                sGpWatchdogFired = true;
                PlayState* play = OoT_gPlayState;
                fprintf(stderr,
                        "[GP-TEST] OoT watchdog: no progress for %.1f s (budget %d s) in phase %d "
                        "(play=%p scene=%d entrance=0x%04X arrivedPhase=%d sceneInits=%d gameplayFrames=%d)\n",
                        elapsedSecs, sGpWatchdogBudgetSecs, (int)phase, (void*)play, play ? play->sceneNum : -1,
                        (uint16_t)gSaveContext.entranceIndex, (int)sGpArrivalPhase, sGpSceneInits, sGpFramesInPhase);
                fflush(stderr);
                IntegrationTest_GameplayFail("OoT-side phase watchdog expired");
            }
        });

        fprintf(stderr, "[OoT] gameplay round-trip hooks registered\n");
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

// Shared cross-game resources (#525) — defined further down this TU, beside the
// apply half; forward-declared so Game_Suspend can harvest.
extern "C" void OoT_HarvestSharedResources(void);

/**
 * Suspend OoT for a game switch (issue #160).
 * Stops audio to prevent interference with MM, keeps libultraship context alive.
 */
void OoT_Game_Suspend(void) {
    fprintf(stderr, "[OoT] Game_Suspend called\n");
    fflush(stderr);

    // Producer (ADR 0002 / Lane A1): commit any staged cross-game items into
    // gComboCtx.sharedItemsTagged before we hand control to MM. This lives at
    // Game_Suspend — not Combo_CheckEntranceSwitch — on purpose: the F10
    // hot-swap path (Combo_FreezeActiveGameForHotSwap) bypasses the entrance
    // hook entirely, and GameRunner_SwitchTo calls suspend() on both switch
    // paths, so this is the one point that never drops a hotkey switch's
    // writes. The array itself is process-global and crosses the switch by
    // being shared; committing here just moves staged pickups into the durable
    // (serialized) store before the arriving game's consumer reads it.
    Combo_CommitStagedSharedItems();

    // Shared cross-game resources (#525): fold OoT's live rupees, wallet tier,
    // hearts, current health and double defense into the shared pool while
    // gSaveContext still belongs to OoT. Same reason this sits at Game_Suspend
    // rather than Combo_CheckEntranceSwitch — the F10 path bypasses the
    // entrance hook, and both switch paths call suspend().
    OoT_HarvestSharedResources();

    // Stop OoT audio playback to prevent interference with MM (issue #160).
    // OoT_Audio_PreNMI triggers the audio reset path which stops all sequences
    // and puts the audio system into a quiescent state.
    //
    // FIRST drain the OTR audio thread. It IS a real std::thread
    // (OTRGlobals.cpp OTRAudio_Init: audio.thread = std::thread(OTRAudio_Thread)),
    // NOT the commented-out N64 audioMgr thread — an earlier note here wrongly
    // claimed audio was synchronous. While it is mid-buffer it can load
    // sequences/soundfonts through the shared ResourceManager, so the switch's
    // archive hot-swap that follows suspend must not free a resource it is
    // using (that use-after-free corrupts the process heap and later faults in
    // RtlAllocateHeap). Wait for the in-flight buffer to finish before PreNMI.
    fprintf(stderr, "[OoT] Draining audio thread before suspend...\n");
    fflush(stderr);
    OoT_Audio_DrainForSuspend();

    fprintf(stderr, "[OoT] Stopping audio via PreNMI path...\n");
    fflush(stderr);
    OoT_Audio_PreNMI();

    // Mark audio as uninitialized so re-init works on resume
    gAudioContextInitalized = false;

    // Retire the graph coroutine: re-entering OoT runs Main() again, which
    // re-initializes the system arena (0xAB fill) underneath any suspended
    // gamestate — resuming the frame loop would update a poisoned PlayState
    // (the int-gameplay-roundtrip return-leg AV in GameState_SetFrameBuffer).
    // The next OoT entry cold-starts the gamestate chain; continuity is the
    // frozen SaveContext + the OoT-tagged startup entrance, which the title
    // screen fast-forwards on and OoT_Play_Init consumes.
    fprintf(stderr, "[OoT] Retiring graph coroutine for switch...\n");
    fflush(stderr);
    OoT_Graph_ResetRunFrameContext();

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
        // Query the OoT-scoped accessor rather than the game-agnostic one: a
        // value tagged for MM (e.g. 0xC010) that leaked into the shared startup
        // global must NOT be applied to OoT's entranceIndex — it is a direct
        // linear index into gEntranceTable and would read far out of bounds
        // (crash) once Play_Init runs. When the leaked value is invisible to
        // OoT, hasStartup is false and we fall back to the frozen OoT return
        // entrance, which is the correct resume target and always in range.
        // Use the Has check rather than (entrance != 0) — entrance 0x0000 is the
        // real id for Kokiri Forest from Deku Tree, so a legit restore to 0 must
        // not be silently dropped. The frozen return entrance is always
        // trustworthy here because we already checked Context_HasFrozenState.
        bool hasStartup = Combo_HasStartupEntranceForGame("oot");
        uint16_t targetEntrance =
            hasStartup ? Combo_GetStartupEntranceForGame("oot") : Context_GetFrozenReturnEntrance(GAME_OOT);
        gSaveContext.entranceIndex = targetEntrance;
        fprintf(stderr, "[OoT] Resume entrance: 0x%04X (startup=%u)\n", targetEntrance, hasStartup);

        // NOTE: with the cold-boot contract this restore is defense in depth,
        // not the continuity mechanism — the resume fast-forward passes
        // through Opening_Init (z_opening.c), which re-authors the ADULT
        // debug save over gSaveContext AFTER this runs. The restore that
        // reaches gameplay (plus the force-child-on-return equip swap) lives
        // at the startup-entrance consumption point in OoT_Play_Init
        // (games/oot/src/code/z_play.c) — the first spot after the last wipe,
        // exactly mirroring MM_Play_ConsumeStartupEntrance.
    }

    // Reinitialize audio message queues for clean state (issue #160).
    // The audio context's queue pointers may be stale after suspend.
    fprintf(stderr, "[OoT] Reinitializing audio message queues...\n");
    fflush(stderr);
    Audio_InitMesgQueues();

    // Re-arm the shared audio thread's OoT synth and restart the sound
    // system (mirrors MM_Game_Resume). The audio heap survives suspend;
    // PreNMI only scheduled a reset and halted the players, and the
    // OoT_AudioMgr_Init bring-up is behind a never-re-run static guard
    // (audioMgr.c hasInitialized). Without this, every OoT return leg played
    // pure silence: the reset completed on the shared thread, no player ever
    // restarted, and the restored save's stale seqId told the scene its BGM
    // was already live (that last part is reset at the startup-entrance
    // consumption in z_play.c). InitSound's commands queue in the ring and
    // apply once the reset finishes.
    gAudioContextInitalized = true;
    OoT_Audio_ResumeFromPreNMI();
    OoT_Audio_InitSound();

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

static GameOps sOoTOps = { "oot",           "Ocarina of Time", OoT_Game_Init, OoT_Game_Run, OoT_Game_Suspend,
                           OoT_Game_Resume, OoT_Game_Shutdown };

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
void Combo_FreezeState(const char* gameId, uint16_t returnEntrance, const void* saveCtx, size_t saveCtxSize);
void Combo_SignalReadyToSwitch(void);
void Combo_RequestGameSwitch(void);
bool Combo_IsGameSwitchRequested(void);
void Combo_ClearGameSwitchRequest(void);
// Hot-swap freeze policy (src/common/switch.cpp). The launcher drives the F10
// freeze, but only this side can supply the SaveContext, so the glue below
// bridges the two.
int Switch_PrepareHotSwap(GameId departing, const void* saveContext, size_t size);
}

/**
 * Freeze the departing game for an F10 hot swap (#364).
 *
 * The launcher (rsbs/src/main.cpp) decides that a hot swap is happening, but
 * `gSaveContext` is only addressable from a game translation unit — hence this
 * one-line bridge. It is the F10 twin of the freeze `Combo_CheckEntranceSwitch`
 * performs below for the entrance path; without it, F10 departures produced no
 * blob at all while the launcher still restored whatever blob an *earlier*
 * entrance switch had left behind.
 *
 * As in `Combo_CheckEntranceSwitch`, `sizeof(gSaveContext)` here is OoT's
 * layout even when MM is the departing game. That over-reads relative to MM's
 * smaller struct but stays in bounds: the underlying unified storage
 * (src/common/unified_save.c) is OOT_SAVE_CONTEXT_SIZE for both games, and
 * Context_FreezeState clamps to the per-game blob capacity.
 *
 * @return 1 if a fresh blob was recorded, 0 if the launcher must refuse the
 *         switch instead of proceeding into a stale restore.
 */
extern "C" int Combo_FreezeActiveGameForHotSwap(GameId departing) {
    return Switch_PrepareHotSwap(departing, &gSaveContext, sizeof(gSaveContext));
}

/**
 * Award a single OoT-origin shared item (ADR 0002 / Lane A1 consumer callback,
 * real give wired by Lane C1 #392).
 *
 * Invoked once per un-redeemed entry tagged GAME_OOT when the player arrives in
 * OoT (see OoT_ConsumeSharedItems). `item->id` is an OoT RandomizerGet (RG_*).
 *
 * The give itself lives in OoT_ForeignItem_Give
 * (soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp): GetGIEntry
 * resolution (progressives resolve against the live save) + the
 * StartingItemGive-style dispatch into OoT_Item_Give / Randomizer_Item_Give.
 * Combo_RedeemSharedItemsForGame marks the entry RSBS_SHARED_ITEM_REDEEMED
 * after this returns, so the crossing stays single-use; the entry is never
 * cleared (the durable record of the crossing). A give that reports failure is
 * logged but still consumes the redemption — by the time this callback can
 * run, OoT is at its presence-gated arrival point and the guarded
 * prerequisites are live, so that path is defensive, not expected.
 */
static void OoT_AwardSharedItem(const SharedItem* item, void* ctx) {
    (void)ctx;
    int given = OoT_ForeignItem_Give(item->id);
    fprintf(stderr, "[OoT] shared-item redeem: RG id=%u %s (Lane C1 foreign give)\n", (unsigned)item->id,
            given ? "awarded" : "NOT awarded — give path unavailable");

    // #494: tell the player. Until this, a cross-game item arrived in OoT with
    // NO in-game signal at all — the item simply appeared in the inventory, an
    // arbitrary number of scenes after the MM check that granted it. The MM
    // side has said "it will be awarded there!" since Lane C1; this is the
    // other half of that sentence.
    //
    // A toast rather than a textbox on purpose: the redeem runs inside
    // Play_Init (z_play.c:565), before the first frame, where there is no
    // message context to drive and nothing to dismiss it with. The notification
    // overlay is the one surface that works from here, and it is also the
    // surface the arrival ALREADY renders over — MM's rando pickups cross-bind
    // to this exact Emit (#427 item 1).
    //
    // No .itemIcon: resolving one needs a per-item icon-name accessor that
    // src/common does not have yet (the Combo_GetForeignItemIconName tier of
    // #494). Emitting text-only is an existing idiom, not a degradation — the
    // MOD_RANDOMIZER branch in hook_handlers.cpp omits the icon too.
    //
    // WORDING (#510): "You got the Fairy Bow", not "Received from Termina: …".
    // The arrival IS the moment the player receives it in OoT, so the native
    // sentence is the correct one; naming the other game was the cross-game
    // tell the operator asked to remove. Name and article both come from the
    // pinned pool via the origin-tagged SharedItem, so this never fabricates an
    // id-space crossing; both return NULL when that origin's pool is not linked
    // into this build, hence the fallbacks.
    const char* foreignName = Combo_GetForeignItemName(*item);
    const char* foreignArticle = Combo_GetForeignItemArticle(*item);
    Notification::Emit({
        .prefix = "You got",
        .message = std::string(foreignArticle != nullptr ? foreignArticle : "") +
                   (foreignName != nullptr ? foreignName : "a foreign item"),
    });
}

/**
 * Consumer hook (ADR 0002 / Lane A1). Award every un-redeemed OoT-origin shared
 * item and mark it redeemed. Called from OoT_Play_Init's presence-gated
 * startup-entrance consumption (games/oot/src/code/z_play.c) — i.e. only on a
 * cross-game arrival into OoT, once per arrival. A plain boot / .redsave load
 * never reaches it, so un-redeemed items wait for the next switch into OoT
 * ("applies on next switch only"; see shared_items.h).
 *
 * Exposed as a plain C entry point so z_play.c does not have to pull in the
 * ADR SharedItem / GameId types (it isolates every src/common call behind bare
 * externs).
 */
extern "C" void OoT_ConsumeSharedItems(void) {
    Combo_RedeemSharedItemsForGame(GAME_OOT, OoT_AwardSharedItem, nullptr);
}

// ============================================================================
// Shared cross-game RESOURCES — OoT side (#525)
//
// The merge rules live in src/common/shared_resources.c, which has no game
// headers. This is the half that does: it reads and writes OoT's own
// gSaveContext fields and converts them into the units the pool is defined in.
// MM's twin is games/mm/2s2h/GameExports_SingleExe.cpp.
// ============================================================================

// OoT's own upgrade setter and the three tables CUR_UPG_VALUE / CUR_CAPACITY
// expand to (games/oot/src/code/z_inventory.c, declared in variables.h).
// Declared here rather than by including variables.h: this TU deliberately keeps
// a narrow game-header surface — it already hand-declares gSaveContext for the
// same reason — and pulling in the full variable set to reach three arrays would
// widen it for no benefit. The declarations match variables.h exactly, so a
// layout change breaks the link rather than reading garbage.
extern "C" void OoT_Inventory_ChangeUpgrade(s16 upgrade, s16 value);
extern "C" u32 OoT_gUpgradeMasks[8];
extern "C" u8 OoT_gUpgradeShifts[8];
extern "C" u16 OoT_gUpgradeCapacities[8][4];

// Heart pieces live in the TOP NIBBLE of questItems in BOTH games (OoT writes
// `1 << (QUEST_HEART_PIECE + 4)`, MM's QUEST_HEART_PIECE_COUNT is 0x1C), which
// is what makes one canonical quantity possible at all.
#define OOT_HEART_PIECE_SHIFT 28u
#define OOT_HEART_PIECE_MASK 0xF0000000u

// The highest wallet tier this build defines (OoT_gUpgradeCapacities row
// UPG_WALLET is {99, 200, 500, 999}). Passed as the apply CAP so a pool value
// authored by a future build with more tiers cannot index off the end of OoT's
// capacity table.
#define OOT_MAX_WALLET_TIER 3u

static uint16_t OoT_ReadHealthQuarters(void) {
    const uint16_t pieces =
        (uint16_t)((gSaveContext.inventory.questItems & OOT_HEART_PIECE_MASK) >> OOT_HEART_PIECE_SHIFT);
    const uint16_t capacity = gSaveContext.healthCapacity < 0 ? 0u : (uint16_t)gSaveContext.healthCapacity;
    // The canonical quantity and its inverse both live in src/common so there
    // is exactly one copy of the arithmetic; see Combo_MakeHealthQuarters for
    // why hearts are one number rather than two shared fields.
    return Combo_MakeHealthQuarters(capacity, pieces);
}

/**
 * HARVEST (#525). Fold OoT's live resource values into the shared pool.
 *
 * Called from OoT_Game_Suspend — the one point on BOTH the entrance and the F10
 * hot-swap path while gSaveContext still belongs to OoT — and immediately
 * before every `.redsave` write, so a file written mid-session carries a pool
 * that agrees with the OoT save stored beside it. Idempotent: a second call
 * with unchanged values is a no-op in both merge disciplines.
 */
extern "C" void OoT_HarvestSharedResources(void) {
    // SETTLE THE ACCUMULATOR FIRST. Both games write rupeeAccumulator, not the
    // count, and drain it one per frame. A pending accumulator would otherwise
    // ride the frozen blob and drain into OoT later — after an apply has
    // already written an authoritative count — crediting the player twice. Fold
    // it in and zero it so what we harvest is what OoT actually has.
    const int32_t walletCap = (int32_t)CUR_CAPACITY(UPG_WALLET);
    int32_t liveRupees = (int32_t)gSaveContext.rupees + (int32_t)gSaveContext.rupeeAccumulator;
    if (liveRupees < 0) {
        liveRupees = 0;
    }
    if (liveRupees > walletCap) {
        liveRupees = walletCap;
    }
    gSaveContext.rupees = (s16)liveRupees;
    gSaveContext.rupeeAccumulator = 0;

    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, (uint16_t)liveRupees);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_WALLET_TIER, (uint16_t)CUR_UPG_VALUE(UPG_WALLET));
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_QUARTERS, OoT_ReadHealthQuarters());
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_CURRENT,
                                gSaveContext.health < 0 ? 0u : (uint16_t)gSaveContext.health);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_DOUBLE_DEFENSE,
                                gSaveContext.isDoubleDefenseAcquired ? 1u : 0u);
}

/**
 * APPLY (#525). Write the shared pool into OoT's live gSaveContext.
 *
 * Called from OoT_Play_Init's presence-gated startup-entrance branch beside
 * OoT_ConsumeSharedItems — the first point after the boot chain's last
 * gSaveContext wipe, and once per arrival into OoT. NOT Game_Resume: both
 * restores are re-authored afterwards by Opening_Init.
 *
 * ORDER MATTERS. Wallet tier and health capacity are applied BEFORE the
 * quantities they bound, because each quantity is clamped to the ceiling the
 * arriving game can actually display — and that ceiling is what the two lines
 * above may have just raised.
 */
extern "C" void OoT_ApplySharedResources(void) {
    // --- Wallet tier (monotonic). Raises OoT's clamp before rupees land.
    uint16_t walletTier = (uint16_t)CUR_UPG_VALUE(UPG_WALLET);
    if (Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_WALLET_TIER, OOT_MAX_WALLET_TIER, &walletTier)) {
        OoT_Inventory_ChangeUpgrade(UPG_WALLET, (s16)walletTier);
    }

    // --- Health capacity + pieces, from the ONE canonical quantity.
    uint16_t quarters = OoT_ReadHealthQuarters();
    if (Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_QUARTERS,
                                  (uint16_t)RSBS_SHARED_RES_MAX_HEALTH_QUARTERS, &quarters)) {
        // Split back: whole hearts to capacity, the remainder to the piece
        // nibble. The 320 clamp inside the split is load-bearing — NEITHER
        // game's give path clamps capacity, and a total accumulated across two
        // pools of pieces and containers exceeds 20 hearts easily. The life
        // meter past 20 is untested in both ports.
        uint16_t capacity = 0;
        uint16_t pieces = 0;
        Combo_SplitHealthQuarters(quarters, &capacity, &pieces);
        gSaveContext.healthCapacity = (s16)capacity;
        gSaveContext.inventory.questItems =
            (gSaveContext.inventory.questItems & ~OOT_HEART_PIECE_MASK) | ((uint32_t)pieces << OOT_HEART_PIECE_SHIFT);
    }

    // --- Double defense (monotonic 0/1). Deliberately NOT a byte copy: the
    // flag is spelled isDoubleDefenseAcquired here and doubleDefense in MM, and
    // each game carries its own separate inventory.defenseHearts counter that
    // the life meter reads. Share the FACT, let each game set its own pair.
    uint16_t doubleDefense = gSaveContext.isDoubleDefenseAcquired ? 1u : 0u;
    if (Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_DOUBLE_DEFENSE, 1u, &doubleDefense) && doubleDefense != 0) {
        gSaveContext.isDoubleDefenseAcquired = 1;
        if (gSaveContext.inventory.defenseHearts < 20) {
            gSaveContext.inventory.defenseHearts = 20;
        }
    }

    // --- Rupees (consumable), clamped to the wallet capacity just applied.
    uint16_t rupees = gSaveContext.rupees < 0 ? 0u : (uint16_t)gSaveContext.rupees;
    if (Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, (uint16_t)CUR_CAPACITY(UPG_WALLET), &rupees)) {
        gSaveContext.rupees = (s16)rupees;
    }
    // Whatever the restored blob had pending would drain on top of the count we
    // just authored. Zero it: the harvest that produced this pool already
    // folded OoT's accumulator in.
    gSaveContext.rupeeAccumulator = 0;

    // --- Current health (consumable). One bar across both games, per OoTMM:
    // "current health is tracked as if OoT and MM were one game with a single
    // health bar". Clamped to the capacity applied above.
    uint16_t health = gSaveContext.health < 0 ? 0u : (uint16_t)gSaveContext.health;
    if (Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_CURRENT, (uint16_t)gSaveContext.healthCapacity,
                                  &health)) {
        // Floor at one heart. A departing game cannot normally hand over a dead
        // bar (death resets health before any suspend can see it), so this only
        // fires on a corrupt or hand-edited pool — and spawning dead on a
        // cross-game arrival lands in a death handler no arrival path has ever
        // been tested through.
        gSaveContext.health = (s16)(health < 0x10u ? 0x10u : health);
    }
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
        fprintf(stderr, "[COMBO] Cross-game switch (%s)! entrance=0x%04X\n", gameId, entranceIndex);

        // Publish the unified slot while OoT still knows it. This is the last
        // moment it is knowable: MM boots with gSaveContext.fileNum pinned to
        // the 0xFF sentinel (ConsoleLogo_Init) for the whole cross-game
        // session, and the only writers of a real 0..2 slot live in MM's own
        // file select, which a portal arrival never enters. Without this, an
        // MM-side save has no slot to address.
        //
        // Guarded two ways. Only when OoT is the DEPARTING game: sizeof and
        // field offsets in this TU are OoT's SaveContext layout, so reading
        // fileNum while MM is active would pull an unrelated MM field. And
        // only for an in-range value, so OoT's own 0xFF title-screen sentinel
        // does not clear a slot that a real load already established.
        if (currentGame != GAME_MM) {
            const int ootFileNum = static_cast<int>(gSaveContext.fileNum);
            if (ootFileNum >= 0 && ootFileNum < RSBS_SAVE_MAX_SLOTS) {
                RsbsSave_SetActiveSlot(ootFileNum);
            }
        }

        uint16_t returnEntrance = Combo_GetSwitchReturnEntrance();
        // sizeof(gSaveContext) in this TU is OoT's SaveContext layout. When MM
        // is the active game this over-reads relative to MM's smaller struct,
        // but the underlying unified storage (unified_save.c) is
        // OOT_SAVE_CONTEXT_SIZE for both games, so the read stays in bounds
        // and Context_FreezeState clamps to the per-game blob capacity.
        Combo_FreezeState(gameId, returnEntrance, &gSaveContext, sizeof(gSaveContext));
        Combo_SignalReadyToSwitch();
    }

    return result;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
