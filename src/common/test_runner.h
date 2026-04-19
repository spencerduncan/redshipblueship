/**
 * @file test_runner.h
 * @brief Integration test runner for single-executable architecture
 *
 * Provides automated testing capabilities for the unified redship executable.
 * Tests can verify game boot, cross-game switching, and state preservation.
 */

#ifndef RSBS_COMMON_TEST_RUNNER_H
#define RSBS_COMMON_TEST_RUNNER_H

#include "game.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Test result codes
 */
typedef enum {
    TEST_PASS = 0,
    TEST_FAIL = 1,
    TEST_SKIP = 2,
    TEST_ERROR = 3
} TestResult;

/**
 * Test descriptor
 */
typedef struct {
    const char* name;
    const char* description;
    TestResult (*runFunc)(void);
} TestDescriptor;

/**
 * Run a specific test by name
 * @param testName Name of the test to run (e.g., "boot-oot", "boot-mm", "all")
 * @return Exit code (0 for success, failure count for "all", 1 for errors)
 */
int TestRunner_Run(const char* testName);

/**
 * List all available tests
 */
void TestRunner_ListTests(void);

/**
 * Check if test mode is requested in command line args
 * @param argc Argument count
 * @param argv Argument vector
 * @return Test name if --test flag found, NULL otherwise
 */
const char* TestRunner_ParseArgs(int argc, char** argv);

/**
 * Boot test callback - called by game when it reaches main menu
 * This allows the test to know when boot is complete
 */
void TestRunner_SignalBootComplete(GameId game);

/**
 * Check if we're in test mode
 */
bool TestRunner_IsTestMode(void);

/**
 * Get the current test's target game (for boot tests)
 */
GameId TestRunner_GetTargetGame(void);

// ============================================================================
// Integration Test API
// ============================================================================

/**
 * Parse command line args for --integration-test flag
 * @param argc Argument count
 * @param argv Argument vector
 * @return Integration test name if --integration-test flag found, NULL otherwise
 */
const char* TestRunner_ParseIntegrationArgs(int argc, char** argv);

/**
 * Set up an integration test
 * This initializes the integration test mode and registers hooks
 * @param testName Name of the integration test to run
 * @return true if setup succeeded, false if test not found
 */
bool TestRunner_SetupIntegrationTest(const char* testName);

/**
 * Check if we're in integration test mode
 * Integration tests actually boot the game, unlike unit tests
 */
bool TestRunner_IsIntegrationTestMode(void);

/**
 * Get the target game for the current integration test
 * @return Target game, or GAME_NONE if not in integration test mode
 */
GameId TestRunner_GetIntegrationTestGame(void);

/**
 * Get the result of the integration test
 * Should be called after the game exits
 * @return 0 for pass, 1 for fail/error
 */
int TestRunner_GetIntegrationTestResult(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_TEST_RUNNER_H
