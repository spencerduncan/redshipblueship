#include "combo/GameArchives.h"
#include "ship/resource/archive/Archive.h"
#include "ship/resource/File.h"
#include <stdexcept>

namespace Combo {

GameArchiveManager& GameArchiveManager::Instance() {
    static GameArchiveManager instance;
    return instance;
}

void GameArchiveManager::RegisterArchive(Game game, std::shared_ptr<Ship::Archive> archive) {
    if (archive) {
        mGameArchives[game].push_back(archive);
    }
}

std::vector<std::shared_ptr<Ship::Archive>> GameArchiveManager::GetArchives(Game game) const {
    auto it = mGameArchives.find(game);
    if (it != mGameArchives.end()) {
        return it->second;
    }
    return {};
}

std::shared_ptr<Ship::File> GameArchiveManager::LoadFile(Game game, const std::string& path) {
    auto archives = GetArchives(game);
    for (const auto& archive : archives) {
        if (archive && archive->HasFile(path)) {
            return archive->LoadFile(path);
        }
    }
    return nullptr;
}

bool GameArchiveManager::HasFile(Game game, const std::string& path) const {
    auto archives = GetArchives(game);
    for (const auto& archive : archives) {
        if (archive && archive->HasFile(path)) {
            return true;
        }
    }
    return false;
}

Game GameArchiveManager::GetFileOwner(const std::string& path) const {
    bool inOoT = HasFile(Game::OOT, path);
    bool inMM = HasFile(Game::MM, path);

    if (inOoT && inMM) {
        throw std::runtime_error("File exists in both games: " + path);
    }
    if (inOoT) {
        return Game::OOT;
    }
    if (inMM) {
        return Game::MM;
    }
    throw std::runtime_error("File not found in any game: " + path);
}

bool GameArchiveManager::IsGameLoaded(Game game) const {
    auto it = mGameArchives.find(game);
    return it != mGameArchives.end() && !it->second.empty();
}

void GameArchiveManager::Clear() {
    mGameArchives.clear();
}

} // namespace Combo
