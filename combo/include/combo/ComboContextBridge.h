/**
 * ComboContextBridge - Coordinates multiple game contexts
 *
 * This bridge manages the loading and switching between OoT and MM games,
 * each running as separate shared libraries with their own Ship::Context.
 * The shared library approach provides natural symbol isolation without
 * requiring modifications to libultraship.
 */

#pragma once

#include <string>
#include <map>
#include <optional>
#include "combo/DynamicLibrary.h"
#include "combo/GameExports.h"
#include "combo/CrossGameEntrance.h"  // For Game enum

namespace Combo {

// Game enum is defined in CrossGameEntrance.h
// Helper functions to convert between Game enum and string identifiers

/**
 * Converts Game enum to string identifier
 */
inline const char* GameToId(Game game) {
    return Combo_GameToId(game);  // Use the C API from CrossGameEntrance.h
}

/**
 * Converts string identifier to Game enum
 */
inline Game IdToGame(const std::string& id) {
    return Combo_GameFromId(id.c_str());  // Use the C API from CrossGameEntrance.h
}

/**
 * State of a loaded game
 */
struct GameState {
    ScopedLibrary library;
    GameExports exports;
    bool initialized = false;
    bool running = false;
};

/**
 * Bridge that coordinates multiple game contexts
 */
class ComboContextBridge {
public:
    ComboContextBridge() = default;
    ~ComboContextBridge();

    // Prevent copying
    ComboContextBridge(const ComboContextBridge&) = delete;
    ComboContextBridge& operator=(const ComboContextBridge&) = delete;

    // Allow moving
    ComboContextBridge(ComboContextBridge&&) = default;
    ComboContextBridge& operator=(ComboContextBridge&&) = default;

    /**
     * Load a game's shared library and resolve its exports
     * @param game Which game to load
     * @param libPath Path to the shared library (.so/.dll/.dylib)
     * @return true if successfully loaded and exports resolved
     */
    bool LoadGame(Game game, const std::string& libPath);

    /**
     * Unload a game's shared library
     * @param game Which game to unload
     */
    void UnloadGame(Game game);

    /**
     * Check if a game is loaded
     */
    bool IsGameLoaded(Game game) const;

    /**
     * Get the currently active game
     */
    Game GetActiveGame() const { return mActiveGame; }

    /**
     * Switch to a different game
     * For now, this just sets the active game flag.
     * TODO: In Phase 2, implement state save/restore for hot-switching
     * @param game Which game to make active
     * @return true if switch was successful
     */
    bool SwitchGame(Game game);

    /**
     * Initialize the active game
     * @param argc Argument count
     * @param argv Argument vector
     * @return 0 on success, non-zero on failure
     */
    int Init(int argc, char** argv);

    /**
     * Run the active game's main loop
     * Note: This blocks until the game exits
     */
    void Run();

    /**
     * Shutdown the active game
     */
    void Shutdown();

    /**
     * Get game info for a loaded game
     */
    std::optional<std::string> GetGameName(Game game) const;
    std::optional<std::string> GetGameId(Game game) const;

    /**
     * Get the exports for a loaded game (for advanced usage)
     */
    const GameExports* GetGameExports(Game game) const;

private:
    std::map<Game, GameState> mGames;
    Game mActiveGame = Game::None;
};

} // namespace Combo

// Global hotswap mechanism - accessible from games
// These are extern "C" so games can easily call them
extern "C" {
    /**
     * Request a game switch (called when hotkey is pressed)
     */
    void Combo_RequestGameSwitch();

    /**
     * Check if a game switch was requested
     * Games should check this in their main loop and exit cleanly if true
     */
    bool Combo_IsGameSwitchRequested();

    /**
     * Clear the switch request (called by combo launcher after switching)
     */
    void Combo_ClearGameSwitchRequest();

    /**
     * Get the current switch target game ID ("oot" or "mm")
     * Returns nullptr if no switch requested
     */
    const char* Combo_GetSwitchTargetId();
}
