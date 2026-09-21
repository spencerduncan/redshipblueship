/**
 * @file mod_archives.cpp
 * @brief Implementation of the per-game mod-archive registry (issue #593).
 *
 * See mod_archives.h for why this exists. Storage only — the actual re-mount
 * lives in EnsureGameArchivesLoaded (rsbs/src/main.cpp), which is the one
 * place that knows a switch is happening.
 */

#include "mod_archives.h"

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

// Is `path` inside `<modsRoot>/mm`, at any depth?
//
// lexically_relative, not a textual prefix compare: the two strings reach this
// function from different places and are not spelled the same way. modsRoot
// comes from LocateFileAcrossAppDirs and can be "./mods"; a path can arrive
// already lexically_normal()'d ("mods/mm/x.o2r") from mod_menu's own
// bookkeeping, or with native '\' separators on Windows. Comparing the
// normalized RELATIVE path makes all of those spellings agree, and a path that
// is not under the root at all yields a ".." first component and is rejected
// rather than silently treated as MM's.
bool PathIsUnderMmSubdir(const char* modsRoot, const char* path) {
    if (modsRoot == nullptr || modsRoot[0] == '\0' || path == nullptr || path[0] == '\0') {
        return false;
    }

    std::error_code ec;
    const std::filesystem::path rootPath = std::filesystem::path(modsRoot).lexically_normal();
    const std::filesystem::path filePath = std::filesystem::path(path).lexically_normal();
    const std::filesystem::path rel = filePath.lexically_relative(rootPath);
    (void)ec;

    if (rel.empty()) {
        return false;
    }

    auto it = rel.begin();
    if (it == rel.end()) {
        return false;
    }
    return IEqualsAscii(it->generic_string(), kMmModsSubdir);
}

} // namespace

extern "C" const char* Combo_ModsSubdirForGame(GameId game) {
    return game == GAME_MM ? kMmModsSubdir : "";
}

extern "C" bool Combo_ModPathIsForGame(GameId game, const char* modsRoot, const char* path) {
    if (!ValidGame(game) || modsRoot == nullptr || modsRoot[0] == '\0' || path == nullptr || path[0] == '\0') {
        return false;
    }
    // Exactly one of the two claims any path: MM claims mods/mm, OoT claims the
    // complement. Writing OoT's side as the negation rather than as its own rule
    // is what makes the partition total by construction, and it keeps OoT's
    // behaviour for every path that is not under mods/mm bit-for-bit what it was
    // before #670.
    const bool isMm = PathIsUnderMmSubdir(modsRoot, path);
    return game == GAME_MM ? isMm : !isMm;
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
