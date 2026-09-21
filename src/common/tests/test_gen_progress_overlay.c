/**
 * @file test_gen_progress_overlay.c
 * @brief ROM-free, display-free lock for the creation-progress surface (#582).
 *
 * The on-screen bar has two halves. The painting half needs a GPU and a window
 * and is covered by the file comment in
 * games/oot/soh/SohGui/CreationProgressOverlay.cpp plus a real playtest; this
 * row locks the half that can be driven headlessly, which is the half every
 * "the bar looked wrong" bug actually lives in:
 *
 *  1. THE CHANNEL FEEDS BOTH LEGS INDEPENDENTLY. gen_budget.h grew a second
 *     sink slot for the overlay precisely so the existing combo-creation-event
 *     row's phase-order recorder could not be displaced by the shipped display
 *     leg. If one slot ever silently replaced the other again, that row would go
 *     green while asserting nothing — so BOTH sinks are installed here and both
 *     are checked, and the display leg is checked to hear Begin while the
 *     phase-order leg is checked NOT to.
 *
 *  2. PHASES ARRIVE IN ORDER, AND THE FRACTION NEVER GOES BACKWARDS. The
 *     attempt ladder is the reason this is not trivially true: attempt 2 of the
 *     MM fill starts its own elapsed/budget ratio at zero, and a bar computed
 *     fresh from that ratio would visibly rewind at every re-roll — which reads
 *     as a crash to a player watching a 90-second creation.
 *
 *  3. THE STATE MACHINE'S TERMINAL EDGES ARE DISTINCT. shown -> dismissed and
 *     shown -> failed are different states because the presenter does different
 *     things with them (disappear vs. get out of the failure toast's way), and a
 *     bool would have collapsed them. Also asserted: a terminal report with
 *     nothing on screen does NOT invent a dismissal, because a presenter must
 *     never tear down a modal it never opened.
 *
 *  4. THE HEARTBEAT IS RATE-LIMITED, AND A PHASE TRANSITION IS NOT. The fill
 *     calls the heartbeat on every iteration of its inner loop; painting each
 *     one would spend the fill in the presentation path. A transition must paint
 *     regardless, or a short phase never appears at all.
 *
 *  5. NO PAINTER INSTALLED IS A COMPLETE NO-OP. This is what makes every other
 *     headless row — and the digest comparisons — see no new behaviour: the fill
 *     asks ComboGenOverlay_WantsHeartbeat() before it does anything at all.
 *
 *  6. A SECOND CREATION STARTS FROM ZERO. The fraction is a watermark, so
 *     failing to reset it would leave the next file's bar pinned at 100%.
 *
 * Deliberately absent: anything about appearance, and anything about the
 * gSaveContext paint bracket (that one needs a real creation and is exercised by
 * combo-creation-event's byte-exact leg, which would fail if the bracket's new
 * swap did not restore).
 *
 * Linkage note: #included into test_runner.cpp at file scope like its siblings.
 */

#include "../gen_progress_overlay.h"
#include "../test_runner.h"

#include <stdio.h>
#include <string.h>

#define GPO_ASSERT(cond)                                                   \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return TEST_FAIL;                                              \
        }                                                                  \
    } while (0)

// ---- observers --------------------------------------------------------------

#define GPO_MAX_EVENTS 64

static uint8_t sPhaseLegOrder[GPO_MAX_EVENTS];
static int sPhaseLegCount = 0;

static uint8_t sDisplayLegOrder[GPO_MAX_EVENTS];
static int sDisplayLegCount = 0;

/** Stands in for the existing combo-creation-event recorder: the leg that must
 *  keep working unchanged now that a second one exists. */
static void GpoPhaseLeg(const ComboGenProgress* progress) {
    if (progress != NULL && sPhaseLegCount < GPO_MAX_EVENTS) {
        sPhaseLegOrder[sPhaseLegCount++] = progress->phase;
    }
}

/** Stands in for the shipped display leg AND drives the state machine, which is
 *  exactly what CreationProgressOverlay.cpp's sink does. */
static void GpoDisplayLeg(const ComboGenProgress* progress) {
    if (progress != NULL && sDisplayLegCount < GPO_MAX_EVENTS) {
        sDisplayLegOrder[sDisplayLegCount++] = progress->phase;
    }
    ComboGenOverlay_OnProgress(progress);
}

/** Sized for the whole synthetic creation below (three 30-second attempts at the
 *  repaint interval, plus transitions) so the monotonicity sweep sees EVERY
 *  paint rather than a prefix of them. */
static float sPaintedFractions[2048];
static int sPaintedCount = 0;
static uint8_t sLastPaintedState = 0;
static char sLastPaintedCaption[128];

static void GpoPainter(const ComboGenOverlayView* view) {
    if (view == NULL) {
        return;
    }
    if (sPaintedCount < (int)(sizeof(sPaintedFractions) / sizeof(sPaintedFractions[0]))) {
        sPaintedFractions[sPaintedCount++] = view->fraction;
    }
    sLastPaintedState = view->state;
    snprintf(sLastPaintedCaption, sizeof(sLastPaintedCaption), "%s", view->caption);
}

static void GpoResetObservers(void) {
    sPhaseLegCount = 0;
    sDisplayLegCount = 0;
    sPaintedCount = 0;
    sLastPaintedState = 0;
    sLastPaintedCaption[0] = '\0';
}

static int GpoIndexOf(const uint8_t* order, int count, uint8_t phase) {
    for (int i = 0; i < count; i++) {
        if (order[i] == phase) {
            return i;
        }
    }
    return -1;
}

TestResult Test_GenProgressOverlay(void) {
    printf("[TEST] gen-progress-overlay: the creation-progress surface's phase order, monotone bar and state "
           "machine (#582)\n");

    // ------------------------------------------------------------------
    // 5 — with no painter installed, nothing the fill calls does anything.
    //     Asserted FIRST, before a painter exists, so it cannot be an artefact
    //     of teardown order.
    // ------------------------------------------------------------------
    ComboGenOverlay_Reset();
    ComboGenOverlay_SetPainter(NULL);
    GPO_ASSERT(!ComboGenOverlay_WantsHeartbeat());
    GPO_ASSERT(!ComboGenOverlay_Heartbeat(0));
    GPO_ASSERT(!ComboGenOverlay_Heartbeat(100000));
    GPO_ASSERT(ComboGenOverlay_PaintCount() == 0);
    GPO_ASSERT(ComboGenOverlay_View()->state == (uint8_t)RSBS_GENOVERLAY_HIDDEN);
    printf("[TEST] no painter: WantsHeartbeat false, zero paints\n");

    // ------------------------------------------------------------------
    // 3b — a terminal report with nothing on screen must not fabricate a
    //      dismissal for a presenter to act on.
    // ------------------------------------------------------------------
    GpoResetObservers();
    ComboGenOverlay_SetPainter(&GpoPainter);
    {
        ComboGenProgress orphan;
        memset(&orphan, 0, sizeof(orphan));
        orphan.phase = (uint8_t)RSBS_GENPHASE_DONE;
        orphan.detail = "done";
        ComboGenOverlay_OnProgress(&orphan);
    }
    GPO_ASSERT(ComboGenOverlay_View()->state == (uint8_t)RSBS_GENOVERLAY_HIDDEN);
    GPO_ASSERT(sPaintedCount == 0);
    printf("[TEST] a terminal report with no creation on screen stays HIDDEN and paints nothing\n");

    // ------------------------------------------------------------------
    // 1, 2, 4 — a synthetic creation through the REAL channel, with both legs
    //           installed. The phase stream mirrors what the creation seam and
    //           MM's ladder actually report.
    // ------------------------------------------------------------------
    GpoResetObservers();
    ComboGenOverlay_Reset();
    ComboGenOverlay_SetPainter(&GpoPainter);
    Combo_GenProgress_SetSink(&GpoPhaseLeg);
    Combo_GenProgress_SetDisplaySink(&GpoDisplayLeg);

    Combo_GenProgress_Begin();
    // 1 — Begin reaches the display leg (an overlay must exist before the first
    // phase) and NOT the phase-order leg (a synthetic entry would shift every
    // index the existing row asserts on).
    GPO_ASSERT(sDisplayLegCount == 1);
    GPO_ASSERT(sDisplayLegOrder[0] == (uint8_t)RSBS_GENPHASE_IDLE);
    GPO_ASSERT(sPhaseLegCount == 0);
    GPO_ASSERT(ComboGenOverlay_View()->state == (uint8_t)RSBS_GENOVERLAY_SHOWN);
    GPO_ASSERT(sPaintedCount == 1);
    printf("[TEST] Begin: display leg 1 event, phase-order leg 0, overlay SHOWN after 1 paint\n");

    Combo_GenProgress_SetMaxAttempts(10);

    float fractionAfterFirstAttempt = 0.0f;
    float fractionAfterThirdAttempt = 0.0f;
    for (int attempt = 1; attempt <= 3; attempt++) {
        const int paintsBefore = sPaintedCount;
        Combo_GenProgress_Report((uint8_t)RSBS_GENPHASE_MM_FILL, attempt, NULL);
        // 4b — a transition always paints, even though the previous heartbeat
        // may have painted microseconds ago.
        GPO_ASSERT(sPaintedCount == paintsBefore + 1);
        GPO_ASSERT(ComboGenOverlay_View()->attempt == (uint8_t)attempt);
        GPO_ASSERT(ComboGenOverlay_View()->maxAttempts == 10);
        // The caption names the attempt: this is the whole reason the attempt
        // index is on the surface at all.
        GPO_ASSERT(strstr(sLastPaintedCaption, "attempt") != NULL);
        if (attempt == 1) {
            GPO_ASSERT(strstr(sLastPaintedCaption, "1 of 10") != NULL);
            // Capitalised: the phase names are log fragments, the caption is a
            // heading.
            GPO_ASSERT(sLastPaintedCaption[0] >= 'A' && sLastPaintedCaption[0] <= 'Z');
        }

        // 4a — the heartbeat's rate limit, in the caller's own milliseconds.
        // The first heartbeat of a phase always lands; a second one 1 ms later
        // must not.
        GPO_ASSERT(ComboGenOverlay_WantsHeartbeat());
        GPO_ASSERT(ComboGenOverlay_Heartbeat(0));
        GPO_ASSERT(!ComboGenOverlay_Heartbeat(1));
        GPO_ASSERT(!ComboGenOverlay_Heartbeat(RSBS_GENOVERLAY_REPAINT_INTERVAL_MS - 1));
        GPO_ASSERT(ComboGenOverlay_Heartbeat(RSBS_GENOVERLAY_REPAINT_INTERVAL_MS));
        // ...and a long grind keeps painting, at the interval.
        for (uint32_t t = 2 * RSBS_GENOVERLAY_REPAINT_INTERVAL_MS; t <= 30000u;
             t += RSBS_GENOVERLAY_REPAINT_INTERVAL_MS) {
            GPO_ASSERT(ComboGenOverlay_Heartbeat(t));
        }
        if (attempt == 1) {
            fractionAfterFirstAttempt = ComboGenOverlay_View()->fraction;
        }
        if (attempt == 3) {
            fractionAfterThirdAttempt = ComboGenOverlay_View()->fraction;
        }
    }

    // 2 — the ladder ADVANCED the bar rather than rewinding it. A fresh
    // elapsed/budget ratio at attempt 3 would have put the bar back where
    // attempt 1 started.
    GPO_ASSERT(fractionAfterThirdAttempt > fractionAfterFirstAttempt);
    printf("[TEST] ladder: fraction after attempt 1 = %.4f, after attempt 3 = %.4f (advanced, not rewound)\n",
           (double)fractionAfterFirstAttempt, (double)fractionAfterThirdAttempt);

    Combo_GenProgress_Report((uint8_t)RSBS_GENPHASE_PUBLISH, 0, NULL);
    GPO_ASSERT(ComboGenOverlay_View()->attempt == 0);
    // Outside MM_FILL there is no budget to show, so there must not be one.
    GPO_ASSERT(ComboGenOverlay_View()->budgetMs == 0);

    Combo_GenProgress_End(true);

    // 3a — the success edge.
    GPO_ASSERT(ComboGenOverlay_View()->state == (uint8_t)RSBS_GENOVERLAY_DISMISSED);
    GPO_ASSERT(sLastPaintedState == (uint8_t)RSBS_GENOVERLAY_DISMISSED);
    GPO_ASSERT(ComboGenOverlay_View()->fraction >= 0.999f);
    // The presenter got exactly one chance to tear down, and the heartbeat is
    // dead the moment the overlay is not SHOWN — otherwise a fill that kept
    // looping after the creation ended would resurrect the bar.
    GPO_ASSERT(!ComboGenOverlay_WantsHeartbeat());
    GPO_ASSERT(!ComboGenOverlay_Heartbeat(1000000));
    printf("[TEST] success edge: DISMISSED at fraction %.4f, heartbeat inert afterwards\n",
           (double)ComboGenOverlay_View()->fraction);

    // 2 — MONOTONICITY over every paint of the whole creation. This is the
    // assertion the whole "watermark, not estimate" design exists for.
    GPO_ASSERT(sPaintedCount > 10);
    for (int i = 1; i < sPaintedCount; i++) {
        if (sPaintedFractions[i] < sPaintedFractions[i - 1]) {
            printf("[TEST] FAIL: the bar went backwards at paint %d (%.6f after %.6f)\n", i,
                   (double)sPaintedFractions[i], (double)sPaintedFractions[i - 1]);
            return TEST_FAIL;
        }
        if (sPaintedFractions[i] < 0.0f || sPaintedFractions[i] > 1.0f) {
            printf("[TEST] FAIL: paint %d reported a fraction outside [0,1] (%.6f)\n", i,
                   (double)sPaintedFractions[i]);
            return TEST_FAIL;
        }
    }
    printf("[TEST] monotone over %d paints, first %.4f last %.4f\n", sPaintedCount, (double)sPaintedFractions[0],
           (double)sPaintedFractions[sPaintedCount - 1]);

    // 1 — the phase-order leg saw the transitions, in order, and nothing else.
    {
        const int fillAt = GpoIndexOf(sPhaseLegOrder, sPhaseLegCount, (uint8_t)RSBS_GENPHASE_MM_FILL);
        const int publishAt = GpoIndexOf(sPhaseLegOrder, sPhaseLegCount, (uint8_t)RSBS_GENPHASE_PUBLISH);
        const int doneAt = GpoIndexOf(sPhaseLegOrder, sPhaseLegCount, (uint8_t)RSBS_GENPHASE_DONE);
        GPO_ASSERT(fillAt >= 0 && publishAt > fillAt && doneAt > publishAt);
        GPO_ASSERT(GpoIndexOf(sPhaseLegOrder, sPhaseLegCount, (uint8_t)RSBS_GENPHASE_IDLE) < 0);
        printf("[TEST] phase-order leg: fill %d < publish %d < done %d, no synthetic idle entry\n", fillAt, publishAt,
               doneAt);
    }

    // ------------------------------------------------------------------
    // 6 + 3c — a SECOND creation starts from zero and can end on the failure
    //          edge.
    // ------------------------------------------------------------------
    GpoResetObservers();
    Combo_GenProgress_Begin();
    GPO_ASSERT(ComboGenOverlay_View()->state == (uint8_t)RSBS_GENOVERLAY_SHOWN);
    GPO_ASSERT(ComboGenOverlay_View()->fraction < 0.5f);
    const float fractionAtSecondBegin = ComboGenOverlay_View()->fraction;
    Combo_GenProgress_SetMaxAttempts(10);
    Combo_GenProgress_Report((uint8_t)RSBS_GENPHASE_MM_FILL, 1, NULL);
    const float fractionBeforeFailure = ComboGenOverlay_View()->fraction;
    Combo_GenProgress_End(false);
    GPO_ASSERT(ComboGenOverlay_View()->state == (uint8_t)RSBS_GENOVERLAY_FAILED);
    GPO_ASSERT(sLastPaintedState == (uint8_t)RSBS_GENOVERLAY_FAILED);
    // A failure does NOT complete the bar: where it stopped is evidence, and the
    // file-select toast owns the explanation.
    GPO_ASSERT(ComboGenOverlay_View()->fraction < 0.999f);
    GPO_ASSERT(ComboGenOverlay_View()->fraction >= fractionBeforeFailure);
    GPO_ASSERT(!ComboGenOverlay_WantsHeartbeat());
    printf("[TEST] second creation restarted at %.4f and failed at %.4f (not 100%%)\n", (double)fractionAtSecondBegin,
           (double)ComboGenOverlay_View()->fraction);

    // Leave the channel as it was found: these slots are process-global and
    // later rows in the same binary read them.
    Combo_GenProgress_SetSink(NULL);
    Combo_GenProgress_SetDisplaySink(NULL);
    ComboGenOverlay_SetPainter(NULL);
    ComboGenOverlay_Reset();

    printf("[TEST] PASS: gen-progress-overlay\n");
    return TEST_PASS;
}
