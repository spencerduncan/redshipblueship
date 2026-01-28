/**
 * @file test_runner.cpp
 * @brief Integration test runner for single-executable architecture
 */

#include "test_runner.h"
#include "game_lifecycle.h"
#include "context.h"
#include "entrance.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <atomic>
#include <thread>
#include <chrono>

// ============================================================================
// Internal state
// ============================================================================

namespace {

bool sTestMode = false;
GameId sTargetGame = GAME_NONE;
std::atomic<bool> sBootComplete{false};

constexpr int kBootTimeoutSec = 30;
constexpr int kE2ETimeoutSec  = 60;

// ============================================================================
// Boot helper — init + run game on a background thread, poll sBootComplete
// ============================================================================

TestResult BootGameAndWait(GameId game) {
    sTargetGame = game;
    sBootComplete = false;

    const char* name = (game == GAME_OOT) ? "OoT" : "MM";
    printf("[TEST] Initializing %s...\n", name);

    char arg0[] = "redship";
    char* argv[] = { arg0, nullptr };

    int rc = GameRunner_InitGame(game, 1, argv);
    if (rc != 0) {
        printf("[TEST] FAIL: %s GameRunner_InitGame returned %d\n", name, rc);
        return TEST_FAIL;
    }

    // Run the game loop on a background thread so we can poll for boot
    std::thread gameThread([game]() { GameRunner_RunGame(game); });

    // Poll sBootComplete with timeout
    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::seconds(kBootTimeoutSec);

    while (!sBootComplete.load()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            printf("[TEST] FAIL: %s boot timed out after %ds\n", name, kBootTimeoutSec);
            GameRunner_ShutdownGame(game);
            if (gameThread.joinable()) gameThread.join();
            return TEST_FAIL;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    printf("[TEST] %s boot complete — shutting down\n", name);
    GameRunner_ShutdownGame(game);
    if (gameThread.joinable()) gameThread.join();

    printf("[TEST] PASS: %s booted to main menu\n", name);
    return TEST_PASS;
}

// ============================================================================
// Test implementations
// ============================================================================

TestResult Test_BootOoT(void) {
    printf("[TEST] boot-oot: Boot OoT to main menu\n");
    return BootGameAndWait(GAME_OOT);
}

TestResult Test_BootMM(void) {
    printf("[TEST] boot-mm: Boot MM to main menu\n");
    return BootGameAndWait(GAME_MM);
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
    printf("[TEST] roundtrip: Full round-trip with state verification\n");

    // Initialize systems
    Context_InitFrozenStates();
    Entrance_Init();
    Entrance_RegisterDefaultLinks();

    // Simulate OoT -> MM -> OoT round trip
    // Step 1: OoT triggers switch to MM
    Entrance_CheckCrossGame(GAME_OOT, OOT_ENTR_HAPPY_MASK_SHOP);
    if (!Entrance_IsCrossGameSwitch()) {
        printf("[TEST] FAIL: Step 1 - OoT switch not triggered\n");
        return TEST_FAIL;
    }

    // Simulate freezing OoT state
    uint8_t fakeOoTSave[OOT_SAVE_CONTEXT_SIZE] = {0};
    fakeOoTSave[0] = 0xDE;
    fakeOoTSave[1] = 0xAD;
    Context_FreezeState(GAME_OOT, OOT_ENTR_MARKET_FROM_MASK_SHOP,
                        fakeOoTSave, sizeof(fakeOoTSave));

    Entrance_ClearPendingSwitch();

    // Step 2: MM triggers switch back to OoT
    Entrance_CheckCrossGame(GAME_MM, MM_ENTR_SOUTH_CLOCK_TOWN_0);
    if (!Entrance_IsCrossGameSwitch()) {
        printf("[TEST] FAIL: Step 2 - MM switch not triggered\n");
        return TEST_FAIL;
    }

    // Step 3: Verify OoT state can be restored
    uint8_t restoredSave[OOT_SAVE_CONTEXT_SIZE] = {0};
    if (!Context_RestoreState(GAME_OOT, restoredSave, sizeof(restoredSave))) {
        printf("[TEST] FAIL: Step 3 - OoT state restore failed\n");
        return TEST_FAIL;
    }

    if (restoredSave[0] != 0xDE || restoredSave[1] != 0xAD) {
        printf("[TEST] FAIL: Step 3 - Restored data mismatch\n");
        return TEST_FAIL;
    }

    printf("[TEST] PASS: Round-trip with state preservation verified\n");
    Entrance_ClearPendingSwitch();
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

// ============================================================================
// E2E: Boot OoT, trigger Mido's House entrance, verify cross-game switch
// ============================================================================

TestResult Test_MidosHouseE2E(void) {
    printf("[TEST] midos-house-e2e: Full end-to-end Mido's House test\n");

    // --- Step 1: Boot OoT with test entrance links ---
    Context_InitFrozenStates();
    Entrance_Init();
    Entrance_RegisterTestLinks();

    sTargetGame = GAME_OOT;
    sBootComplete = false;

    char arg0[] = "redship";
    char* bootArgv[] = { arg0, nullptr };

    int rc = GameRunner_InitGame(GAME_OOT, 1, bootArgv);
    if (rc != 0) {
        printf("[TEST] FAIL: OoT GameRunner_InitGame returned %d\n", rc);
        return TEST_FAIL;
    }

    std::thread gameThread([]() { GameRunner_RunGame(GAME_OOT); });

    // Wait for boot
    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::seconds(kBootTimeoutSec);

    while (!sBootComplete.load()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            printf("[TEST] FAIL: OoT boot timed out\n");
            GameRunner_ShutdownGame(GAME_OOT);
            if (gameThread.joinable()) gameThread.join();
            return TEST_FAIL;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    printf("[TEST] OoT boot complete\n");

    // --- Step 2: Trigger Mido's House entrance via the entrance system ---
    // In a live game, the player walks into the entrance and the game calls
    // Combo_CheckEntranceSwitch(OOT_ENTR_MIDOS_HOUSE). We simulate that
    // by calling the cross-game entrance check directly, which is the same
    // codepath the game's entrance override hook invokes.
    printf("[TEST] Triggering Mido's House entrance (0x%04X)...\n", OOT_ENTR_MIDOS_HOUSE);
    Entrance_CheckCrossGame(GAME_OOT, OOT_ENTR_MIDOS_HOUSE);

    if (!Entrance_IsCrossGameSwitch()) {
        printf("[TEST] FAIL: Cross-game switch not triggered\n");
        GameRunner_ShutdownGame(GAME_OOT);
        if (gameThread.joinable()) gameThread.join();
        return TEST_FAIL;
    }

    // --- Step 3: Verify switch targets ---
    if (Entrance_GetSwitchTargetGame() != GAME_MM) {
        printf("[TEST] FAIL: Target game should be MM, got %d\n",
               Entrance_GetSwitchTargetGame());
        GameRunner_ShutdownGame(GAME_OOT);
        if (gameThread.joinable()) gameThread.join();
        return TEST_FAIL;
    }

    if (Entrance_GetSwitchTargetEntrance() != MM_ENTR_CLOCK_TOWER_INTERIOR_1) {
        printf("[TEST] FAIL: Target entrance should be 0x%04X, got 0x%04X\n",
               MM_ENTR_CLOCK_TOWER_INTERIOR_1,
               Entrance_GetSwitchTargetEntrance());
        GameRunner_ShutdownGame(GAME_OOT);
        if (gameThread.joinable()) gameThread.join();
        return TEST_FAIL;
    }

    printf("[TEST] PASS: Mido's House -> Clock Tower (MM) e2e verified\n");

    // --- Cleanup ---
    Entrance_ClearPendingSwitch();
    GameRunner_ShutdownGame(GAME_OOT);
    if (gameThread.joinable()) gameThread.join();

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
    {"boot-oot", "Boot OoT to main menu", Test_BootOoT},
    {"boot-mm", "Boot MM to main menu", Test_BootMM},
    {"switch-oot-mm", "Test game switch OoT -> MM", Test_SwitchOoTMM},
    {"switch-mm-oot", "Test game switch MM -> OoT", Test_SwitchMMOoT},
    {"midos-house", "Test Mido's House entrance (test mode)", Test_MidosHouse},
    {"midos-house-e2e", "E2E: Boot OoT, walk to Mido's House, verify switch", Test_MidosHouseE2E},
    {"startup-entrance", "Test startup entrance flow", Test_StartupEntrance},
    {"roundtrip", "Full round-trip with state verification", Test_Roundtrip},
    {"context", "Test context/state management", Test_Context},
    {nullptr, nullptr, nullptr}  // Sentinel
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
    printf("Available tests:\n\n");
    for (int i = 0; gTests[i].name != nullptr; i++) {
        printf("  %-20s %s\n", gTests[i].name, gTests[i].description);
    }
    printf("\nSpecial commands:\n");
    printf("  %-20s Run all tests\n", "all");
    printf("  %-20s Show this list\n", "list");
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
        sBootComplete.store(true);
    }
}

bool TestRunner_IsTestMode(void) {
    return sTestMode;
}

GameId TestRunner_GetTargetGame(void) {
    return sTargetGame;
}

} // extern "C"
