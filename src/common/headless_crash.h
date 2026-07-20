/**
 * @file headless_crash.h
 * @brief Unattended-safe crash handling for the single executable (issue #388)
 *
 * libultraship's Ship::CrashHandler ends every fatal fault in a MODAL dialog:
 * SDL_ShowSimpleMessageBox on Linux, MessageBoxA on Windows. With nobody to
 * click OK the process never leaves that call, so on a CI runner a crash does
 * not present as a crash — it presents as a CTest timeout, with no exit code,
 * no signal number and no traceback. Three "rando-gen hangs" were really a
 * SIGSEGV at boot; correcting that misread cost a full Linux bisect.
 *
 * Worse on Linux: the message box is called BEFORE PrintCommon(), so the crash
 * dump libultraship assembled is never even flushed to the log. The dialog
 * suppresses the very output it exists to advertise.
 *
 * This module takes crash handling back whenever the session is unattended,
 * and replaces the dialog with "write it to stderr, exit non-zero, now".
 *
 * The handler cannot be installed once at startup and left alone: the
 * Ship::CrashHandler constructor (sigaction / SetUnhandledExceptionFilter)
 * runs later, during shared context bring-up, and would overwrite it. See
 * HeadlessCrash_ClaimAndInstall for how that ordering is closed.
 */

#ifndef RSBS_HEADLESS_CRASH_H
#define RSBS_HEADLESS_CRASH_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Whether this process is running without anyone to answer a dialog.
 *
 * True when any of the following holds:
 *   - RSBS_HEADLESS is set to something other than "0"
 *   - the process is in unit-test or integration-test mode
 *   - CI is set in the environment
 *   - (POSIX) neither DISPLAY nor WAYLAND_DISPLAY is set
 *   - (Windows) the process window station is not visible, i.e. no
 *     interactive desktop to draw a message box on
 *
 * RSBS_HEADLESS=0 forces "interactive" and is the escape hatch for anyone who
 * wants libultraship's original dialog behavior back in an odd environment.
 *
 * Evaluated once and cached: the answer must not change between install time
 * and a signal arriving, and getenv is not async-signal-safe.
 */
int HeadlessCrash_IsHeadless(void);

/**
 * @brief Declare this session unattended regardless of the environment.
 *
 * For entry points that know there is no human even when the heuristics would
 * say otherwise — chiefly the unit-test harness, which runs under a live
 * DISPLAY (rando-gen needs Xvfb for Fast3dWindow bring-up) and so would
 * otherwise look like a desktop session. Must be called before the first
 * HeadlessCrash_IsHeadless query, since that answer is cached.
 */
void HeadlessCrash_ForceHeadless(void);

/**
 * @brief Claim crash handling from libultraship and install the fast-fail path.
 *
 * Call immediately after creating the Ship::Context singleton and BEFORE any
 * game init runs. Two things happen:
 *
 *  1. Ship::Context::InitCrashHandler() is called here. It is idempotent —
 *     it returns early once mCrashHandler is non-null — so the later calls
 *     from OoT's OTRGlobals and MM's BenPort become no-ops and cannot
 *     re-register libultraship's blocking handlers behind our back. This is
 *     what makes the fix cover a crash DURING game init, which is where the
 *     #388 SIGSEGV (InitOTRForMMFirstBoot -> ShipInit::InitAll) actually fired.
 *
 *  2. If the session is headless, our own handlers replace libultraship's.
 *     Interactive sessions are left completely alone: a desktop user still
 *     gets the crash dialog and the rich log dump.
 *
 * Safe to call more than once.
 */
void HeadlessCrash_ClaimAndInstall(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_HEADLESS_CRASH_H
