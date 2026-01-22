#pragma once

#include "combo/Export.h"
#include "combo/Game.h"

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
 * Represents a link between entrances in different games
 */
struct COMBO_API CrossGameEntranceLink {
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
class COMBO_API CrossGameEntranceTable {
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
     * Register test links (for easier testing):
     * - OoT Mido's House ↔ MM Clock Tower Interior
     */
    void RegisterTestLinks();

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
extern COMBO_API CrossGameEntranceTable gCrossGameEntrances;

// ============================================================================
// Pending switch state
// ============================================================================

/**
 * State for a pending cross-game switch
 */
struct COMBO_API PendingGameSwitch {
    bool requested = false;
    Game targetGame = Game::None;
    uint16_t targetEntrance = 0;
    uint16_t returnEntrance = 0;     // Where to return in source game
    bool readyToSwitch = false;     // Set by game after saving state
};

extern COMBO_API PendingGameSwitch gPendingSwitch;

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
COMBO_API uint16_t Combo_CheckCrossGameEntrance(const char* gameId, uint16_t entrance);

/**
 * Check if a cross-game switch is pending
 */
COMBO_API bool Combo_IsCrossGameSwitch(void);

/**
 * Get the target game ID for the pending switch
 * @return "oot", "mm", or nullptr if no switch pending
 */
COMBO_API const char* Combo_GetSwitchTargetGameId(void);

/**
 * Get the target entrance for the pending switch
 */
COMBO_API uint16_t Combo_GetSwitchTargetEntrance(void);

/**
 * Get the return entrance (where to spawn when coming back to current game)
 */
COMBO_API uint16_t Combo_GetSwitchReturnEntrance(void);

/**
 * Signal that the game has saved its state and is ready to switch
 */
COMBO_API void Combo_SignalReadyToSwitch(void);

/**
 * Clear the pending switch state (called by launcher after switch completes)
 */
COMBO_API void Combo_ClearPendingSwitch(void);

/**
 * Set the startup entrance for a game (used on first cross-game switch)
 * Games should check this on init if doing a cross-game switch
 */
COMBO_API void Combo_SetStartupEntrance(uint16_t entrance);

/**
 * Get the startup entrance (0 if not set)
 * Games call this on init to check if they should override the default entrance
 */
COMBO_API uint16_t Combo_GetStartupEntrance(void);

/**
 * Clear the startup entrance (called after game reads it)
 */
COMBO_API void Combo_ClearStartupEntrance(void);

/**
 * Convert game ID string to enum
 */
COMBO_API Combo::Game Combo_GameFromId(const char* gameId);

/**
 * Convert game enum to ID string
 */
COMBO_API const char* Combo_GameToId(Combo::Game game);

} // extern "C"

// ============================================================================
// Default entrance constants (for reference)
// ============================================================================

// OoT entrances - Happy Mask Shop (production)
#define COMBO_OOT_ENTR_HAPPY_MASK_SHOP           0x0530  // Into Happy Mask Shop
#define COMBO_OOT_ENTR_MARKET_FROM_MASK_SHOP     0x01D1  // Out of Happy Mask Shop

// OoT entrances - Mido's House (testing - closer to spawn)
#define COMBO_OOT_ENTR_MIDOS_HOUSE               0x0433  // Into Mido's House
#define COMBO_OOT_ENTR_KOKIRI_FROM_MIDOS         0x0443  // Out of Mido's House

// MM entrances (using ENTRANCE macro: ((scene & 0x7F) << 9) | ((spawn & 0x1F) << 4))
// CLOCK_TOWER_INTERIOR scene = 0x60
// SOUTH_CLOCK_TOWN scene = 0x6C
#define COMBO_MM_ENTR_CLOCK_TOWER_INTERIOR_1     0xC010  // ENTRANCE(CLOCK_TOWER_INTERIOR, 1)
#define COMBO_MM_ENTR_SOUTH_CLOCK_TOWN_0         0xD800  // ENTRANCE(SOUTH_CLOCK_TOWN, 0)
