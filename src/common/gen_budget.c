/**
 * @file gen_budget.c
 * @brief Implementation of the creation event's fill budget and progress
 *        surface. The contract, the numbers and every reason behind them live
 *        in gen_budget.h.
 */

#include "gen_budget.h"

#include <stdio.h>
#include <time.h>

// ============================================================================
// Host calibration
// ============================================================================

/**
 * Milliseconds on a clock that is monotonic enough for a ratio and a budget.
 * clock() rather than a platform timer because this file is plain C compiled
 * for every target the single exe builds for, and the two consumers — a ratio
 * and an elapsed-time display — are both insensitive to the CPU-time /
 * wall-time difference between implementations.
 */
static uint32_t GenBudgetNowMs(void) {
    const clock_t ticks = clock();
    if (ticks == (clock_t)-1) {
        return 0;
    }
    return (uint32_t)(((uint64_t)ticks * 1000ull) / (uint64_t)CLOCKS_PER_SEC);
}

/**
 * THE CALIBRATION WORKLOAD. A fixed, branch-light integer loop: no allocation,
 * no syscalls, no memory pressure, nothing a compiler may elide (the
 * accumulator is volatile and is read afterwards). Its absolute duration means
 * nothing; the ratio against kGenBudgetReferenceMs is the whole output.
 *
 * Sized so the reference machine takes tens of milliseconds — long enough that
 * clock()'s millisecond granularity is not the dominant term, short enough that
 * paying it once per process on the creation path is invisible.
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
 * A machine at or below this number scales to 100% and gets the 30 s floor; one
 * three times slower saturates the 300% clamp and gets the 90 s ceiling.
 *
 * WHEN TO RE-MEASURE: only when the reference machine is deliberately changed,
 * and only as its own commit, because the number silently retunes every host
 * budget. It is not a knob for making a slow CI row pass.
 */
#define GENBUDGET_REFERENCE_MS 18u

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
// The progress surface
// ============================================================================

static const char* const kGenPhaseNames[RSBS_GENPHASE_MAX] = {
    "idle", "freezing the world's rules", "building the Ocarina of Time world",
    "building the Majora's Mask world", "linking the two worlds", "writing the spoiler",
    "saving", "done", "failed",
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
/** Time this creation has spent presenting, in GenBudgetNowMs units. See the
 *  header: the TOTAL budget's stop is compared against elapsed MINUS this, so a
 *  windowed host and a headless one get the same generation headroom. */
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
    // frozen total. The ladder's total-budget check reads this, so a stale
    // elapsed time left over from an earlier creation would abort the next one
    // at its second attempt for no reason. The finished total is still readable
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
        // credit that would disable the total-budget stop for the rest of the
        // creation. Same rule as the per-attempt credit's guard.
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
    sProgressStartMs = GenBudgetNowMs();
    sProgressRunning = true;
    sPresentationMs = 0;
    sPresentationOpen = false;
    sProgress.phase = RSBS_GENPHASE_IDLE;
    sProgress.attempt = 0;
    sProgress.maxAttempts = 0;
    sProgress.elapsedMs = 0;
    sProgress.budgetMs = 0;
    sProgress.detail = kGenPhaseNames[RSBS_GENPHASE_IDLE];
    fprintf(stderr, "[Combo] creation progress: BEGIN (per-attempt budget %ums, total budget %ums, host scale %u%%)\n",
            Combo_GenBudget_FillBudgetMs(0), Combo_GenBudget_TotalBudgetMs(), Combo_GenBudget_HostScalePercent());
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
    if (phase >= RSBS_GENPHASE_MAX) {
        phase = RSBS_GENPHASE_IDLE;
    }
    sProgress.phase = phase;
    sProgress.attempt = (attempt > 0 && attempt < 256) ? (uint8_t)attempt : 0;
    sProgress.elapsedMs = Combo_GenProgress_ElapsedMs();
    sProgress.budgetMs = (phase == RSBS_GENPHASE_MM_FILL) ? Combo_GenBudget_FillBudgetMs(attempt > 0 ? attempt - 1 : 0)
                                                          : 0u;
    sProgress.detail = (detail != NULL) ? detail : kGenPhaseNames[phase];

    // The stderr leg fires unconditionally. It is the only leg a headless row, a
    // CI log or an operator session has, and the one the #582 evidence was
    // gathered from in the first place.
    if (sProgress.attempt != 0) {
        fprintf(stderr, "[Combo] creation progress: %s (attempt %u of %u, %ums elapsed, %ums budget)\n",
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
            "[Combo] creation progress: %s in %ums wall (%ums generating, %ums presenting; per-attempt budget %ums, "
            "floor %ums)\n",
            ok ? "COMPLETE" : "FAILED", sProgress.elapsedMs,
            (sProgress.elapsedMs > sPresentationMs) ? (sProgress.elapsedMs - sPresentationMs) : 0u, sPresentationMs,
            Combo_GenBudget_FillBudgetMs(0), (unsigned)RSBS_GENBUDGET_FLOOR_MS);
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
