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
#include "entrance.h"
#include <ship/resource/ResourceManager.h>
#include <ship/resource/ResourceLoader.h>
#include <ship/resource/archive/ArchiveManager.h>
#include "GameInteractor/GameInteractor.h"
#include <ship/resource/File.h>

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

    // Audio reset for cross-game switch (issue #157) and suspend (issue #270)
    extern s32 gAudioCtxInitalized;
    void AudioThread_InitMesgQueues(void);
    void MM_Audio_PreNMI(void);

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
    // Both games' frame loops fire the same shared OnGameStateMainStart hook
    // storage, so MM-registered test hooks also run during OoT frames — and
    // MM's suspended PlayState stays non-NULL across a switch away. Only
    // count MM's own frames (#344).
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
    // MM-registered hooks also fire during OoT frames (shared hook storage);
    // only MM's own frames count against the watchdog budget.
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
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>(
            []() {
                if (!MM_SceneLoadComplete()) {
                    sSceneLoadStableFrames = 0;
                    MM_SceneLoadWatchdogExpired("int-boot-mm");
                    return;
                }
                sSceneLoadStableFrames++;
                if (sSceneLoadStableFrames == 10) {
                    fprintf(stderr,
                            "[MM-INT-TEST] scene load complete and stable (sceneId=0x%X entrance=0x%04X); PASS\n",
                            MM_gPlayState->sceneId, gSaveContext.save.entrance);
                    fflush(stderr);
                    IntegrationTest_SignalBootComplete(GAME_MM, "MM scene load complete");
                }
            }
        );

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

        // (#344) PASS requires MM to complete the Clock Tower Interior scene
        // load the entrance link asked for (0xC010), not just tick frames.
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>(
            []() {
                if (!MM_SceneLoadComplete() || MM_gPlayState->sceneId != SCENE_INSIDETOWER) {
                    static bool sWrongSceneLogged = false;
                    sSceneLoadStableFrames = 0;
                    if (!sWrongSceneLogged && MM_SceneLoadComplete()) {
                        sWrongSceneLogged = true;
                        fprintf(stderr,
                                "[MM-INT-TEST] scene loaded but sceneId=0x%X != SCENE_INSIDETOWER; waiting\n",
                                MM_gPlayState->sceneId);
                        fflush(stderr);
                    }
                    MM_SceneLoadWatchdogExpired("int-switch-oot-hms-to-mm");
                    return;
                }
                sSceneLoadStableFrames++;
                if (sSceneLoadStableFrames == 10) {
                    fprintf(stderr,
                            "[MM-INT-TEST] Clock Tower Interior scene load complete after HMS->MM switch "
                            "(entrance=0x%04X); PASS\n",
                            gSaveContext.save.entrance);
                    fflush(stderr);
                    IntegrationTest_SignalBootComplete(GAME_MM, "MM scene load complete after HMS->MM switch");
                }
            }
        );

        fprintf(stderr, "[MM] HMS->MM switch hooks registered\n");
        fflush(stderr);
    } else if (mode == INT_TEST_SWITCH_MM_CLOCKTOWN_SOUTH_TO_OOT) {
        // T2 (#261): Boot MM, programmatically trigger the South Clock Town
        // south exit, assert the cross-game switch resolves to OoT Market.
        // Mirror of T1: MM is the trigger side here, OoT is the receiver.
        // MM has no OnPresentFileSelect analog, so we wait for a completed
        // scene load (#344) plus a few stable frames in OnGameStateMainStart
        // and then fire the trigger once. Final pass is signaled from the
        // OoT-side hook after OoT stabilizes post-switch.
        fprintf(stderr, "[MM] Registering integration test hooks for SCT-south->OoT switch (T2)\n");
        fflush(stderr);

        sSceneLoadWaitFrames = 0;
        sSceneLoadStableFrames = 0;

        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>(
            []() {
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

                fprintf(stderr,
                        "[MM-INT-TEST] MM stable; triggering SCT-south entrance 0x%04X\n",
                        MM_ENTR_SOUTH_CLOCK_TOWN_0);
                fflush(stderr);

                // Same call MM's z_play.c makes when the player walks south
                // out of South Clock Town — minus the freeze, which T3 covers.
                Combo_CheckCrossGameEntrance("mm", MM_ENTR_SOUTH_CLOCK_TOWN_0);

                if (!Combo_IsCrossGameSwitch()) {
                    fprintf(stderr, "[MM-INT-TEST] FAIL: SCT-south entrance did not register a cross-game switch\n");
                    fflush(stderr);
                    IntegrationTest_RequestExit();
                    return;
                }

                const char* target = Combo_GetSwitchTargetGameId();
                uint16_t targetEntrance = Combo_GetSwitchTargetEntrance();

                if (!target || strcmp(target, "oot") != 0) {
                    fprintf(stderr, "[MM-INT-TEST] FAIL: target should be 'oot', got '%s'\n",
                            target ? target : "(null)");
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
                        "[MM-INT-TEST] PASS leg 1: SCT-south routes to OoT 0x%04X; main loop will run the switch\n",
                        targetEntrance);
                fflush(stderr);
                // Intentionally NOT signaling boot complete here. The main loop
                // will see the pending cross-game switch on Combo_CheckHotSwap
                // and hand off to OoT. The OoT-side hook signals the final pass.
            }
        );

        fprintf(stderr, "[MM] SCT-south->OoT switch hooks registered\n");
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

        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>(
            []() {
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
                fprintf(stderr, "[MM-INT-TEST] MM stable; archive-hotswap arrival #%d of %d\n",
                        n, ArchiveHotswap_TargetArrivals());
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
                    // Keep the cycle going: re-trigger the MM->OoT switch via the
                    // South Clock Town south exit — same call the T2 branch makes.
                    fprintf(stderr, "[MM-INT-TEST] re-triggering SCT-south entrance 0x%04X to continue cycle\n",
                            MM_ENTR_SOUTH_CLOCK_TOWN_0);
                    fflush(stderr);
                    Combo_CheckCrossGameEntrance("mm", MM_ENTR_SOUTH_CLOCK_TOWN_0);
                }
            }
        );

        fprintf(stderr, "[MM] archive-hotswap cycle hooks registered\n");
        fflush(stderr);
    }
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
    for (const char* ext : {"mm.o2r", "mm.zip", "mm.otr"}) {
        std::string path = Ship::Context::LocateFileAcrossAppDirs(ext, kMmAppName);
        if (!path.empty() && std::filesystem::exists(path)) {
            if (archiveMgr->AddArchive(path)) {
                fprintf(stderr, "[MM] Loaded archive: %s\n", path.c_str());
                RecordMMArchivePath(path);
                loaded++;
            }
            break;  // Only load one mm archive
        }
    }

    // Load 2ship.o2r (MM's equivalent of soh.o2r)
    std::string shipPath = Ship::Context::GetPathRelativeToAppBundle("2ship.o2r");
    if (!shipPath.empty() && std::filesystem::exists(shipPath)) {
        if (archiveMgr->AddArchive(shipPath)) {
            fprintf(stderr, "[MM] Loaded archive: %s\n", shipPath.c_str());
            RecordMMArchivePath(shipPath);
            loaded++;
        }
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

    // Path — overwrites OoT's PathV0 because MM paths have additional fields
    loader->RegisterResourceFactory(std::make_shared<S2H::ResourceFactoryBinaryPathMMV0>(), RESOURCE_FORMAT_BINARY,
                                    "Path", static_cast<uint32_t>(S2H::ResourceType::SOH_Path), 0,
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

    // Register integration test hooks if in integration test mode
    MM_RegisterIntegrationTestHooks();

    fprintf(stderr, "[MM] Game_Init complete\n");
    fflush(stderr);
    return 0;
}

void MM_Game_Run(void) {
    fprintf(stderr, "[MM] Game_Run called, entering MM_Graph_ThreadEntry()\n");
    fflush(stderr);
    // Assert context still valid before graph loop (issue #158)
    assert(Ship::Context::GetInstance() != nullptr &&
           "Ship::Context must be valid when MM graph thread starts");
    assert(Ship::Context::GetInstance()->GetWindow() != nullptr &&
           "Window must be valid when MM graph thread starts");

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

    // Stop MM audio playback to prevent interference with OoT (issue #270).
    // MM_Audio_PreNMI calls AudioThread_PreNMIInternal which halts sequence
    // players and SFX. Same reasoning as OoT side: the SoH/2S2H port doesn't
    // run a real OS audio thread, so audio is processed synchronously from
    // the game loop, which has already returned from MM_Graph_ThreadEntry
    // before suspend is called — no race.
    fprintf(stderr, "[MM] Stopping audio via PreNMI path...\n");
    fflush(stderr);
    MM_Audio_PreNMI();

    // Mark audio as uninitialized so message-queue re-init works on resume.
    gAudioCtxInitalized = false;

    fprintf(stderr, "[MM] Game_Suspend complete\n");
    fflush(stderr);
}

/**
 * Resume MM after being suspended for a game switch (issue #170, #270).
 * - Restores frozen MM SaveContext so gameplay state survives the OoT
 *   round-trip (OoT scribbles over the unified gSaveContext storage while
 *   it is active — see src/common/unified_save.c).
 * - Reinitializes audio message queues for clean state.
 */
void MM_Game_Resume(void) {
    fprintf(stderr, "[MM] Game_Resume called\n");
    fflush(stderr);

    // Restore the frozen MM SaveContext captured before we left for OoT (#170).
    // OoT may have scribbled over the unified gSaveContext storage while it
    // was active (see src/common/unified_save.c), so we must re-hydrate MM's
    // view on every resume. First boot of MM has no frozen state — in that
    // case MM_InitFirstEntrySaveContext still handles bootstrap via #168.
    if (Context_HasFrozenState(GAME_MM)) {
        fprintf(stderr, "[MM] Restoring frozen SaveContext on resume\n");
        fflush(stderr);
        Context_RestoreState(GAME_MM, &gSaveContext, sizeof(gSaveContext));

        // Prefer the startup entrance set for this switch; fall back to the
        // return entrance recorded when we last froze MM. Use the explicit
        // Has accessor because entrance 0x0000 is a valid id and treating it
        // as "unset" would silently drop a legitimate restore.
        bool hasStartup = Combo_HasStartupEntrance();
        uint16_t targetEntrance = hasStartup
            ? Combo_GetStartupEntrance()
            : Context_GetFrozenReturnEntrance(GAME_MM);
        gSaveContext.save.entrance = targetEntrance;
        fprintf(stderr, "[MM] Resume entrance: 0x%04X (startup=%u)\n",
                targetEntrance, hasStartup);
    }

    // Reinitialize audio message queues for clean state (issue #270).
    // The audio context's queue pointers may be stale after suspend's
    // PreNMI shutdown — mirrors the same call MM_Game_Init makes on first
    // entry, and the same Audio_InitMesgQueues call OoT_Game_Resume makes.
    fprintf(stderr, "[MM] Reinitializing audio message queues...\n");
    fflush(stderr);
    AudioThread_InitMesgQueues();

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
// GameOps registration
// ============================================================================

static GameOps sMMOps = {
    "mm",
    "Majora's Mask",
    MM_Game_Init,
    MM_Game_Run,
    MM_Game_Suspend,
    MM_Game_Resume,
    MM_Game_Shutdown
};

extern "C" GameOps* MM_GetGameOps(void) {
    return &sMMOps;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
