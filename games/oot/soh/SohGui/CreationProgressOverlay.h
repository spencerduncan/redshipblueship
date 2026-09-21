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

#ifdef __cplusplus
}
#endif

#endif // SOH_SOHGUI_CREATION_PROGRESS_OVERLAY_H
