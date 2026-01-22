/**
 * ComboContextBridge implementation
 */

#include "combo/ComboContextBridge.h"
#include <iostream>

namespace Combo {

ComboContextBridge::~ComboContextBridge() {
    // Shutdown and unload all games
    for (auto& [game, state] : mGames) {
        if (state.initialized && state.exports.Shutdown) {
            state.exports.Shutdown();
        }
    }
    mGames.clear();
}

bool ComboContextBridge::LoadGame(Game game, const std::string& libPath) {
    if (game == Game::None) {
        std::cerr << "ComboContextBridge: Cannot load Game::None" << std::endl;
        return false;
    }

    // Check if already loaded
    if (mGames.count(game) && mGames[game].library.IsValid()) {
        std::cerr << "ComboContextBridge: Game " << GameToId(game)
                  << " is already loaded" << std::endl;
        return true;
    }

    // Load the shared library
    ScopedLibrary lib(libPath);
    if (!lib.IsValid()) {
        std::cerr << "ComboContextBridge: Failed to load library: " << libPath
                  << " - " << DynamicLibrary::GetError() << std::endl;
        return false;
    }

    // Resolve exports
    GameExports exports;

    // Required exports
    exports.Init = reinterpret_cast<GameInitFn>(lib.GetSymbol("Game_Init"));
    exports.Run = reinterpret_cast<GameRunFn>(lib.GetSymbol("Game_Run"));
    exports.Shutdown = reinterpret_cast<GameShutdownFn>(lib.GetSymbol("Game_Shutdown"));
    exports.GetName = reinterpret_cast<GameGetNameFn>(lib.GetSymbol("Game_GetName"));
    exports.GetId = reinterpret_cast<GameGetIdFn>(lib.GetSymbol("Game_GetId"));

    // Optional hot-switch exports
    exports.Pause = reinterpret_cast<GamePauseFn>(lib.GetSymbol("Game_Pause"));
    exports.Resume = reinterpret_cast<GameResumeFn>(lib.GetSymbol("Game_Resume"));
    exports.SaveState = reinterpret_cast<GameSaveStateFn>(lib.GetSymbol("Game_SaveState"));
    exports.LoadState = reinterpret_cast<GameLoadStateFn>(lib.GetSymbol("Game_LoadState"));

    if (!exports.HasRequiredExports()) {
        std::cerr << "ComboContextBridge: Library missing required exports: "
                  << libPath << std::endl;
        return false;
    }

    // Store the loaded game state
    GameState state;
    state.library = std::move(lib);
    state.exports = exports;
    state.initialized = false;
    state.running = false;

    mGames[game] = std::move(state);

    std::cout << "ComboContextBridge: Loaded " << exports.GetName()
              << " (" << exports.GetId() << ")" << std::endl;

    return true;
}

void ComboContextBridge::UnloadGame(Game game) {
    auto it = mGames.find(game);
    if (it == mGames.end()) {
        return;
    }

    auto& state = it->second;

    // Shutdown if initialized
    if (state.initialized && state.exports.Shutdown) {
        state.exports.Shutdown();
    }

    // Clear active game if this was it
    if (mActiveGame == game) {
        mActiveGame = Game::None;
    }

    // Remove from map (ScopedLibrary will unload)
    mGames.erase(it);
}

bool ComboContextBridge::IsGameLoaded(Game game) const {
    auto it = mGames.find(game);
    return it != mGames.end() && it->second.library.IsValid();
}

bool ComboContextBridge::IsGameInitialized(Game game) const {
    auto it = mGames.find(game);
    return it != mGames.end() && it->second.initialized;
}

bool ComboContextBridge::SwitchGame(Game game) {
    if (!IsGameLoaded(game)) {
        std::cerr << "ComboContextBridge: Cannot switch to unloaded game: "
                  << GameToId(game) << std::endl;
        return false;
    }

    // TODO: In Phase 2, implement state save for current game
    // and state restore for target game for true hot-switching

    if (mActiveGame != Game::None && mActiveGame != game) {
        auto& currentState = mGames[mActiveGame];
        if (currentState.running && currentState.exports.Pause) {
            currentState.exports.Pause();
        }
    }

    mActiveGame = game;

    std::cout << "ComboContextBridge: Switched to " << GameToId(game) << std::endl;

    return true;
}

int ComboContextBridge::Init(int argc, char** argv) {
    if (mActiveGame == Game::None) {
        std::cerr << "ComboContextBridge: No active game to initialize" << std::endl;
        return -1;
    }

    auto& state = mGames[mActiveGame];

    if (state.initialized) {
        return 0;
    }

    if (!state.exports.Init) {
        std::cerr << "ComboContextBridge: No Init function for active game" << std::endl;
        return -1;
    }

    int result = state.exports.Init(argc, argv);

    if (result == 0) {
        state.initialized = true;
    }

    return result;
}

void ComboContextBridge::Run() {
    if (mActiveGame == Game::None) {
        std::cerr << "ComboContextBridge: No active game to run" << std::endl;
        return;
    }

    auto& state = mGames[mActiveGame];

    if (!state.initialized) {
        std::cerr << "ComboContextBridge: Game not initialized" << std::endl;
        return;
    }

    if (!state.exports.Run) {
        std::cerr << "ComboContextBridge: No Run function for active game" << std::endl;
        return;
    }

    state.running = true;
    state.exports.Run();
    state.running = false;
}

void ComboContextBridge::Shutdown() {
    if (mActiveGame == Game::None) {
        return;
    }

    auto& state = mGames[mActiveGame];

    if (!state.initialized) {
        return;
    }

    if (state.exports.Shutdown) {
        state.exports.Shutdown();
    }

    state.initialized = false;
    state.running = false;
}

std::optional<std::string> ComboContextBridge::GetGameName(Game game) const {
    auto it = mGames.find(game);
    if (it == mGames.end() || !it->second.exports.GetName) {
        return std::nullopt;
    }
    return it->second.exports.GetName();
}

std::optional<std::string> ComboContextBridge::GetGameId(Game game) const {
    auto it = mGames.find(game);
    if (it == mGames.end() || !it->second.exports.GetId) {
        return std::nullopt;
    }
    return it->second.exports.GetId();
}

const GameExports* ComboContextBridge::GetGameExports(Game game) const {
    auto it = mGames.find(game);
    if (it == mGames.end()) {
        return nullptr;
    }
    return &it->second.exports;
}

} // namespace Combo

// ============================================================================
// Global hotswap mechanism implementation
// ============================================================================

// Global state for hotswap - shared between launcher and games
static bool gGameSwitchRequested = false;
static const char* gSwitchTargetId = nullptr;

void Combo_RequestGameSwitch() {
    gGameSwitchRequested = true;
}

bool Combo_IsGameSwitchRequested() {
    return gGameSwitchRequested;
}

void Combo_ClearGameSwitchRequest() {
    gGameSwitchRequested = false;
    gSwitchTargetId = nullptr;
}

const char* Combo_GetSwitchTargetId() {
    return gSwitchTargetId;
}

// Internal function to set the switch target (called by launcher)
void Combo_SetSwitchTarget(const char* targetId) {
    gSwitchTargetId = targetId;
}
