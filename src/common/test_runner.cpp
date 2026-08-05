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
// For Test_ZipContention's headless factory registration (#560): both games
// register these only inside their display-bound Initialize paths.
#include <ship/resource/File.h>
#include <ship/resource/ResourceLoader.h>
#include <ship/resource/ResourceType.h>
#include <ship/resource/factory/BlobFactory.h>
#include <fast/resource/ResourceType.h>
#include <fast/resource/factory/TextureFactory.h>
// For Test_CrossGameModel (#577): the model pipeline's factories, registered
// by both games only inside their display-bound Initialize paths.
#include <fast/resource/factory/DisplayListFactory.h>
#include <fast/resource/factory/VertexFactory.h>
#include <fast/resource/type/DisplayList.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>

// Shared-context bring-up entries for the boot regression tests (#329/#330).
// Defined in games/oot/soh/OTRGlobals.cpp and
// games/mm/2s2h/GameExports_SingleExe.cpp; resolved at final link like the
// ArchiveHotswap_* helpers.
extern "C" {
int OoT_InitSharedContextSubsystems(void);
int MM_RegisterResourceFactoriesHeadless(void);
// Model-pipeline factories for the #577 cross-game model row (Texture,
// DisplayList, Vertex, and the game-owned 'OARR' Array reader that extracted
// vertex data needs). Defined in games/oot/soh/OTRGlobals.cpp — OoT's own TU,
// so the row runs against the factory surface a real OoT session has.
int OoT_RegisterModelResourceFactoriesHeadless(void);
// MM scene-command EXECUTE regression (#344). Body lives in an MM TU
// (games/mm/2s2h/mm_scene_execute_test.cpp) so MM's global.h / PlayState never
// enters this translation unit; called through this C entry point, mirroring
// MM_RegisterResourceFactoriesHeadless. Returns 0 on pass, non-zero on fail.
int MM_SceneExecute_RunHeadless(void);
// #539 — the CVar-change driver for MM's S2H::ShipInit map. The body lives in
// an OoT TU (games/oot/soh/soh_shipinit_driver_test.cpp) because it must call
// OoT's inline ShipInit::Init, the exact entry every SohMenu widget uses; its
// probes live in an MM TU (games/mm/2s2h/mm_shipinit_driver_test.cpp) because
// they must sit in MM's map. No TU can hold both games' ShipInit headers —
// which is the same seam that let the bug exist. Returns 0 on pass.
int OoT_ShipInitMMDriver_RunHeadless(void);
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
//     the startup entrance with cutscene/game-mode state reset, and re-derive
//     the sound mode from the restored options (sSoundMode lives outside
//     gSaveContext, so the restore memcpy cannot carry it — #483).
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
// MM notification bridge lock (games/mm/2s2h/mm_notification_binding_test.cpp,
// #427 item 1): MM's 2s2h/BenGui/Notification.cpp is excluded from single-exe
// builds, so MM's toasts render on OoT's overlay. They used to get there by ABI
// coincidence — MM's Notification::Emit call bound OoT's identically-mangled
// body and handed it MM's own view of Options, with no link error possible when
// the two structs drift (one Emit definition survives). This drives the
// explicit bridge that replaced it from MM's side, reads the toast back out of
// OoT's store field by field, and still compares the two ports' Options
// layouts — both trees declare the type, so their implicit ctor/dtor
// COMDAT-fold even now that the struct no longer crosses. Returns 0 on pass,
// non-zero on fail.
int MM_NotificationBinding_RunHeadless(void);
// MM scaled framebuffer-draw binding (games/mm/2s2h/mm_fb_effects_test.cpp,
// #386): MM's FB_DrawFromFramebufferScaled used to bind OoT's surviving body,
// which reads OoT_gScreenWidth/Height — the wrong dimensions for MM, which go
// to HiRes 576 while the Bombers' Notebook is open. Returns 0 on pass, non-zero
// on fail.
int MM_FbEffectsBinding_RunHeadless(void);
// MM flash page-table OOB lock (games/mm/2s2h/mm_flash_filenum_test.cpp): a
// cross-game MM session runs with gSaveContext.fileNum == 0xFF, the "no real
// slot" sentinel. The flash paths index the fixed-size gFlashSave*Pages /
// gFlashOwlSave*Pages tables by fileNum * FLASH_SAVE_MAIN_MULTIPLIER, so 0xFF
// runs hundreds of entries past their end -- a wild flash page number the
// moon-crash reset then copies over the live save (the Fierce-Deity /
// all-Ocarina corruption). Returns 0 on pass, non-zero on fail.
int MM_FlashFileNumOob_RunHeadless(void);
int MM_UnifiedSaveCapture_RunHeadless(void);
// MM capture harvest gate (games/mm/2s2h/mm_capture_harvest_gate_test.cpp,
// #591): MM_Combo_CaptureSaveToUnifiedSlot harvested the shared-resource pool
// BEFORE RsbsSave_Save checked the #533/#568 write latch, so a refused capture
// still advanced gComboCtx.sharedResources and the RAM watermark table for a
// commit that never landed -- and apply ASSIGNS the consumable kinds, so the
// phantom pool was materialized verbatim on the far side of the next crossing.
// Returns 0 on pass, non-zero on fail.
int MM_CaptureHarvestGate_RunHeadless(void);
// MM single-exe hook dispatch (games/mm/2s2h/mm_hook_dispatch_test.cpp, #511 /
// #438): the COND_* macros park registrations in the MM-owned S2H::GameHooks
// registry, but ShouldActorInit / OnActorInit / OnActorDraw / OnOpenText
// dispatched through upstream names that reach OoT's gated wrappers or
// mm_stubs.c no-ops -- so 21 TUs' chest-model rewrites and 24 TUs' rando text
// overrides were registered and never run. Returns 0 on pass, non-zero on fail.
int MM_HookDispatch_RunHeadless(void);
// MM playtime seed (games/mm/2s2h/mm_playtime_seed_test.cpp, #513): with
// lastTimeLog unseeded (0), SavingEnhancements_AdvancePlaytime accrued now-0 --
// a full Unix epoch -- into the persisted filePlaytime. The fix treats the zero
// as the seed. Returns 0 on pass, non-zero on fail.
int MM_PlaytimeSeed_RunHeadless(void);
// MM registrar coverage (games/mm/2s2h/mm_registrar_coverage_test.cpp, #516):
// BenPort.cpp's exclusion elided InitOTR's whole registration list, so
// CustomItem / CustomMessage / RegisterSavingEnhancements / RegisterAutosave
// were absent from the binary. They are re-homed into MM_Rando_Init; this row
// drives that production entry point and asserts each one's registry actually
// filled. Complements the CI symbol allowlist, which can only prove the
// symbols LINKED -- and which cannot attribute RegisterAutosave at all, since
// OoT ships a static twin of that name. Returns 0 on pass, non-zero on fail.
int MM_RegistrarCoverage_RunHeadless(void);
// MM tracker registration surface (games/mm/2s2h/mm_trackers_gui_test.cpp,
// #392): the four MM tracker windows must register on a Gui under
// "MM "-prefixed names (SoH owns the unprefixed ones and Gui::AddGuiWindow
// rejects duplicates), and their Draw/Update path must be inert unless MM is
// the active game (unified gSaveContext storage — an ungated MM tracker
// would read OoT bytes through MM's layout). Needs the display-free shared
// bring-up first (ConsoleVariables/Config/Console). Returns 0 on pass,
// non-zero on fail.
int MM_TrackersGui_RunHeadless(void);
// src/common/tests/test_combo_spoiler_window.c — same bridge shape: the
// spoiler window's ctor reads ConsoleVariables off the Ship::Context
// singleton, so its body needs the display-free shared bring-up below.
int Combo_SpoilerWindow_RunHeadless(void);
// src/common/tests/test_combo_mm_options_window.c — the MM options pane's
// window lock; same bridge shape and the same reason (GuiWindow ctor reads
// ConsoleVariables off the Ship::Context singleton).
int Combo_MMOptionsWindow_RunHeadless(void);
// src/common/tests/test_combo_tracker_view.c — the combo tracker's per-game
// adapters (#458). Needs the shared bring-up because the OoT authoring seam
// constructs a real Rando::Context. test_combo_tracker_window.c is the
// window's lock — same bridge shape as the spoiler window above.
int Combo_TrackerView_RunHeadless(void);
int Combo_TrackerWindow_RunHeadless(void);
// games/mm/2s2h/mm_rando_options_test.cpp (#497 step 4, #499): the option TABLE
// and the paired PROFILE. Both bodies live in an MM TU because they drive
// Rando::StaticData::Options and Rando::Foreign::ResolvePairedProfile, which
// need MM's headers — this file must never acquire them. Return 0 on pass.
int MM_RandoOptions_RunHeadless(void);
int MM_PairedProfile_RunHeadless(void);
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

// Cross-game session invalidation on soft reset / new game (issue #440).
// Included at FILE SCOPE (compiled as C++): it drives the C++-linkage
// rsbs::SaveManager to prove a legitimate existing-slot load still restores,
// and the C++-linkage Entrance_* API to stage the arrival negative control.
#include "tests/test_session_invalidation.c"

// Archive hot-swap regression test (issue #263). Included at FILE SCOPE (not
// inside an extern "C" block): it is compiled as C++ and uses the C++-linkage
// Entrance_* API for setup. Its cross-TU ArchiveHotswap_* helpers are wrapped
// in their own extern "C" inside the file.
#include "tests/test_archive_hotswap.c"

// Unified save (.redsave) headless tests (issue #35, Phase 2 T6). Included at
// FILE SCOPE (compiled as C++): they drive the C++-linkage rsbs::SaveManager.
#include "tests/test_save_roundtrip.c"

// REFUSED-state locks (#533): refusal quarantines the evidence (renamed aside,
// reason-tagged, never overwritten), the refusing session latches the slot
// against writes, and ABSENT / VALID / REFUSED surface as three different
// facts. FILE SCOPE (compiled as C++) for rsbs::SaveManager.
#include "tests/test_save_refusal.c"

// The .redsave commit choke point (#537/#531): monotonic commit generation,
// the torn-write impossibility (stage on the game thread, write from the
// immutable snapshot only), and the load-time cross-artifact freshness
// comparison surfaced through the #533 machinery. FILE SCOPE (compiled as
// C++), same reason as above.
#include "tests/test_commit_generation.c"

// Lane C1 foreign-item pipeline locks (#392, ADR 0002): give-path tagging,
// round-trip survival with a real pool entry, and the foreignPlacements carve
// serialization. FILE SCOPE (compiled as C++) for rsbs::SaveManager; the
// pipeline symbols it drives are C-linkage and declared in the file.
#include "tests/test_foreign_items.c"
#include "tests/test_foreign_award.c"

// Lane C1 in-game spoiler VIEW model (#496): the read-only projection of
// gComboCtx.foreignPlacements the combo spoiler window renders — named
// crossings in slot order, their collected state, and "not paired" reported
// distinctly from "no crossings". FILE SCOPE (compiled as C++) for
// rsbs::SaveManager, like test_foreign_items.c above.
#include "tests/test_combo_spoiler_view.c"

// The window that renders that model (#496 steps 3-4, ADR 0008): registration,
// idempotence, name de-collision, and a hard tripwire that its draw path stays
// out of ImGui under GAME_OOT/GAME_MM/GAME_NONE alike. FILE SCOPE — it drives
// the C++-linkage ComboGui::RegisterComboSpoilerWindow.
#include "tests/test_combo_spoiler_window.c"

// The MM randomizer options pane's window (#497 step 4, ADR 0004 + 0008): same
// registration/idempotence/de-collision shape, plus a tripwire that a pane which
// deliberately READS the active game still never reads that game's save. FILE
// SCOPE — it drives the C++-linkage ComboGui::RegisterComboMmOptionsWindow.
#include "tests/test_combo_mm_options_window.c"

// Combo tracker (#458): the per-game adapters over an authored MM shadow blob
// + an authored OoT heap context, and the window's ADR 0008 inertness
// tripwire. FILE SCOPE (compiled as C++) — the view lock uses std::vector for
// the authored blob and the window lock drives the C++-linkage
// ComboGui::RegisterComboTrackerWindow.
#include "tests/test_combo_tracker_view.c"
#include "tests/test_combo_tracker_window.c"

// Sourced-grant model locks (ADR 0005, netplay 1a #460): per-source cursor
// idempotency, switch-free received-order redemption, loud overflow with
// redeemed-slot reclamation, and .redsave durability including the
// pre-netplay migration path and the #440-composing reset atomicity. FILE
// SCOPE (compiled as C++) for rsbs::SaveManager; the grant API is C-linkage
// via shared_items.h.
#include "tests/test_grant_sources.c"

// Shared cross-game RESOURCE locks (#525): the delta-harvest watermark that
// survives the 500-vs-999 wallet mismatch, the monotonic/consumable split, the
// first-harvest seed that stops a .redsave load from doubling the player's
// money, and the canonical heart quantity with its 20-heart clamp. FILE SCOPE
// (compiled as C++) like the file above; every symbol it drives is C-linkage
// via shared_resources.h.
#include "tests/test_shared_resources.c"

// Netplay grant-relay loopback locks (ADR 0007, #460). Guarded: with
// RSBS_NETPLAY=OFF (the default) the relay sources are not in the build at all,
// so the code under test does not exist. The `#include "tests/..."` text is
// still present for redship_check_test_sources(), which globs the directory and
// greps this file — the guard hides the code from the compiler, not the file
// from the completeness check.
//
// Inside its own extern "C" block: every symbol it drives (Relay_*,
// RelayProto_*, Combo_*) is C-linkage.
#ifdef RSBS_NETPLAY
extern "C" {
#include "tests/test_netplay_relay.c"
}
#endif

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

// Cross-game CVar classification lock (#34, ADR 0003 §6.5 + the
// enhancement-classification inventory). Manifest self-consistency, the
// migrator's pure decision rules, and a source scan over games/ that catches
// drift in BOTH directions — a converged key diverging again, and a per-game
// key being merged because the names looked equivalent.
#include "tests/test_cvar_classification.c"

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

// #560 root-cause contention lock over the shared per-archive handle. FILE
// SCOPE (compiled as C++): it drives the C++-linkage Ship::Context /
// ResourceManager / ArchiveManager APIs directly. The wrapper
// (Test_ZipContention below) performs the display-free shared bring-up and the
// soh.o2r resolvability check (#562) before this body spins its loader
// threads.
#include "tests/test_zip_contention.c"

// Curated-archive mount-order lock (#595) and the mod-survives-a-switch lock
// (#593). FILE SCOPE (compiled as C++): both drive the C++-linkage
// Ship::Context / ArchiveManager APIs directly, and the #593 body calls the
// production Combo_EnsureGameArchivesLoaded defined in rsbs/src/main.cpp. The
// wrappers below resolve the staged archives and SKIP when they are absent.
#include "tests/test_curated_archive_order.c"

// #577 cross-game model resolution lock. FILE SCOPE (compiled as C++): it walks
// a parsed Fast::DisplayList and drives the C++-linkage ResourceManager /
// ArchiveManager. The wrapper (Test_CrossGameModel below) performs the
// display-free OoT bring-up, mounts the curated cross-game archive, and
// registers the DisplayList/Vertex/Texture factories first.
#include "tests/test_crossgame_model.c"

// MM scene-command EXECUTE regression (issue #344). Unlike the parse test, the
// body runs the parsed commands against a PlayState, so it needs MM's global.h
// — which lives in an MM TU (games/mm/2s2h/mm_scene_execute_test.cpp) to keep
// MM's umbrella headers out of this TU. Thin wrapper over the C entry point.
static TestResult Test_MMSceneExecute(void) {
    return MM_SceneExecute_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// #539: a CVar change on the unified menu path must re-arm MM's registrars,
// not only OoT's. Needs no Ship::Context — it touches the two registrar maps
// and nothing else, and it drives only a synthetic key plus the two pseudo-
// paths, so no production registrar with live side effects runs here.
static TestResult Test_MMShipInitDriver(void) {
    return OoT_ShipInitMMDriver_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
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

// MM notification bridge lock (see the extern decl above). Thin wrapper over
// the C entry point in games/mm/2s2h/mm_notification_binding_test.cpp.
static TestResult Test_MMNotificationBinding(void) {
    return MM_NotificationBinding_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// MM scaled framebuffer-draw binding lock (see the extern decl above). Thin
// wrapper over the C entry point in games/mm/2s2h/mm_fb_effects_test.cpp.
static TestResult Test_MMFbEffectsBinding(void) {
    return MM_FbEffectsBinding_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// MM flash page-table OOB lock (see the extern decl above). Thin wrapper over
// the C entry point in games/mm/2s2h/mm_flash_filenum_test.cpp.
static TestResult Test_MMFlashFileNumOob(void) {
    return MM_FlashFileNumOob_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

static TestResult Test_MMUnifiedSaveCapture(void) {
    return MM_UnifiedSaveCapture_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// MM capture harvest gate (#591; see the extern decl above). Thin wrapper over
// the C entry point in games/mm/2s2h/mm_capture_harvest_gate_test.cpp.
static TestResult Test_MMCaptureHarvestGate(void) {
    return MM_CaptureHarvestGate_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// MM hook-dispatch lock (see the extern decl above). Thin wrapper over the C
// entry point in games/mm/2s2h/mm_hook_dispatch_test.cpp.
static TestResult Test_MMHookDispatch(void) {
    return MM_HookDispatch_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// MM playtime-seed lock (see the extern decl above). Thin wrapper over the C
// entry point in games/mm/2s2h/mm_playtime_seed_test.cpp.
static TestResult Test_MMPlaytimeSeed(void) {
    return MM_PlaytimeSeed_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
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
// Full-bring-up generation lock (#560 "Why CI never saw it"; body in
// games/oot/soh/Enhancements/randomizer/randomizer.cpp). Driven by the ONE rando
// row that does not set RSBS_DISABLE_OTR_INIT=1: it first proves the normally
// masked init block actually ran (populated ExtensionCache + vanilla item table +
// registered debug console — the anti-vacuity guard), then generates through the
// THREADED entry a player's file select takes rather than the synchronous
// overload every other row uses.
extern "C" int Rando_HeadlessFullInitSeedTest(const char* seedStr);
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
// Hint-RELOAD lock (#441, body in menu.cpp): generates one seed, then drives the
// REAL randomizer section through save -> Rando::Context reset -> reload (exactly
// as loading a saved file does) and asserts every enabled item hint still renders
// the same text and never the no-item sentinel. This is the runtime complement to
// the generation-time RandoHintValidity lock: #445's checks re-render hints
// against the live, fully-filled context, so they cannot see a placement table
// that fails to rehydrate on load.
extern "C" int Rando_HeadlessHintReloadTest(const char* seedStr);
// Cross-game arrival lock (#441, body in menu.cpp): THE operator-visible defect.
// A switch into OoT cold-boots the title chain, whose Title_Destroy ->
// OoT_Sram_InitSram -> Save_Init wiped the Rando::Context placement table while
// the frozen save is restored without re-running LoadRandomizer, so every item
// hint stranded on RG_NONE ("No Item") though its location phrasing survived.
// Drives the real Save_Init with and without the arrival flag.
extern "C" int Rando_HeadlessHintCrossGameTest(const char* seedStr);
extern "C" void InitOTRForMMFirstBoot(int argc, char* argv[]);
// Arrival-rehydration lock (#482), bodies in the two OoT tracker TUs
// (randomizer_item_tracker.cpp / randomizer_check_tracker.cpp). Each drives the
// REAL SaveManager initFunc that the arrival title-demo Save_InitFile(true)
// dispatches (ItemTrackerInitFile / InitTrackerData), with the OoT arrival flag
// set (the out-of-save tracker global must survive) and without it (the clear
// must still fire, so the pass is not vacuous). Both are pure -- no OTR/display
// -- so they run in the display-free suite. Each returns a failure count.
extern "C" int RandoTest_ItemTrackerArrivalLock(void);
extern "C" int RandoTest_CheckTrackerArrivalLock(void);

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

// #560 coverage hole. rando-gen above, and all 13 of its siblings, run under
// RSBS_DISABLE_OTR_INIT=1, so CI's only seed-generation coverage runs a bring-up
// with OTRAudio_Init / OTRExtScanner / VanillaItemTable_Init / DebugConsole_Init
// all skipped (OTRGlobals.cpp:1811-1823), and drives the synchronous 3-arg
// GenerateRandomizer inline. This entry is the same generation with none of that
// masked and with the fill on a worker thread, which is what a player gets. It
// asserts the un-masking BEFORE generating so the row cannot pass vacuously if
// the env flag is ever re-added to it. Needs a display (Fast3dWindow) like
// rando-gen, so `--test all` skips it.
TestResult Test_RandoGenFullInit(void) {
    printf("[TEST] rando-gen-full-init: seed generation with the full OTR bring-up live, on a worker thread "
           "(#560)\n");

    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }

    // Hard precondition, checked before bring-up because the failure mode is a
    // 300-second HANG rather than a crash. With no archive loaded libultraship
    // pauses the ResourceManager thread pool permanently (libultraship
    // ResourceManager.cpp:58-61, "Nothing ever unpauses the thread pool"), and
    // OTRAudio_Init inside the un-masked block does a synchronous
    // ResourceMgr_LoadDirectory("audio") whose .get() then never returns. That is
    // the real, previously-undocumented reason this tier carries
    // RSBS_DISABLE_OTR_INIT=1 — and it is why CI's rando rows had been running
    // with zero archives mounted for as long as the tier has existed (they were
    // staged where only the install rules look; #560 fixes that in BOTH places
    // the archive can come from — copy-existing-otrs.cmake / the Generate*Otr
    // targets for a locally built archive, and a staging step in each workflow
    // for the CI case where it arrives as a downloaded artifact instead).
    // Resolve exactly the way the bring-up will (OTRGlobals.cpp:
    // LocateFileAcrossAppDirs("soh.o2r")) and fail in a second with a readable
    // reason instead of burning the row's timeout. Deliberately a FAIL and not a
    // CTest SKIP: a staging regression must be loud, since the symptom it
    // otherwise produces is a silently masked subsystem.
    const std::string sohArchive = Ship::Context::LocateFileAcrossAppDirs("soh.o2r");
    if (!std::filesystem::exists(sohArchive)) {
        printf("[TEST] FAIL: no soh.o2r resolvable (tried '%s'). This row needs a mounted archive: with none, "
               "libultraship pauses the resource thread pool and OTRAudio_Init's synchronous load never "
               "returns. Build the port archives once ('cmake --build <dir> --target GenerateSohOtr "
               "Generate2ShipOtr'), which now also lands them in the build root where the ctest rows look "
               "(#560).\n",
               sohArchive.c_str());
        return TEST_FAIL;
    }

    static char arg0[] = "redship";
    static char* fakeArgv[] = { arg0, nullptr };
    InitOTRForMMFirstBoot(1, fakeArgv);

    int rc = Rando_HeadlessFullInitSeedTest("RSBSFULL1");
    printf("[TEST] %s: full-init seed generation rc=%d\n", rc == 0 ? "PASS" : "FAIL", rc);
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

// Hint-RELOAD lock (#441). The operator saw gossip stones read "catching Big
// Poes leads to No Item" while the spoiler named a real item. The hint's
// location survives a load (it is stored by name), but the item name is resolved
// at RUNTIME from the placement table, which is thrown away and rebuilt from the
// save on every load. This drives the real save -> context-reset -> reload cycle
// and proves item hints still name their real item afterward. Needs a display
// (Fast3dWindow) like rando-gen, so `--test all` skips it.
TestResult Test_RandoHintReload(void) {
    printf("[TEST] rando-hint-reload: item hints survive a save/reload cycle (#441)\n");

    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }

    static char arg0[] = "redship";
    static char* fakeArgv[] = { arg0, nullptr };
    InitOTRForMMFirstBoot(1, fakeArgv);

    int rc = Rando_HeadlessHintReloadTest("RSBSHINT1");
    printf("[TEST] %s: hint reload rc=%d\n", rc == 0 ? "PASS" : "FAIL", rc);
    return rc == 0 ? TEST_PASS : TEST_FAIL;
}

// Cross-game arrival lock (#441). THE operator-visible defect: gossip stones read
// "catching Big Poes leads to No Item" after an OoT->MM->OoT switch. The switch
// into OoT cold-boots the title chain, whose Title_Destroy -> OoT_Sram_InitSram
// -> Save_Init wiped the Rando::Context placement table (ClearItemLocations),
// and the frozen save is restored afterwards without going through Save_LoadFile,
// so nothing re-ran LoadRandomizer to rehydrate it. Drives the real Save_Init
// with the arrival flag set (placements must survive) and without it (the clear
// must still fire). Needs a display like rando-gen, so `--test all` skips it.
TestResult Test_RandoHintCrossGame(void) {
    printf("[TEST] rando-hint-crossgame: item hints survive a cross-game OoT arrival (#441)\n");

    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }

    static char arg0[] = "redship";
    static char* fakeArgv[] = { arg0, nullptr };
    InitOTRForMMFirstBoot(1, fakeArgv);

    int rc = Rando_HeadlessHintCrossGameTest("RSBSHINT1");
    printf("[TEST] %s: hint cross-game rc=%d\n", rc == 0 ? "PASS" : "FAIL", rc);
    return rc == 0 ? TEST_PASS : TEST_FAIL;
}

// Arrival-rehydration lock (#482). Two more instances of the #441 class on the
// OoT side: the check tracker's `areasSpoiled` and the item tracker's typed
// `itemTrackerNotes` are blanked by SaveManager initFuncs that the arrival
// title-demo Save_InitFile(true) dispatches, but the frozen save is restored at
// Play_ConsumeStartupEntrance without re-running Save_LoadFile, so nothing
// rehydrates them -- they live OUTSIDE gSaveContext. Both are guarded with
// Combo_HasStartupEntranceForGame("oot"). This lock drives the real init
// functions with the arrival flag set (state must survive) and without it (the
// clear must still fire, proving the pass is non-vacuous). Pure (no display, no
// OTR, no game archive), so unlike the rando-hint locks it runs in the
// display-free suite and is NOT skipped by `--test all`.
TestResult Test_TrackerArrivalRehydration(void) {
    printf("[TEST] tracker-arrival-rehydration: check/item tracker state survives a cross-game OoT arrival (#482)\n");
    int failures = 0;
    failures += RandoTest_CheckTrackerArrivalLock();
    failures += RandoTest_ItemTrackerArrivalLock();
    printf("[TEST] %s: tracker arrival rehydration failures=%d\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? TEST_PASS : TEST_FAIL;
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

// ============================================================================
// #510: the REVERSE foreign pool, end to end through a real OoT generation.
//
// WHY THIS ROW IS IN THE `rando` TIER AND NOT THE DISPLAY-FREE ONE. OoT's host
// predicate reads GetPlacedRandomizerGet() — a FILL RESULT. In a ROM-free run
// with no fill every location is RG_NONE, so the predicate accepts nothing and a
// naive "only chests are accepted" assertion passes with an accepted count of
// ZERO: green, and testing nothing. That is the same vacuity trap #491 recorded
// for MM's deferred COND_HOOK unregister. The defence is structural — run a real
// generation first — plus an explicit assertion that the accepted count is
// NON-ZERO, and printing it so a supply regression is visible in CI logs before
// it becomes a shortfall.
//
// Bridges: OoT_Foreign_IsEligibleHost is the real predicate the placement pass
// uses; MM_ConsumeSharedItems -> MM_AwardSharedItem -> MM_ForeignItem_Give is
// MM's real, already-merged (#507) award chain.
// ============================================================================
extern "C" {
int OoT_Foreign_IsEligibleHost(uint16_t rc);
void MM_ConsumeSharedItems(void);
int MM_ForeignItem_TestPendingCount(void);
uint16_t MM_ForeignItem_TestPendingAt(int index);
void MM_ForeignItem_TestResetPending(void);
}

TestResult Test_ForeignPlacementOoT(void) {
    printf("[TEST] foreign-placement-oot: a real OoT generation hosts MM items, deterministically, and MM's award "
           "chain accepts them (#510)\n");

    auto shipCtx = CreateHarnessStyleContext();
    if (!shipCtx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }

    static char arg0[] = "redship";
    static char* fakeArgv[] = { arg0, nullptr };
    InitOTRForMMFirstBoot(1, fakeArgv);

    // A REAL fill. Everything below depends on this having run — without it the
    // predicate is vacuous and the placement table is empty.
    const char* kSeed = "RSBSFOREIGN1";
    int rc = Rando_HeadlessSeedTest(kSeed);
    if (rc != 0) {
        printf("[TEST] FAIL: seed generation rc=%d\n", rc);
        return TEST_FAIL;
    }

    // The producer's gate: generation stamps the pairing identity, so by now the
    // reverse placement pass has run inside Playthrough_Init.
    if (!Combo_ForeignPairingActive()) {
        printf("[TEST] FAIL: generation did not stamp the pairing identity\n");
        return TEST_FAIL;
    }

    // ------------------------------------------------------------------
    // Host supply: NON-ZERO, and printed.
    // ------------------------------------------------------------------
    int eligibleHosts = 0;
    for (int id = 1; id < 4200; id++) { // RC_MAX is ~4134; the predicate rejects out-of-range itself
        if (OoT_Foreign_IsEligibleHost((uint16_t)id)) {
            eligibleHosts++;
        }
    }
    printf("[TEST] foreign-placement-oot: %d eligible OoT host checks after a real fill\n", eligibleHosts);
    if (eligibleHosts <= 0) {
        printf("[TEST] FAIL: no eligible OoT host check — the predicate accepts nothing (vacuity)\n");
        return TEST_FAIL;
    }

    // ------------------------------------------------------------------
    // Placements: made, MM-tagged, named, and hosted on eligible checks.
    // ------------------------------------------------------------------
    const int placedCount = Combo_CountForeignPlacementsOoT();
    printf("[TEST] foreign-placement-oot: %d MM items hosted in OoT checks (cap %d)\n", placedCount,
           (int)RSBS_FOREIGN_PLACEMENT_CAP);
    if (placedCount <= 0) {
        printf("[TEST] FAIL: generation placed no MM items despite an active pairing\n");
        return TEST_FAIL;
    }
    if (placedCount > (int)RSBS_FOREIGN_PLACEMENT_CAP) {
        printf("[TEST] FAIL: placement count %d exceeds the carve\n", placedCount);
        return TEST_FAIL;
    }

    for (int slot = 0; slot < (int)RSBS_FOREIGN_PLACEMENT_CAP; slot++) {
        const ComboForeignPlacement& p = gComboCtx.foreignPlacementsOoT[slot];
        if (p.item.originGame == (uint8_t)GAME_NONE) {
            continue; // unset slot
        }
        if (p.item.originGame != (uint8_t)GAME_MM) {
            printf("[TEST] FAIL: OoT-hosted placement in slot %d is not MM-tagged (%u)\n", slot,
                   (unsigned)p.item.originGame);
            return TEST_FAIL;
        }
        // Resolvable through the pool: the spoiler and both presentation surfaces
        // read the name through exactly this call.
        if (Combo_GetForeignItemName(p.item) == nullptr) {
            printf("[TEST] FAIL: hosted MM item id=%u resolves to no pool entry\n", (unsigned)p.item.id);
            return TEST_FAIL;
        }
        // The host must still satisfy the predicate that selected it — it keeps
        // its own junk item, which is the degrade invariant (#488): with the
        // placement table absent the check just yields the junk it really holds.
        if (!OoT_Foreign_IsEligibleHost(p.mmCheckId)) {
            printf("[TEST] FAIL: OoT check %u hosts an MM item but is not an eligible host\n",
                   (unsigned)p.mmCheckId);
            return TEST_FAIL;
        }
    }

    // ------------------------------------------------------------------
    // MM's REAL award chain accepts a really-placed item, exactly once.
    // ------------------------------------------------------------------
    // The in-game pickup seam (hook_handlers.cpp) needs a live PlayState and is
    // gameplay-tier, so this drives the half CI can honestly reach: record the
    // crossing the way the seam does, then run MM's real consumer. With no
    // PlayState the give DEFERS into MM's pending queue (#502) rather than
    // dereferencing — so the queue is the observable that the id survived the
    // whole path in MM's id-space.
    const SharedItem firstPlaced = gComboCtx.foreignPlacementsOoT[0].item;
    Combo_ClearSharedItemOutbox();
    MM_ForeignItem_TestResetPending();
    if (Combo_RecordSharedItem(GAME_MM, firstPlaced.id) < 0) {
        printf("[TEST] FAIL: could not record the crossing\n");
        return TEST_FAIL;
    }
    MM_ConsumeSharedItems();
    if (MM_ForeignItem_TestPendingCount() != 1 || MM_ForeignItem_TestPendingAt(0) != firstPlaced.id) {
        printf("[TEST] FAIL: MM's award chain did not accept the placed item (pending=%d)\n",
               MM_ForeignItem_TestPendingCount());
        return TEST_FAIL;
    }
    // Single-use: a second arrival awards nothing more.
    MM_ForeignItem_TestResetPending();
    MM_ConsumeSharedItems();
    if (MM_ForeignItem_TestPendingCount() != 0) {
        printf("[TEST] FAIL: crossing awarded twice\n");
        return TEST_FAIL;
    }
    MM_ForeignItem_TestResetPending();

    // ------------------------------------------------------------------
    // DETERMINISM: the same seed reproduces the same placements exactly.
    // ------------------------------------------------------------------
    // The selection stream is a LOCAL xorshift32 seeded from the paired identity,
    // deliberately not drawn from the fill's RNG. If it ever started borrowing
    // Random_Init's stream — or iterating an unordered container — this is what
    // goes red, and it is the same property the SeedDeterminism digest folds.
    ComboForeignPlacement firstRun[RSBS_FOREIGN_PLACEMENT_CAP];
    memcpy(firstRun, gComboCtx.foreignPlacementsOoT, sizeof(firstRun));

    rc = Rando_HeadlessSeedTest(kSeed);
    if (rc != 0) {
        printf("[TEST] FAIL: second seed generation rc=%d\n", rc);
        return TEST_FAIL;
    }
    if (memcmp(firstRun, gComboCtx.foreignPlacementsOoT, sizeof(firstRun)) != 0) {
        printf("[TEST] FAIL: same seed produced different foreign placements (nondeterministic)\n");
        return TEST_FAIL;
    }

    Combo_ClearSharedItemOutbox();
    printf("[TEST] PASS: %d MM items hosted over %d eligible OoT checks, deterministic, MM awards each once\n",
           placedCount, eligibleHosts);
    return TEST_PASS;
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

// Attempt-ladder locks (ADR 0010 increment 1.2), bridge bodies in
// games/mm/2s2h/mm_rando_gen_test.cpp. The digest bridge generates the paired
// MM world for a pinned master seed KNOWN to dead-end its first ladder
// attempt, asserts convergence + the provenance record, and writes a world
// digest to RSBS_ATTEMPT_DIGEST_OUT; the MMPairedAttemptDeterminism CTest row
// (CMake/CheckPairedAttemptDeterminism.cmake) runs it twice in two processes
// and diffs. The exhaustion bridge drives a cannot-converge profile through
// the full switch-entry arrival and asserts the loud #533 refusal. Both need
// a display like mm-rando-gen, so `--test all` skips them.
extern "C" int MM_Rando_HeadlessPairedAttemptDigest(const char* outPath);
extern "C" int MM_Rando_HeadlessPairedExhaustion(void);

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

// Attempt-ladder convergence + determinism digest (ADR 0010 increment 1.2
// lock (b)). Single-run assertions here; the two-process byte-diff runs via
// the MMPairedAttemptDeterminism row (CMake/CheckPairedAttemptDeterminism
// .cmake), which invokes this same dispatch twice with distinct
// RSBS_ATTEMPT_DIGEST_OUT paths. Needs a display like mm-rando-gen, so
// `--test all` skips it.
TestResult Test_MMPairedAttempt(void) {
    printf("[TEST] mm-paired-attempt: pinned multi-attempt master seed converges deterministically on the ladder "
           "(ADR 0010 inc. 1.2)\n");

    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }

    static char arg0[] = "redship";
    static char* fakeArgv[] = { arg0, nullptr };
    InitOTRForMMFirstBoot(1, fakeArgv);

    const char* digestOut = std::getenv("RSBS_ATTEMPT_DIGEST_OUT"); // NULL => digest to stdout
    int rc = MM_Rando_HeadlessPairedAttemptDigest(digestOut);
    printf("[TEST] %s: attempt-ladder digest rc=%d\n", rc == 0 ? "PASS" : "FAIL", rc);
    return rc == 0 ? TEST_PASS : TEST_FAIL;
}

// Attempt-ladder exhaustion surfaces loudly (ADR 0010 increment 1.2 lock (c)):
// a cannot-converge profile must run the bounded ladder dry and refuse
// through the #533 machinery at the arrival — never a silent vanilla Termina.
// Needs a display like mm-rando-gen, so `--test all` skips it.
TestResult Test_MMPairedExhaustion(void) {
    printf("[TEST] mm-paired-exhaustion: an unconvergeable paired profile exhausts the ladder and refuses loudly "
           "(ADR 0010 inc. 1.2)\n");

    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }

    static char arg0[] = "redship";
    static char* fakeArgv[] = { arg0, nullptr };
    InitOTRForMMFirstBoot(1, fakeArgv);

    int rc = MM_Rando_HeadlessPairedExhaustion();
    printf("[TEST] %s: exhaustion surface rc=%d\n", rc == 0 ? "PASS" : "FAIL", rc);
    return rc == 0 ? TEST_PASS : TEST_FAIL;
}

// Non-arrival entry-path arm-state lock (#439 follow-up). MMPairSwitchEntry
// covers the cross-game arrival convergence; this row covers the OTHER paths
// into MM gameplay that the arrival fix does not touch — the file-select LOAD
// re-arm after the boot chain's disarm (the ordering the owl-save reload
// relies on) and the in-session reload (Song of Time / cycle reset / DayTelop)
// that must leave a live rando session's armed hooks alone. Reads arm state
// through S2H::GameHooks::CountForTest<OnFlagSet>. Bridge body in
// games/mm/2s2h/mm_rando_gen_test.cpp. Needs a display, same as mm-rando-gen,
// so `--test all` skips it.
extern "C" int MM_Rando_HeadlessReloadArmState(void);

TestResult Test_MMReloadArmState(void) {
    printf("[TEST] mm-reload-arm-state: IS_RANDO hooks match the save on the non-arrival entry paths (#439)\n");

    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }

    static char arg0[] = "redship";
    static char* fakeArgv[] = { arg0, nullptr };
    InitOTRForMMFirstBoot(1, fakeArgv);

    int rc = MM_Rando_HeadlessReloadArmState();
    printf("[TEST] %s: reload arm-state rc=%d\n", rc == 0 ? "PASS" : "FAIL", rc);
    return rc == 0 ? TEST_PASS : TEST_FAIL;
}

// Moon-crash arm-state lock (the operator-confirmed P0). A moon crash reloads
// the file from flash over gSaveContext.save; ShipSaveInfo (saveType + the whole
// rando block) is a MEMBER of Save, so an in-memory paired world loses its
// randomizer identity and MM silently plays vanilla from then on -- permanently,
// because the next switch-out freezes the vanilla save. Drives the REAL
// Sram_ResetSaveFromMoonCrash and probes arm state through a VB verdict (not a
// hook count, which lags a deferred unregister). Bridge body in
// games/mm/2s2h/mm_rando_gen_test.cpp. Needs a display, same as mm-rando-gen.
extern "C" int MM_Rando_HeadlessMoonCrashArmState(void);

TestResult Test_MMMoonCrashArmState(void) {
    printf("[TEST] mm-moon-crash-arm-state: a moon crash preserves the paired world and its IS_RANDO hooks\n");

    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }

    static char arg0[] = "redship";
    static char* fakeArgv[] = { arg0, nullptr };
    InitOTRForMMFirstBoot(1, fakeArgv);

    int rc = MM_Rando_HeadlessMoonCrashArmState();
    printf("[TEST] %s: moon-crash arm-state rc=%d\n", rc == 0 ? "PASS" : "FAIL", rc);
    return rc == 0 ? TEST_PASS : TEST_FAIL;
}

// Owl-save arm-state lock (#487). The same class as the moon crash, on the save
// a player performs every cycle: Sram_UpdateWriteToFlashOwlSave re-reads the
// file it just wrote and memcpy's it over gSaveContext, and MM's flash read is a
// no-op stub in single-exe, so the paired world's ShipSaveInfo (saveType + the
// whole rando block) is committed away as zeros. Also covers the file-copy leg
// (func_80147414). Probes arm state through a VB verdict; a hook count is blind
// here in both directions, since nothing re-dispatches OnSaveLoad after the
// readback. Bridge body in games/mm/2s2h/mm_rando_gen_test.cpp. Needs a display,
// same as mm-rando-gen.
extern "C" int MM_Rando_HeadlessOwlSaveArmState(void);

TestResult Test_MMOwlSaveArmState(void) {
    printf("[TEST] mm-owl-save-arm-state: an owl save preserves the paired world and its IS_RANDO hooks\n");

    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }

    static char arg0[] = "redship";
    static char* fakeArgv[] = { arg0, nullptr };
    InitOTRForMMFirstBoot(1, fakeArgv);

    int rc = MM_Rando_HeadlessOwlSaveArmState();
    printf("[TEST] %s: owl-save arm-state rc=%d\n", rc == 0 ? "PASS" : "FAIL", rc);
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

// #560 root-cause lock: libultraship's O2rArchive reads ONE shared zip_t with
// no synchronization anywhere (ResourceManager::mMutex guards only the cache
// map). During menu-triggered seed generation, a resource-pool worker and the
// render thread zip_fread the same handle concurrently; both reads come back
// zeroed ("type 0x0, Format 0, Version 0" — the exact logged pair from the
// field crash), both loads return nullptr, and one unchecked consumer AVs. The
// fix is a per-archive-object mutex in the libultraship fork; this row is its
// deterministic regression tripwire. Body in
// src/common/tests/test_zip_contention.c: 4 loader threads over disjoint cold
// slices of real soh.o2r entries + 1 hot reader over cached ones, all through
// the REAL ResourceManager against the archive #562 now mounts in-tier, with a
// single-threaded calibration pass first so a contention failure can only mean
// concurrency.
//
// Display-free (no Fast3dWindow needed): the loads run on this test's own
// threads via LoadResourceProcess — the render thread's exact call shape — so
// concurrency never depends on the resource pool's sizing, and the row runs in
// the display-free redship tier and inside `--test all`.
//
// SKIP (not FAIL) when soh.o2r is unresolvable: the netplay-relay CI job
// re-runs the redship label archive-less ON PURPOSE, as the tier's
// archive-less control (#562). This does not reopen the silent-staging hole —
// the rando-tier RandoGenFullInit row hard-fails on exactly that condition, so
// a staging regression stays loud there while this row reports an honest CTest
// "Skipped" (SKIP_RETURN_CODE 77) instead of a false red.
TestResult Test_ZipContention(void) {
    printf("[TEST] zip-contention: concurrent cold loads from one o2r return intact resources (#560)\n");

    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }

    const std::string sohArchive = Ship::Context::LocateFileAcrossAppDirs("soh.o2r");
    if (!std::filesystem::exists(sohArchive)) {
        printf("[TEST] SKIP: no soh.o2r resolvable (tried '%s') — the archive-less redship control run keeps "
               "this row skipped by design; RandoGenFullInit is the loud tripwire for a staging regression "
               "(#560/#562)\n",
               sohArchive.c_str());
        return TEST_SKIP;
    }

    if (OoT_InitSharedContextSubsystems() != 0) {
        printf("[TEST] FAIL: shared bring-up reported failure\n");
        return TEST_FAIL;
    }

    // Give the calibration pass the factory surface the real bring-up would
    // have. MM's headless registrar (the same call Test_BootMM makes) brings
    // the Path/Room/Cutscene per-archive dispatchers and MM's own types; the
    // Texture/Blob factories below are otherwise registered only inside the
    // games' display-bound Initialize paths. Texture matters here: the
    // OTR-format textures in soh.o2r are the largest factory-parseable
    // entries, and a cold glyph texture load is the render-thread arm of the
    // #560 race.
    if (MM_RegisterResourceFactoriesHeadless() != 0) {
        printf("[TEST] FAIL: MM resource factory registration failed\n");
        return TEST_FAIL;
    }
    auto loader = ctx->GetResourceManager()->GetResourceLoader();
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV0>(), RESOURCE_FORMAT_BINARY,
                                    "Texture", static_cast<uint32_t>(Fast::ResourceType::Texture), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV1>(), RESOURCE_FORMAT_BINARY,
                                    "Texture", static_cast<uint32_t>(Fast::ResourceType::Texture), 1);
    loader->RegisterResourceFactory(std::make_shared<Ship::ResourceFactoryBinaryBlobV0>(), RESOURCE_FORMAT_BINARY,
                                    "Blob", static_cast<uint32_t>(Ship::ResourceType::Blob), 0);

    int rc = ZipContention_RunHeadless();
    printf("[TEST] %s: zip contention rc=%d\n", rc == 0 ? "PASS" : "FAIL", rc);
    return rc == 0 ? TEST_PASS : TEST_FAIL;
}

TestResult Test_CrossGameModel(void) {
    printf("[TEST] crossgame-model: an MM-exclusive model resolves and draws from an OoT-only session (#577)\n");

    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }

    // The curated cross-game archive is produced by GenerateRedshipOtr, which
    // needs BOTH games extracted. A tree that has not run it (or the tier's
    // deliberate archive-less control run) skips rather than going red — the
    // same convention as ZipContention.
    const std::string crossGameArchive = Ship::Context::LocateFileAcrossAppDirs("redship.o2r");
    if (!std::filesystem::exists(crossGameArchive)) {
        printf("[TEST] SKIP: no redship.o2r resolvable (tried '%s') — run the GenerateRedshipOtr target "
               "(needs both oot.o2r and mm.o2r extracted) to arm this row (#577)\n",
               crossGameArchive.c_str());
        return TEST_SKIP;
    }

    if (OoT_InitSharedContextSubsystems() != 0) {
        printf("[TEST] FAIL: shared bring-up reported failure\n");
        return TEST_FAIL;
    }

    // ONLY OoT has been brought up at this point: the bring-up mounts soh.o2r
    // and nothing else. Mounting the curated archive here is what makes an
    // MM-exclusive path reachable, through the same
    // ArchiveManager::AddArchive call a runtime mount would use — nothing in
    // rsbs/ mounts redship.o2r yet, so this row is its only consumer. If this
    // row ever passes without this mount, the lock has gone vacuous.
    auto archiveManager = ctx->GetResourceManager()->GetArchiveManager();
    if (archiveManager == nullptr || archiveManager->AddArchive(crossGameArchive) == nullptr) {
        printf("[TEST] FAIL: could not mount the cross-game archive at '%s'\n", crossGameArchive.c_str());
        return TEST_FAIL;
    }

    // The model pipeline's factories, registered by OoT's own TU so this row
    // exercises the factory surface a real OoT session has — Texture,
    // DisplayList, Vertex, AND the game-owned 'OARR' Array reader that every
    // extracted object's vertex data actually needs. Registering that set by
    // hand here is how the first run of this row went red for the wrong reason
    // (see the comment on OoT_RegisterModelResourceFactoriesHeadless).
    if (OoT_RegisterModelResourceFactoriesHeadless() != 0) {
        printf("[TEST] FAIL: OoT model factory registration failed\n");
        return TEST_FAIL;
    }

    int rc = CrossGameModel_RunHeadless();
    printf("[TEST] %s: cross-game model rc=%d\n", rc == 0 ? "PASS" : "FAIL", rc);
    return rc == 0 ? TEST_PASS : TEST_FAIL;
}

// #595: the two archives we generate ourselves — soh.o2r and 2ship.o2r —
// collided on 595 paths, 21 differing in content, in the ONE flat
// ArchiveManager both are mounted into. Resolution is last-added-wins, so which
// copy either game rendered depended on which game booted first and whether a
// switch had happened (observed: chest-corner textures). This row mounts them
// BOTH WAYS ROUND and requires every path to resolve to the same bytes either
// way. Body + anti-vacuity guards in
// src/common/tests/test_curated_archive_order.c.
//
// Archive-layer only: no ResourceManager, no factories, no display. SKIPs when
// either curated archive is unstaged, matching the ZipContention policy (the
// netplay-relay job re-runs this label archive-less on purpose, #562).
TestResult Test_CuratedArchiveOrder(void) {
    printf("[TEST] curated-archive-order: soh.o2r/2ship.o2r resolve identically in either mount order (#595)\n");

    const std::string sohArchive = CaoResolveArchive("soh.o2r");
    const std::string mmArchive = CaoResolveArchive("2ship.o2r");
    if (sohArchive.empty() || mmArchive.empty()) {
        printf("[TEST] SKIP: curated archives not staged (soh.o2r '%s', 2ship.o2r '%s') — the archive-less "
               "control run keeps this row skipped by design (#562)\n",
               sohArchive.c_str(), mmArchive.c_str());
        return TEST_SKIP;
    }

    int rc = CuratedArchiveOrder_RunHeadless(sohArchive.c_str(), mmArchive.c_str());
    printf("[TEST] %s: curated archive order rc=%d\n", rc == 0 ? "PASS" : "FAIL", rc);
    return rc == 0 ? TEST_PASS : TEST_FAIL;
}

// #593: a user mod overrides a base asset only by being mounted after it, and
// the switch-time base re-add (the #154 fix) put the base archives back on top
// — silently revoking every override, with the mods still listed as enabled.
// This row drives the production Combo_EnsureGameArchivesLoaded against the
// real shared ArchiveManager with a copy of 2ship.o2r standing in for a mod,
// and carries its own empty-registry negative control. Body in
// src/common/tests/test_curated_archive_order.c.
TestResult Test_ModArchiveSurvivesSwitch(void) {
    printf("[TEST] mod-survives-switch: a registered mod archive still wins after a game switch (#593)\n");

    const std::string sohArchive = CaoResolveArchive("soh.o2r");
    const std::string mmArchive = CaoResolveArchive("2ship.o2r");
    if (sohArchive.empty() || mmArchive.empty()) {
        printf("[TEST] SKIP: no staged archive to stand in for a base archive and a mod (soh.o2r '%s', "
               "2ship.o2r '%s')\n",
               sohArchive.c_str(), mmArchive.c_str());
        return TEST_SKIP;
    }

    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }
    if (OoT_InitSharedContextSubsystems() != 0) {
        printf("[TEST] FAIL: shared bring-up reported failure\n");
        return TEST_FAIL;
    }

    int rc = ModArchiveSurvivesSwitch_RunHeadless(sohArchive.c_str(), mmArchive.c_str());
    printf("[TEST] %s: mod survives switch rc=%d\n", rc == 0 ? "PASS" : "FAIL", rc);
    return rc == 0 ? TEST_PASS : TEST_FAIL;
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

// MM tracker registration surface (#392). The bridge (see the extern decl at
// the top) constructs a standalone Ship::Gui, so it needs the same
// display-free shared bring-up as boot-oot: GuiWindow ctors read
// ConsoleVariables, ItemTrackerSettings::InitElement reads Config, and the
// Gui ctor's default ConsoleWindow registers Console commands.
TestResult Test_MMTrackersGui(void) {
    printf("[TEST] mm-trackers-gui: MM tracker windows register de-collided + gate on the active game (#392)\n");

    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }
    if (OoT_InitSharedContextSubsystems() != 0) {
        printf("[TEST] FAIL: shared bring-up reported failure\n");
        return TEST_FAIL;
    }

    return MM_TrackersGui_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// MM registrar coverage (#516). Needs the same display-free shared bring-up as
// the Gui rows above, for a different reason: it drives the real MM_Rando_Init,
// whose ShipInit registrars and CVar-gated legs read the Ship::Context
// singleton's ConsoleVariables. Without it MM_Rando_Init dereferences a null
// singleton rather than reporting a clean failure.
TestResult Test_MMRegistrarCoverage(void) {
    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }
    if (OoT_InitSharedContextSubsystems() != 0) {
        printf("[TEST] FAIL: shared bring-up reported failure\n");
        return TEST_FAIL;
    }

    return MM_RegistrarCoverage_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// Cross-game spoiler window (#496, ADR 0008). Same bring-up as the MM tracker
// bridge above and for the same reason: the test constructs real
// Ship::GuiWindow objects on a standalone Ship::Gui, and the GuiWindow ctor
// reads its visibility CVar off the Ship::Context singleton's
// ConsoleVariables. Without this the ctor dereferences a null singleton.
TestResult Test_ComboSpoilerWindow(void) {
    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }
    if (OoT_InitSharedContextSubsystems() != 0) {
        printf("[TEST] FAIL: shared bring-up reported failure\n");
        return TEST_FAIL;
    }

    return Combo_SpoilerWindow_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// MM randomizer options pane (#497 step 4, ADR 0004 + 0008). Same bring-up as
// the two Gui bridges above and for the same reason.
TestResult Test_ComboMMOptionsWindow(void) {
    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }
    if (OoT_InitSharedContextSubsystems() != 0) {
        printf("[TEST] FAIL: shared bring-up reported failure\n");
        return TEST_FAIL;
    }

    return Combo_MMOptionsWindow_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// Combo tracker adapters (#458). No Gui, but the OoT-side authoring seam
// constructs a real Rando::Context, so it gets the same display-free bring-up
// as the Gui bridges (cheap insurance against ctor-time singleton reads).
TestResult Test_ComboTrackerView(void) {
    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }
    if (OoT_InitSharedContextSubsystems() != 0) {
        printf("[TEST] FAIL: shared bring-up reported failure\n");
        return TEST_FAIL;
    }

    return Combo_TrackerView_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// Combo tracker window (#458, ADR 0008). Same bring-up as the Gui bridges
// above and for the same reason (GuiWindow ctors read ConsoleVariables off
// the Ship::Context singleton).
TestResult Test_ComboTrackerWindow(void) {
    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }
    if (OoT_InitSharedContextSubsystems() != 0) {
        printf("[TEST] FAIL: shared bring-up reported failure\n");
        return TEST_FAIL;
    }

    return Combo_TrackerWindow_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// MM option TABLE lock (#497 step 4, #499 step 5). No Gui, but it reads and
// writes the option CVars through the real ConsoleVariables store, so it needs
// the same display-free bring-up.
TestResult Test_MMRandoOptions(void) {
    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }
    if (OoT_InitSharedContextSubsystems() != 0) {
        printf("[TEST] FAIL: shared bring-up reported failure\n");
        return TEST_FAIL;
    }

    return MM_RandoOptions_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
}

// Paired MM PROFILE lock (#499 steps 2-4). Drives the real resolver over a
// zeroed MM SaveContext — no fill, no region graph, no window — which is the
// whole point of extracting it out of OnFileCreate.
TestResult Test_MMPairedProfile(void) {
    auto ctx = CreateHarnessStyleContext();
    if (!ctx) {
        printf("[TEST] FAIL: could not create Ship::Context singleton\n");
        return TEST_FAIL;
    }
    if (OoT_InitSharedContextSubsystems() != 0) {
        printf("[TEST] FAIL: shared bring-up reported failure\n");
        return TEST_FAIL;
    }

    return MM_PairedProfile_RunHeadless() == 0 ? TEST_PASS : TEST_FAIL;
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
    // #560: the same generation with the RSBS_DISABLE_OTR_INIT mask OFF and the
    // fill on a worker thread — the bring-up and call shape a player actually
    // gets. Needs a display like rando-gen, so `--test all` skips it (below).
    {"rando-gen-full-init", "Seed generation with the full OTR bring-up live, on a worker thread (#560)",
     Test_RandoGenFullInit},
    // #441: no generated hint may resolve to the no-item sentinel. Needs a
    // display like rando-gen, so `--test all` skips it (below).
    {"rando-hint-validity", "Every generated hint names a real item (#441)", Test_RandoHintValidity},
    // #441 runtime complement: item hints must still name their real item after a
    // save/reload cycle rebuilds the placement table. Needs a display like
    // rando-gen, so `--test all` skips it (below).
    {"rando-hint-reload", "Item hints survive a save/reload cycle (#441)", Test_RandoHintReload},
    // #441 root-cause lock: item hints must survive a cross-game OoT arrival,
    // whose title-chain Save_Init used to wipe the placement table. Needs a
    // display like rando-gen, so `--test all` skips it (below).
    {"rando-hint-crossgame", "Item hints survive a cross-game OoT arrival (#441)", Test_RandoHintCrossGame},
    // #482: two more instances of the #441 class -- the check tracker's
    // areasSpoiled and the item tracker's typed notes are blanked by arrival
    // init-funcs and never rehydrated. Pure (no display), so it runs in the
    // display-free suite (NOT skipped by `--test all`).
    {"tracker-arrival-rehydration", "Check/item tracker state survives a cross-game OoT arrival (#482)",
     Test_TrackerArrivalRehydration},
    // Lane B unified seed: producer stamps gComboCtx at generation time; the
    // two-process same-seed determinism diff runs via the SeedDeterminism row.
    // Needs a display like rando-gen, so `--test all` skips it (below).
    {"rando-determinism", "Unified-seed producer fires + same-seed fill is reproducible (Lane B)",
     Test_RandoDeterminism},
    // Lane C0: MM's randomizer is reachable and generates headlessly. Needs a
    // display like rando-gen, so `--test all` skips it (below).
    {"mm-rando-gen", "MM rando generation runs headlessly + writes tagged spoiler (Lane C0)", Test_MMRandoGen},
    // #510: the reverse pool end to end over a REAL fill. In this tier, not the
    // display-free one, because OoT's host predicate reads a fill result — with
    // no fill it accepts nothing and the lock would pass vacuously.
    {"foreign-placement-oot", "Real OoT fill hosts MM items deterministically; MM's award chain accepts them (#510)",
     Test_ForeignPlacementOoT},
    // #439: the paired world must activate on the SWITCH-ENTRY path (the only
    // flow a player actually takes), not just the direct OnSaveInit chain.
    // Same display requirement as mm-rando-gen, so `--test all` skips it.
    {"mm-pair-switch-entry", "Paired MM world activates via switch-entry; existing saves untouched (#439)",
     Test_MMPairSwitchEntry},
    // ADR 0010 increment 1.2 lock (b): a pinned master seed that NEEDS the
    // attempt ladder converges, records its winning attempt, and digests the
    // world for the two-process MMPairedAttemptDeterminism diff. Same display
    // requirement as mm-rando-gen, so `--test all` skips it.
    {"mm-paired-attempt", "Pinned multi-attempt seed converges deterministically on the ladder (ADR 0010 inc. 1.2)",
     Test_MMPairedAttempt},
    // ADR 0010 increment 1.2 lock (c): exhaustion is a LOUD failure on the
    // #533 surface, never a silent vanilla Termina. Same display requirement
    // as mm-rando-gen, so `--test all` skips it.
    {"mm-paired-exhaustion", "Unconvergeable paired profile exhausts the ladder and refuses loudly (ADR 0010)",
     Test_MMPairedExhaustion},
    // #439 follow-up: the OTHER entry paths into MM gameplay (file-select LOAD,
    // Song of Time / cycle reset, DayTelop) must also reach a live PlayState
    // with the IS_RANDO hooks matching the save. Same display requirement as
    // mm-rando-gen, so `--test all` skips it.
    {"mm-reload-arm-state", "IS_RANDO hooks match the save on MM's non-arrival entry paths (#439)",
     Test_MMReloadArmState},
    // The operator-confirmed P0: a moon crash must not strip a paired world's
    // randomizer identity. Same display requirement, so `--test all` skips it.
    {"mm-moon-crash-arm-state", "A moon crash preserves the paired MM world and its IS_RANDO hooks",
     Test_MMMoonCrashArmState},
    // #487: the owl save (and the file copy) do the same readback-then-commit
    // over gSaveContext that the moon crash did. Same display requirement, so
    // `--test all` skips it.
    {"mm-owl-save-arm-state", "An owl save preserves the paired MM world and its IS_RANDO hooks (#487)",
     Test_MMOwlSaveArmState},
    {"boot-oot", "Shared-context bring-up leaves no null subsystems (#329)", Test_BootOoT},
    {"boot-mm", "MM-first bring-up prerequisites on the shared context (#330)", Test_BootMM},
    // #560: the archive-handle contention lock. Display-free — its own threads
    // ARE the concurrency — so `--test all` runs it; it SKIPs itself when no
    // soh.o2r is mounted (the netplay-relay archive-less control tier).
    {"zip-contention", "Concurrent cold o2r loads return intact resources (#560)", Test_ZipContention},
    {"crossgame-model", "An MM-exclusive model resolves+draws from an OoT-only session (#577)",
     Test_CrossGameModel},
    // #595/#593: archive-layer order locks. Display-free; both SKIP when the
    // curated archives are not staged.
    {"curated-archive-order", "soh.o2r/2ship.o2r resolve identically in either mount order (#595)",
     Test_CuratedArchiveOrder},
    {"mod-survives-switch", "A registered mod archive still wins after a game switch (#593)",
     Test_ModArchiveSurvivesSwitch},
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
    // #493: the reverse direction's OoT-keyed placement carve is a separate key
    // space from the MM-keyed one, serializes byte-exact, and zero-extends.
    {"foreign-item-give-reverse",
     "Reverse (MM->OoT) placement carve: separate key space, serializes, redeems once (#493)",
     Test_ForeignItemGiveReverse},
    // #488: host selection must reject any check class the game does not arm —
    // the give path is gated on `.eligible`, so an unarmed host strands a
    // pinned OoT progression item and no error is ever raised.
    {"foreign-host-eligibility",
     "Foreign hosts limited to game-armed check classes; skipped/sentinel slots rejected (#488)",
     Test_ForeignHostEligibility},
    // #502: MM's half of the crossing. The reverse row above deliberately stops
    // at a test award callback because MM_AwardSharedItem was a placeholder
    // fprintf; this one drives the real one and the real give behind it.
    {"foreign-award-mm", "MM's real award reaches the real give: once per crossing, deferred safely (#502)",
     Test_ForeignAwardMM},
    // #510: the reverse direction's SOURCE pool. Display-free — the table is a
    // static in the WHOLE_ARCHIVE'd 2ship_rando and its registrar runs before
    // main() — so this also proves that registrar survived the link.
    {"foreign-pool-mm", "MM's cross-game source pool: registered, well-formed, giveable, non-junk (#510)",
     Test_ForeignPoolMM},
    // #525: shared cross-game resources. Display-free, ROM-free and save-free —
    // everything under test is gComboCtx plus a RAM watermark table.
    {"shared-resources", "One quantity across both games: watermark, disciplines, seed, heart clamp (#525)",
     Test_SharedResources},
    {"combo-spoiler-view", "In-game spoiler view model: named crossings, collected state, unpaired != empty (#496)",
     Test_ComboSpoilerView},
    {"combo-spoiler-window", "Common-owned spoiler window registers de-collided; inert under every active game (#496)",
     Test_ComboSpoilerWindow},
    // Combo tracker (#458): both games' progress through per-game adapters —
    // MM's frozen shadow at registered offsets, OoT's suspended heap through a
    // registered vtable — staleness-labelled, plus the window's inertness.
    {"combo-tracker-view", "Tracker adapters recover authored MM shadow + OoT heap worlds, staleness-labelled (#458)",
     Test_ComboTrackerView},
    {"combo-tracker-window",
     "Combo tracker window registers de-collided; inert under every active game and absent adapters (#458)",
     Test_ComboTrackerWindow},
    // MM randomizer options surface (#497 step 4, #499). Three locks, split by
    // what they can see: the table needs MM's headers, the profile needs MM's
    // SaveContext, the window needs a Gui.
    {"mm-rando-options", "Every MM rando option has a row, a label, a bound cvar, and honest gating (#497)",
     Test_MMRandoOptions},
    {"mm-paired-profile", "The paired MM profile honours explicit choices and publishes a moving digest (#499)",
     Test_MMPairedProfile},
    {"combo-mm-options-window",
     "Common-owned MM options pane registers de-collided; inert under every active game (#497)",
     Test_ComboMMOptionsWindow},
    // Netplay 1a (ADR 0005, #460): the sourced-grant model, transport-free.
    {"grant-idempotency", "Retransmit delivers once; a second gift of the same item delivers twice (ADR 0005)",
     Test_GrantIdempotency},
    {"grant-redeem-no-switch", "Grants redeem in received order at a safe point, no switch machinery (ADR 0005)",
     Test_GrantRedeemNoSwitch},
    {"grant-overflow", "Full array refuses loudly, backpressures sources, reclaims redeemed slots (ADR 0005)",
     Test_GrantOverflow},
    {"grant-persistence", "Cursors + overflow ride the .redsave; legacy loads unset; reset retires atomically "
     "(ADR 0005)",
     Test_GrantPersistence},
#ifdef RSBS_NETPLAY
    // Netplay 1b (ADR 0007, #460): the grant relay, over a loopback ledger with
    // multi-server's semantics. Registered only when the relay is built.
    {"relay-wire-format", "Relay wire format matches golden vectors; foreign payloads skip (ADR 0007)",
     Test_RelayWireFormat},
    {"relay-loopback", "Loopback peers: retransmit delivers once, two peers' same item delivers twice, "
     "self-echo filtered (ADR 0007)",
     Test_RelayLoopback},
    {"relay-catchup", "Late joiner catches up from ledger base 0; grants survive a cross-game switch (ADR 0007)",
     Test_RelayCatchup},
    {"relay-backpressure", "A full array backpressures the relay without losing the grant (ADR 0007)",
     Test_RelayBackpressure},
    {"relay-suspend-latch", "Suspend stops applying but not polling; resume drains in order (ADR 0007)",
     Test_RelaySuspendLatch},
#endif
    {"context", "Test context/state management", Test_Context},
    // Clears all frozen states on entry and exit, so it must not run between a
    // test that freezes and one that expects that freeze to still be there.
    {"hotswap-freeze", "F10 hot swap freezes the departing game; frozen state is single-use (#364)",
     Test_HotSwapFreeze},
    // #440: the inverse #400 never got. Retiring a blob ON CONSUME stops it
    // being consumed twice; it does nothing about a dead session's blob still
    // being there for the NEXT session's first consume. Like hotswap-freeze
    // this clears all frozen states on entry and exit, so it must not run
    // between a test that freezes and one expecting that freeze to persist.
    {"session-invalidation", "Soft reset / new game retire cross-game session state; loads still restore (#440)",
     Test_SessionInvalidation},
    {"lifecycle", "Game lifecycle unit tests", Test_Lifecycle},
    // Unified save (.redsave) headless coverage (issue #35, Phase 2 T6).
    {"save-roundtrip-tiers", "Unified .redsave preserves ComboContext + both SaveContexts (#35)", Test_SaveRoundtripTiers},
    {"save-header", "Unified .redsave header fields + CRC are well-formed (#35)", Test_SaveHeader},
    {"save-has-delete", "Unified save HasSave/DeleteSave lifecycle (#35)", Test_SaveHasDelete},
    {"save-version-reject", "Unified save Load rejects unknown version, no clobber (#35)", Test_SaveVersionReject},
    {"save-size-mismatch", "Unified save Load rejects oversized tier, no clobber (#35)", Test_SaveSizeMismatch},
    {"save-legacy-size", "Unified save Load zero-extends shorter legacy tiers (#35)", Test_SaveLegacySize},
    {"save-crc-corrupt", "Unified save Load rejects corrupt payload, no clobber (#35)", Test_SaveCrcCorrupt},
    // #533: REFUSED as a first-class slot state, distinct from ABSENT.
    {"save-refused-quarantine", "A refused .redsave is quarantined byte-exact and the slot write-latched (#533)",
     Test_SaveRefusedQuarantine},
    {"save-write-latch", "Save refuses slots this session never loaded/created/erased (#533)", Test_SaveWriteLatch},
    {"save-arm-on-create", "File-create quarantines a failing .redsave before its first write (#533)",
     Test_SaveArmOnCreate},
    {"save-refused-meta", "The slot surface reports ABSENT / VALID / REFUSED distinctly (#533)",
     Test_SaveRefusedMeta},
    // Tier-1 format-headroom locks: the migration path Lane A's widened
    // sharedItems rides on. Without these, "ComboContext can grow" is a claim.
    {"save-combo-legacy-record", "Pre-headroom Tier-1 still loads and zero-extends", Test_SaveComboLegacyRecord},
    {"save-combo-record-fixed", "Tier-1 written at a fixed padded size; headroom round-trips", Test_SaveComboRecordFixed},
    {"save-combo-oversize", "Load rejects an oversized Tier-1 record, no clobber", Test_SaveComboOversize},
    {"save-tagged-items", "Origin-tagged shared items round-trip; empty slots stay unset (ADR 0002)",
     Test_SaveTaggedItems},
    // The commit choke point (#537/#531): one game-thread-marshalled snapshot,
    // a monotonic generation in both artifacts, and load-time skew detection.
    {"commit-generation-monotonic", "Choke-point commits stamp a monotonic Tier-1 generation (#537)",
     Test_CommitGenerationMonotonic},
    {"commit-torn-write", "Post-stage mutation cannot reach the .redsave; the #537 tear is unrepresentable",
     Test_CommitTornWrite},
    {"commit-generation-skew", "Load detects .redsave/.sav freshness divergence (#531/#564 V16)",
     Test_CommitGenerationSkew},
    {"mm-scene-parse", "MM scene commands parse via the S2H factory (#344)", Test_MMSceneParse},
    {"seq-map-bounds", "Sequence-map capacity covers the id range + custom slack (#371, #378)", Test_SeqMapBounds},
    {"cvar-classification", "Cross-game CVar classification matches ADR 0003 + the inventory (#34)",
     Test_CVarClassification},
    {"active-queue", "__osGetActiveQueue returns a walkable list, not a return register (#385)", Test_ActiveQueue},
    {"oot-audio-init-guard", "OoT synth no-ops while gAudioContextInitalized == false (#365)", Test_OoTAudioInitGuard},
    {"gp-watchdog", "Gameplay round-trip watchdog is wall-clock budgeted, can fire before the timeout (#376)",
     Test_GpWatchdog},
    {"mm-scene-execute", "MM scene commands execute against a PlayState (#344)", Test_MMSceneExecute},
    {"mm-culling-binding", "MM's Ship_ExtendedCulling* bind MM's Actor, not OoT's (#382)", Test_MMCullingBinding},
    {"mm-gi-shim", "MM hook registration goes through the MM-owned shim, not the shared 4-byte instance (#395)",
     Test_MMGIShim},
    {"mm-notification-binding",
     "MM's toasts reach OoT's overlay through the MM_Notify_Emit bridge, Options stays layout-identical (#427)",
     Test_MMNotificationBinding},
    {"mm-fb-effects-binding", "MM's scaled framebuffer draw binds its own body against MM's dimensions (#386)",
     Test_MMFbEffectsBinding},
    // Flash page-table OOB from the 0xFF fileNum sentinel: the moon-crash reset
    // (and the owl-delete write it fires) index the fixed-size flash tables far
    // out of bounds in a cross-game MM session, then copy the garbage over the
    // live save. Pure (no display), so it runs in the display-free suite.
    {"mm-flash-filenum-oob", "Flash page indices stay in bounds for fileNum 0xFF; moon-crash reset keeps the save",
     Test_MMFlashFileNumOob},
    // MM's redship-native unified-save capture (#35 follow-up). MM has no
    // persistence in single-exe (2s2h/SaveManager/*.cpp is link-excluded and
    // the flash stubs are a -1 read plus an empty write), so every .redsave's
    // Tier-3 was zeros and sourceGame could never say GAME_MM. Locks slot
    // normalization, the no-slot no-op, the file round trip, and — the subtle
    // one — that the capture is FULL-WIDTH rather than a sizeof(Save) prefix.
    // Pure (no display, no ROM).
    {"mm-unified-save-capture", "MM captures its live SaveContext into the unified slot, full-width and round-tripping",
     Test_MMUnifiedSaveCapture},
    // A capture the #533/#568 write latch refuses must be a no-op on the
    // SHARED-RESOURCE POOL too, not just on the file: the harvest used to run
    // ahead of the latch check, so a refused commit still credited (or debited)
    // rupees/hearts/magic/ammo and raised permanent monotonic tiers for a record
    // that never reached disk. Pure (no display, no ROM).
    {"mm-capture-harvest-gate", "a latch-refused MM capture leaves the shared-resource pool untouched (#591)",
     Test_MMCaptureHarvestGate},
    // Hook dispatch reaches the MM-owned registry the COND_* macros register
    // into. Registers through the production macros and drives each dispatcher
    // through the name MM's call sites spell, so both a deleted bridge and a
    // dropped rebind #define fail here. Pure (no display, no ROM).
    {"mm-hook-dispatch", "ShouldActorInit/OnActorInit/OnActorDraw/OnOpenText dispatch reaches S2H::GameHooks (#511)",
     Test_MMHookDispatch},
    // The sibling of the row above, one layer earlier: a hook is only as armed
    // as the registrar that (un)registers it, and MM's registrar map had no
    // CVar-change driver at all — the unified menu re-armed OoT and left MM
    // latched at its first-boot value. Pure (no display, no ROM).
    {"mm-shipinit-driver", "A unified-menu CVar change re-arms MM's ShipInit registrars, not only OoT's (#539)",
     Test_MMShipInitDriver},
    // filePlaytime epoch injection: AdvancePlaytime accrued now-lastTimeLog with
    // lastTimeLog unseeded (0), writing a full Unix epoch into the persisted
    // playtime. Pure (no display), so it runs in the display-free suite.
    {"mm-playtime-seed", "AdvancePlaytime seeds an unset lastTimeLog instead of injecting an epoch (#513)",
     Test_MMPlaytimeSeed},
    {"mm-trackers-gui", "MM tracker windows register de-collided on the shared Gui + gate on the active game (#392)",
     Test_MMTrackersGui},
    // Registrar coverage: BenPort.cpp's exclusion elided InitOTR's whole
    // registration list, so the re-homed registrars in MM_Rando_Init must be
    // shown to POPULATE their registries, not merely to link. Runs the real
    // MM_Rando_Init, which is irreversible in-process, so `--test all` skips
    // this entry and it is exercised only through its own CTest row (see the
    // skip block in TestRunner_Run for the full reasoning).
    {"mm-registrar-coverage", "MM_Rando_Init populates the registries BenPort's exclusion emptied (#516)",
     Test_MMRegistrarCoverage},
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
        int skipped = 0;
        int total = 0;

        for (int i = 0; gTests[i].name != nullptr; i++) {
            // rando-gen / rando-determinism need a display (Fast3dWindow
            // bring-up) and the RSBS_DISABLE_OTR_INIT environment; they run as
            // their own CTests ("rando" label, under xvfb-run) rather than in
            // this display-free suite, where they would hang the 60s timeout.
            if (strcmp(gTests[i].name, "rando-gen") == 0 || strcmp(gTests[i].name, "rando-gen-full-init") == 0 ||
                strcmp(gTests[i].name, "rando-determinism") == 0 ||
                strcmp(gTests[i].name, "rando-hint-validity") == 0 ||
                strcmp(gTests[i].name, "rando-hint-reload") == 0 ||
                strcmp(gTests[i].name, "rando-hint-crossgame") == 0 ||
                strcmp(gTests[i].name, "mm-rando-gen") == 0 ||
                strcmp(gTests[i].name, "mm-pair-switch-entry") == 0 ||
                strcmp(gTests[i].name, "mm-paired-attempt") == 0 ||
                strcmp(gTests[i].name, "mm-paired-exhaustion") == 0 ||
                strcmp(gTests[i].name, "mm-reload-arm-state") == 0 ||
                strcmp(gTests[i].name, "mm-moon-crash-arm-state") == 0 ||
                strcmp(gTests[i].name, "mm-owl-save-arm-state") == 0 ||
                strcmp(gTests[i].name, "foreign-placement-oot") == 0) {
                printf("\n--- Skipping: %s (needs display; runs as a rando-label CTest) ---\n", gTests[i].name);
                continue;
            }
            // mm-registrar-coverage is display-free, but it is the only row that
            // runs the real MM_Rando_Init, and that is IRREVERSIBLE in-process:
            // the once-guard cannot be cleared, and InitAll + Rando::Init leave
            // registrants across dozens of S2H::GameHooks types, far more than a
            // row could plausibly reset. Those registrations are correct
            // production state, but later rows in this shared process were
            // written against a process where MM's registrars had never run.
            //
            // Concretely: mm-startup-restore freezes a byte-patterned save and
            // asserts an exact restore, and MM_Play_ConsumeStartupEntrance ends
            // in GameInteractor_ExecuteOnSaveLoad. With RegisterSavingEnhancements
            // live, that hook seeds shipSaveContext.lastTimeLog -- and
            // shipSaveContext is the LAST member of SaveContext, so the seed
            // overwrites the very tail byte that row checks.
            //
            // Worth stating plainly, because it is a latent finding rather than
            // a quirk of this row: in a real MM boot MM_Rando_Init runs before
            // any restore, so that seeder IS live, and mm-startup-restore's
            // byte-exact tail assertion passes today only because its isolated
            // process never registered it. Re-examining that assertion against
            // production is its own change, not this one's.
            //
            // Skipping here costs no coverage: MMRegistrarCoverage has its own
            // CTest row in the redship tier, which is what CI enforces.
            if (strcmp(gTests[i].name, "mm-registrar-coverage") == 0) {
                printf("\n--- Skipping: %s (runs MM_Rando_Init irreversibly; has its own CTest row) ---\n",
                       gTests[i].name);
                continue;
            }
            printf("\n--- Running: %s ---\n", gTests[i].name);
            TestResult result = gTests[i].runFunc();
            total++;

            if (result == TEST_PASS) {
                passed++;
            } else if (result == TEST_SKIP) {
                // Environment-gated (e.g. zip-contention with no archive
                // mounted): counted and reported, never a failure.
                skipped++;
            } else if (result == TEST_FAIL || result == TEST_ERROR) {
                failures++;
            }
        }

        printf("\n=== Test Summary ===\n");
        printf("Total: %d, Passed: %d, Skipped: %d, Failed: %d\n", total, passed, skipped, failures);

        return failures;
    }

    // Run single test
    TestResult result = RunSingleTest(testName);
    if (result == TEST_SKIP) {
        // Rows that can self-skip declare SKIP_RETURN_CODE 77 in
        // CMake/SingleExecutable.cmake, so ctest reports "Skipped" rather than
        // a false red (the netplay-relay job re-runs the redship label
        // archive-less as a control; see Test_ZipContention).
        return 77;
    }
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
