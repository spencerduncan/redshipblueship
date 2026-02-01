/**
 * @file entrance.cpp
 * @brief Cross-game entrance infrastructure for single-executable architecture
 *
 * Adapted from combo/src/CrossGameEntrance.cpp for the unified build.
 * Manages entrance links between OoT and MM for seamless game transitions.
 */

#include "entrance.h"
#include "test_runner.h"
#include <cstring>
#include <vector>
#include <algorithm>

// ============================================================================
// Internal state
// ============================================================================

namespace {

// Entrance link table
std::vector<CrossGameEntranceLink> gEntranceLinks;

// Startup entrance for cross-game switch (0 = not set)
uint16_t sStartupEntrance = 0;

// Game switch request flag (for F10 hotkey)
bool sGameSwitchRequested = false;

} // anonymous namespace

// Global pending switch state
PendingGameSwitch gPendingSwitch = {};

// ============================================================================
// C++ API implementation (C++ linkage - no collision with OoT's Entrance_*)
// ============================================================================

void Entrance_Init(void) {
    gEntranceLinks.clear();
    gPendingSwitch = {};
    sStartupEntrance = 0;
    sGameSwitchRequested = false;
}

void Entrance_RegisterDefaultLinks(void) {
    // PRODUCTION: OoT Happy Mask Shop <-> MM Clock Tower Interior
    Entrance_RegisterBidirectionalLink(
        GAME_OOT, OOT_ENTR_HAPPY_MASK_SHOP, OOT_ENTR_MARKET_FROM_MASK_SHOP,
        GAME_MM, MM_ENTR_CLOCK_TOWER_INTERIOR_1, MM_ENTR_SOUTH_CLOCK_TOWN_0
    );
}

void Entrance_RegisterTestLinks(void) {
    // TEST MODE: Mido's House <-> Clock Tower (closer to spawn for quick testing)
    Entrance_RegisterBidirectionalLink(
        GAME_OOT, OOT_ENTR_MIDOS_HOUSE, OOT_ENTR_KOKIRI_FROM_MIDOS,
        GAME_MM, MM_ENTR_CLOCK_TOWER_INTERIOR_1, MM_ENTR_SOUTH_CLOCK_TOWN_0
    );
}

void Entrance_RegisterBidirectionalLink(
    GameId game1, uint16_t entrance1, uint16_t return1,
    GameId game2, uint16_t entrance2, uint16_t return2
) {
    // Forward link: game1:entrance1 -> game2:entrance2
    CrossGameEntranceLink forward = {
        .sourceGame = game1,
        .sourceEntrance = entrance1,
        .targetGame = game2,
        .targetEntrance = entrance2,
        .returnEntrance = return1
    };
    gEntranceLinks.push_back(forward);

    // Reverse link: game2:return2 -> game1:return1
    CrossGameEntranceLink reverse = {
        .sourceGame = game2,
        .sourceEntrance = return2,
        .targetGame = game1,
        .targetEntrance = return1,
        .returnEntrance = entrance2
    };
    gEntranceLinks.push_back(reverse);
}

void Entrance_ClearLinks(void) {
    gEntranceLinks.clear();
}

size_t Entrance_GetLinkCount(void) {
    return gEntranceLinks.size();
}

uint16_t Entrance_CheckCrossGame(GameId game, uint16_t entrance) {
    // Search for a matching link
    auto it = std::find_if(gEntranceLinks.begin(), gEntranceLinks.end(),
        [game, entrance](const CrossGameEntranceLink& link) {
            return link.sourceGame == game && link.sourceEntrance == entrance;
        });

    if (it == gEntranceLinks.end()) {
        // Not a cross-game entrance
        return entrance;
    }

    // Set up pending switch
    gPendingSwitch.requested = true;
    gPendingSwitch.targetGame = it->targetGame;
    gPendingSwitch.targetEntrance = it->targetEntrance;
    gPendingSwitch.returnEntrance = it->returnEntrance;
    gPendingSwitch.readyToSwitch = false;

    // Signal test runner if in Xvfb test mode
    TestRunner_SignalEntranceTriggered(it->targetEntrance, it->targetGame);

    return entrance;
}

bool Entrance_IsCrossGameSwitch(void) {
    return gPendingSwitch.requested;
}

GameId Entrance_GetSwitchTargetGame(void) {
    return gPendingSwitch.targetGame;
}

uint16_t Entrance_GetSwitchTargetEntrance(void) {
    return gPendingSwitch.targetEntrance;
}

uint16_t Entrance_GetSwitchReturnEntrance(void) {
    return gPendingSwitch.returnEntrance;
}

void Entrance_SignalReadyToSwitch(void) {
    gPendingSwitch.readyToSwitch = true;
}

void Entrance_ClearPendingSwitch(void) {
    gPendingSwitch = {};
}

void Entrance_SetStartupEntrance(uint16_t entrance) {
    sStartupEntrance = entrance;
}

uint16_t Entrance_GetStartupEntrance(void) {
    return sStartupEntrance;
}

void Entrance_ClearStartupEntrance(void) {
    sStartupEntrance = 0;
}

// ============================================================================
// C API - extern "C" for use by game code
// ============================================================================

extern "C" {

uint16_t Combo_CheckCrossGameEntrance(const char* gameId, uint16_t entrance) {
    GameId game = Game_FromString(gameId);
    if (game == GAME_NONE) return entrance;
    return Entrance_CheckCrossGame(game, entrance);
}

bool Combo_IsCrossGameSwitch(void) {
    return Entrance_IsCrossGameSwitch();
}

const char* Combo_GetSwitchTargetGameId(void) {
    if (!gPendingSwitch.requested) return nullptr;
    return Game_ToString(gPendingSwitch.targetGame);
}

uint16_t Combo_GetSwitchTargetEntrance(void) {
    return Entrance_GetSwitchTargetEntrance();
}

uint16_t Combo_GetSwitchReturnEntrance(void) {
    return Entrance_GetSwitchReturnEntrance();
}

void Combo_SignalReadyToSwitch(void) {
    Entrance_SignalReadyToSwitch();
}

void Combo_ClearPendingSwitch(void) {
    Entrance_ClearPendingSwitch();
}

void Combo_SetStartupEntrance(uint16_t entrance) {
    Entrance_SetStartupEntrance(entrance);
}

uint16_t Combo_GetStartupEntrance(void) {
    return Entrance_GetStartupEntrance();
}

void Combo_ClearStartupEntrance(void) {
    Entrance_ClearStartupEntrance();
}

// ============================================================================
// Game switch request API (for F10 hotkey)
// ============================================================================

void Combo_RequestGameSwitch(void) {
    sGameSwitchRequested = true;
}

bool Combo_IsGameSwitchRequested(void) {
    return sGameSwitchRequested;
}

void Combo_ClearGameSwitchRequest(void) {
    sGameSwitchRequested = false;
}

} // extern "C"
