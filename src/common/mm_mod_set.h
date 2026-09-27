/**
 * @file mm_mod_set.h
 * @brief Majora's Mask's enabled mod set: which archives under `mods/mm` are
 *        mounted, and in what order (issue #706).
 *
 * THE GAP THIS CLOSES. #670 made MM mount `mods/mm/` in the single exe, but it
 * mounted EVERY archive there, ordered by path. OoT's half of the same mods tree
 * has an enabled set persisted in `gSettings.EnabledMods` and a menu to reorder
 * it; MM's half had nothing, so a player's only controls were renaming a file to
 * move it and moving it out of the folder to disable it. That also made #593's
 * disabled-mod protection vacuous for MM: `Combo_EnsureGameArchivesLoaded` replays
 * the per-game registry (mod_archives.h) instead of re-globbing precisely so that a
 * switch cannot resurrect a disabled archive, and MM had no disabled state for it
 * to protect.
 *
 * THE MODEL. Two CVars, both MM-only preferences (never world identity, never
 * frozen):
 *
 *   RSBS_CVAR_MM_ENABLED_MODS   the enabled archives in MOUNT order, so the LAST
 *                               entry is mounted last and wins every path it ships
 *                               (ArchiveManager is last-added-wins). The menu shows
 *                               this list reversed: top of the list = highest
 *                               priority, the same reading OoT's mod menu has.
 *   RSBS_CVAR_MM_DISABLED_MODS  the archives the player turned off.
 *
 * Entries are KEYS: the archive's path relative to MM's half of the tree, with
 * '/' separators (`hd-pack.o2r`, `packs/hd-pack.o2r`). A path, not OoT's file-name
 * stem, because MM's walk is recursive and two same-named archives in different
 * subfolders are two different mods. Joined with '|' like OoT's list ('|' cannot
 * occur in an NTFS file name and almost never does in an ext4 one).
 *
 * THE RULES a scan applies, the same ones every time (Rsbs::ResolveMMModSet):
 *
 *   1. An archive in neither list is NEW and is ENABLED, appended at the top of
 *      the priority order — OoT's UpdateModFiles rule ("new files default to
 *      enabled", appended to its enabled list). With both CVars unset every
 *      archive is new, so the mount order is exactly the one #670 shipped (the
 *      stem sort, Rsbs::MMModNameLess): an existing install does not move until
 *      the player moves something.
 *   2. A key in BOTH lists is disabled. The protective answer: this model exists
 *      so that a disabled mod stays off.
 *   3. An enabled entry whose file is gone is dropped (OoT drops its missing
 *      entries too). A DISABLED entry whose file is gone is KEPT, so a mod the
 *      player turned off and then moved out of the folder for a while comes back
 *      still off rather than as "new" and therefore enabled.
 *   4. Nothing is persisted from a walk that ended early (an unreadable folder, a
 *      broken link): the archives after the break point would read as missing and
 *      then as new, which would silently re-enable a disabled one.
 *
 * WHEN CHANGES APPLY. MM mounts its mods once, at its first boot in the process
 * (LoadMMArchives, games/mm/2s2h/GameExports_SingleExe.cpp), and the switch-time
 * re-apply replays exactly what it mounted. So a change made before MM first
 * starts applies when it starts; a change made after needs a restart, as every
 * change in OoT's mod menu does. Combo_MMModSet_RestartPending() is that fact, for
 * the menu note. Live remounting is deliberately not attempted: ArchiveManager has
 * no priority field, a removed archive's paths would have to be handed back to
 * whatever mounted them before, and OoT does not attempt it either.
 *
 * Game-header-free (ADR 0002): the menu page (games/oot/soh/SohGui/
 * SohMenuComboMmMods.cpp) and MM's mount both reach this through src/common.
 */

#ifndef RSBS_MM_MOD_SET_H
#define RSBS_MM_MOD_SET_H

#include "game.h" /* GameId; stdbool.h */

/* MM's enabled archives, in mount order (last = highest priority). */
#define RSBS_CVAR_MM_ENABLED_MODS "gSettings.MM.EnabledMods"
/* MM's disabled archives. */
#define RSBS_CVAR_MM_DISABLED_MODS "gSettings.MM.DisabledMods"
/* The list separator, OoT's (games/oot/soh/Enhancements/mod_menu.cpp). */
#define RSBS_MM_MOD_SET_SEPARATOR "|"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Walk MM's half of @p modsRoot (the root itself, as Combo_ModsRootForGame(GAME_MM)
 * returns it), resolve what it finds against the two CVars and make that the
 * current model. Persists the lists when the rules above changed them and the walk
 * was complete.
 *
 * @return how many archives were found (enabled + disabled), 0 for a missing
 *         folder (the normal case), or -1 for a NULL/empty root.
 */
int Combo_MMModSet_Scan(const char* modsRoot);

/** Has anything filled the model yet (a scan, or MM's own mount)? The menu page
 *  scans lazily on its first draw when this is false. */
bool Combo_MMModSet_Scanned(void);

/** Forget the model and the session's mount record (tests; the menu rescans on
 *  its next draw). Touches no CVar. */
void Combo_MMModSet_Reset(void);

/** How many archives are enabled. */
int Combo_MMModSet_EnabledCount(void);

/**
 * The enabled archive at @p priority, 0 being the HIGHEST priority (mounted last),
 * or NULL out of range. Owned by the model; valid until the next call that changes
 * it (a scan, a mount, or any setter below).
 */
const char* Combo_MMModSet_EnabledKey(int priority);

/** How many disabled archives are present on disk (a disabled entry whose file is
 *  gone is kept in the CVar but not listed). */
int Combo_MMModSet_DisabledCount(void);

/** The @p index-th present disabled archive, in name order, or NULL. Same lifetime
 *  as Combo_MMModSet_EnabledKey. */
const char* Combo_MMModSet_DisabledKey(int index);

/**
 * Turn the archive @p key on or off, and persist. Enabling puts it at the top of
 * the priority order, where a new archive goes. False (and no change) for an
 * unknown key or one already in the requested state.
 */
bool Combo_MMModSet_SetEnabled(const char* key, bool enabled);

/** Move the enabled archive @p key one step up (Raise: it then overrides the one it
 *  passed) or down, and persist. False (no change) at the end of the list, for a
 *  disabled key or an unknown one. */
bool Combo_MMModSet_Raise(const char* key);
bool Combo_MMModSet_Lower(const char* key);

/** Has MM mounted its mods in this process? */
bool Combo_MMModSet_MountedThisSession(void);

/** MM has mounted its mods and the enabled list no longer matches what it mounted,
 *  so the difference applies only after a restart. */
bool Combo_MMModSet_RestartPending(void);

/**
 * Fill the model from an explicit list of keys, in default (stem-sort) order,
 * resolved against the CURRENT CVars exactly as a scan would, but WITHOUT
 * persisting anything. For the UI snapshot harness, which must show a populated
 * list without touching the disk or the player's settings.
 */
void Combo_MMModSet_LoadForTest(const char* const* keysInDefaultOrder, int count);

#ifdef __cplusplus
}

#include <string>
#include <vector>

namespace Rsbs {

/** One scan's answer: the lists to persist and what they mean. */
struct MMModSetResolution {
    /// Enabled keys in MOUNT order (last = highest priority), present on disk.
    std::vector<std::string> enabled;
    /// Disabled keys to persist: the CVar's entries, de-duplicated and in its
    /// order, INCLUDING absent ones (rule 3).
    std::vector<std::string> disabled;
    /// The two lists differ from the CVar values they were resolved from.
    bool changed = false;
};

/**
 * The rules in the file comment, as a pure function of what is on disk and what
 * the two CVars hold. @p discoveredInDefaultOrder is every archive the walk
 * found, as keys, in Rsbs::MMModNameLess order.
 */
MMModSetResolution ResolveMMModSet(const std::vector<std::string>& discoveredInDefaultOrder,
                                   const std::string& enabledValue, const std::string& disabledValue);

/** Split a persisted list on RSBS_MM_MOD_SET_SEPARATOR, dropping empty entries. */
std::vector<std::string> SplitModList(const std::string& value);

/** Join a list with RSBS_MM_MOD_SET_SEPARATOR. */
std::string JoinModList(const std::vector<std::string>& keys);

/**
 * MM's default mod precedence: the whole path with its extension removed,
 * compared case-insensitively. Byte-for-byte upstream 2Ship's comparator
 * (games/mm/2s2h/BenPort.cpp), so a mod behaves as in standalone 2Ship until the
 * player reorders it; moved here from GameExports_SingleExe.cpp so the menu's scan
 * and MM's mount sort with one definition.
 *
 * Two consequences worth stating because they are not obvious. Stripping the
 * extension means renaming `10-foo.otr` to `10-foo.o2r` does not move a mod in the
 * order. Comparing the whole path rather than the file name means a subfolder's
 * name participates: `mods/mm/aaa/z.o2r` sorts before `mods/mm/bbb/a.o2r`, and
 * `mods/mm/sub/20-b.o2r` after `mods/mm/30-c.o2r`.
 */
bool MMModNameLess(const std::string& a, const std::string& b);

/** One MM mod archive the walk found. */
struct MMModArchive {
    std::string path; ///< generic ('/') path, as the walk yielded it: what is mounted and registered
    std::string key;  ///< the path relative to MM's half of the tree: what the lists persist
};

/**
 * MM's walk over the shared mods tree: every archive under `<modsRoot>/mm` at any
 * depth that Combo_ModArchiveExtensionIsValid accepts, taken through
 * Combo_ModPathIsForGame(GAME_MM, ...) and Rsbs::kModsWalkOptions exactly as #670's
 * glob did (it is that glob, moved), sorted by MMModNameLess.
 *
 * @param complete set false when the walk ended early on an error; the result is
 *                 then a prefix of the tree. May be NULL.
 */
std::vector<MMModArchive> CollectMMModArchives(const std::string& modsRoot, bool* complete);

/**
 * What MM mounts, in mount order: CollectMMModArchives, resolved against the two
 * CVars (persisting when the rules changed them and the walk was complete), made
 * the current model, and recorded as this session's mount. The one production
 * caller is MountMMModArchives (games/mm/2s2h/GameExports_SingleExe.cpp); a
 * disabled archive is therefore neither mounted nor registered, so #593's re-apply
 * cannot resurrect it.
 */
std::vector<std::string> MMModArchivesToMount(const std::string& modsRoot);

} // namespace Rsbs
#endif

#endif /* RSBS_MM_MOD_SET_H */
