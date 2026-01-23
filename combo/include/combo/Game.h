#pragma once

/**
 * Game.h - Compatibility header for legacy combo/ code
 *
 * This header provides the Combo::Game enum class expected by existing code
 * while the actual implementation has moved to src/common/game.h.
 *
 * For new code, prefer using src/common/game.h directly.
 */

#include <cstdint>

// Include the new C API for GameId enum and functions
extern "C" {
#include "game.h"  // From src/common/ via include path
}

namespace Combo {

/**
 * Game identifier enum (C++ enum class wrapper around GameId)
 */
enum class Game : uint8_t {
    None = GAME_NONE,
    OoT = GAME_OOT,
    MM = GAME_MM
};

/**
 * Convert between Combo::Game and GameId
 */
inline GameId ToGameId(Game game) {
    return static_cast<GameId>(static_cast<uint8_t>(game));
}

inline Game FromGameId(GameId id) {
    return static_cast<Game>(static_cast<uint8_t>(id));
}

/**
 * Get the combo library version string
 * @return Version string (e.g., "0.1.0-dev")
 */
inline const char* GetVersion() {
    return "0.2.0-single-exe";  // Updated for single executable architecture
}

} // namespace Combo

// C API compatibility - these forward to the new Game_* functions
extern "C" {
    inline Combo::Game Combo_GameFromId(const char* gameId) {
        return Combo::FromGameId(Game_FromString(gameId));
    }

    inline const char* Combo_GameToId(Combo::Game game) {
        return Game_ToString(Combo::ToGameId(game));
    }
}
