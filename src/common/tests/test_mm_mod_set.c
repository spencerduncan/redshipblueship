/**
 * @file test_mm_mod_set.c
 * @brief #706: Majora's Mask's enabled mod set — enable, disable and reorder
 *        round-trip through the persisted lists, and the rules a scan applies.
 *
 * CTest row MMModSet ("mm-mod-set", label "redship"). ROM-free and display-free:
 * it needs the console variables (the shared display-free bring-up) and a staged
 * directory of EMPTY files, because the walk never opens an archive — so it never
 * skips, unlike MMModsMount, which proves the same set drives the real mount and
 * the switch-time re-apply.
 *
 * WHAT IT ASSERTS, against the production functions (src/common/mm_mod_set.cpp):
 *
 *   A. Rsbs::ResolveMMModSet, the rules as a pure function: unset lists reproduce
 *      the walk's order (so an existing install does not move); a new archive is
 *      enabled at the TOP of the priority order; a key in both lists is disabled;
 *      a missing enabled entry is dropped and a missing DISABLED one is kept; and
 *      "changed" is true exactly when the persisted lists would differ.
 *   B. The scan over a staged tree: MM's half only (the root-level archive is
 *      OoT's), nested archives keyed by their path inside mods/mm, `.otr` in and
 *      `.zip`/`.txt` out (the shared extension rule), and the default lists
 *      persisted.
 *   C. Disable, raise, lower and enable through the C API the menu page calls,
 *      each persisted, then a fresh scan from the persisted lists reproducing the
 *      same model — the round trip.
 *   D. A disabled archive whose file leaves the folder and comes back is still
 *      disabled (rule 3); an archive added later arrives enabled at the top.
 *   E. MM's mount record: nothing pending before MM mounts, nothing pending right
 *      after, pending after a reorder, and not pending once the order is put back.
 *   F. Rule 4, a missing mods/mm (#706's review): with the player's lists stored
 *      and no mods/mm folder (and with no mods root at all), neither a scan nor
 *      MM's own mount writes the lists, the model is read-only, and every setter
 *      refuses; once the folder exists the next scan drops the absent enabled
 *      entry and keeps the absent disabled one, as OoT does with its folder.
 *   G. Rule 4, a walk that ended early (#706's review): a real walk with its last
 *      archive cut off, adopted as incomplete, writes nothing, is read-only, and
 *      refuses every setter, so a click on the page cannot persist the truncated
 *      order; a complete rescan restores the stored order and editing.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++).
 */

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#include "../mm_mod_set.h"

namespace {

#define MMS_ASSERT(cond, code, ...)                   \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL(%d): ", (code));      \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            return (code);                            \
        }                                             \
    } while (0)

std::string MmsCVar(const char* key) {
    const char* v = CVarGetString(key, "");
    return v != nullptr ? std::string(v) : std::string();
}

std::string MmsJoin(const std::vector<std::string>& v) {
    return Rsbs::JoinModList(v);
}

/** The enabled list as the menu shows it, highest priority first. */
std::string MmsShownEnabled() {
    std::vector<std::string> keys;
    for (int i = 0; i < Combo_MMModSet_EnabledCount(); i++) {
        keys.push_back(Combo_MMModSet_EnabledKey(i));
    }
    return MmsJoin(keys);
}

std::string MmsShownDisabled() {
    std::vector<std::string> keys;
    for (int i = 0; i < Combo_MMModSet_DisabledCount(); i++) {
        keys.push_back(Combo_MMModSet_DisabledKey(i));
    }
    return MmsJoin(keys);
}

bool MmsTouch(const std::filesystem::path& p) {
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    std::FILE* f = std::fopen(p.generic_string().c_str(), "wb");
    if (f == nullptr) {
        return false;
    }
    std::fclose(f);
    return std::filesystem::is_regular_file(p, ec);
}

int MmsResolveLegs() {
    using Rsbs::ResolveMMModSet;
    const std::vector<std::string> disk = { "a.o2r", "b.o2r", "c.o2r" };

    // Unset lists: the walk's order, persisted.
    auto r = ResolveMMModSet(disk, "", "");
    MMS_ASSERT(MmsJoin(r.enabled) == "a.o2r|b.o2r|c.o2r" && r.disabled.empty() && r.changed, 1,
               "unset lists resolved to enabled '%s' changed=%d, expected the walk order and changed",
               MmsJoin(r.enabled).c_str(), (int)r.changed);
    // Nothing on disk and nothing stored: nothing to write, so a plain install's
    // config never gains the keys.
    r = ResolveMMModSet({}, "", "");
    MMS_ASSERT(r.enabled.empty() && r.disabled.empty() && !r.changed, 1, "an empty folder reported changed");

    // A stored order is kept; a new archive goes to the TOP (mounted last).
    r = ResolveMMModSet(disk, "b.o2r|a.o2r", "");
    MMS_ASSERT(MmsJoin(r.enabled) == "b.o2r|a.o2r|c.o2r" && r.changed, 2,
               "stored order + new archive resolved to '%s', expected b|a|c (c new, at the top)",
               MmsJoin(r.enabled).c_str());
    // Resolving what was just persisted changes nothing.
    r = ResolveMMModSet(disk, "b.o2r|a.o2r|c.o2r", "");
    MMS_ASSERT(!r.changed, 2, "a resolved list re-resolved as changed");

    // In both lists: disabled.
    r = ResolveMMModSet(disk, "a.o2r|b.o2r|c.o2r", "b.o2r");
    MMS_ASSERT(MmsJoin(r.enabled) == "a.o2r|c.o2r" && MmsJoin(r.disabled) == "b.o2r" && r.changed, 3,
               "a key in both lists resolved to enabled '%s' disabled '%s', expected it disabled only",
               MmsJoin(r.enabled).c_str(), MmsJoin(r.disabled).c_str());

    // Missing enabled entry dropped; missing disabled entry kept.
    r = ResolveMMModSet({ "a.o2r" }, "gone.o2r|a.o2r", "away.o2r");
    MMS_ASSERT(MmsJoin(r.enabled) == "a.o2r" && MmsJoin(r.disabled) == "away.o2r", 4,
               "missing entries resolved to enabled '%s' disabled '%s', expected a.o2r and away.o2r",
               MmsJoin(r.enabled).c_str(), MmsJoin(r.disabled).c_str());

    // Duplicates and empty separators collapse.
    r = ResolveMMModSet(disk, "|a.o2r||a.o2r|b.o2r|", "c.o2r|c.o2r");
    MMS_ASSERT(MmsJoin(r.enabled) == "a.o2r|b.o2r" && MmsJoin(r.disabled) == "c.o2r" && r.changed, 5,
               "duplicates resolved to enabled '%s' disabled '%s'", MmsJoin(r.enabled).c_str(),
               MmsJoin(r.disabled).c_str());
    MMS_ASSERT(Rsbs::SplitModList("|x||y|").size() == 2 && Rsbs::JoinModList({ "x", "y" }) == "x|y", 5,
               "split/join do not round-trip");
    return 0;
}

int MmsTreeLegs(const std::filesystem::path& root) {
    const std::string rootStr = root.generic_string();
    // ---- B: the scan -------------------------------------------------------
    for (const char* rel :
         { "root-level.o2r", "mm/10-a.o2r", "mm/sub/20-b.o2r", "mm/30-c.otr", "mm/40-d.zip", "mm/notes.txt" }) {
        MMS_ASSERT(MmsTouch(root / rel), 10, "could not stage %s under %s", rel, rootStr.c_str());
    }
    MMS_ASSERT(Combo_MMModSet_Scan(nullptr) == -1 && Combo_MMModSet_Scan("") == -1, 10,
               "a NULL/empty root did not report -1");
    const int found = Combo_MMModSet_Scan(rootStr.c_str());
    MMS_ASSERT(found == 3 && Combo_MMModSet_Scanned(), 11,
               "the scan found %d archive(s), expected 3 (10-a, sub/20-b, 30-c.otr; the root-level archive is OoT's, "
               "and .zip/.txt are not mod archives)",
               found);
    // The stem sort compares whole paths, so the subfolder's name participates:
    // "sub/20-b" sorts after "30-c" and is mounted last (upstream 2Ship's order).
    MMS_ASSERT(MmsShownEnabled() == "sub/20-b.o2r|30-c.otr|10-a.o2r" && MmsShownDisabled().empty(), 11,
               "the default list shows '%s' (disabled '%s'), expected the stem order reversed: sub/20-b on top",
               MmsShownEnabled().c_str(), MmsShownDisabled().c_str());
    MMS_ASSERT(MmsCVar(RSBS_CVAR_MM_ENABLED_MODS) == "10-a.o2r|30-c.otr|sub/20-b.o2r" &&
                   MmsCVar(RSBS_CVAR_MM_DISABLED_MODS).empty(),
               12, "the default lists were persisted as enabled '%s' disabled '%s'",
               MmsCVar(RSBS_CVAR_MM_ENABLED_MODS).c_str(), MmsCVar(RSBS_CVAR_MM_DISABLED_MODS).c_str());

    // ---- C: disable, raise, lower, enable, and the round trip ---------------
    MMS_ASSERT(Combo_MMModSet_SetEnabled("sub/20-b.o2r", false), 13, "disabling sub/20-b.o2r was refused");
    MMS_ASSERT(MmsShownEnabled() == "30-c.otr|10-a.o2r" && MmsShownDisabled() == "sub/20-b.o2r", 13,
               "after disabling: enabled '%s' disabled '%s'", MmsShownEnabled().c_str(), MmsShownDisabled().c_str());
    MMS_ASSERT(MmsCVar(RSBS_CVAR_MM_ENABLED_MODS) == "10-a.o2r|30-c.otr" &&
                   MmsCVar(RSBS_CVAR_MM_DISABLED_MODS) == "sub/20-b.o2r",
               13, "disabling was not persisted: enabled '%s' disabled '%s'",
               MmsCVar(RSBS_CVAR_MM_ENABLED_MODS).c_str(), MmsCVar(RSBS_CVAR_MM_DISABLED_MODS).c_str());

    MMS_ASSERT(Combo_MMModSet_Raise("10-a.o2r"), 14, "raising 10-a.o2r was refused");
    MMS_ASSERT(MmsShownEnabled() == "10-a.o2r|30-c.otr" && MmsCVar(RSBS_CVAR_MM_ENABLED_MODS) == "30-c.otr|10-a.o2r",
               14, "after raising 10-a: shown '%s' persisted '%s'", MmsShownEnabled().c_str(),
               MmsCVar(RSBS_CVAR_MM_ENABLED_MODS).c_str());
    MMS_ASSERT(!Combo_MMModSet_Raise("10-a.o2r") && !Combo_MMModSet_Lower("30-c.otr"), 14,
               "moving past the end of the list was accepted");
    MMS_ASSERT(!Combo_MMModSet_Raise("sub/20-b.o2r") && !Combo_MMModSet_Raise("nope.o2r") &&
                   !Combo_MMModSet_SetEnabled("nope.o2r", true) && !Combo_MMModSet_SetEnabled("10-a.o2r", true) &&
                   !Combo_MMModSet_SetEnabled("sub/20-b.o2r", false) && !Combo_MMModSet_Raise(nullptr),
               14, "a move of a disabled/unknown key, or a no-op enable/disable, was accepted");
    MMS_ASSERT(MmsCVar(RSBS_CVAR_MM_ENABLED_MODS) == "30-c.otr|10-a.o2r", 14,
               "a refused edit still rewrote the list ('%s')", MmsCVar(RSBS_CVAR_MM_ENABLED_MODS).c_str());

    // The round trip: forget the model and rebuild it from the persisted lists.
    Combo_MMModSet_Reset();
    MMS_ASSERT(!Combo_MMModSet_Scanned() && Combo_MMModSet_EnabledCount() == 0, 15, "Reset left a model behind");
    MMS_ASSERT(Combo_MMModSet_Scan(rootStr.c_str()) == 3, 15, "the rescan did not find the 3 archives");
    MMS_ASSERT(MmsShownEnabled() == "10-a.o2r|30-c.otr" && MmsShownDisabled() == "sub/20-b.o2r", 15,
               "the round trip rebuilt enabled '%s' disabled '%s', expected 10-a|30-c and sub/20-b",
               MmsShownEnabled().c_str(), MmsShownDisabled().c_str());

    MMS_ASSERT(Combo_MMModSet_Lower("10-a.o2r") && MmsShownEnabled() == "30-c.otr|10-a.o2r", 16,
               "lowering 10-a gave '%s'", MmsShownEnabled().c_str());
    MMS_ASSERT(Combo_MMModSet_SetEnabled("sub/20-b.o2r", true), 16, "re-enabling sub/20-b.o2r was refused");
    MMS_ASSERT(MmsShownEnabled() == "sub/20-b.o2r|30-c.otr|10-a.o2r" && MmsShownDisabled().empty() &&
                   MmsCVar(RSBS_CVAR_MM_DISABLED_MODS).empty(),
               16, "a re-enabled mod is not at the top: '%s' (disabled '%s')", MmsShownEnabled().c_str(),
               MmsCVar(RSBS_CVAR_MM_DISABLED_MODS).c_str());

    // ---- D: a disabled mod stays disabled across leaving the folder ---------
    MMS_ASSERT(Combo_MMModSet_SetEnabled("30-c.otr", false), 17, "disabling 30-c.otr was refused");
    std::error_code ec;
    std::filesystem::remove(root / "mm" / "30-c.otr", ec);
    MMS_ASSERT(Combo_MMModSet_Scan(rootStr.c_str()) == 2, 17, "the rescan after removing 30-c.otr did not find 2");
    MMS_ASSERT(MmsShownDisabled().empty() && MmsCVar(RSBS_CVAR_MM_DISABLED_MODS) == "30-c.otr", 17,
               "a disabled mod whose file left: shown '%s', persisted '%s' — expected hidden but kept",
               MmsShownDisabled().c_str(), MmsCVar(RSBS_CVAR_MM_DISABLED_MODS).c_str());
    MMS_ASSERT(MmsTouch(root / "mm" / "30-c.otr"), 17, "could not restore 30-c.otr");
    MMS_ASSERT(Combo_MMModSet_Scan(rootStr.c_str()) == 3, 17, "the rescan after restoring 30-c.otr did not find 3");
    MMS_ASSERT(MmsShownDisabled() == "30-c.otr" && MmsShownEnabled() == "sub/20-b.o2r|10-a.o2r", 17,
               "a disabled mod that came back is enabled '%s' / disabled '%s' — it must come back disabled",
               MmsShownEnabled().c_str(), MmsShownDisabled().c_str());

    // An archive added later arrives enabled at the top; a missing enabled entry
    // leaves the persisted list.
    MMS_ASSERT(MmsTouch(root / "mm" / "05-new.o2r"), 18, "could not stage 05-new.o2r");
    std::filesystem::remove(root / "mm" / "10-a.o2r", ec);
    MMS_ASSERT(Combo_MMModSet_Scan(rootStr.c_str()) == 3, 18, "the rescan with 05-new added and 10-a removed");
    MMS_ASSERT(MmsShownEnabled() == "05-new.o2r|sub/20-b.o2r" &&
                   MmsCVar(RSBS_CVAR_MM_ENABLED_MODS) == "sub/20-b.o2r|05-new.o2r",
               18, "new/missing handling gave shown '%s' persisted '%s', expected 05-new on top and 10-a dropped",
               MmsShownEnabled().c_str(), MmsCVar(RSBS_CVAR_MM_ENABLED_MODS).c_str());

    // ---- E: MM's mount record and the restart note -------------------------
    Combo_MMModSet_Reset();
    MMS_ASSERT(!Combo_MMModSet_MountedThisSession() && !Combo_MMModSet_RestartPending(), 19,
               "a fresh model reports a mount");
    const std::vector<std::string> toMount = Rsbs::MMModArchivesToMount(rootStr);
    MMS_ASSERT(toMount.size() == 2 && toMount[0].find("sub/20-b.o2r") != std::string::npos &&
                   toMount[1].find("05-new.o2r") != std::string::npos,
               19, "MM would mount %zu archive(s) (first '%s'), expected sub/20-b then 05-new, and never 30-c",
               toMount.size(), toMount.empty() ? "" : toMount[0].c_str());
    MMS_ASSERT(Combo_MMModSet_MountedThisSession() && !Combo_MMModSet_RestartPending(), 19,
               "right after the mount: mounted=%d pending=%d", (int)Combo_MMModSet_MountedThisSession(),
               (int)Combo_MMModSet_RestartPending());
    MMS_ASSERT(Combo_MMModSet_Lower("05-new.o2r") && Combo_MMModSet_RestartPending(), 19,
               "a reorder after MM mounted does not report a pending restart");
    MMS_ASSERT(Combo_MMModSet_Raise("05-new.o2r") && !Combo_MMModSet_RestartPending(), 19,
               "putting the order back still reports a pending restart");
    MMS_ASSERT(Combo_MMModSet_SetEnabled("30-c.otr", true) && Combo_MMModSet_RestartPending(), 19,
               "enabling a mod after MM mounted does not report a pending restart");
    return 0;
}

/** The two persisted lists, as one string, for "nothing was written" checks. */
std::string MmsStored() {
    return MmsCVar(RSBS_CVAR_MM_ENABLED_MODS) + " / " + MmsCVar(RSBS_CVAR_MM_DISABLED_MODS);
}

/** Every setter, on keys that exist in the model: all must refuse while it is
 *  read-only. */
bool MmsAnySetterAccepted(const char* enabledKey, const char* disabledKey) {
    return Combo_MMModSet_Raise(enabledKey) || Combo_MMModSet_Lower(enabledKey) ||
           Combo_MMModSet_SetEnabled(enabledKey, false) || Combo_MMModSet_SetEnabled(disabledKey, true);
}

int MmsMissingFolderLegs(const std::filesystem::path& root) {
    const std::string rootStr = root.generic_string();
    // ---- F: no mods/mm, the lists stored -----------------------------------
    // The root exists and holds an OoT archive, so the walk runs and finds
    // nothing of MM's: the case where the pre-review code dropped every enabled
    // entry and cleared the CVar.
    MMS_ASSERT(MmsTouch(root / "root-level.o2r"), 20, "could not stage %s", rootStr.c_str());
    CVarSetString(RSBS_CVAR_MM_ENABLED_MODS, "keep-a.o2r|keep-b.o2r");
    CVarSetString(RSBS_CVAR_MM_DISABLED_MODS, "keep-off.o2r");
    const std::string stored = MmsStored();
    Combo_MMModSet_Reset();
    MMS_ASSERT(Combo_MMModSet_Scan(rootStr.c_str()) == 0 && Combo_MMModSet_Scanned(), 20,
               "a scan of a root with no mods/mm did not find 0");
    MMS_ASSERT(MmsStored() == stored, 20,
               "a scan with mods/mm missing rewrote the lists to '%s' (were '%s'): OoT leaves its list alone when its "
               "folder is missing, and MM must too",
               MmsStored().c_str(), stored.c_str());
    MMS_ASSERT(!Combo_MMModSet_Editable(), 20, "the model from a missing mods/mm is editable");
    Combo_MMModSet_Reset();
    MMS_ASSERT(Rsbs::MMModArchivesToMount(rootStr).empty() && MmsStored() == stored, 20,
               "MM's own mount with mods/mm missing rewrote the lists to '%s' (were '%s')", MmsStored().c_str(),
               stored.c_str());
    // No mods root at all: the same.
    Combo_MMModSet_Reset();
    MMS_ASSERT(Combo_MMModSet_Scan((rootStr + "/does-not-exist").c_str()) == 0 && MmsStored() == stored &&
                   !Combo_MMModSet_Editable(),
               20, "a scan of a missing root rewrote the lists to '%s' or is editable", MmsStored().c_str());

    // The folder comes back with one of the two enabled archives: now the rules
    // apply, the absent enabled entry goes and the absent disabled one stays.
    MMS_ASSERT(MmsTouch(root / "mm" / "keep-b.o2r"), 21, "could not stage mm/keep-b.o2r");
    Combo_MMModSet_Reset();
    MMS_ASSERT(Combo_MMModSet_Scan(rootStr.c_str()) == 1 && Combo_MMModSet_Editable(), 21,
               "the scan with mods/mm back did not find 1 editable archive");
    MMS_ASSERT(MmsCVar(RSBS_CVAR_MM_ENABLED_MODS) == "keep-b.o2r" &&
                   MmsCVar(RSBS_CVAR_MM_DISABLED_MODS) == "keep-off.o2r",
               21, "with mods/mm present the lists became '%s', expected keep-b.o2r / keep-off.o2r",
               MmsStored().c_str());
    // An EMPTY mods/mm is a present folder (MM creates it at boot): the rules apply.
    std::error_code ec;
    std::filesystem::remove(root / "mm" / "keep-b.o2r", ec);
    Combo_MMModSet_Reset();
    MMS_ASSERT(Combo_MMModSet_Scan(rootStr.c_str()) == 0 && Combo_MMModSet_Editable() &&
                   MmsCVar(RSBS_CVAR_MM_ENABLED_MODS).empty() &&
                   MmsCVar(RSBS_CVAR_MM_DISABLED_MODS) == "keep-off.o2r",
               21, "an empty but present mods/mm gave editable=%d and lists '%s', expected editable, nothing "
               "enabled and keep-off kept",
               (int)Combo_MMModSet_Editable(), MmsStored().c_str());
    return 0;
}

int MmsPartialWalkLegs(const std::filesystem::path& root) {
    const std::string rootStr = root.generic_string();
    // ---- G: a walk that ended early ----------------------------------------
    for (const char* rel : { "mm/10-a.o2r", "mm/20-b.o2r", "mm/30-c.o2r", "mm/40-off.o2r" }) {
        MMS_ASSERT(MmsTouch(root / rel), 22, "could not stage %s under %s", rel, rootStr.c_str());
    }
    // The player's order, 30-c on top, and 40-off turned off.
    CVarSetString(RSBS_CVAR_MM_ENABLED_MODS, "10-a.o2r|20-b.o2r|30-c.o2r");
    CVarSetString(RSBS_CVAR_MM_DISABLED_MODS, "40-off.o2r");
    const std::string stored = MmsStored();

    Rsbs::MMModWalkStatus status;
    std::vector<Rsbs::MMModArchive> walked = Rsbs::CollectMMModArchives(rootStr, &status);
    MMS_ASSERT(walked.size() == 4 && status.complete && status.folderPresent && status.Persistable(), 22,
               "precondition: the real walk found %zu archive(s), complete=%d folderPresent=%d, expected 4, 1, 1",
               walked.size(), (int)status.complete, (int)status.folderPresent);
    // A walk that broke after 20-b: the prefix it would have yielded, reported
    // incomplete. (An iteration error cannot be produced on demand from a real
    // folder, so the walk's own result is cut where one would have cut it.)
    walked.resize(2);
    status.complete = false;
    Combo_MMModSet_Reset();
    Rsbs::AdoptMMModWalk(walked, status);
    MMS_ASSERT(MmsStored() == stored, 22,
               "adopting a partial walk rewrote the lists to '%s' (were '%s'): 30-c would lose its place",
               MmsStored().c_str(), stored.c_str());
    MMS_ASSERT(!Combo_MMModSet_Editable() && MmsShownEnabled() == "20-b.o2r|10-a.o2r", 22,
               "the partial model is editable=%d and shows '%s', expected read-only and 20-b|10-a",
               (int)Combo_MMModSet_Editable(), MmsShownEnabled().c_str());
    // The page's four arrows, on keys the partial model holds.
    MMS_ASSERT(!MmsAnySetterAccepted("10-a.o2r", "40-off.o2r") && !MmsAnySetterAccepted("20-b.o2r", "40-off.o2r"),
               22, "a setter accepted an edit on the read-only partial model");
    MMS_ASSERT(MmsStored() == stored && MmsShownEnabled() == "20-b.o2r|10-a.o2r", 22,
               "a refused edit on the partial model changed the lists to '%s' or the model to '%s'",
               MmsStored().c_str(), MmsShownEnabled().c_str());

    // A complete rescan: the stored order is back, and so is editing.
    MMS_ASSERT(Combo_MMModSet_Scan(rootStr.c_str()) == 4 && Combo_MMModSet_Editable(), 23,
               "the complete rescan did not find 4 editable archives");
    MMS_ASSERT(MmsShownEnabled() == "30-c.o2r|20-b.o2r|10-a.o2r" && MmsShownDisabled() == "40-off.o2r" &&
                   MmsStored() == stored,
               23, "after the complete rescan: shown '%s' / '%s', stored '%s'", MmsShownEnabled().c_str(),
               MmsShownDisabled().c_str(), MmsStored().c_str());
    MMS_ASSERT(Combo_MMModSet_Lower("30-c.o2r") && MmsCVar(RSBS_CVAR_MM_ENABLED_MODS) == "10-a.o2r|30-c.o2r|20-b.o2r",
               23, "an edit after the complete rescan was refused or not persisted ('%s')",
               MmsCVar(RSBS_CVAR_MM_ENABLED_MODS).c_str());
    return 0;
}

} // namespace

extern "C" int MMModSet_RunHeadless(void) {
    printf("[TEST] mm-mod-set: MM's enabled mod set — enable, disable and reorder round-trip; the scan rules (#706)\n");

    const std::string savedEnabled = MmsCVar(RSBS_CVAR_MM_ENABLED_MODS);
    const std::string savedDisabled = MmsCVar(RSBS_CVAR_MM_DISABLED_MODS);
    CVarClear(RSBS_CVAR_MM_ENABLED_MODS);
    CVarClear(RSBS_CVAR_MM_DISABLED_MODS);
    Combo_MMModSet_Reset();

    int rc = MmsResolveLegs();
    std::error_code ec;
    const std::filesystem::path root = std::filesystem::current_path(ec) / "rsbs_test_mod_set_706";
    std::filesystem::remove_all(root, ec);
    if (rc == 0) {
        rc = MmsTreeLegs(root);
    }
    std::filesystem::remove_all(root, ec);
    if (rc == 0) {
        Combo_MMModSet_Reset();
        rc = MmsMissingFolderLegs(root);
    }
    std::filesystem::remove_all(root, ec);
    if (rc == 0) {
        Combo_MMModSet_Reset();
        rc = MmsPartialWalkLegs(root);
    }
    std::filesystem::remove_all(root, ec);

    Combo_MMModSet_Reset();
    if (savedEnabled.empty()) {
        CVarClear(RSBS_CVAR_MM_ENABLED_MODS);
    } else {
        CVarSetString(RSBS_CVAR_MM_ENABLED_MODS, savedEnabled.c_str());
    }
    if (savedDisabled.empty()) {
        CVarClear(RSBS_CVAR_MM_DISABLED_MODS);
    } else {
        CVarSetString(RSBS_CVAR_MM_DISABLED_MODS, savedDisabled.c_str());
    }
    if (rc == 0) {
        printf(
            "[mm-mod-set] PASS: the rules (unset = walk order, new on top, both = disabled, missing enabled dropped, "
            "missing disabled kept), the scan's partition and extension rule, disable/raise/lower/enable persisted "
            "and rebuilt identically from the lists, a disabled mod that leaves and returns is still disabled, "
            "the restart note follows MM's mount, and a missing mods/mm or a walk that ended early writes nothing "
            "and refuses every edit\n");
    }
    return rc;
}
