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
#define MM_ENTR_CLOCK_TOWER_INTERIOR_1     0xC010  // ENTRANCE(CLOCK_TOWER_INTERIOR, 1)
#define MM_ENTR_SOUTH_CLOCK_TOWN_0         0xD800  // ENTRANCE(SOUTH_CLOCK_TOWN, 0)

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
void Combo_ClearStartupEntrance(void);

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
 * - OoT Happy Mask Shop <-> MM Clock Tower Interior
 */
void Entrance_RegisterDefaultLinks(void);

/**
 * Register test links (for easier testing):
 * - OoT Mido's House <-> MM Clock Tower Interior
 */
void Entrance_RegisterTestLinks(void);

/**
 * Register a bidirectional link between game entrances
 */
void Entrance_RegisterBidirectionalLink(
    GameId game1, uint16_t entrance1, uint16_t return1,
    GameId game2, uint16_t entrance2, uint16_t return2
);

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
 * Get the startup entrance (0 if not set)
 */
uint16_t Entrance_GetStartupEntrance(void);

/**
 * Clear the startup entrance (called after game reads it)
 */
void Entrance_ClearStartupEntrance(void);

#endif // __cplusplus

#endif // RSBS_COMMON_ENTRANCE_H
