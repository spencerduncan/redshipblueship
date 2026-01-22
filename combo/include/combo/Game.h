#pragma once

/**
 * Game.h - Canonical definition of the Game enum
 *
 * This is the single source of truth for the Game enum used throughout
 * the combo codebase. All headers that need the Game enum should include
 * this file instead of defining their own.
 */

#include <cstdint>

namespace Combo {

/**
 * Game identifier enum
 */
enum class Game : uint8_t {
    None = 0,
    OoT = 1,
    MM = 2
};

/**
 * Get the combo library version string
 * @return Version string (e.g., "0.1.0-dev")
 */
const char* GetVersion();

} // namespace Combo
