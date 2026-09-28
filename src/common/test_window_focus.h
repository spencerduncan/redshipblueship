/**
 * @file test_window_focus.h
 * @brief Test windows never take keyboard focus from the person at the workstation
 *
 * Every `redship --test <name>` row that brings up a window (the rando tier, the
 * `ui` harness, anything that calls InitOTRForMMFirstBoot) used to show that
 * window the normal way, and on Windows a normally shown window is ACTIVATED: it
 * takes the foreground and the keyboard caret from whatever the person at the
 * workstation was typing into. A gate runs ~180 such processes, so the operator
 * lost focus once per row for the length of a tier.
 *
 * SDL already knows how to show a window without activating it
 * (SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN: SW_SHOWNA instead of SW_SHOW on
 * Windows). This module arms that hint, and SDL_HINT_FORCE_RAISEWINDOW="0", for a
 * test process, before any window can exist; normal play never calls it, so a
 * player's window still comes to the front.
 *
 * The hints are set at SDL_HINT_OVERRIDE priority on purpose. SDL2 refuses a
 * NORMAL-priority SDL_SetHint whenever an environment variable of the same name
 * exists, and SDL_GetHint prefers that variable over a normal-priority value, so a
 * stray `SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN=0` in the environment would silently
 * win over a plain SDL_SetHint. OVERRIDE is the only priority that beats it.
 *
 * What it cannot fix: libultraship's DXGI backend shows its own HWND with
 * ShowWindow(SW_SHOW) (fast/backends/gfx_dxgi.cpp), which never consults SDL's
 * hint, so a DirectX 11 window activates regardless. Tests are run with the GL
 * backend (`shipofharkinian.json` Window.Backend.Id 1), which the staged trees do.
 */

#ifndef RSBS_TEST_WINDOW_FOCUS_H
#define RSBS_TEST_WINDOW_FOCUS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Arm the no-activation policy for this (test) process.
 *
 * Call before anything can create a window: rsbs/src/main.cpp calls it for
 * `--test` and `--integration-test` before handing control to the test runner or
 * creating the Ship::Context. Never called on the normal-play path.
 *
 * @p mode names the caller for the diagnostics line ("--test", ...).
 *
 * RSBS_TEST_FOCUS_SABOTAGE=no-hint skips the arming (and says so on stderr): the
 * red-half switch the test-window-no-activation row is proven against, standing in
 * for "main.cpp's call was removed".
 */
void TestWindowFocus_ArmForTestMode(const char* mode);

/** @brief 1 once TestWindowFocus_ArmForTestMode has set the hints, else 0. */
int TestWindowFocus_IsArmed(void);

/**
 * @brief One `[FOCUS]` diagnostics line: is SDL video up, and does any SDL window
 *        hold keyboard focus right now.
 *
 * SDL's keyboard focus is updated synchronously by WM_ACTIVATE / WM_SETFOCUS on
 * Windows, so a window the shell activated reads as focused here without any
 * event pumping. Safe before SDL_Init (reports "video=0").
 *
 * @return 1 if an SDL window holds keyboard focus, else 0.
 */
int TestWindowFocus_Probe(const char* where);

/** Outcome of TestWindowFocus_ShowProbeWindow. */
typedef struct TestWindowFocusProbeResult {
    int ran;               /**< 0 when no video subsystem could be brought up (no display) */
    int keyboardFocus;     /**< SDL_GetKeyboardFocus() was the probe window after the frames */
    int inputFocusFlag;    /**< SDL_WINDOW_INPUT_FOCUS was set on the probe window */
    int focusGainedEvents; /**< SDL_WINDOWEVENT_FOCUS_GAINED / TAKE_FOCUS seen for it */
    char why[256];         /**< why it did not run, when ran == 0 */
} TestWindowFocusProbeResult;

/**
 * @brief Show a real, plain SDL window the way a test boot does, pump a few frames,
 *        and report whether it took keyboard focus. Destroys it afterwards.
 *
 * Brings the SDL video subsystem up itself if it is not already, and takes it
 * down again in that case. With no display (a hosted Linux runner's redship
 * tier) it reports ran == 0 instead of failing.
 */
void TestWindowFocus_ShowProbeWindow(TestWindowFocusProbeResult* out);

#ifdef __cplusplus
}
#endif

#endif /* RSBS_TEST_WINDOW_FOCUS_H */
