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
 * RE-ARMING, NOT ONCE-ONLY, and all three slots follow that one rule: a call
 * either arms the latch, the painter and the display sink together, or arms none
 * of them. (It used to latch the thread unconditionally and arm the slots only the
 * first time, so the latch was re-pointable while the slots were not -- anything
 * that cleared them disarmed the overlay for the rest of the process.)
 *
 * IT CAN REFUSE. When this process has nothing presentable -- no Fast3D window, no
 * Gui, or no frame has ever been presented -- it arms NOTHING and returns, leaving
 * `ComboGenOverlay_WantsHeartbeat()` false. That is what makes a headless creation
 * take the pre-#582 code path exactly: MM's fill never reads its clock a second
 * time and never accumulates a presentation credit, rather than calling into
 * guards that bail.
 *
 * Deliberately called from the creation seam rather than at boot: the seam runs on
 * the game/render thread, so the latch cannot pick up the wrong thread, and OoT's
 * own menu-side generation (which runs on a worker thread and already has the
 * menu's own spinner) is then filtered out by the latch instead of by a guess
 * about who calls what.
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

/** TEST SEAM. The RSBS_GENOVERLAY_REFUSED_* code (src/common/gen_progress_overlay.h,
 *  which is where the codes live so the display-free runner can read them without
 *  reaching into SohGui) from the last paint attempt. */
int OoT_CreationProgressOverlay_TestLastRefusal(void);

/**
 * TEST SEAM. 1 when the install armed this process's slots, 0 when it refused
 * because nothing here is presentable. The row uses it to tell "the overlay is
 * wired" from "this box cannot present", and to check that a call after the slots
 * were cleared RE-arms them.
 */
int OoT_CreationProgressOverlay_TestIsArmed(void);

/**
 * TEST SEAM. 1 when the last frame this file presented was submitted with
 * ImGuiConfigFlags_NoMouse and ImGuiConfigFlags_NoKeyboard both in force, read
 * from the live io INSIDE that frame.
 *
 * A pumped frame draws SoH's whole menu (Gui::StartDraw -> DrawMenu, Gui::EndDraw
 * -> DrawFloatingWindows) while the creation seam's gSaveContext bracket is
 * active, so a click landing on a menu handler would run it re-entrantly in the
 * middle of the creation. The suppression is what stops that, and this is how a
 * row sees it rather than taking a comment's word for it.
 */
int OoT_CreationProgressOverlay_TestLastFrameSuppressedInput(void);

#ifdef __cplusplus
}
#endif

#endif // SOH_SOHGUI_CREATION_PROGRESS_OVERLAY_H
