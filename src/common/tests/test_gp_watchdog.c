/**
 * Gameplay round-trip phase-watchdog regression test (#376 item 4).
 *
 * The OoT-side watchdog (games/oot/soh/GameExports_SingleExe.cpp) exists to
 * fail a wedged round-trip run loudly, WITH state, before the CTest/`timeout`
 * wall clock kills the process silently. It used to budget in FRAMES
 * (maxPhaseFrames * 4 + 3600 == 4080 at defaults), which under Xvfb + llvmpipe
 * is 300-800 s of wall clock — longer than the 300 s IntGameplayRoundtrip
 * TIMEOUT — so it could never fire first and the diagnostic dump never emitted.
 * A budget that reads as coverage but cannot fire is a vacuous gate.
 *
 * The budget is now WALL-CLOCK seconds. The decision was factored into two pure
 * helpers so the frame-vs-wall-clock fix is regression-locked with no display,
 * no ROM archives, and no game loop:
 *   - IntegrationTest_GameplayWatchdogParse  (env -> budget seconds)
 *   - IntegrationTest_GameplayWatchdogExpired (elapsed >= budget, wall clock)
 *
 * The round-trip itself is ROM-gated and cannot run in hosted CI, so this is
 * the only tier that can prove the watchdog is able to fire before the timeout.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++); the helpers are
 * already declared via its include of integration_test_hooks.h.
 */

#include <cstdio>

// The default budget, restated independently as the contract (the source
// spells it kGpWatchdogDefaultSecs in integration_test_hooks.cpp).
#define GPWD_DEFAULT_SECS 60

// The IntGameplayRoundtrip CTest TIMEOUT (CMake/SingleExecutable.cmake,
// REDSHIP_GAMEPLAY_TEST_TIMEOUT default). The whole point of item 4: the
// watchdog budget must stay comfortably under this so its dump beats the kill.
#define GPWD_INTEGRATION_TIMEOUT_SECS 300

#define GPWD_CHECK(cond, msg)                   \
    do {                                        \
        if (!(cond)) {                          \
            printf("[TEST] FAIL: %s\n", (msg)); \
            return TEST_FAIL;                   \
        }                                       \
    } while (0)

TestResult Test_GpWatchdog(void) {
    printf("[TEST] gp-watchdog: phase watchdog is wall-clock budgeted and can fire before the timeout (#376)\n");

    // (1) The core defect. The budget must be WALL-CLOCK seconds and it must be
    // strictly less than the CTest timeout, or the watchdog can never emit its
    // diagnostic before the hard kill. The frame budget failed exactly this.
    int budget = IntegrationTest_GameplayWatchdogParse(NULL);
    GPWD_CHECK(budget == GPWD_DEFAULT_SECS, "default watchdog budget is not 60 s");
    GPWD_CHECK(budget > 0, "default watchdog budget is non-positive (watchdog disabled)");
    GPWD_CHECK(budget < GPWD_INTEGRATION_TIMEOUT_SECS,
               "default watchdog budget is not under the IntGameplayRoundtrip TIMEOUT — cannot fire first");

    // (2) The fire predicate is a wall-clock >= comparison, NOT a frame count.
    // Below budget it must not fire; at or past the budget it must.
    GPWD_CHECK(!IntegrationTest_GameplayWatchdogExpired(0.0, budget), "watchdog fired at 0 s elapsed");
    GPWD_CHECK(!IntegrationTest_GameplayWatchdogExpired((double)budget - 0.1, budget),
               "watchdog fired just before its budget");
    GPWD_CHECK(IntegrationTest_GameplayWatchdogExpired((double)budget, budget),
               "watchdog did not fire exactly at its budget");
    GPWD_CHECK(IntegrationTest_GameplayWatchdogExpired((double)budget + 0.1, budget),
               "watchdog did not fire past its budget");
    // A run that wall-clock-exceeds the whole integration timeout must be well
    // past a 60 s phase budget — the property the frame budget could not give.
    GPWD_CHECK(IntegrationTest_GameplayWatchdogExpired((double)GPWD_INTEGRATION_TIMEOUT_SECS, budget),
               "watchdog did not fire after a timeout's worth of wall clock");

    // (3) A non-positive budget disables the watchdog rather than firing every
    // tick (defensive: the parser floors at 1, but the predicate is public).
    GPWD_CHECK(!IntegrationTest_GameplayWatchdogExpired(1000.0, 0), "zero budget fired");
    GPWD_CHECK(!IntegrationTest_GameplayWatchdogExpired(1000.0, -5), "negative budget fired");

    // (4) Env override + validation. A valid value is honored; anything the
    // strtol path rejects (garbage, trailing junk, below the 1 s minimum, empty)
    // falls back to the default so a fat-fingered knob cannot silently disable
    // or hair-trigger the watchdog.
    GPWD_CHECK(IntegrationTest_GameplayWatchdogParse("5") == 5, "valid override '5' not honored");
    GPWD_CHECK(IntegrationTest_GameplayWatchdogParse("240") == 240, "valid override '240' not honored");
    GPWD_CHECK(IntegrationTest_GameplayWatchdogParse("0x1E") == 30, "hex override '0x1E' not honored");
    GPWD_CHECK(IntegrationTest_GameplayWatchdogParse("") == GPWD_DEFAULT_SECS, "empty override not defaulted");
    GPWD_CHECK(IntegrationTest_GameplayWatchdogParse("0") == GPWD_DEFAULT_SECS, "below-minimum '0' not defaulted");
    GPWD_CHECK(IntegrationTest_GameplayWatchdogParse("-3") == GPWD_DEFAULT_SECS, "negative override not defaulted");
    GPWD_CHECK(IntegrationTest_GameplayWatchdogParse("60s") == GPWD_DEFAULT_SECS, "trailing junk not defaulted");
    GPWD_CHECK(IntegrationTest_GameplayWatchdogParse("abc") == GPWD_DEFAULT_SECS, "non-numeric override not defaulted");

    printf("[TEST] PASS: watchdog budget is wall-clock, bounded under the timeout, and validated\n");
    return TEST_PASS;
}

#undef GPWD_CHECK
