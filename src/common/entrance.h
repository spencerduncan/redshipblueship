/**
 * @file entrance.h
 * @brief Cross-game entrance infrastructure for single-executable architecture
 *
 * This module provides the infrastructure for tracking cross-game entrance links,
 * allowing seamless transitions between OoT and MM at specific entrance points.
 *
 * Adapted from combo/src/CrossGameEntrance.cpp for single-executable architecture.
 *
 * Design:
 * - Entrance tables store bidirectional links between game entrances
 * - C API (Combo_*) allows game code to query for cross-game transitions
 * - C++ API (Entrance_*) for internal use and testing
 * - Extensible for future entrance shuffling (any entrance -> any entrance)
 */

#ifndef RSBS_COMMON_ENTRANCE_H
#define RSBS_COMMON_ENTRANCE_H

#include "game.h"

// ============================================================================
// Default entrance constants
// ============================================================================

// OoT entrances - Happy Mask Shop (production)
#define OOT_ENTR_HAPPY_MASK_SHOP           0x0530  // Into Happy Mask Shop
#define OOT_ENTR_MARKET_FROM_MASK_SHOP     0x01D1  // Out of Happy Mask Shop

// OoT entrances - Mido's House (testing - closer to spawn)
#define OOT_ENTR_MIDOS_HOUSE               0x0433  // Into Mido's House
#define OOT_ENTR_KOKIRI_FROM_MIDOS         0x0443  // Out of Mido's House

// MM entrances (using ENTRANCE macro: ((scene & 0x7F) << 9) | ((spawn & 0x1F) << 4))
// CLOCK_TOWER_INTERIOR scene = 0x60
// SOUTH_CLOCK_TOWN scene = 0x6C
//
// The portal is the Clock Tower door itself, mirroring OoT's mask-shop door:
// - MM->OoT trigger: walking INTO the tower from South Clock Town sets
//   ENTRANCE(CLOCK_TOWER_INTERIOR, 1) = 0xC010.
// - OoT->MM arrival: spawn in South Clock Town as if you just walked OUT of
//   the tower — ENTRANCE(SOUTH_CLOCK_TOWN, 0) = 0xD800, the same spawn the
//   Song of Time reset / save-warp / cycle start use.
// The trigger and arrival MUST stay distinct ids: 0xD800 is targeted by MM's
// own cycle resets (Song of Time, save-warp, the title attract demo), so it
// can never be a switch trigger — when it was, every in-game reset to South
// Clock Town spuriously fired a cross-game switch.
#define MM_ENTR_CLOCK_TOWER_INTERIOR_1     0xC010  // ENTRANCE(CLOCK_TOWER_INTERIOR, 1) — MM->OoT trigger
#define MM_ENTR_SOUTH_CLOCK_TOWN_0         0xD800  // ENTRANCE(SOUTH_CLOCK_TOWN, 0) — OoT->MM arrival

// ============================================================================
// Cross-game entrance link
// ============================================================================

/**
 * Represents a link between entrances in different games
 */
typedef struct {
    GameId sourceGame;
    uint16_t sourceEntrance;     // Entrance ID in source game
    GameId targetGame;
    uint16_t targetEntrance;     // Where to spawn in target game
    uint16_t returnEntrance;     // Where to return when coming back
} CrossGameEntranceLink;

/**
 * Pending game switch state
 */
typedef struct {
    bool requested;
    GameId targetGame;
    uint16_t targetEntrance;
    uint16_t returnEntrance;     // Where to return in source game
    bool readyToSwitch;          // Set by game after saving state
} PendingGameSwitch;

#ifdef __cplusplus
extern "C" {
#endif

extern PendingGameSwitch gPendingSwitch;

// ============================================================================
// C API - for use by game code (extern "C" linkage)
// These match the existing combo/ API for compatibility
// ============================================================================

uint16_t Combo_CheckCrossGameEntrance(const char* gameId, uint16_t entrance);
bool Combo_IsCrossGameSwitch(void);
const char* Combo_GetSwitchTargetGameId(void);
uint16_t Combo_GetSwitchTargetEntrance(void);
uint16_t Combo_GetSwitchReturnEntrance(void);
void Combo_SignalReadyToSwitch(void);
void Combo_ClearPendingSwitch(void);
void Combo_SetStartupEntrance(uint16_t entrance);
uint16_t Combo_GetStartupEntrance(void);
// Entrance 0x0000 is valid (Kokiri from Deku Tree), so presence is tracked
// separately from the value — callers must check this before trusting the id.
bool Combo_HasStartupEntrance(void);
void Combo_ClearStartupEntrance(void);
// Game-scoped variants: only return/report the startup entrance when it was
// tagged for `gameId` (or tagged as wildcard via the 1-arg setter). This
// prevents a cross-game entrance (e.g. MM 0xC010) from being consumed by the
// wrong game and indexing that game's entrance table out of bounds.
uint16_t Combo_GetStartupEntranceForGame(const char* gameId);
bool Combo_HasStartupEntranceForGame(const char* gameId);

// Game switch request API
void Combo_RequestGameSwitch(void);
bool Combo_IsGameSwitchRequested(void);
void Combo_ClearGameSwitchRequest(void);

#ifdef __cplusplus
}

// ============================================================================
// C++ API - for internal use and testing (C++ linkage, name-mangled)
// These do NOT collide with OoT's Entrance_* functions which have C linkage
// ============================================================================

/**
 * Initialize the entrance table (clears all links)
 */
void Entrance_Init(void);

/**
 * Register the default OoTMM combomizer links:
 * - OoT Happy Mask Shop door <-> MM Clock Tower door
 *   (arrival in MM is the South Clock Town tower-exit spawn; see the portal
 *   note above the MM entrance constants)
 *
 * @return false if a link already claims one of the source doors.
 */
bool Entrance_RegisterDefaultLinks(void);

/**
 * Register test links (for easier testing):
 * - OoT Mido's House <-> the same MM portal (SCT arrival / tower-door trigger)
 *
 * MUTUALLY EXCLUSIVE with Entrance_RegisterDefaultLinks: both claim the same
 * MM-side trigger (0xC010), so registering both is rejected (#374). Prefer
 * Entrance_RegisterPortalLinks, which picks exactly one.
 *
 * @return false if a link already claims one of the source doors.
 */
bool Entrance_RegisterTestLinks(void);

/**
 * Register the one cross-game portal, choosing its OoT-side face.
 *
 * @param useTestPortal true  -> Mido's House  (the --test-entrance face)
 *                      false -> Happy Mask Shop (production)
 * @return false if registration was rejected (a link already claims a door).
 */
bool Entrance_RegisterPortalLinks(bool useTestPortal);

/**
 * Register a bidirectional link between game entrances.
 *
 * Rejects the registration (loudly, on stderr) and returns false if either
 * leg's source (game, entrance) is already claimed by a registered link, or
 * if the two legs of this call collide with each other. Rejection is atomic:
 * on false, NOTHING was added — never a one-way half-link.
 *
 * @return true if both legs were registered.
 */
bool Entrance_RegisterBidirectionalLink(
    GameId game1, uint16_t entrance1, uint16_t return1,
    GameId game2, uint16_t entrance2, uint16_t return2
);

/**
 * Whether some registered link already resolves from (game, entrance).
 */
bool Entrance_HasLinkFor(GameId game, uint16_t entrance);

/**
 * Clear all registered entrance links
 */
void Entrance_ClearLinks(void);

/**
 * Get the number of registered links
 */
size_t Entrance_GetLinkCount(void);

/**
 * Check if an entrance triggers a cross-game switch.
 * If it does, sets up the pending switch state.
 *
 * @param game Current game
 * @param entrance The entrance being taken
 * @return The entrance to use (original if no cross-game)
 */
uint16_t Entrance_CheckCrossGame(GameId game, uint16_t entrance);

/**
 * Check if a cross-game switch is pending
 */
bool Entrance_IsCrossGameSwitch(void);

/**
 * Get the target game for the pending switch
 */
GameId Entrance_GetSwitchTargetGame(void);

/**
 * Get the target entrance for the pending switch
 */
uint16_t Entrance_GetSwitchTargetEntrance(void);

/**
 * Get the return entrance (where to spawn when coming back to current game)
 */
uint16_t Entrance_GetSwitchReturnEntrance(void);

/**
 * Signal that the game has saved its state and is ready to switch
 */
void Entrance_SignalReadyToSwitch(void);

/**
 * Clear the pending switch state (called by launcher after switch completes)
 */
void Entrance_ClearPendingSwitch(void);

/**
 * Set the startup entrance for a game (used on first cross-game switch)
 */
void Entrance_SetStartupEntrance(uint16_t entrance);

/**
 * Set the startup entrance tagged with the game it targets, so only that game
 * consumes it. The 1-arg overload above tags GAME_NONE (wildcard).
 */
void Entrance_SetStartupEntrance(uint16_t entrance, GameId targetGame);

/**
 * Get the startup entrance. Use Entrance_HasStartupEntrance() to check
 * whether one is actually set — entrance 0x0000 is a valid id.
 */
uint16_t Entrance_GetStartupEntrance(void);

/**
 * Whether a startup entrance has been set since the last clear.
 */
bool Entrance_HasStartupEntrance(void);

/**
 * Whether a startup entrance is set and targets `game` (or is a wildcard).
 */
bool Entrance_HasStartupEntranceForGame(GameId game);

/**
 * The startup entrance if it targets `game` (or is a wildcard), else 0.
 */
uint16_t Entrance_GetStartupEntranceForGame(GameId game);

/**
 * Clear the startup entrance (called after game reads it)
 */
void Entrance_ClearStartupEntrance(void);

#endif // __cplusplus

#endif // RSBS_COMMON_ENTRANCE_H
