/**
 * @file frame_capture.h
 * @brief In-process frame capture for playtest evidence (#843). Test-only,
 *        env-gated, OpenGL only; a no-op unless a knob below is set.
 *
 * WHY IT EXISTS. Agent-driven playtests used to screenshot the game window from
 * outside the process (PrintWindow). That returns an all-black image whenever
 * the desktop is locked or the display is off, which is exactly when the work
 * runs unattended. This capture reads the window's own back buffer from inside
 * the frame, after the game AND ImGui have drawn and before the swap, so it
 * sees what a player would see whatever the desktop is doing.
 *
 * KNOBS (read once, at the first call into this file):
 *   RSBS_CAPTURE_FRAMES=<n>[,<n>...] | every:<k>
 *       Live frames to capture, counted per game from that game's first frame
 *       (each graph.c's frame update calls FrameCapture_OnGameFrame). Frame n is
 *       written as <dir>/<game>-frame-<n>.png, game = "oot" or "mm". A frame the
 *       renderer drops is captured on the next frame that draws.
 *   RSBS_CAPTURE_OUT=<dir>
 *       Where the PNGs go (created when missing). Defaults to RSBS_GP_SHOT_DIR
 *       (the older MM-only playtest knob), then to "frame-capture" in the cwd.
 *   RSBS_CAPTURE_VERIFY=1
 *       The IntGameplayRoundtripCapture row's lock: every listed frame of BOTH
 *       games must exist on disk at the round trip's PASS, and at least one per
 *       game must not be uniform (FrameCapture_Verify). Files from a previous
 *       run are deleted when the integration mode starts, so a stale file can
 *       never pass it.
 * The capture is active when RSBS_CAPTURE_FRAMES, RSBS_CAPTURE_OUT or
 * RSBS_GP_SHOT_DIR is set. Drives that know the moment better than a frame
 * number call IntegrationTest_CaptureFrame("<name>"): <dir>/<name>.png, taken
 * at the end of the next frame that draws.
 *
 * HOW. Arming a capture installs (once per ImGui context) a RenderPre context
 * hook. On the armed frame the hook appends a draw callback to the END of the
 * main viewport's foreground draw list, the last list ImGui renders, so when
 * the OpenGL backend reaches the callback the game frame and every ImGui window
 * are in framebuffer 0's back buffer. The callback reads it with glReadPixels
 * (RGBA8) and writes it through the UI snapshot harness's PNG writer
 * (ui_snapshot_image.h). Any other backend logs once and captures nothing.
 */
#ifndef RSBS_FRAME_CAPTURE_H
#define RSBS_FRAME_CAPTURE_H

#include "game.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** One live frame of `game` is about to draw (both graph.c frame updates). */
void FrameCapture_OnGameFrame(GameId game);

/** Capture the next frame that draws as <dir>/<name>.png. A no-op while the capture is off. */
void IntegrationTest_CaptureFrame(const char* name);

/** True when any capture knob is set. */
bool FrameCapture_IsActive(void);

/** True when RSBS_CAPTURE_VERIFY=1 (the lock row). */
bool FrameCapture_VerifyRequested(void);

/**
 * Delete the files RSBS_CAPTURE_VERIFY will look for, so only this run's
 * captures can satisfy it. Called when the integration mode is selected.
 */
void FrameCapture_ResetForVerify(void);

/**
 * The lock: every listed frame of both games is on disk, and at least one per
 * game is not uniform. Writes the verdict (pass or the first failure) to `msg`.
 */
bool FrameCapture_Verify(char* msg, size_t cap);

#ifdef __cplusplus
}
#endif

#endif // RSBS_FRAME_CAPTURE_H
