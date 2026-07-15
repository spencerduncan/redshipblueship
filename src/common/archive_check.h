/**
 * @file archive_check.h
 * @brief ROM-archive availability checks for the single executable (issue #317)
 *
 * The single exe can only run a game whose ROM-derived archive is present
 * (oot.o2r / oot-mq.o2r for OoT, mm.o2r / mm.zip / mm.otr for MM). Either
 * game can regenerate its archive through a bundled in-app extractor on a
 * cold boot: OoT during its own init (RunExtract), MM through the start-
 * prompt offer the harness drives via MM_Extract_OfferAndRun (see
 * rsbs/src/main.cpp and docs/mm-archive-setup.md).
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

/**
 * Check whether the game's PORT-ASSET archive is present. Unlike the
 * ROM-derived archives above, these ship with RedShipBlueShip itself
 * (2ship.o2r for MM — fonts, port textures, overlays from
 * games/mm/assets/custom) and cannot be regenerated from a ROM, so a miss
 * means an incomplete install, not a missing ROM. Probes the same locations
 * LoadMMArchives uses: next to the executable, then the working directory.
 *
 * OoT's port archive (soh.o2r) is loaded and validated by its own port
 * layer, so this returns true for every game except GAME_MM.
 */
bool ArchiveCheck_PortArchiveAvailable(GameId game);

/**
 * Report a missing port-asset archive: explains that the install/download is
 * incomplete (re-download, don't re-extract a ROM). Mirrors
 * ArchiveCheck_ReportMissing's stderr + optional message-box behavior.
 */
void ArchiveCheck_ReportMissingPortArchive(GameId game, bool showDialog);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_ARCHIVE_CHECK_H
