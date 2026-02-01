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
    INT_TEST_BOOT_OOT,      // Boot OoT, exit on title/file select
    INT_TEST_BOOT_MM,       // Boot MM, exit on title/file select
    INT_TEST_SWITCH_OOT_MM, // Boot OoT, warp, switch to MM
    INT_TEST_SWITCH_MM_OOT  // Boot MM, warp, switch to OoT
} IntegrationTestMode;

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
 * Register hooks for the specified game
 * Should be called after GameInteractor is initialized
 * @param game The game that is being initialized
 */
void IntegrationTest_RegisterHooks(GameId game);

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
 * Get the maximum timeout in milliseconds for boot tests
 */
int IntegrationTest_GetTimeoutMs(void);

/**
 * Request game exit (for use in hooks)
 */
void IntegrationTest_RequestExit(void);

/**
 * Check if exit was requested
 */
bool IntegrationTest_ExitRequested(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_INTEGRATION_TEST_HOOKS_H
