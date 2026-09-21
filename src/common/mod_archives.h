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
 *
 * ---------------------------------------------------------------------------
 * The shared mods/ tree (issue #670)
 * ---------------------------------------------------------------------------
 *
 * Both ports look their mods folder up with
 * `Ship::Context::LocateFileAcrossAppDirs("mods", <appShortName>)` — "soh" for
 * OoT, "2s2h" for MM. In a PORTABLE build (`NON_PORTABLE=OFF`, which is what
 * this project configures and what every release ships)
 * `Context::GetAppDirectoryPath(appName)` ignores its appName argument entirely
 * and returns "." (libultraship/src/ship/Context.cpp), so BOTH lookups resolve
 * to the SAME `./mods` directory. The per-app-name separation upstream relies on
 * exists only in non-portable builds.
 *
 * That collapse is why MM cannot simply re-use upstream BenPort's glob: it would
 * mount every OoT mod a second time and register it under GAME_MM, and the
 * switch-time re-apply above would then stack OoT's mods on top of MM's base
 * archives on every arrival in MM — a cross-game shadowing that SURVIVES the
 * switch, which is strictly worse than the bug being fixed.
 *
 * So the one shared `mods/` tree is partitioned by subdirectory, and the
 * partition is asymmetric on purpose:
 *
 *   - MM's mods are the ones under `mods/mm/` (at any depth).
 *   - OoT's mods are everything else: the root and any other subfolder. OoT
 *     keeps the root because existing installs and every upstream SoH mod
 *     distribution already put archives there; the only behavioural change on
 *     OoT's side is that it now skips `mods/mm/`, a path that had no meaning
 *     before this issue, so no existing install can depend on it.
 *
 * Combo_ModPathIsForGame is the single definition of that split, used by BOTH
 * globs (games/oot/soh/Enhancements/mod_menu.cpp and
 * games/mm/2s2h/GameExports_SingleExe.cpp), so the two can never disagree about
 * who owns a file. Total and disjoint by construction: for any path, exactly one
 * of the two games claims it.
 */

#ifndef RSBS_MOD_ARCHIVES_H
#define RSBS_MOD_ARCHIVES_H

#include "game.h" /* GameId; also pulls stdbool.h for the bool return below */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Subdirectory of the shared mods/ tree that holds @p game's mod archives,
 * relative to the mods root: "" for OoT (the root itself) and "mm" for MM.
 * Never NULL; "" for an unknown game.
 */
const char* Combo_ModsSubdirForGame(GameId game);

/**
 * Does the mod file at @p path belong to @p game?
 *
 * @param modsRoot the mods directory both games glob, as
 *                 LocateFileAcrossAppDirs returned it (e.g. "./mods").
 * @param path     a file path yielded by iterating @p modsRoot.
 *
 * GAME_MM is true exactly when @p path is under `<modsRoot>/mm` at any depth,
 * matched case-insensitively — the reserved folder name must not depend on how
 * the player typed it. GAME_OOT is the exact complement, so the two partition
 * every path between them with no gap and no overlap. False for an unknown
 * game, and false for both games on a NULL/empty argument.
 *
 * Path-shaped, not filesystem-shaped: it compares lexically normalized paths and
 * never touches the disk, so it gives the same answer for a file that has since
 * been deleted and is safe to call from inside a directory-iteration loop.
 *
 * SEPARATORS. Whatever std::filesystem::path treats as a separator on the host
 * platform, and nothing more. So '\' splits on Windows and does NOT on POSIX,
 * where it is a legal filename character — a POSIX file genuinely called
 * `mm\x.o2r` in the mods root stays OoT's rather than being handed to MM.
 * Neither caller depends on that either way: both globs pass generic_string()
 * (forward slashes) on both platforms, against a root that
 * LocateFileAcrossAppDirs built by concatenating with '/'.
 */
bool Combo_ModPathIsForGame(GameId game, const char* modsRoot, const char* path);

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
