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
 * The hints live until SDL_Quit, which clears every hint (OVERRIDE ones too).
 * The only SDL_Quit is GfxWindowBackendSDL2::Destroy, reached through
 * ~Fast3dWindow when DeinitOTR drops the window at final shutdown
 * (GameRunner_ShutdownAll in main.cpp); a --test process _Exits after its row
 * without reaching it, and no row tears the window down and shows another. A
 * row that ever does must call TestWindowFocus_ArmForTestMode again first.
 *
 * What it cannot fix: libultraship's DXGI backend shows its own HWND with
 * ShowWindow(SW_SHOW) (fast/backends/gfx_dxgi.cpp), which never consults SDL's
 * hint, so a DirectX 11 window activates regardless. Tests are run with the GL
 * backend (`shipofharkinian.json` Window.Backend.Id 1), which the staged trees do.
 */

#ifndef RSBS_TEST_WINDOW_FOCUS_H
#define RSBS_TEST_WINDOW_FOCUS_H

#ifdef __cplusplus
#include <string>

namespace Ship {
class Config;
}

/**
 * What TestWindowFocus_ForceNullAudioBackend changed on a Ship::Config, so
 * TestWindowFocus_RestoreAudioBackend can put it back exactly.
 */
struct TestSilentAudioConfigState {
    bool forced = false;  /**< the key was overridden (the process is armed and not opted out) */
    bool hadKey = false;  /**< Window.AudioBackend held a value before the override */
    std::string previous; /**< that value */
};

/**
 * @brief Force the Ship audio backend to "null" in @p config, in memory.
 *
 * Why the SDL driver pin (TestWindowFocus_ArmSilentAudio) is not enough: on
 * Windows libultraship's default backend is WASAPI, chosen by
 * Config::GetCurrentAudioBackend whenever `Window.AudioBackend` is absent or
 * "wasapi" (libultraship/src/ship/config/Config.cpp), and a WasapiAudioPlayer never
 * goes through SDL's audio driver. So a test process on a config without the key
 * (or with "sdl", which would reach a real device on a non-dummy driver) still
 * played the games' audio. Audio::Init reads that key, so overriding it before
 * Context::InitAudio makes the Ship audio player a NullAudioPlayer whatever the
 * staged config says.
 *
 * A no-op (state.forced == false) unless TestWindowFocus_ArmSilentAudio armed this
 * process, so normal play and RSBS_TEST_AUDIO=1 keep the config they were given.
 * RSBS_TEST_AUDIO_SABOTAGE=no-config skips it: the red-half switch for "the call in
 * OTRGlobals was removed".
 */
TestSilentAudioConfigState TestWindowFocus_ForceNullAudioBackend(Ship::Config* config);

/**
 * @brief Put Window.AudioBackend back to what @p state recorded (erase it if it was
 *        absent) and save the config.
 *
 * Audio::Init saves the config while the key is forced, and a persisted "null"
 * would silence the operator's own next run from the same directory (a stored
 * value outliving the process is how config contamination happens), so the
 * override is undone as soon as the audio player exists. A no-op when
 * state.forced is false. RSBS_TEST_AUDIO_SABOTAGE=no-restore skips it.
 */
void TestWindowFocus_RestoreAudioBackend(Ship::Config* config, const TestSilentAudioConfigState& state);
#endif

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
 * @brief Make this (test) process silent: default SDL's audio driver to "dummy".
 *
 * Same lifetime and call site as TestWindowFocus_ArmForTestMode (rsbs/src/main.cpp,
 * for `--test` and `--integration-test`, before SDL or Ship::Context initialise),
 * and for the same reason: a tier is run on a workstation while the person at it
 * is working. Test rows used to be silent only when the CTest row (about 49 of
 * them) or the staged `shipofharkinian.json` (`Window.AudioBackend` "null") said
 * so; a ROM-mounted row on a config without it played the games' audio through
 * the operator's speakers. Now no test process depends on either.
 *
 * Mechanics: an SDL_HINT_AUDIODRIVER hint at SDL_HINT_OVERRIDE priority, so the
 * value survives anything done to the environment afterwards (a normal-priority
 * hint is refused outright while the variable exists, and would read back as
 * whatever the variable says). An explicit non-empty `SDL_AUDIODRIVER` in the
 * environment at arm time wins: that value is what gets pinned. SDL_setenv
 * (overwrite = 0) also fills an absent variable for any code that reads the
 * environment directly.
 *
 * `RSBS_TEST_AUDIO=1` skips it (hearing the game on purpose).
 * `RSBS_TEST_AUDIO_SABOTAGE=no-default` skips it and says so on stderr: the red-half
 * switch the test-audio-silent row is proven against, standing in for "main.cpp's
 * call was removed". `no-config` and `no-restore` (comma-joinable) keep the pin but
 * skip TestWindowFocus_ForceNullAudioBackend / TestWindowFocus_RestoreAudioBackend.
 *
 * An explicit `SDL_AUDIODRIVER` in the caller's shell is pinned as it is, so
 * `SDL_AUDIODRIVER=wasapi` on a direct run is respected by the SDL layer; the Ship
 * backend is forced to "null" regardless, so it is still silent.
 *
 * Prints one `[AUDIO]` line saying what it pinned.
 */
void TestWindowFocus_ArmSilentAudio(const char* mode);

/** @brief 1 once TestWindowFocus_ArmSilentAudio has pinned a driver, else 0. */
int TestWindowFocus_IsAudioArmed(void);

/**
 * @brief Call immediately before Ship::Context::InitAudio: forces the live context's
 *        Window.AudioBackend to "null" for an armed test process (see
 *        TestWindowFocus_ForceNullAudioBackend). Safe on any process.
 */
void TestWindowFocus_BeforeInitAudio(void);

/** @brief Call immediately after Ship::Context::InitAudio: undoes BeforeInitAudio. */
void TestWindowFocus_AfterInitAudio(void);

/**
 * @brief One `[AUDIO]` diagnostics line and a verdict: the SDL audio driver in use
 *        (SDL_GetCurrentAudioDriver) if SDL audio is up, the pinned hint, and the
 *        Ship audio backend the game actually built.
 *
 * @return 0 when the process is silent: opted out (RSBS_TEST_AUDIO=1), or armed by
 *         TestWindowFocus_ArmSilentAudio with no Ship audio player other than the
 *         null one; 1 otherwise (never armed, or a sdl/wasapi player is up). The
 *         integration path fails the run on 1 (main.cpp), which is what makes
 *         deleting its arm call, or the BeforeInitAudio call, observable.
 */
int TestWindowFocus_AudioProbe(const char* where);

/** @brief 1 once any TestWindowFocus_AudioProbe call returned 1, else 0. */
int TestWindowFocus_AudioVerdictFailed(void);

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
