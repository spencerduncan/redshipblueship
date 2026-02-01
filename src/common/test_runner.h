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

/**
 * Check if running Xvfb gameplay test (full SDL loop)
 */
bool TestRunner_IsXvfbTestMode(void);

/**
 * Get the current Xvfb test name (NULL if not in Xvfb mode)
 */
const char* TestRunner_GetXvfbTestName(void);

/**
 * Get the test timeout in seconds (0 = no timeout)
 */
int TestRunner_GetTimeout(void);

/**
 * Parse command line args for Xvfb test mode
 * @param argc Argument count
 * @param argv Argument vector
 * @return Test name if --xvfb-test flag found, NULL otherwise
 */
const char* TestRunner_ParseXvfbArgs(int argc, char** argv);

/**
 * Parse timeout from command line args
 * @param argc Argument count
 * @param argv Argument vector
 * @return Timeout in seconds (0 if not specified)
 */
int TestRunner_ParseTimeoutArg(int argc, char** argv);

/**
 * Initialize Xvfb gameplay test mode
 * Sets up test entrance links and callbacks
 */
void TestRunner_InitXvfbTest(const char* testName, int timeoutSecs);

/**
 * Called by entrance system when a cross-game entrance is triggered
 * Used to verify that entrance hooks fire correctly
 */
void TestRunner_SignalEntranceTriggered(uint16_t entrance, GameId targetGame);

/**
 * Check if the expected entrance was triggered
 */
bool TestRunner_WasEntranceTriggered(void);

/**
 * Get the entrance that was triggered
 */
uint16_t TestRunner_GetTriggeredEntrance(void);

/**
 * Get the target game from the triggered entrance
 */
GameId TestRunner_GetTriggeredTargetGame(void);

/**
 * Main game loop hook - checks test timeout and assertions
 * Returns true if test should continue, false if test should exit
 */
bool TestRunner_XvfbLoopTick(void);

/**
 * Signal that the main menu was reached (for boot tests)
 */
void TestRunner_SignalMainMenuReached(void);

/**
 * Signal that a save was loaded
 */
void TestRunner_SignalSaveLoaded(void);

/**
 * Get the test result after Xvfb test completes
 */
TestResult TestRunner_GetXvfbResult(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_TEST_RUNNER_H
