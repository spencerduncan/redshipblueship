/**
 * @file entrance.cpp
 * @brief Cross-game entrance infrastructure for single-executable architecture
 *
 * Adapted from combo/src/CrossGameEntrance.cpp for the unified build.
 * Manages entrance links between OoT and MM for seamless game transitions.
 */

#include "entrance.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

// ============================================================================
// Internal state
// ============================================================================

namespace {

// Entrance link table
std::vector<CrossGameEntranceLink> gEntranceLinks;

// Startup entrance for cross-game switch. Entrance 0x0000 is a real entrance
// id (Kokiri Forest from Deku Tree), so we track presence with a separate flag
// rather than overloading 0 as "unset".
uint16_t sStartupEntrance = 0;
bool sStartupEntrancePresent = false;
// Which game the startup entrance targets. GAME_NONE = wildcard (the legacy
// 1-arg setter), so only the matching game consumes a tagged value. This is
// what stops an MM entrance (e.g. 0xC010) from leaking into OoT's linear
// gEntranceTable index and reading far out of bounds (crash 0xC0000005).
GameId sStartupEntranceGame = GAME_NONE;

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
    sStartupEntrancePresent = false;
    sStartupEntranceGame = GAME_NONE;
    sGameSwitchRequested = false;
}

bool Entrance_RegisterDefaultLinks(void) {
    // PRODUCTION: the mask-shop door and the Clock Tower door are the two
    // faces of one portal (see entrance.h):
    //   OoT enters Happy Mask Shop (0x0530)   -> MM spawns in South Clock
    //     Town at the tower exit (0xD800), as if walking out of the tower.
    //   MM enters the Clock Tower from SCT (0xC010) -> OoT spawns outside
    //     the mask shop (0x01D1).
    // Argument roles: entrance2 = OoT->MM arrival, return2 = MM->OoT trigger.
    // The arrival (0xD800) is deliberately NOT a trigger — MM's own cycle
    // resets target it (Song of Time, save-warp, title attract demo).
    return Entrance_RegisterBidirectionalLink(
        GAME_OOT, OOT_ENTR_HAPPY_MASK_SHOP, OOT_ENTR_MARKET_FROM_MASK_SHOP,
        GAME_MM, MM_ENTR_SOUTH_CLOCK_TOWN_0, MM_ENTR_CLOCK_TOWER_INTERIOR_1
    );
}

bool Entrance_RegisterTestLinks(void) {
    // TEST MODE: Mido's House <-> the same MM portal (closer to spawn for
    // quick testing). Same arrival/trigger split as the production link.
    //
    // NOTE: this reuses the SAME MM-side trigger (0xC010) as the production
    // link, so the two are mutually exclusive — see
    // Entrance_RegisterPortalLinks. Registering both is now rejected rather
    // than silently shadowed (#374).
    return Entrance_RegisterBidirectionalLink(
        GAME_OOT, OOT_ENTR_MIDOS_HOUSE, OOT_ENTR_KOKIRI_FROM_MIDOS,
        GAME_MM, MM_ENTR_SOUTH_CLOCK_TOWN_0, MM_ENTR_CLOCK_TOWER_INTERIOR_1
    );
}

bool Entrance_RegisterPortalLinks(bool useTestPortal) {
    // There is exactly ONE cross-game portal, and its MM face is the Clock
    // Tower door (0xC010). Its OoT face is selectable: the Happy Mask Shop
    // door in production, or Mido's House when --test-entrance is passed
    // (closer to the OoT spawn, so a switch can be exercised in seconds).
    //
    // These are alternatives, never additions. Both OoT doors want to own the
    // single MM-side return leg, and only one link can — which is exactly the
    // bug in #374, where main.cpp registered both unconditionally and the
    // default won the first-match lookup, so entering MM from Mido's House
    // spat you out in Hyrule Market instead of Kokiri Forest.
    //
    // Giving the test link its own MM entrance is not an option: every other
    // MM id is a real door somewhere else in Termina, so a "distinct" test
    // trigger would either hijack an unrelated MM transition or index out of
    // MM's entrance table. Selecting one OoT face is the only honest model.
    return useTestPortal ? Entrance_RegisterTestLinks() : Entrance_RegisterDefaultLinks();
}

bool Entrance_HasLinkFor(GameId game, uint16_t entrance) {
    return std::any_of(gEntranceLinks.begin(), gEntranceLinks.end(),
        [game, entrance](const CrossGameEntranceLink& link) {
            return link.sourceGame == game && link.sourceEntrance == entrance;
        });
}

bool Entrance_RegisterBidirectionalLink(
    GameId game1, uint16_t entrance1, uint16_t return1,
    GameId game2, uint16_t entrance2, uint16_t return2
) {
    // Entrance_CheckCrossGame resolves by FIRST match on
    // (sourceGame, sourceEntrance), so a second link on an already-claimed key
    // is not an override — it is dead weight that silently changes nothing,
    // while the caller believes it registered a route. #374 is precisely that:
    // the default and test links both claimed MM 0xC010, the default won, and
    // the test link's return leg mis-routed with no diagnostic anywhere.
    //
    // Reject the whole call ATOMICALLY. Validating both legs before pushing
    // either matters: a half-registered link (forward accepted, reverse
    // rejected) is a one-way portal — you switch games and can never come
    // back, which is strictly worse than the collision it replaced.
    struct {
        GameId game;
        uint16_t entrance;
        const char* leg;
    } const legs[] = {
        { game1, entrance1, "forward" },
        { game2, return2, "reverse" },
    };

    for (const auto& leg : legs) {
        if (Entrance_HasLinkFor(leg.game, leg.entrance)) {
            fprintf(stderr,
                "[ENTRANCE] REJECTED duplicate registration: the %s leg's source "
                "(%s entrance 0x%04X) is already claimed by an existing link. "
                "Nothing was registered. Two links cannot share a source door — "
                "the lookup resolves first-match, so the later one would be dead "
                "(issue #374).\n",
                leg.leg, Game_ToString(leg.game), leg.entrance);
            return false;
        }
    }

    // Also reject a self-collision inside this single call: if the forward
    // source and the reverse source are the same key, the reverse leg would be
    // born dead. Neither loop iteration above can see it, since nothing has
    // been pushed yet.
    if (game1 == game2 && entrance1 == return2) {
        fprintf(stderr,
            "[ENTRANCE] REJECTED self-colliding link: forward and reverse legs "
            "share source (%s entrance 0x%04X). Nothing was registered.\n",
            Game_ToString(game1), entrance1);
        return false;
    }

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

    return true;
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
    // Legacy 1-arg form: tag as wildcard so existing (non-production) callers
    // keep their current game-agnostic behavior.
    Entrance_SetStartupEntrance(entrance, GAME_NONE);
}

void Entrance_SetStartupEntrance(uint16_t entrance, GameId targetGame) {
    sStartupEntrance = entrance;
    sStartupEntrancePresent = true;
    sStartupEntranceGame = targetGame;
}

uint16_t Entrance_GetStartupEntrance(void) {
    return sStartupEntrance;
}

bool Entrance_HasStartupEntrance(void) {
    return sStartupEntrancePresent;
}

bool Entrance_HasStartupEntranceForGame(GameId game) {
    return sStartupEntrancePresent &&
           (sStartupEntranceGame == GAME_NONE || sStartupEntranceGame == game);
}

uint16_t Entrance_GetStartupEntranceForGame(GameId game) {
    return Entrance_HasStartupEntranceForGame(game) ? sStartupEntrance : 0;
}

void Entrance_ClearStartupEntrance(void) {
    sStartupEntrance = 0;
    sStartupEntrancePresent = false;
    sStartupEntranceGame = GAME_NONE;
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

bool Combo_HasStartupEntrance(void) {
    return Entrance_HasStartupEntrance();
}

void Combo_ClearStartupEntrance(void) {
    Entrance_ClearStartupEntrance();
}

uint16_t Combo_GetStartupEntranceForGame(const char* gameId) {
    GameId game = Game_FromString(gameId);
    if (game == GAME_NONE) return 0;
    return Entrance_GetStartupEntranceForGame(game);
}

bool Combo_HasStartupEntranceForGame(const char* gameId) {
    GameId game = Game_FromString(gameId);
    if (game == GAME_NONE) return false;
    return Entrance_HasStartupEntranceForGame(game);
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
