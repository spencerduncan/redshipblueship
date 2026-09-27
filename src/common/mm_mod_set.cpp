/**
 * @file mm_mod_set.cpp
 * @brief Majora's Mask's enabled mod set (#706). See mm_mod_set.h for the model,
 *        its rules and when a change applies.
 */

#include "mm_mod_set.h"

#include "mod_archives.h"

#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <system_error>
#include <vector>

namespace {

/**
 * The model the menu page reads and MM's mount fills. Touched from the thread
 * that draws the menu and runs the game loop (one thread in this binary), so it
 * carries no lock; the C accessors hand out pointers into it, valid until the
 * next change, which is the same contract as Combo_GetModArchive.
 */
struct MMModSetModel {
    bool scanned = false;
    /// Enabled keys in mount order (last = highest priority).
    std::vector<std::string> enabled;
    /// The persisted disabled list: present keys AND absent ones (rule 3).
    std::vector<std::string> disabled;
    /// Every key the last walk found.
    std::set<std::string> present;
    /// The present disabled keys, in name order: what the menu lists.
    std::vector<std::string> disabledShown;
    /// What MM mounted this session, in mount order.
    bool mounted = false;
    std::vector<std::string> mountedOrder;
};

MMModSetModel sModel;

bool StoreAvailable() {
    // CreateUninitializedInstance leaves ConsoleVariables null until the shared
    // bring-up runs, and the bridge would dereference it (the same guard as
    // Combo_ComboSettingStoreAvailable).
    auto ctx = Ship::Context::GetInstance();
    return ctx != nullptr && ctx->GetConsoleVariables() != nullptr;
}

std::string ReadList(const char* cvar) {
    if (!StoreAvailable()) {
        return std::string();
    }
    const char* value = CVarGetString(cvar, "");
    return value != nullptr ? std::string(value) : std::string();
}

void WriteList(const char* cvar, const std::vector<std::string>& keys) {
    const std::string value = Rsbs::JoinModList(keys);
    if (value.empty()) {
        CVarClear(cvar);
    } else {
        CVarSetString(cvar, value.c_str());
    }
}

/** Write both lists and ask the GUI to save the config on its next frame, the way
 *  every SoH CVar widget does (and OoT's mod menu does after changing its list). */
void Persist() {
    if (!StoreAvailable()) {
        return;
    }
    WriteList(RSBS_CVAR_MM_ENABLED_MODS, sModel.enabled);
    WriteList(RSBS_CVAR_MM_DISABLED_MODS, sModel.disabled);
    auto ctx = Ship::Context::GetInstance();
    if (ctx->GetWindow() != nullptr && ctx->GetWindow()->GetGui() != nullptr) {
        ctx->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
}

void RebuildDisabledShown() {
    sModel.disabledShown.clear();
    for (const std::string& key : sModel.disabled) {
        if (sModel.present.count(key) != 0) {
            sModel.disabledShown.push_back(key);
        }
    }
    std::sort(sModel.disabledShown.begin(), sModel.disabledShown.end(), Rsbs::MMModNameLess);
}

/** Make @p resolution the model, over the keys @p discovered. */
void Adopt(const std::vector<std::string>& discovered, const Rsbs::MMModSetResolution& resolution) {
    sModel.scanned = true;
    sModel.present = std::set<std::string>(discovered.begin(), discovered.end());
    sModel.enabled = resolution.enabled;
    sModel.disabled = resolution.disabled;
    RebuildDisabledShown();
}

/** Resolve @p archives against the CVars, adopt the answer, and persist it when the
 *  rules changed the lists and the walk that found them was complete (rule 4). */
void ResolveAndAdopt(const std::vector<Rsbs::MMModArchive>& archives, bool complete) {
    std::vector<std::string> keys;
    keys.reserve(archives.size());
    for (const Rsbs::MMModArchive& a : archives) {
        keys.push_back(a.key);
    }
    const Rsbs::MMModSetResolution resolution =
        Rsbs::ResolveMMModSet(keys, ReadList(RSBS_CVAR_MM_ENABLED_MODS), ReadList(RSBS_CVAR_MM_DISABLED_MODS));
    Adopt(keys, resolution);
    if (resolution.changed && complete) {
        Persist();
    }
}

std::vector<std::string>::iterator FindKey(std::vector<std::string>& list, const char* key) {
    return std::find(list.begin(), list.end(), std::string(key));
}

/** The key of @p path within MM's half of the tree rooted at @p modsRoot: the
 *  path relative to the root with its first component (the `mm` folder, in
 *  whatever case the player typed it) removed. */
std::string KeyFor(const std::filesystem::path& path, const std::filesystem::path& modsRoot) {
    const std::filesystem::path rel = path.lexically_normal().lexically_relative(modsRoot.lexically_normal());
    auto it = rel.begin();
    if (it == rel.end()) {
        return std::string();
    }
    ++it;
    std::filesystem::path key;
    for (; it != rel.end(); ++it) {
        key /= *it;
    }
    return key.generic_string();
}

} // namespace

namespace Rsbs {

std::vector<std::string> SplitModList(const std::string& value) {
    std::vector<std::string> out;
    const std::string sep = RSBS_MM_MOD_SET_SEPARATOR;
    size_t start = 0;
    while (start <= value.size()) {
        const size_t end = value.find(sep, start);
        const std::string item = value.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!item.empty()) {
            out.push_back(item);
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + sep.size();
    }
    return out;
}

std::string JoinModList(const std::vector<std::string>& keys) {
    std::string out;
    for (const std::string& key : keys) {
        if (!out.empty()) {
            out += RSBS_MM_MOD_SET_SEPARATOR;
        }
        out += key;
    }
    return out;
}

bool MMModNameLess(const std::string& a, const std::string& b) {
    const std::string aStem = a.substr(0, a.find_last_of('.'));
    const std::string bStem = b.substr(0, b.find_last_of('.'));
    return std::lexicographical_compare(aStem.begin(), aStem.end(), bStem.begin(), bStem.end(), [](char c1, char c2) {
        return std::tolower((unsigned char)c1) < std::tolower((unsigned char)c2);
    });
}

MMModSetResolution ResolveMMModSet(const std::vector<std::string>& discoveredInDefaultOrder,
                                   const std::string& enabledValue, const std::string& disabledValue) {
    MMModSetResolution out;
    const std::set<std::string> present(discoveredInDefaultOrder.begin(), discoveredInDefaultOrder.end());

    // The disabled list first, because a key in both lists is disabled (rule 2).
    // De-duplicated, in the CVar's own order, absent keys kept (rule 3).
    std::set<std::string> disabledSet;
    for (const std::string& key : SplitModList(disabledValue)) {
        if (disabledSet.insert(key).second) {
            out.disabled.push_back(key);
        }
    }

    // The enabled list, in the CVar's order: present, not disabled, once each.
    std::set<std::string> enabledSet;
    for (const std::string& key : SplitModList(enabledValue)) {
        if (present.count(key) != 0 && disabledSet.count(key) == 0 && enabledSet.insert(key).second) {
            out.enabled.push_back(key);
        }
    }

    // New archives: enabled, appended at the top of the priority order, in the
    // walk's (stem-sort) order (rule 1). With both CVars empty this reproduces
    // #670's mount order exactly.
    for (const std::string& key : discoveredInDefaultOrder) {
        if (disabledSet.count(key) == 0 && enabledSet.insert(key).second) {
            out.enabled.push_back(key);
        }
    }

    out.changed = JoinModList(out.enabled) != enabledValue || JoinModList(out.disabled) != disabledValue;
    return out;
}

std::vector<MMModArchive> CollectMMModArchives(const std::string& modsRoot, bool* complete) {
    std::vector<MMModArchive> found;
    if (complete != nullptr) {
        *complete = true;
    }
    std::error_code ec;
    if (modsRoot.empty() || !std::filesystem::is_directory(modsRoot, ec)) {
        return found;
    }

    // Recursive: a mod may ship as mods/mm/<modname>/<archive>.o2r, and upstream
    // BenPort recurses too. Every step takes an error_code overload, so nothing in a
    // player's mods folder — a broken reparse point, a permission-denied
    // subdirectory — can throw out of MM's boot path or the menu.
    //
    // walkEc is the ITERATION's error and controls the loop; entryEc is separate and
    // per-entry. Sharing one would end the walk on the first entry whose status could
    // not be read, silently dropping every mod after it.
    //
    // Rsbs::kModsWalkOptions: the SAME options OoT's walk over the same tree uses
    // (mod_archives.h), so a mod installed through a symlinked folder works in both
    // halves. One tree, one traversal rule.
    std::vector<std::string> paths;
    std::error_code walkEc;
    for (std::filesystem::recursive_directory_iterator it(modsRoot, kModsWalkOptions, walkEc), end;
         it != end && !walkEc; it.increment(walkEc)) {
        const std::filesystem::path& p = it->path();
        std::error_code entryEc;
        if (it->is_directory(entryEc) || !Combo_ModArchiveExtensionIsValid(p.extension().string().c_str())) {
            continue;
        }
        const std::string generic = p.generic_string();
        // The partition. A file OoT owns must never be mounted for MM: registering it
        // under GAME_MM would make the switch-time re-apply stack OoT's mods over
        // MM's base archives on every MM arrival.
        if (!Combo_ModPathIsForGame(GAME_MM, modsRoot.c_str(), generic.c_str())) {
            continue;
        }
        paths.push_back(generic);
    }
    if (walkEc) {
        if (complete != nullptr) {
            *complete = false;
        }
        std::fprintf(stderr,
                     "[MM] WARNING: the walk of mods folder '%s/%s' ended early after %d archive(s): %s. The enabled "
                     "and disabled mod lists are not updated from this walk.\n",
                     modsRoot.c_str(), Combo_ModsSubdirForGame(GAME_MM), (int)paths.size(), walkEc.message().c_str());
    }

    std::sort(paths.begin(), paths.end(), MMModNameLess);
    found.reserve(paths.size());
    for (const std::string& path : paths) {
        std::string key = KeyFor(std::filesystem::path(path), std::filesystem::path(modsRoot));
        if (key.empty()) {
            continue;
        }
        found.push_back(MMModArchive{ path, std::move(key) });
    }
    return found;
}

std::vector<std::string> MMModArchivesToMount(const std::string& modsRoot) {
    bool complete = true;
    const std::vector<MMModArchive> archives = CollectMMModArchives(modsRoot, &complete);
    ResolveAndAdopt(archives, complete);

    std::vector<std::string> paths;
    paths.reserve(sModel.enabled.size());
    for (const std::string& key : sModel.enabled) {
        for (const MMModArchive& a : archives) {
            if (a.key == key) {
                paths.push_back(a.path);
                break;
            }
        }
    }
    sModel.mounted = true;
    sModel.mountedOrder = sModel.enabled;
    return paths;
}

} // namespace Rsbs

extern "C" int Combo_MMModSet_Scan(const char* modsRoot) {
    if (modsRoot == nullptr || modsRoot[0] == '\0') {
        return -1;
    }
    bool complete = true;
    const std::vector<Rsbs::MMModArchive> archives = Rsbs::CollectMMModArchives(std::string(modsRoot), &complete);
    ResolveAndAdopt(archives, complete);
    return (int)archives.size();
}

extern "C" bool Combo_MMModSet_Scanned(void) {
    return sModel.scanned;
}

extern "C" void Combo_MMModSet_Reset(void) {
    sModel = MMModSetModel();
}

extern "C" int Combo_MMModSet_EnabledCount(void) {
    return (int)sModel.enabled.size();
}

extern "C" const char* Combo_MMModSet_EnabledKey(int priority) {
    if (priority < 0 || priority >= (int)sModel.enabled.size()) {
        return nullptr;
    }
    return sModel.enabled[sModel.enabled.size() - 1 - (size_t)priority].c_str();
}

extern "C" int Combo_MMModSet_DisabledCount(void) {
    return (int)sModel.disabledShown.size();
}

extern "C" const char* Combo_MMModSet_DisabledKey(int index) {
    if (index < 0 || index >= (int)sModel.disabledShown.size()) {
        return nullptr;
    }
    return sModel.disabledShown[(size_t)index].c_str();
}

extern "C" bool Combo_MMModSet_SetEnabled(const char* key, bool enabled) {
    if (key == nullptr || key[0] == '\0' || sModel.present.count(key) == 0) {
        return false;
    }
    auto inEnabled = FindKey(sModel.enabled, key);
    auto inDisabled = FindKey(sModel.disabled, key);
    if (enabled) {
        if (inDisabled == sModel.disabled.end()) {
            return false;
        }
        sModel.disabled.erase(inDisabled);
        // The top of the priority order: where a new archive goes (rule 1).
        sModel.enabled.push_back(key);
    } else {
        if (inEnabled == sModel.enabled.end()) {
            return false;
        }
        sModel.enabled.erase(inEnabled);
        sModel.disabled.push_back(key);
    }
    RebuildDisabledShown();
    Persist();
    return true;
}

static bool MoveEnabled(const char* key, int step) {
    if (key == nullptr) {
        return false;
    }
    auto it = FindKey(sModel.enabled, key);
    if (it == sModel.enabled.end()) {
        return false;
    }
    const long from = (long)(it - sModel.enabled.begin());
    const long to = from + step;
    if (to < 0 || to >= (long)sModel.enabled.size()) {
        return false;
    }
    std::swap(sModel.enabled[(size_t)from], sModel.enabled[(size_t)to]);
    Persist();
    return true;
}

// Mount order is lowest priority first, so raising a mod moves it one place LATER.
extern "C" bool Combo_MMModSet_Raise(const char* key) {
    return MoveEnabled(key, +1);
}

extern "C" bool Combo_MMModSet_Lower(const char* key) {
    return MoveEnabled(key, -1);
}

extern "C" bool Combo_MMModSet_MountedThisSession(void) {
    return sModel.mounted;
}

extern "C" bool Combo_MMModSet_RestartPending(void) {
    return sModel.mounted && sModel.enabled != sModel.mountedOrder;
}

extern "C" void Combo_MMModSet_LoadForTest(const char* const* keysInDefaultOrder, int count) {
    std::vector<std::string> keys;
    for (int i = 0; keysInDefaultOrder != nullptr && i < count; i++) {
        if (keysInDefaultOrder[i] != nullptr && keysInDefaultOrder[i][0] != '\0') {
            keys.emplace_back(keysInDefaultOrder[i]);
        }
    }
    Adopt(keys, Rsbs::ResolveMMModSet(keys, ReadList(RSBS_CVAR_MM_ENABLED_MODS), ReadList(RSBS_CVAR_MM_DISABLED_MODS)));
}
