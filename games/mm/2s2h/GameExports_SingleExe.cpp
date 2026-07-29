/**
 * Game Entry Points for MM (2Ship2Harkinian) - Single Executable Build
 *
 * This file provides the MM_Game_* functions expected by the redship
 * main.cpp for single-executable builds.
 *
 * In single-exe mode, the libultraship context singleton is created by the
 * harness (rsbs/src/main.cpp) before either game's Init runs — but created
 * UNINITIALIZED. Whichever game boots first runs the shared bring-up
 * (issues #329/#330): OoT via OoT_Game_Init -> InitOTR, MM via
 * InitOTRForMMFirstBoot below. The bring-up lives in OoT's port layer
 * (games/oot/soh/OTRGlobals.cpp) because that layer owns the process-wide
 * runtime MM's frame loop depends on in single-exe builds (the shared
 * Graph_* bridges, Ship_GetInterpolationFPS, the OTR audio thread). MM
 * contributes its own resource factories via RegisterMMResourceFactories()
 * below. Neither game depends on the *other* having booted first (#271) —
 * the second game's bring-up call is a no-op.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstddef> // offsetof for the #395 layout facts; ptrdiff_t
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <filesystem>
#include <string>

#include <ship/Context.h>
#include <ship/window/Window.h>

#include "game_lifecycle.h"
#include "integration_test_hooks.h"
#include "context.h"
#include "save.h" // RsbsSave_* — MM's redship-native unified-save capture
#include "shared_items.h"
#include "shared_resources.h" // Shared cross-game rupees/hearts (#525)
// Paired-world keying + placement-table accessors (#439 switch-entry
// activation logs the placement count at the pairing decision point).
#include "foreign_items.h"
#include "entrance.h"
#include <ship/resource/ResourceManager.h>
#include <ship/resource/ResourceLoader.h>
#include <ship/resource/archive/ArchiveManager.h>
#include "GameInteractor/GameInteractor.h"
#include "mm_game_hooks.h" // MM-owned hook/event shim (#395) — implemented below
#include <ship/resource/File.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "2s2h/Enhancements/Audio/AudioCollection.h"
#include "2s2h/Enhancements/Audio/AudioEditor.h"
// #516: registrars orphaned by the excluded BenPort InitOTR sequence, re-homed
// into MM_Rando_Init below. Headers here so the calls are type-checked rather
// than file-local extern prototypes.
#include "2s2h/CustomItem/CustomItem.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/Enhancements/Saving/SavingEnhancements.h"
#include "2s2h/Enhancements/GfxPatcher/AuthenticGfxPatches.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/resource/type/2shResourceType.h"
#include "2s2h/resource/importer/PathFactory.h"
#include "2s2h/resource/importer/TextMMFactory.h"
#include "2s2h/resource/importer/TextureAnimationFactory.h"
#include "2s2h/resource/importer/KeyFrameFactory.h"
#include "2s2h/resource/importer/SceneFactory.h"
#include "2s2h/resource/importer/CutsceneFactory.h"
#include <ship/resource/ResourceFactory.h>
#include <vector>
#include <algorithm>
#include "Extractor/Extract.h"

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

// Retire the graph coroutine on suspend (games/mm/src/code/graph.c) —
// mirrors OoT: the frame loop must cold-start on re-entry after a switch.
void MM_Graph_ResetRunFrameContext(void);
// Audio reset for cross-game switch (issue #157) and suspend (issue #270)
extern s32 gAudioCtxInitalized;
void AudioThread_InitMesgQueues(void);
void MM_Audio_PreNMI(void);
// Restart the sound system on resume (code_8019AF00.c) — suspend's
// PreNMI halts the sequence players and nothing else re-arms them
// (MM_AudioMgr_Init only runs in MM_Game_Init).
void MM_Audio_InitSound(void);
// Clears the PreNMI resetTimer latch (audio/lib/thread.c) — while it is
// nonzero every sequence start is silently dropped.
void MM_Audio_ResumeFromPreNMI(void);
// Drains the SHARED audio thread (games/oot/soh/OTRGlobals.cpp) — the
// "OoT_" prefix is historical; in single-exe it is the one process-wide
// audio pump both games' synths run on.
void OoT_Audio_DrainForSuspend(void);

// MM's SaveContext (type defined via global.h -> z64save.h).
// Declared here so MM_Game_Resume() can restore it on return from OoT (#170).
// Mirrors the explicit extern on the OoT TU — keeps the symbol's visibility
// independent of whether global.h ever stops including z64save.h transitively.
extern SaveContext gSaveContext;

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

// The sOwlWarpEntrancesForMods copy that used to live here is gone (Lane C0,
// #392): 2s2h/ShipUtils.cpp is compiled in single-exe builds now (the
// un-elided randomizer needs its MM-unique Ship_* surface), so its real
// definition of the table serves the kaleido/PauseOwlWarp consumers and a
// second copy here would be a duplicate strong symbol.

// MM's AudioCollection singleton (S2H::AudioCollection under the single-exe
// namespace split — see include/mm_audio_prefix.h). Upstream defines and
// initializes this in BenPort.cpp, which is excluded from single-exe builds;
// before the split the unqualified reference silently resolved to SoH's
// identically-mangled AudioCollection::Instance, so both games shared one
// pointer slot and MM's custom-sequence bookkeeping ran against OoT's
// collection. Constructed in MM_Game_Init below.
AudioCollection* AudioCollection::Instance = nullptr;

// The cross-game shadow buffers and unified gSaveContext storage are sized at
// MM_SAVE_CONTEXT_SIZE (src/common/game.h), which src/common code cannot
// derive from sizeof(SaveContext) because it never includes MM's z64save.h.
// This TU can, so it enforces the capacity here: if 2S2H's shipSaveInfo /
// rando tables grow past the capacity, the build fails instead of
// freeze/restore silently truncating MM save state on every cross-game switch.
static_assert(sizeof(SaveContext) <= MM_SAVE_CONTEXT_SIZE,
              "MM_SAVE_CONTEXT_SIZE (src/common/game.h) is smaller than 2S2H's runtime SaveContext; "
              "raise the capacity or cross-game freeze/restore will truncate save state");

// Archive hot-swap cycle helpers (#263). Defined in
// src/common/tests/test_archive_hotswap.c (compiled into redship_common via
// test_runner.cpp); resolved at final link. Record this MM arrival, query the
// RSS bound, and read the target arrival count for the cycle-complete check.
extern "C" {
int ArchiveHotswap_RecordArrival(void);
int ArchiveHotswap_RssExceeded(void);
int ArchiveHotswap_TargetArrivals(void);
}

// Shared single-exe bring-up (issues #329/#330). Defined in OoT's port layer
// (games/oot/soh/OTRGlobals.cpp); resolved at final link. Runs the same init
// an OoT-first boot gets, minus the OoT ROM-extraction prompt. No-op if the
// bring-up already happened.
extern "C" void InitOTRForMMFirstBoot(int argc, char* argv[]);

// OoT's binary "Room" (scene) and "Cutscene" factory creators, defined in
// games/oot/soh/OTRGlobals.cpp — the dispatcher below needs OoT's parsers but
// cannot include OoT's factory headers from an MM translation unit (#344).
std::shared_ptr<Ship::ResourceFactory> OoT_CreateSceneFactory();
std::shared_ptr<Ship::ResourceFactory> OoT_CreateCutsceneFactory();
std::shared_ptr<Ship::ResourceFactory> OoT_CreatePathFactory();

// MM's message-table loader (games/mm/2s2h/z_message_OTR.cpp) — populates
// sMessageTableNES/sMessageTableCredits from mm.o2r. Without it, the first
// scene title card after a scene load dereferences NULL message tables (#344).
extern "C" void MM_OTRMessage_Init(void);

// LUS app-directory key for MM. Must match the appName in
// src/common/archive_check.cpp's MM spec: the extraction flow exports
// mm.o2r to this app directory, which is the first location the archive
// availability checks probe.
static const char kMmAppName[] = "2s2h";

// Track if MM has been initialized (for re-entry after game switch)
static bool sMMInitialized = false;
static bool sMMArchivesLoaded = false;

// On-disk paths of the archives LoadMMArchives() registered with the shared
// ArchiveManager. The Room factory dispatcher uses this to decide whether a
// scene/room file came from an MM archive and must be parsed with MM's scene
// command set (#344). Never cleared: archives stay in the ArchiveManager for
// the process lifetime even across MM_Game_Shutdown.
static std::vector<std::string> sMMArchivePaths;

static void RecordMMArchivePath(const std::string& path) {
    if (std::find(sMMArchivePaths.begin(), sMMArchivePaths.end(), path) == sMMArchivePaths.end()) {
        sMMArchivePaths.push_back(path);
    }
}

static bool IsMMArchivePath(const std::string& path) {
    return std::find(sMMArchivePaths.begin(), sMMArchivePaths.end(), path) != sMMArchivePaths.end();
}

// C-visible wrapper over the registry above, for archive-origin filtering
// outside this TU (ResourceMgr_ListFilesForGame in SoH's
// ResourceManagerHelpers.cpp — the sequence/soundfont enumeration scoping).
extern "C" bool Combo_ArchivePathIsMM(const char* path) {
    return path != nullptr && IsMMArchivePath(path);
}

// ============================================================================
// MM-owned GameInteractor shim (#395) — API in games/mm/include/mm_game_hooks.h
// ============================================================================
// MM must never register hooks through the shared C++ GameInteractor: the one
// allocation is OoT's (sizeof 4, nextHookId at offset 0) while this TU
// compiles the class at sizeof 104 / nextHookId offset 96 (MSVC; 72 / 64 on
// Linux GCC), so a registration compiled from MM's body writes ~60-92 bytes
// past the end of the block. (The PR #415 diagnostic showed Linux happened to
// fold these calls to OoT's body — a link-order accident, not a contract.)
// Hooks registered here are dispatched from MM's own frame loop
// (games/mm/src/code/game.c -> MM_GameHooks_ExecuteOnGameStateMainStart), so
// they run on MM frames only — the semantics MM's upstream executor had. The
// cross-bound OoT wrapper path stayed gated off while MM is active (#367),
// which is also why these hooks never enter OoT's registry: no MM handler can
// alias an OoT hook type or VB ordinal from here.

namespace {

struct MMGameHookEntry {
    uint32_t id;
    void (*fn)(void);
};

std::vector<MMGameHookEntry> sMMHooksOnGameStateMainStart;
std::vector<uint32_t> sMMHooksPendingUnregister;
uint32_t sMMNextGameHookId = 1;

// MM-owned storage for the GIEvent queue MM code used to reach as
// GameInteractor::Instance->events / ->currentEvent — MM-only data members at
// offsets entirely past the shared 4-byte allocation, i.e. the same #395 OOB on
// the data path. Lane C's Rando/enh migration reads and writes these instead
// (contract in mm_game_hooks.h and on #392).
std::vector<GIEvent> sMMEventQueue;
GIEvent sMMCurrentEvent = GIEventNone{};

} // namespace

extern "C" uint32_t MM_GameHooks_RegisterOnGameStateMainStart(void (*fn)(void)) {
    if (fn == nullptr) {
        return 0;
    }
    uint32_t id = sMMNextGameHookId++;
    sMMHooksOnGameStateMainStart.push_back({ id, fn });
    return id;
}

extern "C" void MM_GameHooks_UnregisterOnGameStateMainStart(uint32_t hookId) {
    if (hookId == 0) {
        return;
    }
    // Deferred like the C++ registry's UnregisterGameHook: a hook may
    // unregister itself (or a peer) while the dispatch walk is in progress.
    sMMHooksPendingUnregister.push_back(hookId);
}

extern "C" void MM_GameHooks_ExecuteOnGameStateMainStart(void) {
    for (uint32_t deadId : sMMHooksPendingUnregister) {
        for (size_t i = 0; i < sMMHooksOnGameStateMainStart.size(); i++) {
            if (sMMHooksOnGameStateMainStart[i].id == deadId) {
                sMMHooksOnGameStateMainStart.erase(sMMHooksOnGameStateMainStart.begin() +
                                                   static_cast<std::ptrdiff_t>(i));
                break;
            }
        }
    }
    sMMHooksPendingUnregister.clear();

    // Snapshot the count: a hook may register another hook mid-walk (the
    // vector may reallocate — hence indices, not iterators); new hooks first
    // run on the next dispatch, matching the registration contract.
    const size_t count = sMMHooksOnGameStateMainStart.size();
    for (size_t i = 0; i < count; i++) {
        sMMHooksOnGameStateMainStart[i].fn();
    }
}

extern "C" uint32_t MM_GameHooks_CountOnGameStateMainStart(void) {
    return static_cast<uint32_t>(sMMHooksOnGameStateMainStart.size());
}

extern "C" void MM_GameHooks_ResetForTest(void) {
    sMMHooksOnGameStateMainStart.clear();
    sMMHooksPendingUnregister.clear();
}

std::vector<GIEvent>& MM_GameEvents_Queue() {
    return sMMEventQueue;
}

GIEvent& MM_GameEvents_Current() {
    return sMMCurrentEvent;
}

// Integration test hook frame counter (reset each time hooks are registered)
static int sGameStateMainFrameCount = 0;

// (#344) Frames the per-test OnGameStateMainStart hook has run without the
// scene-load predicate holding, and frames it has held. Reset per registration.
static int sSceneLoadWaitFrames = 0;
static int sSceneLoadStableFrames = 0;

// (#344) In-band watchdog: fail the test with diagnostics if the scene never
// finishes loading. Frame-rate dependent, so this is a best-effort early-out;
// the CTest 120s timeout remains the hard backstop.
static const int kSceneLoadWatchdogFrames = 1800;

// ============================================================================
// Integration Test Hooks
// ============================================================================

/**
 * (#344) True once MM has COMPLETED a scene load: a Play state exists, the
 * spawn-list scene command populated linkActorEntry (the pointer that stayed
 * NULL when MM's scene loader was unported), the player actor spawned (the
 * former crash site, MM_Actor_SpawnEntry), and the first room's commands ran.
 */
static bool MM_SceneLoadComplete(void) {
    // MM's test hooks are dispatched from MM's own frame loop via the
    // MM-owned shim (#395), so this gate is belt-and-braces today — but the
    // unit-test harness also drives the dispatch headlessly, and MM's
    // suspended PlayState stays non-NULL across a switch away. Only count
    // MM's own frames (#344).
    if (Context_GetCurrentGame() != GAME_MM) {
        return false;
    }
    PlayState* play = MM_gPlayState;
    if (play == NULL) {
        return false;
    }
    if (play->linkActorEntry == NULL) {
        return false;
    }
    if (play->actorCtx.actorLists[ACTORCAT_PLAYER].first == NULL) {
        return false;
    }
    if (play->roomCtx.curRoom.segment == NULL) {
        return false;
    }
    return true;
}

/**
 * (#344) Watchdog helper shared by the MM-side test hooks: logs why the scene
 * load predicate is failing and requests a failing exit. Returns true once the
 * watchdog has fired so callers can stop doing per-frame work.
 */
static bool MM_SceneLoadWatchdogExpired(const char* testName) {
    // Only MM's own frames count against the watchdog budget (the unit-test
    // harness dispatches the shim headlessly with no game active).
    if (Context_GetCurrentGame() != GAME_MM) {
        return false;
    }
    sSceneLoadWaitFrames++;
    if (sSceneLoadWaitFrames != kSceneLoadWatchdogFrames) {
        return sSceneLoadWaitFrames > kSceneLoadWatchdogFrames;
    }

    PlayState* play = MM_gPlayState;
    fprintf(stderr,
            "[MM-INT-TEST] FAIL (%s): no completed scene load after %d frames "
            "(gPlayState=%p linkActorEntry=%p player=%p roomSegment=%p)\n",
            testName, sSceneLoadWaitFrames, (void*)play, play ? (void*)play->linkActorEntry : (void*)0,
            play ? (void*)play->actorCtx.actorLists[ACTORCAT_PLAYER].first : (void*)0,
            play ? (void*)play->roomCtx.curRoom.segment : (void*)0);
    fflush(stderr);
    IntegrationTest_RequestExit();
    Combo_RequestGameSwitch();
    return true;
}

/**
 * Register integration test hooks for MM.
 * Called after MM is initialized when integration test mode is active.
 */
// Phase-local state for the gameplay round-trip's MM frame driver
// (MM_IntegrationGameplayFrameTick below; reset when the mode is armed).
static GameplayPhase sGpMMLastPhase = GP_PHASE_DONE;
static int sGpMMStableFrames = 0;
static int sGpMMPlayFrames = 0;
static int sGpMMWatchdogFrames = 0;
// Completed MM arrivals this run. Drives the save-continuity tripwire: each
// cycle writes a sentinel rupee count before leaving through the tower door
// (frozen with the live save) and the NEXT arrival asserts it survived the
// round trip — the boot chain wipes gSaveContext after resume's restore, so
// this fails loudly if MM_Play_ConsumeStartupEntrance's in-chain restore
// (z_play.c) ever regresses.
static int sGpMMArrivalCount = 0;
#define GP_MM_CONTINUITY_RUPEE_BASE 100

static void MM_RegisterIntegrationTestHooks(void) {
    if (!IntegrationTest_IsActive()) {
        return;
    }

    IntegrationTestMode mode = IntegrationTest_GetMode();

    if (mode == INT_TEST_BOOT_MM) {
        fprintf(stderr, "[MM] Registering integration test hooks for boot detection\n");
        fflush(stderr);

        // Reset frame counters for fresh registration
        sSceneLoadWaitFrames = 0;
        sSceneLoadStableFrames = 0;

        // (#344) PASS requires a COMPLETED scene load, not early-boot frames.
        // The old criteria (console-logo frames / 10 OnGameStateMainStart
        // firings) passed while MM was still on the console logo, ~5s before
        // the title-demo Play state crashed in MM_Actor_SpawnEntry. (The
        // OnConsoleLogoUpdate hook was dead anyway: MM's executor is excluded
        // in single-exe builds and the call resolves to a no-op stub.)
        MM_GameHooks_RegisterOnGameStateMainStart([]() {
            if (!MM_SceneLoadComplete()) {
                sSceneLoadStableFrames = 0;
                MM_SceneLoadWatchdogExpired("int-boot-mm");
                return;
            }
            sSceneLoadStableFrames++;
            if (sSceneLoadStableFrames == 10) {
                fprintf(stderr, "[MM-INT-TEST] scene load complete and stable (sceneId=0x%X entrance=0x%04X); PASS\n",
                        MM_gPlayState->sceneId, gSaveContext.save.entrance);
                fflush(stderr);
                IntegrationTest_SignalBootComplete(GAME_MM, "MM scene load complete");
            }
        });

        fprintf(stderr, "[MM] Integration test hooks registered\n");
        fflush(stderr);
    } else if (mode == INT_TEST_SWITCH_OOT_HMS_TO_MM) {
        // T1 (#260) leg 2: MM has been booted via the cross-game switch from
        // OoT's HMS trigger. Reaching this hook means OoT's freeze + main
        // loop's hand-off + MM_Game_Init all succeeded end-to-end. Signal pass
        // once MM's graph thread is running steady frames.
        fprintf(stderr, "[MM] Registering integration test hooks for HMS->MM switch completion (T1)\n");
        fflush(stderr);

        sSceneLoadWaitFrames = 0;
        sSceneLoadStableFrames = 0;

        // (#344) PASS requires MM to complete the South Clock Town scene
        // load the entrance link asked for (0xD800, the tower-exit arrival),
        // not just tick frames.
        MM_GameHooks_RegisterOnGameStateMainStart([]() {
            if (!MM_SceneLoadComplete() || MM_gPlayState->sceneId != SCENE_CLOCKTOWER) {
                static bool sWrongSceneLogged = false;
                sSceneLoadStableFrames = 0;
                if (!sWrongSceneLogged && MM_SceneLoadComplete()) {
                    sWrongSceneLogged = true;
                    fprintf(stderr, "[MM-INT-TEST] scene loaded but sceneId=0x%X != SCENE_CLOCKTOWER; waiting\n",
                            MM_gPlayState->sceneId);
                    fflush(stderr);
                }
                MM_SceneLoadWatchdogExpired("int-switch-oot-hms-to-mm");
                return;
            }
            sSceneLoadStableFrames++;
            if (sSceneLoadStableFrames == 10) {
                fprintf(stderr,
                        "[MM-INT-TEST] South Clock Town scene load complete after HMS->MM switch "
                        "(entrance=0x%04X); PASS\n",
                        gSaveContext.save.entrance);
                fflush(stderr);
                IntegrationTest_SignalBootComplete(GAME_MM, "MM scene load complete after HMS->MM switch");
            }
        });

        fprintf(stderr, "[MM] HMS->MM switch hooks registered\n");
        fflush(stderr);
    } else if (mode == INT_TEST_SWITCH_MM_CLOCKTOWN_SOUTH_TO_OOT) {
        // T2 (#261): Boot MM, programmatically trigger the Clock Tower door
        // (the MM->OoT trigger; the test id's "clocktown-south" is
        // historical), assert the cross-game switch resolves to OoT Market.
        // Mirror of T1: MM is the trigger side here, OoT is the receiver.
        // MM has no OnPresentFileSelect analog, so we wait for a completed
        // scene load (#344) plus a few stable frames in OnGameStateMainStart
        // and then fire the trigger once. Final pass is signaled from the
        // OoT-side hook after OoT stabilizes post-switch.
        fprintf(stderr, "[MM] Registering integration test hooks for Clock Tower door->OoT switch (T2)\n");
        fflush(stderr);

        sSceneLoadWaitFrames = 0;
        sSceneLoadStableFrames = 0;

        MM_GameHooks_RegisterOnGameStateMainStart([]() {
            static bool sTriggered = false;
            if (sTriggered) {
                return;
            }
            if (!MM_SceneLoadComplete()) {
                sSceneLoadStableFrames = 0;
                MM_SceneLoadWatchdogExpired("int-switch-mm-clocktown-south-to-oot");
                return;
            }
            sSceneLoadStableFrames++;
            if (sSceneLoadStableFrames < 10) {
                return;
            }
            sTriggered = true;

            fprintf(stderr, "[MM-INT-TEST] MM stable; triggering Clock Tower door entrance 0x%04X\n",
                    MM_ENTR_CLOCK_TOWER_INTERIOR_1);
            fflush(stderr);

            // Same call MM's z_play.c makes when the player walks into
            // the Clock Tower from SCT — minus the freeze, which T3 covers.
            Combo_CheckCrossGameEntrance("mm", MM_ENTR_CLOCK_TOWER_INTERIOR_1);

            if (!Combo_IsCrossGameSwitch()) {
                fprintf(stderr, "[MM-INT-TEST] FAIL: Clock Tower door entrance did not register a cross-game switch\n");
                fflush(stderr);
                IntegrationTest_RequestExit();
                return;
            }

            const char* target = Combo_GetSwitchTargetGameId();
            uint16_t targetEntrance = Combo_GetSwitchTargetEntrance();

            if (!target || strcmp(target, "oot") != 0) {
                fprintf(stderr, "[MM-INT-TEST] FAIL: target should be 'oot', got '%s'\n", target ? target : "(null)");
                fflush(stderr);
                IntegrationTest_RequestExit();
                return;
            }

            if (targetEntrance != OOT_ENTR_MARKET_FROM_MASK_SHOP) {
                fprintf(stderr,
                        "[MM-INT-TEST] FAIL: target entrance should be 0x%04X (Market from Mask Shop), got 0x%04X\n",
                        OOT_ENTR_MARKET_FROM_MASK_SHOP, targetEntrance);
                fflush(stderr);
                IntegrationTest_RequestExit();
                return;
            }

            fprintf(stderr,
                    "[MM-INT-TEST] PASS leg 1: Clock Tower door routes to OoT 0x%04X; main loop will run the "
                    "switch\n",
                    targetEntrance);
            fflush(stderr);
            // Intentionally NOT signaling boot complete here. The main loop
            // will see the pending cross-game switch on Combo_CheckHotSwap
            // and hand off to OoT. The OoT-side hook signals the final pass.
        });

        fprintf(stderr, "[MM] Clock Tower door->OoT switch hooks registered\n");
        fflush(stderr);
    } else if (mode == INT_TEST_ARCHIVE_HOTSWAP_CYCLE) {
        // T4 (#263): MM side of the OoT<->MM archive hot-swap cycle. OoT boots
        // first, so MM is arrivals #2 and #4 (the last) of the
        // OoT->MM->OoT->MM cycle (4 arrivals == 3 transitions). Because the
        // target arrival count is even, the FINAL arrival lands on MM, so this
        // is the side that signals the cycle-complete PASS.
        //
        // Registration runs from MM_Game_Init, which fires only on MM's FIRST
        // entry — later MM arrivals come back through MM_Game_Resume (see
        // GameRunner_SwitchTo: a suspended game is resumed, not re-init'd), so
        // this hook is registered exactly once and the persistent frame counter
        // is NOT reset per arrival. The hook therefore re-arms itself: it fires
        // ~10 stable frames after each (re)entry, then resets the counter so the
        // next arrival reached via resume is detected the same way.
        fprintf(stderr, "[MM] Registering integration test hooks for archive-hotswap cycle (T4)\n");
        fflush(stderr);

        sGameStateMainFrameCount = 0;
        sSceneLoadWaitFrames = 0;
        sSceneLoadStableFrames = 0;

        MM_GameHooks_RegisterOnGameStateMainStart([]() {
            // Fire once per arrival, ~10 stable frames after (re)entry, then
            // re-arm for the next MM arrival (reached via MM_Game_Resume).
            // (#344) Frames only count while a completed scene load is live,
            // so every arrival of the cycle proves real MM gameplay state,
            // not just early-boot frames.
            if (!MM_SceneLoadComplete()) {
                sGameStateMainFrameCount = 0;
                MM_SceneLoadWatchdogExpired("int-archive-hotswap-cycle");
                return;
            }
            sGameStateMainFrameCount++;
            if (sGameStateMainFrameCount < 10) {
                return;
            }
            sGameStateMainFrameCount = 0;
            sSceneLoadWaitFrames = 0;

            int n = ArchiveHotswap_RecordArrival();
            fprintf(stderr, "[MM-INT-TEST] MM stable; archive-hotswap arrival #%d of %d\n", n,
                    ArchiveHotswap_TargetArrivals());
            fflush(stderr);

            if (ArchiveHotswap_RssExceeded()) {
                // Steady-state RSS blew the bound — the #154 per-switch leak
                // regression. Fail fast: RequestExit does NOT set the pass
                // flag, so the run returns non-zero. Combo_RequestGameSwitch()
                // right after unblocks the main loop promptly (known fix).
                fprintf(stderr, "[MM-INT-TEST] FAIL: steady-state RSS bound exceeded after %d arrivals\n", n);
                fflush(stderr);
                IntegrationTest_RequestExit();
                Combo_RequestGameSwitch();
            } else if (n >= ArchiveHotswap_TargetArrivals()) {
                // Target arrivals reached with a healthy runtime — PASS.
                // SignalBootComplete sets the pass flag and requests the
                // switch that unblocks the main loop.
                fprintf(stderr, "[MM-INT-TEST] archive-hotswap cycle complete after %d arrivals\n", n);
                fflush(stderr);
                IntegrationTest_SignalBootComplete(GAME_MM, "archive-hotswap cycle complete");
            } else {
                // Keep the cycle going: re-trigger the MM->OoT switch via
                // the Clock Tower door — same call the T2 branch makes.
                fprintf(stderr, "[MM-INT-TEST] re-triggering Clock Tower door entrance 0x%04X to continue cycle\n",
                        MM_ENTR_CLOCK_TOWER_INTERIOR_1);
                fflush(stderr);
                Combo_CheckCrossGameEntrance("mm", MM_ENTR_CLOCK_TOWER_INTERIOR_1);
            }
        });

        fprintf(stderr, "[MM] archive-hotswap cycle hooks registered\n");
        fflush(stderr);
    } else if (mode == INT_TEST_GAMEPLAY_ROUNDTRIP) {
        // MM half of the gameplay round-trip repro (phase machine in
        // integration_test_hooks.h; OoT half in
        // games/oot/soh/GameExports_SingleExe.cpp). MM owns two phases:
        // stabilize in South Clock Town (the tower-exit arrival) after the
        // HMS switch, run the configured live-gameplay window, then leave
        // through the REAL Clock Tower door transition machinery —
        // z_play.c's TRANS_MODE_SETUP calls
        // Combo_CheckEntranceSwitch(nextEntrance), which freezes MM's live
        // SaveContext, exactly like a player walking into the tower.
        //
        // The MM driver predates the MM-owned hook shim (#395) and drives
        // frames directly: MM's graph loop calls
        // MM_IntegrationGameplayFrameTick() (games/mm/src/code/graph.c),
        // which is a no-op outside this mode. (Historically it could not use
        // GameInteractor hooks at all — MM's dispatch went through the
        // cross-bound OoT wrappers, which #367 no-ops while MM is active.)
        // Kept as-is: the direct tick is proven by the soak tier and needs
        // none of the shim's registration machinery.
        sGpMMLastPhase = GP_PHASE_DONE;
        sGpMMStableFrames = 0;
        sGpMMPlayFrames = 0;
        sGpMMWatchdogFrames = 0;
        sGpMMArrivalCount = 0;

        fprintf(stderr, "[MM] gameplay round-trip driver armed (direct frame tick)\n");
        fflush(stderr);
    }
}

// C entry point for the mm-gi-shim test (games/mm/2s2h/mm_gi_shim_test.cpp):
// runs the REAL per-mode arming path against whatever mode
// IntegrationTest_SetMode selected, so the four-mode registration surface is
// locked ROM-free (the operator-batch check on #392 covers live arming).
extern "C" void MM_GIShim_TestArmIntegrationHooks(void) {
    MM_RegisterIntegrationTestHooks();
}

// MM's view of the GameInteractor layout, reported from this TU's production
// headers/flags for the mm-gi-shim layout lock. The Register* members
// themselves are compile-time poisoned in this target
// (games/mm/include/mm_gi_hook_guard.h), so layout facts are all this TU can
// export — which is the point.
extern "C" size_t MM_GI_InstanceSize(void) {
    return sizeof(GameInteractor);
}

extern "C" size_t MM_GI_NextHookIdOffset(void) {
    return offsetof(GameInteractor, nextHookId);
}

// ============================================================================
// Gameplay round-trip repro (INT_TEST_GAMEPLAY_ROUNDTRIP) — MM frame driver
// ============================================================================

/**
 * Per-frame MM driver for the gameplay round-trip. Called directly from MM's
 * graph loop (games/mm/src/code/graph.c, single-exe only) once per frame —
 * NOT via GameInteractor: MM's calls into the shared hook wrappers are
 * suppressed while MM is active (#367), which is production behavior this
 * repro must preserve, so the harness cannot ride those hooks on the MM side.
 * No-op unless the gameplay round-trip mode is active.
 */
extern "C" void MM_IntegrationGameplayFrameTick(void) {
    if (IntegrationTest_GetMode() != INT_TEST_GAMEPLAY_ROUNDTRIP) {
        return;
    }
    if (Context_GetCurrentGame() != GAME_MM) {
        return; // switch in flight; only count MM-owned frames
    }
    GameplayPhase phase = IntegrationTest_GetGameplayPhase();
    if (phase != GP_PHASE_MM_STABILIZE && phase != GP_PHASE_MM_PLAY) {
        sGpMMWatchdogFrames = 0;
        return;
    }
    if (phase != sGpMMLastPhase) {
        sGpMMLastPhase = phase;
        sGpMMStableFrames = 0;
        sGpMMPlayFrames = 0;
        sGpMMWatchdogFrames = 0;
    }

    const GameplayTestConfig* cfg = IntegrationTest_GetGameplayConfig();
    sGpMMWatchdogFrames++;
    if (sGpMMWatchdogFrames == cfg->framesPerPhase * 4 + 3600) {
        PlayState* play = MM_gPlayState;
        fprintf(stderr,
                "[GP-TEST] MM watchdog: no progress after %d frames "
                "(play=%p linkActorEntry=%p player=%p roomSegment=%p stable=%d gameplay=%d)\n",
                sGpMMWatchdogFrames, (void*)play, play ? (void*)play->linkActorEntry : (void*)0,
                play ? (void*)play->actorCtx.actorLists[ACTORCAT_PLAYER].first : (void*)0,
                play ? (void*)play->roomCtx.curRoom.segment : (void*)0, sGpMMStableFrames, sGpMMPlayFrames);
        fflush(stderr);
        IntegrationTest_GameplayFail("MM-side phase watchdog expired");
        return;
    }

    if (phase == GP_PHASE_MM_STABILIZE) {
        // (#344) A completed South Clock Town scene load — player spawned at
        // the tower-exit arrival (0xD800, as if walking out of the Clock
        // Tower), first room's commands ran — plus a few stable frames
        // before the gameplay window starts.
        if (!MM_SceneLoadComplete() || MM_gPlayState->sceneId != SCENE_CLOCKTOWER) {
            sGpMMStableFrames = 0;
            return;
        }
        sGpMMStableFrames++;
        if (sGpMMStableFrames == 10) {
            sGpMMArrivalCount++;
            // Save-continuity tripwire (see sGpMMArrivalCount): arrivals
            // after the first must carry the sentinel the previous MM leg
            // froze — proof the in-chain restore beat the boot-chain wipe.
            if (sGpMMArrivalCount > 1) {
                s16 expected = (s16)(GP_MM_CONTINUITY_RUPEE_BASE + sGpMMArrivalCount - 1);
                s16 actual = gSaveContext.save.saveInfo.playerData.rupees;
                if (actual != expected) {
                    fprintf(stderr,
                            "[GP-TEST] FAIL: MM save continuity lost across round trip "
                            "(arrival %d: rupees=%d, expected sentinel %d — frozen save "
                            "not restored over the boot-chain wipe)\n",
                            sGpMMArrivalCount, actual, expected);
                    fflush(stderr);
                    IntegrationTest_GameplayFail("MM frozen-save continuity lost");
                    return;
                }
                fprintf(stderr, "[GP-TEST] MM save continuity verified (arrival %d: rupee sentinel %d)\n",
                        sGpMMArrivalCount, actual);
                fflush(stderr);
            }
            fprintf(stderr,
                    "[GP-TEST] MM South Clock Town stable (entrance=0x%04X); "
                    "starting gameplay window\n",
                    gSaveContext.save.entrance);
            fflush(stderr);
            IntegrationTest_SetGameplayPhase(GP_PHASE_MM_PLAY);
        }
        return;
    }

    // GP_PHASE_MM_PLAY: only count frames with live gameplay state.
    if (!MM_SceneLoadComplete()) {
        return;
    }
    sGpMMPlayFrames++;
    if (sGpMMPlayFrames < cfg->framesPerPhase) {
        return;
    }
    PlayState* play = MM_gPlayState;
    // Arm the continuity sentinel for the next arrival BEFORE firing the
    // door: the transition's Combo_CheckEntranceSwitch freezes the live
    // SaveContext, sentinel included.
    gSaveContext.save.saveInfo.playerData.rupees = (s16)(GP_MM_CONTINUITY_RUPEE_BASE + sGpMMArrivalCount);
    fprintf(stderr,
            "[GP-TEST] firing Clock Tower door: entrance 0x%04X after %d live MM frames "
            "(continuity sentinel: %d rupees)\n",
            MM_ENTR_CLOCK_TOWER_INTERIOR_1, sGpMMPlayFrames, GP_MM_CONTINUITY_RUPEE_BASE + sGpMMArrivalCount);
    fflush(stderr);
    play->nextEntrance = MM_ENTR_CLOCK_TOWER_INTERIOR_1;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
    IntegrationTest_SetGameplayPhase(GP_PHASE_OOT_RETURN);
}

/**
 * Load MM archives (mm.o2r, 2ship.o2r) into the shared ArchiveManager.
 * OoT already initialized Ship::Context with OoT archives; we add MM's.
 * Idempotent — skips if already loaded.
 */
static int LoadMMArchives() {
    if (sMMArchivesLoaded) {
        fprintf(stderr, "[MM] Archives already loaded, skipping\n");
        return 0;
    }

    auto ctx = Ship::Context::GetInstance();
    if (!ctx || !ctx->GetResourceManager()) {
        fprintf(stderr, "[MM] ERROR: No ResourceManager — cannot load archives\n");
        return -1;
    }
    auto archiveMgr = ctx->GetResourceManager()->GetArchiveManager();

    int loaded = 0;

    // Try mm.o2r (primary), then .zip/.otr fallbacks
    for (const char* ext : { "mm.o2r", "mm.zip", "mm.otr" }) {
        std::string path = Ship::Context::LocateFileAcrossAppDirs(ext, kMmAppName);
        if (!path.empty() && std::filesystem::exists(path)) {
            if (archiveMgr->AddArchive(path)) {
                fprintf(stderr, "[MM] Loaded archive: %s\n", path.c_str());
                RecordMMArchivePath(path);
                loaded++;
            }
            break; // Only load one mm archive
        }
    }

    // Load 2ship.o2r (MM's port-asset archive — the equivalent of soh.o2r).
    // Probes the same locations ArchiveCheck_PortArchiveAvailable checks
    // (src/common/archive_check.cpp — keep the two in sync): next to the
    // executable, then the working directory. A missing 2ship.o2r means an
    // incomplete install, and MM's early boot dereferences null resources
    // (boot-logo textures, fonts) without it, so fail hard instead of
    // continuing into undefined behavior. The harness gates in
    // rsbs/src/main.cpp normally catch this earlier with a user dialog;
    // this is the backstop for direct callers.
    bool shipLoaded = false;
    for (const std::string& shipPath :
         { Ship::Context::GetPathRelativeToAppBundle("2ship.o2r"), std::string("./2ship.o2r") }) {
        if (!shipPath.empty() && std::filesystem::exists(shipPath)) {
            if (archiveMgr->AddArchive(shipPath)) {
                fprintf(stderr, "[MM] Loaded archive: %s\n", shipPath.c_str());
                RecordMMArchivePath(shipPath);
                loaded++;
                shipLoaded = true;
            }
            break;
        }
    }
    if (!shipLoaded) {
        fprintf(stderr, "[MM] ERROR: 2ship.o2r not found or unreadable — incomplete install, cannot start MM\n");
        return -1;
    }

    fprintf(stderr, "[MM] Loaded %d MM archive(s) into shared context\n", loaded);
    if (loaded == 0) {
        fprintf(stderr, "[MM] ERROR: No MM archives found — cannot proceed\n");
        return -1;
    }
    sMMArchivesLoaded = true;
    return 0;
}

/**
 * Per-archive factory dispatcher for loader slots both games claim (#344).
 *
 * Both games stamp scenes/rooms as (BINARY, 'OROM', version 0) and cutscenes
 * as (BINARY, 'OCUT', version 0) in the OTR header, so the shared
 * ResourceLoader has exactly one slot for each — but the wire formats are
 * game-specific (scene commands: MM's SetRoomBehavior is 6 bytes vs OoT's 5,
 * MM-only opcodes 0x1A-0x1F, opcode 0x19 means camera-settings in OoT vs
 * world-map-visited in MM; cutscenes: entirely different command systems).
 * A dispatcher owns each slot and routes by source archive: files served
 * from an archive that LoadMMArchives() registered are parsed with MM's
 * S2H factory, everything else with OoT's. The archive lookup uses the
 * same ArchiveManager map that served the file bytes, so recursive loads
 * (e.g. alternate scene headers, per-scene cutscene files) dispatch
 * consistently by construction. Known limitation: MM assets shipped in
 * OTHER archives (user mod .o2rs) route to OoT's parser — MM mod support
 * in single-exe builds is a follow-up.
 */
class RsbsMMArchiveFactoryDispatcher final : public Ship::ResourceFactoryBinary {
  public:
    RsbsMMArchiveFactoryDispatcher(std::shared_ptr<Ship::ResourceFactory> ootFactory,
                                   std::shared_ptr<Ship::ResourceFactory> mmFactory)
        : mOoTFactory(ootFactory), mMMFactory(mmFactory) {
    }

    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override {
        // Alt assets ("alt/<path>" from texture packs) replace a base asset;
        // the game whose asset is being replaced determines the parser, so
        // dispatch on the archive owning the canonical path.
        std::string path = initData->Path;
        if (path.rfind(Ship::IResource::gAltAssetPrefix, 0) == 0) {
            path = path.substr(Ship::IResource::gAltAssetPrefix.size());
        }
        auto resourceMgr = Ship::Context::GetInstance()->GetResourceManager();
        auto archiveMgr = resourceMgr != nullptr ? resourceMgr->GetArchiveManager() : nullptr;
        auto archive = archiveMgr != nullptr ? archiveMgr->GetArchiveFromFile(path) : nullptr;
        if (archive != nullptr && IsMMArchivePath(archive->GetPath())) {
            return mMMFactory->ReadResource(file, initData);
        }
        return mOoTFactory->ReadResource(file, initData);
    }

  private:
    std::shared_ptr<Ship::ResourceFactory> mOoTFactory;
    std::shared_ptr<Ship::ResourceFactory> mMMFactory;
};

/**
 * Register MM-only resource factories into the shared ResourceLoader.
 * OoT already registered shared types (Animation, Skeleton, etc.).
 * MM needs its own factories for Path (overwrites OoT's — MM paths have
 * extra fields), TextMM, TextureAnimation, and KeyFrame types, plus the
 * archive-dispatching Room factory (#344).
 */
static void RegisterMMResourceFactories() {
    auto loader = Ship::Context::GetInstance()->GetResourceManager()->GetResourceLoader();

    // Path — per-archive dispatcher like Room/Cutscene below: MM paths carry
    // additional fields, so the two wire formats are incompatible. This was a
    // flat overwrite until 2026-07-18, which made every OoT Path resource
    // parse with MM's reader once MM had initialized — the reader ran off the
    // end of the buffer and the std::out_of_range rethrown out of the
    // resource future killed the process on the first OoT scene with patrol
    // paths loaded after any MM visit (caught by the RSBS_GP_BOOT_AGE=adult
    // gameplay round trip).
    loader->RegisterResourceFactory(
        std::make_shared<RsbsMMArchiveFactoryDispatcher>(OoT_CreatePathFactory(),
                                                         std::make_shared<S2H::ResourceFactoryBinaryPathMMV0>()),
        RESOURCE_FORMAT_BINARY, "Path", static_cast<uint32_t>(S2H::ResourceType::SOH_Path), 0,
        /*allowOverwrite=*/true);

    // Room (scene) and Cutscene — replace OoT's registrations with per-archive
    // dispatchers so MM assets parse with MM's wire formats (#344). Idempotent:
    // re-running just installs equivalent dispatchers. Registration happens
    // before any MM scene load (MM_Game_Init runs it ahead of the graph
    // thread), so no load can race the overwrite.
    loader->RegisterResourceFactory(
        std::make_shared<RsbsMMArchiveFactoryDispatcher>(OoT_CreateSceneFactory(),
                                                         std::make_shared<S2H::ResourceFactoryBinarySceneV0>()),
        RESOURCE_FORMAT_BINARY, "Room", static_cast<uint32_t>(S2H::ResourceType::SOH_Room), 0,
        /*allowOverwrite=*/true);
    loader->RegisterResourceFactory(
        std::make_shared<RsbsMMArchiveFactoryDispatcher>(OoT_CreateCutsceneFactory(),
                                                         std::make_shared<S2H::ResourceFactoryBinaryCutsceneV0>()),
        RESOURCE_FORMAT_BINARY, "Cutscene", static_cast<uint32_t>(S2H::ResourceType::SOH_Cutscene), 0,
        /*allowOverwrite=*/true);

    // TextMM — MM-only text format
    loader->RegisterResourceFactory(std::make_shared<S2H::ResourceFactoryBinaryTextMMV0>(), RESOURCE_FORMAT_BINARY,
                                    "TextMM", static_cast<uint32_t>(S2H::ResourceType::TSH_TextMM), 0);

    // TextureAnimation — MM-only
    loader->RegisterResourceFactory(std::make_shared<S2H::ResourceFactoryBinaryTextureAnimationV0>(),
                                    RESOURCE_FORMAT_BINARY, "TextureAnimation",
                                    static_cast<uint32_t>(S2H::ResourceType::TSH_TexAnim), 0);

    // KeyFrame animation and skeleton — MM-only
    loader->RegisterResourceFactory(std::make_shared<S2H::ResourceFactoryBinaryKeyFrameAnim>(), RESOURCE_FORMAT_BINARY,
                                    "KeyFrameAnim", static_cast<uint32_t>(S2H::ResourceType::TSH_CKeyFrameAnim), 0);
    loader->RegisterResourceFactory(std::make_shared<S2H::ResourceFactoryBinaryKeyFrameSkel>(), RESOURCE_FORMAT_BINARY,
                                    "KeyFrameSkel", static_cast<uint32_t>(S2H::ResourceType::TSH_CKeyFrameSkel), 0);

    fprintf(stderr, "[MM] Registered MM resource factories\n");
}

/**
 * Headless entry for the boot-mm regression test (#330,
 * src/common/test_runner.cpp): registers MM's resource factories against the
 * shared ResourceManager, which is exactly what MM_Game_Init needs from the
 * shared bring-up before it can load archives. Returns 0 on success, -1 when
 * the shared context has no ResourceManager (the #329/#330 failure mode).
 */
extern "C" int MM_RegisterResourceFactoriesHeadless(void) {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetResourceManager() == nullptr ||
        ctx->GetResourceManager()->GetResourceLoader() == nullptr) {
        return -1;
    }
    RegisterMMResourceFactories();
    return 0;
}

/**
 * Verify the shared Ship::Context is ready for MM's graph thread (#271).
 *
 * MM_Graph_ThreadEntry calls WindowIsRunning(), GfxDebuggerIsDebugging(),
 * WindowGetWidth/Height/AspectRatio() every frame — all route through
 * Ship::Context::GetInstance(). MM_Game_Init runs the shared bring-up
 * (InitOTRForMMFirstBoot, #330) before this check, so by the time it runs
 * the window must exist whichever game booted first — this is a
 * postcondition assertion on the bring-up, not a dependency on OoT having
 * booted first.
 */
static bool VerifySharedContext(void) {
    auto ctx = Ship::Context::GetInstance();
    if (!ctx) {
        fprintf(stderr, "[MM] ERROR: Ship::Context is null — harness must init first\n");
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

/**
 * Audio-editor sequence queries, MM side. The real implementations live in
 * 2s2h/Enhancements/Audio/AudioEditor.cpp, which is excluded from single-exe
 * builds (its UI needs BenGui/BenMenu, not yet ported), so MM's audio core
 * gets identity mappings here: no sequence replacement or randomization for
 * MM until that UI ports. Identity, NOT 0 — the untyped mm_stubs.c stub this
 * replaces returned 0 for every seqId, funneling every
 * MM_AudioLoad_SetSeqLoadStatus write into slot 0. The header declarations
 * (renamed to MM_AudioEditor_* by include/mm_audio_prefix.h) keep these
 * signature-checked.
 *
 * NOTE for whoever ports the MM audio editor: both games' editors persist
 * replacements under the same "gAudioEditor.*" CVar keys, so wiring this to
 * S2H::AudioCollection::GetReplacementSequence for real also needs a per-game
 * CVar prefix split.
 */
u16 AudioEditor_GetReplacementSeq(u16 seqId) {
    return seqId;
}

u16 AudioEditor_GetOriginalSeq(u16 seqId) {
    // NOT the identity: callers index the 128-entry MM_sSeqFlags[] with the
    // result, and two call sites (code_8019AF00.c Audio_PlayBgm_StorePrevBgm
    // and Audio_UpdateEnemyBgmVolume) reach here with NA_BGM_DISABLED
    // (0xFFFF) while the main BGM player is stopped — identity would read
    // ~64KB past the array. Clamping ids past GetMaxOriginalSeqId() (0x7F)
    // to 0 reproduces upstream AudioCollection::GetOriginalSequence with no
    // replacement CVars set: original ids map to themselves, everything
    // else (custom ids, NA_BGM_DISABLED) resolves to 0.
    return seqId <= 0x7F ? seqId : 0;
}

/**
 * Real single-exe definition for #372. MM's implementation TU
 * (2s2h/GameInteractor/GameInteractor.cpp) is excluded ("use OoT's") and OoT
 * defines no GameInteractor_InvertControl, so the old src/common/mm_stubs.c
 * stub was the sole definition — and it returned the ENUM ORDINAL as the
 * multiplier. Every caller multiplies a stick axis by the result:
 * Lib_GetControlStickData ran x *= 2 (GI_INVERT_MOVEMENT_X == 2) on every
 * movement frame, skewing every analog angle toward the X axis, and
 * Actor_SetControlStickData's s8 path wrapped past half deflection.
 *
 * Body lifted from upstream 2s2h/GameInteractor/GameInteractor.cpp
 * (GameInteractor_InvertControl; unchanged except explicit default: branches
 * to keep -Wswitch quiet) — CVar-driven, so the shipped 2S2H defaults
 * apply (camera right-stick Y and first-person aim/right-stick Y default to
 * inverted) and Mirrored World keeps working if its toggle ever ports.
 * Declared extern "C" in MM's GameInteractor.h, which this TU includes:
 * signature drift here is a compile error, retiring this symbol from the
 * mm_stubs.c drift class.
 */
int GameInteractor_InvertControl(GIInvertType type) {
    int result = 1;

    switch (type) {
        case GI_INVERT_CAMERA_RIGHT_STICK_X:
            if (CVarGetInteger("gEnhancements.Camera.RightStick.InvertXAxis", 0)) {
                result *= -1;
            }
            break;
        case GI_INVERT_CAMERA_RIGHT_STICK_Y:
            if (CVarGetInteger("gEnhancements.Camera.RightStick.InvertYAxis", 1)) {
                result *= -1;
            }
            break;
        case GI_INVERT_FIRST_PERSON_AIM_X:
            if (CVarGetInteger("gEnhancements.Camera.FirstPerson.InvertX", 0)) {
                result *= -1;
            }
            break;
        case GI_INVERT_FIRST_PERSON_AIM_Y:
            if (CVarGetInteger("gEnhancements.Camera.FirstPerson.InvertY", 1)) {
                result *= -1;
            }
            break;
        case GI_INVERT_FIRST_PERSON_GYRO_X:
            if (CVarGetInteger("gEnhancements.Camera.FirstPerson.GyroInvertX", 0)) {
                result *= -1;
            }
            break;
        case GI_INVERT_FIRST_PERSON_GYRO_Y:
            if (CVarGetInteger("gEnhancements.Camera.FirstPerson.GyroInvertY", 0)) {
                result *= -1;
            }
            break;
        case GI_INVERT_FIRST_PERSON_RIGHT_STICK_X:
            if (CVarGetInteger("gEnhancements.Camera.FirstPerson.RightStickInvertX", 0)) {
                result *= -1;
            }
            break;
        case GI_INVERT_FIRST_PERSON_RIGHT_STICK_Y:
            if (CVarGetInteger("gEnhancements.Camera.FirstPerson.RightStickInvertY", 1)) {
                result *= -1;
            }
            break;
        case GI_INVERT_SHIELD_Y:
            if (CVarGetInteger("gEnhancements.Equipment.InvertShieldY", 0)) {
                result *= -1;
            }
            break;
        default:
            break;
    }

    // Invert all X axis inputs if the Mirrored World mode is enabled
    if (CVarGetInteger("gModes.MirroredWorld.State", 0)) {
        switch (type) {
            case GI_INVERT_CAMERA_RIGHT_STICK_X:
            case GI_INVERT_MOVEMENT_X:
            case GI_INVERT_SHIELD_X:
            case GI_INVERT_SHOP_X:
            case GI_INVERT_HORSE_X:
            case GI_INVERT_ZORA_SWIM_X:
            case GI_INVERT_DEBUG_DPAD_X:
            case GI_INVERT_TELESCOPE_X:
            case GI_INVERT_FIRST_PERSON_AIM_X:
            case GI_INVERT_FIRST_PERSON_GYRO_X:
            case GI_INVERT_FIRST_PERSON_RIGHT_STICK_X:
            case GI_INVERT_FIRST_PERSON_MOVING_X:
                result *= -1;
                break;
            default:
                break;
        }
    }

    return result;
}

/**
 * Real single-exe definitions for the last two GameInteractor input shims
 * that lived in src/common/mm_stubs.c — same drift class and same
 * treatment as GameInteractor_InvertControl above (#372 / PR #415): MM's
 * implementation TU (2s2h/GameInteractor/GameInteractor.cpp) is excluded
 * from the link ("use OoT's") and OoT defines neither symbol, so the
 * untyped stubs were the sole definitions.
 *
 * The old stub `int GameInteractor_Dpad(void* input, int dpad)` returned
 * the button combo unconditionally, force-enabling both CVar-gated D-pad
 * enhancements: BTN_DPAD_EQUIP (include/z64save.h) expanded to BTN_DPAD as
 * if gEnhancements.DpadEquips were on, and the ocarina paths in
 * src/audio/code_8019AF00.c treated the D-pad as ocarina buttons as if
 * gEnhancements.Playback.DpadOcarina were on.
 *
 * Bodies lifted verbatim from upstream 2s2h/GameInteractor/
 * GameInteractor.cpp so the shipped 2S2H defaults apply (both
 * enhancements off until their CVars are set). Declared extern "C" in
 * MM's GameInteractor.h, which this TU includes: signature drift here is
 * a compile error.
 */
uint32_t GameInteractor_Dpad(GIDpadType type, uint32_t buttonCombo) {
    uint32_t result = 0;

    switch (type) {
        case GI_DPAD_OCARINA:
            if (CVarGetInteger("gEnhancements.Playback.DpadOcarina", 0)) {
                result = buttonCombo;
            }
            break;
        case GI_DPAD_EQUIP:
            if (CVarGetInteger("gEnhancements.DpadEquips", 0)) {
                result = buttonCombo;
            }
            break;
    }

    return result;
}

/**
 * The old stub `int GameInteractor_RightStickOcarina(void* input)` returned
 * 0, which matched the enhancement's default-off CVar
 * (gEnhancements.Playback.RightStickOcarina) — but it kept the enhancement
 * dead even when enabled, and its untyped signature (void* vs Input*) sat
 * in the same drift class. Body lifted verbatim from upstream.
 */
uint32_t GameInteractor_RightStickOcarina(Input* input) {
    uint32_t result = 0;

    if (!CVarGetInteger("gEnhancements.Playback.RightStickOcarina", 0)) {
        return result;
    }

    s8 rstick_x = input->cur.right_stick_x;
    s8 rstick_y = input->cur.right_stick_y;
    const s8 sensitivity = 64;

    if (rstick_x > sensitivity) {
        result |= BTN_CRIGHT;
    } else if (rstick_x < -sensitivity) {
        result |= BTN_CLEFT;
    }

    if (rstick_y > sensitivity) {
        result |= BTN_CUP;
    } else if (rstick_y < -sensitivity) {
        result |= BTN_CDOWN;
    }

    return result;
}

extern "C" void MM_Rando_Init(void); // defined below MM_Game_Init

int MM_Game_Init(int argc, char** argv) {
    fprintf(stderr, "[MM] Game_Init called, argc=%d\n", argc);
    fflush(stderr);

    // The harness (rsbs/src/main.cpp) creates the Ship::Context singleton
    // uninitialized. If MM is the first game to boot, run the shared
    // bring-up here (#330); when OoT booted first this is a no-op. See the
    // file header and InitOTRForMMFirstBoot's doc comment for why the
    // bring-up is shared with OoT rather than MM-specific.
    {
        auto ctx = Ship::Context::GetInstance();
        if (ctx == nullptr || ctx->GetWindow() == nullptr) {
            fprintf(stderr, "[MM] Shared context uninitialized — running shared bring-up (#330)\n");
            fflush(stderr);
            InitOTRForMMFirstBoot(argc, argv);
        }
    }

    if (!VerifySharedContext()) {
        fprintf(stderr, "[MM] FATAL: Cannot start without Ship::Context\n");
        return -1;
    }

    // Load MM's archives into the shared ResourceManager (issue #159).
    if (LoadMMArchives() != 0) {
        fprintf(stderr, "[MM] FATAL: Failed to load MM archives\n");
        return -1;
    }

    // Register MM-only resource factories (issue #159).
    RegisterMMResourceFactories();

    // Create MM's own AudioCollection (mirrors BenPort.cpp's InitOTR, which is
    // excluded from single-exe builds). Must precede MM_AudioMgr_Init: the
    // audio-load path registers custom sequences through
    // MM_AudioCollection_{HasSequenceNum,AddToCollection}. Guarded for
    // re-entry after a cross-game switch.
    if (AudioCollection::Instance == nullptr) {
        AudioCollection::Instance = new AudioCollection();
    }

    // Populate MM's message tables from the archives (#344). Must run after
    // LoadMMArchives + RegisterMMResourceFactories (needs the TextMM factory).
    // Idempotent — re-entry after a game switch keeps the existing tables.
    MM_OTRMessage_Init();

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

    // Bring up MM's randomizer (Lane C0, #392). Upstream 2S2H does this from
    // BenPort.cpp's InitOTR (excluded TU): run every MM ShipInit registrar —
    // which is what populates the 2ship_rando Logic/Regions graph and the
    // rando-affected enhancement hooks — then Rando::Init(). All hook
    // registrations land in the MM-owned S2H::GameHooks registry
    // (include/mm_game_hooks.h), never the shared C++ GameInteractor (#395);
    // per-type dispatch is wired deliberately where MM's own code paths
    // execute each hook. Runs after MM_OTRMessage_Init/audio bring-up so the
    // spoiler/CVar/file paths Rando::Init touches are live.
    MM_Rando_Init();

    // Register integration test hooks if in integration test mode
    MM_RegisterIntegrationTestHooks();

    fprintf(stderr, "[MM] Game_Init complete\n");
    fflush(stderr);
    return 0;
}

/**
 * One-shot MM randomizer bring-up (Lane C0, #392) — extern "C" so the
 * headless test harness (src/common/test_runner.cpp) can drive it without
 * including MM C++ headers. Once-only guarded: MM_Game_Init re-runs on
 * MM re-entry paths, but ShipInit registrars and Rando::Init's hook
 * registrations must not double-register (the single-exe resume contract —
 * see MM_Game_Resume).
 */
// The GIEvent pump registrar (Lane C1) — defined in
// games/mm/2s2h/GameInteractorEventsSingleExe.cpp.
extern "C" void MM_GameEvents_RegisterPump(void);
// MM tracker windows on the shared Gui (#392) — defined in
// games/mm/2s2h/TrackersGuiSingleExe.cpp.
extern "C" void MM_TrackersGui_Init(void);

// Cycle-safe shared rupees (#525) — defined further down THIS TU, beside the
// rest of the MM shared-resource half.
extern "C" void MM_RegisterSharedResourceCycleHooks(void);

// Defined below in this TU; forward-declared so the #516 GfxPatcher gate can
// reference it from MM_Rando_Init above the definition.
extern "C" bool MM_Rando_AssetsReady(void);

extern "C" void MM_Rando_Init(void) {
    static bool sRandoInitDone = false;
    if (sRandoInitDone) {
        return;
    }
    sRandoInitDone = true;

    fprintf(stderr, "[MM] MM_Rando_Init: running ShipInit registrars + Rando::Init\n");
    fflush(stderr);
    S2H::ShipInit::InitAll();
    Rando::Init();

    // #516: registrars orphaned by the excluded BenPort InitOTR sequence.
    //
    // BenPort.cpp (MM's OTRGlobals equivalent) is excluded from the single exe —
    // it drags in a second window/Gui/config bring-up and the OoT-aliasing
    // hazard class the shim exists to avoid — so every Init*/Register* it was
    // the sole caller of got link-elided. This is not a "recompile BenPort"
    // fix: only the individually single-exe-safe registrars are re-homed, here,
    // by explicit call. An explicit call (rather than a per-TU
    // RegisterShipInitFunc) is deliberate: 2ship_enh is a plain STATIC archive,
    // so a registrar's own static initializer would be stripped with its
    // unreferenced TU — the exact mechanism that elided these. A hard call from
    // this always-linked TU forces /OPT:REF to keep the symbol AND pulls the TU
    // into the link, and it is the reference the #516 CI probe asserts on.
    //
    // This block MUST run exactly once: CustomItem/CustomMessage register through
    // raw RegisterForID (no unregister-first) and RegisterSavingEnhancements
    // through plain Register<>, none of which de-dup. The sRandoInitDone guard
    // above provides that — MM_Game_Resume does not re-enter here.
    //
    // Both hook registrars only became LIVE with #512/#514/#515, which wired the
    // MM ShouldActorInit and OnOpenText execute points their bodies were written
    // against; before that, reviving them here would have registered into a
    // registry nothing dispatched.
    CustomItem::RegisterHooks();    // ShouldActorInit[EN_ITEM00]: the swap that
                                    // makes CustomItem::Spawn's placeholder a
                                    // real item — cross-game arrivals + every
                                    // rando reward. #516 critical.
    CustomMessage::RegisterHooks(); // OnOpenText[0x4B]: loads staged rando/hint
                                    // text; without it every custom message
                                    // renders vanilla entry 0x4B. #516 critical.
    // #516 Phase 2 — save mechanics. RegisterSavingEnhancements gives owl-save
    // persistence, cycle-save playtime banking, grotto respawn restore and
    // moon-crash owl cleanup; RegisterAutosave gives MM its periodic owl autosave
    // and its on-screen icon.
    //
    // These were held back from Phase 1 because their DeleteOwlSave leg (on
    // BeforeMoonCrashSaveReset / BeforeEndOfCycleSave) dereferences MM_gPlayState,
    // which crashed the headless MMMoonCrashArmState test — the only test that
    // drives a real reset path. That test now stands up a play state (as
    // production always has: z_demo.c:379 resets from a live &play->sramCtx), so
    // the leg runs faithfully rather than being avoided.
    //
    // The Autosave frame legs are headless-safe as-is and need no scaffolding:
    // HandleAutoSave's interval guard and DrawAutosaveIcon's iconTimer==0 guard
    // both return before touching MM_gPlayState, and OnGameStateUpdate/DrawFinish
    // dispatch only from MM's real frame loop (game.c), never a headless test.
    RegisterSavingEnhancements();
    RegisterAutosave();

    // GfxPatcher is a one-shot resource patcher, not a hook registrar, so it is
    // called (not registered) and gated on assets: its ResourceMgr_Load*ByName
    // helpers null-deref with no mm.o2r, and mm_rando_gen_test.cpp drives this
    // function ROM-free. Fixes OOB textures, mini-game symbols, and a latent
    // matrix-stack UB the smithy chimney-fire DL hits (per its own comment).
    if (MM_Rando_AssetsReady()) {
        GfxPatcher_ApplyNecessaryAuthenticPatches();
    }

    // Lane C1 (#392): register the GIEvent pump (the port of upstream's
    // ProcessEvents player-update hook, games/mm/2s2h/
    // GameInteractorEventsSingleExe.cpp). Upstream registered it from
    // GameInteractor::RegisterOwnHooks at boot; in the single exe only the
    // rando queue produces GIEvents, so the rando bring-up is its natural
    // (once-only, same guard) home.
    MM_GameEvents_RegisterPump();

    // MM tracker windows (#392): register on the shared Gui, gated to draw
    // only while MM is the active game. Upstream did this from BenGui.cpp's
    // SetupGuiElements (excluded); the bypass surface lives in
    // 2s2h/TrackersGuiSingleExe.cpp. No-op when the harness has no window.
    MM_TrackersGui_Init();

    // Shared cross-game resources (#525): keep the SHARED rupee pool alive
    // across MM's three-day cycle. Registered from HERE, not from
    // 2s2h/Enhancements/Cycle/EndOfCycle.cpp where the analogous
    // DoNotResetRupees restore lives, because that TU is link-elided from the
    // plain-archive 2ship_enh — its six registrants are absent from the binary
    // (verified against redship.map; see the Before/AfterEndOfCycleSave
    // dispatch comment below). Registering there would be the #516/#513
    // elided-provider class exactly: code that reads correctly and never runs.
    // This TU is always linked, and the sRandoInitDone guard above is what
    // makes the registration once-only, as the block header requires.
    MM_RegisterSharedResourceCycleHooks();
}

/**
 * True once LoadMMArchives() registered mm.o2r with the shared
 * ResourceManager. ShipInit registrar lambdas that copy display lists out of
 * MM assets (Rando/DrawItem.cpp, Rando/ActorBehavior/EnBox.cpp) gate on this:
 * in the ROM-free unit harness no archives exist and
 * ResourceMgr_LoadGfxByName null-derefs on a missing resource. In the real
 * boot path LoadMMArchives always precedes MM_Rando_Init.
 */
extern "C" bool MM_Rando_AssetsReady(void) {
    return sMMArchivesLoaded;
}

void MM_Game_Run(void) {
    fprintf(stderr, "[MM] Game_Run called, entering MM_Graph_ThreadEntry()\n");
    fflush(stderr);
    // Assert context still valid before graph loop (issue #158)
    assert(Ship::Context::GetInstance() != nullptr && "Ship::Context must be valid when MM graph thread starts");
    assert(Ship::Context::GetInstance()->GetWindow() != nullptr && "Window must be valid when MM graph thread starts");

    // Run the main game loop
    MM_Graph_ThreadEntry(nullptr);
    fprintf(stderr, "[MM] MM_Graph_ThreadEntry() returned\n");
    fflush(stderr);
}

// Shared cross-game resources (#525) — defined further down this TU beside the
// apply half; forward-declared so Game_Suspend and the unified-save capture can
// harvest before they hand MM's state over.
extern "C" void MM_HarvestSharedResources(void);

/**
 * Suspend MM for a game switch (issue #270).
 * Stops audio to prevent interference with OoT, keeps libultraship context and
 * MM heaps alive. Mirrors OoT_Game_Suspend so OoT <-> MM round-trips don't
 * re-init either game.
 */
void MM_Game_Suspend(void) {
    fprintf(stderr, "[MM] Game_Suspend called\n");
    fflush(stderr);

    // Producer (ADR 0002 / Lane A1) — mirrors OoT_Game_Suspend. Commit any
    // staged cross-game items into gComboCtx.sharedItemsTagged before handing
    // control to OoT. This lives at Game_Suspend, not Combo_CheckEntranceSwitch:
    // the F10 hot-swap path bypasses the entrance hook, and GameRunner_SwitchTo
    // calls suspend() on both switch paths, so this is the one point that never
    // drops a hotkey switch's writes.
    Combo_CommitStagedSharedItems();

    // Shared cross-game resources (#525) — mirrors OoT_Game_Suspend. Fold MM's
    // live rupees, wallet tier, hearts, current health and double defense into
    // the shared pool while gSaveContext still belongs to MM.
    MM_HarvestSharedResources();

    // Drain the SHARED audio thread before touching MM's audio state. In
    // single-exe builds MM's synth runs on OoT's OTRAudio_Thread (the
    // active-game dispatch in games/oot/soh/OTRGlobals.cpp) — a REAL
    // std::thread that can be mid-buffer inside MM's sequence/soundfont lazy
    // load path right now. The switch path continues into an archive
    // hot-swap; ripping resources out from under an in-flight synth is the
    // exact use-after-free OoT_Audio_DrainForSuspend was added to close on
    // the OoT side. (An earlier comment here claimed the port processes audio
    // synchronously on the game loop — the same stale claim the OoT side
    // already corrected; it was never true of the shared thread.)
    OoT_Audio_DrainForSuspend();

    // Stop MM audio playback to prevent interference with OoT (issue #270).
    // MM_Audio_PreNMI halts the players via AudioThread_PreNMIInternal.
    fprintf(stderr, "[MM] Stopping audio via PreNMI path...\n");
    fflush(stderr);
    MM_Audio_PreNMI();

    // Mark audio as uninitialized so message-queue re-init works on resume.
    gAudioCtxInitalized = false;

    // Retire the graph coroutine (mirrors OoT_Game_Suspend): the cross-game
    // entrance path stops the gamestate with init/destroy nulled, so resuming
    // MM_RunFrame's state machine would walk into a dead gamestate. The next
    // MM entry cold-starts the gamestate chain — the same (working) path as
    // MM's first entry — with continuity from the frozen SaveContext + the
    // MM-tagged startup entrance consumed in MM_Play_Init.
    fprintf(stderr, "[MM] Retiring graph coroutine for switch...\n");
    fflush(stderr);
    MM_Graph_ResetRunFrameContext();

    fprintf(stderr, "[MM] Game_Suspend complete\n");
    fflush(stderr);
}

/**
 * Re-arm MM's per-session allocators for a cold gamestate-chain boot.
 *
 * Every MM re-entry cold-starts the full gamestate chain (see
 * MM_Graph_ResetRunFrameContext, games/mm/src/code/graph.c): the previous
 * session's system-arena contents are unreachable by design — the switch
 * path retires the live Play gamestate without running the frame loop's
 * destroy/free epilogue, and Play's GameState_Realloc(&state, 0) had taken
 * the entire largest free block of the 32MB arena. MM_SystemHeap_Init lives
 * only in MM_Game_Init, so without this re-arm the second entry's first
 * gamestate malloc (graph.c MM_RunFrame) returns NULL and dies. OoT never
 * had this fault because OoT_Game_Run re-enters Main(), which re-runs its
 * own SystemHeap_Init on every entry — this mirrors that contract.
 *
 * Regs_Init must re-run with it: gRegEditor (games/mm/src/code/z_debug.c)
 * was the old arena's first resident and is dereferenced unchecked
 * throughout the boot chain; a fresh arena's first allocation would
 * otherwise overwrite it.
 *
 * extern "C" and standalone so the mm-resume-arena headless test
 * (mm_resume_state_test.cpp, CTest label "redship") can lock the contract:
 * an exhausted arena must be allocatable again after this call.
 */
extern "C" void MM_ResumeColdBootPrep(void) {
    MM_SystemHeap_Init((void*)MM_gSystemHeap, SYSTEM_HEAP_SIZE);
    Regs_Init();
}

/**
 * Commit MM's LIVE SaveContext into the unified .redsave slot (#35 follow-up).
 *
 * WHY THIS EXISTS. In single-exe builds MM has no persistence whatsoever:
 * games/mm/CMakeLists.txt filters out 2s2h/SaveManager/*.cpp, and the linked
 * replacement (games/mm/2s2h/mm_save_manager_stubs.c) returns -1 from
 * SaveManager_SysFlashrom_ReadData and has an EMPTY SaveManager_SysFlashrom_-
 * WriteData body. Every MM save route — new-cycle, owl, pause save-and-quit,
 * autosave — funnels through those two symbols, so MM writes nothing at all.
 * Before this, the ONLY thing that ever put real MM bytes into the cross-game
 * shadow was the departure freeze in Combo_CheckEntranceSwitch, i.e. MM state
 * was captured at exactly one instant: walking back through the portal. A
 * session ended any other way (owl save, quit, closing the window) recorded
 * nothing, which is why a .redsave's Tier-3 reads as 65536 zero bytes.
 *
 * This is the redship-native capture path: it does not go anywhere near MM's
 * flash funnel, which is dead-ended twice over (the no-op stub, and the
 * gSaveContext.fileNum != 0xFF gates that a cross-game session can never
 * satisfy). It also cannot route through OoT's GameInteractor hooks, because
 * GI_SINGLE_EXE_GATE returns early from every OoT executor for the entire MM
 * session.
 *
 * FULL WIDTH, deliberately. The shadow write passes MM's real
 * sizeof(SaveContext) — this TU is one of the few that can see it — and NOT
 * the sizeof(Save) prefix the excluded SaveManager.cpp used. Context_Update-
 * ShadowCopy does not zero the tail, so a prefix write would leave everything
 * past Save (eventInf, cycleSceneFlags, the timer arrays, the runtime respawn
 * table, ShipSaveContext) at whatever a DIFFERENT point in time left there.
 *
 * @return 1 on a committed write, 0 when there was nothing to write to.
 */
extern "C" int MM_Combo_CaptureSaveToUnifiedSlot(void) {
    const int slot = RsbsSave_GetActiveSlot();
    if (slot < 0) {
        // No slot established this session. Honest no-op rather than a guess:
        // defaulting to slot 0 here would let an MM session started from an
        // unsaved new file overwrite an unrelated slot.
        fprintf(stderr, "[MM] unified save skipped: no active slot for this session\n");
        fflush(stderr);
        return 0;
    }

    // Shared cross-game resources (#525) BEFORE the shadow capture, so the pool
    // and the MM blob stored beside it agree. Without it, money earned since the
    // last switch lives in the MM save but not in the pool, and the post-load
    // first-harvest seed reads that balance as already counted and drops it.
    MM_HarvestSharedResources();

    Context_UpdateShadowCopy(GAME_MM, &gSaveContext, sizeof(gSaveContext));
    gComboCtx.sourceGame = GAME_MM;

    const int ok = RsbsSave_Save(slot);
    fprintf(stderr, "[MM] unified save to slot %d: %s\n", slot, ok ? "ok" : "FAILED");
    fflush(stderr);
    return ok;
}

/**
 * Resume MM after being suspended for a game switch (issue #170, #270).
 * - Re-arms the system arena for the cold gamestate-chain boot (see
 *   MM_ResumeColdBootPrep — the retired session's allocations leak by
 *   design and would otherwise exhaust the arena).
 * - Restores frozen MM SaveContext so gameplay state survives the OoT
 *   round-trip (OoT scribbles over the unified gSaveContext storage while
 *   it is active — see src/common/unified_save.c).
 * - Reinitializes audio message queues for clean state.
 */
void MM_Game_Resume(void) {
    fprintf(stderr, "[MM] Game_Resume called\n");
    fflush(stderr);

    fprintf(stderr, "[MM] Re-arming system arena for cold boot...\n");
    fflush(stderr);
    MM_ResumeColdBootPrep();

    // Restore the frozen MM SaveContext captured before we left for OoT (#170).
    // NOTE: with the cold-boot contract this restore is defense in depth, not
    // the continuity mechanism — the boot chain wipes gSaveContext again
    // (Setup -> MM_SaveContext_Init, TitleSetup -> MM_Sram_InitNewSave), and
    // the restore that reaches gameplay is the one MM_Play_ConsumeStartupEntrance
    // (z_play.c) performs at startup-entrance consumption. The main loop sets
    // a startup entrance for every switch into a frozen game (entrance-based
    // and hotkey alike — rsbs/src/main.cpp), so that path always runs when
    // there is something to restore. First boot of MM has no frozen state —
    // in that case MM_InitFirstEntrySaveContext still handles bootstrap (#168).
    if (Context_HasFrozenState(GAME_MM)) {
        fprintf(stderr, "[MM] Restoring frozen SaveContext on resume\n");
        fflush(stderr);
        Context_RestoreState(GAME_MM, &gSaveContext, sizeof(gSaveContext));

        // Prefer the startup entrance set for this switch; fall back to the
        // return entrance recorded when we last froze MM. Use the explicit
        // Has accessor because entrance 0x0000 is a valid id and treating it
        // as "unset" would silently drop a legitimate restore.
        // Query the MM-scoped accessor: an OoT-tagged value that leaked into
        // the shared startup global stays invisible to MM, so we fall back to
        // MM's frozen return entrance instead of spawning from an OoT id
        // (symmetric with OoT_Game_Resume).
        bool hasStartup = Combo_HasStartupEntranceForGame("mm");
        uint16_t targetEntrance =
            hasStartup ? Combo_GetStartupEntranceForGame("mm") : Context_GetFrozenReturnEntrance(GAME_MM);
        gSaveContext.save.entrance = targetEntrance;
        fprintf(stderr, "[MM] Resume entrance: 0x%04X (startup=%u)\n", targetEntrance, hasStartup);
    }

    // Reinitialize audio message queues for clean state (issue #270).
    // The audio context's queue pointers may be stale after suspend's
    // PreNMI shutdown — mirrors the same call MM_Game_Init makes on first
    // entry, and the same Audio_InitMesgQueues call OoT_Game_Resume makes.
    fprintf(stderr, "[MM] Reinitializing audio message queues...\n");
    fflush(stderr);
    AudioThread_InitMesgQueues();

    // Re-arm the shared audio thread's MM dispatch (OTRGlobals.cpp gates the
    // MM synth on this flag). The audio heap and context genuinely remain
    // initialized across suspend — MM_AudioLoad_Init runs once in
    // MM_Game_Init and only MM_Game_Shutdown frees the heaps — suspend just
    // cleared the flag to park the dispatch on silence while OoT runs.
    gAudioCtxInitalized = true;
    MM_Audio_ResumeFromPreNMI();

    // Restart the sound system: suspend's PreNMI halted the sequence
    // players, and the guarded MM_AudioMgr_Init bring-up never re-runs. The
    // queued commands sit in the ring until the PreNMI-scheduled heap reset
    // finishes on the shared audio thread, then apply — after which the
    // arrival scene starts its BGM from scratch (the startup-entrance
    // consumption resets the restored save's stale "already playing" ids).
    MM_Audio_InitSound();

    fprintf(stderr, "[MM] Game_Resume complete\n");
    fflush(stderr);
}

void MM_Game_Shutdown(void) {
    fprintf(stderr, "[MM] Game_Shutdown called\n");
    fflush(stderr);
    // Don't call DeinitOTR() - the shared context stays alive
    // Reset audio state so re-init works after game switch (issue #157)
    gAudioCtxInitalized = false;
    MM_Heaps_Free();
    sMMInitialized = false;
    sMMArchivesLoaded = false;
    fprintf(stderr, "[MM] Game_Shutdown complete\n");
    fflush(stderr);
}

const char* MM_Game_GetName(void) {
    return "Majora's Mask";
}

const char* MM_Game_GetId(void) {
    return "mm";
}

/**
 * Room-load hook executors for MM's scene loader (#344).
 *
 * z_scene_2SH.cpp (MM_OTRfunc_8009728C) and z_play_2SH.cpp
 * (MM_OTRfunc_800973FC) call these; the real executors live in MM's
 * GameInteractor.cpp, which is excluded in single-exe builds. OnRoomInit and
 * AfterRoomSceneCommands are MM-only hook types (no OoT counterpart, so no
 * signature clash in the merged hook storage), and ExecuteHooks only touches
 * the per-hook-type inline-static maps — safe to run against the shared
 * GameInteractor instance that OoT's port layer owns.
 */
void GameInteractor_ExecuteOnRoomInit(s16 sceneId, s8 roomNum) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnRoomInit>(sceneId, roomNum);
}

void GameInteractor_ExecuteAfterRoomSceneCommands(s16 sceneId, s8 roomNum) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::AfterRoomSceneCommands>(sceneId, roomNum);
}

/**
 * MM-side hook DISPATCH (Lane C1, #392) — the Execute half of the #415 shim
 * contract. C0 parked MM's Rando/enh registrations in the MM-owned
 * S2H::GameHooks registries; these bridges run them at the points upstream
 * 2S2H dispatched each hook type. All of them go through S2H::GameHooks, NEVER
 * the shared GameInteractor instance: unlike OnRoomInit above, these hook
 * NAMES exist in both games, so touching the shared registry would be the
 * #367/#395 aliasing class. The mm-gi-shim CTest plus MMRandoGen lock the
 * chain (Sram_InitSave -> OnSaveInit -> OnFileCreate generation).
 *
 * OnSaveInit / OnSaveLoad replace the former mm_stubs.c no-ops: MM's C code
 * already calls them at the right places (z_sram_NES.c MM_Sram_InitSave;
 * z_file_choose_NES.c / z_opening.c / z_select.c save loads). OnActorUpdate
 * and the flag hooks are called from RSBS-guarded additions in
 * games/mm/src/code/z_actor.c, right beside the unqualified
 * GameInteractor_Execute* calls that cross-bind to OoT's (correctly no-op
 * while MM is active) wrappers.
 */
extern "C" void GameInteractor_ExecuteOnSaveInit(s16 fileNum) {
    S2H::GameHooks::Execute<GameInteractor::OnSaveInit>(fileNum);
}

extern "C" void GameInteractor_ExecuteOnSaveLoad(s16 fileNum) {
    S2H::GameHooks::Execute<GameInteractor::OnSaveLoad>(fileNum);
}

/**
 * OnGameStateUpdate / OnGameStateDrawFinish / OnPassPlayerInputs /
 * BeforeMoonCrashSaveReset replace former mm_stubs.c no-ops (#442 follow-up:
 * SavingEnhancements.cpp's raw registrations for these four hook types moved
 * onto S2H::GameHooks alongside the OOB-write fix, which would otherwise have
 * left autosave, its draw-icon, the entrance-cutscene-skip gameplay-started
 * detector, and owl-save-on-moon-crash-reset silently registered but never
 * dispatched). MM's own code already calls these at the right places —
 * games/mm/src/code/game.c MM_GameState_Update (every frame, mirroring the
 * OnGameStateMainStart pump from #415), games/mm/src/overlays/actors/
 * ovl_player_actor/z_player.c (every gameplay-input frame), and
 * games/mm/src/code/z_sram_NES.c (the moon-crash reset point) — so this is
 * the same "pump already exists, only the body was a no-op" pattern as
 * OnSaveInit/OnSaveLoad above. No ForFilter leg for OnPassPlayerInputs: the
 * S2H registry has no Ptr/Filter surface and the linked MM set registers
 * none (same omission #435 made for the Should family).
 */
extern "C" void GameInteractor_ExecuteOnGameStateUpdate() {
    S2H::GameHooks::Execute<GameInteractor::OnGameStateUpdate>();
}

extern "C" void GameInteractor_ExecuteOnGameStateDrawFinish() {
    S2H::GameHooks::Execute<GameInteractor::OnGameStateDrawFinish>();
}

extern "C" void GameInteractor_ExecuteOnPassPlayerInputs(Input* input) {
    S2H::GameHooks::Execute<GameInteractor::OnPassPlayerInputs>(input);
}

extern "C" void GameInteractor_ExecuteBeforeMoonCrashSaveReset() {
    S2H::GameHooks::Execute<GameInteractor::BeforeMoonCrashSaveReset>();
}

extern "C" void MM_GameHooks_ExecuteOnActorUpdate(Actor* actor) {
    S2H::GameHooks::Execute<GameInteractor::OnActorUpdate>(actor);
    S2H::GameHooks::ExecuteForID<GameInteractor::OnActorUpdate>(actor->id, actor);
    // ForPtr leg (upstream GameInteractor_ExecuteOnActorUpdate has all four
    // legs): registrants bind to one live actor by address — 2ship_enh's
    // SkipLearningSongOfHealing is the first shim user (#427 item 2). The
    // ForFilter leg stays unported: no compiled MM TU registers one.
    S2H::GameHooks::ExecuteForPtr<GameInteractor::OnActorUpdate>((uintptr_t)actor, actor);
}

/**
 * OnSceneInit dispatch (#392 tracker follow-up). Called from MM's Play_Init
 * (games/mm/src/code/z_play.c) at the exact point upstream 2S2H called its
 * excluded 2-arg GameInteractor_ExecuteOnSceneInit — which the single exe
 * could not call: the unprefixed name binds OoT's 1-arg executor, and firing
 * the shared registry would hit the #367 type-aliasing class. The MM-owned
 * S2H::GameHooks registry has neither problem. Runs every parked OnSceneInit
 * registration in the linked MM set: the check tracker's scroll-to-scene
 * hook (Rando/CheckTracker), Rando::MiscBehavior::OnSceneInit, and the
 * open-dungeons COND_ID_HOOKs (all IS_RANDO-conditioned). The ForFilter leg
 * is deliberately absent: the S2H registry has no filter surface and the
 * linked MM set registers none (same deviation as the Should executors).
 */
extern "C" void MM_GameHooks_ExecuteOnSceneInit(s16 sceneId, s8 spawnNum) {
    S2H::GameHooks::Execute<GameInteractor::OnSceneInit>(sceneId, spawnNum);
    S2H::GameHooks::ExecuteForID<GameInteractor::OnSceneInit>(sceneId, sceneId, spawnNum);
}

/**
 * OnActorInit / OnActorDraw / OnOpenText dispatch (#438).
 *
 * Reached through the macro rebind at the bottom of MM's GameInteractor.h, so
 * MM's call sites in z_actor.c / z_message.c stay textually upstream. Each
 * mirrors the leg set of its GameInteractor.cpp twin, minus ForFilter: the S2H
 * registry has no filter surface and no compiled MM TU registers one (the same
 * deviation the Should executors and OnSceneInit already document).
 *
 * The ForPtr legs are carried on the actor pair because upstream has them and
 * the ptr-keyed registry exists; no MM TU binds one today, so they cost an
 * empty map lookup and stop the next ptr-keyed registrant from being silently
 * dead the way these three were.
 */
extern "C" void MM_GameHooks_ExecuteOnActorInit(Actor* actor) {
    S2H::GameHooks::Execute<GameInteractor::OnActorInit>(actor);
    S2H::GameHooks::ExecuteForID<GameInteractor::OnActorInit>(actor->id, actor);
    S2H::GameHooks::ExecuteForPtr<GameInteractor::OnActorInit>((uintptr_t)actor, actor);
}

extern "C" void MM_GameHooks_ExecuteOnActorDraw(Actor* actor) {
    S2H::GameHooks::Execute<GameInteractor::OnActorDraw>(actor);
    S2H::GameHooks::ExecuteForID<GameInteractor::OnActorDraw>(actor->id, actor);
    S2H::GameHooks::ExecuteForPtr<GameInteractor::OnActorDraw>((uintptr_t)actor, actor);
}

/**
 * The id leg reads *textId AFTER the unkeyed leg has run, deliberately: an
 * unkeyed registrant may rewrite the text id, and upstream's
 * GameInteractor_ExecuteOnOpenText resolves its ExecuteHooksForID key from the
 * same post-mutation read. Hoisting it into a local before the first Execute
 * would be the more obvious spelling and would silently change which
 * registrants match.
 */
extern "C" void MM_GameHooks_ExecuteOnOpenText(u16* textId, bool* loadFromMessageTable) {
    S2H::GameHooks::Execute<GameInteractor::OnOpenText>(textId, loadFromMessageTable);
    S2H::GameHooks::ExecuteForID<GameInteractor::OnOpenText>(*textId, textId, loadFromMessageTable);
}

/**
 * OnActorKill / OnActorDestroy dispatch (#515) — the actor-lifecycle pair the
 * block above left behind.
 *
 * WHY THESE TWO OUTLIVED THE #511 AUDIT: neither name has an entry in
 * src/common/mm_stubs.c. The other members of this class announced themselves
 * as a no-op somebody had written down; these bound OoT's definitions in
 * soh/Enhancements/game-interactor/GameInteractor_Hooks.cpp purely because the
 * spelling matches, and OoT's first statement there is GI_SINGLE_EXE_GATE() —
 * an unconditional return for the whole MM session. Silent bind, no stub to
 * grep, no diagnostic. Both MM call sites (z_actor.c, MM_Actor_Kill and
 * MM_Actor_Delete) were live and unguarded the entire time.
 *
 * WHAT THAT COST, and why this one can make a seed unwinnable: EnemyDrops.cpp
 * registers the only code that pays out the 18 DROP_TYPE_KILL enemies, and
 * ObjGrass.cpp's ACTOR_OBJ_GRASS_UNIT registrant is the SOLE writer of
 * RandoCheckIds onto non-actor grass elements — every RC_*_GRASS_* check, 216
 * in Termina Field alone and every other grass location from Romani Ranch to
 * the cow grottos. Grass carrying a check looks and behaves exactly vanilla, so
 * there is no on-screen tell: a seed that placed a required item behind one
 * could not be finished, and nothing told the player why.
 *
 * ExecuteForID IS LOAD-BEARING, not defensive like the ForPtr legs. ObjGrass
 * registers through COND_ID_HOOK(OnActorKill, ACTOR_OBJ_GRASS_UNIT, ...), so an
 * Execute-only bridge compiles, links, fixes the enemy drops, reads correctly,
 * and leaves every grass check exactly as dead as it was.
 *
 * THE PAIR LANDS TOGETHER, and the reason runs the opposite way to the usual
 * before/after pairing argument: OnActorDestroy is harmless TODAY ONLY BECAUSE
 * OnActorKill is dead. The grass registrant keys its ObjectExtension entries on
 * element addresses INTERIOR to the ObjGrass actor allocation
 * (&grassGroup->elements[j]), and ObjectExtension's key is the exact pointer —
 * z_actor.c's own ObjectExtension_Free(actor) in MM_Actor_Delete erases only
 * entries whose key is the actor base address, so it does not reach them.
 * ObjGrass.cpp's ACTOR_OBJ_GRASS OnActorDestroy registrant is their only
 * reaper. Wiring Kill alone would start creating hundreds of entries per scene
 * with nothing to free them, keyed on arena addresses MM_ZeldaArena_Free then
 * recycles — a later actor landing on a stale key reads a cross-scene
 * RandoCheckId back out of GetObjectRandoCheckId. That is a worse bug than the
 * one being fixed, which is why half of this change must never ship alone.
 *
 * Leg sets mirror the GameInteractor.cpp twins (:166-178) minus ForFilter, the
 * standing deviation: the S2H registry has no filter surface. The ForPtr legs
 * ride along for the same reason the #512 actor pair carries them — upstream
 * has them and the ptr-keyed registry exists, while the one ptr registrant in
 * the tree (ActorViewer.cpp, OnActorDestroy) sits in link-elided
 * DeveloperTools. They cost an empty map lookup and stop the next ptr-keyed
 * registrant from being silently dead the way these two were.
 */
extern "C" void MM_GameHooks_ExecuteOnActorKill(Actor* actor) {
    S2H::GameHooks::Execute<GameInteractor::OnActorKill>(actor);
    S2H::GameHooks::ExecuteForID<GameInteractor::OnActorKill>(actor->id, actor);
    S2H::GameHooks::ExecuteForPtr<GameInteractor::OnActorKill>((uintptr_t)actor, actor);
}

extern "C" void MM_GameHooks_ExecuteOnActorDestroy(Actor* actor) {
    S2H::GameHooks::Execute<GameInteractor::OnActorDestroy>(actor);
    S2H::GameHooks::ExecuteForID<GameInteractor::OnActorDestroy>(actor->id, actor);
    S2H::GameHooks::ExecuteForPtr<GameInteractor::OnActorDestroy>((uintptr_t)actor, actor);
}

extern "C" void MM_GameHooks_ExecuteOnFlagSet(FlagType flagType, u32 flag) {
    S2H::GameHooks::Execute<GameInteractor::OnFlagSet>(flagType, flag);
}

extern "C" void MM_GameHooks_ExecuteOnSceneFlagSet(s16 sceneId, FlagType flagType, u32 flag) {
    S2H::GameHooks::Execute<GameInteractor::OnSceneFlagSet>(sceneId, flagType, flag);
}

/**
 * Before/AfterEndOfCycleSave dispatch (#514, the #438 table's worst entry).
 *
 * These two bracket Sram_SaveEndOfCycle (games/mm/src/code/z_sram_NES.c) — the
 * vanilla three-day wipe that Song of Time and "Dawn of the New Day" run. Both
 * call sites are live and unguarded; only the bodies were mm_stubs.c no-ops,
 * so the wipe ran with no snapshot taken and no restore performed and
 * Rando::MiscBehavior's cycle fix-ups (2s2h/Rando/MiscBehavior/OnCycleSave.cpp
 * — dungeon keys, boss keys, stray fairies, skulltula tokens, frog flags, the
 * three trade slots, and the per-check cycleObtained reset) never ran. The
 * checks stay flagged obtained, so nothing the wipe took was re-collectable:
 * silent, unrecoverable loss on a routine action, which is what makes this the
 * P0 of the class rather than another dormant hook.
 *
 * The pair is dispatched TOGETHER and must stay that way. The restore work all
 * hangs off After; Before's rando registrant is nothing but the memcpy into
 * OnCycleSave.cpp's saveContextCopy that After reads. Wiring one half is not
 * half a fix, it is a snapshot nobody consumes — the reasoning the retired
 * mm_stubs.c comment recorded, honoured here by landing both.
 *
 * WHAT ELSE GOES LIVE WITH THE BEFORE HALF, since it is not only a snapshot:
 * SavingEnhancements.cpp registers BeforeEndOfCycleSave unconditionally
 * (SavingEnhancements_AdvancePlaytime + DeleteOwlSave), so a cycle save now
 * banks playtime and clears the owl save. That is upstream 2S2H behaviour
 * restored, and DeleteOwlSave already ran live on the moon-crash leg (#442) —
 * it deletes a flash slot and clears isOwlSave rather than reading flash back
 * over gSaveContext, so it is not the #487 flash-readback class.
 *
 * WHAT DOES NOT COME BACK: 2s2h/Enhancements/Cycle/EndOfCycle.cpp's six
 * registrants (its own saveInfoCopy plus the DoNotReset{Rupees,Consumables,
 * BottleContent,RazorSword,TimeSpeed} restores) are still absent from the
 * binary — that TU is link-elided from the plain-archive 2ship_enh, verified
 * against build-cmake/redship.map, which lists no EndOfCycle.cpp.obj. Nothing
 * here changes that; those CVars stay inert until the TU links. The rando half
 * is unaffected because 2ship_rando links WHOLE_ARCHIVE.
 *
 * Unkeyed leg only, matching the GameInteractor.cpp twins: both hook types are
 * DEFINE_HOOK(..., ()) 0-arg, upstream's executors call plain ExecuteHooks<>,
 * and no compiled MM TU registers either type by id or ptr. The ForFilter leg
 * is absent for the usual reason — the S2H registry has no filter surface (see
 * the OnSceneInit and Should blocks).
 */
extern "C" void MM_GameHooks_ExecuteBeforeEndOfCycleSave(void) {
    S2H::GameHooks::Execute<GameInteractor::BeforeEndOfCycleSave>();
}

extern "C" void MM_GameHooks_ExecuteAfterEndOfCycleSave(void) {
    S2H::GameHooks::Execute<GameInteractor::AfterEndOfCycleSave>();
}

/**
 * OnGameCompletion dispatch (#438). The last revived-registrar gap: #520
 * un-elided RegisterSavingEnhancements, whose COND_HOOK(OnGameCompletion)
 * (SavingEnhancements.cpp) stamps shipSaveInfo.fileCompletedAt and freezes
 * post-credits playtime — but the call sites (z_boss_07.c Majora's Wrath death,
 * Rando/GiveItem.cpp final Triforce) reached the mm_stubs.c no-op, so the stamp
 * never happened. 0-arg (DEFINE_HOOK(OnGameCompletion, ())), single Execute<>
 * leg — no MM TU registers it by id/ptr/filter — matching the excluded
 * GameInteractor.cpp twin. The hook body touches only gSaveContext + playtime,
 * no MM_gPlayState, so it is headless-safe.
 */
extern "C" void MM_GameHooks_ExecuteOnGameCompletion(void) {
    S2H::GameHooks::Execute<GameInteractor::OnGameCompletion>();
}

/**
 * MM-owned "Should" dispatch (#392 VB follow-up). MM call sites reach these
 * through the single-exe macro rebind at the bottom of MM's GameInteractor.h
 * (GameInteractor_Should -> MM_GameHooks_ExecuteVBShould, etc.), so MM VB ids
 * and actor ids resolve against the MM-owned S2H::GameHooks registries only —
 * never OoT's ordinal-aliased tables. Bodies are line-faithful ports of the
 * excluded upstream executors (2s2h/GameInteractor/GameInteractor.cpp), minus
 * the ForPtr/ForFilter legs: no compiled MM TU registers a Should hook by Ptr
 * (the registry's ForPtr surface exists for OnActorUpdate, #427 item 2) and
 * the registry deliberately has no Filter surface — the only Should filter
 * user, DeveloperTools.cpp, stays link-elided; re-audit if it ever un-elides.
 * Locked by MMRandoGen's VB-dispatch phase (mm_rando_gen_test.cpp).
 */
extern "C" bool MM_GameHooks_ExecuteVBShould(GIVanillaBehavior flag, uint32_t result, ...) {
    va_list args;
    va_start(args, result);

    // Default argument promotion: the caller's verdict arrives as a uint32_t
    // (va_start on a bool is UB); downcast for the hook handlers, exactly as
    // upstream GameInteractor_Should does.
    bool boolResult = static_cast<bool>(result);

    S2H::GameHooks::Execute<GameInteractor::ShouldVanillaBehavior>(flag, &boolResult, args);
    S2H::GameHooks::ExecuteForID<GameInteractor::ShouldVanillaBehavior>(flag, flag, &boolResult, args);

    va_end(args);
    return boolResult;
}

extern "C" bool MM_GameHooks_ExecuteShouldActorInit(Actor* actor) {
    bool result = true;
    S2H::GameHooks::Execute<GameInteractor::ShouldActorInit>(actor, &result);
    S2H::GameHooks::ExecuteForID<GameInteractor::ShouldActorInit>(actor->id, actor, &result);
    return result;
}

extern "C" bool MM_GameHooks_ExecuteShouldActorUpdate(Actor* actor) {
    bool result = true;
    S2H::GameHooks::Execute<GameInteractor::ShouldActorUpdate>(actor, &result);
    S2H::GameHooks::ExecuteForID<GameInteractor::ShouldActorUpdate>(actor->id, actor, &result);
    return result;
}

extern "C" bool MM_GameHooks_ExecuteShouldActorDraw(Actor* actor) {
    bool result = true;
    S2H::GameHooks::Execute<GameInteractor::ShouldActorDraw>(actor, &result);
    S2H::GameHooks::ExecuteForID<GameInteractor::ShouldActorDraw>(actor->id, actor, &result);
    return result;
}

extern "C" bool MM_GameHooks_ExecuteShouldItemGive(u8 item) {
    bool result = true;
    S2H::GameHooks::Execute<GameInteractor::ShouldItemGive>(item, &result);
    S2H::GameHooks::ExecuteForID<GameInteractor::ShouldItemGive>(item, item, &result);
    return result;
}

// MM's foreign-item give (2s2h/Rando/ForeignItemsSingleExe.cpp). Declared here
// rather than in src/common/foreign_items.h — which is where its OoT twin is
// declared — because that header is Lane 1's and the MM pool surface lands in
// it with the reverse-direction pool, not with this give. Fold this in then.
extern "C" int MM_ForeignItem_Give(uint16_t riId);

/**
 * Award a single MM-origin shared item (ADR 0002 / Lane A1 consumer callback),
 * the MM twin of OoT_AwardSharedItem. `item->id` is an MM RandoItemId (RI_*).
 *
 * The give itself lives in MM_ForeignItem_Give
 * (2s2h/Rando/ForeignItemsSingleExe.cpp), which dispatches into Rando::GiveItem
 * when a PlayState is live and otherwise QUEUES for the next gameplay frame.
 * The queue is not incidental: this callback runs from
 * MM_Play_ConsumeStartupEntrance (z_play.c:2406), which is BEFORE
 * `MM_gPlayState = this` (z_play.c:2468), and MM's give path is not NULL-play
 * tolerant the way OoT's starting-item give is. See that file's header for the
 * full argument, including why the deferral window cannot lose an item.
 *
 * Combo_RedeemSharedItemsForGame marks the entry RSBS_SHARED_ITEM_REDEEMED
 * after this returns, so the crossing stays single-use; the entry is never
 * cleared (the durable record of the crossing). A give that reports failure is
 * logged but still consumes the redemption — an unresolvable id is a data bug,
 * not something to retry on every future arrival.
 */
static void MM_AwardSharedItem(const SharedItem* item, void* ctx) {
    (void)ctx;
    int given = MM_ForeignItem_Give(item->id);
    fprintf(stderr, "[MM] shared-item redeem: RI id=%u %s (Lane 6 foreign give)\n", (unsigned)item->id,
            given ? "awarded or queued for the next gameplay frame" : "NOT awarded — no such MM item");
}

/**
 * Consumer hook (ADR 0002 / Lane A1), the MM twin of OoT_ConsumeSharedItems.
 * Award every un-redeemed MM-origin shared item and mark it redeemed. Called
 * from MM_Play_ConsumeStartupEntrance (games/mm/src/code/z_play.c), which is
 * presence-gated and runs once per cross-game arrival into MM. A plain boot /
 * .redsave load never reaches it, so un-redeemed items wait for the next switch
 * into MM ("applies on next switch only"; see shared_items.h).
 */
void MM_ConsumeSharedItems(void) {
    Combo_RedeemSharedItemsForGame(GAME_MM, MM_AwardSharedItem, nullptr);
}

// ============================================================================
// Shared cross-game RESOURCES — MM side (#525)
//
// The OoT twin of this block is games/oot/soh/GameExports_SingleExe.cpp; the
// merge rules both call into live in src/common/shared_resources.c, which has
// no game headers. This half owns MM's field names and unit conversions.
//
// THE ASYMMETRY THAT MAKES THE WATERMARK NECESSARY LIVES HERE: MM's
// gUpgradeCapacities row for UPG_WALLET is {99, 200, 500, 500} while OoT's is
// {99, 200, 500, 999}. At tier 3 MM can hold 500 and OoT 999, so a shared pool
// above 500 simply does not fit in MM's wallet. The pool keeps the true total;
// apply materializes only what fits and records THAT as the watermark, so the
// overflow rides out the MM visit instead of being harvested away.
// ============================================================================

// MM's own upgrade setter and the three tables CUR_UPG_VALUE / CUR_CAPACITY
// expand to (games/mm/src/code/z_inventory.c, declared in variables.h).
// Declared here rather than by including variables.h, for the same
// narrow-header-surface reason as the OoT twin; the declarations match
// variables.h exactly.
extern "C" void MM_Inventory_ChangeUpgrade(s16 upgrade, u32 value);
extern "C" u32 MM_gUpgradeMasks[8];
extern "C" u8 MM_gUpgradeShifts[8];
extern "C" u16 MM_gUpgradeCapacities[][4];

// Heart pieces occupy the TOP NIBBLE of questItems in both games
// (MM's QUEST_HEART_PIECE_COUNT is 0x1C; OoT writes 1 << (QUEST_HEART_PIECE+4)).
#define MM_HEART_PIECE_SHIFT 28u
#define MM_HEART_PIECE_MASK 0xF0000000u

// Highest wallet tier this build defines (MM_gUpgradeCapacities UPG_WALLET row
// has 4 entries). Bounds the pool value so a tier authored by a future build
// cannot index off the end of MM's capacity table.
#define MM_MAX_WALLET_TIER 3u

static uint16_t MM_ReadHealthQuarters(void) {
    const uint16_t pieces =
        (uint16_t)((gSaveContext.save.saveInfo.inventory.questItems & MM_HEART_PIECE_MASK) >> MM_HEART_PIECE_SHIFT);
    const s16 rawCapacity = gSaveContext.save.saveInfo.playerData.healthCapacity;
    // MM converts 4 pieces into a container immediately inside Item_GiveImpl,
    // where OoT defers to textbox close — so MM never produces the pieces==4
    // state OoT can sit in. The canonical quantity (capacity + 4 per piece)
    // dissolves that divergence rather than guarding it; the arithmetic has one
    // definition, in src/common.
    return Combo_MakeHealthQuarters(rawCapacity < 0 ? 0u : (uint16_t)rawCapacity, pieces);
}

/**
 * HARVEST (#525), the twin of OoT_HarvestSharedResources.
 *
 * Called from MM_Game_Suspend and immediately before MM's `.redsave` writes.
 * Idempotent in both merge disciplines.
 */
extern "C" void MM_HarvestSharedResources(void) {
    // Settle rupeeAccumulator into the count first — MM drains it one per frame
    // exactly as OoT does, so a pending accumulator would be harvested as
    // nothing now and then credited again later.
    const int32_t walletCap = (int32_t)CUR_CAPACITY(UPG_WALLET);
    int32_t liveRupees = (int32_t)gSaveContext.save.saveInfo.playerData.rupees + (int32_t)gSaveContext.rupeeAccumulator;
    if (liveRupees < 0) {
        liveRupees = 0;
    }
    if (liveRupees > walletCap) {
        liveRupees = walletCap;
    }
    gSaveContext.save.saveInfo.playerData.rupees = (s16)liveRupees;
    gSaveContext.rupeeAccumulator = 0;

    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_RUPEES, (uint16_t)liveRupees);
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_WALLET_TIER, (uint16_t)CUR_UPG_VALUE(UPG_WALLET));
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_HEALTH_QUARTERS, MM_ReadHealthQuarters());
    Combo_HarvestSharedResource(
        GAME_MM, RSBS_SHARED_RES_HEALTH_CURRENT,
        gSaveContext.save.saveInfo.playerData.health < 0 ? 0u : (uint16_t)gSaveContext.save.saveInfo.playerData.health);
    Combo_HarvestSharedResource(GAME_MM, RSBS_SHARED_RES_DOUBLE_DEFENSE,
                                gSaveContext.save.saveInfo.playerData.doubleDefense ? 1u : 0u);
}

/**
 * APPLY (#525), the twin of OoT_ApplySharedResources.
 *
 * Called from MM_Play_ConsumeStartupEntrance beside MM_ConsumeSharedItems, once
 * per cross-game arrival into MM. Capacities are applied before the quantities
 * they bound.
 *
 * Touches gSaveContext only — no MM_gPlayState — so it is safe at this point in
 * the arrival, which runs BEFORE `MM_gPlayState = this` (z_play.c). That is the
 * same constraint that forces MM's shared-ITEM give to defer to the first
 * gameplay frame; a direct save-field write has no such dependency.
 */
extern "C" void MM_ApplySharedResources(void) {
    // --- Wallet tier (monotonic). Raises MM's clamp before rupees land.
    uint16_t walletTier = (uint16_t)CUR_UPG_VALUE(UPG_WALLET);
    if (Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_WALLET_TIER, MM_MAX_WALLET_TIER, &walletTier)) {
        MM_Inventory_ChangeUpgrade(UPG_WALLET, (u32)walletTier);
    }

    // --- Health capacity + pieces from the one canonical quantity, clamped at
    // 20 hearts: neither game's give path clamps capacity, and a total summed
    // across both games' pieces and containers passes 20 easily.
    uint16_t quarters = MM_ReadHealthQuarters();
    if (Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_HEALTH_QUARTERS,
                                  (uint16_t)RSBS_SHARED_RES_MAX_HEALTH_QUARTERS, &quarters)) {
        uint16_t capacity = 0;
        uint16_t pieces = 0;
        Combo_SplitHealthQuarters(quarters, &capacity, &pieces);
        gSaveContext.save.saveInfo.playerData.healthCapacity = (s16)capacity;
        gSaveContext.save.saveInfo.inventory.questItems =
            (gSaveContext.save.saveInfo.inventory.questItems & ~MM_HEART_PIECE_MASK) |
            ((uint32_t)pieces << MM_HEART_PIECE_SHIFT);
    }

    // --- Double defense (monotonic 0/1). MM spells the flag `doubleDefense`
    // where OoT spells it `isDoubleDefenseAcquired`, and each game keeps its own
    // inventory.defenseHearts counter that its life meter reads — so this
    // shares the FACT and lets each side set its own pair. A byte copy across
    // the two layouts would be wrong in both directions.
    uint16_t doubleDefense = gSaveContext.save.saveInfo.playerData.doubleDefense ? 1u : 0u;
    if (Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_DOUBLE_DEFENSE, 1u, &doubleDefense) && doubleDefense != 0) {
        gSaveContext.save.saveInfo.playerData.doubleDefense = 1;
        if (gSaveContext.save.saveInfo.inventory.defenseHearts < 20) {
            gSaveContext.save.saveInfo.inventory.defenseHearts = 20;
        }
    }

    // --- Rupees (consumable), clamped to the wallet capacity just applied.
    uint16_t rupees =
        gSaveContext.save.saveInfo.playerData.rupees < 0 ? 0u : (uint16_t)gSaveContext.save.saveInfo.playerData.rupees;
    if (Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_RUPEES, (uint16_t)CUR_CAPACITY(UPG_WALLET), &rupees)) {
        gSaveContext.save.saveInfo.playerData.rupees = (s16)rupees;
    }
    gSaveContext.rupeeAccumulator = 0;

    // --- Current health (consumable): one bar across both games.
    uint16_t health =
        gSaveContext.save.saveInfo.playerData.health < 0 ? 0u : (uint16_t)gSaveContext.save.saveInfo.playerData.health;
    if (Combo_ApplySharedResource(GAME_MM, RSBS_SHARED_RES_HEALTH_CURRENT,
                                  (uint16_t)gSaveContext.save.saveInfo.playerData.healthCapacity, &health)) {
        // Floored at one heart for the same reason as OoT's side: a departing
        // game cannot normally hand over a dead bar, and spawning dead on an
        // arrival lands in a death handler no arrival path has been tested
        // through.
        gSaveContext.save.saveInfo.playerData.health = (s16)(health < 0x10u ? 0x10u : health);
    }
}

// ============================================================================
// Cycle-safe shared rupees (#525)
// ============================================================================
//
// MM's three-day cycle zeroes the wallet: Sram_SaveEndOfCycle
// (games/mm/src/code/z_sram_NES.c) sets playerData.rupees = 0 and
// rupeeAccumulator = 0, and both Song of Time and "Dawn of the New Day" run it.
// With ONE shared pool spanning both games, leaving that unhandled means the
// next delta harvest computes `0 - watermark` and the wipe drains the OoT
// rupees too.
//
// OPERATOR CALL: the shared pool is CYCLE-SAFE. A Song of Time no longer costs
// the player money that crossed over from OoT. This is a deliberate departure
// from vanilla MM, taken because the alternative — strict one-game semantics —
// lets an MM mechanic silently delete an OoT-side grind the player was not
// thinking about when they played the song. Rupees SPENT in MM are still gone;
// only the wipe is suppressed.
//
// This is the same seam 2S2H's own DoNotResetRupees enhancement uses, and the
// restore is deliberately unconditional rather than CVar-gated: the shared pool
// is not an optional enhancement, it is the resource model this build ships.
//
// The pair brackets the wipe — Before snapshots, After restores — and both are
// genuinely dispatched in single-exe as of #514
// (MM_GameHooks_ExecuteBefore/AfterEndOfCycleSave below, called from
// z_sram_NES.c, locked by games/mm/2s2h/mm_hook_dispatch_test.cpp). That was
// checked before this was written, not assumed: registering against a hook
// nothing dispatches is a documented recurring failure here (#512/#517).
//
// No watermark bookkeeping is needed. Nothing harvests during a cycle save, and
// the restore puts the count back where the watermark already expects it, so
// the round trip is invisible to the delta arithmetic.
//
// CURRENT HEALTH deliberately has no equivalent hook. The cycle reset floors
// health at 0x30, and under one-health-bar semantics that is a Song of Time
// healing the single shared bar — which is what "as if OoT and MM were one
// game" means. It reads as a heal, not a bug.

static s16 sPreCycleRupees = 0;

extern "C" void MM_RegisterSharedResourceCycleHooks(void) {
    S2H::GameHooks::Register<GameInteractor::BeforeEndOfCycleSave>([]() {
        // Snapshot the SETTLED balance: a pending accumulator is money the
        // player has earned but not yet seen counted, and the wipe below drops
        // it too. Clamped to the wallet so the restore cannot exceed what MM
        // can hold.
        const int32_t walletCap = (int32_t)CUR_CAPACITY(UPG_WALLET);
        int32_t settled =
            (int32_t)gSaveContext.save.saveInfo.playerData.rupees + (int32_t)gSaveContext.rupeeAccumulator;
        if (settled < 0) {
            settled = 0;
        }
        if (settled > walletCap) {
            settled = walletCap;
        }
        sPreCycleRupees = (s16)settled;
    });

    S2H::GameHooks::Register<GameInteractor::AfterEndOfCycleSave>([]() {
        gSaveContext.save.saveInfo.playerData.rupees = sPreCycleRupees;
        gSaveContext.rupeeAccumulator = 0;
        // The "you lost your rupees" notice would be a lie now, and it is the
        // same flag 2S2H's DoNotResetRupees clears for the same reason.
        CLEAR_EVENTINF(EVENTINF_THREEDAYRESET_LOST_RUPEES);
    });
}

/**
 * Paired-world activation on the SWITCH-ENTRY path (#439).
 *
 * The bug this closes is a dispatch-site gap, not a registration gap.
 * Rando::MiscBehavior::OnFileCreate is registered against OnSaveInit at boot
 * (Rando::MiscBehavior::Init), but OnSaveInit is only ever DISPATCHED from
 * MM_Sram_InitSave (z_sram_NES.c) — MM's file-select "create a new file"
 * flow. The natural player flow into the paired world never touches file
 * select: entering the Happy Mask Shop hands off to GameRunner_SwitchTo,
 * whose cold gamestate-chain boot runs ConsoleLogo -> TitleSetup ->
 * MM_Play_Init. TitleSetup authors the save with MM_Sram_InitNewSave() (a
 * plain VANILLA new file) and dispatches OnSaveLoad, never OnSaveInit. So
 * OnFileCreate never ran, no MM fill happened, gComboCtx.foreignPlacements
 * stayed empty and no MM spoiler was ever written — exactly the operator
 * forensics on #439, where Lane B's producer had correctly stamped
 * sourceIsRando/sharedRandoSeed in every slot.
 *
 * Called from MM_Play_ConsumeStartupEntrance (games/mm/src/code/z_play.c) —
 * the one point that is after every boot-chain wipe and before the save is
 * interpreted, and the same point the frozen-state restore uses.
 *
 * @param hadFrozenState nonzero when Combo_ConsumeFrozenState just restored a
 *        real MM session over the boot-chain save. That save belongs to the
 *        player, so it is NEVER regenerated — the whole point of the
 *        "existing MM saves are never silently modified" contract.
 *
 * Every decision point logs to stderr with a greppable `[MM] pairing:`
 * prefix: the operator flew blind through this seam for an entire playtest.
 */
void MM_Rando_PairOnCrossGameArrival(int hadFrozenState) {
    const bool pairing = Combo_ForeignPairingActive();
    const bool alreadyRando = (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO);

    if (!pairing) {
        fprintf(stderr,
                "[MM] pairing: skipped-because-no-paired-oot-world "
                "(sourceIsRando=%d settingsHash=%08X masterSeed=%u)\n",
                gComboCtx.sourceIsRando ? 1 : 0, gComboCtx.sharedRandoSettingsHash, gComboCtx.sharedRandoSeed);
        return;
    }

    if (hadFrozenState) {
        // Self-heal a save that is INTERNALLY INCONSISTENT before accepting it.
        //
        // A legitimately vanilla MM file always has rando.finalSeed == 0:
        // Sram_ResetSave memsets shipSaveInfo wholesale and MM_Sram_InitNewSave
        // then stamps SAVETYPE_VANILLA, so nothing in MM's own code can author
        // "a complete rando world sitting under a vanilla type byte". Seeing it
        // is positive evidence that a RANDO file lost its type byte to a bulk
        // Save overwrite (the moon-crash reload in z_sram_NES.c
        // Sram_ResetSaveFromMoonCrash is the operator-confirmed instance, fixed
        // at its source; this is the belt-and-braces catch for that whole class).
        //
        // Without this, the loss is PERMANENT and silent: once a vanilla-stamped
        // save is frozen on the next switch-out, every later return leg restores
        // it, re-reports "existing save", and MM plays vanilla forever with the
        // player's entire placement table still sitting intact underneath.
        if (!alreadyRando && gSaveContext.save.shipSaveInfo.rando.finalSeed != 0) {
            gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
            fprintf(stderr,
                    "[MM] pairing: REPAIRED — restored save carried a complete rando world "
                    "(finalSeed=%08X) under saveType=vanilla; re-stamping SAVETYPE_RANDO\n",
                    gSaveContext.save.shipSaveInfo.rando.finalSeed);
            fflush(stderr);
            // Fall through to the skip below: the world is the player's own and
            // must NOT be regenerated. z_play.c's GameInteractor_ExecuteOnSaveLoad
            // at the end of MM_Play_ConsumeStartupEntrance then arms the IS_RANDO
            // COND_HOOKs against the repaired save.
            fprintf(stderr, "[MM] pairing: skipped-because-existing-mm-save "
                            "(frozen MM session restored, saveType=rando) — an existing file is never regenerated\n");
            return;
        }

        // A restored MM session — vanilla or rando — is the player's own save.
        // Re-running generation over it would wipe their progress (OnFileCreate
        // memsets shipSaveInfo.rando and re-authors the starting state), so the
        // paired world simply does not apply to a file that already exists.
        fprintf(stderr,
                "[MM] pairing: skipped-because-existing-mm-save "
                "(frozen MM session restored, saveType=%s) — an existing file is never regenerated\n",
                alreadyRando ? "rando" : "vanilla");
        return;
    }

    if (alreadyRando) {
        // Defensive: the bootstrap save the boot chain authored should always
        // be vanilla. If some other path already paired it, do not do it twice.
        fprintf(stderr, "[MM] pairing: skipped-because-already-paired (mmFinalSeed=%08X foreignPlacements=%d)\n",
                gSaveContext.save.shipSaveInfo.rando.finalSeed, Combo_CountForeignPlacements());
        return;
    }

    fprintf(stderr, "[MM] pairing: armed on switch-entry (masterSeed=%u settingsHash=%08X) — dispatching OnSaveInit\n",
            gComboCtx.sharedRandoSeed, gComboCtx.sharedRandoSettingsHash);
    fflush(stderr);

    // The REAL dispatch — the same bridge MM_Sram_InitSave calls on the
    // file-select path, so both entry paths converge on one generation code
    // path (S2H::GameHooks Execute<OnSaveInit> -> Rando::MiscBehavior::OnFileCreate).
    GameInteractor_ExecuteOnSaveInit(gSaveContext.fileNum);

    if (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) {
        fprintf(stderr, "[MM] pairing: paired world ACTIVE (mmFinalSeed=%08X foreignPlacements=%d)\n",
                gSaveContext.save.shipSaveInfo.rando.finalSeed, Combo_CountForeignPlacements());
    } else {
        // OnFileCreate's catch reverts to a vanilla save on any generation
        // failure. That is the correct fallback, but it must never be silent.
        fprintf(stderr, "[MM] pairing: FAILED — generation reverted the save to vanilla; MM plays vanilla this "
                        "session (see the preceding SPDLOG_ERROR for the cause)\n");
    }
    fflush(stderr);
}

} // extern "C"

// ============================================================================
// In-app mm.o2r generation for the start prompt (issue #317)
// ============================================================================

/**
 * Offer to generate mm.o2r from the user's own Majora's Mask ROM.
 *
 * Called by the harness (rsbs/src/main.cpp) when Majora's Mask is selected at
 * the start prompt but no MM game archive exists. Runs before any game init or
 * window creation, so the UI is parentless SDL message boxes plus a native
 * file dialog — the same pre-window flow standalone 2Ship drives from InitOTR
 * (2s2h/BenPort.cpp). ROMs are searched for next to the executable (next to
 * the .AppImage file on AppImage runs); the archive is exported to MM's app
 * directory, the first location the archive checks probe
 * (src/common/archive_check.cpp). The harness re-checks archive presence
 * after this returns — success is "mm.o2r now exists", not a return value.
 */
extern "C" void MM_Extract_OfferAndRun(void) {
#if defined(__SWITCH__) || defined(__WIIU__)
    // No extractor on console platforms (games/mm/CMakeLists.txt globs the
    // Extractor sources out there) — consoles bring their own archives.
    return;
#else
    const std::string installPath = Ship::Context::GetAppBundlePath();
    const std::string exportDir = Ship::Context::GetAppDirectoryPath(kMmAppName);

    if (!std::filesystem::exists(installPath + "/assets")) {
        MMExtractor::ShowErrorBox(
            "Extractor assets not found",
            "No Majora's Mask O2R file found, and the assets folder needed to generate one is missing.\n\n"
            "In-app generation needs a packaged RedShipBlueShip install (an assets/ folder next to the executable).");
        return;
    }

    if (MMExtractor::ShowYesNoBox("No Majora's Mask O2R file",
                                  "No Majora's Mask game archive (mm.o2r) was found.\n\n"
                                  "Generate one from your Majora's Mask ROM now?") != IDYES) {
        return;
    }

    // ROMs are searched for next to the executable — except on AppImage runs,
    // where the bundle path is a transient read-only mount: there the user's
    // ROM sits next to the .AppImage file itself.
    std::string romSearchDir = installPath;
    const char* appImagePath = getenv("APPIMAGE");
    if (appImagePath != nullptr && appImagePath[0] != '\0') {
        std::filesystem::path appImageDir = std::filesystem::path(appImagePath).parent_path();
        std::error_code aec;
        if (!appImageDir.empty() && std::filesystem::is_directory(appImageDir, aec)) {
            romSearchDir = appImageDir.string();
        }
    }

    // GetAppDirectoryPath can name a directory that does not exist yet (e.g.
    // ~/.local/share/2s2h on a first boot); CallZapd's final copy needs it.
    std::error_code ec;
    std::filesystem::create_directories(exportDir, ec);

    // The extractor drives throwing std::filesystem overloads (file probes,
    // reads); nothing above this frame catches, so keep exceptions from
    // escaping the pre-window flow.
    try {
        MMExtractor extract;
        if (!extract.Run(romSearchDir)) {
            MMExtractor::ShowErrorBox("Error", "An error occurred, no O2R file was generated.");
            return;
        }
        if (!extract.CallZapd(installPath, exportDir)) {
            // CallZapd reports its own errors via message boxes.
            return;
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "[MM] Extraction failed: %s\n", e.what());
        fflush(stderr);
        MMExtractor::ShowErrorBox("Extraction Failed", e.what());
        return;
    }

    fprintf(stderr, "[MM] mm.o2r generated into %s\n", exportDir.c_str());
    fflush(stderr);
#endif
}

// ============================================================================
// Extended-culling helpers (MM_-prefixed) — issue #382
// ============================================================================
//
// MM's copies of these live in games/mm/2s2h/ShipUtils.cpp, which is EXCLUDED
// from the single-exe build (games/mm/CMakeLists.txt) because the rest of that
// file collides wholesale with OoT's soh/ShipUtils.cpp. That left OoT's
// definitions as the only ones in the link, and MM's actor overlays — which
// call these unprefixed — executed OoT's bodies against MM's Actor.
//
// The two layouts are not the same: projectedPos sits at 0x0E4 in OoT's Actor
// and 0x0EC in MM's. OoT's AdjustProjectedZ therefore wrote MM's
// projectedPos.x (so MM's extended draw distance scaled the wrong axis and
// never worked), and OoT's AdjustProjectedX wrote MM's shape.feetPos[1].y (so
// MM's widescreen-culling option corrupted a foot position the shadow/limb
// code reads). No link error, because only one definition existed — the exact
// blind spot /FORCE:MULTIPLE and single-definition binding share.
//
// These are defined here rather than by un-excluding 2s2h/ShipUtils.cpp: that
// file also defines Ship_GetSceneName / Ship_IsCStringEmpty /
// Ship_CreateQuadVertexGroup / Ship_GetCharFontWidthNES and pulls in Rando +
// ImGui, every one of which is a genuine duplicate of OoT's. Un-excluding it
// would trade this silent fault for a Linux multiple-definition link failure.
// The MM_ prefix + the include/mm_ship_utils_prefix.h rename is the surgical
// fix, and matches the repo's established remedy.
//
// Locked ROM-free by mm-culling-binding (games/mm/2s2h/mm_culling_test.cpp).

extern "C" float OTRGetAspectRatio();

extern "C" void MM_Ship_ExtendedCullingActorAdjustProjectedZ(Actor* actor) {
    s32 multiplier = CVarGetInteger("gEnhancements.Graphics.IncreaseActorDrawDistance", 1);
    if (multiplier > 1) {
        actor->projectedPos.z /= multiplier;
    }
}

extern "C" void MM_Ship_ExtendedCullingActorAdjustProjectedX(Actor* actor) {
    if (CVarGetInteger("gEnhancements.Graphics.ActorCullingAccountsForWidescreen", 0)) {
        // Same clamp-to-1 as Ship_GetExtendedAspectRatioMultiplier, computed
        // inline. That helper is itself a duplicated symbol across the two
        // ports; it happens to be Actor-free and therefore harmless to share,
        // but depending on it here would reintroduce a cross-game binding for
        // no benefit.
        constexpr float kFourByThree = 4.0f / 3.0f;
        float ratioAdjusted = OTRGetAspectRatio() / kFourByThree;
        if (ratioAdjusted < 1.0f) {
            ratioAdjusted = 1.0f;
        }
        actor->projectedPos.x /= ratioAdjusted;
    }
}

// Restores projectedPos after the Adjust* hacks. Previously the ONLY
// definition in the link was a one-parameter no-op stub in
// src/common/mm_stubs.c, so this restore silently did nothing for both games
// while every call site passed two arguments.
extern "C" void MM_Ship_ExtendedCullingActorRestoreProjectedPos(PlayState* play, Actor* actor) {
    f32 invW = 0.0f;
    Actor_GetProjectedPos(play, &actor->world.pos, &actor->projectedPos, &invW);
}

// ============================================================================
// GameOps registration
// ============================================================================

static GameOps sMMOps = { "mm",           "Majora's Mask", MM_Game_Init, MM_Game_Run, MM_Game_Suspend,
                          MM_Game_Resume, MM_Game_Shutdown };

extern "C" GameOps* MM_GetGameOps(void) {
    return &sMMOps;
}

// ============================================================================
// Pause-menu / file-select hook dispatch (#438)
// ============================================================================
//
// The last hook types that combined a LIVE registrant (2ship_rando links with
// WHOLE_ARCHIVE, so Rando::MiscBehavior's registrations are in the binary)
// with dead dispatch. Same class and remedy as the #512/#514/#515 blocks
// above: the rebind block at the bottom of MM's GameInteractor.h routes each
// upstream-spelled call site here, and these bridges consult ONLY the MM-owned
// S2H::GameHooks registries. Locked by mm_hook_dispatch_test.cpp checks 9-11.
//
//  - OnKaleidoUpdate (z_kaleido_scope_NES.c KaleidoScope_Update): bound OoT's
//    0-arg gated wrapper — C linkage hid the arity mismatch. KaleidoItemPage's
//    registrant is the trade-slot item-cycling input handler; while it was
//    dead, left/right on SLOT_TRADE_DEED/KEY_MAMA/COUPLE did nothing, so
//    alternate shuffled trade items in those slots were unreachable from the
//    pause menu.
//  - Before/AfterKaleidoDrawPage (z_kaleido_scope_NES.c brackets every page
//    draw with the pair): bound the mm_stubs.c no-ops. After carries
//    KaleidoItemPage's COND_ID_HOOK(PAUSE_ITEM) cycling arrows and adjacent-
//    item previews for those same slots — dead, and combined with the LIVE
//    VB_KALEIDO_DISPLAY_ITEM_TEXT suppression that made the slots strictly
//    worse than vanilla (static icon, no name, no arrows). Before's only
//    registrant (PersistentMasks.cpp, 2ship_enh) stays link-elided today; it
//    is wired here as a pair with After per the pairing rule mm_game_hooks.h
//    records, and its plain-Unregister-vs-RegisterForID leg mismatch was
//    fixed in the same change so un-elision cannot accumulate stale entries.
//    Both ForID legs mirror the excluded GameInteractor.cpp twin (keyed on
//    pauseIndex); upstream has no ForPtr/ForFilter legs for these.
//  - OnFileSelectSaveLoad (five z_sram_NES.c file-select flows): bound a
//    mm_stubs.c no-op whose (void*, int) signature had drifted from the real
//    (s16, bool, SaveContext*) — the #372/#424 hazard class, retired with the
//    stub. FileSelect.cpp's registrant is the sole isRando[] writer, so a
//    randomizer file on MM's file-select list rendered exactly like a vanilla
//    one.
//
// HEADLESS SAFETY (the #516 SIGSEGV class): KaleidoItemPage's OnKaleidoUpdate
// and AfterKaleidoDrawPage registrants dereference MM_gPlayState with no null
// check. Every call site newly reached here runs inside an active pause/file-
// select flow — KaleidoScope_Update and the page draws only execute under a
// live PlayState, and the z_sram_NES.c flows only under the file-select game
// state — and no ROM-free CTest row drives any of them. The MMHookDispatch row
// resets these registries before dispatching its own probes, so it cannot run
// the production registrants against a null play state.

extern "C" void MM_GameHooks_ExecuteOnKaleidoUpdate(PauseContext* pauseCtx) {
    S2H::GameHooks::Execute<GameInteractor::OnKaleidoUpdate>(pauseCtx);
}

extern "C" void MM_GameHooks_ExecuteBeforeKaleidoDrawPage(PauseContext* pauseCtx, u16 pauseIndex) {
    S2H::GameHooks::Execute<GameInteractor::BeforeKaleidoDrawPage>(pauseCtx, pauseIndex);
    S2H::GameHooks::ExecuteForID<GameInteractor::BeforeKaleidoDrawPage>(pauseIndex, pauseCtx, pauseIndex);
}

extern "C" void MM_GameHooks_ExecuteAfterKaleidoDrawPage(PauseContext* pauseCtx, u16 pauseIndex) {
    S2H::GameHooks::Execute<GameInteractor::AfterKaleidoDrawPage>(pauseCtx, pauseIndex);
    S2H::GameHooks::ExecuteForID<GameInteractor::AfterKaleidoDrawPage>(pauseIndex, pauseCtx, pauseIndex);
}

extern "C" void MM_GameHooks_ExecuteOnFileSelectSaveLoad(s16 fileNum, bool isOwlSave, SaveContext* saveContext) {
    S2H::GameHooks::Execute<GameInteractor::OnFileSelectSaveLoad>(fileNum, isOwlSave, saveContext);
}

#endif /* RSBS_SINGLE_EXECUTABLE */
