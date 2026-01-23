#pragma once

/**
 * CrossGameEntrance.h - Compatibility header for legacy combo/ code
 *
 * This header provides the C++ classes and C API expected by existing code
 * while the actual implementation has moved to src/common/entrance.h.
 *
 * For new code, prefer using src/common/entrance.h directly.
 */

#include "combo/Export.h"
#include "combo/Game.h"

// Include the new C API
extern "C" {
#include "entrance.h"  // From src/common/ via include path
}

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Combo {

/**
 * Represents a link between entrances in different games
 * This wraps the C struct CrossGameEntranceLink
 */
struct COMBO_API CrossGameEntranceLink {
    Game sourceGame;
    uint16_t sourceEntrance;
    Game targetGame;
    uint16_t targetEntrance;
    uint16_t returnEntrance;

    bool operator==(const CrossGameEntranceLink& other) const {
        return sourceGame == other.sourceGame &&
               sourceEntrance == other.sourceEntrance &&
               targetGame == other.targetGame &&
               targetEntrance == other.targetEntrance;
    }
};

/**
 * Manages cross-game entrance links
 * This is a C++ wrapper around the C API in entrance.h
 */
class COMBO_API CrossGameEntranceTable {
public:
    bool IsCrossGameEntrance(Game game, uint16_t entrance) const {
        uint16_t result = Entrance_CheckCrossGame(ToGameId(game), entrance);
        return result != entrance || Entrance_IsCrossGameSwitch();
    }

    std::optional<CrossGameEntranceLink> GetLink(Game game, uint16_t entrance) const {
        uint16_t result = Entrance_CheckCrossGame(ToGameId(game), entrance);
        if (Entrance_IsCrossGameSwitch()) {
            CrossGameEntranceLink link;
            link.sourceGame = game;
            link.sourceEntrance = entrance;
            link.targetGame = FromGameId(Entrance_GetSwitchTargetGame());
            link.targetEntrance = Entrance_GetSwitchTargetEntrance();
            link.returnEntrance = Entrance_GetSwitchReturnEntrance();
            Entrance_ClearPendingSwitch();  // Reset so next query works
            return link;
        }
        return std::nullopt;
    }

    void RegisterBidirectionalLink(
        Game game1, uint16_t entrance1, uint16_t return1,
        Game game2, uint16_t entrance2, uint16_t return2
    ) {
        Entrance_RegisterBidirectionalLink(
            ToGameId(game1), entrance1, return1,
            ToGameId(game2), entrance2, return2
        );
    }

    void RegisterDefaultLinks() {
        Entrance_RegisterDefaultLinks();
    }

    void Clear() {
        Entrance_ClearLinks();
    }

    size_t Size() const {
        return Entrance_GetLinkCount();
    }
};

// Global entrance table
inline CrossGameEntranceTable& GetEntranceTable() {
    static CrossGameEntranceTable table;
    return table;
}

/**
 * Pending game switch state (C++ wrapper)
 */
struct COMBO_API PendingGameSwitch {
    bool requested = false;
    Game targetGame = Game::None;
    uint16_t targetEntrance = 0;
    uint16_t returnEntrance = 0;
    bool readyToSwitch = false;
};

// Note: gPendingSwitch is defined in entrance.h as a C struct
// For C++ code that expects namespace Combo, provide an alias
inline PendingGameSwitch GetPendingSwitch() {
    PendingGameSwitch ps;
    ps.requested = ::gPendingSwitch.requested;
    ps.targetGame = FromGameId(::gPendingSwitch.targetGame);
    ps.targetEntrance = ::gPendingSwitch.targetEntrance;
    ps.returnEntrance = ::gPendingSwitch.returnEntrance;
    ps.readyToSwitch = ::gPendingSwitch.readyToSwitch;
    return ps;
}

} // namespace Combo

// Re-export entrance constants with COMBO_ prefix for compatibility
#define COMBO_OOT_ENTR_HAPPY_MASK_SHOP       OOT_ENTR_HAPPY_MASK_SHOP
#define COMBO_OOT_ENTR_MARKET_FROM_MASK_SHOP OOT_ENTR_MARKET_FROM_MASK_SHOP
#define COMBO_OOT_ENTR_MIDOS_HOUSE           OOT_ENTR_MIDOS_HOUSE
#define COMBO_OOT_ENTR_KOKIRI_FROM_MIDOS     OOT_ENTR_KOKIRI_FROM_MIDOS
#define COMBO_MM_ENTR_CLOCK_TOWER_INTERIOR_1 MM_ENTR_CLOCK_TOWER_INTERIOR_1
#define COMBO_MM_ENTR_SOUTH_CLOCK_TOWN_0     MM_ENTR_SOUTH_CLOCK_TOWN_0
