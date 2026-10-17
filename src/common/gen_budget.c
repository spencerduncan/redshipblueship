/**
 * @file gen_budget.c
 * @brief Implementation of the creation event's fill budget and progress
 *        surface. The contract, the numbers and every reason behind them live
 *        in gen_budget.h.
 */

#include "gen_budget.h"
#include "integration_test_hooks.h" // #793: each report is progress for the int-* watchdog

#include <stdio.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <time.h>
#endif

// ============================================================================
// The clock
// ============================================================================

/**
 * MONOTONIC WALL time in ms, the same kind of clock on every platform (#950).
 *
 * This used to be C clock(), and the comment above it said its consumers were
 * "insensitive to the CPU-time / wall-time difference between implementations".
 * That stopped being true the moment the clock decided whether a creation
 * stopped: clock() is wall time under MSVC but the process's CPU time, summed
 * over every thread, under glibc and macOS, so one seed got a different budget
 * on Windows and on Linux. Since #950 no stop reads any clock (both count fill
 * rounds, gen_budget.h), and what is left reading this one is the elapsed
 * display, the presentation credit, the host-scale estimate and the stderr
 * timing lines. Those are all WALL-time questions ("how long did the player
 * wait"), so the clock is a monotonic wall clock everywhere: QueryPerformanceCounter
 * on Windows, CLOCK_MONOTONIC elsewhere. Monotonic rather than calendar time
 * (timespec_get's TIME_UTC) so a clock adjustment mid-creation cannot make an
 * elapsed figure jump or run backwards.
 */
static uint32_t GenBudgetNowMs(void) {
#ifdef _WIN32
    static LARGE_INTEGER sFrequency = { 0 };
    LARGE_INTEGER now;
    if (sFrequency.QuadPart == 0 && !QueryPerformanceFrequency(&sFrequency)) {
        return 0;
    }
    if (!QueryPerformanceCounter(&now) || sFrequency.QuadPart <= 0) {
        return 0;
    }
    return (uint32_t)(((uint64_t)now.QuadPart / (uint64_t)sFrequency.QuadPart) * 1000ull +
                      (((uint64_t)now.QuadPart % (uint64_t)sFrequency.QuadPart) * 1000ull) /
                          (uint64_t)sFrequency.QuadPart);
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint32_t)((uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull);
#endif
}

uint32_t Combo_GenBudget_NowMs(void) {
    return GenBudgetNowMs();
}

// ============================================================================
// Host calibration (DISPLAY ONLY since #950: it sizes the overlay's estimate)
// ============================================================================

/**
 * THE CALIBRATION WORKLOAD. A fixed, branch-light integer loop: no allocation,
 * no syscalls, no memory pressure, nothing a compiler may elide (the
 * accumulator is volatile and is read afterwards). Its absolute duration means
 * nothing; the ratio against kGenBudgetReferenceMs is the whole output.
 *
 * Sized so the reference machine takes tens of milliseconds — long enough that
 * a millisecond granularity is not the dominant term, short enough that paying
 * it once per process on the creation path is invisible.
 */
#define GENBUDGET_CALIBRATION_ITERATIONS 24000000u

/**
 * The reference measurement, in ms, of the workload above.
 *
 * MEASURED, NOT GUESSED: 18 ms is what the loop above took on the development
 * workstation ADR 0010 increment 2 was built and measured on (a 16-core Windows
 * host, Release/MSVC, 2026-09-17). It is pinned in source on purpose — see
 * gen_budget.h for why a runtime-sampled reference would make every host scale
 * drift out from under the ruling.
 *
 * A machine at or below this number scales to 100% and its overlay estimates
 * the 30 s floor; one three times slower saturates the 300% clamp and estimates
 * the 90 s ceiling. (Until #950 those were the host's real per-attempt budgets.)
 *
 * WHEN TO RE-MEASURE: only when the reference machine is deliberately changed,
 * and only as its own commit. Since #950 it moves only the overlay's estimate,
 * never a verdict.
 */
#define GENBUDGET_REFERENCE_MS 18u

/*
 * RSBS_GENBUDGET_REFERENCE_ROUND_MS (gen_budget.h), WHERE IT COMES FROM.
 *
 * The cost of one coordinator fill round on the reference machine (the same
 * workstation GENBUDGET_REFERENCE_MS was measured on), from real paired
 * creations of the shipped profile, every fill a single batch:
 *   - ADR 0010's K11 entry (PR #743): thirty creations through the event, host
 *     scale 105%: 6.41 s of fill on average over about 264 rounds = 24.3 ms per
 *     round. (One headless creation at scale 100%: 5.61 s over 256 = 21.9 ms.)
 *   - #950 (2026-10-16, Release/MSVC, scale 100-105%): nine single-batch fills
 *     of 258 rounds across the MMPairedExhaustion, MMRoundBudgetHostScale and
 *     GoldenSeedsRoundBudget rows took 4.96-7.70 s, mean 6.63 s = 25.7 ms per
 *     round (19.2 to 29.8).
 * Each figure is the single-bag call's wall time (bag build and bookkeeping
 * included) over its rounds, so a little above a bare round's cost.
 *
 * 24 ms is the round the floor is divided by: 30 s / 24 ms = 1250 rounds per
 * attempt. A shipped single batch is 258 rounds, so a fill that needs four
 * batches (1032 rounds) is created and one that needs five (1290) is stopped,
 * which is where the wall-clock floor sat on the reference machine (ADR 0010's
 * K11 entry: "at about 6.4 s per batch, a seed would need five batches to exceed
 * the budget"). The total is 2500 rounds.
 */

static uint32_t sRoundsPerAttemptOverride = 0;
static uint32_t sTotalRoundsOverride = 0;
static uint32_t sRoundsCharged = 0;

static uint32_t sHostScaleOverridePercent = 0;
static uint32_t sMeasuredScalePercent = 0;

static uint32_t MeasureHostScalePercent(void) {
    const uint32_t t0 = GenBudgetNowMs();
    volatile uint64_t acc = 1469598103934665603ull;
    for (uint32_t i = 0; i < GENBUDGET_CALIBRATION_ITERATIONS; i++) {
        acc ^= (uint64_t)i;
        acc *= 1099511628211ull;
    }
    const uint32_t t1 = GenBudgetNowMs();
    const uint32_t elapsed = (t1 >= t0) ? (t1 - t0) : 0u;
    // Keep the accumulator observably used so no optimizer can delete the loop.
    if (acc == 0ull) {
        fprintf(stderr, "[Combo] gen budget: calibration accumulator collapsed (impossible)\n");
    }

    uint32_t percent = (elapsed * 100u) / GENBUDGET_REFERENCE_MS;
    if (percent < 100u) {
        // A host FASTER than the reference does not get a smaller budget: the
        // floor is a floor, and it will simply finish sooner.
        percent = 100u;
    }
    if (percent > RSBS_GENBUDGET_MAX_SCALE_PERCENT) {
        percent = RSBS_GENBUDGET_MAX_SCALE_PERCENT;
    }
    fprintf(stderr, "[Combo] gen budget: host calibration %ums against a %ums reference -> scale %u%%\n", elapsed,
            GENBUDGET_REFERENCE_MS, percent);
    return percent;
}

uint32_t Combo_GenBudget_HostScalePercent(void) {
    if (sHostScaleOverridePercent != 0) {
        return sHostScaleOverridePercent;
    }
    if (sMeasuredScalePercent == 0) {
        sMeasuredScalePercent = MeasureHostScalePercent();
    }
    return sMeasuredScalePercent;
}

void Combo_GenBudget_SetHostScalePercentOverride(uint32_t percent) {
    if (percent != 0 && percent < 100u) {
        percent = 100u;
    }
    if (percent > RSBS_GENBUDGET_MAX_SCALE_PERCENT) {
        percent = RSBS_GENBUDGET_MAX_SCALE_PERCENT;
    }
    sHostScaleOverridePercent = percent;
}

uint32_t Combo_GenBudget_FillBudgetMs(int attempt) {
    // Constant across attempts (gen_budget.h says why the shape still takes the
    // index). Named so the parameter is deliberately unused rather than absent.
    (void)attempt;
    uint64_t ms = ((uint64_t)RSBS_GENBUDGET_FLOOR_MS * (uint64_t)Combo_GenBudget_HostScalePercent()) / 100ull;
    if (ms < (uint64_t)RSBS_GENBUDGET_FLOOR_MS) {
        ms = (uint64_t)RSBS_GENBUDGET_FLOOR_MS;
    }
    if (ms > (uint64_t)RSBS_GENBUDGET_CEILING_MS) {
        ms = (uint64_t)RSBS_GENBUDGET_CEILING_MS;
    }
    return (uint32_t)ms;
}

uint32_t Combo_GenBudget_TotalBudgetMs(void) {
    return Combo_GenBudget_FillBudgetMs(0) * RSBS_GENBUDGET_TOTAL_MULTIPLIER;
}

// ============================================================================
// The round budget (#950): the creation's two stops
// ============================================================================

uint32_t Combo_GenBudget_RoundsPerAttempt(int attempt) {
    // Constant across attempts for the reason Combo_GenBudget_FillBudgetMs is.
    // NO HOST INPUT, deliberately and entirely: not the scale, not a clock, not
    // a platform. That is the whole of #950.
    (void)attempt;
    return (sRoundsPerAttemptOverride != 0) ? sRoundsPerAttemptOverride : (uint32_t)RSBS_GENBUDGET_ROUNDS_PER_ATTEMPT;
}

uint32_t Combo_GenBudget_TotalRounds(void) {
    return (sTotalRoundsOverride != 0) ? sTotalRoundsOverride : (uint32_t)RSBS_GENBUDGET_TOTAL_ROUNDS;
}

void Combo_GenBudget_ResetRoundsCharged(void) {
    sRoundsCharged = 0;
}

void Combo_GenBudget_ChargeRounds(uint32_t rounds) {
    sRoundsCharged = (rounds > 0xFFFFFFFFu - sRoundsCharged) ? 0xFFFFFFFFu : sRoundsCharged + rounds;
}

uint32_t Combo_GenBudget_RoundsCharged(void) {
    return sRoundsCharged;
}

void Combo_GenBudget_SetRoundBudgetOverride(uint32_t roundsPerAttempt, uint32_t totalRounds) {
    sRoundsPerAttemptOverride = roundsPerAttempt;
    sTotalRoundsOverride = totalRounds;
}

// ============================================================================
// The progress surface
// ============================================================================

static const char* const kGenPhaseNames[RSBS_GENPHASE_MAX] = {
    "idle",
    "freezing the world's rules",
    "building the Ocarina of Time world",
    "building the Majora's Mask world",
    "linking the two worlds",
    "writing the spoiler",
    "saving",
    "done",
    "failed",
};

const char* Combo_GenPhaseName(uint8_t phase) {
    if (phase >= RSBS_GENPHASE_MAX) {
        return "unknown";
    }
    return kGenPhaseNames[phase];
}

static ComboGenProgress sProgress = { RSBS_GENPHASE_IDLE, 0, 0, 0, 0, "idle" };
static ComboGenProgressSink sProgressSink = NULL;
static ComboGenProgressSink sDisplaySink = NULL;
static uint32_t sProgressStartMs = 0;
static bool sProgressRunning = false;
/** Time this creation has spent presenting, in GenBudgetNowMs units. Reporting
 *  only since #950 (the stops count rounds); see the header. */
static uint32_t sPresentationMs = 0;
static uint32_t sPresentationStartMs = 0;
static bool sPresentationOpen = false;

void Combo_GenProgress_SetSink(ComboGenProgressSink sink) {
    sProgressSink = sink;
}

void Combo_GenProgress_SetDisplaySink(ComboGenProgressSink sink) {
    sDisplaySink = sink;
}

const ComboGenProgress* Combo_GenProgress_Current(void) {
    return &sProgress;
}

uint32_t Combo_GenProgress_ElapsedMs(void) {
    // ZERO WHEN NOT RUNNING, deliberately, and NOT the previous creation's
    // frozen total. The ladder's total-budget check read this until #950, when a
    // stale elapsed time left over from an earlier creation would have aborted
    // the next one at its second attempt; the display and the stderr lines still
    // read it, and must not show a previous creation's time. The finished total is still readable
    // through Combo_GenProgress_Current()->elapsedMs, which is where a display
    // (and the P12 measurement line) gets it.
    if (!sProgressRunning) {
        return 0;
    }
    const uint32_t now = GenBudgetNowMs();
    return (now >= sProgressStartMs) ? (now - sProgressStartMs) : 0u;
}

void Combo_GenProgress_PresentationBegin(void) {
    // Outside a session there is nothing to credit (the terminal paint happens
    // after End has already stopped the clock), and a nested bracket would
    // double-count the inner interval.
    if (!sProgressRunning || sPresentationOpen) {
        return;
    }
    sPresentationStartMs = GenBudgetNowMs();
    sPresentationOpen = true;
}

void Combo_GenProgress_PresentationEnd(void) {
    if (!sPresentationOpen) {
        return;
    }
    sPresentationOpen = false;
    const uint32_t now = GenBudgetNowMs();
    if (now > sPresentationStartMs) {
        // A backwards step credits nothing rather than wrapping into an enormous
        // credit that would turn every later "generating" figure into zero.
        sPresentationMs += now - sPresentationStartMs;
    }
}

uint32_t Combo_GenProgress_PresentationMs(void) {
    return sPresentationMs;
}

uint32_t Combo_GenProgress_GenerationElapsedMs(void) {
    const uint32_t elapsed = Combo_GenProgress_ElapsedMs();
    return (elapsed > sPresentationMs) ? (elapsed - sPresentationMs) : 0u;
}

void Combo_GenProgress_Begin(void) {
    // #793: the creation blocks the game thread inside one frame for as long as
    // its own budget allows; Begin, every Report and End are the progress the
    // int-* wall-clock watchdog sees meanwhile (integration_test_hooks.h).
    IntegrationTest_CreationProgress();
    sProgressStartMs = GenBudgetNowMs();
    sProgressRunning = true;
    sPresentationMs = 0;
    sPresentationOpen = false;
    sProgress.phase = RSBS_GENPHASE_IDLE;
    sProgress.attempt = 0;
    sProgress.maxAttempts = 0;
    sProgress.elapsedMs = 0;
    sProgress.budgetMs = 0;
    // NOT kGenPhaseNames[IDLE] (#582's review). The phase IS idle — nothing has
    // begun filling yet — but this record is the FIRST thing the on-screen surface
    // shows, and it stays on screen for the whole pre-fill stretch (MM's rando core
    // init and the vanilla bootstrap, measured in the creation log). A bar at 2%
    // captioned "Idle" is a bar that looks broken. The phase stays IDLE so the
    // weights and the existing phase-order assertions are untouched; only the words
    // the player reads change, which is what `detail` is for.
    sProgress.detail = "preparing to build your paired world";
    fprintf(stderr,
            "[Combo] creation progress: BEGIN (per-attempt budget %u rounds, total budget %u rounds; host scale %u%%, "
            "estimated %ums per attempt)\n",
            Combo_GenBudget_RoundsPerAttempt(0), Combo_GenBudget_TotalRounds(), Combo_GenBudget_HostScalePercent(),
            Combo_GenBudget_FillBudgetMs(0));
    fflush(stderr);

    // THE ON-SCREEN LEG ONLY (#582). An overlay has to exist before the first
    // phase lands, or the player stares at a frozen file-select screen for the
    // whole of the first MM fill attempt. The phase-order sink is deliberately
    // NOT told: it records transitions, and a synthetic IDLE entry at index 0
    // would shift every index the existing row asserts about.
    if (sDisplaySink != NULL) {
        sDisplaySink(&sProgress);
    }
}

void Combo_GenProgress_Report(uint8_t phase, int attempt, const char* detail) {
    IntegrationTest_CreationProgress();
    if (phase >= RSBS_GENPHASE_MAX) {
        phase = RSBS_GENPHASE_IDLE;
    }
    sProgress.phase = phase;
    sProgress.attempt = (attempt > 0 && attempt < 256) ? (uint8_t)attempt : 0;
    sProgress.elapsedMs = Combo_GenProgress_ElapsedMs();
    sProgress.budgetMs =
        (phase == RSBS_GENPHASE_MM_FILL) ? Combo_GenBudget_FillBudgetMs(attempt > 0 ? attempt - 1 : 0) : 0u;
    sProgress.detail = (detail != NULL) ? detail : kGenPhaseNames[phase];

    // The stderr leg fires unconditionally. It is the only leg a headless row, a
    // CI log or an operator session has, and the one the #582 evidence was
    // gathered from in the first place.
    if (sProgress.attempt != 0) {
        fprintf(stderr, "[Combo] creation progress: %s (attempt %u of %u, %ums elapsed, ~%ums estimated)\n",
                sProgress.detail, (unsigned)sProgress.attempt, (unsigned)sProgress.maxAttempts, sProgress.elapsedMs,
                sProgress.budgetMs);
    } else {
        fprintf(stderr, "[Combo] creation progress: %s (%ums elapsed)\n", sProgress.detail, sProgress.elapsedMs);
    }
    fflush(stderr);

    if (sProgressSink != NULL) {
        sProgressSink(&sProgress);
    }
    if (sDisplaySink != NULL) {
        sDisplaySink(&sProgress);
    }
}

void Combo_GenProgress_End(bool ok) {
    IntegrationTest_CreationProgress();
    sProgress.elapsedMs = Combo_GenProgress_ElapsedMs(); // read while still running
    sProgressRunning = false;
    sProgress.phase = ok ? (uint8_t)RSBS_GENPHASE_DONE : (uint8_t)RSBS_GENPHASE_FAILED;
    sProgress.attempt = 0;
    sProgress.budgetMs = 0;
    sProgress.detail = kGenPhaseNames[sProgress.phase];

    // THE P12 MEASUREMENT. "Measure the linked round against the ~30 s floor"
    // asked for a number, and this is where the number is produced on every real
    // creation — not only in a benchmark somebody has to remember to run.
    // The presentation split is printed with it, because "the bar cost the
    // generation nothing" is a claim about numbers and this is where the numbers
    // are. Zero on a headless host by construction.
    fprintf(stderr,
            "[Combo] creation progress: %s in %ums wall (%ums generating, %ums presenting; per-attempt budget %u "
            "rounds, floor %ums)\n",
            ok ? "COMPLETE" : "FAILED", sProgress.elapsedMs,
            (sProgress.elapsedMs > sPresentationMs) ? (sProgress.elapsedMs - sPresentationMs) : 0u, sPresentationMs,
            Combo_GenBudget_RoundsPerAttempt(0), (unsigned)RSBS_GENBUDGET_FLOOR_MS);
    fflush(stderr);

    if (sProgressSink != NULL) {
        sProgressSink(&sProgress);
    }
    if (sDisplaySink != NULL) {
        sDisplaySink(&sProgress);
    }
}

void Combo_GenProgress_SetMaxAttempts(uint8_t maxAttempts) {
    sProgress.maxAttempts = maxAttempts;
}
