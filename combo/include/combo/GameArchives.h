#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations
namespace Ship {
class Archive;
class ArchiveManager;
struct File;
} // namespace Ship

namespace Combo {

enum class Game {
    OOT,
    MM
};

/**
 * Manages game-specific archive access for the combo randomizer.
 *
 * Since OoT and MM share ~40% of asset paths (e.g., objects/object_link_boy),
 * loading both archives into a single namespace causes collisions.
 * This class tracks which archives belong to which game and provides
 * game-specific loading APIs.
 */
class GameArchiveManager {
public:
    static GameArchiveManager& Instance();

    /**
     * Register an archive as belonging to a specific game.
     * Call this during initialization after loading archives.
     */
    void RegisterArchive(Game game, std::shared_ptr<Ship::Archive> archive);

    /**
     * Get all archives for a specific game.
     */
    std::vector<std::shared_ptr<Ship::Archive>> GetArchives(Game game) const;

    /**
     * Load a file from a specific game's archives.
     * Searches only the archives registered for that game.
     */
    std::shared_ptr<Ship::File> LoadFile(Game game, const std::string& path);

    /**
     * Check if a file exists in a specific game's archives.
     */
    bool HasFile(Game game, const std::string& path) const;

    /**
     * Determine which game owns a file (checks all registered archives).
     * Returns the game if found in exactly one, or throws if ambiguous/missing.
     */
    Game GetFileOwner(const std::string& path) const;

    /**
     * Check if archives are registered for a game.
     */
    bool IsGameLoaded(Game game) const;

    /**
     * Clear all registered archives (useful for testing).
     */
    void Clear();

private:
    GameArchiveManager() = default;
    ~GameArchiveManager() = default;
    GameArchiveManager(const GameArchiveManager&) = delete;
    GameArchiveManager& operator=(const GameArchiveManager&) = delete;

    std::unordered_map<Game, std::vector<std::shared_ptr<Ship::Archive>>> mGameArchives;
};

// Convenience functions
inline std::shared_ptr<Ship::File> LoadOoTFile(const std::string& path) {
    return GameArchiveManager::Instance().LoadFile(Game::OOT, path);
}

inline std::shared_ptr<Ship::File> LoadMMFile(const std::string& path) {
    return GameArchiveManager::Instance().LoadFile(Game::MM, path);
}

} // namespace Combo
