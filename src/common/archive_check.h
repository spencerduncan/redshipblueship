/**
 * @file archive_check.h
 * @brief ROM-archive availability checks for the single executable (issue #317)
 *
 * The single exe can only run a game whose ROM-derived archive is present
 * (oot.o2r / oot-mq.o2r for OoT, mm.o2r / mm.zip / mm.otr for MM). OoT can
 * regenerate its archive through the bundled in-app extractor on a cold boot,
 * but MM has no in-app extraction path yet — its archive must be brought by
 * the user (see docs/mm-archive-setup.md).
 *
 * These helpers let the harness (rsbs/src/main.cpp) verify availability
 * up front and show an instructive message instead of failing mid-init or
 * mid-switch.
 */

#ifndef RSBS_COMMON_ARCHIVE_CHECK_H
#define RSBS_COMMON_ARCHIVE_CHECK_H

#include <stdbool.h>
#include "game.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Check whether the ROM-derived archives needed to run `game` are present.
 *
 * Searches the same locations the games use at init time
 * (Ship::Context::LocateFileAcrossAppDirs): the per-app data directory,
 * the directory of the executable, and the current working directory.
 *
 * @return true if at least one accepted archive exists (or game is GAME_NONE)
 */
bool ArchiveCheck_GameAvailable(GameId game);

/**
 * Report a missing archive for `game`: prints step-by-step instructions to
 * stderr (which file is missing, where the app looked, how to install one)
 * and, if showDialog is true, also shows a message box so GUI-launched users
 * see it. Safe to call before any window exists.
 */
void ArchiveCheck_ReportMissing(GameId game, bool showDialog);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_ARCHIVE_CHECK_H
