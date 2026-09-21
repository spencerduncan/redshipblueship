/**
 * @file gen_progress_overlay.h
 * @brief The ON-SCREEN creation-progress surface's STATE MACHINE (#582).
 *
 * ------------------------------------------------------------------------
 * WHAT WAS MISSING
 * ------------------------------------------------------------------------
 * `src/common/gen_budget.h` delivered the phase CHANNEL: the creation event
 * reports phase / attempt / elapsed / budget, a greppable stderr line always
 * fires, and a sink can be registered for presentation. Nothing presented,
 * because the paired creation runs inside `Save_InitFile` on the one thread
 * that renders (`OoT_Graph_ThreadEntry` -> `OoT_RunFrame` ->
 * `OoT_GameState_Update` -> file select -> `OoT_Sram_InitSave`), so for the
 * whole of MM's fill nothing paints and the window stops answering the OS.
 *
 * This file is the half of the fix that can be tested without a GPU: the state
 * machine a progress overlay needs, with no ImGui, no window and no game
 * headers. The presentation half (which actually pumps a frame) is
 * `games/oot/soh/SohGui/CreationProgressOverlay.cpp`; it installs itself as
 * this file's PAINTER and as `gen_budget`'s display sink.
 *
 * ------------------------------------------------------------------------
 * THE SHAPE, AND WHY IT IS A STATE MACHINE AND NOT A FLAG
 * ------------------------------------------------------------------------
 * HIDDEN -> SHOWN (a creation began) -> SHOWN (every update) ->
 *   DISMISSED (RSBS_GENPHASE_DONE: the file exists, the overlay goes away) or
 *   FAILED    (RSBS_GENPHASE_FAILED: the creation seam's existing toast owns
 *              the explanation, so the overlay only has to stop drawing over
 *              it).
 * A bool would have collapsed the last two, and they differ in the one way a
 * player notices: on success the overlay must disappear before file select
 * redraws; on failure it must get out of the way of
 * `OoT_Creation_ReportFailureAtFileSelect`'s toast rather than covering it.
 *
 * ------------------------------------------------------------------------
 * MONOTONE PROGRESS IS A PROMISE, NOT AN ESTIMATE
 * ------------------------------------------------------------------------
 * A bar that goes backwards reads as a crash. Two things here would push it
 * back if the fraction were computed fresh each time: the attempt ladder (the
 * second MM fill attempt starts over) and the within-attempt elapsed/budget
 * ratio (which resets per attempt). So the fraction is a WATERMARK: it is only
 * ever raised. The phase weights are deliberately coarse and are honest about
 * being coarse -- the creation has no measurable total, so the bar expresses
 * "how far through the phase list", plus a bounded within-attempt creep so a
 * long fill still looks alive.
 *
 * NO WALL CLOCK DECIDES ANYTHING HERE (#581 section 2a). Every millisecond
 * this file BRANCHES on is one the caller already measured for its own reasons
 * and passed in, and the only thing it decides with them is whether to paint,
 * which no world depends on. The one clock value it reads itself
 * (Combo_GenProgress_ElapsedMs, during a heartbeat) is DISPLAYED and never
 * compared against anything.
 *
 * ------------------------------------------------------------------------
 * LAYERING
 * ------------------------------------------------------------------------
 * This file is a CONSUMER of gen_budget's channel and gen_budget knows nothing
 * about it. The dependency runs one way on purpose: the channel has to keep
 * working (and its existing headless locks have to keep passing) with no
 * overlay linked at all.
 */

#ifndef RSBS_COMMON_GEN_PROGRESS_OVERLAY_H
#define RSBS_COMMON_GEN_PROGRESS_OVERLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "gen_budget.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * How often the overlay may repaint while ONE phase grinds on, in ms of the
 * caller-supplied attempt clock.
 *
 * 100 ms (10 Hz) rather than every heartbeat, because a painted frame costs a
 * buffer swap -- which on this workstation's OpenGL backend waits for vblank,
 * ~16 ms. Painting on every fill iteration would spend most of the fill in the
 * presentation path. 10 Hz is fast enough that a bar and an elapsed counter
 * read as live, and slow enough that the presentation cost stays a small
 * fraction of the fill; and whatever it does cost is CREDITED BACK to the
 * fill's budget by the caller (Combo_GenProgress_Pump, gen_budget.h), so
 * painting never buys the generation less headroom than a headless run gets.
 */
#define RSBS_GENOVERLAY_REPAINT_INTERVAL_MS 100u

typedef enum ComboGenOverlayState {
    RSBS_GENOVERLAY_HIDDEN = 0, /**< no creation in flight; nothing drawn */
    RSBS_GENOVERLAY_SHOWN,      /**< a creation is running; the overlay draws */
    RSBS_GENOVERLAY_DISMISSED,  /**< the creation succeeded; stop drawing */
    RSBS_GENOVERLAY_FAILED      /**< the creation failed; the toast owns the screen */
} ComboGenOverlayState;

/** Everything a painter needs, and nothing that would make it read a global. */
typedef struct ComboGenOverlayView {
    uint8_t state;       /**< a ComboGenOverlayState */
    uint8_t phase;       /**< the last reported ComboGenPhase */
    uint8_t attempt;     /**< 1-based ladder attempt; 0 outside MM_FILL */
    uint8_t maxAttempts; /**< the ladder bound; 0 outside MM_FILL */
    float fraction;      /**< [0,1], never decreasing within one creation */
    uint32_t elapsedMs;  /**< whole-creation elapsed, from the channel */
    uint32_t budgetMs;   /**< this attempt's budget; 0 when N/A */
    /** One line naming the phase, e.g. "Building the Majora's Mask world
     *  (attempt 2 of 10)". NUL-terminated; never empty while SHOWN. */
    char caption[128];
} ComboGenOverlayView;

/** The presentation leg. Called on the thread that reported, so whoever
 *  installs one is responsible for refusing to touch a renderer off the
 *  render thread. */
typedef void (*ComboGenOverlayPainter)(const ComboGenOverlayView* view);

/**
 * Feed one channel record in. This is the function to hand to
 * Combo_GenProgress_SetDisplaySink().
 *
 * Advances the state machine, rebuilds the caption, raises the fraction
 * watermark, and paints once -- a phase transition always paints, because it
 * is the thing the player is waiting to see change.
 */
void ComboGenOverlay_OnProgress(const ComboGenProgress* progress);

/**
 * Keep the overlay alive DURING a phase that has not ended yet.
 *
 * @param attemptElapsedMs the caller's own elapsed-in-this-attempt clock, used
 *        only to rate-limit repaints and to creep the bar.
 * @return true when this call painted.
 */
bool ComboGenOverlay_Heartbeat(uint32_t attemptElapsedMs);

/** True when a painter is installed AND a creation is on screen -- i.e. when a
 *  heartbeat would do something. Cheap enough to call in a fill's inner loop. */
bool ComboGenOverlay_WantsHeartbeat(void);

/** Install / remove the presentation leg. NULL leaves the state machine
 *  running and silent, which is exactly what a headless row wants. */
void ComboGenOverlay_SetPainter(ComboGenOverlayPainter painter);

/** The current view. Never NULL. */
const ComboGenOverlayView* ComboGenOverlay_View(void);

/** Back to HIDDEN with a zeroed view. For the locks, and for a host that wants
 *  to guarantee no stale overlay survives a refused creation. */
void ComboGenOverlay_Reset(void);

/** How many times a painter has been invoked since the last reset. The locks
 *  assert paint CADENCE, and a counter is the only way to see it without a
 *  window. */
uint32_t ComboGenOverlay_PaintCount(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_GEN_PROGRESS_OVERLAY_H
