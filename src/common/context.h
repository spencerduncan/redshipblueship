/**
 * @file context.h
 * @brief Game context and state management for single-executable architecture
 *
 * Manages game state preservation for cross-game switching at room transitions.
 * Also provides read-only access to both games' states for tracking systems.
 *
 * Adapted from combo/src/FrozenState.cpp for single-executable architecture.
 *
 * Design:
 * - Both SaveContexts always in memory (N64 games fit in cache: ~5KB + ~18KB)
 * - Zero-padded on startup so trackers always have valid memory to read
 * - HasFrozenState() distinguishes first-switch (fresh init) from restore
 * - FreezeState() called before switch, RestoreState() after
 */

#ifndef RSBS_COMMON_CONTEXT_H
#define RSBS_COMMON_CONTEXT_H

#include "game.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the frozen state manager
 * Should be called once at startup
 */
void Context_InitFrozenStates(void);

/**
 * Freeze current game state before switching
 * @param game Which game's state to freeze
 * @param returnEntrance Where to spawn when returning
 * @param saveContext Pointer to the game's SaveContext
 * @param size Size of the SaveContext
 */
void Context_FreezeState(GameId game, uint16_t returnEntrance,
                         const void* saveContext, size_t size);

/**
 * Restore frozen state when returning to a game
 * @param game Which game's state to restore
 * @param saveContext Pointer to the game's SaveContext (will be written to)
 * @param size Size of the SaveContext buffer
 * @return 1 if state was restored, 0 if no frozen state (first switch)
 */
int Context_RestoreState(GameId game, void* saveContext, size_t size);

/**
 * Check if a game has been frozen at least once
 * @return 1 if frozen state exists, 0 if first switch
 */
int Context_HasFrozenState(GameId game);

/**
 * Get return entrance for a frozen game
 */
uint16_t Context_GetFrozenReturnEntrance(GameId game);

/**
 * Clear frozen state for a game
 */
void Context_ClearFrozenState(GameId game);

/**
 * Clear all frozen states
 */
void Context_ClearAllFrozenStates(void);

/**
 * Get read-only pointer to OoT SaveContext (for trackers)
 */
const void* Context_GetOoTSaveContext(void);

/**
 * Get read-only pointer to MM SaveContext (for trackers)
 */
const void* Context_GetMMSaveContext(void);

/**
 * Update shadow copy of active game's SaveContext (for trackers)
 */
void Context_UpdateShadowCopy(GameId game, const void* saveContext, size_t size);

// ============================================================================
// Combo context (from rsbs/src/combo_context.c)
// ============================================================================

#define COMBO_CONTEXT_MAGIC "OoT+MM<3"

typedef struct {
    char magic[8];        // "OoT+MM<3"
    uint32_t version;
    bool switchRequested;
    GameId targetGame;
    uint16_t targetEntrance;
    GameId sourceGame;
    uint16_t sourceEntrance;
    uint32_t sharedFlags[64];
    uint16_t sharedItems[32];
    int32_t saveSlot;
} ComboContext;

extern ComboContext gComboCtx;

void ComboContext_Init(void);
void ComboContext_RequestSwitch(GameId target, uint16_t entrance);
bool ComboContext_IsSwitchPending(void);
void ComboContext_ClearSwitch(void);

// ============================================================================
// Legacy C API compatibility (maps to new functions)
// These match the existing combo/ API for easier transition
// ============================================================================

// Combo_* functions for legacy compatibility
#define Combo_InitFrozenStates Context_InitFrozenStates
void Combo_FreezeState(const char* gameId, uint16_t returnEntrance,
                       const void* saveContext, size_t size);
int Combo_RestoreState(const char* gameId, void* saveContext, size_t size);
int Combo_HasFrozenState(const char* gameId);
uint16_t Combo_GetFrozenReturnEntrance(const char* gameId);
void Combo_ClearFrozenState(const char* gameId);
#define Combo_GetOoTSaveContext Context_GetOoTSaveContext
#define Combo_GetMMSaveContext Context_GetMMSaveContext
void Combo_UpdateShadowCopy(const char* gameId, const void* saveContext, size_t size);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_CONTEXT_H
