/**
 * @file integration_test_hooks.cpp
 * @brief Integration test state management
 *
 * This file provides the state management for integration tests.
 * Hook registration happens in each game's init code (OoT/MM) using their
 * respective GameInteractor implementations.
 *
 * See:
 *   - games/oot/soh/GameExports_SingleExe.cpp: OoT_RegisterIntegrationTestHooks()
 *   - games/mm/2s2h/GameExports_SingleExe.cpp: MM_RegisterIntegrationTestHooks()
 */

#include "integration_test_hooks.h"
#include <cstdio>
#include <cstdlib>
#include <atomic>

// Game switch request (to signal game to exit)
extern "C" {
    void Combo_RequestGameSwitch(void);
}

namespace {

// Integration test state
std::atomic<IntegrationTestMode> sTestMode{INT_TEST_NONE};
std::atomic<bool> sBootPassed{false};
std::atomic<bool> sExitRequested{false};
std::atomic<GameId> sBootedGame{GAME_NONE};

} // anonymous namespace

extern "C" {

void IntegrationTest_SetMode(IntegrationTestMode mode) {
    sTestMode = mode;
    sBootPassed = false;
    sExitRequested = false;
    sBootedGame = GAME_NONE;

    const char* modeName = "unknown";
    switch (mode) {
        case INT_TEST_NONE: modeName = "none"; break;
        case INT_TEST_BOOT_OOT: modeName = "boot-oot"; break;
        case INT_TEST_BOOT_MM: modeName = "boot-mm"; break;
        case INT_TEST_SWITCH_OOT_HMS_TO_MM: modeName = "switch-oot-hms-to-mm"; break;
        case INT_TEST_SWITCH_MM_OOT: modeName = "switch-mm-oot"; break;
    }

    if (mode != INT_TEST_NONE) {
        printf("[INT-TEST] Integration test mode: %s\n", modeName);
    }
}

IntegrationTestMode IntegrationTest_GetMode(void) {
    return sTestMode;
}

bool IntegrationTest_IsActive(void) {
    return sTestMode != INT_TEST_NONE;
}

bool IntegrationTest_BootPassed(void) {
    return sBootPassed.load();
}

void IntegrationTest_SignalBootComplete(GameId game, const char* reason) {
    printf("[INT-TEST] Boot complete: %s (%s)\n",
           Game_ToString(game), reason);
    fflush(stdout);
    sBootPassed = true;
    sBootedGame = game;
    sExitRequested = true;

    // Signal the game to exit by requesting a "switch"
    // This causes the game loop to exit cleanly
    printf("[INT-TEST] Requesting game exit...\n");
    fflush(stdout);
    Combo_RequestGameSwitch();
}

void IntegrationTest_RequestExit(void) {
    sExitRequested = true;
}

bool IntegrationTest_ExitRequested(void) {
    return sExitRequested.load();
}

} // extern "C"
