/**
 * Game Entry Points for MM (2Ship2Harkinian) - Single Executable Build
 *
 * This file provides the MM_Game_* functions expected by the redship
 * main.cpp for single-executable builds.
 *
 * In single-exe mode, the libultraship context is created and owned by the
 * harness (rsbs/src/main.cpp) before either game's Init runs. MM skips its
 * own InitOTR() — the shared context is set up by the harness, and OoT's
 * OTRGlobals layer adds resource factories on top when OoT is the running
 * game. MM contributes its own resource factories via
 * RegisterMMResourceFactories() below. This is symmetric with OoT (which
 * also reuses the harness-provided context) — neither game depends on the
 * other having booted first (#271).
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>
#include <cstring>
#include <cassert>
#include <filesystem>
#include <string>

#include <ship/Context.h>
#include <ship/window/Window.h>
#include <fast/Fast3dWindow.h>
#include <libultraship/controller/controldeck/ControlDeck.h>
#include "BenPort.h" // BTN_CUSTOM_MODIFIER1/2 for the shared ControlDeck
#include <memory>
#include <vector>

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

// Common factories registered by the MM-first bootstrap (#330) — same set
// BenPort.cpp registers standalone; in OoT-first boots OoT registers these.
#include <fast/resource/ResourceType.h>
#include <fast/resource/factory/DisplayListFactory.h>
#include <fast/resource/factory/MatrixFactory.h>
#include <fast/resource/factory/TextureFactory.h>
#include <fast/resource/factory/VertexFactory.h>
#include <ship/resource/factory/BlobFactory.h>
#include "2s2h/resource/importer/AnimationFactory.h"
#include "2s2h/resource/importer/ArrayFactory.h"
#include "2s2h/resource/importer/AudioSampleFactory.h"
#include "2s2h/resource/importer/AudioSequenceFactory.h"
#include "2s2h/resource/importer/AudioSoundFontFactory.h"
#include "2s2h/resource/importer/BackgroundFactory.h"
#include "2s2h/resource/importer/CollisionHeaderFactory.h"
#include "2s2h/resource/importer/CutsceneFactory.h"
#include "2s2h/resource/importer/PlayerAnimationFactory.h"
#include "2s2h/resource/importer/SceneFactory.h"
#include "2s2h/resource/importer/SkeletonFactory.h"
#include "2s2h/resource/importer/SkeletonLimbFactory.h"

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

// Track if MM has been initialized (for re-entry after game switch)
static bool sMMInitialized = false;
static bool sMMArchivesLoaded = false;

// Integration test hook frame counters (reset each time hooks are registered)
static int sConsoleLogoFrameCount = 0;
static int sGameStateMainFrameCount = 0;

// ============================================================================
// Integration Test Hooks
// ============================================================================

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
        sConsoleLogoFrameCount = 0;
        sGameStateMainFrameCount = 0;

        // Register hook for console logo update (early boot detection)
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnConsoleLogoUpdate>(
            []() {
                sConsoleLogoFrameCount++;
                // Wait a few frames to ensure stable boot
                if (sConsoleLogoFrameCount >= 5) {
                    fprintf(stderr, "[MM-INT-TEST] OnConsoleLogoUpdate hook fired (frame %d)!\n", sConsoleLogoFrameCount);
                    fflush(stderr);
                    IntegrationTest_SignalBootComplete(GAME_MM, "console logo update");
                }
            }
        );

        // Register hook for game state main start (alternative detection)
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>(
            []() {
                sGameStateMainFrameCount++;
                // Wait a few frames to ensure stable boot
                if (sGameStateMainFrameCount >= 10) {
                    fprintf(stderr, "[MM-INT-TEST] OnGameStateMainStart hook fired (frame %d)!\n", sGameStateMainFrameCount);
                    fflush(stderr);
                    IntegrationTest_SignalBootComplete(GAME_MM, "game state main start");
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

        sConsoleLogoFrameCount = 0;
        sGameStateMainFrameCount = 0;

        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>(
            []() {
                sGameStateMainFrameCount++;
                if (sGameStateMainFrameCount >= 10) {
                    fprintf(stderr,
                            "[MM-INT-TEST] MM stable after HMS->MM switch (frame %d)\n",
                            sGameStateMainFrameCount);
                    fflush(stderr);
                    IntegrationTest_SignalBootComplete(GAME_MM, "MM stable after HMS->MM switch");
                }
            }
        );

        fprintf(stderr, "[MM] HMS->MM switch hooks registered\n");
        fflush(stderr);
    } else if (mode == INT_TEST_SWITCH_MM_CLOCKTOWN_SOUTH_TO_OOT) {
        // T2 (#261): Boot MM, programmatically trigger the South Clock Town
        // south exit, assert the cross-game switch resolves to OoT Market.
        // Mirror of T1: MM is the trigger side here, OoT is the receiver.
        // MM has no OnPresentFileSelect analog, so we wait N stable frames in
        // OnGameStateMainStart and then fire the trigger once. Final pass is
        // signaled from the OoT-side hook after OoT stabilizes post-switch.
        fprintf(stderr, "[MM] Registering integration test hooks for SCT-south->OoT switch (T2)\n");
        fflush(stderr);

        sConsoleLogoFrameCount = 0;
        sGameStateMainFrameCount = 0;

        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>(
            []() {
                static bool sTriggered = false;
                if (sTriggered) {
                    return;
                }
                sGameStateMainFrameCount++;
                if (sGameStateMainFrameCount < 10) {
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

        sConsoleLogoFrameCount = 0;
        sGameStateMainFrameCount = 0;

        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>(
            []() {
                // Fire once per arrival, ~10 stable frames after (re)entry, then
                // re-arm for the next MM arrival (reached via MM_Game_Resume).
                sGameStateMainFrameCount++;
                if (sGameStateMainFrameCount < 10) {
                    return;
                }
                sGameStateMainFrameCount = 0;

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

    const std::string mmAppName = "2s2h";
    int loaded = 0;

    // Try mm.o2r (primary), then .zip/.otr fallbacks
    for (const char* ext : {"mm.o2r", "mm.zip", "mm.otr"}) {
        std::string path = Ship::Context::LocateFileAcrossAppDirs(ext, mmAppName);
        if (!path.empty() && std::filesystem::exists(path)) {
            if (archiveMgr->AddArchive(path)) {
                fprintf(stderr, "[MM] Loaded archive: %s\n", path.c_str());
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
 * Register MM-only resource factories into the shared ResourceLoader.
 * OoT already registered shared types (Animation, Skeleton, etc.).
 * MM needs its own factories for Path (overwrites OoT's — MM paths have
 * extra fields), TextMM, TextureAnimation, and KeyFrame types.
 */
static void RegisterMMResourceFactories() {
    auto loader = Ship::Context::GetInstance()->GetResourceManager()->GetResourceLoader();

    // Path — overwrites OoT's PathV0 because MM paths have additional fields
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryPathMMV0>(), RESOURCE_FORMAT_BINARY,
                                    "Path", static_cast<uint32_t>(SOH::ResourceType::SOH_Path), 0,
                                    /*allowOverwrite=*/true);

    // TextMM — MM-only text format
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryTextMMV0>(), RESOURCE_FORMAT_BINARY,
                                    "TextMM", static_cast<uint32_t>(SOH::ResourceType::TSH_TextMM), 0);

    // TextureAnimation — MM-only
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryTextureAnimationV0>(),
                                    RESOURCE_FORMAT_BINARY, "TextureAnimation",
                                    static_cast<uint32_t>(SOH::ResourceType::TSH_TexAnim), 0);

    // KeyFrame animation and skeleton — MM-only
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryKeyFrameAnim>(), RESOURCE_FORMAT_BINARY,
                                    "KeyFrameAnim", static_cast<uint32_t>(SOH::ResourceType::TSH_CKeyFrameAnim), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryKeyFrameSkel>(), RESOURCE_FORMAT_BINARY,
                                    "KeyFrameSkel", static_cast<uint32_t>(SOH::ResourceType::TSH_CKeyFrameSkel), 0);

    fprintf(stderr, "[MM] Registered MM resource factories\n");
}

/**
 * Bootstrap the harness-provided Ship::Context when MM is the first game
 * to boot (#330). The harness (rsbs/src/main.cpp) creates only an
 * *uninitialized* singleton — no window, no ResourceManager. Whichever game
 * boots first must initialize the shared subsystems (#271's symmetric
 * design; OoT's side lives in OTRGlobals::OTRGlobals(), #329). When the
 * other game already bootstrapped the context (ResourceManager present),
 * this is a no-op and MM only adds its archives/factories on top, as before.
 *
 * Mirrors the init order of BenPort.cpp's InitOTR(), which is excluded from
 * single-exe builds: FileDropMgr, GfxDebugger, Configuration, ConsoleVars,
 * Logging, ResourceManager, ControlDeck, CrashHandler, Console, Window,
 * Audio. The ResourceManager is seeded with the same archive set
 * LoadMMArchives() would add, so that pass is marked done.
 */
static bool BootstrapSharedContext(void) {
    auto ctx = Ship::Context::GetInstance();
    if (!ctx) {
        fprintf(stderr, "[MM] ERROR: Ship::Context is null — harness must init first\n");
        return false;
    }
    if (ctx->GetResourceManager() != nullptr) {
        return true;
    }

    fprintf(stderr, "[MM] Bootstrapping harness-provided Ship::Context (#330)\n");
    ctx->InitFileDropMgr();
    ctx->InitGfxDebugger();
    ctx->InitConfiguration();
    ctx->InitConsoleVariables();
    ctx->InitLogging();

    std::vector<std::string> archiveFiles;
    for (const char* name : { "mm.o2r", "mm.zip", "mm.otr" }) {
        std::string path = Ship::Context::LocateFileAcrossAppDirs(name, "2s2h");
        if (!path.empty() && std::filesystem::exists(path)) {
            archiveFiles.push_back(path);
            break; // only one mm archive, same as LoadMMArchives()
        }
    }
    std::string shipPath = Ship::Context::GetPathRelativeToAppBundle("2ship.o2r");
    if (!shipPath.empty() && std::filesystem::exists(shipPath)) {
        archiveFiles.push_back(shipPath);
    }
    // 3 reserved threads (Game, Audio, Save), matching BenPort and OoT.
    ctx->InitResourceManager(archiveFiles, {}, 3, true);
    if (!archiveFiles.empty()) {
        for (const auto& f : archiveFiles) {
            fprintf(stderr, "[MM] Loaded archive: %s\n", f.c_str());
        }
        sMMArchivesLoaded = true; // LoadMMArchives() would re-add the same set
    }

    // When MM boots first there is no OoT layer to register the common
    // resource factories (Texture, DisplayList, audio types, ...) — without
    // them the loader cannot deserialize anything from mm.o2r/2ship.o2r
    // (first casualty: MM_AudioLoad_Init null-derefs on a sequence). Register
    // the same set BenPort.cpp registers standalone; the MM-specific ones
    // (Path, TextMM, TextureAnimation, KeyFrame*) follow in
    // RegisterMMResourceFactories(), and an OoT booted second overwrites
    // shared entries with its own equivalents, as MM does today in reverse.
    auto loader = ctx->GetResourceManager()->GetResourceLoader();
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV0>(), RESOURCE_FORMAT_BINARY,
                                    "Texture", static_cast<uint32_t>(Fast::ResourceType::Texture), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV1>(), RESOURCE_FORMAT_BINARY,
                                    "Texture", static_cast<uint32_t>(Fast::ResourceType::Texture), 1);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryVertexV0>(), RESOURCE_FORMAT_BINARY,
                                    "Vertex", static_cast<uint32_t>(Fast::ResourceType::Vertex), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryXMLVertexV0>(), RESOURCE_FORMAT_XML, "Vertex",
                                    static_cast<uint32_t>(Fast::ResourceType::Vertex), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryDisplayListV0>(),
                                    RESOURCE_FORMAT_BINARY, "DisplayList",
                                    static_cast<uint32_t>(Fast::ResourceType::DisplayList), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryXMLDisplayListV0>(), RESOURCE_FORMAT_XML,
                                    "DisplayList", static_cast<uint32_t>(Fast::ResourceType::DisplayList), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryMatrixV0>(), RESOURCE_FORMAT_BINARY,
                                    "Matrix", static_cast<uint32_t>(Fast::ResourceType::Matrix), 0);
    loader->RegisterResourceFactory(std::make_shared<Ship::ResourceFactoryBinaryBlobV0>(), RESOURCE_FORMAT_BINARY,
                                    "Blob", static_cast<uint32_t>(Ship::ResourceType::Blob), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryArrayV0>(), RESOURCE_FORMAT_BINARY,
                                    "Array", static_cast<uint32_t>(SOH::ResourceType::SOH_Array), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAnimationV0>(), RESOURCE_FORMAT_BINARY,
                                    "Animation", static_cast<uint32_t>(SOH::ResourceType::SOH_Animation), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryPlayerAnimationV0>(),
                                    RESOURCE_FORMAT_BINARY, "PlayerAnimation",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_PlayerAnimation), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinarySceneV0>(), RESOURCE_FORMAT_BINARY,
                                    "Room", static_cast<uint32_t>(SOH::ResourceType::SOH_Room), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryCollisionHeaderV0>(),
                                    RESOURCE_FORMAT_BINARY, "CollisionHeader",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_CollisionHeader), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinarySkeletonV0>(), RESOURCE_FORMAT_BINARY,
                                    "Skeleton", static_cast<uint32_t>(SOH::ResourceType::SOH_Skeleton), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinarySkeletonLimbV0>(),
                                    RESOURCE_FORMAT_BINARY, "SkeletonLimb",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_SkeletonLimb), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryCutsceneV0>(), RESOURCE_FORMAT_BINARY,
                                    "Cutscene", static_cast<uint32_t>(SOH::ResourceType::SOH_Cutscene), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAudioSampleV2>(), RESOURCE_FORMAT_BINARY,
                                    "AudioSample", static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSample), 2);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLAudioSampleV0>(), RESOURCE_FORMAT_XML,
                                    "Sample", static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSample), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAudioSoundFontV2>(),
                                    RESOURCE_FORMAT_BINARY, "AudioSoundFont",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSoundFont), 2);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLSoundFontV0>(), RESOURCE_FORMAT_XML,
                                    "SoundFont", static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSoundFont), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAudioSequenceV2>(),
                                    RESOURCE_FORMAT_BINARY, "AudioSequence",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSequence), 2);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLAudioSequenceV0>(), RESOURCE_FORMAT_XML,
                                    "Sequence", static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSequence), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryBackgroundV0>(), RESOURCE_FORMAT_BINARY,
                                    "Background", static_cast<uint32_t>(SOH::ResourceType::SOH_Background), 0);

    auto controlDeck = std::make_shared<LUS::ControlDeck>(std::vector<CONTROLLERBUTTONS_T>({
        BTN_CUSTOM_MODIFIER1,
        BTN_CUSTOM_MODIFIER2,
    }));
    ctx->InitControlDeck(controlDeck);
    ctx->InitCrashHandler();
    ctx->InitConsole();

    auto fast3dWindow = std::make_shared<Fast::Fast3dWindow>(std::vector<std::shared_ptr<Ship::GuiWindow>>({}));
    ctx->InitWindow(fast3dWindow);
    ctx->InitAudio({ .SampleRate = 32000, .SampleLength = 1024, .DesiredBuffered = 1680 });

    return ctx->GetWindow() != nullptr;
}

/**
 * Verify the shared Ship::Context is ready for MM's graph thread (#271).
 *
 * MM_Graph_ThreadEntry calls WindowIsRunning(), GfxDebuggerIsDebugging(),
 * WindowGetWidth/Height/AspectRatio() every frame — all route through
 * Ship::Context::GetInstance(). The harness creates the singleton and the
 * first game to boot bootstraps its subsystems (BootstrapSharedContext
 * above / OTRGlobals on the OoT side, #329/#330), so this check is a
 * defensive assertion that that happened — *not* a dependency on OoT
 * having booted first.
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

    // In single-exe mode, skip InitOTR() — the harness (rsbs/src/main.cpp)
    // creates the Ship::Context singleton before either game's Init runs,
    // and the first game to boot initializes its subsystems (#329/#330).
    // When MM boots first, BootstrapSharedContext() does that here; when
    // OoT booted first, it is a no-op and MM just adds its own archives
    // and factories on top (#158, #271).
    if (!BootstrapSharedContext() || !VerifySharedContext()) {
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

    // MM's GameInteractor singleton is constructed in BenPort.cpp's InitOTR,
    // which is excluded from single-exe builds — without it every
    // RegisterGameHook call (integration hooks, gameplay hooks) null-derefs.
    // OoT's InitOTR news up only OoT's own GameInteractor class (#330).
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }

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

} // extern "C"

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
