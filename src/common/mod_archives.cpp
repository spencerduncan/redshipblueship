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

#include <cctype>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>

namespace {

// The one subdirectory of the shared mods/ tree that is MM's. See the header for
// why the tree is partitioned at all (the portable-build collapse of
// LocateFileAcrossAppDirs's appName argument) and why OoT keeps the root.
constexpr const char kMmModsSubdir[] = "mm";

// The app short names the two ports hand LocateFileAcrossAppDirs, in ONE place.
// games/oot/soh/OTRGlobals.h's `appShortName` and
// games/mm/2s2h/GameExports_SingleExe.cpp's `kMmAppName` are the originals, and
// both mods lookups now go through Combo_ModsRootForGame below rather than
// spelling the call out again. A drift between two copies of "2s2h" would be
// silent and expensive: Combo_ModsRootsAreShared would answer "not shared", OoT
// would claim mods/mm although MM is globbing that very directory, and every MM
// mod would be mounted twice and registered under BOTH games — the cross-game
// shadowing the partition exists to prevent.
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

// OoT's rule, computed independently rather than as `!PathIsUnderMmSubdir`: the
// path lies under the root AND its first component is not a folder reserved for
// another game.
//
// Why not the negation, which is shorter and "total by construction"? Because
// then there is no partition to check. Two answers derived from one bool in one
// expression cannot disagree, so the disjointness assertion in the #670 row
// (test_mm_mods_mount.c) was a restatement of the return statement with no
// reachable red half — the reviewer of PR #704 was right about that. Written as
// its own rule the two CAN disagree, so the row's table measures something: it
// pins each game's answer per path against the authored expectation, and the
// disjointness and totality checks fire when the two rules drift. The price is
// that totality now holds only over paths under the root, which is the honest
// domain anyway — a path in a different tree belongs to neither game, and the
// old code answered "OoT's" for it.
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
    // Two independent rules, not one bool and its negation. See PathIsOoTs.
    return game == GAME_MM ? PathIsUnderMmSubdir(modsRoot, path) : PathIsOoTs(modsRoot, path);
}

extern "C" const char* Combo_ModsRootForGame(GameId game) {
    if (!ValidGame(game)) {
        return "";
    }
    const char* appName = game == GAME_MM ? kMmAppShortName : kOoTAppShortName;
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
