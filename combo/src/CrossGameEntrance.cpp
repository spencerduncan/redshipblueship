#include "combo/CrossGameEntrance.h"
#include <cstring>
#include <algorithm>

namespace Combo {

// Global instances
CrossGameEntranceTable gCrossGameEntrances;
PendingGameSwitch gPendingSwitch;

// ============================================================================
// CrossGameEntranceTable implementation
// ============================================================================

bool CrossGameEntranceTable::IsCrossGameEntrance(Game game, uint16_t entrance) const {
    return GetLink(game, entrance).has_value();
}

std::optional<CrossGameEntranceLink> CrossGameEntranceTable::GetLink(Game game, uint16_t entrance) const {
    auto it = std::find_if(mLinks.begin(), mLinks.end(),
        [game, entrance](const CrossGameEntranceLink& link) {
            return link.sourceGame == game && link.sourceEntrance == entrance;
        });

    if (it != mLinks.end()) {
        return *it;
    }
    return std::nullopt;
}

void CrossGameEntranceTable::RegisterLink(const CrossGameEntranceLink& link) {
    // Check for duplicate
    auto existing = GetLink(link.sourceGame, link.sourceEntrance);
    if (existing.has_value()) {
        // Replace existing link
        auto it = std::find_if(mLinks.begin(), mLinks.end(),
            [&link](const CrossGameEntranceLink& l) {
                return l.sourceGame == link.sourceGame && l.sourceEntrance == link.sourceEntrance;
            });
        if (it != mLinks.end()) {
            *it = link;
        }
    } else {
        mLinks.push_back(link);
    }
}

void CrossGameEntranceTable::RegisterBidirectionalLink(
    Game game1, uint16_t entrance1, uint16_t return1,
    Game game2, uint16_t entrance2, uint16_t return2
) {
    // Forward link: game1:entrance1 → game2:entrance2
    RegisterLink({
        .sourceGame = game1,
        .sourceEntrance = entrance1,
        .targetGame = game2,
        .targetEntrance = entrance2,
        .returnEntrance = return1
    });

    // Reverse link: game2:return2 → game1:return1
    // (exit from game2 at return2 leads back to game1 at return1)
    RegisterLink({
        .sourceGame = game2,
        .sourceEntrance = return2,
        .targetGame = game1,
        .targetEntrance = return1,
        .returnEntrance = entrance2
    });
}

void CrossGameEntranceTable::RegisterDefaultLinks() {
    // OoT Happy Mask Shop ↔ MM Clock Tower Interior
    //
    // Forward: Entering Happy Mask Shop (OoT) → Clock Tower Interior (MM)
    // Return:  Exiting Clock Tower (MM) to South Clock Town → Back to Market (OoT)

    RegisterBidirectionalLink(
        // OoT side: Happy Mask Shop entrance, return to Market outside shop
        Game::OoT, COMBO_OOT_ENTR_HAPPY_MASK_SHOP, COMBO_OOT_ENTR_MARKET_FROM_MASK_SHOP,
        // MM side: Clock Tower spawn 1, exit to South Clock Town
        Game::MM, COMBO_MM_ENTR_CLOCK_TOWER_INTERIOR_1, COMBO_MM_ENTR_SOUTH_CLOCK_TOWN_0
    );
}

void CrossGameEntranceTable::Clear() {
    mLinks.clear();
}

} // namespace Combo

// ============================================================================
// C API implementation
// ============================================================================

extern "C" {

Combo::Game Combo_GameFromId(const char* gameId) {
    if (gameId == nullptr) {
        return Combo::Game::None;
    }
    if (strcmp(gameId, "oot") == 0) {
        return Combo::Game::OoT;
    }
    if (strcmp(gameId, "mm") == 0) {
        return Combo::Game::MM;
    }
    return Combo::Game::None;
}

const char* Combo_GameToId(Combo::Game game) {
    switch (game) {
        case Combo::Game::OoT: return "oot";
        case Combo::Game::MM: return "mm";
        default: return nullptr;
    }
}

uint16_t Combo_CheckCrossGameEntrance(const char* gameId, uint16_t entrance) {
    Combo::Game game = Combo_GameFromId(gameId);
    if (game == Combo::Game::None) {
        return entrance;
    }

    auto link = Combo::gCrossGameEntrances.GetLink(game, entrance);
    if (!link.has_value()) {
        // Not a cross-game entrance, proceed normally
        return entrance;
    }

    // Set up pending switch
    Combo::gPendingSwitch.requested = true;
    Combo::gPendingSwitch.targetGame = link->targetGame;
    Combo::gPendingSwitch.targetEntrance = link->targetEntrance;
    Combo::gPendingSwitch.returnEntrance = link->returnEntrance;
    Combo::gPendingSwitch.readyToSwitch = false;

    // Return the original entrance - the game will check Combo_IsCrossGameSwitch()
    // and handle the switch appropriately
    return entrance;
}

bool Combo_IsCrossGameSwitch(void) {
    return Combo::gPendingSwitch.requested;
}

const char* Combo_GetSwitchTargetGameId(void) {
    if (!Combo::gPendingSwitch.requested) {
        return nullptr;
    }
    return Combo_GameToId(Combo::gPendingSwitch.targetGame);
}

uint16_t Combo_GetSwitchTargetEntrance(void) {
    return Combo::gPendingSwitch.targetEntrance;
}

uint16_t Combo_GetSwitchReturnEntrance(void) {
    return Combo::gPendingSwitch.returnEntrance;
}

void Combo_SignalReadyToSwitch(void) {
    Combo::gPendingSwitch.readyToSwitch = true;
}

void Combo_ClearPendingSwitch(void) {
    Combo::gPendingSwitch = Combo::PendingGameSwitch{};
}

} // extern "C"
