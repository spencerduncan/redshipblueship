/**
 * @file integration_test_hooks.h
 * @brief GameInteractor hooks for integration testing
 *
 * Provides hook registration for detecting game boot completion during
 * integration tests. These hooks integrate with the GameInteractor system
 * to detect when the game reaches specific states (title screen, file select, etc.)
 */

#ifndef RSBS_INTEGRATION_TEST_HOOKS_H
#define RSBS_INTEGRATION_TEST_HOOKS_H

#include "game.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Integration test mode types
 */
typedef enum {
    INT_TEST_NONE = 0,
    INT_TEST_BOOT_OOT,            // Boot OoT, exit on title/file select
    INT_TEST_BOOT_MM,             // Boot MM, exit on title/file select
    INT_TEST_SWITCH_OOT_HMS_TO_MM,        // Boot OoT, trigger HMS entrance, verify MM South Clock Town spawn
    INT_TEST_SWITCH_MM_CLOCKTOWN_SOUTH_TO_OOT, // Boot MM, trigger the Clock Tower door, verify OoT Market spawn
                                               // (name is historical — the trigger moved from the SCT south exit
                                               // to the tower door when the arrival became SCT)
    INT_TEST_ARCHIVE_HOTSWAP_CYCLE,       // Boot OoT, hot-swap OoT<->MM >=3 times, verify healthy runtime (#263)
    INT_TEST_GAMEPLAY_ROUNDTRIP           // Full operator repro: debug save + live gameplay + production
                                          // cross-game round-trip + post-return warp + door transition
} IntegrationTestMode;

/**
 * Phases of the gameplay round-trip repro. The phase value is the contract
 * between the OoT-side and MM-side hook drivers (each game's
 * GameExports_SingleExe.cpp): a phase is owned by exactly one game, which
 * advances it when its step completes. Mirrors the manual operator repro:
 * load debug save -> play -> door into Happy Mask Shop -> MM South Clock
 * Town (as if walking out of the Clock Tower) -> play -> Clock Tower door ->
 * OoT resume (the crash surface) -> play -> debug warp -> play -> door
 * transition -> play.
 */
typedef enum {
    GP_PHASE_BOOT = 0,      // OoT: waiting to inject the debug save + enter Play
    GP_PHASE_OOT_PRE,       // OoT: live gameplay frames, then trigger the HMS door
    GP_PHASE_MM_STABILIZE,  // MM: waiting for the South Clock Town scene load (tower-exit arrival)
    GP_PHASE_MM_PLAY,       // MM: live gameplay frames, then trigger the Clock Tower door
    GP_PHASE_OOT_RETURN,    // OoT: RESUME leg — restored save + return entrance + gameplay frames
    GP_PHASE_OOT_WARP,      // OoT: post-return debug warp arrival + gameplay frames
    GP_PHASE_OOT_EXIT,      // OoT: final door-transition arrival + gameplay frames
    GP_PHASE_DONE           // PASS signaled
} GameplayPhase;

/**
 * Runtime parameters for INT_TEST_GAMEPLAY_ROUNDTRIP, read from the
 * environment when the mode is selected (defaults in parentheses):
 *   RSBS_GP_FRAMES        (120)    gameplay frames per phase
 *   RSBS_GP_CYCLES        (1)      OoT->MM->OoT round trips before the warp
 *   RSBS_GP_BOOT_ENTRANCE (0x01D1) OoT entrance the debug save boots into
 *   RSBS_GP_WARP_ENTRANCE (0x00B1) post-return debug-warp target (map-select Market)
 *   RSBS_GP_WARP_FRAMES   (=FRAMES) gameplay frames for the post-return warp
 *                                  phase only — lets a Lon Lon interior soak
 *                                  run long without stretching every phase
 *   RSBS_GP_EXIT_ENTRANCE (0x0033) final door transition target
 *   RSBS_GP_BOOT_AGE      (child)  "adult" boots the debug save as adult Link,
 *                                  exercising the forced-child-on-return swap
 *                                  end-to-end (the return leg must still
 *                                  arrive as child)
 *   RSBS_GP_CAMERA_ASSERT (1)      0 disables the return/warp-phase
 *                                  camera-follow assert (forced player march +
 *                                  camera displacement check, bug 1b)
 *   RSBS_GP_WATCHDOG_SECS (60)     wall-clock seconds a single OoT-owned phase
 *                                  may run WITHOUT advancing before the watchdog
 *                                  dumps state and fails the run. Must stay well
 *                                  under the CTest TIMEOUT so the diagnostic
 *                                  beats the hard wall-clock kill (#376 item 4).
 *                                  Raise it in lockstep if you raise
 *                                  RSBS_GP_WARP_FRAMES for a long in-scene soak.
 * Entrance values accept hex (0x...) or decimal and must be < OoT's ENTR_MAX.
 */
typedef struct {
    int framesPerPhase;
    int cycles;
    uint16_t bootEntrance;
    uint16_t warpEntrance;
    uint16_t exitEntrance;
    int bootAdult;
    int warpFrames;
    int cameraAssert;
    int watchdogSecs;
} GameplayTestConfig;

/**
 * Variants of INT_TEST_GAMEPLAY_ROUNDTRIP. The variant rides the round trip's
 * phase machine, drivers and watchdogs unchanged and changes only what the
 * session plays and what each arrival asserts:
 *
 *   GP_VARIANT_ROUNDTRIP (int-gameplay-roundtrip): the OoT debug save, as the
 *       repro has always run. Every MM arrival is vanilla
 *       (`[MM] pairing: skipped-because-no-paired-oot-world`).
 *   GP_VARIANT_PAIRED_FIRST_CROSSING (int-paired-first-crossing): the boot
 *       generates the pinned paired world on the shipped defaults, creates a
 *       file for it through OoT's own new-file seam (OoT_Sram_InitSave, which
 *       runs the production creation event and writes the slot), loads that
 *       file back the way the file select does (Sram_OpenSave + OnLoadGame),
 *       and plays it. The MM arrival must hydrate the creation-frozen half with
 *       the archives mounted; the return leg must restore OoT's half, not
 *       regenerate it. RSBS_PFC_SKIP_CREATION=1 skips the generation and the
 *       creation and boots the debug save instead (the red half: the arrival
 *       must then fail on the skipped-because-no-paired-oot-world leg).
 */
typedef enum {
    GP_VARIANT_ROUNDTRIP = 0,
    GP_VARIANT_PAIRED_FIRST_CROSSING = 1
} GameplayVariant;

/**
 * Initialize integration test mode
 * Should be called before game initialization
 * @param mode The integration test mode to run
 */
void IntegrationTest_SetMode(IntegrationTestMode mode);

/**
 * Select the gameplay round trip's variant (after IntegrationTest_SetMode,
 * which resets it to GP_VARIANT_ROUNDTRIP). The paired variant starts the
 * stderr capture below, so its first line is the variant's own.
 */
void IntegrationTest_SetGameplayVariant(GameplayVariant variant);
GameplayVariant IntegrationTest_GetGameplayVariant(void);

/** True when the running test is int-paired-first-crossing. */
bool IntegrationTest_PairedFirstCrossing(void);

/** True when RSBS_PFC_SKIP_CREATION=1: the paired row's red half (no creation). */
bool IntegrationTest_PairedSkipCreation(void);

/**
 * Get current integration test mode
 */
IntegrationTestMode IntegrationTest_GetMode(void);

/**
 * Check if we're in integration test mode
 */
bool IntegrationTest_IsActive(void);

/**
 * Check if the boot test has passed
 */
bool IntegrationTest_BootPassed(void);

/**
 * Signal that boot detection happened
 * Called from hooks when they detect the expected game state
 */
void IntegrationTest_SignalBootComplete(GameId game, const char* reason);

/**
 * Request game exit (for use in hooks)
 */
void IntegrationTest_RequestExit(void);

/**
 * Check if exit was requested
 */
bool IntegrationTest_ExitRequested(void);

// ============================================================================
// Gameplay round-trip repro (INT_TEST_GAMEPLAY_ROUNDTRIP)
// ============================================================================

/**
 * Current phase of the gameplay round-trip. Only meaningful while the
 * gameplay mode is active.
 */
GameplayPhase IntegrationTest_GetGameplayPhase(void);

/**
 * Advance the phase machine. Logs the transition. Called by whichever game
 * side owns the completing phase.
 */
void IntegrationTest_SetGameplayPhase(GameplayPhase phase);

/**
 * The env-derived parameters (parsed once when the mode is selected).
 */
const GameplayTestConfig* IntegrationTest_GetGameplayConfig(void);

/**
 * Completed round-trip counter (incremented by the OoT side at the end of
 * each GP_PHASE_OOT_RETURN gameplay window).
 */
int IntegrationTest_GameplayCyclesDone(void);
void IntegrationTest_GameplayRecordCycle(void);

/**
 * Fail the gameplay test loudly: logs the reason plus the full phase/config
 * state, requests a failing exit, and unblocks the main loop.
 */
void IntegrationTest_GameplayFail(const char* reason);

/**
 * Dump phase/config state to stderr (used by watchdogs and the crash-signal
 * path so a wedged or crashed run is attributable from the log alone).
 */
void IntegrationTest_LogGameplayState(const char* tag);

// ----------------------------------------------------------------------------
// Phase watchdog (#376 item 4)
//
// The OoT-side round-trip watchdog fails loudly, WITH state, when an OoT-owned
// phase stops making progress — instead of letting the phase wedge until the
// CTest/`timeout` wall clock kills the process (exit 124) and the diagnostic
// dump the watchdog exists to produce is never emitted.
//
// It used to budget in FRAMES (maxPhaseFrames * 4 + 3600 == 4080 at defaults),
// which under Xvfb + llvmpipe is 300-800 s of wall clock — longer than the
// 300 s timeout, so it could never fire first. The budget is now WALL-CLOCK
// seconds, per phase, defaulting well under the timeout. These pure helpers
// carry the parse + fire decision so the fix is regression-locked ROM-free
// (test `gp-watchdog`).
// ----------------------------------------------------------------------------

/**
 * Resolve a watchdog budget (seconds) from a raw RSBS_GP_WATCHDOG_SECS string.
 * Pure: NULL/empty/invalid/below-minimum all fall back to the default (60).
 */
int IntegrationTest_GameplayWatchdogParse(const char* raw);

/**
 * The effective per-phase watchdog budget in seconds, reading
 * RSBS_GP_WATCHDOG_SECS from the environment (default 60).
 */
int IntegrationTest_GameplayWatchdogBudgetSecs(void);

/**
 * Pure predicate the OoT watchdog evaluates each phase-owned tick: has a phase
 * that has run `elapsedSecs` of wall clock without advancing exhausted its
 * budget? A non-positive budget disables the watchdog (never expires).
 */
bool IntegrationTest_GameplayWatchdogExpired(double elapsedSecs, int budgetSecs);

// ----------------------------------------------------------------------------
// Progress word for the int-* rows (#793)
//
// Every frame either game starts, and every step main takes between frames
// (the first game's init, a cross-game hand-off, the final shutdown), bumps a
// progress word and names the stage it is in. The stage pointer is stored, not
// copied: pass string literals only.
//
// RSBS_INT_WEDGE=oot|mm|handoff is a test-only fault injection for the rows
// that lock the wall-clock watchdog: it sleeps RSBS_INT_WEDGE_SECS (600) inside
// that game's RSBS_INT_WEDGE_FRAME'th frame (30), or inside main's first
// cross-game hand-off, so the process wedges with no frame completing.
// ----------------------------------------------------------------------------

/** One frame of `game` started (called from each game's per-frame integration hook). */
void IntegrationTest_FrameProgress(GameId game);

/** main entered `stage` (a string literal), outside any game's frame loop. */
void IntegrationTest_StageProgress(const char* stage);

/**
 * A paired-world creation reported progress (gen_budget.c: Begin, every phase
 * and fill-progress Report, End). The creation event blocks the game thread
 * inside ONE frame for as long as its own wall-clock budget allows (up to 90 s
 * per attempt on a slow host, gen_budget.h), so without this a healthy slow
 * creation would read as a wedged frame. A creation that stops reporting still
 * stalls the word, and the watchdog names it.
 */
void IntegrationTest_CreationProgress(void);

/** The progress word's current value (read by the ROM-free gp-watchdog row). */
uint64_t IntegrationTest_ProgressCount(void);

/** The stage the run was in when it last made progress (a string literal). */
const char* IntegrationTest_ProgressStage(void);

/** The hand-off site of RSBS_INT_WEDGE=handoff (main, before the switch runs). */
void IntegrationTest_HandoffWedgeIfArmed(void);

// ----------------------------------------------------------------------------
// Wall-clock watchdog thread (#793)
//
// Every other int-* watchdog runs from a per-frame hook, so a run that stops
// completing frames (a wedge inside one frame, or in the cross-game hand-off
// between two games' frame loops) used to reach the CTest wall with no line
// saying where. This detached thread, started with the integration mode, polls
// the progress word above. Once RSBS_INT_WATCHDOG_SECS (default 60; 0
// disables, e.g. under a debugger) pass with no progress, it prints the last
// stage, both games' frame counts and each game's state, then ends the process
// with INT_WATCHDOG_EXIT_CODE itself: a wedged frame loop never returns to
// main, so nothing else can.
//
// The default sits well above the longest healthy gap between two progress
// bumps (reported by every run as "[INT-WATCHDOG] longest stall") and, for the
// 120 s rows, far enough below the CTest TIMEOUT that a stall anywhere in a
// healthy run's span fires first.
// ----------------------------------------------------------------------------
#define INT_WATCHDOG_EXIT_CODE 3

/** Arm the watchdog thread (once; reads RSBS_INT_WATCHDOG_SECS). */
void IntegrationTest_WatchdogStart(void);

/** Writes one line of a game's state into `out` (no locks; runs on the watchdog thread). */
typedef void (*IntegrationTestStateDescriber)(char* out, size_t cap);

/** Each game registers its describer when its integration hooks are armed. */
void IntegrationTest_WatchdogSetDescriber(GameId game, IntegrationTestStateDescriber describer);

/** Print the longest stall seen this run and its stage (healthy-run margin evidence). */
void IntegrationTest_WatchdogReport(void);

// ----------------------------------------------------------------------------
// stderr capture (int-paired-first-crossing)
//
// The crossing's verdicts are fprintf(stderr) lines in both ports (the MM
// arrival's `[MM] pairing:` lines, the creation's `[MM] creation:` and
// `[OoT] creation event:` lines). The paired row asserts on those lines rather
// than on a parallel test-only channel, so what it checks is exactly what a
// tester reads. The capture TEES fd 2 through a pipe: every byte still reaches
// the original stderr (the CTest log) as it is written, and lines naming a
// pairing, a creation or a refusal are kept for the assertions.
// ----------------------------------------------------------------------------

/** Start the tee (idempotent). Returns false when the pipe could not be set up. */
bool IntegrationTest_StderrCaptureStart(void);

/** Drain, restore fd 2 and join the reader (idempotent; safe when never started). */
void IntegrationTest_StderrCaptureStop(void);

/**
 * Crash-path restore: point fd 2 back at the original stderr so the crash
 * report is written directly, without joining anything. Async-signal-safe.
 */
void IntegrationTest_StderrCaptureRestoreForCrash(void);

/** Number of kept lines containing `needle` (0 when the capture never started). */
int IntegrationTest_StderrCaptureCount(const char* needle);

/**
 * Copy the LAST kept line containing `needle` into `out` (NUL-terminated,
 * truncated to `cap`). Returns false when no kept line contains it.
 */
bool IntegrationTest_StderrCaptureLast(const char* needle, char* out, size_t cap);

// ----------------------------------------------------------------------------
// The paired world's identity, recorded right after the creation event returns
// (BEFORE the file is loaded back), and compared after the load and at every
// later arrival (int-paired-first-crossing). Read from
// the game-neutral carriers only (gComboCtx and the crossing store), so the
// same comparison runs from either game's driver.
// ----------------------------------------------------------------------------
typedef struct {
    uint32_t masterSeed;       // gComboCtx.sharedRandoSeed
    uint32_t settingsHash;     // gComboCtx.sharedRandoSettingsHash
    uint32_t mmProfileDigest;  // gComboCtx.mmProfileDigest
    uint32_t comboFingerprint; // gComboCtx.comboSettingsHash
    uint32_t crossingDigest;   // Combo_Crossings_Digest()
    int crossingsInHyrule;     // Combo_Crossings_Count(GAME_OOT): MM items in OoT checks
    int crossingsInTermina;    // Combo_Crossings_Count(GAME_MM): OoT items in MM checks
    bool crossingsFrozen;      // Combo_Crossings_IsFrozen()
    bool pairingActive;        // Combo_ForeignPairingActive()
} PairedIdentity;

void IntegrationTest_PairedIdentityCapture(PairedIdentity* out);

/** Record the live identity as the one every later arrival must match. */
void IntegrationTest_PairedIdentityRecord(void);

/** The recorded identity, or NULL before IntegrationTest_PairedIdentityRecord. */
const PairedIdentity* IntegrationTest_PairedIdentityRecorded(void);

/**
 * Compare the live identity against the recorded one. On a mismatch writes a
 * description naming every differing field into `msg` and returns false.
 */
bool IntegrationTest_PairedIdentityMatches(char* msg, size_t cap);

/**
 * MM's generation-dispatch count (MM_Rando_OnSaveInitDispatchCount) right after
 * the creation event, recorded by the OoT driver: the creation dispatches once,
 * and no arrival may dispatch again. Plain storage, so the common layer names no
 * MM symbol.
 */
void IntegrationTest_PairedSetMMGenerationBaseline(uint32_t dispatches);
uint32_t IntegrationTest_PairedMMGenerationBaseline(void);

/** One-line description of an identity, for the log. */
void IntegrationTest_PairedIdentityDescribe(const PairedIdentity* id, char* out, size_t cap);

// ----------------------------------------------------------------------------
// RSBS_PFC_DIVERGE=1 (#804; CTest IntPairedFirstCrossingDiverged): settings
// changed between the creation and the load. After the creation event returns
// the driver takes the combo layer back to the title screen
// (Context_InvalidateSessionOnReturnToTitle, the call title_setup.c makes) and
// moves one MM option (Starting Hearts), one MM trick (the first settable one)
// and one Cross-Game Rule (Goal) through the pages' own writers. The load must
// put the file's values back (#781, PR #787) and toast it; MM then boots for
// real, and its arrival must match with the keys still holding the file's
// values. The three keys are cleared again when the run's verdict is taken
// (IntegrationTest_PairedDivergeCleanup), so the build directory's config is
// left as the shipped-defaults check found it.
// ----------------------------------------------------------------------------

/** True when RSBS_PFC_DIVERGE=1. */
bool IntegrationTest_PairedDiverge(void);

/** Record the file's values, go back to the title, change the three keys.
 *  Returns false with the reason in `msg`. */
bool IntegrationTest_PairedDivergeApply(char* msg, size_t cap);

/** True when all three keys resolve to the file's values; otherwise names each
 *  one that does not, prefixed by `when`, in `msg`. */
bool IntegrationTest_PairedDivergeKeysHoldFile(const char* when, char* msg, size_t cap);

/** True when the load's two restore toasts ("Restored from file:" naming Goal,
 *  and "Restored for Majora's Mask:") were raised; `msg` quotes them or says
 *  which is missing. */
bool IntegrationTest_PairedDivergeLoadToasts(char* msg, size_t cap);

/** Clear the three keys and save the config, when Apply wrote them. */
void IntegrationTest_PairedDivergeCleanup(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_INTEGRATION_TEST_HOOKS_H
