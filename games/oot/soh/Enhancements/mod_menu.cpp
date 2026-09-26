#include <map>
#include <vector>

#include <libultraship/classes.h>
#include <ship/utils/StringHelper.h>

#include "mod_menu.h"
// #670: the one-time re-homing notice and the early-ended-walk warning both go to
// stderr, and the second of those is in code standalone SoH compiles too.
#include <cstdio>
#ifdef RSBS_SINGLE_EXECUTABLE
#include "mod_archives.h" // src/common — #593, mod mounts must survive a game switch
#endif
#include "soh/OTRGlobals.h"
#include "soh/resource/type/Skeleton.h"
#include "soh/SohGui/MenuTypes.h"
#include "soh/SohGui/SohMenu.h"
#include "soh/SohGui/SohGui.hpp"

std::vector<std::string> enabledModFiles;
std::vector<std::string> disabledModFiles;
std::vector<std::string> unsupportedFiles;
std::map<std::string, std::filesystem::path> filePaths;
static int dragSourceIndex = -1;
static int dragTargetIndex = -1;

namespace SohGui {
extern std::shared_ptr<SohMenu> mSohMenu;
}

static WidgetInfo enableModsWidget;
static WidgetInfo tabHotkeyWidget;

#define CVAR_ENABLED_MODS_NAME CVAR_SETTING("EnabledMods")
#define CVAR_ENABLED_MODS_DEFAULT ""
#define CVAR_ENABLED_MODS_VALUE CVarGetString(CVAR_ENABLED_MODS_NAME, CVAR_ENABLED_MODS_DEFAULT)

// "|" was chosen as the separator due to
// it being an invalid character in NTFS
// and being rarely used in ext4
// it is also an ASCII character
// improving portability

// if being an ASCII character is not a requirement,
// other possible candidates include:
// - U+FFFF: non-character
// - any private use character
#define SEPARATOR "|"

void SetEnabledModsCVarValue() {
    std::string s = "";

    for (auto& modPath : enabledModFiles) {
        s += modPath + SEPARATOR;
    }

    // remove trailing separator if present
    if (s.length() != 0) {
        s.pop_back();
    }

    CVarSetString(CVAR_ENABLED_MODS_NAME, s.c_str());
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void AfterModChange() {
    // disabled mods are always sorted
    std::sort(disabledModFiles.begin(), disabledModFiles.end(), [](const std::string& a, const std::string& b) {
        return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
                                            [](char c1, char c2) { return std::tolower(c1) < std::tolower(c2); });
    });
}

void ModsPostDragAndDrop() {
    if (dragTargetIndex != -1) {
        std::string file = enabledModFiles[dragSourceIndex];
        enabledModFiles.erase(enabledModFiles.begin() + dragSourceIndex);
        enabledModFiles.insert(enabledModFiles.begin() + dragTargetIndex, file);
        dragTargetIndex = dragSourceIndex = -1;
        AfterModChange();
    }
}

void ModsHandleDragAndDrop(std::vector<std::string>& objectList, int targetIndex, const std::string& itemName,
                           ImGuiDragDropFlags flags = ImGuiDragDropFlags_SourceAllowNullID) {
    if (ImGui::BeginDragDropSource(flags)) {
        ImGui::SetDragDropPayload("DragMove", &targetIndex, sizeof(uint32_t));
        ImGui::Text("Move %s", itemName.c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DragMove")) {
            IM_ASSERT(payload->DataSize == sizeof(uint32_t));
            dragSourceIndex = *(const int*)payload->Data;
            dragTargetIndex = targetIndex;
        }
        ImGui::EndDragDropTarget();
    }
}

std::vector<std::string> GetEnabledModsFromCVar() {
    std::string enabledModsCVarValue = CVAR_ENABLED_MODS_VALUE;
    return StringHelper::Split(enabledModsCVarValue, SEPARATOR);
}

std::vector<std::string>& GetModFiles(bool enabled) {
    return enabled ? enabledModFiles : disabledModFiles;
}

std::shared_ptr<Ship::ArchiveManager> GetArchiveManager() {
    return Ship::Context::GetInstance()->GetResourceManager()->GetArchiveManager();
}

bool IsValidExtension(std::string extension) {
#ifdef RSBS_SINGLE_EXECUTABLE
    // #670: one rule for both halves of the shared mods/ tree. The shared
    // predicate is this function's rule verbatim — `.o2r` always, `.otr` only with
    // INCLUDE_MPQ_SUPPORT, never `.zip` for the reason spelled out below — so OoT's
    // accepted set is unchanged by the delegation. MM's glob now calls the same
    // predicate, which is what stops one folder tree from accepting a distribution
    // zip on its `mods/mm/` side and ignoring it on its `mods/` side.
    return Combo_ModArchiveExtensionIsValid(extension.c_str());
#else
    if (
#ifdef INCLUDE_MPQ_SUPPORT
        // .mpq doesn't make sense to support because all tools to make such mods output OTR
        StringHelper::IEquals(extension, ".otr") /*|| StringHelper::IEquals(extension, ".mpq")*/ ||
#endif
        // .zip needs to be excluded because mods are most often distributed in zip archives
        // and thus could contain .otr/o2r files
        StringHelper::IEquals(extension, ".o2r") /*|| StringHelper::IEquals(extension, ".zip")*/) {
        return true;
    }
    return false;
#endif
}

#ifdef RSBS_SINGLE_EXECUTABLE
// #670: `mods/mm/` is Majora's Mask's folder now, and it was NOT an unused
// namespace before this change — the walk below is recursive over the whole tree,
// so an archive already sitting at `mods/mm/x.o2r` WAS an OoT mod and is now
// mounted for MM instead. The re-homing is deliberate (in one shared tree the
// folder name is the only signal there is) but it must not be silent for the
// installs that actually have such a file, so say so once per run — and only when
// OoT's own enabled-mods CVar still names the file, which is exactly the set of
// installs whose behaviour changes.
static void WarnOnceOnRehomedEnabledMod(const std::filesystem::path& skipped) {
    static bool warned = false;
    if (warned || !Combo_ModArchiveExtensionIsValid(skipped.extension().generic_string().c_str())) {
        return;
    }
    const std::string name = skipped.filename().generic_string();
    const std::string stem = name.substr(0, name.rfind("."));
    if (std::find(enabledModFiles.begin(), enabledModFiles.end(), stem) == enabledModFiles.end()) {
        return;
    }
    warned = true;
    fprintf(stderr,
            "[OoT] NOTE: mod archive '%s' is under mods/mm, which is Majora's Mask's folder as of #670. It is still "
            "listed in OoT's enabled mods, but OoT no longer mounts it — MM does. Move it up into mods/ to keep it as "
            "an OoT mod.\n",
            skipped.generic_string().c_str());
}
#endif

// #670: the mods walk itself, extracted from UpdateModFiles so that the partition
// skip and the extension filter have exactly ONE definition, shared by the real
// menu path below and by OoT_MountModArchivesHeadless (the seam the #670 lock
// drives). A lock over a second copy of this loop would keep passing after someone
// deleted the partition from the copy that ships.
//
// @param modsPath   OoT's mods root.
// @param mmModsPath MM's mods root, as MM itself resolves it
//                   (Combo_ModsRootForGame(GAME_MM)). Only its IDENTITY with
//                   @p modsPath matters: `mods/mm` is reserved for MM only when MM
//                   is really globbing this same directory. Ignored without
//                   RSBS_SINGLE_EXECUTABLE, where there is no MM.
// @return the entries of @p modsPath that are OoT's mod archives, in iteration
//         order.
//
// WHAT CHANGED FOR STANDALONE SoH (no RSBS_SINGLE_EXECUTABLE). Corrected after PR
// #716's review, whose reviewer was right that the previous line here claimed
// "unchanged behaviour" on the one axis where it is not unchanged:
//   - The FILTER is unchanged: IsValidExtension and nothing else. No partition, no
//     roots-shared question, no `mods/mm` skip.
//   - The WALK is not. Upstream used the THROWING recursive_directory_iterator
//     overloads, so an unreadable subdirectory or a broken reparse point threw out
//     of UpdateModFiles at GUI init. This walk is error_code-driven, and as first
//     written it would instead have stopped early and silently dropped every mod
//     after the bad entry — UpdateModFiles then erases those stems from the enabled
//     set as "missing", so a player's mod disappears with no message at all, in
//     iteration order. Two things make the change safe rather than silent:
//     `skip_permission_denied` is now in the options on BOTH sides of the #ifdef, so
//     the common case costs that one subdirectory instead of the rest of the walk;
//     and an error that does end the walk is reported on stderr below.
//   - Covered by no row: every build this tree produces defines
//     RSBS_SINGLE_EXECUTABLE (CI builds no standalone SoH), so the #else branch is
//     compiled nowhere here. The divergence is removed, not locked.
std::vector<std::filesystem::path> CollectOoTModFiles(const std::string& modsPath, const std::string& mmModsPath) {
    std::vector<std::filesystem::path> claimed;
    std::error_code ec;
    if (modsPath.empty() || !std::filesystem::is_directory(modsPath, ec)) {
        return claimed;
    }
#ifdef RSBS_SINGLE_EXECUTABLE
    // #670, corrected after PR #704's review: the skip below exists ONLY because MM
    // globs this very directory. In a non-portable build the two roots are
    // different directories (SDL_GetPrefPath per app name), MM never looks under
    // OoT's root, and reserving `mm` there would leave an archive at
    // `<soh-prefdir>/mods/mm/x.o2r` mounted by NEITHER game. Ask, do not assume:
    // `SHIP_HOME` collapses the two roots again on Linux even with NON_PORTABLE, so
    // this is not a compile-time fact in either direction.
    const bool reserveMmSubdir = Combo_ModsRootsAreShared(modsPath.c_str(), mmModsPath.c_str());
#else
    (void)mmModsPath;
#endif
    // The SHARED walk options and the same error_code discipline MM's glob uses
    // (Rsbs::kModsWalkOptions, src/common/mod_archives.h): one tree must not have
    // two traversal rules.
    //
    // The #else spells the SAME TWO OPTIONS out because mod_archives.h is a
    // single-exe include here (see the top of this file), not because standalone SoH
    // wants different ones. It carried only follow_directory_symlink until PR #716's
    // review: paired with the error_code overloads below, a permission-denied
    // subdirectory then ENDED the walk instead of costing one directory, and every
    // mod after it vanished from the menu without a message. If these two lists ever
    // have to differ, say why here.
    constexpr std::filesystem::directory_options kWalkOptions =
#ifdef RSBS_SINGLE_EXECUTABLE
        Rsbs::kModsWalkOptions;
#else
        std::filesystem::directory_options::follow_directory_symlink |
        std::filesystem::directory_options::skip_permission_denied;
#endif
    std::error_code walkEc;
    for (std::filesystem::recursive_directory_iterator it(modsPath, kWalkOptions, walkEc), end;
         it != end && !walkEc; it.increment(walkEc)) {
        std::error_code entryEc;
        if (it->is_directory(entryEc)) {
            continue;
        }
#ifdef RSBS_SINGLE_EXECUTABLE
        // #670: the one mods/ tree is shared with MM, because
        // LocateFileAcrossAppDirs's appName argument is inert in a portable build
        // and both games resolve "mods" to ./mods. MM's mods live in mods/mm and
        // are mounted by MountMMModArchives
        // (games/mm/2s2h/GameExports_SingleExe.cpp). Picking them up here would
        // mount them a second time AND list them in OoT's enabled set, so the #593
        // switch-time re-apply would stack MM's mods over OoT's base archives on
        // every OoT arrival — a cross-game shadowing that survives the switch.
        // OoT keeps every other path in the tree. This skip DOES change OoT's
        // behaviour for an install that already had an archive under mods/mm (that
        // file was an OoT mod and is now MM's); the notice above is why that is not
        // silent.
        if (reserveMmSubdir &&
            !Combo_ModPathIsForGame(GAME_OOT, modsPath.c_str(), it->path().generic_string().c_str())) {
            WarnOnceOnRehomedEnabledMod(it->path());
            continue;
        }
#endif
        if (!IsValidExtension(it->path().extension().generic_string())) {
            continue;
        }
        claimed.push_back(it->path());
    }
    // An error_code-driven walk that ends early returns a PREFIX of the tree, and
    // UpdateModFiles cannot tell that from "those mods are gone": it erases their
    // stems from the enabled set as missing. Say so, once, with the count already
    // collected, so the symptom ("some of my mods vanished") has a cause on stderr
    // instead of being invisible. Not fatal: a prefix of the mods is still better
    // than upstream's exception out of GUI init.
    if (walkEc) {
        fprintf(stderr,
                "[OoT] WARNING: the walk of mods folder '%s' ended early after %d archive(s): %s. Mods that sort "
                "after that point were NOT offered in the mod menu and will be dropped from the enabled list.\n",
                modsPath.c_str(), (int)claimed.size(), walkEc.message().c_str());
    }
    return claimed;
}

// #670, corrected after PR #704's review: the per-archive mount+register PAIR,
// one definition, so the seam below genuinely drives what ships. AddArchive then
// Combo_RegisterModArchive, in that order — the order is the contract, because
// #593's switch-time re-apply replays the registry in registration order — and
// without consulting AddArchive's return value, which is upstream's own choice
// here and not an oversight this seam may quietly "fix".
//
// What is NOT in here, and therefore not driven by the seam, is the init leg's
// menu bookkeeping around it: which stems the enabled set names, and the
// stem-keyed `filePaths` map that collapses two same-stem archives in different
// subfolders to one. See OoT_MountModArchivesHeadless's contract.
static void MountAndRegisterOoTMod(const std::string& modArchivePath) {
    GetArchiveManager()->AddArchive(modArchivePath);
#ifdef RSBS_SINGLE_EXECUTABLE
    // #593: OoT mounts its mods HERE, at GUI init — long after the base
    // archives — and relies on ArchiveManager's last-added-wins resolution to
    // make the override stick. Every cross-game switch re-adds oot.o2r/soh.o2r
    // on top (EnsureGameArchivesLoaded, rsbs/src/main.cpp), which silently
    // revoked that. Recording the mount lets the switch path re-apply it in this
    // exact order.
    Combo_RegisterModArchive(GAME_OOT, modArchivePath.c_str());
#endif
}

#ifdef RSBS_SINGLE_EXECUTABLE
// #705: OoT's loose (unpacked) asset folder, `<mods>/loose`, mounted as a
// FolderArchive AFTER the packed mods — the same helper, at the same point in the
// mount order, as MM's `<mods>/mm/loose` (MountMMModArchives,
// games/mm/2s2h/GameExports_SingleExe.cpp), so the two halves of one game have one
// modding rule: base archives, then packed mods, then loose files. The helper
// registers it with Combo_RegisterModArchive(GAME_OOT, ...) so the switch-time
// re-apply keeps it on top.
//
// Not in the mod menu's enabled list, deliberately: the list is keyed by archive
// file-name stem and persisted in a CVar, and the loose folder is not an archive.
// Rename or empty the folder to turn it off. Called only from MountOoTModLayers
// below, so the init leg and OoT_MountModArchivesHeadless cannot disagree on it.
static int MountOoTLooseMods(const std::string& modsPath) {
    return (int)Rsbs::MountLooseModDirs(GAME_OOT, modsPath).size();
}
#endif

// The whole OoT mount sequence, ONE definition, called by BOTH the init leg of
// UpdateModFiles and OoT_MountModArchivesHeadless (PR #732's review): each packed
// archive in the order given, through the mount/register pair above, THEN the #705
// loose layer. Before this existed the two callers each made their own loose call,
// so deleting the init leg's — the one a player's boot runs — left every row green.
// Now the loose call lives only here, and the #705 row, which drives this through
// the seam, goes red if it is removed.
//
// What stays unlocked, as for #670's pair: that the init leg calls THIS at all. No
// row drives UpdateModFiles(true), which reads the player's real mods root and the
// enabled-mods CVar. The caller decides WHICH archives and in what order (the init
// leg: the enabled set, in the menu's order; the seam: every archive the walk
// claims); this decides everything after that.
//
// @return how many packed archives were mounted and registered plus how many loose
//         folders were.
static int MountOoTModLayers(const std::vector<std::filesystem::path>& packedInMountOrder,
                             const std::string& modsPath) {
    int mounted = 0;
    for (const std::filesystem::path& modPath : packedInMountOrder) {
        MountAndRegisterOoTMod(modPath.generic_string());
        mounted++;
    }
#ifdef RSBS_SINGLE_EXECUTABLE
    // #705: the loose layer, LAST, so it wins over every packed mod.
    mounted += MountOoTLooseMods(modsPath);
#else
    (void)modsPath;
#endif
    return mounted;
}

void UpdateModFiles(bool init = false, bool reset = false) {
    if (init || reset) {
        enabledModFiles.clear();
        enabledModFiles = GetEnabledModsFromCVar();
    }
    disabledModFiles.clear();
    unsupportedFiles.clear();
    filePaths.clear();
    bool changed = false;
#ifdef RSBS_SINGLE_EXECUTABLE
    // #670, PR #716's review: the same resolver MM's root comes from
    // (Combo_ModsRootForGame -> LocateFileAcrossAppDirs("mods",
    // Combo_ModsAppShortName(GAME_OOT))), so the two roots the roots-shared gate
    // below compares are produced by ONE piece of code rather than by two copies of
    // the call that could drift. Identical to the upstream line while
    // Combo_ModsAppShortName(GAME_OOT) and appShortName agree, which the #670 row
    // asserts (OoT_ModsAppShortName, below).
    std::string modsPath = Combo_ModsRootForGame(GAME_OOT);
#else
    std::string modsPath = Ship::Context::LocateFileAcrossAppDirs("mods", appShortName);
#endif
    std::map<std::string, std::string> tempMods;
    if (modsPath.length() > 0 && std::filesystem::exists(modsPath)) {
        std::vector<std::filesystem::path> enabledFiles;
        if (std::filesystem::is_directory(modsPath)) {
            // #670: the walk, the partition and the extension filter now live in
            // CollectOoTModFiles above — one definition, shared with the seam the
            // lock drives. Everything below is this function's own bookkeeping,
            // unchanged.
            // #670: MM's root, asked for the way MM asks for it, so the reserve
            // decision inside the walk is made against the directory MM really
            // globs rather than against an assumption about the build flags.
#ifdef RSBS_SINGLE_EXECUTABLE
            const std::string mmModsPath = Combo_ModsRootForGame(GAME_MM);
#else
            const std::string mmModsPath;
#endif
            for (const std::filesystem::path& modPath : CollectOoTModFiles(modsPath, mmModsPath)) {
                std::string filename =
                    modPath.filename().generic_string().substr(0, modPath.filename().generic_string().rfind("."));
                bool enabled =
                    std::find(enabledModFiles.begin(), enabledModFiles.end(), filename) != enabledModFiles.end();
                if (!enabled) {
                    tempMods.emplace(modPath.lexically_normal().generic_string(), filename);
                }
                filePaths.emplace(filename, modPath);
            }
            if (tempMods.size() > 0) {
                changed = true;
                for (auto [path, name] : tempMods) {
                    enabledModFiles.push_back(name);
                }
                tempMods.clear();
            }
            if (init) {
                std::vector<std::string> enabledTemp(enabledModFiles);
                // The enabled set, in the menu's order; mounted by the one sequence
                // the seam below shares (MountOoTModLayers: these, then the loose
                // layer). Collected first and mounted after the bookkeeping, in the
                // same order the loop used to mount them in.
                std::vector<std::filesystem::path> toMount;
                for (std::string mod : enabledTemp) {
                    if (filePaths.contains(mod)) {
                        toMount.push_back(filePaths.at(mod));
                    } else {
                        enabledModFiles.erase(std::find(enabledModFiles.begin(), enabledModFiles.end(), mod));
                        changed = true;
                    }
                }
                (void)MountOoTModLayers(toMount, modsPath);
            }
        }
        if (changed) {
            SetEnabledModsCVarValue();
        }
    }
}

#ifdef RSBS_SINGLE_EXECUTABLE
/**
 * The app short name THIS port's mods lookups pass, straight off
 * games/oot/soh/OTRGlobals.h's `appShortName`.
 *
 * Seam for the #670 row's app-short-name leg, which pins it equal to
 * Combo_ModsAppShortName(GAME_OOT). "soh" has two definitions — that one, and the
 * port-global here, which names the app DIRECTORY in some forty places and cannot be
 * replaced from src/common. If they drift, OoT reads its config and archives out of
 * one app directory and its mods out of another, and CheckAndCreateModFolder
 * (games/oot/soh/OTRGlobals.cpp, the one mods path still resolved from
 * `appShortName` directly, through the GetPathRelativeToAppDirectory API this
 * resolver does not wrap) creates the folder in the wrong one. A row is the only
 * thing that can catch that; there is no way to make it one definition.
 */
extern "C" const char* OoT_ModsAppShortName(void) {
    return appShortName.c_str();
}

/**
 * Headless seam for the #670 partition lock (src/common/tests/test_mm_mods_mount.c).
 *
 * WHAT IT REALLY DRIVES, stated exactly — the first version of this comment
 * claimed the seam performs the init-leg registration "exactly as the init leg
 * does", and PR #704's reviewer was right that it does not:
 *
 *   - CollectOoTModFiles, the very function UpdateModFiles's loop iterates. The
 *     walk, its directory_options, the roots-shared question, the `mods/mm` skip
 *     and the extension filter are therefore genuinely shared: delete any of them
 *     from the production walk and this goes red.
 *   - MountOoTModLayers, the very function the init leg calls with its enabled
 *     set: MountAndRegisterOoTMod per archive — the PAIR and its ORDER, AddArchive
 *     then Combo_RegisterModArchive(GAME_OOT, ...), return value ignored — and then
 *     the #705 loose layer. So the pair, and "loose after packed", are shared too:
 *     delete the loose call and the #705 row goes red (observed, PR #732).
 *
 * WHAT IT DOES NOT DRIVE, and which no row covers: the init leg's CALL of
 * MountOoTModLayers (nothing drives UpdateModFiles(true)), and its menu bookkeeping
 * around that call. The shipping leg iterates the ENABLED set and registers only
 * `filePaths.at(stem)`, a map keyed by file-name stem, so two claimed archives with
 * the same stem in different subfolders collapse to ONE registration and a disabled
 * archive is claimed by the walk but never mounted. This seam registers every path
 * the walk returns. Reading a count from this seam is therefore a measurement of
 * the walk and of the mount pair, NOT of the enabled-set filter.
 *
 * Both roots are parameters rather than resolved through LocateFileAcrossAppDirs so
 * the row can stage a private tree and never touch the player's ./mods — and so it
 * can drive the non-portable case, where MM's root is a DIFFERENT directory and OoT
 * must keep its own `mods/mm`. Nothing else in the menu's state is read or written:
 * not the enabled-mods CVar, not filePaths, not enabledModFiles.
 *
 * @param modsRoot   OoT's mods root.
 * @param mmModsRoot MM's mods root. Pass the same string for the shared-tree
 *                   (portable) case; a different directory for the non-portable
 *                   one.
 * @return how many archives OoT claimed and registered, PLUS the loose asset
 *         folders (#705, MountOoTLooseMods) it mounted and registered after them,
 *         or -1 for a NULL/empty root or with no live ArchiveManager.
 */
extern "C" int OoT_MountModArchivesHeadless(const char* modsRoot, const char* mmModsRoot) {
    if (modsRoot == nullptr || modsRoot[0] == '\0' || mmModsRoot == nullptr || mmModsRoot[0] == '\0') {
        return -1;
    }
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetResourceManager() == nullptr ||
        ctx->GetResourceManager()->GetArchiveManager() == nullptr) {
        return -1;
    }

    // #705 / PR #732's review: the SAME sequence function the init leg calls, so the
    // loose layer after the packed mods is shared, not copied.
    return MountOoTModLayers(CollectOoTModFiles(std::string(modsRoot), std::string(mmModsRoot)),
                             std::string(modsRoot));
}
#endif

extern "C" void gfx_texture_cache_clear();

void EnableMod(std::string file) {
    disabledModFiles.erase(std::find(disabledModFiles.begin(), disabledModFiles.end(), file));
    enabledModFiles.insert(enabledModFiles.begin(), file);

    // TODO: runtime changes
    // GetArchiveManager()->AddArchive(file);
    AfterModChange();
}

void DisableMod(std::string file) {
    enabledModFiles.erase(std::find(enabledModFiles.begin(), enabledModFiles.end(), file));
    disabledModFiles.insert(disabledModFiles.begin(), file);

    // TODO: runtime changes
    // GetArchiveManager()->RemoveArchive(file);
    AfterModChange();
}

void DrawModInfo(std::string file) {
    ImGui::SameLine();
    ImGui::Text("%s", file.c_str());
}

void DrawMods(bool enabled) {
    std::vector<std::string>& selectedModFiles = GetModFiles(enabled);
    if (selectedModFiles.empty()) {
        return;
    }

    bool madeAnyChange = false;
    int switchFromIndex = -1;
    int switchToIndex = -1;
    uint32_t index = 0;

    for (size_t i = selectedModFiles.size() - 1; i != SIZE_MAX; i--) {
        std::string file = selectedModFiles[i];
        if (enabled) {
            ImGui::BeginGroup();
        }
        // if (UIWidgets::StateButton((file + "_left_right").c_str(), enabled ? ICON_FA_ARROW_RIGHT :
        // ICON_FA_ARROW_LEFT,
        //                            ImVec2(25, 25), UIWidgets::ButtonOptions().Color(THEME_COLOR))) {
        //     if (enabled) {
        //         DisableMod(file);
        //     } else {
        //         EnableMod(file);
        //     }
        // }

        // it's not relevant to reorder disabled mods
        if (enabled) {
            // ImGui::SameLine();
            if (i == selectedModFiles.size() - 1) {
                ImGui::BeginDisabled();
            }
            if (UIWidgets::StateButton((file + "_up").c_str(), ICON_FA_ARROW_UP, ImVec2(25, 25),
                                       UIWidgets::ButtonOptions().Color(THEME_COLOR))) {
                madeAnyChange = true;
                switchFromIndex = i;
                switchToIndex = i + 1;
            }
            if (i == selectedModFiles.size() - 1) {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();
            if (i == 0) {
                ImGui::BeginDisabled();
            }
            if (UIWidgets::StateButton((file + "_down").c_str(), ICON_FA_ARROW_DOWN, ImVec2(25, 25),
                                       UIWidgets::ButtonOptions().Color(THEME_COLOR))) {
                madeAnyChange = true;
                switchFromIndex = i;
                switchToIndex = i - 1;
            }
            if (i == 0) {
                ImGui::EndDisabled();
            }
        }

        DrawModInfo(filePaths.at(file).filename().generic_string());
        if (enabled) {
            ImGui::EndGroup();
            ModsHandleDragAndDrop(selectedModFiles, i, file);
        }
    }

    if (enabled) {
        ModsPostDragAndDrop();
    }

    if (madeAnyChange) {
        std::iter_swap(selectedModFiles.begin() + switchFromIndex, selectedModFiles.begin() + switchToIndex);
        AfterModChange();
    }
}

bool editing = false;

void ModMenuWindow::DrawElement() {
    SohGui::mSohMenu->MenuDrawItem(enableModsWidget, 200, THEME_COLOR);
    ImGui::SameLine();
    SohGui::mSohMenu->MenuDrawItem(tabHotkeyWidget, 200, THEME_COLOR);

    ImGui::TextColored(
        UIWidgets::ColorValues.at(UIWidgets::Colors::Yellow),
        "Mods are currently not reloaded at runtime. Close and re-open Ship for the changes to take effect.\n"
        "Drag ordering for the enabled list is available.\nMod priority is top to bottom. They override mods listed "
        "below them.");

    // if (UIWidgets::Button(
    //         "Update", UIWidgets::ButtonOptions({ { .disabled = editing, .disabledTooltip = "Currently editing..." }
    //         })
    //                       .Size(UIWidgets::Sizes::Inline)
    //                       .Color(THEME_COLOR))) {
    //     UpdateModFiles();
    // }
    // ImGui::SameLine();
    if (UIWidgets::Button("Edit",
                          UIWidgets::ButtonOptions({ { .disabled = editing, .disabledTooltip = "Already editing..." } })
                              .Size(UIWidgets::Sizes::Inline)
                              .Color(THEME_COLOR))) {
        editing = true;
    }
    if (editing) {
        ImGui::SameLine();
        if (UIWidgets::Button("Cancel", UIWidgets::ButtonOptions().Size(UIWidgets::Sizes::Inline))) {
            editing = false;
            UpdateModFiles(false, true);
        }
        ImGui::SameLine();
        if (UIWidgets::Button("Apply & Close",
                              UIWidgets::ButtonOptions().Size(UIWidgets::Sizes::Inline).Color(THEME_COLOR))) {
            SohGui::RegisterPopup("Apply & Close",
                                  "Application currently requires a restart. Save the mod info and close SoH?", "Close",
                                  "Cancel", [&]() {
                                      // TODO: runtime changes
                                      SetEnabledModsCVarValue();
                                      // TODO: runtime changes
                                      /*
                                      gfx_texture_cache_clear();
                                      SOH::SkeletonPatcher::ClearSkeletons();
                                      */
                                      Ship::Context::GetInstance()->GetConsoleVariables()->Save();
                                      Ship::Context::GetInstance()->GetWindow()->Close();
                                  });
        }
    }
    ImGui::BeginDisabled(!editing);
    if (ImGui::BeginTable("tableMods", 2, ImGuiTableFlags_BordersH | ImGuiTableFlags_BordersV)) {
        ImGui::TableSetupColumn("Enabled Mods", ImGuiTableColumnFlags_WidthStretch, 200.0f);
        // ImGui::TableSetupColumn("Disabled Mods", ImGuiTableColumnFlags_WidthStretch, 200.0f);
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::TableHeadersRow();
        ImGui::PopItemFlag();
        ImGui::TableNextRow();

        ImGui::TableNextColumn();

        if (ImGui::BeginChild("Enabled Mods", ImVec2(0, -8))) {
            DrawMods(true);

            ImGui::EndChild();
        }

        /*ImGui::TableNextColumn();

        if (ImGui::BeginChild("Disabled Mods", ImVec2(0, -8))) {
            DrawMods(false);

            ImGui::EndChild();
        }*/

        ImGui::EndTable();
    }
    ImGui::EndDisabled();
}

void ModMenuWindow::InitElement() {
    UpdateModFiles(true);
}

void RegisterModMenuWidgets() {
    enableModsWidget = { .name = "Enable Mods", .type = WidgetType::WIDGET_CVAR_CHECKBOX };
    enableModsWidget.CVar(CVAR_SETTING("AltAssets"))
        .RaceDisable(false)
        .Options(UIWidgets::CheckboxOptions({ { .disabledTooltip = "Temporarily disabled while editing mods list." } })
                     .Color(THEME_COLOR)
                     .Tooltip("Toggle mods. For graphics mods, this means toggling between default and mod graphics.")
                     .DefaultValue(true))
        .PreFunc([&](WidgetInfo& info) {
            auto options = std::static_pointer_cast<UIWidgets::CheckboxOptions>(info.options);
            options->disabled = editing;
        });
    SohGui::mSohMenu->AddSearchWidget({ enableModsWidget, "Settings", "Mod Menu", "Top", "alternate assets" });

    tabHotkeyWidget = { .name = "Mods Tab Hotkey", .type = WidgetType::WIDGET_CVAR_CHECKBOX };
    tabHotkeyWidget.CVar(CVAR_SETTING("Mods.AlternateAssetsHotkey"))
        .RaceDisable(false)
        .Options(UIWidgets::CheckboxOptions()
                     .Color(THEME_COLOR)
                     .Tooltip("Allows pressing the Tab key to toggle mods")
                     .DefaultValue(true));
    SohGui::mSohMenu->AddSearchWidget(
        { tabHotkeyWidget, "Settings", "Mod Menu", "Top", "alternate assets tab hotkey" });
}

static RegisterMenuInitFunc menuInitFunc(RegisterModMenuWidgets);
