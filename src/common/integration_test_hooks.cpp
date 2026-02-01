/**
 * @file integration_test_hooks.cpp
 * @brief Integration test state management
 *
 * This file provides the state management for integration tests.
 * Hook registration happens in each game's code (OoT/MM) using their
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
IntegrationTestMode sTestMode = INT_TEST_NONE;
std::atomic<bool> sBootPassed{false};
std::atomic<bool> sExitRequested{false};
GameId sBootedGame = GAME_NONE;

// Boot detection timeout (60 seconds default)
constexpr int kBootTimeoutMs = 60000;

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
        case INT_TEST_SWITCH_OOT_MM: modeName = "switch-oot-mm"; break;
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

int IntegrationTest_GetTimeoutMs(void) {
    return kBootTimeoutMs;
}

void IntegrationTest_RequestExit(void) {
    sExitRequested = true;
}

bool IntegrationTest_ExitRequested(void) {
    return sExitRequested.load();
}

void IntegrationTest_RegisterHooks(GameId game) {
    if (!IntegrationTest_IsActive()) {
        return;
    }

    printf("[INT-TEST] Hook registration requested for %s\n", Game_ToString(game));
    printf("[INT-TEST] Note: Each game registers its own hooks in its init code\n");
    fflush(stdout);

    // Actual hook registration happens in game-specific code:
    //   - OoT: games/oot/soh/GameExports_SingleExe.cpp
    //   - MM: games/mm/2s2h/GameExports_SingleExe.cpp
    //
    // This function is called from main.cpp after game init to remind the
    // game to register hooks. In single-exe mode, each game's GameInteractor
    // hooks are registered in their respective OoT_Game_Init / MM_Game_Init.
}

} // extern "C"
