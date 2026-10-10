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

    // (5) #793: a paired-world creation blocks the game thread inside ONE frame
    // for as long as its own wall-clock budget allows (gen_budget.h: up to 90 s
    // per attempt on a slow host), which the int-* wall-clock watchdog thread's
    // 60 s default would read as a wedged frame. Each creation-progress report
    // (Begin, every Report, End) must therefore advance the progress word that
    // thread polls, and name the creation as the stage. Observed without it: a
    // healthy IntPairedFirstCrossing pinned to one core failed with
    // "[INT-WATCHDOG] FAIL ... no progress for 60.1 s ... last stage: OoT frame"
    // in the middle of the production creation.
    Combo_GenBudget_SetHostScalePercentOverride(100);
    uint64_t before = IntegrationTest_ProgressCount();
    Combo_GenProgress_Begin();
    uint64_t afterBegin = IntegrationTest_ProgressCount();
    Combo_GenProgress_Report(RSBS_GENPHASE_MM_FILL, 1, "gp-watchdog: a fill-progress report");
    uint64_t afterReport = IntegrationTest_ProgressCount();
    const char* stage = IntegrationTest_ProgressStage();
    Combo_GenProgress_End(true);
    uint64_t afterEnd = IntegrationTest_ProgressCount();
    Combo_GenBudget_SetHostScalePercentOverride(0);
    GPWD_CHECK(afterBegin > before, "Combo_GenProgress_Begin did not advance the #793 progress word");
    GPWD_CHECK(afterReport > afterBegin, "Combo_GenProgress_Report did not advance the #793 progress word");
    GPWD_CHECK(afterEnd > afterReport, "Combo_GenProgress_End did not advance the #793 progress word");
    GPWD_CHECK(stage != NULL && strstr(stage, "paired world creation") != NULL,
               "a creation-progress report did not name the creation as the watchdog's stage");

    // (6) #882: the timer measures a STALL, not a phase's length. Observed: a
    // 700-frame warp phase on a loaded host (about 11.6 frames per second) failed
    // with "[GP-TEST] OoT watchdog: no progress for 60.0 s (budget 60 s) in
    // phase 5 (... gameplayFrames=683)" while its frames were still completing,
    // because the timer re-based only when the phase changed. Each gameplay frame
    // of the phase is progress; a phase with no frames still fails at the budget,
    // and a phase that keeps advancing still meets a hard ceiling.
    {
        const int warp = (int)GP_PHASE_OOT_WARP;
        GameplayWatchdogTimer t;

        // (6a) The #882 run: 700 frames at 11.6 fps (about 60.3 s, past the
        // budget), ticked every frame, must never trip.
        IntegrationTest_GameplayWatchdogRebase(&t, warp, 0, 1000.0);
        for (int f = 1; f <= 700; f++) {
            double now = 1000.0 + (double)f / 11.6;
            GPWD_CHECK(IntegrationTest_GameplayWatchdogTick(&t, warp, f, now, budget) == GP_WATCHDOG_OK,
                       "a phase still completing frames (#882: 700 frames at 11.6 fps) tripped the watchdog");
        }
        // (6b) Slower still: one frame every (budget - 1) s, three gaps (177 s
        // of wall clock at the default budget, under the ceiling), each gap
        // under the budget: never trips.
        IntegrationTest_GameplayWatchdogRebase(&t, warp, 0, 2000.0);
        for (int f = 1; f <= 3; f++) {
            double now = 2000.0 + (double)f * (double)(budget - 1);
            GPWD_CHECK(IntegrationTest_GameplayWatchdogTick(&t, warp, f, now, budget) == GP_WATCHDOG_OK,
                       "a phase advancing one frame per (budget - 1) s tripped the watchdog");
        }

        // (6c) A phase with NO frames (a wedged scene load) still fails at the
        // budget, measured from the phase's start.
        IntegrationTest_GameplayWatchdogRebase(&t, warp, 0, 3000.0);
        GPWD_CHECK(IntegrationTest_GameplayWatchdogTick(&t, warp, 0, 3000.0 + budget - 0.1, budget) == GP_WATCHDOG_OK,
                   "a frameless phase tripped before its budget");
        GPWD_CHECK(IntegrationTest_GameplayWatchdogTick(&t, warp, 0, 3000.0 + budget, budget) ==
                       GP_WATCHDOG_STALLED,
                   "a phase with no frames did not trip the watchdog at its budget");

        // (6d) Frames that stop: the budget runs from the LAST frame.
        IntegrationTest_GameplayWatchdogRebase(&t, warp, 0, 4000.0);
        GPWD_CHECK(IntegrationTest_GameplayWatchdogTick(&t, warp, 50, 4030.0, budget) == GP_WATCHDOG_OK,
                   "a phase completing frames tripped the watchdog");
        GPWD_CHECK(IntegrationTest_GameplayWatchdogTick(&t, warp, 50, 4030.0 + budget - 0.1, budget) ==
                       GP_WATCHDOG_OK,
                   "a phase tripped before a budget had passed since its last frame");
        GPWD_CHECK(IntegrationTest_GameplayWatchdogTick(&t, warp, 50, 4030.0 + budget, budget) ==
                       GP_WATCHDOG_STALLED,
                   "a phase whose frames stopped did not trip a budget after its last frame");

        // (6e) A phase change is progress too, with no frame counted yet. The
        // tick lands past a whole budget since the previous phase's last
        // progress with the frame count unchanged, so only the phase-change
        // re-base keeps it OK.
        IntegrationTest_GameplayWatchdogRebase(&t, (int)GP_PHASE_OOT_RETURN, 0, 5000.0);
        GPWD_CHECK(IntegrationTest_GameplayWatchdogTick(&t, warp, 0, 5000.0 + budget + 1.0, budget) ==
                       GP_WATCHDOG_OK,
                   "a phase change did not re-base the watchdog");

        // (6f) The hard ceiling: a phase that keeps completing frames forever
        // still fails, at the ceiling and not before, with its own verdict. The
        // ceiling bounds ONE phase, counted from that phase's start; the CTest
        // TIMEOUT counts from process start. So the ceiling sitting under the
        // TIMEOUT is necessary for its dump to beat the kill, not sufficient: a
        // phase entered late in the run is killed before its ceiling.
        int ceiling = IntegrationTest_GameplayWatchdogCeilingSecs(budget);
        GPWD_CHECK(ceiling > budget, "the phase ceiling is not above the stall budget");
        GPWD_CHECK(ceiling < GPWD_INTEGRATION_TIMEOUT_SECS,
                   "one phase's ceiling is not even under the whole row's TIMEOUT at the default budget");
        IntegrationTest_GameplayWatchdogRebase(&t, warp, 0, 6000.0);
        GameplayWatchdogVerdict v = GP_WATCHDOG_OK;
        int f = 0;
        for (; v == GP_WATCHDOG_OK && f < 100000; f++) {
            v = IntegrationTest_GameplayWatchdogTick(&t, warp, f + 1, 6000.0 + (double)(f + 1) * 0.5, budget);
        }
        GPWD_CHECK(v == GP_WATCHDOG_CEILING, "a phase advancing forever never met the hard ceiling");
        GPWD_CHECK((double)f * 0.5 >= (double)ceiling && (double)(f - 1) * 0.5 < (double)ceiling,
                   "the hard ceiling did not fire exactly at the ceiling");

        // (6g) A non-positive budget disables both.
        IntegrationTest_GameplayWatchdogRebase(&t, warp, 0, 7000.0);
        GPWD_CHECK(IntegrationTest_GameplayWatchdogTick(&t, warp, 0, 1.0e6, 0) == GP_WATCHDOG_OK,
                   "a zero budget tripped the watchdog");
    }

    printf("[TEST] PASS: watchdog budget is wall-clock, bounded under the timeout, and validated; "
           "creation-progress reports are #793 watchdog progress; the stall timer re-bases on each gameplay "
           "frame and a hard ceiling still bounds the phase (#882)\n");
    return TEST_PASS;
}

#undef GPWD_CHECK
