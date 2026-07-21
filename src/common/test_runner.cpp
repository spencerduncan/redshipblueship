/**
 * @file test_runner.cpp
 * @brief Integration test runner for single-executable architecture
 */

#include "test_runner.h"
#include "context.h"
#include "entrance.h"
#include "integration_test_hooks.h"
#include "headless_crash.h"
#include "rsbs_version.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <atomic>
#include <filesystem>

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>

// Shared-context bring-up entries for the boot regression tests (#329/#330).
// Defined in games/oot/soh/OTRGlobals.cpp and
// games/mm/2s2h/GameExports_SingleExe.cpp; resolved at final link like the
// ArchiveHotswap_* helpers.
extern "C" {
int OoT_InitSharedContextSubsystems(void);
int MM_RegisterResourceFactoriesHeadless(void);
// MM scene-command EXECUTE regression (#344). Body lives in an MM TU
// (games/mm/2s2h/mm_scene_execute_test.cpp) so MM's global.h / PlayState never
// enters this translation unit; called through this C entry point, mirroring
// MM_RegisterResourceFactoriesHeadless. Returns 0 on pass, non-zero on fail.
int MM_SceneExecute_RunHeadless(void);
// CosmeticEditor gfx-wrapper contract (games/mm/2s2h/CosmeticGfxSingleExe.cpp):
// MM's HUD draw consumes these wrappers' Gfx* return as its display-list
// write pointer; the old void stubs in mm_stubs.c fed it garbage (WRITE AV
// at 0xA7 in MM_Interface_DrawItemButtons, first HUD-visible MM frame).
// Returns 0 on pass, non-zero on fail.
int CosmeticGfxStub_RunHeadless(void);
// MM resume-path cold-boot contracts (games/mm/2s2h/mm_resume_state_test.cpp):
// (a) mm-resume-arena — MM_ResumeColdBootPrep must make an exhausted system
//     arena allocatable again (the cycle-2 re-entry crash class: the retired
//     session leaks the whole arena and the next boot chain's first gamestate
//     malloc returned NULL -> memset(NULL) AV).
// (b) mm-startup-restore — MM_Play_ConsumeStartupEntrance must re-apply the
//     frozen MM save after the boot chain's SaveContext wipes, then spawn at
//     the startup entrance with cutscene/game-mode state reset.
// Return 0 on pass, non-zero on fail.
int MM_ResumeArena_RunHeadless(void);
int MM_StartupRestore_RunHeadless(void);
// MM extended-culling binding (games/mm/2s2h/mm_culling_test.cpp, #382): MM's
// Ship_ExtendedCullingActor* calls used to bind OoT's bodies, which index
// Actor::projectedPos 8 bytes earlier than MM's Actor puts it, and the restore
// was a one-parameter no-op stub. Returns 0 on pass, non-zero on fail.
int MM_CullingBinding_RunHeadless(void);
// MM GameInteractor shim lock (games/mm/2s2h/mm_gi_shim_test.cpp, #395): MM's
// hook registrations must never touch the shared 4-byte OoT-owned
// GameInteractor instance through MM's larger view of the class (a ~60-92
// byte out-of-bounds write, layout is platform-dependent). Returns 0 on
// pass, non-zero on fail.
int MM_GIShim_RunHeadless(void);
// MM Notification::Emit cross-bind lock (games/mm/2s2h/mm_notification_binding_test.cpp,
// #427 item 1): MM's 2s2h/BenGui/Notification.cpp is excluded from single-exe
// builds, so MM's Rando pickup toast binds OoT's Notification::Emit against
// MM's own view of Notification::Options. Safe only while the two ports' Options
// stay field-identical; this compares their layout fingerprints and fails on
// drift (no link error can catch it — one Emit definition survives). Returns 0
// on pass, non-zero on fail.
int MM_NotificationBinding_RunHeadless(void);
// MM scaled framebuffer-draw binding (games/mm/2s2h/mm_fb_effects_test.cpp,
// #386): MM's FB_DrawFromFramebufferScaled used to bind OoT's surviving body,
// which reads OoT_gScreenWidth/Height — the wrong dimensions for MM, which go
// to HiRes 576 while the Bombers' Notebook is open. Returns 0 on pass, non-zero
// on fail.
int MM_FbEffectsBinding_RunHeadless(void);
// VB-affinity regression: MM's GameInteractor_* calls resolve to OoT's
// extern "C" wrappers in single-exe builds, and the two games' vanilla-
// behavior ordinals alias each other. The wrappers gate on the active game;
// these helpers (GameInteractor_Hooks.cpp) arm a veto hook to prove it.
bool GameInteractor_Should(int32_t flag, uint32_t result, ...);
void GameInteractor_TestArmVBVeto(int32_t flag);
void GameInteractor_TestDisarmVBVeto(void);
}

// Lifecycle unit tests — included directly to avoid static library link ordering issues
extern "C" {
#include "tests/test_game_lifecycle.c"
}

// Roundtrip SaveContext byte-integrity test (issue #262). Included at FILE
// SCOPE (compiled as C++), NOT inside the extern "C" block above: its body
// calls the C++-linkage Entrance_Init/Entrance_RegisterDefaultLinks. Under
// extern "C" those would bind to OoT's C-linkage randomizer Entrance_Init
// instead of the combo entrance system.
#include "tests/test_roundtrip_integrity.c"

// Shared-state plumbing smoke test (issue #264) — included like the lifecycle
// test (its own extern "C" block) to avoid static-library link ordering
// issues. It only references C-linkage ComboContext_* symbols and gComboCtx.
extern "C" {
#include "tests/test_shared_state_roundtrip.c"
}

// Hot-swap (F10) freeze/consume contract (issue #364). Included inside its own
// extern "C" block: it only touches C-linkage Context_/Combo_/Switch_ symbols,
// so it has no reason to be compiled as C++ and bind anything by mangled name.
extern "C" {
#include "tests/test_hotswap_freeze.c"
}

// Archive hot-swap regression test (issue #263). Included at FILE SCOPE (not
// inside an extern "C" block): it is compiled as C++ and uses the C++-linkage
// Entrance_* API for setup. Its cross-TU ArchiveHotswap_* helpers are wrapped
// in their own extern "C" inside the file.
#include "tests/test_archive_hotswap.c"

// Unified save (.redsave) headless tests (issue #35, Phase 2 T6). Included at
// FILE SCOPE (compiled as C++): they drive the C++-linkage rsbs::SaveManager.
#include "tests/test_save_roundtrip.c"

// Lane C1 foreign-item pipeline locks (#392, ADR 0002): give-path tagging,
// round-trip survival with a real pool entry, and the foreignPlacements carve
// serialization. FILE SCOPE (compiled as C++) for rsbs::SaveManager; the
// pipeline symbols it drives are C-linkage and declared in the file.
#include "tests/test_foreign_items.c"

// MM scene-command parse regression (issue #344). Included at FILE SCOPE
// (compiled as C++): it drives MM's S2H::ResourceFactoryBinarySceneV0 directly
// over a synthetic scene buffer — no ROM archives or display needed.
#include "tests/test_mm_scene_parse.c"

// Sequence-map capacity bounds (#371, #378). Included at FILE SCOPE: it calls
// the two C-linkage capacity helpers in games/oot/src/code/audio_load.c and
// games/mm/src/audio/lib/load.c, which it declares itself in an extern "C"
// block (including either game's z64audio.h here would collide on
// MAX_AUTHENTIC_SEQID). Pure arithmetic — no audio subsystem, no archives.
#include "tests/test_seq_map_bounds.c"

// Active-thread-queue contract (issue #385). soh/stubs.c's empty-bodied
// __osGetActiveQueue fed a return register to the crash handler's thread walk
// (fault.c:537). Included at FILE SCOPE like the rest; declares the C-linkage
// symbols it needs itself. No subsystem, no archives.
#include "tests/test_active_queue.c"

// OoT audio producer init-guard (#365). The shared OTRAudio_Thread dispatches
// OoT_AudioMgr_CreateNextAudioBuffer whenever MM's synth is inactive, including
// while OoT is suspended with gAudioContextInitalized == false — the producer
// must no-op there rather than drive the synth against a torn-down context.
// FILE SCOPE; declares its C-linkage symbols itself. No audio heap, no archives.
#include "tests/test_oot_audio_init_guard.c"

// Gameplay round-trip phase watchdog (#376 item 4). FILE SCOPE: it exercises
// the pure watchdog helpers declared in integration_test_hooks.h (already
// included above), proving the budget is wall-clock and can fire before the
// CTest timeout. No display, no ROM archives, no game loop.
#include "tests/test_gp_watchdog.c"

// MM scene-command EXECUTE regression (issue #344). Unlike the parse test, the
// body runs the parsed commands against a PlayState, so it needs MM's global.h
// — which lives in an MM TU (games/mm/2s2h/mm_scene_execute_test.cpp) to keep
// MM's umbrella headers out of this TU. Thin wrapper over the C entry point.
static TestResult Test_MMSceneExecute(void) {
    return MM_SceneExecute_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// CosmeticEditor gfx-wrapper contract (see the extern decl above). Thin
// wrapper over the C entry point in games/mm/2s2h/CosmeticGfxSingleExe.cpp.
static TestResult Test_CosmeticGfxStub(void) {
    return CosmeticGfxStub_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// MM resume-path cold-boot contracts (see the extern decls above). Thin
// wrappers over the C entry points in games/mm/2s2h/mm_resume_state_test.cpp.
static TestResult Test_MMResumeArena(void) {
    return MM_ResumeArena_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}
static TestResult Test_MMStartupRestore(void) {
    return MM_StartupRestore_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// MM extended-culling binding (see the extern decl above). Thin wrapper over
// the C entry point in games/mm/2s2h/mm_culling_test.cpp.
static TestResult Test_MMCullingBinding(void) {
    return MM_CullingBinding_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// MM GameInteractor shim lock (see the extern decl above). Thin wrapper over
// the C entry point in games/mm/2s2h/mm_gi_shim_test.cpp.
static TestResult Test_MMGIShim(void) {
    return MM_GIShim_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// MM Notification::Emit cross-bind lock (see the extern decl above). Thin
// wrapper over the C entry point in
// games/mm/2s2h/mm_notification_binding_test.cpp.
static TestResult Test_MMNotificationBinding(void) {
    return MM_NotificationBinding_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// MM scaled framebuffer-draw binding lock (see the extern decl above). Thin
// wrapper over the C entry point in games/mm/2s2h/mm_fb_effects_test.cpp.
static TestResult Test_MMFbEffectsBinding(void) {
    return MM_FbEffectsBinding_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// Internal state
// ============================================================================

namespace {

bool sTestMode = false;
bool sIntegrationTestMode = false;
GameId sTargetGame = GAME_NONE;
std::atomic<bool> sBootComplete{false};
const char* sIntegrationTestName = nullptr;

// ============================================================================
// Test implementations
// ============================================================================

// Reproduce the harness's exact pre-boot state (rsbs/src/main.cpp): an
// uninitialized Ship::Context singleton. SHIP_HOME is pointed at a sandbox
// directory first so InitConfiguration/InitLogging write their files there
// instead of the working tree (same pattern as the .redsave tests). Note:
// libultraship only honors SHIP_HOME on Linux/macOS — on Windows the files
// land in the working directory like any portable-mode run. If the
// singleton already exists (e.g. --test all ran the other boot test first),
// CreateUninitializedInstance returns it unchanged — which mirrors the
// second game's arrival on a live context.
static std::shared_ptr<Ship::Context> CreateHarnessStyleContext(void) {
    const char* kBootTestHome = "rsbs_test_boot_home";
    std::error_code ec;
    std::filesystem::create_directories(kBootTestHome, ec);
#ifdef _WIN32
    _putenv_s("SHIP_HOME", kBootTestHome);
#else
    setenv("SHIP_HOME", kBootTestHome, 1);
#endif
    auto ctx = Ship::Context::CreateUninitializedInstance(RSBS_WINDOW_TITLE, "soh", "shipofharkinian.json");

    // Take crash handling before any bring-up runs (#388). A unit test is
    // unattended by definition even though it runs under a live DISPLAY
    // (rando-gen needs Xvfb), so say so explicitly rather than letting the
    // DISPLAY heuristic conclude "desktop session". Without this, a fault
    // inside the bring-up below ends in libultraship's modal
    // SDL_ShowSimpleMessageBox and the test burns its full CTest timeout
    // instead of reporting a crash.
    HeadlessCrash_ForceHeadless();
    if (ctx) {
        HeadlessCrash_ClaimAndInstall();
    }

    return ctx;
}

TestResult Test_BootOoT(void) {
    printf("[TEST] boot-oot: shared-context bring-up leaves no null subsystems (#329)\n");
    sTargetGame = GAME_OOT;
    sBootComplete = false;

    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }

    // Run the display-free prefix of the bring-up OoT's OTRGlobals
    // constructor performs. #329's failure mode was exactly this being
    // skipped on the harness context: OTRGlobals::Initialize() then crashed
    // dereferencing the null ResourceManager. Scope honesty: this drives the
    // extracted helper, not the constructor itself — a regression INSIDE the
    // constructor's branch logic (e.g. re-adding an early return before the
    // helper call) is only caught by the archive-gated int-boot tests, which
    // boot the real binary. Window/audio init needs a display and likewise
    // stays in the int tier.
    if (OoT_InitSharedContextSubsystems() != 0) {
        printf("[TEST] FAIL: OoT_InitSharedContextSubsystems reported failure\n");
        return TEST_FAIL;
    }

    if (ctx->GetConfig() == nullptr) {
        printf("[TEST] FAIL: Config is null after bring-up\n");
        return TEST_FAIL;
    }
    if (ctx->GetConsoleVariables() == nullptr) {
        printf("[TEST] FAIL: ConsoleVariables is null after bring-up\n");
        return TEST_FAIL;
    }
    if (ctx->GetControlDeck() == nullptr) {
        printf("[TEST] FAIL: ControlDeck is null after bring-up\n");
        return TEST_FAIL;
    }
    if (ctx->GetResourceManager() == nullptr) {
        printf("[TEST] FAIL: ResourceManager is null after bring-up — the #329 crash predicate\n");
        return TEST_FAIL;
    }
    if (ctx->GetResourceManager()->GetArchiveManager() == nullptr) {
        printf("[TEST] FAIL: ArchiveManager is null after bring-up\n");
        return TEST_FAIL;
    }
    if (ctx->GetConsole() == nullptr) {
        printf("[TEST] FAIL: Console is null after bring-up\n");
        return TEST_FAIL;
    }

    printf("[TEST] PASS: display-free shared subsystems all initialized\n");
    return TEST_PASS;
}

extern "C" int Rando_HeadlessSeedTest(const char* seedStr);
// Lane B unified-seed determinism bridge (body in
// games/oot/soh/Enhancements/randomizer/3drando/menu.cpp). Runs ONE seed
// generation, asserts the live producer stamped gComboCtx, and writes a
// canonical placement+seed digest to `outPath` (NULL => stdout). Returns 0 on
// success. Kept behind a C entry point so the OoT randomizer headers never enter
// this delicate multi-include TU, mirroring MM_SceneExecute_RunHeadless.
extern "C" int Rando_HeadlessSeedDeterminismDigest(const char* seedStr, const char* outPath);
// Lane C1 foreign-placement determinism (body in games/mm/2s2h/
// mm_rando_gen_test.cpp): generates the PAIRED MM world keyed off the
// gComboCtx the OoT digest run just stamped and APPENDS the MM world identity
// + every foreign placement to the same digest file, so the SeedDeterminism
// two-process diff covers the whole paired world.
extern "C" int MM_Rando_HeadlessForeignDigest(const char* outPath);
// Hint-validity lock (#441, body in menu.cpp): generates one seed, then asserts
// every enabled hint names a REAL item — no hint may resolve to the no-item
// sentinel, hinted locations must hold items, and each hinted location's name
// must round-trip through locationNameToEnum (the transform the save file puts
// hints through). Returns 0 only if every enabled hint passes all three.
extern "C" int Rando_HeadlessHintValidityTest(const char* seedStr);
extern "C" void InitOTRForMMFirstBoot(int argc, char* argv[]);

TestResult Test_RandoGen(void) {
    printf("[TEST] rando-gen: seed generation succeeds with default settings (#337)\n");

    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }

    // Full OoT bring-up minus the extraction prompt (the MM-first path):
    // tolerant of a missing oot.o2r, constructs OTRGlobals::Instance,
    // gRandomizer and gRandoContext, which the fill logic dereferences all
    // over. Needs a display (Fast3dWindow) — run under Xvfb.
    static char arg0[] = "redship";
    static char* fakeArgv[] = { arg0, nullptr };
    InitOTRForMMFirstBoot(1, fakeArgv);

    int rc = Rando_HeadlessSeedTest("RSBSDIAG1");
    printf("[TEST] %s: seed generation rc=%d\n", rc == 0 ? "PASS" : "FAIL", rc);
    return rc == 0 ? TEST_PASS : TEST_FAIL;
}

// Hint-validity lock (#441). Gossip stones were reading "They say that catching
// Big Poes leads to No Item" while the spoiler for that same seed named a real
// item, so the fill was complete and only the hint-side resolution was wrong.
// This drives one generation and proves no hint resolves to the no-item
// sentinel. Needs a display (Fast3dWindow) like rando-gen, so `--test all`
// skips it.
TestResult Test_RandoHintValidity(void) {
    printf("[TEST] rando-hint-validity: every generated hint names a real item (#441)\n");

    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }

    static char arg0[] = "redship";
    static char* fakeArgv[] = { arg0, nullptr };
    InitOTRForMMFirstBoot(1, fakeArgv);

    int rc = Rando_HeadlessHintValidityTest("RSBSHINT1");
    printf("[TEST] %s: hint validity rc=%d\n", rc == 0 ? "PASS" : "FAIL", rc);
    return rc == 0 ? TEST_PASS : TEST_FAIL;
}

// Lane B unified-seed lock. Single run: bring up OoT, generate one pinned seed,
// and (inside the bridge) assert the LIVE producer stamped
// gComboCtx.sourceIsRando/sharedRandoSeed at generation time, then emit a
// canonical digest of the seed fields + full placement. The two-process
// same-seed determinism comparison is driven by the SeedDeterminism CTest row
// (CMake/CheckSeedDeterminism.cmake), which runs this dispatch twice with
// distinct RSBS_SEED_DIGEST_OUT paths and diffs them — two processes, because
// re-entering the generator in one process is unverified. Needs a display
// (Fast3dWindow) like rando-gen, so it is skipped by `--test all`.
TestResult Test_RandoDeterminism(void) {
    printf("[TEST] rando-determinism: unified-seed producer fires + placement digest emitted (Lane B)\n");

    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }

    static char arg0[] = "redship";
    static char* fakeArgv[] = { arg0, nullptr };
    InitOTRForMMFirstBoot(1, fakeArgv);

    // Pinned SEED for the unified-seed contract. The pinned SETTINGS profile is
    // supplied via the CTest ENVIRONMENT (RSBS_DIAG_CVARS), so the determinism
    // wrapper's two runs share it (see CMake/CheckSeedDeterminism.cmake).
    const char* seed = "RSBSUNIFIED1";
    const char* digestOut = std::getenv("RSBS_SEED_DIGEST_OUT");  // NULL => digest to stdout
    int rc = Rando_HeadlessSeedDeterminismDigest(seed, digestOut);
    printf("[TEST] %s: determinism digest rc=%d\n", rc == 0 ? "PASS" : "FAIL", rc);
    if (rc != 0) {
        return TEST_FAIL;
    }

    // Lane C1: the MM half of the paired world, keyed off the gComboCtx the
    // live OoT producer stamped during the generation above — the real
    // end-to-end pairing path. Appends to the same digest, so the
    // SeedDeterminism diff locks OoT fill + MM fill + foreign placements
    // together.
    int mmRc = MM_Rando_HeadlessForeignDigest(digestOut);
    printf("[TEST] %s: foreign-placement digest rc=%d\n", mmRc == 0 ? "PASS" : "FAIL", mmRc);
    return mmRc == 0 ? TEST_PASS : TEST_FAIL;
}

// Lane C0 reachability lock (#392): MM's 2ship_rando is un-elided and
// actually generates — ShipInit registrars populated the Logic/Regions
// graph, OnFileCreate runs GeneratePools + a logic apply headlessly, and the
// spoiler JSON lands on disk tagged 2S2H_RANDO_SPOILER. Bridge body in
// games/mm/2s2h/mm_rando_gen_test.cpp (extern "C" so MM headers never enter
// this multi-include TU). Needs a display like rando-gen (the shared bring-up
// constructs the Fast3dWindow), so `--test all` skips it.
extern "C" int MM_Rando_HeadlessGenTest(void);

TestResult Test_MMRandoGen(void) {
    printf("[TEST] mm-rando-gen: MM seed generation runs headlessly and writes a tagged spoiler (Lane C0)\n");

    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }

    static char arg0[] = "redship";
    static char* fakeArgv[] = { arg0, nullptr };
    InitOTRForMMFirstBoot(1, fakeArgv);

    int rc = MM_Rando_HeadlessGenTest();
    printf("[TEST] %s: MM rando generation rc=%d\n", rc == 0 ? "PASS" : "FAIL", rc);
    return rc == 0 ? TEST_PASS : TEST_FAIL;
}

// Switch-entry activation lock (#439). MMRandoGen drives the OnSaveInit chain
// directly; this row drives the path a player actually takes — the cold
// gamestate-chain boot a cross-game switch performs, then the real
// MM_Play_ConsumeStartupEntrance consumption point — and additionally locks
// that an EXISTING MM save (vanilla or paired) is never regenerated. Bridge
// body in games/mm/2s2h/mm_rando_gen_test.cpp. Needs a display, same as
// mm-rando-gen, so `--test all` skips it.
extern "C" int MM_Rando_HeadlessPairSwitchEntry(void);

TestResult Test_MMPairSwitchEntry(void) {
    printf("[TEST] mm-pair-switch-entry: paired MM world activates through the switch-entry path (#439)\n");

    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }

    static char arg0[] = "redship";
    static char* fakeArgv[] = { arg0, nullptr };
    InitOTRForMMFirstBoot(1, fakeArgv);

    int rc = MM_Rando_HeadlessPairSwitchEntry();
    printf("[TEST] %s: switch-entry pairing rc=%d\n", rc == 0 ? "PASS" : "FAIL", rc);
    return rc == 0 ? TEST_PASS : TEST_FAIL;
}

TestResult Test_BootMM(void) {
    printf("[TEST] boot-mm: MM-first bring-up prerequisites (#330)\n");
    sTargetGame = GAME_MM;
    sBootComplete = false;

    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }

    // MM-first boots reach the same display-free bring-up through
    // MM_Game_Init -> InitOTRForMMFirstBoot -> OTRGlobals(). Drive it
    // directly (window creation needs a display) and then assert the two
    // things MM_Game_Init needs from it: a live ResourceManager for
    // LoadMMArchives, and MM's resource factories registering against it.
    if (OoT_InitSharedContextSubsystems() != 0) {
        printf("[TEST] FAIL: shared bring-up reported failure\n");
        return TEST_FAIL;
    }

    if (ctx->GetResourceManager() == nullptr) {
        printf("[TEST] FAIL: ResourceManager is null — LoadMMArchives precondition (#330)\n");
        return TEST_FAIL;
    }

    if (MM_RegisterResourceFactoriesHeadless() != 0) {
        printf("[TEST] FAIL: MM resource factory registration failed\n");
        return TEST_FAIL;
    }

    printf("[TEST] PASS: MM-first bring-up prerequisites satisfied\n");
    return TEST_PASS;
}

TestResult Test_SwitchOoTMM(void) {
    printf("[TEST] switch-oot-mm: Test game switch OoT -> MM\n");

    // Initialize entrance system
    Entrance_Init();
    Entrance_RegisterDefaultLinks();

    // Simulate OoT triggering Happy Mask Shop entrance
    uint16_t result = Entrance_CheckCrossGame(GAME_OOT, OOT_ENTR_HAPPY_MASK_SHOP);

    if (!Entrance_IsCrossGameSwitch()) {
        printf("[TEST] FAIL: Cross-game switch not triggered\n");
        return TEST_FAIL;
    }

    if (Entrance_GetSwitchTargetGame() != GAME_MM) {
        printf("[TEST] FAIL: Target game should be MM\n");
        return TEST_FAIL;
    }

    // OoT->MM arrival is the South Clock Town tower-exit spawn (as if the
    // player just walked out of the Clock Tower), NOT Clock Tower Interior.
    if (Entrance_GetSwitchTargetEntrance() != MM_ENTR_SOUTH_CLOCK_TOWN_0) {
        printf("[TEST] FAIL: Target entrance incorrect\n");
        return TEST_FAIL;
    }

    printf("[TEST] PASS: OoT -> MM switch correctly triggered\n");
    Entrance_ClearPendingSwitch();
    return TEST_PASS;
}

TestResult Test_SwitchMMOoT(void) {
    printf("[TEST] switch-mm-oot: Test game switch MM -> OoT\n");

    // Initialize entrance system
    Entrance_Init();
    Entrance_RegisterDefaultLinks();

    // Simulate MM entering the Clock Tower from South Clock Town — the
    // tower door is the MM->OoT trigger (the SCT tower-exit spawn 0xD800 is
    // the ARRIVAL and must never trigger: MM's own cycle resets target it).
    uint16_t result = Entrance_CheckCrossGame(GAME_MM, MM_ENTR_CLOCK_TOWER_INTERIOR_1);

    if (!Entrance_IsCrossGameSwitch()) {
        printf("[TEST] FAIL: Cross-game switch not triggered\n");
        return TEST_FAIL;
    }

    if (Entrance_GetSwitchTargetGame() != GAME_OOT) {
        printf("[TEST] FAIL: Target game should be OoT\n");
        return TEST_FAIL;
    }

    printf("[TEST] PASS: MM -> OoT switch correctly triggered\n");
    Entrance_ClearPendingSwitch();
    return TEST_PASS;
}

TestResult Test_Roundtrip(void) {
    printf("[TEST] roundtrip: Full round-trip with state verification (issue #170)\n");

    // Initialize systems
    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();
    Entrance_Init();
    Entrance_RegisterDefaultLinks();

    // ------------------------------------------------------------------
    // Leg 1: OoT -> MM via Happy Mask Shop
    // ------------------------------------------------------------------
    // Use the production C API (Combo_*) — this is the same surface the
    // game code actually calls from z_play.c. Exercising it here guards
    // against the pre-#170 bug where Combo_CheckEntranceSwitch hardcoded
    // "oot" as the source game on both legs.
    Combo_CheckCrossGameEntrance("oot", OOT_ENTR_HAPPY_MASK_SHOP);
    if (!Combo_IsCrossGameSwitch()) {
        printf("[TEST] FAIL: Leg 1 - OoT->MM switch not triggered\n");
        return TEST_FAIL;
    }
    if (strcmp(Combo_GetSwitchTargetGameId(), "mm") != 0) {
        printf("[TEST] FAIL: Leg 1 - Target should be mm, got %s\n",
               Combo_GetSwitchTargetGameId());
        return TEST_FAIL;
    }

    // Freeze a fingerprint for OoT so we can verify integrity after the trip.
    // Static: at the full runtime blob capacity these buffers are too large to
    // keep on the stack (OoT alone is ~136KB).
    static uint8_t fakeOoTSave[OOT_SAVE_CONTEXT_SIZE];
    memset(fakeOoTSave, 0, sizeof(fakeOoTSave));
    fakeOoTSave[0] = 0xDE;
    fakeOoTSave[1] = 0xAD;
    fakeOoTSave[OOT_SAVE_CONTEXT_SIZE - 1] = 0xEF;  // Tail marker
    Combo_FreezeState("oot", Combo_GetSwitchReturnEntrance(),
                      fakeOoTSave, sizeof(fakeOoTSave));
    Entrance_ClearPendingSwitch();

    // ------------------------------------------------------------------
    // Leg 2: MM -> OoT via the Clock Tower door
    // ------------------------------------------------------------------
    // This is the leg that pre-#170 silently no-op'd because the shared
    // Combo_CheckEntranceSwitch implementation looked up "oot" links for
    // MM's entrance id and always missed.
    Combo_CheckCrossGameEntrance("mm", MM_ENTR_CLOCK_TOWER_INTERIOR_1);
    if (!Combo_IsCrossGameSwitch()) {
        printf("[TEST] FAIL: Leg 2 - MM->OoT switch not triggered\n");
        return TEST_FAIL;
    }
    if (strcmp(Combo_GetSwitchTargetGameId(), "oot") != 0) {
        printf("[TEST] FAIL: Leg 2 - Target should be oot, got %s\n",
               Combo_GetSwitchTargetGameId());
        return TEST_FAIL;
    }

    // Also freeze MM state; both games' frozen states must coexist.
    static uint8_t fakeMMSave[MM_SAVE_CONTEXT_SIZE];
    memset(fakeMMSave, 0, sizeof(fakeMMSave));
    fakeMMSave[0] = 0xBE;
    fakeMMSave[1] = 0xEF;
    Combo_FreezeState("mm", Combo_GetSwitchReturnEntrance(),
                      fakeMMSave, sizeof(fakeMMSave));

    // ------------------------------------------------------------------
    // Leg 3: Verify OoT state can be restored after the round-trip.
    // ------------------------------------------------------------------
    static uint8_t restoredSave[OOT_SAVE_CONTEXT_SIZE];
    memset(restoredSave, 0, sizeof(restoredSave));
    if (!Combo_RestoreState("oot", restoredSave, sizeof(restoredSave))) {
        printf("[TEST] FAIL: Leg 3 - OoT state restore failed\n");
        return TEST_FAIL;
    }
    if (restoredSave[0] != 0xDE || restoredSave[1] != 0xAD ||
        restoredSave[OOT_SAVE_CONTEXT_SIZE - 1] != 0xEF) {
        printf("[TEST] FAIL: Leg 3 - Restored OoT data corrupted\n");
        return TEST_FAIL;
    }

    // And MM's frozen state is still intact.
    static uint8_t restoredMM[MM_SAVE_CONTEXT_SIZE];
    memset(restoredMM, 0, sizeof(restoredMM));
    if (!Combo_RestoreState("mm", restoredMM, sizeof(restoredMM))) {
        printf("[TEST] FAIL: Leg 3 - MM state restore failed\n");
        return TEST_FAIL;
    }
    if (restoredMM[0] != 0xBE || restoredMM[1] != 0xEF) {
        printf("[TEST] FAIL: Leg 3 - Restored MM data corrupted\n");
        return TEST_FAIL;
    }

    printf("[TEST] PASS: Full OoT<->MM round-trip preserves both SaveContexts\n");
    Entrance_ClearPendingSwitch();
    Context_ClearAllFrozenStates();
    return TEST_PASS;
}

// Regression lock for #374: two links must never share a source door.
//
// The default (mask shop) and test (Mido's House) links both return through MM
// 0xC010. Both used to be registered unconditionally, and because
// Entrance_CheckCrossGame resolves by first match, the default silently won:
// entering MM from Mido's House and walking back into the Clock Tower dumped
// you in Hyrule Market (0x01D1) instead of Kokiri Forest (0x0443). Nothing
// reported the shadowing.
//
// ROM-free: pure table logic, no archives, no display.
TestResult Test_EntranceDedup(void) {
    printf("[TEST] entrance-dedup: duplicate links rejected; each portal face routes home\n");

    // --- Leg 1: the production portal registers, and routes both ways. ------
    Entrance_Init();
    if (!Entrance_RegisterPortalLinks(false)) {
        printf("[TEST] FAIL: default portal registration was rejected on an empty table\n");
        return TEST_FAIL;
    }
    if (Entrance_GetLinkCount() != 2) {
        printf("[TEST] FAIL: expected 2 links after default registration, got %zu\n",
               Entrance_GetLinkCount());
        return TEST_FAIL;
    }

    Entrance_CheckCrossGame(GAME_MM, MM_ENTR_CLOCK_TOWER_INTERIOR_1);
    if (!Entrance_IsCrossGameSwitch() ||
        Entrance_GetSwitchTargetGame() != GAME_OOT ||
        Entrance_GetSwitchTargetEntrance() != OOT_ENTR_MARKET_FROM_MASK_SHOP) {
        printf("[TEST] FAIL: default return leg should land at Market 0x%04X, got 0x%04X\n",
               OOT_ENTR_MARKET_FROM_MASK_SHOP, Entrance_GetSwitchTargetEntrance());
        return TEST_FAIL;
    }
    Entrance_ClearPendingSwitch();

    // --- Leg 2: stacking the test link on top must be REJECTED, atomically. -
    // This is the exact call pair main.cpp used to make unconditionally.
    if (Entrance_RegisterTestLinks()) {
        printf("[TEST] FAIL: test links were accepted despite MM 0x%04X already being claimed\n",
               MM_ENTR_CLOCK_TOWER_INTERIOR_1);
        return TEST_FAIL;
    }
    // Atomicity: a rejected call must not leave its forward leg behind. A
    // half-registered link is a one-way portal — worse than the collision.
    if (Entrance_GetLinkCount() != 2) {
        printf("[TEST] FAIL: rejected registration mutated the table (%zu links, expected 2)\n",
               Entrance_GetLinkCount());
        return TEST_FAIL;
    }
    if (Entrance_HasLinkFor(GAME_OOT, OOT_ENTR_MIDOS_HOUSE)) {
        printf("[TEST] FAIL: rejected registration left a dangling forward leg for Mido's House\n");
        return TEST_FAIL;
    }

    // --- Leg 3: the test portal, registered alone, returns to Kokiri. -------
    // Under the old first-match shadowing this produced 0x01D1 (the #374 bug).
    Entrance_Init();
    if (!Entrance_RegisterPortalLinks(true)) {
        printf("[TEST] FAIL: test portal registration was rejected on an empty table\n");
        return TEST_FAIL;
    }
    if (Entrance_HasLinkFor(GAME_OOT, OOT_ENTR_HAPPY_MASK_SHOP)) {
        printf("[TEST] FAIL: test portal must not also register the mask-shop face\n");
        return TEST_FAIL;
    }

    Entrance_CheckCrossGame(GAME_OOT, OOT_ENTR_MIDOS_HOUSE);
    if (!Entrance_IsCrossGameSwitch() ||
        Entrance_GetSwitchTargetEntrance() != MM_ENTR_SOUTH_CLOCK_TOWN_0 ||
        Entrance_GetSwitchReturnEntrance() != OOT_ENTR_KOKIRI_FROM_MIDOS) {
        printf("[TEST] FAIL: Mido's outbound leg wrong (target 0x%04X, return 0x%04X)\n",
               Entrance_GetSwitchTargetEntrance(), Entrance_GetSwitchReturnEntrance());
        return TEST_FAIL;
    }
    Entrance_ClearPendingSwitch();

    Entrance_CheckCrossGame(GAME_MM, MM_ENTR_CLOCK_TOWER_INTERIOR_1);
    if (!Entrance_IsCrossGameSwitch() ||
        Entrance_GetSwitchTargetGame() != GAME_OOT ||
        Entrance_GetSwitchTargetEntrance() != OOT_ENTR_KOKIRI_FROM_MIDOS) {
        printf("[TEST] FAIL: test return leg should land at Kokiri 0x%04X, got 0x%04X "
               "(0x%04X means the default link shadowed it — #374)\n",
               OOT_ENTR_KOKIRI_FROM_MIDOS, Entrance_GetSwitchTargetEntrance(),
               OOT_ENTR_MARKET_FROM_MASK_SHOP);
        return TEST_FAIL;
    }
    Entrance_ClearPendingSwitch();

    // --- Leg 4: rejection is keyed per-leg, not per-call. -------------------
    // A link whose OoT face is fresh but whose MM face is taken must still be
    // rejected — the MM trigger is the scarce resource.
    if (Entrance_RegisterBidirectionalLink(
            GAME_OOT, OOT_ENTR_HAPPY_MASK_SHOP, OOT_ENTR_MARKET_FROM_MASK_SHOP,
            GAME_MM, MM_ENTR_SOUTH_CLOCK_TOWN_0, MM_ENTR_CLOCK_TOWER_INTERIOR_1)) {
        printf("[TEST] FAIL: accepted a link reusing the claimed MM trigger 0x%04X\n",
               MM_ENTR_CLOCK_TOWER_INTERIOR_1);
        return TEST_FAIL;
    }

    // A link colliding with itself (forward source == reverse source) is dead
    // on arrival and must be rejected too.
    if (Entrance_RegisterBidirectionalLink(
            GAME_OOT, 0x0EDD, 0x0EDE,
            GAME_OOT, 0x0EDF, 0x0EDD)) {
        printf("[TEST] FAIL: accepted a self-colliding link\n");
        return TEST_FAIL;
    }

    // A genuinely distinct pair still registers — the guard rejects duplicates,
    // not everything.
    const size_t before = Entrance_GetLinkCount();
    if (!Entrance_RegisterBidirectionalLink(
            GAME_OOT, 0x0EE0, 0x0EE1,
            GAME_MM, 0x0EE2, 0x0EE3)) {
        printf("[TEST] FAIL: rejected a link with no source collision\n");
        return TEST_FAIL;
    }
    if (Entrance_GetLinkCount() != before + 2) {
        printf("[TEST] FAIL: accepted registration did not add both legs\n");
        return TEST_FAIL;
    }

    // Restore the default table for tests that run after this one.
    Entrance_Init();
    Entrance_RegisterDefaultLinks();
    printf("[TEST] PASS: duplicate source doors rejected atomically; each face routes home\n");
    return TEST_PASS;
}

TestResult Test_MidosHouse(void) {
    printf("[TEST] midos-house: Test Mido's House entrance (test mode)\n");

    // Initialize with TEST links (Mido's House instead of Happy Mask Shop)
    Entrance_Init();
    Entrance_RegisterTestLinks();

    // Simulate entering Mido's House in OoT
    uint16_t result = Entrance_CheckCrossGame(GAME_OOT, OOT_ENTR_MIDOS_HOUSE);

    if (!Entrance_IsCrossGameSwitch()) {
        printf("[TEST] FAIL: Cross-game switch not triggered for Mido's House\n");
        return TEST_FAIL;
    }

    if (Entrance_GetSwitchTargetGame() != GAME_MM) {
        printf("[TEST] FAIL: Target should be MM\n");
        return TEST_FAIL;
    }

    if (Entrance_GetSwitchTargetEntrance() != MM_ENTR_SOUTH_CLOCK_TOWN_0) {
        printf("[TEST] FAIL: Should target South Clock Town (tower-exit arrival)\n");
        return TEST_FAIL;
    }

    printf("[TEST] PASS: Mido's House -> MM portal link works\n");
    Entrance_ClearPendingSwitch();
    return TEST_PASS;
}

TestResult Test_StartupEntrance(void) {
    printf("[TEST] startup-entrance: Test startup entrance flow\n");

    // Initialize systems
    Entrance_Init();
    Entrance_RegisterTestLinks();

    // Step 1: Simulate OoT triggering Mido's House entrance
    Entrance_CheckCrossGame(GAME_OOT, OOT_ENTR_MIDOS_HOUSE);
    if (!Entrance_IsCrossGameSwitch()) {
        printf("[TEST] FAIL: Switch not triggered\n");
        return TEST_FAIL;
    }

    // Step 2: Set the startup entrance (this is what main.cpp does)
    uint16_t targetEntrance = Entrance_GetSwitchTargetEntrance();
    Entrance_SetStartupEntrance(targetEntrance);

    // Step 3: Verify startup entrance is set (the OoT->MM arrival, i.e. the
    // South Clock Town tower-exit spawn)
    uint16_t startup = Combo_GetStartupEntrance();
    if (startup != MM_ENTR_SOUTH_CLOCK_TOWN_0) {
        printf("[TEST] FAIL: Startup entrance not set correctly (got 0x%04X, expected 0x%04X)\n",
               startup, MM_ENTR_SOUTH_CLOCK_TOWN_0);
        return TEST_FAIL;
    }

    // Step 4: Clear and verify (simulates what Play_Init does)
    Combo_ClearStartupEntrance();
    if (Combo_GetStartupEntrance() != 0) {
        printf("[TEST] FAIL: Startup entrance not cleared\n");
        return TEST_FAIL;
    }

    // ------------------------------------------------------------------
    // Step 5: Game affinity — the exact condition behind the Market
    // cutscene crash. An entrance tagged for one game must be invisible to
    // the other, so a cross-game value (MM Clock Tower 0xC010) can never be
    // applied to OoT's entranceIndex and index gEntranceTable out of bounds.
    // ------------------------------------------------------------------
    Entrance_SetStartupEntrance(MM_ENTR_CLOCK_TOWER_INTERIOR_1, GAME_MM);

    // MM (the tagged game) sees it...
    if (!Entrance_HasStartupEntranceForGame(GAME_MM) ||
        Entrance_GetStartupEntranceForGame(GAME_MM) != MM_ENTR_CLOCK_TOWER_INTERIOR_1) {
        printf("[TEST] FAIL: MM-tagged startup entrance not visible to MM\n");
        return TEST_FAIL;
    }
    // ...but OoT must NOT — this is precisely what prevents the OOB crash.
    if (Entrance_HasStartupEntranceForGame(GAME_OOT) ||
        Entrance_GetStartupEntranceForGame(GAME_OOT) != 0) {
        printf("[TEST] FAIL: MM-tagged startup entrance leaked to OoT (crash condition)\n");
        return TEST_FAIL;
    }
    // The C API used by the game code must agree with the C++ API.
    if (!Combo_HasStartupEntranceForGame("mm") || Combo_HasStartupEntranceForGame("oot") ||
        Combo_GetStartupEntranceForGame("oot") != 0 ||
        Combo_GetStartupEntranceForGame("mm") != MM_ENTR_CLOCK_TOWER_INTERIOR_1) {
        printf("[TEST] FAIL: Combo_*ForGame C API disagrees with affinity\n");
        return TEST_FAIL;
    }

    // Symmetric: an OoT-tagged entrance must be invisible to MM.
    Entrance_SetStartupEntrance(OOT_ENTR_MARKET_FROM_MASK_SHOP, GAME_OOT);
    if (!Entrance_HasStartupEntranceForGame(GAME_OOT) ||
        Entrance_HasStartupEntranceForGame(GAME_MM)) {
        printf("[TEST] FAIL: OoT-tagged startup entrance leaked to MM\n");
        return TEST_FAIL;
    }

    // Back-compat: the legacy 1-arg setter tags wildcard, visible to both.
    Entrance_SetStartupEntrance(OOT_ENTR_HAPPY_MASK_SHOP);
    if (!Entrance_HasStartupEntranceForGame(GAME_OOT) ||
        !Entrance_HasStartupEntranceForGame(GAME_MM)) {
        printf("[TEST] FAIL: wildcard (1-arg) startup entrance not visible to both games\n");
        return TEST_FAIL;
    }

    // Clearing resets the affinity tag too.
    Combo_ClearStartupEntrance();
    if (Entrance_HasStartupEntranceForGame(GAME_OOT) ||
        Entrance_HasStartupEntranceForGame(GAME_MM)) {
        printf("[TEST] FAIL: startup entrance affinity not cleared\n");
        return TEST_FAIL;
    }

    printf("[TEST] PASS: Startup entrance flow + game affinity verified\n");
    Entrance_ClearPendingSwitch();
    return TEST_PASS;
}

TestResult Test_VBAffinity(void) {
    printf("[TEST] vb-affinity: OoT VB hooks must not fire while MM is active\n");

    // 206 is the aliased ordinal behind the Market -> Clock Tower crash: MM's
    // VB_SETUP_TRANSITION and OoT's VB_PLAY_RAINBOW_BRIDGE_CS share it, so an
    // OoT hook could veto MM's transition setup and leave transitionCtx.init
    // NULL (called at games/mm/src/code/z_play.c TRANS_MODE_INSTANCE_INIT).
    const int32_t kAliasedOrdinal = 206;

    GameId prevGame = Context_GetCurrentGame();
    GameInteractor_TestArmVBVeto(kAliasedOrdinal);

    // With OoT active, dispatch must reach OoT's hook registry: veto applies.
    Context_SetCurrentGame(GAME_OOT);
    bool vetoed = GameInteractor_Should(kAliasedOrdinal, 1);

    // With MM active, the affinity gate must return the vanilla default before
    // any OoT hook runs.
    Context_SetCurrentGame(GAME_MM);
    bool mmTrueDefault = GameInteractor_Should(kAliasedOrdinal, 1);
    bool mmFalseDefault = GameInteractor_Should(kAliasedOrdinal, 0);

    GameInteractor_TestDisarmVBVeto();
    Context_SetCurrentGame(prevGame);

    if (vetoed != false) {
        printf("[TEST] FAIL: OoT-active dispatch ignored the armed veto hook\n");
        return TEST_FAIL;
    }
    if (mmTrueDefault != true || mmFalseDefault != false) {
        printf("[TEST] FAIL: MM-active call did not get vanilla behavior (got %d/%d, want 1/0)\n",
               (int)mmTrueDefault, (int)mmFalseDefault);
        return TEST_FAIL;
    }

    printf("[TEST] PASS: MM-active VB calls stay vanilla; OoT-active hooks still fire\n");
    return TEST_PASS;
}

TestResult Test_RoundtripIntegrity(void) {
    printf("[TEST] roundtrip-integrity: OoT SaveContext byte-integrity across roundtrip (issue #262)\n");
    int failures = TestRoundtripIntegrity_Run();
    return (failures == 0) ? TEST_PASS : TEST_FAIL;
}

TestResult Test_HotSwapFreeze(void) {
    printf("[TEST] hotswap-freeze: F10 freeze + single-use frozen state (issue #364)\n");
    int failures = TestHotSwapFreeze_Run();
    return (failures == 0) ? TEST_PASS : TEST_FAIL;
}

TestResult Test_Lifecycle(void) {
    printf("[TEST] lifecycle: Game lifecycle unit tests\n");
    int failures = TestLifecycle_RunAll();
    return (failures == 0) ? TEST_PASS : TEST_FAIL;
}

TestResult Test_Context(void) {
    printf("[TEST] context: Test context/state management\n");

    Context_InitFrozenStates();
    // Clear any state from previous tests
    Context_ClearAllFrozenStates();

    // Test that no frozen state exists initially
    if (Context_HasFrozenState(GAME_OOT)) {
        printf("[TEST] FAIL: OoT should not have frozen state initially\n");
        return TEST_FAIL;
    }

    // Freeze a state (static: too large for the stack at the full blob capacity)
    static uint8_t testData[OOT_SAVE_CONTEXT_SIZE];
    memset(testData, 0, sizeof(testData));
    testData[100] = 0x42;
    Context_FreezeState(GAME_OOT, 0x1234, testData, sizeof(testData));

    // Verify frozen state exists
    if (!Context_HasFrozenState(GAME_OOT)) {
        printf("[TEST] FAIL: OoT should have frozen state after freeze\n");
        return TEST_FAIL;
    }

    // Verify return entrance
    if (Context_GetFrozenReturnEntrance(GAME_OOT) != 0x1234) {
        printf("[TEST] FAIL: Return entrance mismatch\n");
        return TEST_FAIL;
    }

    // Restore and verify
    static uint8_t restored[OOT_SAVE_CONTEXT_SIZE];
    memset(restored, 0, sizeof(restored));
    if (!Context_RestoreState(GAME_OOT, restored, sizeof(restored))) {
        printf("[TEST] FAIL: Restore failed\n");
        return TEST_FAIL;
    }

    if (restored[100] != 0x42) {
        printf("[TEST] FAIL: Restored data mismatch\n");
        return TEST_FAIL;
    }

    // ComboContext_Init stamps the magic + zero-inits the cross-game fields.
    // (Ported from the removed combo/tests/context_test.cpp in T10 #265 so the
    // one assertion not already covered by the live CTest suite isn't lost.)
    ComboContext_Init();
    if (strncmp(gComboCtx.magic, COMBO_CONTEXT_MAGIC, 8) != 0) {
        printf("[TEST] FAIL: ComboContext magic not initialized\n");
        return TEST_FAIL;
    }
    if (ComboContext_IsSwitchPending() || gComboCtx.targetGame != GAME_NONE) {
        printf("[TEST] FAIL: ComboContext not in a clean post-init state\n");
        return TEST_FAIL;
    }

    printf("[TEST] PASS: Context management working correctly\n");
    return TEST_PASS;
}

// ============================================================================
// Test registry
// ============================================================================

const TestDescriptor gTests[] = {
    {"rando-gen", "Seed generation succeeds with default settings (#337)", Test_RandoGen},
    // #441: no generated hint may resolve to the no-item sentinel. Needs a
    // display like rando-gen, so `--test all` skips it (below).
    {"rando-hint-validity", "Every generated hint names a real item (#441)", Test_RandoHintValidity},
    // Lane B unified seed: producer stamps gComboCtx at generation time; the
    // two-process same-seed determinism diff runs via the SeedDeterminism row.
    // Needs a display like rando-gen, so `--test all` skips it (below).
    {"rando-determinism", "Unified-seed producer fires + same-seed fill is reproducible (Lane B)",
     Test_RandoDeterminism},
    // Lane C0: MM's randomizer is reachable and generates headlessly. Needs a
    // display like rando-gen, so `--test all` skips it (below).
    {"mm-rando-gen", "MM rando generation runs headlessly + writes tagged spoiler (Lane C0)", Test_MMRandoGen},
    // #439: the paired world must activate on the SWITCH-ENTRY path (the only
    // flow a player actually takes), not just the direct OnSaveInit chain.
    // Same display requirement as mm-rando-gen, so `--test all` skips it.
    {"mm-pair-switch-entry", "Paired MM world activates via switch-entry; existing saves untouched (#439)",
     Test_MMPairSwitchEntry},
    {"boot-oot", "Shared-context bring-up leaves no null subsystems (#329)", Test_BootOoT},
    {"boot-mm", "MM-first bring-up prerequisites on the shared context (#330)", Test_BootMM},
    {"switch-oot-mm", "Test game switch OoT -> MM", Test_SwitchOoTMM},
    {"switch-mm-oot", "Test game switch MM -> OoT", Test_SwitchMMOoT},
    {"midos-house", "Test Mido's House entrance (test mode)", Test_MidosHouse},
    {"entrance-dedup", "Duplicate entrance links rejected; each portal face routes home (#374)", Test_EntranceDedup},
    {"startup-entrance", "Test startup entrance flow", Test_StartupEntrance},
    {"vb-affinity", "OoT VB hooks stay quiet while MM is active", Test_VBAffinity},
    {"cosmetic-gfx-stub", "MM HUD gfx wrappers write commands and advance the display list", Test_CosmeticGfxStub},
    {"roundtrip", "Full round-trip with state verification", Test_Roundtrip},
    {"roundtrip-integrity", "OoT SaveContext byte-identical across OoT->MM->OoT (issue #262)", Test_RoundtripIntegrity},
    {"shared-roundtrip", "Shared flag/seed survive OoT->MM switch (issue #264)", Test_SharedStateRoundtrip},
    {"shared-item-roundtrip", "Origin-tagged item survives suspend->switch->resume x2, both dirs (ADR 0002)",
     Test_SharedItemRoundtrip},
    // Lane C1: the foreign-item give path tags the shared structure, the
    // crossing awards exactly once, and the placement carve serializes.
    {"foreign-item-give", "Foreign give path tags shared structure; awards once; placements serialize (Lane C1)",
     Test_ForeignItemGive},
    {"context", "Test context/state management", Test_Context},
    // Clears all frozen states on entry and exit, so it must not run between a
    // test that freezes and one that expects that freeze to still be there.
    {"hotswap-freeze", "F10 hot swap freezes the departing game; frozen state is single-use (#364)",
     Test_HotSwapFreeze},
    {"lifecycle", "Game lifecycle unit tests", Test_Lifecycle},
    // Unified save (.redsave) headless coverage (issue #35, Phase 2 T6).
    {"save-roundtrip-tiers", "Unified .redsave preserves ComboContext + both SaveContexts (#35)", Test_SaveRoundtripTiers},
    {"save-header", "Unified .redsave header fields + CRC are well-formed (#35)", Test_SaveHeader},
    {"save-has-delete", "Unified save HasSave/DeleteSave lifecycle (#35)", Test_SaveHasDelete},
    {"save-version-reject", "Unified save Load rejects unknown version, no clobber (#35)", Test_SaveVersionReject},
    {"save-size-mismatch", "Unified save Load rejects oversized tier, no clobber (#35)", Test_SaveSizeMismatch},
    {"save-legacy-size", "Unified save Load zero-extends shorter legacy tiers (#35)", Test_SaveLegacySize},
    {"save-crc-corrupt", "Unified save Load rejects corrupt payload, no clobber (#35)", Test_SaveCrcCorrupt},
    // Tier-1 format-headroom locks: the migration path Lane A's widened
    // sharedItems rides on. Without these, "ComboContext can grow" is a claim.
    {"save-combo-legacy-record", "Pre-headroom Tier-1 still loads and zero-extends", Test_SaveComboLegacyRecord},
    {"save-combo-record-fixed", "Tier-1 written at a fixed padded size; headroom round-trips", Test_SaveComboRecordFixed},
    {"save-combo-oversize", "Load rejects an oversized Tier-1 record, no clobber", Test_SaveComboOversize},
    {"save-tagged-items", "Origin-tagged shared items round-trip; empty slots stay unset (ADR 0002)",
     Test_SaveTaggedItems},
    {"mm-scene-parse", "MM scene commands parse via the S2H factory (#344)", Test_MMSceneParse},
    {"seq-map-bounds", "Sequence-map capacity covers the id range + custom slack (#371, #378)", Test_SeqMapBounds},
    {"active-queue", "__osGetActiveQueue returns a walkable list, not a return register (#385)", Test_ActiveQueue},
    {"oot-audio-init-guard", "OoT synth no-ops while gAudioContextInitalized == false (#365)", Test_OoTAudioInitGuard},
    {"gp-watchdog", "Gameplay round-trip watchdog is wall-clock budgeted, can fire before the timeout (#376)",
     Test_GpWatchdog},
    {"mm-scene-execute", "MM scene commands execute against a PlayState (#344)", Test_MMSceneExecute},
    {"mm-culling-binding", "MM's Ship_ExtendedCulling* bind MM's Actor, not OoT's (#382)", Test_MMCullingBinding},
    {"mm-gi-shim", "MM hook registration goes through the MM-owned shim, not the shared 4-byte instance (#395)",
     Test_MMGIShim},
    {"mm-notification-binding", "MM Notification::Emit binds OoT's only while Options stays layout-identical (#427)",
     Test_MMNotificationBinding},
    {"mm-fb-effects-binding", "MM's scaled framebuffer draw binds its own body against MM's dimensions (#386)",
     Test_MMFbEffectsBinding},
    // The two MM resume-contract tests below mutate process-global state
    // (mm-resume-arena re-inits the MM system arena + heaps; mm-startup-restore
    // scribbles and re-zeroes the unified gSaveContext). Both clean up after
    // themselves, but a future test that depends on live MM arena contents or
    // a pre-populated gSaveContext must run BEFORE them.
    {"mm-resume-arena", "MM resume re-arms an exhausted system arena for the cold boot chain", Test_MMResumeArena},
    {"mm-startup-restore", "MM startup-entrance consumption restores the frozen save post-wipe", Test_MMStartupRestore},
    // Keep archive-hotswap-logic LAST: it re-inits the entrance table, so it
    // must not run before any test that relies on the default links.
    {"archive-hotswap-logic", "Headless multi-switch archive/state regression (#263)", Test_ArchiveHotswapLogic},
    {nullptr, nullptr, nullptr}  // Sentinel
};

// Integration tests that require actually booting the game
struct IntegrationTestDescriptor {
    const char* name;
    const char* description;
    IntegrationTestMode mode;
    GameId targetGame;
};

const IntegrationTestDescriptor gIntegrationTests[] = {
    {"int-boot-oot", "Boot OoT and verify title screen (integration)", INT_TEST_BOOT_OOT, GAME_OOT},
    {"int-boot-mm", "Boot MM and verify title screen (integration)", INT_TEST_BOOT_MM, GAME_MM},
    {"int-switch-oot-hms-to-mm",
     "Boot OoT, trigger Happy Mask Shop entrance (0x0530), verify spawn at MM South Clock Town tower exit (0xD800)",
     INT_TEST_SWITCH_OOT_HMS_TO_MM, GAME_OOT},
    {"int-switch-mm-clocktown-south-to-oot",
     "Boot MM, trigger the Clock Tower door (0xC010), verify spawn at OoT Market from Mask Shop (0x01D1)",
     INT_TEST_SWITCH_MM_CLOCKTOWN_SOUTH_TO_OOT, GAME_MM},
    {"int-archive-hotswap-cycle",
     "Boot OoT, hot-swap OoT<->MM >=3 times, verify healthy runtime (no missing assets, bounded RSS) (#263)",
     INT_TEST_ARCHIVE_HOTSWAP_CYCLE, GAME_OOT},
    {"int-gameplay-roundtrip",
     "Operator crash repro: debug save, live gameplay, production OoT<->MM round trip (freeze/restore + "
     "resume leg), post-return debug warp, door transition. Env: RSBS_GP_FRAMES/CYCLES/BOOT|WARP|EXIT_ENTRANCE",
     INT_TEST_GAMEPLAY_ROUNDTRIP, GAME_OOT},
    {nullptr, nullptr, INT_TEST_NONE, GAME_NONE}  // Sentinel
};

TestResult RunSingleTest(const char* name) {
    for (int i = 0; gTests[i].name != nullptr; i++) {
        if (strcmp(gTests[i].name, name) == 0) {
            return gTests[i].runFunc();
        }
    }
    printf("[TEST] ERROR: Unknown test '%s'\n", name);
    return TEST_ERROR;
}

} // anonymous namespace

// ============================================================================
// Public API
// ============================================================================

extern "C" {

int TestRunner_Run(const char* testName) {
    sTestMode = true;

    printf("=== RedShip Test Runner ===\n\n");

    if (strcmp(testName, "list") == 0) {
        TestRunner_ListTests();
        return 0;
    }

    if (strcmp(testName, "all") == 0) {
        int failures = 0;
        int passed = 0;
        int total = 0;

        for (int i = 0; gTests[i].name != nullptr; i++) {
            // rando-gen / rando-determinism need a display (Fast3dWindow
            // bring-up) and the RSBS_DISABLE_OTR_INIT environment; they run as
            // their own CTests ("rando" label, under xvfb-run) rather than in
            // this display-free suite, where they would hang the 60s timeout.
            if (strcmp(gTests[i].name, "rando-gen") == 0 || strcmp(gTests[i].name, "rando-determinism") == 0 ||
                strcmp(gTests[i].name, "rando-hint-validity") == 0 ||
                strcmp(gTests[i].name, "mm-rando-gen") == 0 ||
                strcmp(gTests[i].name, "mm-pair-switch-entry") == 0) {
                printf("\n--- Skipping: %s (needs display; runs as a rando-label CTest) ---\n", gTests[i].name);
                continue;
            }
            printf("\n--- Running: %s ---\n", gTests[i].name);
            TestResult result = gTests[i].runFunc();
            total++;

            if (result == TEST_PASS) {
                passed++;
            } else if (result == TEST_FAIL || result == TEST_ERROR) {
                failures++;
            }
        }

        printf("\n=== Test Summary ===\n");
        printf("Total: %d, Passed: %d, Failed: %d\n", total, passed, failures);

        return failures;
    }

    // Run single test
    TestResult result = RunSingleTest(testName);
    return (result == TEST_PASS) ? 0 : 1;
}

void TestRunner_ListTests(void) {
    printf("Available unit tests (--test <name>):\n\n");
    for (int i = 0; gTests[i].name != nullptr; i++) {
        printf("  %-20s %s\n", gTests[i].name, gTests[i].description);
    }
    printf("\nSpecial commands:\n");
    printf("  %-20s Run all unit tests\n", "all");
    printf("  %-20s Show this list\n", "list");

    printf("\nIntegration tests (--integration-test <name>):\n");
    printf("  (These tests actually boot the game - requires display/Xvfb)\n\n");
    for (int i = 0; gIntegrationTests[i].name != nullptr; i++) {
        printf("  %-20s %s\n",
               gIntegrationTests[i].name,
               gIntegrationTests[i].description);
    }
}

const char* TestRunner_ParseArgs(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0 && i + 1 < argc) {
            return argv[i + 1];
        }
        // Also support --test=name syntax
        if (strncmp(argv[i], "--test=", 7) == 0) {
            return argv[i] + 7;
        }
    }
    return nullptr;
}

void TestRunner_SignalBootComplete(GameId game) {
    if (game == sTargetGame) {
        sBootComplete = true;
        printf("[TEST] Boot complete signaled for %s\n", Game_ToString(game));

        // Also signal through integration test hooks if active
        if (sIntegrationTestMode) {
            IntegrationTest_SignalBootComplete(game, "TestRunner_SignalBootComplete");
        }
    }
}

bool TestRunner_IsTestMode(void) {
    return sTestMode;
}

GameId TestRunner_GetTargetGame(void) {
    return sTargetGame;
}

// ============================================================================
// Integration Test API
// ============================================================================

const char* TestRunner_ParseIntegrationArgs(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--integration-test") == 0 && i + 1 < argc) {
            return argv[i + 1];
        }
        // Also support --integration-test=name syntax
        if (strncmp(argv[i], "--integration-test=", 19) == 0) {
            return argv[i] + 19;
        }
    }
    return nullptr;
}

bool TestRunner_SetupIntegrationTest(const char* testName) {
    // Find the integration test
    for (int i = 0; gIntegrationTests[i].name != nullptr; i++) {
        if (strcmp(gIntegrationTests[i].name, testName) == 0) {
            sIntegrationTestMode = true;
            sTestMode = true;
            sTargetGame = gIntegrationTests[i].targetGame;
            sIntegrationTestName = testName;
            sBootComplete = false;

            // Set up integration test hooks
            IntegrationTest_SetMode(gIntegrationTests[i].mode);

            printf("[INT-TEST] Setting up integration test: %s\n", testName);
            printf("[INT-TEST] Target game: %s\n", Game_ToString(sTargetGame));

            return true;
        }
    }

    printf("[INT-TEST] ERROR: Unknown integration test '%s'\n", testName);
    TestRunner_ListTests();
    return false;
}

bool TestRunner_IsIntegrationTestMode(void) {
    return sIntegrationTestMode;
}

GameId TestRunner_GetIntegrationTestGame(void) {
    if (!sIntegrationTestMode) {
        return GAME_NONE;
    }
    return sTargetGame;
}

int TestRunner_GetIntegrationTestResult(void) {
    if (!sIntegrationTestMode) {
        return 1; // Error - not in integration test mode
    }

    bool passed = IntegrationTest_BootPassed();
    printf("\n=== Integration Test Result ===\n");
    printf("Test: %s\n", sIntegrationTestName);
    printf("Result: %s\n", passed ? "PASS" : "FAIL");

    return passed ? 0 : 1;
}

} // extern "C"
