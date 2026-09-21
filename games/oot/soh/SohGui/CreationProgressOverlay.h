#ifndef SOH_SOHGUI_CREATION_PROGRESS_OVERLAY_H
#define SOH_SOHGUI_CREATION_PROGRESS_OVERLAY_H

/**
 * @file CreationProgressOverlay.h
 * @brief The PRESENTATION half of the paired-creation progress surface (#582).
 *
 * The state machine is `src/common/gen_progress_overlay.h` and has no idea a
 * renderer exists. This is the half that paints, and it lives in SohGui
 * because that is where the render-during-blocking-work seam already is:
 * `OTRGlobals::RunExtract` drives the ROM extraction's progress bar with
 * exactly the sequence used here, from inside a loop that blocks the same
 * thread the game later renders on.
 *
 * ONE entry point, C linkage, because the caller is the creation seam
 * (games/oot/soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp) and MM's
 * fill heartbeat reaches this indirectly through the common state machine.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Install the on-screen leg: `gen_budget`'s display sink plus the common state
 * machine's painter, and latch THIS thread as the only one allowed to touch the
 * renderer.
 *
 * Idempotent, and deliberately called from the creation seam rather than at
 * boot: the seam runs on the game/render thread, so the latch cannot pick up
 * the wrong thread, and OoT's own menu-side generation (which runs on a worker
 * thread and already has the menu's own spinner) is then filtered out by the
 * latch instead of by a guess about who calls what.
 */
void OoT_CreationProgressOverlay_Install(void);

/**
 * TEST SEAM (combo-creation-event's overlay leg). Install, stand in for the game
 * loop, and try to present ONE gui-only frame.
 *
 * @return 1 when a frame was actually presented, 0 when this process cannot
 *         present one at all (no Fast3D window, a backend that declined). The
 *         caller uses that to tell "this renderer cannot do it here" (skip the
 *         leg) from "the renderer can and the creation painted nothing" (a
 *         defect). Nothing in a shipping path calls this.
 */
int OoT_CreationProgressOverlay_TestPresentOnce(void);

/**
 * TEST SEAM. Frames actually PRESENTED since process start -- the full
 * StartDraw / StartFrame / RunGuiOnly / EndDraw / EndFrame sequence, not painter
 * invocations. The state machine's ComboGenOverlay_PaintCount() counts calls,
 * which could all have bailed at a guard; this counter can only move when a
 * frame went out, which is what "the bar paints" has to mean.
 */
uint32_t OoT_CreationProgressOverlay_TestPresentedFrames(void);

#ifdef __cplusplus
}
#endif

#endif // SOH_SOHGUI_CREATION_PROGRESS_OVERLAY_H
