/**
 * ComboContext.h - Cross-game context and switching infrastructure
 *
 * This header defines the shared context structure that persists across
 * game switches and enables cross-game state transfer.
 */

#pragma once

#include <cstdint>
#include <cstring>

namespace Combo {

/**
 * Game identifier for runtime dispatch
 */
enum class Game : uint8_t {
    None = 0,
    OoT = 1,
    MM = 2
};

/**
 * ComboContext - Shared state that persists across game switches
 *
 * This structure is the central hub for cross-game communication.
 * It's designed to be accessible from both C and C++ code.
 */
struct ComboContext {
    // Magic validation string
    char magic[8] = {'O', 'o', 'T', '+', 'M', 'M', '<', '3'};

    // Version for compatibility checking
    uint32_t version = 1;

    // Switch control flags
    bool switchRequested = false;
    Game targetGame = Game::None;
    uint16_t targetEntrance = 0;

    // Source state (for return trips)
    Game sourceGame = Game::None;
    uint16_t sourceEntrance = 0;

    // Shared persistent state
    uint32_t sharedFlags[64] = {};      // Cross-game event flags
    uint16_t sharedItems[32] = {};      // Shared inventory items

    // Save slot tracking
    int32_t saveSlot = 0;

    /**
     * Validate the magic string to ensure this is a valid context
     */
    bool IsValid() const {
        return magic[0] == 'O' && magic[1] == 'o' && magic[2] == 'T' &&
               magic[3] == '+' && magic[4] == 'M' && magic[5] == 'M' &&
               magic[6] == '<' && magic[7] == '3';
    }

    /**
     * Reset to default state
     */
    void Reset() {
        *this = ComboContext{};
    }
};

// Global context instance (defined in unified_main.cpp or context.cpp)
extern ComboContext gComboCtx;
extern Game gCurrentGame;

} // namespace Combo

// C API for game code integration
extern "C" {

/**
 * Request a game switch to the other game.
 *
 * @param targetEntrance The entrance ID in the target game (0 for default)
 */
void Combo_RequestSwitch(uint16_t targetEntrance);

/**
 * Request a specific game switch.
 *
 * @param target The target game
 * @param entrance The entrance ID in the target game
 */
void Combo_SwitchToGame(Combo::Game target, uint16_t entrance);

/**
 * Check if a game switch has been requested.
 */
bool Combo_IsSwitchRequested(void);

/**
 * Get the current game.
 */
Combo::Game Combo_GetCurrentGame(void);

/**
 * Set a shared flag (accessible from both games).
 *
 * @param flagId The flag index (0-63)
 * @param bit The bit within the flag (0-31)
 */
void Combo_SetSharedFlag(uint8_t flagId, uint8_t bit);

/**
 * Check a shared flag.
 */
bool Combo_GetSharedFlag(uint8_t flagId, uint8_t bit);

/**
 * Clear a shared flag.
 */
void Combo_ClearSharedFlag(uint8_t flagId, uint8_t bit);

} // extern "C"
