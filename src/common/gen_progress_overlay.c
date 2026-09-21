/**
 * @file gen_progress_overlay.c
 * @brief The creation-progress overlay's state machine. The contract and every
 *        reason behind it live in gen_progress_overlay.h.
 */

#include "gen_progress_overlay.h"

#include <stdio.h>
#include <string.h>

// ============================================================================
// Phase weights
// ============================================================================
//
// COARSE ON PURPOSE. The creation has no measurable total -- MM's fill is a
// search, and its duration is a property of the seed -- so a percentage
// pretending to be a completion estimate would be a lie with a number on it.
// What these weights DO express honestly is "how far down the phase list we
// are", which is the question a player actually has, and the caption carries
// the part that is precise (the phase name and the attempt index).
//
// The values are spaced so the phase the player waits longest in (MM's fill
// under the attempt ladder) owns the widest band, because that is where a
// still bar reads as a hang.
static float PhaseWeight(uint8_t phase) {
    switch (phase) {
        case RSBS_GENPHASE_IDLE:
            return 0.02f;
        case RSBS_GENPHASE_FREEZE:
            return 0.06f;
        case RSBS_GENPHASE_OOT_FILL:
            return 0.15f;
        case RSBS_GENPHASE_MM_FILL:
            return 0.30f;
        case RSBS_GENPHASE_CROSSINGS:
            return 0.75f;
        case RSBS_GENPHASE_SPOILER:
            return 0.88f;
        case RSBS_GENPHASE_PUBLISH:
            return 0.95f;
        case RSBS_GENPHASE_DONE:
            return 1.0f;
        case RSBS_GENPHASE_FAILED:
        default:
            return 0.0f;
    }
}

/** The weight the phase AFTER @p phase starts at -- the ceiling this phase's
 *  within-phase creep may approach and never reach. */
static float NextPhaseWeight(uint8_t phase) {
    switch (phase) {
        case RSBS_GENPHASE_IDLE:
            return PhaseWeight(RSBS_GENPHASE_FREEZE);
        case RSBS_GENPHASE_FREEZE:
            return PhaseWeight(RSBS_GENPHASE_OOT_FILL);
        case RSBS_GENPHASE_OOT_FILL:
            return PhaseWeight(RSBS_GENPHASE_MM_FILL);
        case RSBS_GENPHASE_MM_FILL:
            return PhaseWeight(RSBS_GENPHASE_CROSSINGS);
        case RSBS_GENPHASE_CROSSINGS:
            return PhaseWeight(RSBS_GENPHASE_SPOILER);
        case RSBS_GENPHASE_SPOILER:
            return PhaseWeight(RSBS_GENPHASE_PUBLISH);
        case RSBS_GENPHASE_PUBLISH:
            return 1.0f;
        default:
            return PhaseWeight(phase);
    }
}

// ============================================================================
// State
// ============================================================================

static ComboGenOverlayView sView;
static ComboGenOverlayPainter sPainter = NULL;
static uint32_t sPaintCount = 0;
/** The attempt clock value at the last paint, so the repaint interval is
 *  measured in the caller's own milliseconds and this file still reads no
 *  clock. UINT32_MAX means "nothing painted in this phase yet". */
static uint32_t sLastPaintAttemptMs = 0xFFFFFFFFu;
static bool sLastPaintValid = false;

void ComboGenOverlay_SetPainter(ComboGenOverlayPainter painter) {
    sPainter = painter;
}

const ComboGenOverlayView* ComboGenOverlay_View(void) {
    return &sView;
}

uint32_t ComboGenOverlay_PaintCount(void) {
    return sPaintCount;
}

void ComboGenOverlay_Reset(void) {
    memset(&sView, 0, sizeof(sView));
    sView.state = (uint8_t)RSBS_GENOVERLAY_HIDDEN;
    sView.phase = (uint8_t)RSBS_GENPHASE_IDLE;
    sPaintCount = 0;
    sLastPaintValid = false;
    sLastPaintAttemptMs = 0xFFFFFFFFu;
}

bool ComboGenOverlay_WantsHeartbeat(void) {
    return sPainter != NULL && sView.state == (uint8_t)RSBS_GENOVERLAY_SHOWN;
}

static void Paint(void) {
    if (sPainter == NULL) {
        return;
    }
    sPaintCount++;
    // BRACKETED, so the ladder's TOTAL budget measures generation rather than
    // presentation (gen_budget.h explains why both stops need the credit and why
    // each is measured in its own clock). Here rather than at the heartbeat's
    // call site because a phase TRANSITION paints too, and it waits for the same
    // vblank; the per-attempt credit at the fill's call site only ever saw the
    // heartbeat paints.
    Combo_GenProgress_PresentationBegin();
    sPainter(&sView);
    Combo_GenProgress_PresentationEnd();
}

/**
 * Capitalise the phrase and bolt the attempt index on.
 *
 * The phase names in gen_budget.c are lower-case sentence fragments because
 * their first consumer was a log line ("creation progress: building the ...").
 * A caption is a heading, so the first letter is raised HERE rather than by
 * duplicating the table -- one source of truth for the words, two presentations
 * of them.
 */
static void BuildCaption(const ComboGenProgress* progress) {
    const char* phrase = (progress->detail != NULL) ? progress->detail : Combo_GenPhaseName(progress->phase);
    if (progress->attempt != 0 && progress->maxAttempts != 0) {
        snprintf(sView.caption, sizeof(sView.caption), "%s (attempt %u of %u)", phrase, (unsigned)progress->attempt,
                 (unsigned)progress->maxAttempts);
    } else if (progress->attempt != 0) {
        snprintf(sView.caption, sizeof(sView.caption), "%s (attempt %u)", phrase, (unsigned)progress->attempt);
    } else {
        snprintf(sView.caption, sizeof(sView.caption), "%s", phrase);
    }
    if (sView.caption[0] >= 'a' && sView.caption[0] <= 'z') {
        sView.caption[0] = (char)(sView.caption[0] - ('a' - 'A'));
    }
}

/**
 * Raise the watermark; never lower it (see the header on why).
 *
 * THE `>` IS THE FEATURE, and the row that proves it is
 * test_gen_progress_overlay.c's "the watermark's red half": three streams that
 * each hand this function a LOWER candidate than the current fraction (an attempt
 * clock that steps back, an earlier ladder attempt reported after a later one, a
 * lower-weight phase after a higher one). Turn this into a plain assignment and
 * all three go red. The ordinary ladder stream does NOT distinguish them — its
 * within-attempt creep is capped strictly below the next attempt's base, so it
 * stays monotone even under assignment, which is exactly why that stream alone
 * could not lock this.
 */
static void RaiseFraction(float candidate) {
    if (candidate > 1.0f) {
        candidate = 1.0f;
    }
    if (candidate > sView.fraction) {
        sView.fraction = candidate;
    }
}

/**
 * The band ONE ladder attempt owns inside the MM_FILL phase.
 *
 * Splitting MM_FILL's band by the ladder bound is what makes the ladder
 * visible: attempt 3 of 10 sits a third of the way through the fill's band
 * rather than snapping back to its start, which is what a fresh
 * elapsed/budget ratio would have done at every re-roll.
 */
static float AttemptSlice(uint8_t maxAttempts) {
    const float band = PhaseWeight(RSBS_GENPHASE_CROSSINGS) - PhaseWeight(RSBS_GENPHASE_MM_FILL);
    const unsigned n = (maxAttempts > 0) ? (unsigned)maxAttempts : 1u;
    return band / (float)n;
}

static float AttemptBase(uint8_t attempt, uint8_t maxAttempts) {
    const unsigned index = (attempt > 0) ? (unsigned)attempt - 1u : 0u;
    return PhaseWeight(RSBS_GENPHASE_MM_FILL) + (float)index * AttemptSlice(maxAttempts);
}

void ComboGenOverlay_OnProgress(const ComboGenProgress* progress) {
    if (progress == NULL) {
        return;
    }

    const bool terminal =
        progress->phase == (uint8_t)RSBS_GENPHASE_DONE || progress->phase == (uint8_t)RSBS_GENPHASE_FAILED;

    if (terminal) {
        if (sView.state != (uint8_t)RSBS_GENOVERLAY_SHOWN) {
            // Nothing was ever on screen (a headless creation, or a session
            // whose sink was installed after the fact). There is nothing to
            // dismiss, and inventing a DISMISSED state here would make a
            // painter tear down a modal it never opened.
            return;
        }
        sView.phase = progress->phase;
        sView.attempt = 0;
        sView.maxAttempts = 0;
        sView.budgetMs = 0;
        sView.elapsedMs = progress->elapsedMs;
        if (progress->phase == (uint8_t)RSBS_GENPHASE_DONE) {
            sView.state = (uint8_t)RSBS_GENOVERLAY_DISMISSED;
            RaiseFraction(1.0f);
        } else {
            // FAILED keeps the fraction where it stopped: the bar is evidence
            // of how far the creation got, and the existing file-select toast
            // (OoT_Creation_ReportFailureAtFileSelect) owns the explanation.
            sView.state = (uint8_t)RSBS_GENOVERLAY_FAILED;
        }
        BuildCaption(progress);
        // Painted once, so a presenter gets exactly one chance to tear down.
        Paint();
        return;
    }

    if (sView.state != (uint8_t)RSBS_GENOVERLAY_SHOWN) {
        // A fresh creation. Reset the watermark: the previous creation's
        // fraction must not make this one's bar start at 100%.
        const ComboGenOverlayPainter painter = sPainter;
        ComboGenOverlay_Reset();
        sPainter = painter;
        sView.state = (uint8_t)RSBS_GENOVERLAY_SHOWN;
    }

    sView.phase = progress->phase;
    sView.attempt = progress->attempt;
    sView.maxAttempts = progress->maxAttempts;
    sView.elapsedMs = progress->elapsedMs;
    sView.budgetMs = progress->budgetMs;
    if (progress->phase == (uint8_t)RSBS_GENPHASE_MM_FILL) {
        RaiseFraction(AttemptBase(progress->attempt, progress->maxAttempts));
    } else {
        RaiseFraction(PhaseWeight(progress->phase));
    }
    BuildCaption(progress);

    // A phase transition ALWAYS paints, regardless of the repaint interval: it
    // is the one event the player is waiting to see, and rate-limiting it
    // would hide a phase that happens to be short.
    sLastPaintValid = false;
    sLastPaintAttemptMs = 0xFFFFFFFFu;
    Paint();
}

bool ComboGenOverlay_Heartbeat(uint32_t attemptElapsedMs) {
    if (!ComboGenOverlay_WantsHeartbeat()) {
        return false;
    }
    if (sLastPaintValid && attemptElapsedMs >= sLastPaintAttemptMs &&
        (attemptElapsedMs - sLastPaintAttemptMs) < RSBS_GENOVERLAY_REPAINT_INTERVAL_MS) {
        return false;
    }

    // The whole-creation counter the overlay DISPLAYS. Read from the channel
    // rather than accumulated here, so the number on screen is the same one the
    // stderr leg and the P12 measurement print, and so a heartbeat cannot
    // invent a clock of its own. It is displayed, never decided with.
    sView.elapsedMs = Combo_GenProgress_ElapsedMs();

    // WITHIN-PHASE CREEP. Two shapes, both monotone in the caller's clock and
    // both strictly below the next band's start:
    //
    // (a) MM's fill, where a real budget exists: the attempt's own band times
    //     elapsed/budget, held just under the next attempt's base so a re-roll
    //     never has to move the bar backwards.
    // (b) everything else, where no total exists at all: an asymptote,
    //     t/(t+3000), which rises fast enough to look alive and provably never
    //     reaches the next phase's weight. An asymptote rather than a
    //     "pretend 30 seconds" linear ramp, because the latter would either
    //     saturate and freeze or overshoot into the next phase's band.
    if (sView.phase == (uint8_t)RSBS_GENPHASE_MM_FILL && sView.budgetMs > 0) {
        float ratio = (float)attemptElapsedMs / (float)sView.budgetMs;
        if (ratio > 1.0f) {
            ratio = 1.0f;
        }
        RaiseFraction(AttemptBase(sView.attempt, sView.maxAttempts) +
                      AttemptSlice(sView.maxAttempts) * ratio * 0.95f);
    } else {
        const float base = (sView.phase == (uint8_t)RSBS_GENPHASE_MM_FILL)
                               ? AttemptBase(sView.attempt, sView.maxAttempts)
                               : PhaseWeight(sView.phase);
        const float ceiling = (sView.phase == (uint8_t)RSBS_GENPHASE_MM_FILL)
                                  ? base + AttemptSlice(sView.maxAttempts)
                                  : NextPhaseWeight(sView.phase);
        const float t = (float)attemptElapsedMs;
        RaiseFraction(base + (ceiling - base) * (t / (t + 3000.0f)));
    }

    sLastPaintAttemptMs = attemptElapsedMs;
    sLastPaintValid = true;
    Paint();
    return true;
}
