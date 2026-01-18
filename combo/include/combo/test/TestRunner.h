/**
 * TestRunner - Built-in test infrastructure for redship
 *
 * Provides automated testing of game boot, switching, and state preservation.
 * Tests run headless (no GPU/audio) using SDL dummy drivers.
 *
 * Usage:
 *   redship --test boot-oot        # Boot OoT to main menu
 *   redship --test boot-mm         # Boot MM to main menu
 *   redship --test switch-oot-mm   # Test game switch OoT -> MM
 *   redship --test switch-mm-oot   # Test game switch MM -> OoT
 *   redship --test roundtrip       # Full round-trip with state verification
 *   redship --test all             # Run all tests (exit code = failure count)
 *   redship --test list            # List available tests (exit code = 0)
 *
 * Exit Codes (for CI integration):
 *   0     = All tests passed, OR 'list' command was executed
 *   N > 0 = N tests failed (when running 'all')
 *   1     = Single test failed (when running specific test)
 */

#pragma once

#include <functional>
#include <string>
#include <vector>
#include <cstdint>

#include "combo/CrossGameEntrance.h"  // For Game enum

namespace Combo {

/**
 * Minimal state snapshot for test verification.
 * Only includes data needed to verify test outcomes.
 */
struct TestableState {
    float playerPosX;
    float playerPosY;
    float playerPosZ;
    int16_t playerYaw;
    uint16_t sceneId;
    uint16_t entranceIndex;
    Game currentGame;
    bool isValid;
};

/**
 * Test result for a single test case.
 */
struct TestResult {
    std::string testName;
    bool passed;
    std::string failureReason;
    double durationSeconds;
};

// Forward declaration for test function signature
class TestRunner;

/**
 * Test case registration entry.
 * Uses a registration pattern to prevent test list/implementation desync.
 */
struct TestCase {
    std::string name;
    std::function<bool(TestRunner&)> fn;
};

/**
 * TestRunner - drives game execution for automated testing.
 *
 * The TestRunner initializes games in headless mode and can drive
 * the main loop, inject inputs, and verify state.
 */
class TestRunner {
public:
    TestRunner();
    ~TestRunner();

    // ========================================================================
    // Main Entry Points
    // ========================================================================

    /**
     * Run a specific test by name.
     * @param testName One of: boot-oot, boot-mm, switch-oot-mm, switch-mm-oot, roundtrip
     * @return true if test passed, false otherwise
     */
    bool RunTest(const std::string& testName);

    /**
     * Run all tests.
     * @return Number of failed tests (0 = all passed)
     */
    int RunAllTests();

    /**
     * List available test names.
     * Derived from the registered test cases to ensure sync.
     */
    static std::vector<std::string> GetAvailableTests();

    /**
     * Get the registered test cases.
     * Uses a registration pattern to prevent test list/implementation desync.
     */
    static const std::vector<TestCase>& GetRegisteredTests();

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * Enable/disable headless mode. Default: true.
     * When headless, sets SDL_VIDEODRIVER=dummy and SDL_AUDIODRIVER=dummy.
     */
    void SetHeadless(bool headless) { m_headless = headless; }

    /**
     * Enable/disable verbose logging. Default: false.
     */
    void SetVerbose(bool verbose) { m_verbose = verbose; }

    // ========================================================================
    // Test Results
    // ========================================================================

    /**
     * Get results from the last run.
     */
    const std::vector<TestResult>& GetResults() const { return m_results; }

    /**
     * Print summary of test results to stdout.
     */
    void PrintSummary() const;

private:
    // ========================================================================
    // Individual Tests
    // ========================================================================

    bool TestBootGame(Game game);
    bool TestGameSwitch(Game from, Game to);
    bool TestRoundTrip();

    // ========================================================================
    // Test Helpers
    // ========================================================================

    /**
     * Initialize a game in headless mode.
     */
    void InitGame(Game game);

    /**
     * Run game frames until condition is true or timeout.
     * @param condition Condition to wait for
     * @param timeoutFrames Max frames to run (at 60fps)
     * @return true if condition was met, false if timeout
     */
    bool RunUntil(std::function<bool()> condition, int timeoutFrames);

    /**
     * Run a specific number of frames.
     */
    void RunFrames(int count);

    /**
     * Trigger an entrance transition.
     * This is a direct state manipulation, faster and more reliable than
     * physically navigating to the entrance.
     */
    void TriggerEntrance(uint16_t entrance);

    /**
     * Capture current game state for verification.
     */
    TestableState CaptureState();

    // ========================================================================
    // State Queries
    // ========================================================================

    bool IsAtMainMenu();
    bool IsGameplayActive();
    Game GetCurrentGame();

    // ========================================================================
    // Logging
    // ========================================================================

    void Log(const char* fmt, ...);
    void Pass(const std::string& testName);
    void Fail(const std::string& testName, const std::string& reason);

    // ========================================================================
    // Member Data
    // ========================================================================

    bool m_headless = true;
    bool m_verbose = false;
    bool m_initialized = false;
    std::vector<TestResult> m_results;
};

/**
 * Parse --test argument and run appropriate tests.
 *
 * @param testArg The argument after --test (e.g., "boot-oot", "all")
 * @return Exit code for CI integration:
 *         - 0: All tests passed, or 'list' command executed
 *         - N > 0: N tests failed (for 'all' command)
 *         - 1: Single test failed (for specific test name)
 */
int RunTestMode(const std::string& testArg);

/**
 * Set up headless environment for testing.
 * Sets SDL_VIDEODRIVER=dummy and SDL_AUDIODRIVER=dummy.
 * Must be called before SDL_Init().
 */
void SetupHeadlessEnvironment();

} // namespace Combo
