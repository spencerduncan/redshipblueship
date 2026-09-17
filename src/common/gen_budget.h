/**
 * @file gen_budget.h
 * @brief The creation event's FILL BUDGET and its PROGRESS SURFACE
 *        (#582's operator decision; ADR 0010 increment 2; ADR 0010
 *        Consequences → "Creation-time compute budget"; solver-inventory P12).
 *
 * ------------------------------------------------------------------------
 * WHY THIS EXISTS AT ALL
 * ------------------------------------------------------------------------
 * PR #581's increment-1 review measured the question ADR 0010 flagged as owed:
 * under the heavy Glitchless profile, 14 of 66 master seeds failed their first
 * generation attempt, and EVERY ONE of those 14 was the fill's 10-second
 * wall-clock abort. Zero were deterministic dead-ends. While generation ran at
 * the arrival, that abort cost a player a paired Termina discovered hours later.
 * Since ADR 0010 increment 2 it costs them the FILE, at file select, while they
 * wait — so the budget stopped being a constant in a fill and became a product
 * decision.
 *
 * The operator ruled on that evidence (2026-08-04, recorded in ADR 0010 Decision
 * 5 under increment 2): **a visible generation-progress surface and a ~30 second
 * cap as the floor, layered with a per-attempt adaptive budget calibrated to
 * host speed.** Sizing the floor, the calibration and the copy was left to the
 * implementer; the numbers below are that sizing and every one of them is
 * named, not buried.
 *
 * ------------------------------------------------------------------------
 * WHAT THE BUDGET DOES *NOT* CHANGE — PR #581 §2a's DETERMINISM RULE
 * ------------------------------------------------------------------------
 * A WALL-CLOCK ABORT NEVER CLIMBS A LADDER RUNG. An abort is a function of how
 * fast the machine is, not of the seed, and letting it advance the attempt index
 * would hand two players on different hardware two different worlds under one
 * frozen identity. The ladder catches Rando::Logic::GenerationTimeout BEFORE
 * std::exception and stops cold (OnFileCreate.cpp). Nothing here touches that.
 * All a bigger budget buys is a slower host reaching the SAME verdict the fast
 * host reached; all the calibration does is decide how long it waits first.
 *
 * ------------------------------------------------------------------------
 * THE NUMBERS
 * ------------------------------------------------------------------------
 * - FLOOR = 30 s per attempt. The operator's "~30 second cap as the floor",
 *   taken literally: no host ever gets less. Three times upstream's 10 s, which
 *   is the constant every one of the 14 observed failures hit.
 * - CEILING = 90 s per attempt. A budget has to end somewhere, and a player
 *   staring at a file-select screen for longer than a minute and a half with no
 *   world at the end of it is a worse outcome than "try another seed".
 * - HOST SCALE = measured/reference, clamped to [1.0, 3.0]. Never below 1: the
 *   floor is a floor, and a machine FASTER than the reference does not need a
 *   smaller budget — it will simply finish sooner.
 * - TOTAL CREATION BUDGET = 2 x the per-attempt budget, checked BETWEEN
 *   attempts. Without it, ten deterministic dead-ends that each grind for most
 *   of their budget would keep a player waiting for minutes. Checked between
 *   attempts rather than inside one, so it can never truncate an attempt that is
 *   about to succeed; worst-case wait is therefore total + one in-flight attempt
 *   (~90 s at scale 1.0).
 *
 * THE REFERENCE MACHINE IS NAMED, NOT IMPLIED. kGenBudgetReferenceMs is the time
 * the fixed calibration workload below takes on the development workstation this
 * increment was measured on. That is an arbitrary choice of reference and it is
 * SUPPOSED to be: only the RATIO is used, and a reference that drifts with new
 * hardware makes every host's scale drift with it — which is why the constant is
 * pinned in the source and re-measured deliberately, never sampled at runtime
 * from something like "the last generation's duration".
 *
 * ------------------------------------------------------------------------
 * THE PROGRESS SURFACE
 * ------------------------------------------------------------------------
 * The creation event blocks the game thread, so an in-frame progress BAR needs a
 * render-during-blocking-work seam that does not exist in this tree and would
 * live in SohGui. What exists here is the CHANNEL: a phase/attempt/elapsed/budget
 * record the creation event reports into, with two legs — a greppable stderr line
 * always, and a registered sink for presentation. The sink is what a bar, a
 * modal or a file-select caption plugs into later without the creation event
 * learning anything about rendering.
 *
 * Combo_GenProgress_ElapsedMs() at the end of a creation is also the P12
 * measurement ("measure the linked round against the ~30 s floor"), which is why
 * the elapsed clock is part of this surface rather than an ad-hoc timer at the
 * seam.
 */

#ifndef RSBS_COMMON_GEN_BUDGET_H
#define RSBS_COMMON_GEN_BUDGET_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- The pinned numbers (see the header comment for each one's reason) ------

/** Per-attempt wall-clock floor, ms. The operator's "~30 second cap as the
 *  floor". No host ever gets less than this. */
#define RSBS_GENBUDGET_FLOOR_MS 30000u
/** Per-attempt wall-clock ceiling, ms. */
#define RSBS_GENBUDGET_CEILING_MS 90000u
/** Upper clamp on the measured host scale. */
#define RSBS_GENBUDGET_MAX_SCALE_PERCENT 300u
/** The whole creation's budget is this many per-attempt budgets, checked
 *  BETWEEN ladder attempts. */
#define RSBS_GENBUDGET_TOTAL_MULTIPLIER 2u

/**
 * The host's speed relative to the reference machine, in PERCENT (100 = the
 * reference; 250 = two and a half times slower). Integer percent rather than a
 * double so the value is exactly reproducible in a log line and in a test
 * assertion.
 *
 * Measured ONCE per process, on first call, by timing a fixed integer workload —
 * not by sampling a previous generation, which would make the budget a function
 * of world history. Clamped to [100, RSBS_GENBUDGET_MAX_SCALE_PERCENT].
 */
uint32_t Combo_GenBudget_HostScalePercent(void);

/**
 * The wall-clock budget, in ms, for ladder attempt @p attempt (0-based).
 *
 * Constant across attempts by design: every attempt is a FRESH derivation of
 * equal expected cost, so a taper would only express impatience, and the total
 * budget below is the honest place to express that. Shaped as a function of the
 * attempt anyway because the shape is the thing a future tuning pass changes,
 * and a caller that already passes the index cannot be the thing that blocks it.
 */
uint32_t Combo_GenBudget_FillBudgetMs(int attempt);

/** The whole creation's budget in ms (per-attempt x the multiplier). */
uint32_t Combo_GenBudget_TotalBudgetMs(void);

/**
 * TEST SEAM. Pin the host scale so the locks are deterministic on any CI
 * machine; 0 restores the measured value. Nothing in a shipping path calls this.
 */
void Combo_GenBudget_SetHostScalePercentOverride(uint32_t percent);

// ---- The progress surface ---------------------------------------------------

typedef enum ComboGenPhase {
    RSBS_GENPHASE_IDLE = 0,
    RSBS_GENPHASE_FREEZE,    /**< identity frozen; before either fill */
    RSBS_GENPHASE_OOT_FILL,  /**< OoT's Fill() */
    RSBS_GENPHASE_MM_FILL,   /**< MM's fill, under the attempt ladder */
    RSBS_GENPHASE_CROSSINGS, /**< the placement passes */
    RSBS_GENPHASE_SPOILER,   /**< the one spoiler artifact */
    RSBS_GENPHASE_PUBLISH,   /**< identity publish + shadow arm */
    RSBS_GENPHASE_DONE,
    RSBS_GENPHASE_FAILED,
    RSBS_GENPHASE_MAX
} ComboGenPhase;

typedef struct ComboGenProgress {
    uint8_t phase;       /**< a ComboGenPhase */
    uint8_t attempt;     /**< 1-based ladder attempt; 0 outside MM_FILL */
    uint8_t maxAttempts; /**< the ladder bound; 0 outside MM_FILL */
    uint32_t elapsedMs;  /**< since Combo_GenProgress_Begin */
    uint32_t budgetMs;   /**< this attempt's wall-clock budget; 0 when N/A */
    const char* detail;  /**< a short human phrase; never NULL */
} ComboGenProgress;

/** The presentation leg. NULL (the default) means "stderr only". */
typedef void (*ComboGenProgressSink)(const ComboGenProgress* progress);

/** Start a creation: zero the record and start the elapsed clock. */
void Combo_GenProgress_Begin(void);

/**
 * Report a phase transition.
 *
 * @param phase      a ComboGenPhase.
 * @param attempt    1-based ladder attempt for RSBS_GENPHASE_MM_FILL; 0
 *                   elsewhere.
 * @param detail     a short phrase for the surface; NULL becomes the phase name.
 */
void Combo_GenProgress_Report(uint8_t phase, int attempt, const char* detail);

/** Finish a creation. @p ok selects DONE or FAILED and emits the final line
 *  (which carries the P12 creation-time measurement). */
void Combo_GenProgress_End(bool ok);

/** The latest reported record. Never NULL. */
const ComboGenProgress* Combo_GenProgress_Current(void);

/** Milliseconds since Combo_GenProgress_Begin (0 when never begun). */
uint32_t Combo_GenProgress_ElapsedMs(void);

/** Register the presentation leg; NULL removes it. */
void Combo_GenProgress_SetSink(ComboGenProgressSink sink);

/** Stable short name for a phase. Never NULL. */
const char* Combo_GenPhaseName(uint8_t phase);

/** Tell the surface the ladder's bound, so the MM_FILL lines can read
 *  "attempt 2 of 10" without this file learning MM's constant. */
void Combo_GenProgress_SetMaxAttempts(uint8_t maxAttempts);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_GEN_BUDGET_H
