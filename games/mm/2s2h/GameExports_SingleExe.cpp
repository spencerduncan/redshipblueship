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
#include "shared_items.h"
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
 * if gEnhancements.Dpad.DpadEquips were on, and the ocarina paths in
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
            if (CVarGetInteger("gEnhancements.Dpad.DpadEquips", 0)) {
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

    // Lane C1 (#392): register the GIEvent pump (the port of upstream's
    // ProcessEvents player-update hook, games/mm/2s2h/
    // GameInteractorEventsSingleExe.cpp). Upstream registered it from
    // GameInteractor::RegisterOwnHooks at boot; in the single exe only the
    // rando queue produces GIEvents, so the rando bring-up is its natural
    // (once-only, same guard) home.
    MM_GameEvents_RegisterPump();
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

extern "C" void MM_GameHooks_ExecuteOnActorUpdate(Actor* actor) {
    S2H::GameHooks::Execute<GameInteractor::OnActorUpdate>(actor);
    S2H::GameHooks::ExecuteForID<GameInteractor::OnActorUpdate>(actor->id, actor);
}

extern "C" void MM_GameHooks_ExecuteOnFlagSet(FlagType flagType, u32 flag) {
    S2H::GameHooks::Execute<GameInteractor::OnFlagSet>(flagType, flag);
}

extern "C" void MM_GameHooks_ExecuteOnSceneFlagSet(s16 sceneId, FlagType flagType, u32 flag) {
    S2H::GameHooks::Execute<GameInteractor::OnSceneFlagSet>(sceneId, flagType, flag);
}

/**
 * Award a single MM-origin shared item (ADR 0002 / Lane A1 consumer callback),
 * the MM twin of OoT_AwardSharedItem. `item->id` is an MM RandoItemId (RI_*).
 *
 * Lane A1 scope: plumbing seam only — it logs; Combo_RedeemSharedItemsForGame
 * still marks the entry RSBS_SHARED_ITEM_REDEEMED so the crossing is single-use
 * once wired. Lane C replaces the log with MM's real give (Rando::GiveItem /
 * MM_Item_Give) for the chosen foreign-item class; do NOT clear the entry.
 */
static void MM_AwardSharedItem(const SharedItem* item, void* ctx) {
    (void)ctx;
    fprintf(stderr, "[MM] shared-item redeem (Lane A1 plumbing): RI id=%u — Lane C wires the MM give\n",
            (unsigned)item->id);
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

#endif /* RSBS_SINGLE_EXECUTABLE */
