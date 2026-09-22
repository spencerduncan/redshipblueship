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
 *   - OoT's mods are everything else UNDER ITS OWN ROOT: the root itself and any
 *     other subfolder. OoT keeps the root because existing installs and every
 *     upstream SoH mod distribution already put archives there.
 *
 * ONLY WHEN THE TWO ROOTS ARE THE SAME DIRECTORY. The collapse above is a
 * property of a portable build, not of the source: with `NON_PORTABLE` defined,
 * `GetAppDirectoryPath` returns `SDL_GetPrefPath(NULL, appName)` and the two
 * lookups land in genuinely different directories — and on Linux `SHIP_HOME`
 * collapses them again even then, so the answer is not a compile-time constant in
 * either direction. If OoT reserved `mods/mm` unconditionally, then in a
 * non-portable build an archive at `<soh-prefdir>/mods/mm/x.o2r` would be skipped
 * by OoT while MM only ever globs `<2s2h-prefdir>/mods/mm` — mounted by NEITHER
 * game. That is exactly the gap the #670 row's disjointness check is supposed to
 * catch, and it cannot, because it is only ever handed one root. So OoT reserves
 * the subfolder only when `Combo_ModsRootsAreShared` says MM is really globbing
 * the same tree; otherwise OoT keeps its whole tree, `mods/mm` included, exactly
 * as it did before #670. (PR #704 shipped the unconditional reservation and a PR
 * body claiming "the partition still holds" in that configuration. It did not.)
 *
 * THIS RE-HOMES ARCHIVES THAT WERE ALREADY OoT's. It is not a no-op for existing
 * installs, and an earlier version of this comment ("a subdirectory that had no
 * meaning before #670") was simply wrong. OoT's glob is, and always was, a
 * `recursive_directory_iterator` over the WHOLE tree
 * (games/oot/soh/Enhancements/mod_menu.cpp), so an archive a player already had
 * at `mods/mm/*.o2r` — a mod distributed inside a folder somebody named `mm`, a
 * Majora-themed OoT retexture pack — was enumerated, offered in OoT's mod menu
 * and mounted as an OoT mod. After this change the same file is MM's: mounted for
 * MM, and no longer mounted for OoT. The change of owner is deliberate (the
 * folder name is the only signal available in one shared tree), but it IS a
 * migration. So that it is not a silent one for the installs that actually have
 * such a file, OoT's walk warns once on stderr when it skips an archive under
 * `mods/mm/` whose name OoT's own enabled-mods CVar still lists.
 *
 * Combo_ModPathIsForGame is the single definition of that split, used by BOTH
 * globs (games/oot/soh/Enhancements/mod_menu.cpp and
 * games/mm/2s2h/GameExports_SingleExe.cpp), so the two can never disagree about
 * who owns a file. Its two sides are computed by two INDEPENDENT rules rather than
 * one bool and its negation, so that the #670 row's disjointness and totality
 * assertions have a reachable red half instead of being a restatement of the
 * return statement; the domain over which the partition is total is "paths under
 * the root", a path in some other tree belonging to neither game.
 * Combo_ModArchiveExtensionIsValid is the same arrangement for the other half of
 * the question — "is this file a mod archive at all" — because one shared tree
 * must not accept different file types in its two halves, and kModsWalkOptions is
 * the same arrangement for how the tree is WALKED.
 *
 * THE STATED ASYMMETRIES. Everything about the two halves of this one tree is
 * aligned except the following, which are listed here and in docs/MODDING.md
 * rather than papered over:
 *
 *   - Enable/disable/reorder. OoT's half has a mod menu; MM's half mounts every
 *     archive it finds, sorted by stem. Making MM read OoT's EnabledMods CVar
 *     would let a stale OoT list silently disable an MM mod, which is worse.
 *   - Registration bookkeeping. OoT registers only the archives its enabled set
 *     names, keyed by file-name STEM (so two same-stem archives in different
 *     subfolders collapse to one); MM registers every archive it mounts.
 *
 * The walk itself is NOT on that list any more: both globs take the same
 * directory_options and the same error_code discipline (see kModsWalkOptions).
 * PR #704 shipped a third, unstated divergence there — OoT followed directory
 * symlinks and MM did not, so the "a mod may ship as its own folder" installation
 * the docs bless worked under `mods/` and silently did nothing under `mods/mm/`.
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
 * The mods directory @p game globs, as LocateFileAcrossAppDirs resolves it —
 * i.e. `LocateFileAcrossAppDirs("mods", "soh"|"2s2h")` with the app short name in
 * ONE place instead of one per port. Never NULL; "" for an unknown game.
 *
 * Re-resolved on every call, because the answer changes once the folder exists
 * (MM creates `mods/mm` during boot and then globs it). The returned pointer is
 * owned by a per-game slot and stays valid until the next call FOR THE SAME game,
 * so `Combo_ModsRootsAreShared(Combo_ModsRootForGame(GAME_OOT),
 * Combo_ModsRootForGame(GAME_MM))` is well defined.
 */
const char* Combo_ModsRootForGame(GameId game);

/**
 * Do @p ootModsRoot and @p mmModsRoot name the same directory — i.e. is there
 * really ONE shared mods tree to partition?
 *
 * True in a portable build, where `GetAppDirectoryPath` ignores its appName
 * argument and both lookups land on `./mods`. False when the two resolve
 * elsewhere, which `NON_PORTABLE` does (`SDL_GetPrefPath(NULL, appName)` per app
 * name) — and true again under `SHIP_HOME` on Linux even with `NON_PORTABLE`, so
 * this is a runtime question and not a `#ifdef`.
 *
 * It is what gates OoT's `mods/mm` skip: reserving the subfolder when MM is NOT
 * globbing that tree would leave archives there mounted by neither game. Compares
 * `weakly_canonical` forms, so two spellings of one directory agree and a
 * not-yet-created directory still answers. False on a NULL/empty argument.
 */
bool Combo_ModsRootsAreShared(const char* ootModsRoot, const char* mmModsRoot);

/**
 * Does the mod file at @p path belong to @p game?
 *
 * @param modsRoot the mods directory @p game globs, as LocateFileAcrossAppDirs
 *                 returned it (e.g. "./mods").
 * @param path     a file path yielded by iterating @p modsRoot.
 *
 * GAME_MM is true exactly when @p path is under `<modsRoot>/mm` at any depth,
 * matched case-insensitively — the reserved folder name must not depend on how
 * the player typed it. GAME_OOT is true exactly when @p path is under @p modsRoot
 * and its first component is NOT that reserved folder. Two independent rules, so
 * over the paths an iteration of @p modsRoot can yield they partition with no gap
 * and no overlap, and a drift between them is detectable rather than impossible
 * to express. A path in some other tree entirely belongs to NEITHER game (the
 * pre-#704-review code answered "OoT's" for it), as does @p modsRoot itself.
 * False for an unknown game, and false for both games on a NULL/empty argument.
 *
 * NOTE: this answers "whose half of a SHARED tree is this". It is the right
 * question only when the two games' roots really are one directory; ask
 * Combo_ModsRootsAreShared first. OoT's glob does.
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
 * Is @p extension (with its leading dot, as
 * `std::filesystem::path::extension()` yields it) a mod archive this build can
 * mount? Case-insensitive; false for NULL.
 *
 * ONE rule for both halves of the shared mods/ tree, which is the point of it.
 * The rule is OoT's, unchanged: `.o2r` always; `.otr` only where the MPQ reader
 * is compiled in (INCLUDE_MPQ_SUPPORT — without it libultraship cannot read one
 * at all); and `.zip` NEVER, because a mod is most often DISTRIBUTED as a zip
 * containing the .o2r, and mounting the wrapper silently mounts nothing useful
 * while looking like success (the reason is stated in OoT's own
 * IsValidExtension).
 *
 * MM's single-exe glob used to take `.zip` too, copied from upstream BenPort
 * (games/mm/2s2h/BenPort.cpp). That made one folder tree accept different file
 * types on its two sides — the same distribution zip mounted under `mods/mm/` and
 * ignored under `mods/` — so MM now shares this rule instead. Nothing regresses:
 * MM mounted no mods at all in single-exe builds before #670, so there is no
 * installed base of MM `.zip` mods to break here.
 */
bool Combo_ModArchiveExtensionIsValid(const char* extension);

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

#include <filesystem>

namespace Rsbs {

/**
 * The ONE directory_options both globs over the shared mods/ tree use
 * (CollectOoTModFiles in games/oot/soh/Enhancements/mod_menu.cpp and
 * MountMMModArchives in games/mm/2s2h/GameExports_SingleExe.cpp).
 *
 * `follow_directory_symlink` because the documented installation for a mod that
 * ships as its own folder (docs/MODDING.md) is a folder under `mods/`, and a
 * symlinked folder is how a player keeps one library of mods in two installs. OoT
 * has always followed them; PR #704 gave MM's new glob the default (do not
 * follow), so the same trick worked under `mods/` and silently did nothing under
 * `mods/mm/` — one tree, two traversal rules, with the same user-visible symptom
 * ("my mod did nothing") the shared extension rule exists to avoid.
 *
 * `skip_permission_denied` because a player's mods folder is arbitrary user data:
 * an unreadable subdirectory must cost that subdirectory, not the whole walk.
 *
 * Both walks also drive the iterator through the `error_code` overloads for the
 * same reason: nothing in a mods folder — a broken reparse point, a deleted entry
 * mid-walk — may throw out of either game's boot path. A symlink LOOP is the one
 * case `follow_directory_symlink` makes reachable, and it surfaces there as an
 * error_code that ends the walk rather than as an exception.
 */
inline constexpr std::filesystem::directory_options kModsWalkOptions =
    std::filesystem::directory_options::follow_directory_symlink |
    std::filesystem::directory_options::skip_permission_denied;

} // namespace Rsbs
#endif

#endif /* RSBS_MOD_ARCHIVES_H */
