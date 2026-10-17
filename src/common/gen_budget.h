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
 * THE BUDGET IS COUNTED IN ROUNDS, NOT MILLISECONDS (#950)
 * ------------------------------------------------------------------------
 * Until #950 both of the creation's stops compared a CLOCK against the budget,
 * and the clock was C clock(): wall time under MSVC, the process's CPU time
 * summed over every thread under glibc and macOS. So one seed got a different
 * effective budget on Windows and on Linux, and a loaded or slow machine could
 * refuse a seed a faster one created. The never-climb rule below kept every
 * machine to the SAME world, but "created" versus "Not created" still depended
 * on the machine, which one-game semantics does not allow: a seed must be
 * creatable everywhere or nowhere.
 *
 * Both stops now count WORK, and the work is the coordinator's fill round
 * (Combo_Logic_RunFill; ComboLogicFillResult::rounds), which is a pure function
 * of the frozen identity:
 * - PER ATTEMPT: at most RSBS_GENBUDGET_ROUNDS_PER_ATTEMPT rounds, checked by the
 *   single-bag fill's observer before every round (combo_single_bag.c).
 * - WHOLE CREATION: at most RSBS_GENBUDGET_TOTAL_ROUNDS rounds summed over every
 *   ladder attempt, checked BETWEEN attempts (OnFileCreate.cpp).
 * Both numbers are compile-time constants; nothing about the host reaches them.
 * The host scale and the millisecond figures below survive only as the
 * progress overlay's ESTIMATE of how long the round budget lasts on this host
 * (the bar's within-attempt creep) and in the stderr timing lines.
 *
 * ------------------------------------------------------------------------
 * WHAT THE BUDGET DOES *NOT* CHANGE — PR #581 §2a's DETERMINISM RULE
 * ------------------------------------------------------------------------
 * A BUDGET STOP NEVER CLIMBS A LADDER RUNG. When the stop read a clock, an abort
 * was a function of how fast the machine was, and letting it advance the attempt
 * index would have handed two players on different hardware two different worlds
 * under one frozen identity. The round budget is deterministic now, but the rule
 * stays exactly as it was: the ladder catches Rando::Logic::GenerationTimeout
 * BEFORE std::exception and stops cold (OnFileCreate.cpp), and the total stop
 * throws outside the attempt's try. A seed that spends its budget is refused,
 * never re-rolled into a different world.
 *
 * ------------------------------------------------------------------------
 * THE NUMBERS
 * ------------------------------------------------------------------------
 * - FLOOR = 30 s per attempt. The operator's "~30 second cap as the floor",
 *   taken literally. Three times upstream's 10 s, which is the constant every one
 *   of the 14 observed failures hit. Since #950 it is the TIME the round budget
 *   is sized to last on the reference machine, not a stop.
 * - REFERENCE ROUND = the measured cost of one fill round on the reference
 *   machine (gen_budget.c says where the number comes from).
 * - ROUNDS PER ATTEMPT = FLOOR / REFERENCE ROUND: the round budget that lasts
 *   the floor on the reference machine. THE STOP. A slower host takes longer to
 *   spend it and a faster one less; neither gets a different verdict.
 * - TOTAL ROUNDS = 2 x the rounds per attempt, summed over every ladder attempt
 *   and checked BETWEEN attempts. Without it, ten deterministic dead-ends that
 *   each grind for most of their budget would keep a player waiting for
 *   minutes. Checked between attempts rather than inside one, so it can never
 *   truncate an attempt that is about to succeed.
 * - CEILING = 90 s and HOST SCALE = measured/reference clamped to [1.0, 3.0]:
 *   DISPLAY ONLY since #950. They size the overlay's estimate of how long this
 *   host takes to spend the round budget (Combo_GenBudget_FillBudgetMs), and
 *   nothing a creation decides reads either of them.
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

/** The operator's "~30 second cap as the floor", ms: the time the per-attempt
 *  round budget is sized to last on the reference machine. Display and sizing
 *  only since #950; no stop compares a clock against it. */
#define RSBS_GENBUDGET_FLOOR_MS 30000u
/** The overlay estimate's ceiling, ms (display only since #950). */
#define RSBS_GENBUDGET_CEILING_MS 90000u
/** Upper clamp on the measured host scale (display only since #950). */
#define RSBS_GENBUDGET_MAX_SCALE_PERCENT 300u
/** The whole creation's budget is this many per-attempt budgets, checked
 *  BETWEEN ladder attempts. */
#define RSBS_GENBUDGET_TOTAL_MULTIPLIER 2u
/**
 * What one coordinator fill round costs on the reference machine, in ms: the
 * conversion from the operator's 30 s floor to a count of rounds. MEASURED, not
 * chosen; gen_budget.c records the measurement. Like GENBUDGET_REFERENCE_MS it is
 * re-measured only deliberately and only in its own commit, because changing it
 * changes which seeds are creatable, on every machine at once.
 */
#define RSBS_GENBUDGET_REFERENCE_ROUND_MS 22u
/** THE PER-ATTEMPT STOP: fill rounds one ladder attempt may run (#950). */
#define RSBS_GENBUDGET_ROUNDS_PER_ATTEMPT (RSBS_GENBUDGET_FLOOR_MS / RSBS_GENBUDGET_REFERENCE_ROUND_MS)
/** THE TOTAL STOP: fill rounds the whole ladder may run, summed over attempts. */
#define RSBS_GENBUDGET_TOTAL_ROUNDS (RSBS_GENBUDGET_ROUNDS_PER_ATTEMPT * RSBS_GENBUDGET_TOTAL_MULTIPLIER)

/**
 * The host's speed relative to the reference machine, in PERCENT (100 = the
 * reference; 250 = two and a half times slower). Integer percent rather than a
 * double so the value is exactly reproducible in a log line and in a test
 * assertion. DISPLAY ONLY since #950: it sizes the overlay's estimate and is
 * printed, and no stop reads it.
 *
 * Measured ONCE per process, on first call, by timing a fixed integer workload —
 * not by sampling a previous generation, which would make the budget a function
 * of world history. Clamped to [100, RSBS_GENBUDGET_MAX_SCALE_PERCENT].
 */
uint32_t Combo_GenBudget_HostScalePercent(void);

/**
 * The ESTIMATED time, in ms, this host takes to spend ladder attempt @p attempt's
 * round budget (0-based): the progress overlay's denominator for the bar's
 * within-attempt creep and the figure the stderr lines print. Since #950 it is
 * NOT a stop: nothing compares a clock against it.
 *
 * Constant across attempts by design: every attempt is a FRESH derivation of
 * equal expected cost, so a taper would only express impatience, and the total
 * budget below is the honest place to express that. Shaped as a function of the
 * attempt anyway because the shape is the thing a future tuning pass changes,
 * and a caller that already passes the index cannot be the thing that blocks it.
 */
uint32_t Combo_GenBudget_FillBudgetMs(int attempt);

/** The whole creation's estimated time in ms (per-attempt x the multiplier).
 *  Display only since #950, like Combo_GenBudget_FillBudgetMs. */
uint32_t Combo_GenBudget_TotalBudgetMs(void);

/**
 * THE PER-ATTEMPT STOP (#950): the fill rounds ladder attempt @p attempt
 * (0-based) may run. RSBS_GENBUDGET_ROUNDS_PER_ATTEMPT unless a test override is
 * armed. Takes no host input of any kind: the same seed spends the same rounds
 * on every machine, so it is stopped by this budget everywhere or nowhere.
 */
uint32_t Combo_GenBudget_RoundsPerAttempt(int attempt);

/** THE TOTAL STOP (#950): the fill rounds the whole ladder may run, summed over
 *  attempts. RSBS_GENBUDGET_TOTAL_ROUNDS unless a test override is armed. */
uint32_t Combo_GenBudget_TotalRounds(void);

/** The ladder's running sum of fill rounds: zeroed by the ladder before its
 *  first attempt, charged by every single-bag fill that ran (combo_single_bag.c,
 *  success or failure alike), and compared against Combo_GenBudget_TotalRounds
 *  between attempts. */
void Combo_GenBudget_ResetRoundsCharged(void);
/** Add @p rounds to the running sum (saturating). */
void Combo_GenBudget_ChargeRounds(uint32_t rounds);
/** The running sum since the last reset. */
uint32_t Combo_GenBudget_RoundsCharged(void);

/**
 * TEST SEAM (#950). Arm a per-attempt and a total round budget in place of the
 * shipped constants; 0 leaves that one at the shipped value. Nothing in a shipping
 * path calls this. The locks that force a stop (the 1-round per-attempt leg and
 * the total-budget leg of MMPairedExhaustion, the host-scale row) arm it before
 * the creation and disarm it before any assertion can return.
 */
void Combo_GenBudget_SetRoundBudgetOverride(uint32_t roundsPerAttempt, uint32_t totalRounds);

/** The budget's own clock: MONOTONIC WALL time in ms since an arbitrary origin,
 *  the same kind of clock on every platform since #950 (it was C clock(), CPU
 *  time under glibc). Used for the elapsed display, the presentation credit and
 *  the stderr timing lines; no stop reads it. Only DIFFERENCES mean anything. */
uint32_t Combo_GenBudget_NowMs(void);

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
    uint32_t budgetMs;   /**< this attempt's ESTIMATED duration (display only); 0 when N/A */
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

/** Milliseconds since Combo_GenProgress_Begin (0 when never begun). WALL time,
 *  including whatever an overlay spent presenting — this is the number a player
 *  is shown and the number the P12 measurement prints. Nothing decides with it
 *  (since #950 the ladder decides with rounds). */
uint32_t Combo_GenProgress_ElapsedMs(void);

/**
 * PRESENTATION TIME
 *
 * SINCE #950 NEITHER STOP READS THIS: both count fill rounds, and presenting a
 * frame runs no round, so a windowed host and a headless one spend the same
 * budget by construction. The credit below survives for the stderr lines (the
 * "generating / presenting" split of the P12 measurement) and the history that
 * follows is why it was built.
 *
 * A paired creation had exactly two wall-clock stops: the fill's PER-ATTEMPT
 * timeout (GlitchlessLogic's `tick` against its budget) and the ladder's TOTAL
 * budget (OnFileCreate.cpp against Combo_GenBudget_TotalBudgetMs). Since #582 a
 * host with a window spends part of the creation presenting frames — a buffer
 * swap that waits for vblank, ~16 ms each, at up to 10 Hz. Charging that to
 * EITHER stop gives a windowed host measurably less generation headroom than the
 * same host headless: two abort probabilities for one seed on one machine,
 * decided by whether anything was on screen. Crediting only one of the two (which
 * is what the first cut of #582 did) leaves the asymmetry in place at the other.
 *
 * The per-attempt stop is credited at its own site and in its own clock, because
 * `tick` is a GetUnixTimestamp() value and only a delta in that same clock may be
 * added to it. This pair is the credit for the TOTAL stop, measured in the clock
 * sProgressStartMs was taken in. Two measurements of one interval, each against
 * the baseline it is later compared with — deliberately not one number subtracted
 * from a baseline in a different clock, which is how a CPU-time clock() and a
 * wall-time system_clock would have been mixed.
 *
 * Bracketed by the state machine's Paint() (gen_progress_overlay.c), so EVERY
 * painted frame is credited and not only the heartbeat ones: a phase transition
 * also paints, and it also waits for vblank. Nested calls are ignored, and a
 * bracket outside a running session credits nothing.
 */
void Combo_GenProgress_PresentationBegin(void);
/** Close the bracket Combo_GenProgress_PresentationBegin opened. */
void Combo_GenProgress_PresentationEnd(void);
/** Milliseconds this creation has spent inside presentation brackets. Zero on a
 *  host that painted nothing, which is what keeps a headless run's arithmetic
 *  identical to the pre-#582 one. */
uint32_t Combo_GenProgress_PresentationMs(void);
/**
 * Elapsed WITHOUT the time spent presenting (reporting only since #950; the
 * ladder's total stop counts rounds). Never larger than
 * Combo_GenProgress_ElapsedMs(), and equal to it whenever nothing painted.
 */
uint32_t Combo_GenProgress_GenerationElapsedMs(void);

/** Register the presentation leg; NULL removes it. */
void Combo_GenProgress_SetSink(ComboGenProgressSink sink);

/**
 * Register the ON-SCREEN leg (#582); NULL removes it.
 *
 * A SECOND SLOT RATHER THAN SHARING THE ONE ABOVE, because the two legs have
 * different owners and neither may displace the other. The sink above is what
 * the headless `combo-creation-event` row installs to record the phase ORDER;
 * the shipped overlay installs here. With one slot they were mutually
 * exclusive, so wiring the overlay would have silently emptied that row's
 * phase-order array -- a green test asserting nothing.
 *
 * It also differs in WHAT it is told: this leg additionally hears
 * Combo_GenProgress_Begin (an overlay has to appear before the first phase
 * lands, and a phase-order recorder must not see a synthetic entry for it).
 */
void Combo_GenProgress_SetDisplaySink(ComboGenProgressSink sink);

/** Stable short name for a phase. Never NULL. */
const char* Combo_GenPhaseName(uint8_t phase);

/** Tell the surface the ladder's bound, so the MM_FILL lines can read
 *  "attempt 2 of 10" without this file learning MM's constant. */
void Combo_GenProgress_SetMaxAttempts(uint8_t maxAttempts);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_GEN_BUDGET_H
