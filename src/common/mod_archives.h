/**
 * @file mod_archives.h
 * @brief Per-game registry of user mod archives, so a game switch cannot
 *        silently revoke them (issue #593).
 *
 * The problem this exists to solve:
 *
 * libultraship's ArchiveManager has no priority field. `AddArchive` walks the
 * archive's file list and overwrites `mFileToArchive[hash]` unconditionally,
 * so resolution is LAST-ADDED-WINS, globally
 * (libultraship/src/ship/resource/archive/ArchiveManager.cpp AddArchive). That
 * is the entire mechanism by which a user mod overrides a base asset: both
 * ports mount `mods/*.o2r` AFTER the base archives and the mod therefore wins
 * every path it ships.
 *
 * `EnsureGameArchivesLoaded` (rsbs/src/main.cpp) re-adds the destination game's
 * base archives on EVERY cross-game switch — it has to, that is the #154 fix.
 * Re-adding them puts them back on top, which silently un-does every mod
 * override. OoT is worse off than MM because OoT mounts mods late, at GUI init
 * (games/oot/soh/Enhancements/mod_menu.cpp UpdateModFiles), so the very first
 * return to OoT clobbers them; MM mounts them in its initial archive list
 * (games/mm/2s2h/BenPort.cpp InitOTR) and so survives until its second arrival.
 * Either way the failure is silent: the mods are still "enabled" in the menu,
 * they just stop applying.
 *
 * The fix is not to stop re-adding the base archives (they must be re-added)
 * but to re-apply the mods on top of them afterwards, in the order the game
 * originally mounted them. This registry is how the switch code knows what
 * "the mods" are without reaching into either port's private state, and
 * without re-globbing `mods/` (which would resurrect archives the player
 * DISABLED — OoT's enabled set is a CVar-persisted subset of the folder).
 *
 * Contract:
 *   - Each port calls Combo_RegisterModArchive once per archive it mounts from
 *     its mods folder, at mount time, in mount order.
 *   - Registration is idempotent per (game, path): re-registering an already
 *     known path keeps its ORIGINAL position, because relative mod precedence
 *     is user-visible (OoT's mod menu reorders it deliberately).
 *   - The registry records paths only. It never mounts anything itself.
 */

#ifndef RSBS_MOD_ARCHIVES_H
#define RSBS_MOD_ARCHIVES_H

#include "game.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Record that @p game mounted the mod archive at @p path.
 * No-op for a NULL/empty path, an unknown game, or a duplicate registration.
 */
void Combo_RegisterModArchive(GameId game, const char* path);

/** @return how many mod archives @p game has registered. */
int Combo_GetModArchiveCount(GameId game);

/**
 * @return the @p index-th registered mod archive path for @p game in mount
 *         order, or NULL if @p index is out of range. The returned pointer is
 *         owned by the registry and stays valid until Combo_ClearModArchives.
 */
const char* Combo_GetModArchive(GameId game, int index);

/** Drop every registration for @p game (tests). */
void Combo_ClearModArchives(GameId game);

/**
 * The switch-time archive (re)mount: adds @p targetGame's base archives to the
 * shared ArchiveManager and then re-applies its registered mod archives on top.
 *
 * Defined in rsbs/src/main.cpp (it is the switch loop's own step, called before
 * every GameRunner_SwitchTo). Declared here rather than left file-static so the
 * #593 lock can drive the PRODUCTION function instead of a re-implementation of
 * it — a re-implementation would keep passing after someone deleted the
 * re-apply from the real one. No-op when there is no live Ship::Context.
 */
void Combo_EnsureGameArchivesLoaded(GameId targetGame);

#ifdef __cplusplus
}
#endif

#endif /* RSBS_MOD_ARCHIVES_H */
