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
#include "soh/cvar_prefixes.h" // CVAR_ENHANCEMENT: the continue value OoT_Combo_ReviveDeadHealthForFreeze mirrors

#include "game_lifecycle.h"
#include "integration_test_hooks.h"
#include "context.h"
#include "save.h" // RsbsSave_SetActiveSlot — publish the slot MM will save into
#include "shared_items.h"
#include "shared_resources.h" // Shared cross-game rupees/hearts (#525)
#include "triforce_hunt.h"    // ADR 0010 O10: the one triforce piece count's apply cap
#include "foreign_items.h"    // OoT_ForeignItem_Give (Lane C1 redemption)
#include "entrance.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Notification/Notification.h" // Lane 6 (#494): the foreign-arrival toast
// Rando::Context's definition, for GetBombchuCapacity() in the ammo shim.
// OTRGlobals.h only forward-declares Rando::Context, which is enough to hold
// the shared_ptr but not to call through it.
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/SaveManager.h"     // SaveFileMetaInfo: the paired row's slot ownership mark
#include "notification_bridge.h" // the paired row's "no refusal toast" check
#include "crossing_store.h"      // the paired row's crossing-store check
// The paired row's "shipped defaults" check: every setting the generation
// reads, per surface (OoT's options/tricks/exclusions, MM's options/tricks, the
// combo settings), must be unset in the CVar store.
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "combo_mm_options_view.h"
#include "combo_mm_tricks_view.h"
#include "combo_settings_view.h"
// #750: OoT_RetireAbandonedSession resets the actor DB's clients and drops every
// per-actor object extension of the session a departure abandons.
#include "soh/ActorDB.h"
#include "soh/ObjectExtension/ObjectExtension.h"
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
// Defined below OoT_Game_Suspend, which calls it (#750).
void OoT_RetireAbandonedSession(void);
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
 * Author a debug save in-place and enter Play directly at `entrance`. Mirrors
 * the operator's map-select flow — Select_LoadGame (z_select.c) and the boot
 * branch of Enhancements/Warping.cpp Warp() — so the test runs on the same
 * kind of full-inventory save + Play_Init entry the manual repro uses.
 *
 * Shared by every integration row that needs live gameplay without a Start
 * press: called from the title screen's OnZTitleUpdate (which ticks every
 * frame of the boot logo, unattended) or, belt-and-braces, from
 * OnPresentFileSelect. `tag` prefixes the log line. Returns false (and does
 * nothing) when there is no GameState to redirect.
 */
static bool IntInjectDebugSaveAndEnterPlay(GameState* gameState, uint16_t entrance, bool adult, const char* tag,
                                           const char* from) {
    if (gameState == NULL) {
        return false;
    }
    fprintf(stderr, "[%s] injecting debug save at %s; entering Play at entrance 0x%04X\n", tag, from, entrance);
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
    gSaveContext.linkAge = adult ? LINK_AGE_ADULT : LINK_AGE_CHILD;
    gSaveContext.nightFlag = 0;
    gSaveContext.dayTime = 0x8000;
    gSaveContext.skyboxTime = 0x8000;
    gSaveContext.entranceIndex = entrance;
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.showTitleCard = true;
    gSaveContext.respawnFlag = 0;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = ENTR_LOAD_OPENING;

    gameState->running = false;
    SET_NEXT_GAMESTATE(gameState, OoT_Play_Init, PlayState);
    GameInteractor_ExecuteOnLoadGame(gSaveContext.fileNum);
    return true;
}

// The gameplay round-trip's boot step: the configured entrance and age.
static void GpInjectDebugSaveAndEnterPlay(GameState* gameState, const char* from) {
    const GameplayTestConfig* cfg = IntegrationTest_GetGameplayConfig();
    if (!IntInjectDebugSaveAndEnterPlay(gameState, cfg->bootEntrance, cfg->bootAdult != 0, "GP-TEST", from)) {
        IntegrationTest_GameplayFail("no GameState available for debug-save injection");
        return;
    }
    IntegrationTest_SetGameplayPhase(GP_PHASE_OOT_PRE);
}

// ============================================================================
// T1 (#260, #544): Happy Mask Shop -> MM, fired from live gameplay
// ============================================================================
//
// #544: this row used to fire from OnPresentFileSelect, whose only dispatch is
// FileChoose_FinishFadeIn (z_file_choose.c) — reached only after a Start press
// at the title, which no unattended run sends. OoT settled into the attract
// demo loop (cutsceneIndex 0xfff3/0xfff2) until the CTest wall killed it, so
// the row never executed one of its assertions. It now reaches gameplay the
// way IntGameplayRoundtrip does — a debug save injected from the title screen,
// Play entered directly outside the Happy Mask Shop — fires the HMS entrance
// after a window of live gameplay frames, and, when an OoT stage before the
// trigger stalls while OoT keeps ticking frames (the #544 attract-demo state),
// fails with the stage it was stuck in on a per-stage wall-clock budget instead
// of hitting the wall silently.
//
// What this does NOT cover: the budget is checked from OoT's
// OnGameStateMainStart, not from a thread, so (a) a wedge inside one OoT frame
// (e.g. a hang in Play_Init after the injection) never reaches the check, and
// (b) once the row is T1_STAGE_TRIGGERED the check stops, and the OoT->MM
// hand-off (suspend, archive swap, MM_Game_Init) and the MM half have only
// MM's frame-counted scene-load watchdog. Those still end at the CTest
// timeout; a wall-clock watchdog thread is the fix, not done here.
typedef enum {
    T1_STAGE_BOOT,          // waiting for the title screen (or file select) to inject the debug save
    T1_STAGE_ENTERING_PLAY, // debug save injected; waiting for OoT's scene init at kT1BootEntrance
    T1_STAGE_GAMEPLAY,      // scene built; counting live gameplay frames before the trigger
    T1_STAGE_TRIGGERED,     // HMS entrance fired and routing asserted; MM's side owns the rest
    T1_STAGE_FAILED,
} T1Stage;

static T1Stage sT1Stage = T1_STAGE_BOOT;
static int sT1GameplayFrames = 0;
static std::chrono::steady_clock::time_point sT1StageStart{};
// Right outside the Happy Mask Shop: where a player stands before walking in.
static const uint16_t kT1BootEntrance = OOT_ENTR_MARKET_FROM_MASK_SHOP;
static const int kT1GameplayFramesBeforeTrigger = 20;
// Per stage, wall clock, checked once per OoT frame. One stalled stage fails
// the row, so a stall is reported at about 30 s plus the time spent in earlier
// stages (a healthy run spends under a second in each). It bounds only OoT
// stages that keep ticking frames; see the block comment above for what still
// reaches the 120 s REDSHIP_INTEGRATION_TEST_TIMEOUT.
static const int kT1StageBudgetSecs = 30;

static const char* T1StageName(T1Stage stage) {
    switch (stage) {
        case T1_STAGE_BOOT:
            return "boot";
        case T1_STAGE_ENTERING_PLAY:
            return "entering-play";
        case T1_STAGE_GAMEPLAY:
            return "gameplay";
        case T1_STAGE_TRIGGERED:
            return "triggered";
        case T1_STAGE_FAILED:
            return "failed";
    }
    return "unknown";
}

static double T1SecondsInStage(void) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - sT1StageStart).count();
}

static void T1SetStage(T1Stage stage) {
    fprintf(stderr, "[OoT-INT-TEST] T1 stage %s -> %s (%.1f s in %s)\n", T1StageName(sT1Stage), T1StageName(stage),
            T1SecondsInStage(), T1StageName(sT1Stage));
    fflush(stderr);
    sT1Stage = stage;
    sT1StageStart = std::chrono::steady_clock::now();
}

// Fail loudly with the stage and enough state to attribute the wedge from the
// log alone. RequestExit does not set the pass flag; Combo_RequestGameSwitch
// makes Combo_CheckHotSwap return true so OoT's frame loop actually hands back
// to main, which sees the exit request before any switch.
static void T1Fail(const char* why) {
    PlayState* play = OoT_gPlayState;
    fprintf(stderr,
            "[OoT-INT-TEST] FAIL (int-switch-oot-hms-to-mm): %s [stage=%s after %.1f s; gameMode=%d "
            "entranceIndex=0x%04X cutsceneIndex=0x%04X gPlayState=%p sceneNum=%d]\n",
            why, T1StageName(sT1Stage), T1SecondsInStage(), (int)gSaveContext.gameMode,
            (uint16_t)gSaveContext.entranceIndex, (uint16_t)gSaveContext.cutsceneIndex, (void*)play,
            play != NULL ? (int)play->sceneNum : -1);
    fflush(stderr);
    sT1Stage = T1_STAGE_FAILED;
    IntegrationTest_RequestExit();
    Combo_RequestGameSwitch();
}

static void T1InjectFrom(GameState* gameState, const char* from) {
    if (sT1Stage != T1_STAGE_BOOT) {
        return;
    }
    if (!IntInjectDebugSaveAndEnterPlay(gameState, kT1BootEntrance, false, "OoT-INT-TEST", from)) {
        T1Fail("no GameState available for debug-save injection");
        return;
    }
    T1SetStage(T1_STAGE_ENTERING_PLAY);
}

// Fire the HMS entrance and assert where it routes. Same call OoT's z_play.c
// makes when the player walks into the Happy Mask Shop door — minus the
// SaveContext freeze, which IntGameplayRoundtrip's production door covers.
static void T1TriggerHmsAndAssertRouting(void) {
    fprintf(stderr, "[OoT-INT-TEST] gameplay reached (scene %d, %d live frames); triggering HMS entrance 0x%04X\n",
            OoT_gPlayState != NULL ? (int)OoT_gPlayState->sceneNum : -1, sT1GameplayFrames, OOT_ENTR_HAPPY_MASK_SHOP);
    fflush(stderr);

    Combo_CheckCrossGameEntrance("oot", OOT_ENTR_HAPPY_MASK_SHOP);

    if (!Combo_IsCrossGameSwitch()) {
        T1Fail("HMS entrance did not register a cross-game switch");
        return;
    }

    const char* target = Combo_GetSwitchTargetGameId();
    uint16_t targetEntrance = Combo_GetSwitchTargetEntrance();

    if (!target || strcmp(target, "mm") != 0) {
        char msg[96];
        snprintf(msg, sizeof(msg), "target should be 'mm', got '%s'", target ? target : "(null)");
        T1Fail(msg);
        return;
    }

    if (targetEntrance != MM_ENTR_SOUTH_CLOCK_TOWN_0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "target entrance should be 0x%04X (South Clock Town tower exit), got 0x%04X",
                 MM_ENTR_SOUTH_CLOCK_TOWN_0, targetEntrance);
        T1Fail(msg);
        return;
    }

    fprintf(stderr, "[OoT-INT-TEST] PASS leg 1: HMS routes to MM 0x%04X; main loop will run the switch\n",
            targetEntrance);
    fflush(stderr);
    T1SetStage(T1_STAGE_TRIGGERED);
    // Intentionally NOT signaling boot complete here. OoT's frame loop sees
    // the pending cross-game switch in Combo_CheckHotSwap and hands off to MM;
    // the MM-side hook signals the final pass once South Clock Town is loaded
    // and stable (and has its own scene-load watchdog).
}

// ============================================================================
// int-paired-first-crossing (the gameplay round trip's paired variant)
// ============================================================================

extern "C" {
void OoT_Sram_InitSave(FileChooseContext* fileChooseCtx);
void OoT_Sram_OpenSave(void);
// ovl_file_choose: the file select gamestate (SoH's Boot Sequence: File Select target).
void FileChoose_Init(GameState* thisx);
u32 Save_Exist(int fileNum);
void Save_DeleteFile(int fileNum);
SaveFileMetaInfo* Save_GetSaveMetaInfo(int fileNum);
// 3drando/menu.cpp: the synchronous generation the rando-tier creation rows use.
int Rando_HeadlessSeedTest(const char* seedStr);
// OTRGlobals.h declares these inside its C-only block.
uint32_t Randomizer_GetCurrentWorldSeed(void);
// games/mm/2s2h/GameExports_SingleExe.cpp: MM's generation dispatch count.
uint32_t MM_Rando_OnSaveInitDispatchCount(void);
}

// The slot the paired row writes: the THIRD file (fileNum 2), named "RSBSTEST"
// in OoT's NTSC filename charset. The name is also the row's ownership mark: a
// slot holding a file by any other name is refused, never erased, so running the
// row in a build directory somebody plays in cannot destroy their save.
static const int kPfcSlot = 2;
static const uint8_t kPfcName[8] = { 0xBC, 0xBD, 0xAC, 0xBD, 0xBE, 0xAF, 0xBD, 0xBE };
// The ComboSingleBag / mm-creation-new-file-world pinned seed.
static const char* const kPfcSeed = "RSBSSINGLEBAG1";
// OoT-only sentinel written into the live OoT half just before the Happy Mask
// Shop door; the return leg must find it (the half was RESTORED from the frozen
// state, not reloaded from the slot or regenerated). OoT's death counter is not
// a shared cross-game resource, and the slot on disk holds 0.
static const u16 kPfcDeathsSentinel = 777;
static uint32_t sPfcOoTWorldSeed = 0;

static bool PfcNoRefusalToast(char* msg, size_t cap) {
    const int toasts = OoT_Notification_EmittedCountForTest();
    for (int i = 0; i < toasts; i++) {
        char toast[256];
        if (OoT_Notification_EmittedAtForTest(i, toast, sizeof(toast)) &&
            (strstr(toast, "Not paired") != nullptr || strstr(toast, "Not saved") != nullptr)) {
            snprintf(msg, cap, "a refusal toast was raised (toast %d of %d): \"%s\"", i + 1, toasts, toast);
            return false;
        }
    }
    return true;
}

/**
 * "Shipped defaults" means NOTHING the generation reads was chosen: every
 * option, trick and exclusion CVar of all three surfaces is unset, so each one
 * resolves to its compiled default. Rando_HeadlessSeedTest reads the live CVar
 * store (the build directory's persisted config), so a config that carries any
 * randomizer choice would generate a different world under the same log line.
 * Returns the number of explicit settings found and names the first few in
 * `msg`; 0 is the only passing answer.
 */
static int PfcExplicitWorldSettings(char* msg, size_t cap) {
    int found = 0;
    std::string names;
    auto note = [&](const std::string& cvar) {
        if (found < 4) {
            names += names.empty() ? cvar : ", " + cvar;
        }
        found++;
    };
    auto checkInt = [&](const std::string& cvar) {
        if (!cvar.empty() && Combo_CVarIsExplicitInt(cvar.c_str())) {
            note(cvar);
        }
    };
    // OoT: the option set Rando_HeadlessSeedTest builds (CreateOptions is what
    // it calls first; calling it here as well changes nothing it reads).
    auto settings = Rando::Settings::GetInstance();
    settings->CreateOptions();
    int ootOptions = 0;
    for (const auto& option : settings->GetAllOptions()) {
        ootOptions += option.GetCVarName().empty() ? 0 : 1;
        checkInt(option.GetCVarName());
    }
    for (int i = 0; i < RT_MAX; i++) {
        checkInt(settings->GetTrickOption(static_cast<RandomizerTrick>(i)).GetCVarName());
    }
    for (int i = 0; i < RC_MAX; i++) {
        Rando::Location* loc = Rando::StaticData::GetLocation(static_cast<RandomizerCheck>(i));
        if (loc != nullptr && loc->GetExcludedOption() != nullptr) {
            checkInt(loc->GetExcludedOption()->GetCVarName());
        }
    }
    for (const char* list : { CVAR_RANDOMIZER_SETTING("ExcludedLocations"), CVAR_RANDOMIZER_SETTING("EnabledTricks"),
                              CVAR_RANDOMIZER_SETTING("EnabledGlitches") }) {
        const char* value = CVarGetString(list, "");
        if (value != nullptr && value[0] != '\0') {
            note(list);
        }
    }
    // MM: the descriptor tables the options and tricks pages register (both
    // idempotent; re-registering the same table is silent).
    if (Combo_MMOptionCount() == 0) {
        MM_RandoOptionsUi_Register();
    }
    if (Combo_MMTrickCount() == 0) {
        MM_RandoTricksUi_Register();
    }
    for (int i = 0; i < Combo_MMOptionCount(); i++) {
        const ComboMMOptionDesc* desc = Combo_MMOptionAt(i);
        if (desc != nullptr && desc->cvar != nullptr) {
            checkInt(desc->cvar);
        }
    }
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* desc = Combo_MMTrickAt(i);
        if (desc != nullptr && desc->cvar != nullptr) {
            checkInt(desc->cvar);
        }
    }
    // The combo settings.
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        int32_t value = 0;
        if (Combo_ComboSettingReadStore((ComboSettingId)i, &value)) {
            note(Combo_ComboSettingKey((ComboSettingId)i));
        }
    }
    if (ootOptions == 0 || Combo_MMOptionCount() == 0 || Combo_MMTrickCount() == 0) {
        // A surface with no descriptors would pass vacuously.
        snprintf(
            msg, cap,
            "the defaults check has nothing to check on some surface (OoT options %d, MM options %d, MM tricks %d)",
            ootOptions, Combo_MMOptionCount(), Combo_MMTrickCount());
        return -1;
    }
    snprintf(msg, cap,
             "%d explicit (%s%s); checked %d OoT options, %d OoT tricks, %d OoT exclusions, %d MM options, "
             "%d MM tricks, %d combo settings",
             found, names.empty() ? "none" : names.c_str(), found > 4 ? ", ..." : "", ootOptions, (int)RT_MAX,
             (int)RC_MAX, Combo_MMOptionCount(), Combo_MMTrickCount(), (int)COMBO_SETTING_COUNT);
    return found;
}

/**
 * The creation's own account of the crossings, read off the lines it printed,
 * against the live store right after OoT_Sram_InitSave returned (before any
 * load): "[OoT] creation event: single bag — N crossings stored (X OoT items in
 * Termina, Y MM items in Hyrule)" and "[Crossings] captured A OoT-hosted and B
 * MM-hosted crossings (digest D)". Returns false with the reason in `msg`.
 */
static bool PfcCreationMatchesStore(char* msg, size_t cap) {
    char bagLine[512];
    char capLine[512];
    int total = -1;
    int intoTermina = -1;
    int intoHyrule = -1;
    if (!IntegrationTest_StderrCaptureLast("[OoT] creation event: single bag", bagLine, sizeof(bagLine))) {
        snprintf(msg, cap, "the creation never logged its single-bag line");
        return false;
    }
    const char* p = strstr(bagLine, "single bag");
    while (*p != '\0' && (*p < '0' || *p > '9')) {
        p++;
    }
    if (sscanf(p, "%d crossings stored (%d OoT items in Termina, %d MM items in Hyrule)", &total, &intoTermina,
               &intoHyrule) != 3) {
        snprintf(msg, cap, "the creation's single-bag line does not parse: \"%s\"", bagLine);
        return false;
    }
    int capOoT = -1;
    int capMM = -1;
    unsigned capDigest = 0;
    const char* c = nullptr;
    if (IntegrationTest_StderrCaptureLast("[Crossings] captured", capLine, sizeof(capLine))) {
        c = strstr(capLine, "captured ");
    }
    if (c == nullptr ||
        sscanf(c, "captured %d OoT-hosted and %d MM-hosted crossings (digest %X)", &capOoT, &capMM, &capDigest) != 3) {
        snprintf(msg, cap, "the crossing store's capture line is missing or does not parse: \"%s\"",
                 c != nullptr ? capLine : "(none)");
        return false;
    }
    const int storeHyrule = Combo_Crossings_Count(GAME_OOT);
    const int storeTermina = Combo_Crossings_Count(GAME_MM);
    const unsigned storeDigest = (unsigned)Combo_Crossings_Digest();
    if (total <= 0 || total != intoTermina + intoHyrule || intoTermina != storeTermina || intoHyrule != storeHyrule ||
        capOoT != storeHyrule || capMM != storeTermina || capDigest != storeDigest) {
        snprintf(msg, cap,
                 "the creation's crossings disagree with the store it left: bag line %d = %d into Termina + %d into "
                 "Hyrule; capture line %d OoT-hosted, %d MM-hosted, digest %08X; store inHyrule=%d inTermina=%d "
                 "digest %08X",
                 total, intoTermina, intoHyrule, capOoT, capMM, capDigest, storeHyrule, storeTermina, storeDigest);
        return false;
    }
    snprintf(msg, cap, "%d crossings (%d OoT items in Termina, %d MM items in Hyrule), digest %08X", total, intoTermina,
             intoHyrule, storeDigest);
    return true;
}

static bool sPfcCreationAttempted = false;

/**
 * The paired variant's boot: the production path a player takes, minus the menu
 * clicks. It runs from the REAL file select (FileChoose_Main's OnFileChooseMain,
 * with its own FileChooseContext), which the title reaches the way SoH's "Boot
 * Sequence: File Select" setting takes it there (GpPairedTitleToFileSelect), so
 * Title_Destroy's Sram_InitSram (SaveManager::Init: the Save directory, the slot
 * metadata) has run before anything is created, exactly as in play.
 *
 *   1. GENERATE the pinned paired world on the shipped defaults (no config beyond
 *      the OpenGL backend): Rando_HeadlessSeedTest(RSBSSINGLEBAG1), the same
 *      call and seed as the mm-creation-new-file-world row, then the
 *      seed-generated flag the menu's RandoMain::GenerateRando sets.
 *   2. CREATE the file through OoT's own new-file seam, OoT_Sram_InitSave (what
 *      the naming screen's "END" calls, z_file_nameset_NES.c): session retire,
 *      the slot arm, THE CREATION EVENT (OoT_Creation_AuthorRandoFile: MM's half
 *      authored and armed, the single bag, the crossings stored, the spoiler),
 *      Randomizer_InitSaveFile, and the slot's first write (.sav + .redsave).
 *   3. LOAD that file the way the file select does (FileChoose_LoadGame):
 *      Sram_OpenSave (Save_LoadFile, whose OnLoadFile reads the .redsave back:
 *      the frozen rules, the crossing store, the armed MM half), the same field
 *      resets, then OnLoadGame.
 *   4. PLACE the loaded file at the round trip's boot entrance (Market, 0x01D1,
 *      child, noon: the same spawn the debug-save repro uses) and enter Play.
 *
 * So the session plays slot 3's OoT half AS LOADED BACK FROM DISK, and MM's first
 * arrival hydrates the MM half the load re-armed from the .redsave.
 */
static void GpCreatePairedFileAndEnterPlay(FileChooseContext* fileChoose, const char* from) {
    const GameplayTestConfig* cfg = IntegrationTest_GetGameplayConfig();
    char msg[512];
    if (fileChoose == NULL) {
        IntegrationTest_GameplayFail("no file-select GameState available for the paired creation");
        return;
    }
    GameState* gameState = &fileChoose->state;
    // Runs at most once: the file select's main keeps calling the boot
    // injection while the phase is still BOOT, and a failure below leaves it
    // there until the exit request lands. A second pass would erase the slot
    // the first one wrote (a failure keeps it for forensics) and generate again.
    if (sPfcCreationAttempted) {
        static bool sReentryLogged = false;
        if (!sReentryLogged) {
            sReentryLogged = true;
            fprintf(stderr, "[PFC] the file select re-entered the boot injection after the creation attempt; "
                            "not running it again\n");
            fflush(stderr);
        }
        return;
    }
    sPfcCreationAttempted = true;

    // ---- 0. the slot: empty, or this row's own file from an earlier run -----
    if (Save_Exist(kPfcSlot)) {
        const SaveFileMetaInfo* meta = Save_GetSaveMetaInfo(kPfcSlot);
        if (meta == NULL || memcmp(meta->playerName, kPfcName, sizeof(kPfcName)) != 0) {
            snprintf(msg, sizeof(msg),
                     "file slot %d already holds a save this row did not create; refusing to erase it (run the row "
                     "in a build directory without that save)",
                     kPfcSlot + 1);
            IntegrationTest_GameplayFail(msg);
            return;
        }
        fprintf(stderr, "[PFC] erasing this row's own file from an earlier run (slot %d)\n", kPfcSlot + 1);
        Save_DeleteFile(kPfcSlot);
    }

    // ---- 1. generate ---------------------------------------------------------
    // "The shipped defaults" is checked, not assumed: generation reads the live
    // CVar store, which here is the build directory's persisted config.
    const int explicitSettings = PfcExplicitWorldSettings(msg, sizeof(msg));
    if (explicitSettings != 0) {
        char reason[640];
        snprintf(reason, sizeof(reason),
                 "the world would not be generated on the shipped defaults: %s (run the row with a config that "
                 "carries no randomizer, MM or combo setting)",
                 msg);
        IntegrationTest_GameplayFail(reason);
        return;
    }
    fprintf(stderr, "[PFC] shipped defaults verified: %s\n", msg);
    fprintf(stderr, "[PFC] generating the pinned paired world %s on the shipped defaults at %s\n", kPfcSeed, from);
    fflush(stderr);
    if (Rando_HeadlessSeedTest(kPfcSeed) != 0) {
        IntegrationTest_GameplayFail("the pinned paired world did not generate");
        return;
    }
    OTRGlobals::Instance->gRandoContext->SetSeedGenerated(true);
    if (!Combo_ForeignPairingActive()) {
        IntegrationTest_GameplayFail("generation on the shipped defaults published no cross-game pairing identity");
        return;
    }

    // ---- 2. create, through the naming screen's seam -------------------------
    // What the naming screen leaves set when the player confirms a name on file
    // 3 with the Randomizer quest selected (z_file_nameset_NES.c): the button,
    // the quest, the typed name in the slot's metadata, the slot as fileNum.
    fileChoose->buttonIndex = kPfcSlot;
    fileChoose->questType[kPfcSlot] = QUEST_RANDOMIZER;
    fileChoose->n64ddFlag = 0;
    memcpy(Save_GetSaveMetaInfo(kPfcSlot)->playerName, kPfcName, sizeof(kPfcName));
    gSaveContext.fileNum = kPfcSlot;
    const u16 dayTime = gSaveContext.dayTime;
    fprintf(stderr, "[PFC] creating file %d through OoT_Sram_InitSave (the production creation event)\n", kPfcSlot + 1);
    fflush(stderr);
    OoT_Sram_InitSave(fileChoose);
    gSaveContext.dayTime = dayTime;
    char line[512];
    snprintf(msg, sizeof(msg), "[OoT] creation event: slot %d complete", kPfcSlot);
    if (!Save_Exist(kPfcSlot) || !RsbsSave_HasSave(kPfcSlot) ||
        !IntegrationTest_StderrCaptureLast(msg, line, sizeof(line))) {
        snprintf(msg, sizeof(msg),
                 "the creation did not write a complete paired file (slot %d: .sav %s, .redsave %s, creation-complete "
                 "line %s)",
                 kPfcSlot + 1, Save_Exist(kPfcSlot) ? "written" : "MISSING",
                 RsbsSave_HasSave(kPfcSlot) ? "written" : "MISSING",
                 IntegrationTest_StderrCaptureCount("creation event: slot") > 0 ? "absent" : "never logged");
        IntegrationTest_GameplayFail(msg);
        return;
    }
    fprintf(stderr, "[PFC] created: \"%s\"\n", line);

    // ---- the identity AS CREATED, before anything is loaded back ---------------
    // The baseline every later check compares against is the creation's, not
    // the load's: a .redsave round trip that lost or reordered a crossing, or
    // changed a digest, must fail here rather than become the baseline.
    if (!PfcCreationMatchesStore(msg, sizeof(msg))) {
        IntegrationTest_GameplayFail(msg);
        return;
    }
    fprintf(stderr, "[PFC] creation's crossings = the store it left: %s\n", msg);
    if (!Combo_ForeignPairingActive() || gComboCtx.sharedRandoSeed == 0 || gComboCtx.mmProfileDigest == 0 ||
        gComboCtx.comboSettingsHash == 0) {
        snprintf(msg, sizeof(msg),
                 "the creation left no complete paired identity (paired=%d masterSeed=%u mmProfileDigest=%08X "
                 "comboFingerprint=%08X)",
                 Combo_ForeignPairingActive() ? 1 : 0, (unsigned)gComboCtx.sharedRandoSeed,
                 (unsigned)gComboCtx.mmProfileDigest, (unsigned)gComboCtx.comboSettingsHash);
        IntegrationTest_GameplayFail(msg);
        return;
    }
    IntegrationTest_PairedIdentityRecord();

    // ---- 3. load it back, as the file select does -----------------------------
    gSaveContext.fileNum = kPfcSlot;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    OoT_Sram_OpenSave();
    gameState->running = false;
    SET_NEXT_GAMESTATE(gameState, OoT_Play_Init, PlayState);
    gSaveContext.respawn[0].entranceIndex = ENTR_LOAD_OPENING;
    gSaveContext.respawnFlag = 0;
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.showTitleCard = true;
    gSaveContext.timerState = TIMER_STATE_OFF;
    gSaveContext.subTimerState = SUBTIMER_STATE_OFF;
    gSaveContext.eventInf[0] = 0;
    gSaveContext.eventInf[1] = 0;
    gSaveContext.eventInf[2] = 0;
    gSaveContext.eventInf[3] = 0;
    gSaveContext.unk_13EE = 0x32;
    gSaveContext.nayrusLoveTimer = 0;
    gSaveContext.healthAccumulator = 0;
    gSaveContext.magicState = MAGIC_STATE_IDLE;
    gSaveContext.prevMagicState = MAGIC_STATE_IDLE;
    gSaveContext.forcedSeqId = NA_BGM_GENERAL_SFX;
    gSaveContext.skyboxTime = 0;
    gSaveContext.nextTransitionType = TRANS_NEXT_TYPE_DEFAULT;
    gSaveContext.nextCutsceneIndex = 0xFFEF;
    gSaveContext.cutsceneTrigger = 0;
    gSaveContext.chamberCutsceneNum = 0;
    gSaveContext.nextDayTime = 0xFFFF;
    gSaveContext.retainWeatherMode = 0;
    for (int buttonIndex = 0; buttonIndex < ARRAY_COUNT(gSaveContext.buttonStatus); buttonIndex++) {
        gSaveContext.buttonStatus[buttonIndex] = BTN_ENABLED;
    }
    gSaveContext.forceRisingButtonAlphas = 0;
    gSaveContext.unk_13E8 = 0;
    gSaveContext.unk_13EA = 0;
    gSaveContext.unk_13EC = 0;
    gSaveContext.magicCapacity = 0;
    gSaveContext.magicFillTarget = gSaveContext.magic;
    gSaveContext.magic = 0;
    gSaveContext.magicLevel = gSaveContext.magic;
    gSaveContext.naviTimer = 0;
    GameInteractor_ExecuteOnLoadGame(gSaveContext.fileNum);

    // ---- 4. the spawn the round trip boots into -------------------------------
    gSaveContext.linkAge = LINK_AGE_CHILD;
    gSaveContext.sceneSetupIndex = 0;
    gSaveContext.cutsceneIndex = 0;
    gSaveContext.nightFlag = 0;
    gSaveContext.dayTime = 0x8000;
    gSaveContext.skyboxTime = 0x8000;
    gSaveContext.entranceIndex = cfg->bootEntrance;

    // ---- the loaded file is the paired world the creation authored ------------
    if (!IS_RANDO || !Combo_ForeignPairingActive() || !Context_HasFrozenState(GAME_MM) || !Combo_Crossings_IsFrozen() ||
        Combo_Crossings_Count(GAME_OOT) + Combo_Crossings_Count(GAME_MM) == 0) {
        snprintf(msg, sizeof(msg),
                 "the loaded file is not the paired world the creation wrote (rando=%d paired=%d mmHalfArmed=%d "
                 "crossingStoreFrozen=%d inHyrule=%d inTermina=%d)",
                 IS_RANDO ? 1 : 0, Combo_ForeignPairingActive() ? 1 : 0, Context_HasFrozenState(GAME_MM) ? 1 : 0,
                 Combo_Crossings_IsFrozen() ? 1 : 0, Combo_Crossings_Count(GAME_OOT), Combo_Crossings_Count(GAME_MM));
        IntegrationTest_GameplayFail(msg);
        return;
    }
    if (!PfcNoRefusalToast(msg, sizeof(msg))) {
        IntegrationTest_GameplayFail(msg);
        return;
    }
    char diff[512];
    if (!IntegrationTest_PairedIdentityMatches(diff, sizeof(diff))) {
        snprintf(msg, sizeof(msg), "the file loaded back from disk is not the world the creation authored: %s", diff);
        IntegrationTest_GameplayFail(msg);
        return;
    }
    fprintf(stderr, "[PFC] loaded identity = the creation's (masterSeed, settingsHash, mmProfileDigest, "
                    "comboFingerprint, crossing counts and digest)\n");
    sPfcOoTWorldSeed = Randomizer_GetCurrentWorldSeed();
    IntegrationTest_PairedSetMMGenerationBaseline(MM_Rando_OnSaveInitDispatchCount());
    fprintf(stderr,
            "[PFC] session plays file %d (slot %d) as loaded back from disk: OoT world seed %u, entering Play at "
            "entrance 0x%04X; MM generation dispatches so far %u (the creation's)\n",
            kPfcSlot + 1, kPfcSlot, (unsigned)sPfcOoTWorldSeed, cfg->bootEntrance,
            (unsigned)MM_Rando_OnSaveInitDispatchCount());
    fflush(stderr);
    IntegrationTest_SetGameplayPhase(GP_PHASE_OOT_PRE);
}

static bool sPfcTitleRedirected = false;

/**
 * The paired variant's title: straight to the file select, as SoH's "Boot
 * Sequence: File Select" option does it (CustomLogoTitle.cpp,
 * OnZTitleUpdateSkipToFileSelect). Title_Destroy runs on the way out, and with
 * it Sram_InitSram.
 */
static void GpPairedTitleToFileSelect(GameState* gameState) {
    if (sPfcTitleRedirected || gameState == NULL) {
        return;
    }
    sPfcTitleRedirected = true;
    fprintf(stderr, "[PFC] title -> file select (SoH's Boot Sequence: File Select path)\n");
    fflush(stderr);
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.gameMode = GAMEMODE_FILE_SELECT;
    gameState->running = false;
    SET_NEXT_GAMESTATE(gameState, FileChoose_Init, FileChooseContext);
}

/**
 * The boot injection, by variant: the debug save (the round trip, and the
 * paired row's red half), or the paired creation, which needs the file select.
 * `isFileSelect` says which gamestate `gameState` is.
 */
static void GpBootInject(GameState* gameState, const char* from, bool isFileSelect) {
    if (IntegrationTest_PairedFirstCrossing() && !IntegrationTest_PairedSkipCreation()) {
        if (isFileSelect) {
            GpCreatePairedFileAndEnterPlay((FileChooseContext*)gameState, from);
        } else {
            GpPairedTitleToFileSelect(gameState);
        }
    } else {
        GpInjectDebugSaveAndEnterPlay(gameState, from);
    }
}

/**
 * The paired variant's return leg (OoT scene init, Market from the Mask Shop):
 * OoT's half came back RESTORED from the frozen state, not reloaded or
 * regenerated, under the same identity. Returns false after failing the run.
 */
static bool GpPairedCheckReturn(void) {
    char msg[768];
    char diff[512];
    if (!IS_RANDO || gSaveContext.fileNum != kPfcSlot) {
        snprintf(msg, sizeof(msg), "the return leg's OoT half is not the created file (rando=%d fileNum=%d)",
                 IS_RANDO ? 1 : 0, (int)gSaveContext.fileNum);
        IntegrationTest_GameplayFail(msg);
        return false;
    }
    const uint32_t worldSeed = Randomizer_GetCurrentWorldSeed();
    if (worldSeed != sPfcOoTWorldSeed) {
        snprintf(msg, sizeof(msg), "OoT's world changed across the crossing (seed %u -> %u): regenerated",
                 (unsigned)sPfcOoTWorldSeed, (unsigned)worldSeed);
        IntegrationTest_GameplayFail(msg);
        return false;
    }
    if (gSaveContext.deaths != kPfcDeathsSentinel) {
        snprintf(msg, sizeof(msg),
                 "OoT's half was not restored from the frozen state: the sentinel written before the Happy Mask Shop "
                 "door (deaths=%u) came back as %u",
                 (unsigned)kPfcDeathsSentinel, (unsigned)gSaveContext.deaths);
        IntegrationTest_GameplayFail(msg);
        return false;
    }
    if (!IntegrationTest_PairedIdentityMatches(diff, sizeof(diff))) {
        snprintf(msg, sizeof(msg), "the paired identity changed across the round trip: %s", diff);
        IntegrationTest_GameplayFail(msg);
        return false;
    }
    if (MM_Rando_OnSaveInitDispatchCount() != IntegrationTest_PairedMMGenerationBaseline()) {
        snprintf(msg, sizeof(msg), "MM's generation dispatch ran during the round trip (%u -> %u)",
                 (unsigned)IntegrationTest_PairedMMGenerationBaseline(), (unsigned)MM_Rando_OnSaveInitDispatchCount());
        IntegrationTest_GameplayFail(msg);
        return false;
    }
    char line[512];
    if (IntegrationTest_StderrCaptureLast("REFUSED", line, sizeof(line))) {
        snprintf(msg, sizeof(msg), "a refusal was logged during the round trip: \"%s\"", line);
        IntegrationTest_GameplayFail(msg);
        return false;
    }
    if (!PfcNoRefusalToast(msg, sizeof(msg))) {
        IntegrationTest_GameplayFail(msg);
        return false;
    }
    char desc[256];
    IntegrationTest_PairedIdentityDescribe(IntegrationTest_PairedIdentityRecorded(), desc, sizeof(desc));
    fprintf(stderr,
            "[PFC] return leg PASS: OoT's half restored, not regenerated (file %d, OoT world seed %u, sentinel "
            "deaths=%u survived), same identity: %s\n",
            kPfcSlot + 1, (unsigned)worldSeed, (unsigned)gSaveContext.deaths, desc);
    fflush(stderr);
    return true;
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
        // T1 (#260, #544): reach live OoT gameplay without a Start press, fire
        // the Happy Mask Shop entrance from there, assert the cross-game switch
        // resolves to MM South Clock Town (the tower-exit arrival). Leg 1 passes
        // when routing is verified here; the final pass is signaled from the
        // MM-side hook after MM stabilizes post-switch. Stage machine and
        // watchdog: T1Stage above.
        fprintf(stderr, "[OoT] Registering integration test hooks for HMS->MM switch (T1)\n");
        fflush(stderr);

        sT1Stage = T1_STAGE_BOOT;
        sT1GameplayFrames = 0;
        sT1StageStart = std::chrono::steady_clock::now();

        // Boot injection, whichever fires first (both are no-ops past
        // T1_STAGE_BOOT). The title hook ticks every frame of the boot logo, so
        // an unattended run always reaches it; file select is belt-and-braces
        // for a boot sequence that skips straight there.
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnZTitleUpdate>(
            [](void* gameState) { T1InjectFrom((GameState*)gameState, "title screen"); });
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPresentFileSelect>(
            []() { T1InjectFrom(OoT_gGameState, "file select"); });

        // Gameplay reached: OoT_Play_Init built the scene the debug save asked
        // for. Fires after the startup-entrance consumption, so entranceIndex
        // is final here.
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t sceneNum) {
            if (Context_GetCurrentGame() != GAME_OOT || sT1Stage != T1_STAGE_ENTERING_PLAY) {
                return; // shared hook storage guard (#344); only the injected arrival counts
            }
            uint16_t entrance = (uint16_t)gSaveContext.entranceIndex;
            fprintf(stderr, "[OoT-INT-TEST] OoT scene init: scene %d, entrance 0x%04X, gameMode %d\n", sceneNum,
                    entrance, (int)gSaveContext.gameMode);
            fflush(stderr);
            if (entrance != kT1BootEntrance) {
                char msg[112];
                snprintf(msg, sizeof(msg), "gameplay arrived at entrance 0x%04X, expected 0x%04X", entrance,
                         kT1BootEntrance);
                T1Fail(msg);
                return;
            }
            sT1GameplayFrames = 0;
            T1SetStage(T1_STAGE_GAMEPLAY);
        });

        // Frame driver + wall-clock budget check. Runs on every OoT gamestate's
        // frame (title, opening, file select, play), so a stage before the
        // trigger that stalls while frames keep ticking is caught here. A wedge
        // inside a frame never returns to this hook (see the T1 block comment).
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>([]() {
            if (Context_GetCurrentGame() != GAME_OOT) {
                return; // (#344) MM frames share this hook storage
            }
            if (sT1Stage == T1_STAGE_TRIGGERED || sT1Stage == T1_STAGE_FAILED) {
                return;
            }
            if (T1SecondsInStage() >= kT1StageBudgetSecs) {
                switch (sT1Stage) {
                    case T1_STAGE_BOOT:
                        T1Fail("title screen / file select never presented: the debug save was never injected");
                        break;
                    case T1_STAGE_ENTERING_PLAY:
                        T1Fail("gameplay never reached: no OoT scene init after the debug-save injection");
                        break;
                    default:
                        T1Fail("gameplay reached but never ran live frames (no PlayState / not GAMEMODE_NORMAL)");
                        break;
                }
                return;
            }
            if (sT1Stage != T1_STAGE_GAMEPLAY) {
                return;
            }
            PlayState* play = OoT_gPlayState;
            if (play == NULL || OoT_gGameState != &play->state || gSaveContext.gameMode != GAMEMODE_NORMAL ||
                GET_PLAYER(play) == NULL) {
                sT1GameplayFrames = 0;
                return;
            }
            if (++sT1GameplayFrames < kT1GameplayFramesBeforeTrigger) {
                return;
            }
            T1TriggerHmsAndAssertRouting();
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
            GpBootInject((GameState*)gameState, "title screen", false);
        });
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPresentFileSelect>([]() {
            if (IntegrationTest_GetGameplayPhase() != GP_PHASE_BOOT) {
                return;
            }
            GpBootInject(OoT_gGameState, "file select", true);
        });
        // The paired variant's creation runs from the file select's own main
        // (every frame, from the first one), not from OnPresentFileSelect, which
        // waits for a Start press no unattended run sends (#544).
        if (IntegrationTest_PairedFirstCrossing()) {
            // The "no refusal toast" checks read the test-only toast record,
            // which is off in every other process.
            OoT_Notification_RecordForTest(1);
        }
        if (IntegrationTest_PairedFirstCrossing() && !IntegrationTest_PairedSkipCreation()) {
            sPfcTitleRedirected = false;
            sPfcCreationAttempted = false;
            GameInteractor::Instance->RegisterGameHook<GameInteractor::OnFileChooseMain>([](void* gameState) {
                if (IntegrationTest_GetGameplayPhase() != GP_PHASE_BOOT) {
                    return;
                }
                GpBootInject((GameState*)gameState, "file select", true);
            });
        }

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
            // Frame-counter reset lock. The scene build runs after the
            // frame-counter block in OoT_Play_Init, so an arrival must land
            // here with a counter that was just zeroed. Left stale, the
            // suspended session's value (or MM's arena leftovers) rides into
            // the arrival and voids the interval autosave's `gameplayFrames <
            // 60` warm-up guard (soh/Enhancements/QoL/Autosave.cpp) — a save
            // can then fire during the arrival fade. Return leg only: the warp
            // and exit phases are ordinary in-game door transitions, where a
            // large counter is the correct session-long value.
            if (phase == GP_PHASE_OOT_RETURN && OoT_gPlayState != NULL) {
                fprintf(stderr, "[GP-TEST] return leg gameplayFrames=%u (expect 0)\n",
                        (unsigned)OoT_gPlayState->gameplayFrames);
                fflush(stderr);
                if (OoT_gPlayState->gameplayFrames != 0) {
                    char frameMsg[176];
                    snprintf(frameMsg, sizeof(frameMsg),
                             "return leg arrived with gameplayFrames=%u, expected 0 — the arrival frame-counter "
                             "reset regressed, so the autosave warm-up guard is void on re-arrivals",
                             (unsigned)OoT_gPlayState->gameplayFrames);
                    IntegrationTest_GameplayFail(frameMsg);
                    return;
                }
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
            if (phase == GP_PHASE_OOT_RETURN && IntegrationTest_PairedFirstCrossing() && !GpPairedCheckReturn()) {
                return;
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
                    if (IntegrationTest_PairedFirstCrossing()) {
                        // Frozen with the live save by the door's cross-game switch.
                        gSaveContext.deaths = kPfcDeathsSentinel;
                        fprintf(stderr, "[PFC] OoT-half sentinel armed before the door: deaths=%u\n",
                                (unsigned)kPfcDeathsSentinel);
                    }
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
                    if (IntegrationTest_PairedFirstCrossing() && !IntegrationTest_PairedSkipCreation()) {
                        // A pass leaves no file behind (a failure keeps it for
                        // forensics; the next run erases it by its name).
                        Save_DeleteFile(kPfcSlot);
                        fprintf(stderr, "[PFC] PASS: the paired first crossing and its return; erased file %d\n",
                                kPfcSlot + 1);
                        fflush(stderr);
                    }
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

    // The Play gamestate just retired never ran Play_Destroy, so none of its
    // actors were deleted: put back what their deletion would have (#750).
    OoT_RetireAbandonedSession();

    fprintf(stderr, "[OoT] Game_Suspend complete\n");
    fflush(stderr);
}

/**
 * Undo what an ABANDONED OoT Play session left behind (#750, the OoT leg of
 * #666; MM's twin is MM_RetireAbandonedSession).
 *
 * Every departure from OoT retires its Play gamestate without Play_Destroy
 * (OoT_Graph_ResetRunFrameContext in OoT_Game_Suspend; the next entry re-runs
 * Main(), which re-initializes the system arena the actors lived in). So
 * OoT_Actor_Delete never runs for the actors that were live at that instant,
 * and two things its epilogue does are otherwise never done:
 *
 *  - each actor's ActorDB entry loses a client (numLoaded--), and the last
 *    client out runs the entry's `reset` (OoT_Actor_FreeOverlay), which is how
 *    the port puts overlay file-scope statics back. Unlike MM, OoT never zeroes
 *    numLoaded again after the entry is created (MM's Actor_InitContext does
 *    on every Play_Init), so without this an overlay with a live actor at
 *    departure keeps a phantom client for the rest of the process: its reset
 *    never runs again, even on ordinary in-OoT scene changes, and every further
 *    departure adds another phantom.
 *  - ObjectExtension_Free(actor) drops the per-actor data SoH hangs off actor
 *    addresses (the rando check identity of a pot, crate, grass blade, tree,
 *    beehive, fairy or scrub, the actor-list index, enemy maximum health). The
 *    next session's arena reuses those addresses.
 *
 * Both are done here, at the point the session is abandoned. An entry with
 * clients is marked client-free and its reset runs once, which is everything
 * OoT_Actor_FreeOverlay does at zero clients apart from a debug print that
 * reads HREG(20) through gGameInfo (calling reset directly keeps this
 * independent of that system-arena allocation). Entries already at zero
 * clients are skipped: they were reset when their last client went, and
 * skipping them keeps this seam equal to what OoT_Actor_FreeOverlay would
 * have done for the abandoned clients (a reset runs only for an overlay that
 * loses its last client). Every reset in the tree today is idempotent (plain
 * assignments, memsets, loops of assignments), so the skip is about matching
 * FreeOverlay and staying correct for a future reset that is not, not a
 * present hazard. The actors' own Destroy functions are NOT run: they take
 * the PlayState of a gamestate that has already been retired (dynapoly,
 * colliders, lights, skeletons). Overlays whose Destroy is what restores a
 * static got that restore in a reset of their own (the [RSBS #750] blocks
 * under games/oot/src/overlays/actors).
 *
 * The oot-abandoned-session-statics row (oot_abandoned_session_test.cpp) drives
 * OoT_GetGameOps()->suspend, so it fails if this call leaves OoT_Game_Suspend
 * or moves ahead of OoT_Graph_ResetRunFrameContext.
 */
void OoT_RetireAbandonedSession(void) {
    s32 overlays = 0;

    if (ActorDB::Instance != nullptr) {
        const int count = ActorDB::Instance->GetEntryCount();
        for (int i = 0; i < count; i++) {
            ActorDBEntry* entry = &ActorDB::Instance->RetrieveEntry(i).entry;
            if (entry->numLoaded <= 0) {
                continue;
            }
            entry->numLoaded = 0;
            if (entry->reset != NULL) {
                entry->reset();
            }
            overlays++;
        }
    }
    const size_t extensions = ObjectExtension::GetInstance().ClearAll();

    fprintf(stderr, "[OoT] Abandoned session retired: %d overlay(s) reset, %zu object extension entries dropped\n",
            (int)overlays, extensions);
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

// z_play.c. The matching prototype is in functions.h, which this TU does not
// include (it reaches z64.h through GameInteractor.h, not global.h).
extern "C" void Play_SaveSceneFlags(PlayState* play);

/**
 * OoT's half of Combo_FlushLiveStateForFreeze (#638): copy the current scene's
 * live flags into gSaveContext before the departure freezes it.
 *
 * OoT keeps the flags of the scene being played in play->actorCtx.flags and
 * copies them into gSaveContext.sceneFlags[play->sceneNum] only from
 * Actor_CleanupContext (z_actor.c) on a scene transition. Both cross-game
 * departures skip that copy: the entrance path freezes from inside
 * Play_Update's transition check (z_play.c, before the transition runs) and
 * then stops the gamestate with init/destroy nulled, and the F10 path breaks the
 * graph loop with the PlayState still live and never destroys it. Everything
 * set during the final scene visit -- a chest, a switch, a collectible -- was
 * frozen as unset and the return leg restored it that way. MM has the same
 * shape (MM_Combo_FlushSceneFlagsForFreeze) and, unlike OoT's Market, a
 * persistent collectible right at its portal, which is how #635 surfaced it.
 *
 * Play_SaveSceneFlags is the exact copy Actor_CleanupContext would have made:
 * four u32 words, idempotent, indexed by the live sceneNum. NULL-safe: the
 * launcher's freeze can run with no PlayState (a switch requested from outside
 * gameplay, or the headless rows), and OoT_gPlayState is nulled by Play_Destroy
 * and OoT_Graph_ResetRunFrameContext, so a non-NULL value is a live PlayState.
 */
extern "C" void OoT_Combo_FlushSceneFlagsForFreeze(void) {
    PlayState* play = OoT_gPlayState;
    if (play == NULL) {
        return;
    }
    Play_SaveSceneFlags(play);
}

// z_actor.c. functions.h is not included here (see Play_SaveSceneFlags above);
// these are the exact entry points the two Destroys below call, so the flag
// hooks (check tracker, network sync) see the same dispatch a scene exit gives.
extern "C" s32 OoT_Flags_GetEventChkInf(s32 flag);
extern "C" void OoT_Flags_SetEventChkInf(s32 flag);
extern "C" void Flags_UnsetEventChkInf(s32 flag);

/**
 * OoT's half of the pre-freeze discipline for actor Destroys whose writes are
 * "on any exit from this scene" (#770, the residue of #750 / PR #767).
 *
 * Both cross-game departures retire OoT's Play gamestate without Play_Destroy,
 * so no actor Destroy runs for the actors live at that instant (see
 * OoT_RetireAbandonedSession, which puts back overlay statics and ActorDB client
 * counts, and deliberately does not run Destroys). Two Destroys write the save
 * UNCONDITIONALLY on scene exit, so an F10 (or a cross-game door) froze the save
 * as if the player had never left:
 *
 *  - Bg_Relay_Objects (z_bg_relay_objects.c, BgRelayObjects_Destroy): the
 *    windmill's rotating gear unsets EVENTCHKINF_PLAYED_SONG_OF_STORMS_IN_WINDMILL
 *    whenever gSaveContext.cutsceneIndex < 0xFFF0. The flag is what makes the
 *    gear and Kakariko's windmill sails (Bg_Spot01_Fusya) spin fast; vanilla
 *    clears it on every non-cutscene exit from the windmill.
 *  - Bg_Spot06_Objects (z_bg_spot06_objects.c, BgSpot06Objects_Destroy): every
 *    Lake Hylia object, in rando, sets EVENTCHKINF_RAISED_LAKE_HYLIA_WATER once
 *    EVENTCHKINF_USED_WATER_TEMPLE_BLUE_WARP is set (the water-control switch
 *    lowers the lake only for the visit). The same Destroy writes the lower
 *    river water box's zMin back on the resource-cached scene collision header
 *    unconditionally, because Init subtracts 50 from it whenever the lake loads
 *    lowered; that is the non-save half of the same skip and is applied here too.
 *
 * WHY HERE AND NOT AT OoT_Game_Suspend. The blob is captured BEFORE suspend on
 * both paths (Combo_CheckEntranceSwitch and Combo_FreezeActiveGameForHotSwap,
 * each right after Combo_FlushLiveStateForFreeze); a write at suspend or in
 * OoT_RetireAbandonedSession would change the live gSaveContext after the
 * freeze and never reach the frozen half. This runs inside that flush, where
 * the PlayState and its actor lists are still live.
 *
 * WHICH ACTORS. The live actor lists are walked, so the condition is the
 * Destroy's own, evaluated on the actors that would have been destroyed: an
 * actor with a destroy function (OoT_Actor_Destroy calls only a non-NULL one),
 * the gear by its post-Init params (WINDMILL_ROTATING_GEAR, 0; a duplicate gear
 * is killed with params 0xFF and its Destroy writes nothing), any Lake Hylia
 * object for the lake. Scene numbers are not consulted: the actors are what the
 * Destroys key on. No vendored Destroy is touched; an ordinary in-OoT scene exit
 * still runs them and never reaches this function, which only the cross-game
 * freeze seam calls. Idempotent (an unset, a set and an assignment). NULL-safe
 * on OoT_gPlayState like the scene-flag flush: no live PlayState, nothing to do.
 *
 * NOT HERE: Obj_Lightswitch's Destroy unsets a scene switch flag only when
 * SoH's Sunlight Arrows static says a Light Arrow lit the switch (not
 * unconditional scene-exit semantics; #767 already resets that static), and the
 * session-only rows of #770 are normalised on re-entry (see that issue).
 */
extern "C" void OoT_Combo_ApplySceneExitWritesForFreeze(void) {
    PlayState* play = OoT_gPlayState;
    if (play == NULL) {
        return;
    }
    // Values private to the two overlays' .c files: WindmillSetpiecesMode's
    // WINDMILL_ROTATING_GEAR, LakeHyliaWaterBoxIndices'
    // LHWB_GERUDO_VALLEY_RIVER_LOWER, and WATER_LEVEL_RIVER_LOWER_Z.
    const s16 kWindmillRotatingGear = 0;
    const s32 kRiverLowerWaterBox = 1;
    const s16 kRiverLowerZMin = 2203;

    bool windmillGear = false;
    bool lakeObjects = false;
    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* actor = play->actorCtx.actorLists[category].head; actor != NULL; actor = actor->next) {
            if (actor->destroy == NULL) {
                continue;
            }
            if (actor->id == ACTOR_BG_RELAY_OBJECTS && actor->params == kWindmillRotatingGear) {
                windmillGear = true;
            } else if (actor->id == ACTOR_BG_SPOT06_OBJECTS) {
                lakeObjects = true;
            }
        }
    }

    if (windmillGear && gSaveContext.cutsceneIndex < 0xFFF0 &&
        OoT_Flags_GetEventChkInf(EVENTCHKINF_PLAYED_SONG_OF_STORMS_IN_WINDMILL)) {
        Flags_UnsetEventChkInf(EVENTCHKINF_PLAYED_SONG_OF_STORMS_IN_WINDMILL);
        fprintf(stderr, "[OoT] pre-freeze: windmill gear exit cleared the Song of Storms windmill flag (#770)\n");
    }

    if (lakeObjects) {
        CollisionHeader* colHeader = play->colCtx.colHeader;
        if (colHeader != NULL && colHeader->waterBoxes != NULL && colHeader->numWaterBoxes > kRiverLowerWaterBox) {
            colHeader->waterBoxes[kRiverLowerWaterBox].zMin = kRiverLowerZMin;
        }
        if (IS_RANDO && OoT_Flags_GetEventChkInf(EVENTCHKINF_USED_WATER_TEMPLE_BLUE_WARP) &&
            !OoT_Flags_GetEventChkInf(EVENTCHKINF_RAISED_LAKE_HYLIA_WATER)) {
            OoT_Flags_SetEventChkInf(EVENTCHKINF_RAISED_LAKE_HYLIA_WATER);
            fprintf(stderr, "[OoT] pre-freeze: Lake Hylia exit raised the water again after the Water Temple (#770)\n");
        }
    }
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
    // Pre-freeze discipline (#638, #626, #664): the live scene flags and a
    // dead health bar (both games, #626/#664) must be folded into gSaveContext BEFORE the bytes are
    // captured. This is the one point on the F10 path that passes the real
    // gSaveContext, so the flush lives here rather than inside
    // Switch_PrepareHotSwap (which src/common tests drive with scratch buffers).
    Combo_FlushLiveStateForFreeze(departing);
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
    // .itemIcon comes from Combo_GetForeignItemIconName (#494): the pinned pool
    // now carries each OoT item's ITEM_* texture-map key alongside its name and
    // article, and the accessor serves it back origin-keyed, so src/common never
    // translates an RG_* into a texture. It may still return NULL (an entry with
    // no icon, or an id outside the pool), and a NULL icon is text-only, not a
    // degradation — the MOD_RANDOMIZER branch in hook_handlers.cpp omits the icon
    // too. The overlay stores the pointer and dereferences it at draw time, well
    // after the item icons are registered, and the pool's key is a string literal
    // that outlives the toast.
    //
    // WORDING (#510): "You got the Fairy Bow", not "Received from Termina: …".
    // The arrival IS the moment the player receives it in OoT, so the native
    // sentence is the correct one; naming the other game was the cross-game
    // tell the operator asked to remove. Name and article both come from the
    // pinned pool via the origin-tagged SharedItem, so this never fabricates an
    // id-space crossing; both return NULL when that origin's pool is not linked
    // into this build, hence the fallbacks.
    //
    // FIELD ARRANGEMENT (#494): verb in `.message`, item name in `.suffix` —
    // the arrangement every OTHER OoT rando pickup toast uses
    // (Enhancements/randomizer/hook_handlers.cpp). Options colours each field
    // differently, so the earlier `.prefix` + `.message` form rendered a foreign
    // arrival in blue-then-grey where an ordinary pickup is grey-then-red. That
    // difference is visible at a glance and reads as "this one is special",
    // which is the class of tell #510 removed from the model and the text.
    const char* foreignName = Combo_GetForeignItemName(*item);
    const char* foreignArticle = Combo_GetForeignItemArticle(*item);
    Notification::Emit({
        .itemIcon = Combo_GetForeignItemIconName(*item),
        .message = "You got ",
        .suffix = std::string(foreignArticle != nullptr ? foreignArticle : "") +
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
// Backs the SLOT/AMMO/INV_CONTENT macros (macros.h) the ammo shim uses.
extern "C" u8 OoT_gItemSlots[56];

// Declared locally because OTRGlobals.h only declares this inside its
// `#ifndef __cplusplus` block — the C half of the header — so it is invisible
// to every C++ TU. Redeclaring it here is the established pattern rather than a
// workaround: draw.cpp, CosmeticsEditor.cpp, NoMasterSword.cpp and several
// other C++ callers each do exactly this. The definition (OTRGlobals.cpp) is
// extern "C", which is the linkage this must match.
extern "C" u8 Randomizer_GetSettingValue(RandomizerSettingKey randoSettingKey);

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

// Highest tier index in every ammo row of OoT_gUpgradeCapacities (each row has
// 4 entries). Passed as the apply CAP for the same reason OOT_MAX_WALLET_TIER
// is: a pool value authored by a future build with more tiers must not index
// off the end of the capacity table.
#define OOT_MAX_AMMO_TIER 3u

// OoT's VANILLA bombchu ceiling. Unlike arrows and bombs, bombchus have no
// upgrade row in either game — OoT fixes the cap at 50 while MM derives it from
// the bomb bag — so the two ends of this shared count clamp against different
// numbers, which is exactly the asymmetry the watermark discipline absorbs.
//
// Only half the story under rando: see OoT_BombchuCapacity.
#define OOT_BOMBCHU_CAPACITY 50u

// One shared ammo pair: the capacity tier, the count it bounds, and the
// inventory item a tier of 1 or more implies. Table-driven because the five
// rows differ only in these four values, and a copy-pasted block per row is
// how one of them silently ends up reading another's slot.
typedef struct {
    uint8_t tierKind;  // RSBS_SHARED_RES_*_TIER, or RSBS_SHARED_RES_NONE when the count has no tier (bombchus)
    uint8_t countKind; // RSBS_SHARED_RES_*_COUNT
    s16 upgrade;       // UPG_* row index, ignored when tierKind is NONE
    uint8_t item;      // ITEM_* whose slot holds the count, and which a nonzero tier grants
} OoTSharedAmmo;

static const OoTSharedAmmo kOoTSharedAmmo[] = {
    { RSBS_SHARED_RES_QUIVER_TIER, RSBS_SHARED_RES_ARROW_COUNT, UPG_QUIVER, ITEM_BOW },
    { RSBS_SHARED_RES_BOMB_BAG_TIER, RSBS_SHARED_RES_BOMB_COUNT, UPG_BOMB_BAG, ITEM_BOMB },
    { RSBS_SHARED_RES_STICK_TIER, RSBS_SHARED_RES_STICK_COUNT, UPG_STICKS, ITEM_STICK },
    { RSBS_SHARED_RES_NUT_TIER, RSBS_SHARED_RES_NUT_COUNT, UPG_NUTS, ITEM_NUT },
    { RSBS_SHARED_RES_NONE, RSBS_SHARED_RES_BOMBCHU_COUNT, 0, ITEM_BOMBCHU },
};

static uint16_t OoT_ReadAmmo(uint8_t item) {
    const s8 held = AMMO(item);
    return held < 0 ? 0u : (uint16_t)held;
}

/**
 * OoT's LIVE bombchu ceiling, which is 50 only outside the randomizer.
 *
 * With RSK_BOMBCHU_BAG set to progressive, SoH replaces the ceiling entirely:
 * Bombchus.cpp registers a VB_CHECK_BOMBCHU_CAPACITY override that CLAMPS
 * AMMO(ITEM_BOMBCHU) down to gRandoContext->GetBombchuCapacity() (0/20/30/50 by
 * bag level) on the next capacity check. Applying the shared pool against a
 * hardcoded 50 therefore does not just overfill — it stages a silent theft:
 * the clamp fires on the very next bombchu thrown, the count drops to the real
 * capacity, and the following harvest reads that as the player having SPENT the
 * difference and debits the shared pool by it. With bag level 0 the pool is
 * zeroed outright, so bombchus the player earned in Termina disappear from both
 * games at once.
 *
 * Mirrors the capacity expression in Bombchus.cpp rather than paraphrasing it,
 * including its restriction to the progressive option (the single-bag and
 * vanilla options leave the 50 in place and register no clamp).
 */
static uint16_t OoT_BombchuCapacity(void) {
    if (IS_RANDO && OTRGlobals::Instance != nullptr && OTRGlobals::Instance->gRandoContext != nullptr &&
        Randomizer_GetSettingValue(RSK_BOMBCHU_BAG) == RO_BOMBCHU_BAG_PROGRESSIVE) {
        return (uint16_t)OTRGlobals::Instance->gRandoContext->GetBombchuCapacity();
    }
    return (uint16_t)OOT_BOMBCHU_CAPACITY;
}

// The count's ceiling in OoT right now: the live capacity for a row that has an
// upgrade, the live bombchu capacity for the one that does not.
static uint16_t OoT_AmmoCapacity(const OoTSharedAmmo* row) {
    if (row->tierKind == RSBS_SHARED_RES_NONE) {
        return OoT_BombchuCapacity();
    }
    return (uint16_t)CUR_CAPACITY(row->upgrade);
}

// Put `item` in its own inventory slot if the slot is empty.
//
// This is the half of a tier grant OoT does NOT do for itself. Its tier-2 and
// tier-3 gives (Bigger Quiver, Bigger Bomb Bag) only widen the capacity, on the
// assumption that whatever granted tier 1 already put the bow or the bombs in
// the inventory. A cross-game apply breaks that assumption — the pool can hand
// OoT a quiver tier it never earned locally — so without this the player gets
// capacity and ammo with no usable C-item. MM's own gives set INV_CONTENT
// unconditionally, which is the behavior being matched.
static void OoT_EnsureInventoryItem(uint8_t item) {
    if (INV_CONTENT(item) == ITEM_NONE) {
        INV_CONTENT(item) = item;
    }
}

// OoT's hookshot ceiling: 0 none, 1 hookshot, 2 longshot.
#define OOT_MAX_HOOKSHOT_TIER 2u

// The hookshot as a tier. Both games keep it in ONE inventory byte, so the
// "progressive item" is really a small monotonic number — which is why this is
// a shared RESOURCE and not a shared item. Nothing crosses but the number: MM's
// ITEM_LONGSHOT is a different id that its give path repurposes into a Red
// Potion, so passing a raw item id over the boundary would hand MM a potion.
static uint16_t OoT_ReadHookshotTier(void) {
    const u8 held = INV_CONTENT(ITEM_HOOKSHOT);
    if (held == ITEM_LONGSHOT) {
        return 2u;
    }
    if (held == ITEM_HOOKSHOT) {
        return 1u;
    }
    return 0u;
}

/**
 * Author OoT's hookshot byte for `tier`, including the part a bare INV_CONTENT
 * write would miss.
 *
 * OoT's own longshot give does more than swap the inventory byte: it rewrites
 * every C-button (and both age-specific equip sets) that currently holds the
 * hookshot, because the button stores the ITEM id rather than the slot
 * (z_parameter.c's ITEM_LONGSHOT branch). Skip that and a player who had the
 * hookshot equipped keeps firing the SHORT one after the pool hands them a
 * longshot — the upgrade silently does nothing until they re-equip it.
 *
 * The icon reload that vanilla pairs with those writes is deliberately NOT
 * copied: it needs a live PlayState and this runs from Play_Init before there
 * is one. The arrival's own interface init loads the C-button icons after this
 * point, so the texture follows the id without it.
 */
static void OoT_WriteHookshotTier(uint16_t tier) {
    if (tier == 0u) {
        return; // monotonic: never take one away
    }
    const u8 item = (tier >= 2u) ? (u8)ITEM_LONGSHOT : (u8)ITEM_HOOKSHOT;
    INV_CONTENT(ITEM_HOOKSHOT) = item;

    u8* const buttonSets[] = {
        gSaveContext.equips.buttonItems,
        gSaveContext.adultEquips.buttonItems,
        gSaveContext.childEquips.buttonItems,
    };
    // From index 1, never 0. Index 0 is the B button; every vanilla loop that
    // matches a hookshot against buttonItems starts at 1 (the ITEM_LONGSHOT
    // give and the button-restriction passes alike), and B is not a slot the
    // hookshot is equippable to. It is also not inert storage: SoH's Bottle
    // Adventure restoration deliberately stages arbitrary save bytes THROUGH
    // buttonItems[0], so a byte there that happens to equal 0x0A or 0x0B is
    // data, not an equipped hookshot, and rewriting it would corrupt the value
    // that restoration exists to reproduce.
    for (size_t set = 0; set < ARRAY_COUNT(buttonSets); set++) {
        for (size_t i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
            if (buttonSets[set][i] == ITEM_HOOKSHOT || buttonSets[set][i] == ITEM_LONGSHOT) {
                buttonSets[set][i] = item;
            }
        }
    }
}

// OoT's ocarina ceiling: 0 none, 1 Fairy Ocarina, 2 Ocarina of Time (#668).
#define OOT_MAX_OCARINA_TIER 2u

// The ocarina as a tier, the hookshot's shape a second time: both games keep
// their ocarina in ONE inventory byte, so "one shared instrument" is a small
// monotonic number and nothing but the number crosses. Passing a raw item id
// would be wrong in the MM direction anyway — MM's ITEM_OCARINA_OF_TIME is a
// different enumerator in a different id space (ADR 0002).
//
// SLOT_OCARINA holds ITEM_OCARINA_FAIRY or ITEM_OCARINA_TIME, and INV_CONTENT()
// indexes by item rather than by slot, so both reads below name the same byte.
static uint16_t OoT_ReadOcarinaTier(void) {
    const u8 held = INV_CONTENT(ITEM_OCARINA_FAIRY);
    if (held == ITEM_OCARINA_TIME) {
        return 2u;
    }
    if (held == ITEM_OCARINA_FAIRY) {
        return 1u;
    }
    return 0u;
}

/**
 * Author OoT's ocarina byte for `tier`, including the part a bare INV_CONTENT
 * write would miss.
 *
 * OoT's own Ocarina of Time give rewrites every C-button (and, under rando,
 * both age-specific equip sets) that currently holds the FAIRY ocarina, because
 * a button stores the ITEM id and not the slot (z_parameter.c's
 * ITEM_OCARINA_TIME branch). Skip that and a player who had the fairy ocarina
 * equipped keeps the old icon and the old id after the pool upgrades them — the
 * same silent no-op the hookshot's writer above exists to avoid.
 *
 * The age-set rewrite is done UNCONDITIONALLY here where vanilla gates it on
 * IS_RANDO && LINK_IS_CHILD/ADULT. Vanilla can assume the other age's set will
 * be re-authored when the player time-travels; a cross-game apply runs from
 * Play_Init on an arrival that may be at either age and may never travel again,
 * and leaving the other set holding ITEM_OCARINA_FAIRY would resurrect the
 * un-upgraded id on the next age change. Writing both is idempotent.
 *
 * Interface_LoadItemIcon1 is deliberately NOT called: it needs a live PlayState
 * and this runs before there is one. The arrival's own interface init loads the
 * C-button icons after this point, exactly as for the hookshot.
 */
static void OoT_WriteOcarinaTier(uint16_t tier) {
    if (tier == 0u) {
        return; // monotonic: never take one away
    }
    const u8 item = (tier >= 2u) ? (u8)ITEM_OCARINA_TIME : (u8)ITEM_OCARINA_FAIRY;
    if (INV_CONTENT(ITEM_OCARINA_FAIRY) == ITEM_NONE || item == (u8)ITEM_OCARINA_TIME) {
        INV_CONTENT(ITEM_OCARINA_FAIRY) = item;
    }
    if (item != (u8)ITEM_OCARINA_TIME) {
        return; // nothing equipped can be stale: tier 1 never replaces an id
    }

    u8* const buttonSets[] = {
        gSaveContext.equips.buttonItems,
        gSaveContext.adultEquips.buttonItems,
        gSaveContext.childEquips.buttonItems,
    };
    // From index 1, never 0 — the same rule and the same reason as the hookshot
    // writer above: index 0 is the B button, which is not a slot the ocarina is
    // equippable to and which SoH's Bottle Adventure restoration stages
    // arbitrary save bytes through.
    for (size_t set = 0; set < ARRAY_COUNT(buttonSets); set++) {
        for (size_t i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
            if (buttonSets[set][i] == ITEM_OCARINA_FAIRY) {
                buttonSets[set][i] = item;
            }
        }
    }
}

/**
 * Would GRANTING this row's item be handing the player a shuffled CHECK the
 * seed placed elsewhere? Then the pool may not hand it over.
 *
 * OoT_Item_Give opens with exactly this refusal for sticks and nuts
 * (z_parameter.c:1876-1885, "in case something got missed"): with
 * RSK_SHUFFLE_DEKU_STICK_BAG or RSK_SHUFFLE_DEKU_NUT_BAG on, neither is
 * obtainable until its bag check is found. That gate matters HERE and not for
 * the wallet or the quiver because MM's side is not neutral: a fresh MM save is
 * BORN at stick and nut tier 1 (the Sram default table), so plain monotonic
 * sharing would push tier 1 into OoT on the very first crossing of every seed.
 *
 * Bombchus are the same hazard wearing different clothes. When the seed shuffles
 * them (RSK_BOMBCHU_BAG is not NONE) the player is meant to hold none until the
 * check is found, and Bombchus.cpp keys shop eligibility on
 * INV_CONTENT(ITEM_BOMBCHU) — so materializing the item here would open the
 * bombchu shops too.
 *
 * ONLY THE GRANT IS SUPPRESSED, not the whole row. The count apply still runs,
 * clamped to this game's real (often zero) capacity, and that is deliberate:
 * the apply is what transfers WATERMARK OWNERSHIP to OoT. Skipping it leaves
 * the watermark owned by MM, and then the first OoT harvest after the player
 * legitimately finds the bag takes the "no watermark for this game" seed path,
 * records the live count as already-counted, and silently discards the stock
 * the bag just granted. Applying a zero costs nothing (the player has none) and
 * keeps the pool's arithmetic honest across the moment the gate opens.
 */
static bool OoT_SharedAmmoGrantBlocked(const OoTSharedAmmo* row) {
    if (!IS_RANDO || OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandoContext == nullptr) {
        return false;
    }
    if (row->item == ITEM_STICK) {
        return Randomizer_GetSettingValue(RSK_SHUFFLE_DEKU_STICK_BAG) != 0 && CUR_UPG_VALUE(UPG_STICKS) == 0;
    }
    if (row->item == ITEM_NUT) {
        return Randomizer_GetSettingValue(RSK_SHUFFLE_DEKU_NUT_BAG) != 0 && CUR_UPG_VALUE(UPG_NUTS) == 0;
    }
    if (row->item == ITEM_BOMBCHU) {
        return Randomizer_GetSettingValue(RSK_BOMBCHU_BAG) != RO_BOMBCHU_BAG_NONE &&
               INV_CONTENT(ITEM_BOMBCHU) == ITEM_NONE;
    }
    return false;
}

static uint16_t OoT_ReadHealthQuarters(void) {
    const uint16_t pieces =
        (uint16_t)((gSaveContext.inventory.questItems & OOT_HEART_PIECE_MASK) >> OOT_HEART_PIECE_SHIFT);
    const uint16_t capacity = gSaveContext.healthCapacity < 0 ? 0u : (uint16_t)gSaveContext.healthCapacity;
    // The canonical quantity and its inverse both live in src/common so there
    // is exactly one copy of the arithmetic; see Combo_MakeHealthQuarters for
    // why hearts are one number rather than two shared fields.
    return Combo_MakeHealthQuarters(capacity, pieces);
}

// Magic meter level (0 none / 1 single / 2 double), derived from the two
// acquired FLAGS. Never from magicLevel: Sram_OpenSave zeroes magicLevel on
// every file load and the interface treats "acquired but level 0" as its
// meter-regrow trigger, so magicLevel is transiently 0 on exactly the frames a
// suspend or a mid-play save can land on.
static uint16_t OoT_ReadMagicLevel(void) {
    if (!gSaveContext.isMagicAcquired) {
        return 0u;
    }
    return gSaveContext.isDoubleMagicAcquired ? 2u : 1u;
}

// Current magic with the state machine's pending CREDITS settled in — the
// magic twin of the rupeeAccumulator fold, except nothing is written back:
// advancing the watermark to the settled value is enough, because the machine
// completing later brings the live field to exactly this number (delta zero),
// and on a switch the arrival path wipes the machine to IDLE and the apply
// re-authors the amount from the pool.
//
//   - STEP_CAPACITY / FILL: the true amount is parked in magicFillTarget and
//     `magic` itself can read 0 — the file-load meter-regrow window
//     (z_file_choose parks the saved amount there) and the tail of every
//     magic-upgrade give.
//   - ADD: a magic jar's credit is stepping toward magicTarget.
//   - The CONSUME_* family is deliberately NOT settled: a staged debit dies
//     with the machine on a switch (the spell never completed, so its cost is
//     refunded), and on a mid-play save the machine finishes normally and the
//     next harvest picks the decrement up as an ordinary spend.
//
// A game with no meter reports 0 regardless of the raw field, so a stale
// `magic` value can never enter the pool as phantom credit.
static uint16_t OoT_ReadSettledMagic(void) {
    const uint16_t level = OoT_ReadMagicLevel();
    if (level == 0u) {
        return 0u;
    }
    int32_t settled = gSaveContext.magic < 0 ? 0 : (int32_t)gSaveContext.magic;
    switch (gSaveContext.magicState) {
        case MAGIC_STATE_STEP_CAPACITY:
        case MAGIC_STATE_FILL:
            if ((int32_t)gSaveContext.magicFillTarget > settled) {
                settled = (int32_t)gSaveContext.magicFillTarget;
            }
            break;
        case MAGIC_STATE_ADD:
            if ((int32_t)gSaveContext.magicTarget > settled) {
                settled = (int32_t)gSaveContext.magicTarget;
            }
            break;
        default:
            break;
    }
    // The parked PRE-regrow idiom is state-machine-invisible: a file load, the
    // debug-save injection, and this file's own apply all leave the true
    // amount in magicFillTarget with magic == 0, magicLevel == 0 and the
    // machine IDLE — and it STAYS that way for the whole arrival fade, because
    // the regrow trigger is transition-gated. A harvest landing inside that
    // window (F10 is polled ungated every frame; the interval autosave's
    // OnSaveFile also harvests) would otherwise read 0 against a watermark
    // holding the applied amount and debit the pool by a full meter. The
    // signature is exact — acquired flags set (level != 0 above) with
    // magicLevel still 0 and magic still 0 — and every OoT path that produces
    // it authors magicFillTarget, so the park is trustworthy here.
    if (gSaveContext.magicLevel == 0 && gSaveContext.magic == 0 && (int32_t)gSaveContext.magicFillTarget > settled) {
        settled = (int32_t)gSaveContext.magicFillTarget;
    }
    const int32_t cap = (int32_t)level * MAGIC_NORMAL_METER;
    return (uint16_t)(settled > cap ? cap : settled);
}

// The harvest gate's game-mode contract (see Combo_SaveIsLiveFile). Asserted
// rather than assumed: these are OoT's own enumerators, and a renumber upstream
// would silently invert the gate — accepting the title screen and rejecting
// gameplay, which is precisely the bug the gate exists to close.
static_assert(GAMEMODE_NORMAL == RSBS_GAMEMODE_NORMAL, "OoT GAMEMODE_NORMAL must match RSBS_GAMEMODE_NORMAL");
static_assert(GAMEMODE_TITLE_SCREEN == RSBS_GAMEMODE_TITLE_SCREEN,
              "OoT GAMEMODE_TITLE_SCREEN must match RSBS_GAMEMODE_TITLE_SCREEN");
static_assert(GAMEMODE_FILE_SELECT == RSBS_GAMEMODE_FILE_SELECT,
              "OoT GAMEMODE_FILE_SELECT must match RSBS_GAMEMODE_FILE_SELECT");
static_assert(GAMEMODE_END_CREDITS == RSBS_GAMEMODE_END_CREDITS,
              "OoT GAMEMODE_END_CREDITS must match RSBS_GAMEMODE_END_CREDITS");

/**
 * Is OoT's live gSaveContext a real loaded file being played (#525 follow-up)?
 *
 * Thin adapter over the shared policy so the two games cannot drift; all of the
 * reasoning — including why fileNum is deliberately NOT consulted, even though
 * OoT's title save is the 0xFF sentinel — lives in Combo_SaveIsLiveFile.
 */
static bool OoT_SaveIsLiveFile(void) {
    return Combo_SaveIsLiveFile(GAME_OOT, (int32_t)gSaveContext.gameMode);
}

/**
 * OoT's half of the pre-freeze revive (#664), the twin of
 * MM_Combo_ReviveDeadHealthForFreeze (#626 / PR #650).
 *
 * THE ROUTE. OoT's game-over is vanilla, not an enhancement: Link's health hits
 * 0, GameOver_Update runs the death states, and the kaleido chain
 * (z_kaleido_scope_PAL.c, pauseCtx->state 8 through 0x11) shows "GAME OVER",
 * the save prompt and "Continue playing?". Health stays at 0 for that whole
 * stretch; the only writer that lifts it is the prompt's continue leg
 * (state 0x11). F10 is polled ungated every frame from OoT's graph loop
 * (graph.c, Combo_CheckHotSwap), so a press anywhere in that stretch breaks the
 * loop and reaches Combo_FreezeActiveGameForHotSwap(GAME_OOT) with health 0.
 * Before this hook, Combo_FlushLiveStateForFreeze's GAME_OOT branch flushed
 * scene flags only: the frozen OoT half carried a dead bar, and
 * OoT_HarvestSharedResources (Game_Suspend, after the freeze) read it verbatim
 * into the shared CONSUMABLE bar, which MM's apply ASSIGNS on arrival -- where
 * only its one-heart floor stood between the player and a dead spawn. Once a
 * watermark exists (any earlier crossing), the harvest debits the bar to 0 and
 * MM arrives with one heart, not the three the continue prompt would have given.
 *
 * THE RULE, symmetric with MM's: revive rather than refuse, because F10 from
 * the game-over screen is a switch taken with Link dead, and a switch owes a
 * RESUMABLE half (the reasoning #625 and ADR 0009 decision 4b record for MM).
 * The revived value is OoT's own continue value, the exact expression of the
 * kaleido continue leg and of Sram_OpenSave (z_sram.c): STARTING_HEALTH, three
 * hearts, or the full capacity when the FullHealthSpawn enhancement is on. MM's
 * twin writes MM's own continue literal (0x30) for the same reason: the player
 * gets what the continue prompt would have given them. The heal accumulator is
 * cleared as the continue leg clears it. The magic-meter regrow the continue
 * leg also starts is deliberately NOT replicated, for the reason #625 records:
 * it is multi-frame and this departure has no frames left.
 *
 * A FAIRY REVIVE IN PROGRESS IS NOT THE CONTINUE CASE. Player's death
 * handler (z_player.c) spends the bottled fairy at the killing blow
 * (OoT_Inventory_ConsumeFairy -> gameOverCtx.state = GAMEOVER_REVIVE_START),
 * but the refill -- healthAccumulator = MAX_HEALTH, which the interface update
 * pours in 4 per frame and clamps at the capacity -- is written only after a
 * 60-frame countdown that starts later still. Health sits at 0 for over a
 * second with the bottle already gone. The continue value there would take the
 * fairy AND the refill it paid for; the player owns that heal exactly as a live
 * bar owns its pending accumulator. So when a live PlayState shows the
 * game-over machine in its GAMEOVER_REVIVE_* range, the revive gives what the
 * fairy would have: MAX_HEALTH clamped to the capacity. Once the refill has
 * lifted health above 0 the bar is alive and passes through untouched,
 * accumulator and all.
 *
 * GATED ON gameMode (OoT_SaveIsLiveFile, the harvest's own gate), never
 * fileNum: a cross-game session is pinned to the 0xFF sentinel, and a revive
 * that fired where the harvest would not -- the title screen's attract save --
 * would edit a save nobody is playing. Idempotent: a live bar is untouched.
 *
 * The CVar read is guarded on a live Ship::Context: the CVar bridge
 * dereferences the singleton unconditionally, and headless rows reach this
 * freeze seam without one. Production always has one.
 */
extern "C" void OoT_Combo_ReviveDeadHealthForFreeze(void) {
    if (!OoT_SaveIsLiveFile()) {
        return;
    }
    if (gSaveContext.health > 0) {
        return;
    }
    const PlayState* play = OoT_gPlayState;
    const bool fairySpent = play != NULL && play->gameOverCtx.state >= GAMEOVER_REVIVE_START &&
                            play->gameOverCtx.state <= GAMEOVER_REVIVE_FADE_OUT;
    if (fairySpent) {
        // The spent fairy's refill (MAX_HEALTH through the accumulator),
        // clamped as the interface update clamps it.
        gSaveContext.health = gSaveContext.healthCapacity < MAX_HEALTH ? gSaveContext.healthCapacity : MAX_HEALTH;
    } else {
        bool fullHealthSpawn = false;
        auto context = Ship::Context::GetInstance();
        if (context != nullptr && context->GetConsoleVariables() != nullptr) {
            fullHealthSpawn = CVarGetInteger(CVAR_ENHANCEMENT("FullHealthSpawn"), 0) != 0;
        }
        gSaveContext.health = fullHealthSpawn ? gSaveContext.healthCapacity : STARTING_HEALTH;
    }
    gSaveContext.healthAccumulator = 0;
    fprintf(stderr, "[OoT] pre-freeze: revived a dead health bar to %d (%s) before the departure freeze (#664)\n",
            (int)gSaveContext.health, fairySpent ? "the spent fairy's refill" : "the continue value");
    fflush(stderr);
}

/**
 * HARVEST (#525). Fold OoT's live resource values into the shared pool.
 *
 * Called from OoT_Game_Suspend — the one point on BOTH the entrance and the F10
 * hot-swap path while gSaveContext still belongs to OoT — and immediately
 * before every `.redsave` write, so a file written mid-session carries a pool
 * that agrees with the OoT save stored beside it. Idempotent: a second call
 * with unchanged values is a no-op in both merge disciplines.
 *
 * GATED on a real loaded file. Without the gate, F10 on the title screen
 * harvests the ATTRACT DEMO's debug save — 14 hearts, single magic, and tier 1
 * of every ammo and wallet upgrade against a new file's zeroes — and because
 * those kinds are MONOTONIC they can never leave the pool again. See
 * Combo_SaveIsLiveFile.
 */
extern "C" void OoT_HarvestSharedResources(void) {
    if (!OoT_SaveIsLiveFile()) {
        // Early return BEFORE the accumulator settle and before the first
        // Combo_HarvestSharedResource call, so this touches neither the pool nor
        // the RAM watermark table: the next real harvest must find the
        // empty/occupied world its first-harvest seed rule expects. Skipping the
        // settle is right too — a menu's save has no balance worth settling, and
        // not writing gSaveContext leaves that save exactly as the menu left it.
        fprintf(stderr, "[OoT] shared-resource harvest skipped: not a live file (gameMode=%d fileNum=%d)\n",
                (int)gSaveContext.gameMode, (int)gSaveContext.fileNum);
        fflush(stderr);
        return;
    }

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
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_MAGIC_LEVEL, OoT_ReadMagicLevel());
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_MAGIC_CURRENT, OoT_ReadSettledMagic());

    // Ammo. No settle step: unlike rupees and magic, neither game defers an
    // ammo change through an accumulator — every give and every shot is an
    // immediate AMMO() write with an inline clamp, so the live count is always
    // already settled.
    for (int i = 0; i < ARRAY_COUNT(kOoTSharedAmmo); i++) {
        const OoTSharedAmmo* row = &kOoTSharedAmmo[i];
        if (row->tierKind != RSBS_SHARED_RES_NONE) {
            Combo_HarvestSharedResource(GAME_OOT, row->tierKind, (uint16_t)CUR_UPG_VALUE(row->upgrade));
        }
        Combo_HarvestSharedResource(GAME_OOT, row->countKind, OoT_ReadAmmo(row->item));
    }

    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HOOKSHOT_TIER, OoT_ReadHookshotTier());

    // Ocarina (#668). Offered UNCONDITIONALLY from here: the arming gate lives
    // in Combo_HarvestSharedResource, which consults the same predicate the
    // apply does, so a per-world option cannot end up gated on one side only.
    // With the option off this call touches neither the pool nor the watermark
    // table.
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_OCARINA_TIER, OoT_ReadOcarinaTier());

    // Triforce pieces (ADR 0010 answer O10): OoT's counter is a MIRROR of the
    // one combo count, so harvesting it max-merges OoT's collects into that
    // count. Offered unconditionally like the ocarina; the arming gate (the
    // frozen goal is triforce-hunt) lives in Combo_HarvestSharedResource.
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_TRIFORCE_PIECES,
                                (uint16_t)gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected);
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
        // Floor at one heart. A departing game cannot hand over a dead bar: an
        // F10 from either game's game-over screen revives before the freeze
        // (MM_Combo_ReviveDeadHealthForFreeze #626, OoT_Combo_ReviveDeadHealthForFreeze
        // #664), so this only fires on a corrupt or hand-edited pool — and
        // spawning dead on a cross-game arrival lands in a death handler no
        // arrival path has ever been tested through.
        gSaveContext.health = (s16)(health < 0x10u ? 0x10u : health);
    }

    // --- Magic level (monotonic 0/1/2), then current magic clamped against it.
    // The settled-current snapshot is taken BEFORE the level apply moves the
    // acquired flags: the settled read gates on the game's own PRE-apply level,
    // which is what keeps a magic-less half's residue (see the MM twin) from
    // being promoted into real magic by the flag flip when the pool carries a
    // level but no current slot.
    uint16_t magic = OoT_ReadSettledMagic();
    uint16_t magicLevel = OoT_ReadMagicLevel();
    const bool magicLevelShared = Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_MAGIC_LEVEL, 2u, &magicLevel);
    if (magicLevelShared) {
        if (magicLevel >= 1u) {
            gSaveContext.isMagicAcquired = 1;
        }
        if (magicLevel >= 2u) {
            gSaveContext.isDoubleMagicAcquired = 1;
        }
    }

    // --- Current magic (consumable): one meter across both games, per OoTMM:
    // "current magic is tracked as if OoT and MM were one game with a single
    // magic meter". The cap is DERIVED from the level just applied, never read
    // from live magicCapacity — at this point in the arrival the boot chain has
    // not run the growth machine, so magicCapacity can be 0 or partial.
    const uint16_t magicCap = (uint16_t)(magicLevel * MAGIC_NORMAL_METER);
    const bool magicShared = Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_MAGIC_CURRENT, magicCap, &magic);

    if ((magicLevelShared || magicShared) && magicLevel != 0u) {
        // Re-author through the vanilla file-load idiom (z_file_choose): park
        // the amount in magicFillTarget, zero magic/magicLevel/magicCapacity,
        // idle the machine, and let OoT_Interface_Update regrow the bar from
        // the acquired flags (STEP_CAPACITY -> FILL) exactly as every file load
        // does. Direct field writes would have to reproduce the
        // level/capacity/HUD invariants by hand — and the idiom also repairs a
        // blob frozen mid-regrow, whose magicCapacity is otherwise stranded at
        // a partial value for the rest of the session (the regrow trigger
        // requires magicLevel == 0, which only this and a real file load
        // re-establish).
        gSaveContext.magicFillTarget = (s16)(magic > magicCap ? magicCap : magic);
        gSaveContext.magic = 0;
        gSaveContext.magicLevel = 0;
        gSaveContext.magicCapacity = 0;
        gSaveContext.magicState = MAGIC_STATE_IDLE;
        gSaveContext.prevMagicState = MAGIC_STATE_IDLE;
    }

    // --- Ammo: every tier BEFORE the count it bounds, in one pass per row, for
    // the same reason wallet precedes rupees. A count applied against a
    // capacity of zero clamps to zero and the watermark records that zero as
    // materialized, quietly dropping the rest of the pool.
    for (int i = 0; i < ARRAY_COUNT(kOoTSharedAmmo); i++) {
        const OoTSharedAmmo* row = &kOoTSharedAmmo[i];
        const bool grantBlocked = OoT_SharedAmmoGrantBlocked(row);

        if (!grantBlocked && row->tierKind != RSBS_SHARED_RES_NONE) {
            uint16_t tier = (uint16_t)CUR_UPG_VALUE(row->upgrade);
            if (Combo_ApplySharedResource(GAME_OOT, row->tierKind, OOT_MAX_AMMO_TIER, &tier)) {
                OoT_Inventory_ChangeUpgrade(row->upgrade, (s16)tier);
                if (tier >= 1u) {
                    OoT_EnsureInventoryItem(row->item);
                }
            }
        }

        // Runs even when the grant is blocked — see OoT_SharedAmmoGrantBlocked
        // for why suppressing the apply entirely loses the player's stock the
        // moment the gate opens. The cap does the suppressing: with no bag it
        // is zero, so nothing materializes.
        uint16_t count = OoT_ReadAmmo(row->item);
        if (Combo_ApplySharedResource(GAME_OOT, row->countKind, OoT_AmmoCapacity(row), &count)) {
            // Bombchus have no tier to carry the item across, so a nonzero
            // shared count is what implies the item here.
            if (!grantBlocked && count > 0u) {
                OoT_EnsureInventoryItem(row->item);
            }
            AMMO(row->item) = (s8)count;
        }
    }

    // --- Hookshot (monotonic 0/1/2). Clamped to OoT's own ceiling of 2, which
    // is the higher of the two: MM has no longshot, so a longshot earned here
    // survives a Termina round trip untouched (max-merge cannot demote it) and
    // materializes there as a plain hookshot.
    uint16_t hookshotTier = OoT_ReadHookshotTier();
    if (Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_HOOKSHOT_TIER, OOT_MAX_HOOKSHOT_TIER, &hookshotTier)) {
        OoT_WriteHookshotTier(hookshotTier);
    }

    // --- Ocarina (monotonic 0/1/2), armed per world by the frozen combo
    // setting (#668). Clamped to OoT's ceiling of 2, the higher of the two: MM
    // holds only one ocarina and harvests tier 1, so an Ocarina of Time earned
    // here survives a Termina round trip untouched — max-merge cannot demote it
    // — and MM's ocarina arrives as the Fairy Ocarina rather than promoting the
    // player to the Door of Time's key. With the option off
    // Combo_ApplySharedResource returns false and nothing below runs.
    uint16_t ocarinaTier = OoT_ReadOcarinaTier();
    if (Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_OCARINA_TIER, OOT_MAX_OCARINA_TIER, &ocarinaTier)) {
        OoT_WriteOcarinaTier(ocarinaTier);
    }

    // --- Triforce pieces (monotonic, ADR 0010 answer O10), armed only when the
    // frozen combo goal is triforce-hunt. OoT's counter becomes the whole combo
    // count, so the next piece OoT gives adds to BOTH worlds' collects and its
    // give arm compares the combo requirement against it. The cap is the combo
    // total, which the creation rule bounds at OoT's 8-bit counter; the 0xFF
    // clamp restates that bound where the narrowing happens.
    uint16_t triforcePieces = (uint16_t)gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected;
    const uint16_t triforceCap = Combo_TriforceHuntTotal() > 0xFFu ? 0xFFu : Combo_TriforceHuntTotal();
    if (Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_TRIFORCE_PIECES, triforceCap, &triforcePieces)) {
        gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected = (u8)triforcePieces;
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
        // Pre-freeze discipline (#638): this hook runs the instant nextEntrance
        // is assigned, before the scene transition that would normally copy the
        // live scene flags into gSaveContext (Actor_CleanupContext), and the
        // switch then stops the gamestate without ever running Play_Destroy.
        // Flush them now, or every flag set during the final scene visit is
        // frozen as unset and the pickup respawns on the return leg (#635).
        // Resolved the same way gameId is above: a not-yet-populated tracker
        // means OoT, never a third game.
        Combo_FlushLiveStateForFreeze((currentGame == GAME_MM) ? GAME_MM : GAME_OOT);
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
