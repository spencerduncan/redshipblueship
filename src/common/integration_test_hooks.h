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
} GameplayTestConfig;

/**
 * Initialize integration test mode
 * Should be called before game initialization
 * @param mode The integration test mode to run
 */
void IntegrationTest_SetMode(IntegrationTestMode mode);

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

#ifdef __cplusplus
}
#endif

#endif // RSBS_INTEGRATION_TEST_HOOKS_H
