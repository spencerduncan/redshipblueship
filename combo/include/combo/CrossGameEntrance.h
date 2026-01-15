#pragma once

/**
 * Cross-Game Entrance Infrastructure
 *
 * This module provides the infrastructure for tracking cross-game entrance links,
 * allowing seamless transitions between OoT and MM at specific entrance points.
 *
 * Design:
 * - CrossGameEntranceTable stores bidirectional links between game entrances
 * - C API allows game code to query for cross-game transitions
 * - Extensible for future entrance shuffling (any entrance → any entrance)
 */

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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
 * Represents a link between entrances in different games
 */
struct CrossGameEntranceLink {
    Game sourceGame;
    uint16_t sourceEntrance;     // Entrance ID in source game
    Game targetGame;
    uint16_t targetEntrance;     // Where to spawn in target game
    uint16_t returnEntrance;     // Where to return when coming back

    bool operator==(const CrossGameEntranceLink& other) const {
        return sourceGame == other.sourceGame &&
               sourceEntrance == other.sourceEntrance &&
               targetGame == other.targetGame &&
               targetEntrance == other.targetEntrance;
    }
};

/**
 * Manages cross-game entrance links
 *
 * Thread safety: Not thread-safe. Designed to be used from main game thread.
 */
class CrossGameEntranceTable {
public:
    /**
     * Check if an entrance leads to another game
     */
    bool IsCrossGameEntrance(Game game, uint16_t entrance) const;

    /**
     * Get the link for a cross-game entrance
     * @return The link if found, std::nullopt otherwise
     */
    std::optional<CrossGameEntranceLink> GetLink(Game game, uint16_t entrance) const;

    /**
     * Register a new cross-game link
     * Note: This does NOT automatically register the reverse link
     */
    void RegisterLink(const CrossGameEntranceLink& link);

    /**
     * Register a bidirectional link (convenience method)
     * Registers both forward and return links
     */
    void RegisterBidirectionalLink(
        Game game1, uint16_t entrance1, uint16_t return1,
        Game game2, uint16_t entrance2, uint16_t return2
    );

    /**
     * Register the default OoTMM combomizer links:
     * - OoT Happy Mask Shop ↔ MM Clock Tower Interior
     */
    void RegisterDefaultLinks();

    /**
     * Clear all registered links
     */
    void Clear();

    /**
     * Get the number of registered links
     */
    size_t GetLinkCount() const { return mLinks.size(); }

private:
    std::vector<CrossGameEntranceLink> mLinks;
};

// Global entrance table instance
extern CrossGameEntranceTable gCrossGameEntrances;

// ============================================================================
// Pending switch state
// ============================================================================

/**
 * State for a pending cross-game switch
 */
struct PendingGameSwitch {
    bool requested = false;
    Game targetGame = Game::None;
    uint16_t targetEntrance = 0;
    uint16_t returnEntrance = 0;     // Where to return in source game
    bool readyToSwitch = false;     // Set by game after saving state
};

extern PendingGameSwitch gPendingSwitch;

} // namespace Combo

// ============================================================================
// C API for games to call
// ============================================================================

extern "C" {

/**
 * Check if an entrance triggers a cross-game switch.
 * If it does, sets up the pending switch state.
 *
 * @param gameId "oot" or "mm"
 * @param entrance The entrance being taken
 * @return The entrance to use (original if no cross-game, or a "stay" entrance if switching)
 */
uint16_t Combo_CheckCrossGameEntrance(const char* gameId, uint16_t entrance);

/**
 * Check if a cross-game switch is pending
 */
bool Combo_IsCrossGameSwitch(void);

/**
 * Get the target game ID for the pending switch
 * @return "oot", "mm", or nullptr if no switch pending
 */
const char* Combo_GetSwitchTargetGameId(void);

/**
 * Get the target entrance for the pending switch
 */
uint16_t Combo_GetSwitchTargetEntrance(void);

/**
 * Get the return entrance (where to spawn when coming back to current game)
 */
uint16_t Combo_GetSwitchReturnEntrance(void);

/**
 * Signal that the game has saved its state and is ready to switch
 */
void Combo_SignalReadyToSwitch(void);

/**
 * Clear the pending switch state (called by launcher after switch completes)
 */
void Combo_ClearPendingSwitch(void);

/**
 * Set the startup entrance for a game (used on first cross-game switch)
 * Games should check this on init if doing a cross-game switch
 */
void Combo_SetStartupEntrance(uint16_t entrance);

/**
 * Get the startup entrance (0 if not set)
 * Games call this on init to check if they should override the default entrance
 */
uint16_t Combo_GetStartupEntrance(void);

/**
 * Clear the startup entrance (called after game reads it)
 */
void Combo_ClearStartupEntrance(void);

/**
 * Convert game ID string to enum
 */
Combo::Game Combo_GameFromId(const char* gameId);

/**
 * Convert game enum to ID string
 */
const char* Combo_GameToId(Combo::Game game);

} // extern "C"

// ============================================================================
// Default entrance constants (for reference)
// ============================================================================

// OoT entrances
#define COMBO_OOT_ENTR_HAPPY_MASK_SHOP           0x0530  // Into Happy Mask Shop
#define COMBO_OOT_ENTR_MARKET_FROM_MASK_SHOP     0x01D1  // Out of Happy Mask Shop

// MM entrances (using ENTRANCE macro: ((scene & 0x7F) << 9) | ((spawn & 0x1F) << 4))
// CLOCK_TOWER_INTERIOR scene = 0x60
// SOUTH_CLOCK_TOWN scene = 0x6C
#define COMBO_MM_ENTR_CLOCK_TOWER_INTERIOR_1     0xC010  // ENTRANCE(CLOCK_TOWER_INTERIOR, 1)
#define COMBO_MM_ENTR_SOUTH_CLOCK_TOWN_0         0xD800  // ENTRANCE(SOUTH_CLOCK_TOWN, 0)
