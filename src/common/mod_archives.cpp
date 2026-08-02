/**
 * @file mod_archives.cpp
 * @brief Implementation of the per-game mod-archive registry (issue #593).
 *
 * See mod_archives.h for why this exists. Storage only — the actual re-mount
 * lives in EnsureGameArchivesLoaded (rsbs/src/main.cpp), which is the one
 * place that knows a switch is happening.
 */

#include "mod_archives.h"

#include <deque>
#include <mutex>
#include <string>

namespace {

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

} // namespace

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
