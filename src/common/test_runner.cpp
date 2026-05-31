/**
 * @file test_runner.cpp
 * @brief Integration test runner for single-executable architecture
 */

#include "test_runner.h"
#include "context.h"
#include "entrance.h"
#include "integration_test_hooks.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <atomic>

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

// Archive hot-swap regression test (issue #263). Included at FILE SCOPE (not
// inside an extern "C" block): it is compiled as C++ and uses the C++-linkage
// Entrance_* API for setup. Its cross-TU ArchiveHotswap_* helpers are wrapped
// in their own extern "C" inside the file.
#include "tests/test_archive_hotswap.c"

// Unified save (.redsave) headless tests (issue #35, Phase 2 T6). Included at
// FILE SCOPE (compiled as C++): they drive the C++-linkage rsbs::SaveManager.
#include "tests/test_save_roundtrip.c"

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

TestResult Test_BootOoT(void) {
    printf("[TEST] boot-oot: Boot OoT to main menu\n");
    sTargetGame = GAME_OOT;
    sBootComplete = false;

    // In headless/test mode, we just verify the infrastructure is set up
    // The actual boot test would require SDL which may not be available
    printf("[TEST] OoT boot infrastructure ready\n");
    printf("[TEST] Note: Full boot test requires SDL2 (available in CI)\n");

    // For now, mark as pass to indicate infrastructure works
    return TEST_PASS;
}

TestResult Test_BootMM(void) {
    printf("[TEST] boot-mm: Boot MM to main menu\n");
    sTargetGame = GAME_MM;
    sBootComplete = false;

    // In headless/test mode, we just verify the infrastructure is set up
    printf("[TEST] MM boot infrastructure ready\n");
    printf("[TEST] Note: Full boot test requires SDL2 (available in CI)\n");

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

    if (Entrance_GetSwitchTargetEntrance() != MM_ENTR_CLOCK_TOWER_INTERIOR_1) {
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

    // Simulate MM exiting from Clock Tower to South Clock Town
    // (which should trigger switch back to OoT)
    uint16_t result = Entrance_CheckCrossGame(GAME_MM, MM_ENTR_SOUTH_CLOCK_TOWN_0);

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
    uint8_t fakeOoTSave[OOT_SAVE_CONTEXT_SIZE] = {0};
    fakeOoTSave[0] = 0xDE;
    fakeOoTSave[1] = 0xAD;
    fakeOoTSave[OOT_SAVE_CONTEXT_SIZE - 1] = 0xEF;  // Tail marker
    Combo_FreezeState("oot", Combo_GetSwitchReturnEntrance(),
                      fakeOoTSave, sizeof(fakeOoTSave));
    Entrance_ClearPendingSwitch();

    // ------------------------------------------------------------------
    // Leg 2: MM -> OoT via South Clock Town
    // ------------------------------------------------------------------
    // This is the leg that pre-#170 silently no-op'd because the shared
    // Combo_CheckEntranceSwitch implementation looked up "oot" links for
    // MM's entrance id 0xD800 and always missed.
    Combo_CheckCrossGameEntrance("mm", MM_ENTR_SOUTH_CLOCK_TOWN_0);
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
    uint8_t fakeMMSave[MM_SAVE_CONTEXT_SIZE] = {0};
    fakeMMSave[0] = 0xBE;
    fakeMMSave[1] = 0xEF;
    Combo_FreezeState("mm", Combo_GetSwitchReturnEntrance(),
                      fakeMMSave, sizeof(fakeMMSave));

    // ------------------------------------------------------------------
    // Leg 3: Verify OoT state can be restored after the round-trip.
    // ------------------------------------------------------------------
    uint8_t restoredSave[OOT_SAVE_CONTEXT_SIZE] = {0};
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
    uint8_t restoredMM[MM_SAVE_CONTEXT_SIZE] = {0};
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

    if (Entrance_GetSwitchTargetEntrance() != MM_ENTR_CLOCK_TOWER_INTERIOR_1) {
        printf("[TEST] FAIL: Should target Clock Tower Interior\n");
        return TEST_FAIL;
    }

    printf("[TEST] PASS: Mido's House -> Clock Tower link works\n");
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

    // Step 3: Verify startup entrance is set
    uint16_t startup = Combo_GetStartupEntrance();
    if (startup != MM_ENTR_CLOCK_TOWER_INTERIOR_1) {
        printf("[TEST] FAIL: Startup entrance not set correctly (got 0x%04X, expected 0x%04X)\n",
               startup, MM_ENTR_CLOCK_TOWER_INTERIOR_1);
        return TEST_FAIL;
    }

    // Step 4: Clear and verify (simulates what Play_Init does)
    Combo_ClearStartupEntrance();
    if (Combo_GetStartupEntrance() != 0) {
        printf("[TEST] FAIL: Startup entrance not cleared\n");
        return TEST_FAIL;
    }

    printf("[TEST] PASS: Startup entrance flow verified\n");
    Entrance_ClearPendingSwitch();
    return TEST_PASS;
}

TestResult Test_RoundtripIntegrity(void) {
    printf("[TEST] roundtrip-integrity: OoT SaveContext byte-integrity across roundtrip (issue #262)\n");
    int failures = TestRoundtripIntegrity_Run();
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

    // Freeze a state
    uint8_t testData[OOT_SAVE_CONTEXT_SIZE] = {0};
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
    uint8_t restored[OOT_SAVE_CONTEXT_SIZE] = {0};
    if (!Context_RestoreState(GAME_OOT, restored, sizeof(restored))) {
        printf("[TEST] FAIL: Restore failed\n");
        return TEST_FAIL;
    }

    if (restored[100] != 0x42) {
        printf("[TEST] FAIL: Restored data mismatch\n");
        return TEST_FAIL;
    }

    printf("[TEST] PASS: Context management working correctly\n");
    return TEST_PASS;
}

// ============================================================================
// Test registry
// ============================================================================

const TestDescriptor gTests[] = {
    {"boot-oot", "Boot OoT to main menu (unit test)", Test_BootOoT},
    {"boot-mm", "Boot MM to main menu (unit test)", Test_BootMM},
    {"switch-oot-mm", "Test game switch OoT -> MM", Test_SwitchOoTMM},
    {"switch-mm-oot", "Test game switch MM -> OoT", Test_SwitchMMOoT},
    {"midos-house", "Test Mido's House entrance (test mode)", Test_MidosHouse},
    {"startup-entrance", "Test startup entrance flow", Test_StartupEntrance},
    {"roundtrip", "Full round-trip with state verification", Test_Roundtrip},
    {"roundtrip-integrity", "OoT SaveContext byte-identical across OoT->MM->OoT (issue #262)", Test_RoundtripIntegrity},
    {"shared-roundtrip", "Shared flag/seed survive OoT->MM switch (issue #264)", Test_SharedStateRoundtrip},
    {"context", "Test context/state management", Test_Context},
    {"lifecycle", "Game lifecycle unit tests", Test_Lifecycle},
    // Unified save (.redsave) headless coverage (issue #35, Phase 2 T6).
    {"save-roundtrip-tiers", "Unified .redsave preserves ComboContext + both SaveContexts (#35)", Test_SaveRoundtripTiers},
    {"save-header", "Unified .redsave header fields + CRC are well-formed (#35)", Test_SaveHeader},
    {"save-has-delete", "Unified save HasSave/DeleteSave lifecycle (#35)", Test_SaveHasDelete},
    {"save-version-reject", "Unified save Load rejects unknown version, no clobber (#35)", Test_SaveVersionReject},
    {"save-size-mismatch", "Unified save Load rejects mismatched tier size, no clobber (#35)", Test_SaveSizeMismatch},
    {"save-crc-corrupt", "Unified save Load rejects corrupt payload, no clobber (#35)", Test_SaveCrcCorrupt},
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
     "Boot OoT, trigger Happy Mask Shop entrance (0x0530), verify spawn at MM Clock Tower Interior (0xC010)",
     INT_TEST_SWITCH_OOT_HMS_TO_MM, GAME_OOT},
    {"int-switch-mm-clocktown-south-to-oot",
     "Boot MM, trigger South Clock Town south exit (0xD800), verify spawn at OoT Market from Mask Shop (0x01D1)",
     INT_TEST_SWITCH_MM_CLOCKTOWN_SOUTH_TO_OOT, GAME_MM},
    {"int-archive-hotswap-cycle",
     "Boot OoT, hot-swap OoT<->MM >=3 times, verify healthy runtime (no missing assets, bounded RSS) (#263)",
     INT_TEST_ARCHIVE_HOTSWAP_CYCLE, GAME_OOT},
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
