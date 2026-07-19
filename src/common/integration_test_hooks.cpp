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
#include <cstring>
#include <atomic>

// Game switch request (to signal game to exit)
extern "C" {
    void Combo_RequestGameSwitch(void);

    // Archive hot-swap cycle helpers (defined in src/common/tests/test_archive_hotswap.c,
    // compiled into redship_common via test_runner.cpp). Reset the cycle counters
    // when the archive-hotswap integration test mode is selected (#263).
    void ArchiveHotswap_ResetCycle(void);
}

namespace {

// Integration test state
std::atomic<IntegrationTestMode> sTestMode{INT_TEST_NONE};
std::atomic<bool> sBootPassed{false};
std::atomic<bool> sExitRequested{false};
std::atomic<GameId> sBootedGame{GAME_NONE};

// Gameplay round-trip state (INT_TEST_GAMEPLAY_ROUNDTRIP)
std::atomic<GameplayPhase> sGameplayPhase{GP_PHASE_BOOT};
std::atomic<int> sGameplayCyclesDone{0};
GameplayTestConfig sGameplayConfig = {};

const char* GameplayPhaseName(GameplayPhase phase) {
    switch (phase) {
        case GP_PHASE_BOOT:          return "boot";
        case GP_PHASE_OOT_PRE:       return "oot-pre-switch";
        case GP_PHASE_MM_STABILIZE:  return "mm-stabilize";
        case GP_PHASE_MM_PLAY:       return "mm-play";
        case GP_PHASE_OOT_RETURN:    return "oot-return";
        case GP_PHASE_OOT_WARP:      return "oot-warp";
        case GP_PHASE_OOT_EXIT:      return "oot-exit";
        case GP_PHASE_DONE:          return "done";
    }
    return "unknown";
}

// OoT's gEntranceTable bound (z64scene.h ENTR_MAX). Kept as a literal because
// src/common cannot include OoT headers; the OoT-side driver re-checks against
// the real ENTR_MAX at consumption time.
constexpr unsigned long kOoTEntranceMax = 0x0614;

int GameplayEnvInt(const char* name, int defaultValue, int minValue) {
    const char* raw = std::getenv(name);
    if (raw == nullptr || raw[0] == '\0') {
        return defaultValue;
    }
    char* end = nullptr;
    long value = strtol(raw, &end, 0);
    if (end == raw || *end != '\0' || value < minValue) {
        fprintf(stderr, "[GP-TEST] WARNING: ignoring invalid %s='%s' (using %d)\n", name, raw, defaultValue);
        return defaultValue;
    }
    return (int)value;
}

uint16_t GameplayEnvEntrance(const char* name, uint16_t defaultValue) {
    const char* raw = std::getenv(name);
    if (raw == nullptr || raw[0] == '\0') {
        return defaultValue;
    }
    char* end = nullptr;
    unsigned long value = strtoul(raw, &end, 0);
    if (end == raw || *end != '\0' || value >= kOoTEntranceMax) {
        fprintf(stderr, "[GP-TEST] WARNING: ignoring invalid %s='%s' (must be < 0x%04lX; using 0x%04X)\n",
                name, raw, kOoTEntranceMax, defaultValue);
        return defaultValue;
    }
    return (uint16_t)value;
}

void GameplayParseConfig(void) {
    sGameplayConfig.framesPerPhase = GameplayEnvInt("RSBS_GP_FRAMES", 120, 1);
    sGameplayConfig.cycles = GameplayEnvInt("RSBS_GP_CYCLES", 1, 1);
    // Defaults: boot outside the Happy Mask Shop (the return-leg spawn), warp
    // to map-select Market (ENTR_MARKET_SOUTH_EXIT), exit through Market's
    // south gate (ENTR_MARKET_ENTRANCE_NORTH_EXIT).
    sGameplayConfig.bootEntrance = GameplayEnvEntrance("RSBS_GP_BOOT_ENTRANCE", 0x01D1);
    sGameplayConfig.warpEntrance = GameplayEnvEntrance("RSBS_GP_WARP_ENTRANCE", 0x00B1);
    sGameplayConfig.exitEntrance = GameplayEnvEntrance("RSBS_GP_EXIT_ENTRANCE", 0x0033);
    const char* bootAge = getenv("RSBS_GP_BOOT_AGE");
    sGameplayConfig.bootAdult = bootAge != NULL && strcmp(bootAge, "adult") == 0;
    // Warp-phase-only frame budget: a time-related fault (bug 1c class) needs
    // a long soak INSIDE the warp target, not longer windows everywhere.
    sGameplayConfig.warpFrames = GameplayEnvInt("RSBS_GP_WARP_FRAMES", sGameplayConfig.framesPerPhase, 1);
    sGameplayConfig.cameraAssert = GameplayEnvInt("RSBS_GP_CAMERA_ASSERT", 1, 0);
    sGameplayPhase = GP_PHASE_BOOT;
    sGameplayCyclesDone = 0;
    printf("[GP-TEST] config: frames/phase=%d cycles=%d boot=0x%04X warp=0x%04X exit=0x%04X bootAge=%s "
           "warpFrames=%d camAssert=%d\n",
           sGameplayConfig.framesPerPhase, sGameplayConfig.cycles, sGameplayConfig.bootEntrance,
           sGameplayConfig.warpEntrance, sGameplayConfig.exitEntrance,
           sGameplayConfig.bootAdult ? "adult" : "child", sGameplayConfig.warpFrames,
           sGameplayConfig.cameraAssert);
    fflush(stdout);
}

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
        case INT_TEST_SWITCH_MM_CLOCKTOWN_SOUTH_TO_OOT: modeName = "switch-mm-clocktown-south-to-oot"; break;
        case INT_TEST_ARCHIVE_HOTSWAP_CYCLE: modeName = "archive-hotswap-cycle"; break;
        case INT_TEST_GAMEPLAY_ROUNDTRIP: modeName = "gameplay-roundtrip"; break;
    }

    // Gameplay round-trip: parse env parameters and reset the phase machine.
    if (mode == INT_TEST_GAMEPLAY_ROUNDTRIP) {
        GameplayParseConfig();
    }

    // Archive hot-swap cycle keeps a running arrival count / RSS baseline across
    // multiple OoT<->MM switches; reset it whenever this mode is (re)selected (#263).
    if (mode == INT_TEST_ARCHIVE_HOTSWAP_CYCLE) {
        ArchiveHotswap_ResetCycle();
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

// ============================================================================
// Gameplay round-trip repro (INT_TEST_GAMEPLAY_ROUNDTRIP)
// ============================================================================

GameplayPhase IntegrationTest_GetGameplayPhase(void) {
    return sGameplayPhase.load();
}

void IntegrationTest_SetGameplayPhase(GameplayPhase phase) {
    GameplayPhase previous = sGameplayPhase.exchange(phase);
    if (previous != phase) {
        printf("[GP-TEST] phase: %s -> %s\n", GameplayPhaseName(previous), GameplayPhaseName(phase));
        fflush(stdout);
    }
}

const GameplayTestConfig* IntegrationTest_GetGameplayConfig(void) {
    return &sGameplayConfig;
}

int IntegrationTest_GameplayCyclesDone(void) {
    return sGameplayCyclesDone.load();
}

void IntegrationTest_GameplayRecordCycle(void) {
    int done = ++sGameplayCyclesDone;
    printf("[GP-TEST] round-trip %d/%d complete\n", done, sGameplayConfig.cycles);
    fflush(stdout);
}

void IntegrationTest_LogGameplayState(const char* tag) {
    fprintf(stderr,
            "[GP-TEST] state (%s): phase=%s cycles=%d/%d frames/phase=%d "
            "boot=0x%04X warp=0x%04X exit=0x%04X\n",
            tag ? tag : "-", GameplayPhaseName(sGameplayPhase.load()), sGameplayCyclesDone.load(),
            sGameplayConfig.cycles, sGameplayConfig.framesPerPhase, sGameplayConfig.bootEntrance,
            sGameplayConfig.warpEntrance, sGameplayConfig.exitEntrance);
    fflush(stderr);
}

void IntegrationTest_GameplayFail(const char* reason) {
    fprintf(stderr, "[GP-TEST] FAIL: %s\n", reason ? reason : "(no reason)");
    IntegrationTest_LogGameplayState("fail");
    // RequestExit does NOT set the pass flag, so the run returns non-zero;
    // Combo_RequestGameSwitch unblocks the main loop promptly (#263 pattern).
    IntegrationTest_RequestExit();
    Combo_RequestGameSwitch();
}

} // extern "C"
