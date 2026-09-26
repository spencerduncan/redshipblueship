/**
 * @file mod_archives.cpp
 * @brief Implementation of the per-game mod-archive registry (issue #593).
 *
 * See mod_archives.h for why this exists. Storage only — the actual re-mount
 * lives in EnsureGameArchivesLoaded (rsbs/src/main.cpp), which is the one
 * place that knows a switch is happening.
 */

#include "mod_archives.h"

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <deque>
#include <exception>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace {

// The one subdirectory of the shared mods/ tree that is MM's. See the header for
// why the tree is partitioned at all (the portable-build collapse of
// LocateFileAcrossAppDirs's appName argument) and why OoT keeps the root.
constexpr const char kMmModsSubdir[] = "mm";

// The folder, inside each game's half, whose files are mounted loose (#705).
constexpr const char kLooseModsDir[] = "loose";

// The app short names the two MODS-ROOT lookups hand LocateFileAcrossAppDirs.
// Stated exactly, because an earlier version of this comment claimed more than the
// code did and the claim was the entire reason the constants exist (PR #716's
// review):
//
//   - "2s2h" HAS ONE DEFINITION FOR MM'S MODS PATHS: this one.
//     games/mm/2s2h/GameExports_SingleExe.cpp's `kMmAppName` is a reference to it
//     (Combo_ModsAppShortName(GAME_MM)), not a second literal, so the four MM sites
//     that use it — the mods-root lookup, MMCreateModFolder's
//     GetPathRelativeToAppDirectory fallback two lines below it, the mm.o2r probe
//     and the export dir — cannot drift apart. It WAS a second `"2s2h"` literal,
//     and the hazard was not hypothetical: the two were used in the same function,
//     MMCreateModFolder resolving `existing` through Combo_ModsRootForGame and its
//     fallback through the local literal, so renaming the obvious-looking one made
//     MM create one folder and glob another.
//   - "soh" still has TWO definitions: this one and OoT's port-global
//     `appShortName` (games/oot/soh/OTRGlobals.h), which names the app directory in
//     some forty places and cannot be replaced from here. They MUST agree, and the
//     #670 partition row pins them equal (test_mm_mods_mount.c's app-short-name
//     leg, through the OoT_ModsAppShortName seam) instead of asserting it in prose.
//   - Other "2s2h" spellings in the tree are NOT unified by this and are not mods:
//     rsbs/src/main.cpp's boot app-name pick, src/common/archive_check.cpp's
//     install probe.
//
// Why it matters: OoT's walk asks Combo_ModsRootsAreShared whether MM is globbing
// the same tree, and gets "not shared" if the two roots were resolved under names
// that drifted. OoT would then claim mods/mm although MM is globbing that very
// directory, and every MM mod would be mounted twice and registered under BOTH
// games — the cross-game shadowing the partition exists to prevent.
constexpr const char kOoTAppShortName[] = "soh";
constexpr const char kMmAppShortName[] = "2s2h";

// One list per game, in mount order. Indexed by GameId, so slot 0 (GAME_NONE)
// is present but never used.
//
// deque, not vector: Combo_GetModArchive hands out a const char* into an
// element, and deque guarantees references to existing elements survive a
// push_back. With vector, a registration that happened between two Get calls
// would dangle every pointer previously returned.
std::deque<std::string> sModArchives[3];
std::mutex sModArchivesMutex;

bool ValidGame(GameId game) {
    return game == GAME_OOT || game == GAME_MM;
}

bool IEqualsAscii(const std::string& a, const char* b) {
    size_t i = 0;
    for (; i < a.size() && b[i] != '\0'; i++) {
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) {
            return false;
        }
    }
    return i == a.size() && b[i] == '\0';
}

// `path` relative to `modsRoot`, or an empty path when `path` does not lie under
// `modsRoot` at all.
//
// lexically_relative, not a textual prefix compare: the two strings reach this
// function from different places and are not spelled the same way. modsRoot
// comes from LocateFileAcrossAppDirs and can be "./mods"; a path can arrive
// already lexically_normal()'d ("mods/mm/x.o2r") from mod_menu's own
// bookkeeping, or with native '\' separators on Windows. Comparing the
// normalized RELATIVE path makes all of those spellings agree, and a path that
// is not under the root at all yields a ".." first component, which is what
// "outside" is recognised by.
std::filesystem::path RelativeToModsRoot(const char* modsRoot, const char* path) {
    if (modsRoot == nullptr || modsRoot[0] == '\0' || path == nullptr || path[0] == '\0') {
        return {};
    }

    const std::filesystem::path rootPath = std::filesystem::path(modsRoot).lexically_normal();
    const std::filesystem::path filePath = std::filesystem::path(path).lexically_normal();
    const std::filesystem::path rel = filePath.lexically_relative(rootPath);

    auto it = rel.begin();
    if (it == rel.end()) {
        return {};
    }
    // ".." — a different tree entirely. "." — the root directory itself, which is
    // not a file either game can claim.
    const std::string first = it->generic_string();
    if (first == ".." || first == ".") {
        return {};
    }
    return rel;
}

// MM's rule: the first component under the root is the reserved `mm` folder.
bool PathIsUnderMmSubdir(const char* modsRoot, const char* path) {
    const std::filesystem::path rel = RelativeToModsRoot(modsRoot, path);
    if (rel.empty()) {
        return false;
    }
    return IEqualsAscii(rel.begin()->generic_string(), kMmModsSubdir);
}

// OoT's rule: the path lies under the root AND its first component is not the
// folder reserved for MM.
//
// WHAT THIS IS AND IS NOT, stated precisely (PR #716's review corrected an earlier
// comment here that overclaimed): this is ONE rule written out twice, the second
// copy negated, over the same helper and the same constant. It is NOT two
// independent tests of two independent properties, and no input can make the two
// copies disagree while both read
// `IEqualsAscii(rel.begin()->generic_string(), kMmModsSubdir)` off the same `rel`.
//
// The duplication is deliberate anyway, and buys two specific things over
// `return game == GAME_MM ? isMm : !isMm;` (PR #704's shape):
//
//   1. A path OUTSIDE the root is now claimed by NEITHER game. The one-expression
//      form answered "OoT's" for `./elsewhere/mm/x.o2r` against root `./mods`,
//      which is wrong and is pinned in the row's table.
//   2. A one-sided hand-edit of either copy — the realistic way this drifts, e.g.
//      teaching MM's side a prefix match or giving OoT's side a second reserved
//      name — becomes observable, because the row's generated sweep then sees a
//      path claimed twice or claimed by nobody. Both reds were produced that way
//      and observed.
//
// What it does NOT buy, and what the row's comment now says instead of the
// opposite: the sweep's "both games claimed it" branch cannot fire against the
// implementation as written, so it is a lock on the duplication staying in step,
// not a measurement of a partition property over the input space.
bool PathIsOoTs(const char* modsRoot, const char* path) {
    const std::filesystem::path rel = RelativeToModsRoot(modsRoot, path);
    if (rel.empty()) {
        return false;
    }
    return !IEqualsAscii(rel.begin()->generic_string(), kMmModsSubdir);
}

// Resolved mods roots, one slot per game, so the pointers two calls hand back
// are independent and `f(Combo_ModsRootForGame(GAME_OOT),
// Combo_ModsRootForGame(GAME_MM))` is well defined. Deliberately re-resolved on
// every call rather than cached: LocateFileAcrossAppDirs's answer CHANGES once a
// mods folder is created (MM creates mods/mm during boot and then globs it), so
// a cache would freeze the pre-creation answer.
//
// The mutex serializes the slot WRITES, which is what keeps two games' concurrent
// lookups from tearing each other's std::string. It does NOT make the returned
// pointer safe against a second call FOR THE SAME game from another thread — that
// would reassign the very string the first caller is holding, and no lock held
// inside this function can cover the caller's use of the result. Both call sites
// are single-threaded boot/GUI-init paths and copy the result immediately
// (games/oot/soh/Enhancements/mod_menu.cpp, games/mm/2s2h/GameExports_SingleExe.cpp);
// the header states the per-game lifetime that callers must respect.
std::string sModsRoots[3];
std::mutex sModsRootsMutex;

} // namespace

extern "C" const char* Combo_ModsSubdirForGame(GameId game) {
    return game == GAME_MM ? kMmModsSubdir : "";
}

extern "C" bool Combo_ModPathIsForGame(GameId game, const char* modsRoot, const char* path) {
    if (!ValidGame(game) || modsRoot == nullptr || modsRoot[0] == '\0' || path == nullptr || path[0] == '\0') {
        return false;
    }
    // One rule written out twice, the second copy negated, and NOT two independent
    // tests — see PathIsOoTs for what that does and does not buy.
    return game == GAME_MM ? PathIsUnderMmSubdir(modsRoot, path) : PathIsOoTs(modsRoot, path);
}

extern "C" const char* Combo_ModsAppShortName(GameId game) {
    if (!ValidGame(game)) {
        return "";
    }
    // The same object every time, not a copy: MM's kMmAppName holds this pointer,
    // and the #670 row asserts the identity so a re-introduced second literal is
    // caught rather than merely equal.
    return game == GAME_MM ? kMmAppShortName : kOoTAppShortName;
}

extern "C" const char* Combo_ModsRootForGame(GameId game) {
    if (!ValidGame(game)) {
        return "";
    }
    const char* appName = Combo_ModsAppShortName(game);
    std::lock_guard<std::mutex> lock(sModsRootsMutex);
    sModsRoots[(int)game] = Ship::Context::LocateFileAcrossAppDirs("mods", appName);
    return sModsRoots[(int)game].c_str();
}

extern "C" bool Combo_ModsRootsAreShared(const char* ootModsRoot, const char* mmModsRoot) {
    if (ootModsRoot == nullptr || ootModsRoot[0] == '\0' || mmModsRoot == nullptr || mmModsRoot[0] == '\0') {
        return false;
    }
    // absolute() FIRST, then weakly_canonical, and not string equality: the two
    // roots are produced by two separate LocateFileAcrossAppDirs calls that
    // legitimately spell the same directory differently. One of them is typically
    // relative ("./mods" or "mods", the install-folder fallback) and the other
    // absolute (SDL_GetPrefPath's, or SHIP_HOME's), and those two are the same
    // directory exactly when the relative one resolves against the current
    // directory to the absolute one.
    //
    // weakly_canonical alone does NOT settle that: it canonicalizes the longest
    // EXISTING prefix and appends the rest lexically, so for a relative path with
    // no existing prefix — which is the normal case here, since this is asked
    // before `mods` has been created — MSVC hands back the relative spelling
    // unchanged and the comparison against an absolute path is false. The #670 row
    // caught exactly that ("absolute vs relative, same directory" came back not
    // shared), and answering "not shared" for one shared tree is the direction that
    // double-mounts every MM mod under both games. absolute() removes the
    // dependence on whether the directory exists yet; weakly_canonical still runs,
    // so a symlinked or ".."-laden existing prefix resolves.
    //
    // On error (absolute() can fail with no current directory) fall back to the
    // lexically normalized spellings rather than guessing: a wrong "shared" answer
    // reserves mods/mm from OoT in a build where MM never looks there, and a wrong
    // "not shared" answer double-mounts, so both directions matter and neither is a
    // safe default.
    std::error_code ootEc;
    std::error_code mmEc;
    const std::filesystem::path ootAbs = std::filesystem::absolute(ootModsRoot, ootEc);
    const std::filesystem::path mmAbs = std::filesystem::absolute(mmModsRoot, mmEc);
    if (ootEc || mmEc) {
        return std::filesystem::path(ootModsRoot).lexically_normal() ==
               std::filesystem::path(mmModsRoot).lexically_normal();
    }
    const std::filesystem::path ootPath = std::filesystem::weakly_canonical(ootAbs, ootEc);
    const std::filesystem::path mmPath = std::filesystem::weakly_canonical(mmAbs, mmEc);
    if (ootEc || mmEc) {
        return ootAbs.lexically_normal() == mmAbs.lexically_normal();
    }
    return ootPath == mmPath;
}

extern "C" bool Combo_ModArchiveExtensionIsValid(const char* extension) {
    if (extension == nullptr || extension[0] == '\0') {
        return false;
    }
    const std::string ext(extension);
    if (IEqualsAscii(ext, ".o2r")) {
        return true;
    }
#ifdef INCLUDE_MPQ_SUPPORT
    // Gated exactly as OoT gates it: the .otr reader is StormLib, which is only
    // linked in when MPQ support is on. This project sets INCLUDE_MPQ_SUPPORT ON
    // unconditionally (CMakeLists.txt:217) and libultraship exports it PUBLIC
    // (CMakeLists.txt:237), so both halves see the same answer; the #670 partition
    // row asserts .otr is accepted, which is what would go red if this TU ever
    // stopped seeing the definition.
    if (IEqualsAscii(ext, ".otr")) {
        return true;
    }
#endif
    // .zip deliberately absent. See the header.
    return false;
}

extern "C" void Combo_RegisterModArchive(GameId game, const char* path) {
    if (!ValidGame(game) || path == nullptr || path[0] == '\0') {
        return;
    }

    std::lock_guard<std::mutex> lock(sModArchivesMutex);
    auto& list = sModArchives[(int)game];
    for (const auto& existing : list) {
        // Idempotent: a re-registration must NOT move the archive to the end.
        // Relative mod order is user-controlled (OoT's mod menu reorders it)
        // and last-added-wins makes that order the precedence order.
        if (existing == path) {
            return;
        }
    }
    list.emplace_back(path);
}

extern "C" int Combo_GetModArchiveCount(GameId game) {
    if (!ValidGame(game)) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(sModArchivesMutex);
    return (int)sModArchives[(int)game].size();
}

extern "C" const char* Combo_GetModArchive(GameId game, int index) {
    if (!ValidGame(game) || index < 0) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(sModArchivesMutex);
    auto& list = sModArchives[(int)game];
    if ((size_t)index >= list.size()) {
        return nullptr;
    }
    // Safe to hand out: entries are only ever appended or cleared wholesale,
    // and std::string's buffer is stable for the lifetime of the element.
    return list[(size_t)index].c_str();
}

extern "C" void Combo_ClearModArchives(GameId game) {
    if (!ValidGame(game)) {
        return;
    }
    std::lock_guard<std::mutex> lock(sModArchivesMutex);
    sModArchives[(int)game].clear();
}

extern "C" const char* Combo_LooseModsDirName(void) {
    return kLooseModsDir;
}

namespace Rsbs {

std::vector<std::string> FindLooseModDirs(GameId game, const std::string& modsRoot) {
    std::vector<std::string> found;
    if (!ValidGame(game) || modsRoot.empty()) {
        return found;
    }
    std::error_code rootEc;
    if (!std::filesystem::is_directory(modsRoot, rootEc)) {
        return found;
    }

    // The candidates are the SAME for both games: `loose` (any case) directly in the
    // root, and `loose` directly in each first-level folder named `mm` (any case —
    // the match MM's archive walk makes through Combo_ModPathIsForGame, so on a
    // case-sensitive filesystem holding both `mods/mm` and `mods/MM` the loose layer
    // is found in both, exactly as the archives are; listed rather than probed at a
    // fixed spelling for that reason). Which of them is THIS game's is then decided
    // by the partition predicate and by nothing else, so the filter below is the
    // one thing that keeps OoT from claiming `mods/mm/loose` and MM from claiming
    // `mods/loose` (PR #732's review found the first version of this loop
    // pre-split by game, which made the filter unreachable; the discovery row
    // now goes red without it).
    std::vector<std::filesystem::path> parents;
    parents.emplace_back(modsRoot);
    {
        std::error_code listEc;
        for (std::filesystem::directory_iterator it(modsRoot, listEc), end; !listEc && it != end;
             it.increment(listEc)) {
            std::error_code entryEc;
            if (it->is_directory(entryEc) && IEqualsAscii(it->path().filename().generic_string(), kMmModsSubdir)) {
                parents.push_back(it->path());
            }
        }
    }

    for (const std::filesystem::path& parent : parents) {
        std::error_code listEc;
        for (std::filesystem::directory_iterator it(parent, listEc), end; !listEc && it != end;
             it.increment(listEc)) {
            std::error_code entryEc;
            if (!it->is_directory(entryEc) || !IEqualsAscii(it->path().filename().generic_string(), kLooseModsDir)) {
                continue;
            }
            const std::string candidate = it->path().generic_string();
            // The partition, enforced: `<root>/loose` is OoT's because its first
            // component is not `mm`, `<root>/mm/loose` is MM's because it is. A
            // folder the OTHER game owns is refused here instead of being mounted
            // and registered under the wrong game — which is the one
            // mis-registration that survives a switch.
            if (!Combo_ModPathIsForGame(game, modsRoot.c_str(), candidate.c_str())) {
                continue;
            }
            found.push_back(candidate);
        }
    }

    // Deterministic mount order when there is more than one (only reachable for MM,
    // on a case-sensitive filesystem). Directory iteration order is unspecified.
    std::sort(found.begin(), found.end());
    return found;
}

std::vector<std::string> MountLooseModDirs(GameId game, const std::string& modsRoot) {
    std::vector<std::string> mounted;
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetResourceManager() == nullptr ||
        ctx->GetResourceManager()->GetArchiveManager() == nullptr) {
        return mounted;
    }
    auto archiveMgr = ctx->GetResourceManager()->GetArchiveManager();
    const char* tag = game == GAME_MM ? "[MM]" : "[OoT]";

    for (const std::string& dir : FindLooseModDirs(game, modsRoot)) {
        // Absolute, normal, '/'-separated: see the header for why FolderArchive
        // needs exactly this spelling. lexically_normal of a directory path can keep
        // a trailing separator; FolderArchive appends its own.
        std::error_code absEc;
        const std::filesystem::path abs = std::filesystem::absolute(dir, absEc);
        std::string archivePath = (absEc ? std::filesystem::path(dir) : abs).lexically_normal().generic_string();
        while (archivePath.size() > 1 && archivePath.back() == '/') {
            archivePath.pop_back();
        }

        std::shared_ptr<Ship::Archive> archive;
        try {
            archive = archiveMgr->AddArchive(archivePath);
        } catch (const std::exception& e) {
            fprintf(stderr, "%s WARNING: could not mount loose asset folder, its files will NOT apply: %s (%s)\n", tag,
                    archivePath.c_str(), e.what());
            continue;
        }
        if (archive == nullptr) {
            fprintf(stderr, "%s WARNING: could not mount loose asset folder, its files will NOT apply: %s\n", tag,
                    archivePath.c_str());
            continue;
        }
        Combo_RegisterModArchive(game, archivePath.c_str());
        const auto files = archive->ListFiles();
        fprintf(stderr, "%s Mounted loose asset folder: %s (%d file(s))\n", tag, archivePath.c_str(),
                files != nullptr ? (int)files->size() : 0);
        mounted.push_back(archivePath);
    }
    return mounted;
}

} // namespace Rsbs
