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
// Combo context
// ============================================================================

#define COMBO_CONTEXT_MAGIC "OoT+MM<3"

/**
 * FIXED on-disk size of the .redsave Tier-1 (ComboContext) record.
 *
 * The serialized record is decoupled from sizeof(ComboContext) on purpose.
 * save.cpp always writes exactly this many bytes — the live struct followed by
 * zero padding — and Load reads the size the header stored, zero-extending a
 * shorter record. That is the same size-field-driven scheme the OoT/MM tiers
 * already use, and it is what lets ComboContext GROW without invalidating every
 * existing .redsave.
 *
 * The growth contract, which Lane A (origin-tagged sharedItems) depends on:
 *   - New fields are APPENDED (or carved out of `reserved`), never inserted
 *     between existing members. Every already-shipped field must keep its
 *     offset, because a short legacy record is loaded as a byte PREFIX of the
 *     current layout.
 *   - sizeof(ComboContext) must stay within this budget. Exceeding it is a
 *     compile-time error (static_assert below), not a silent format break: the
 *     fix is to raise both this constant and RSBS_SAVE_VERSION together.
 */
#define RSBS_COMBO_CONTEXT_RECORD_SIZE 1024u

typedef struct {
    char magic[8];        // "OoT+MM<3"
    uint32_t version;
    bool switchRequested;
    GameId targetGame;
    uint16_t targetEntrance;
    GameId sourceGame;
    uint16_t sourceEntrance;
    uint32_t sharedFlags[64];
    // NOT WIRED: no non-test reader/writer exists yet. Lane A widens this into
    // an origin-tagged form — OoT's RG_* ids and MM's item ids are unrelated
    // enumerations that must never alias by raw integer.
    uint16_t sharedItems[32];
    // NOT WIRED: dead plumbing. Set to -1 by ComboContext_Init and serialized,
    // but nothing outside the tests reads it — do not assume the active slot
    // index is available here until something actually assigns it.
    int32_t saveSlot;

    // Cross-game rando state propagation
    bool sourceIsRando;        // Source game is in randomizer mode
    uint32_t sharedRandoSeed;  // Shared seed for synchronization

    // Headroom. Shrink this when appending a field so the struct stays inside
    // RSBS_COMBO_CONTEXT_RECORD_SIZE; the on-disk record size does not change
    // either way, so old saves keep loading. Zeroed by ComboContext_Init, which
    // is what makes a zero-extended legacy record indistinguishable from a
    // freshly-initialized one.
    uint8_t reserved[640];
} ComboContext;

// Deliberately `<=`, not `==`: the on-disk record is padded to a fixed size, so
// the struct only has to FIT the budget. An exact-match assert would turn a
// harmless ABI padding difference into a build break for no benefit.
#ifdef __cplusplus
static_assert(sizeof(ComboContext) <= RSBS_COMBO_CONTEXT_RECORD_SIZE,
              "ComboContext outgrew its .redsave Tier-1 record budget; raise "
              "RSBS_COMBO_CONTEXT_RECORD_SIZE and RSBS_SAVE_VERSION together");
#else
_Static_assert(sizeof(ComboContext) <= RSBS_COMBO_CONTEXT_RECORD_SIZE,
               "ComboContext outgrew its .redsave Tier-1 record budget; raise "
               "RSBS_COMBO_CONTEXT_RECORD_SIZE and RSBS_SAVE_VERSION together");
#endif

extern ComboContext gComboCtx;
extern GameId gCurrentGame;

void ComboContext_Init(void);
void ComboContext_RequestSwitch(GameId target, uint16_t entrance);
bool ComboContext_IsSwitchPending(void);
void ComboContext_ClearSwitch(void);

// ============================================================================
// High-level context API (for switch.cpp and external use)
// ============================================================================

/**
 * Initialize all context systems (frozen states + combo context)
 */
void Context_Init(void);

/**
 * Request a game switch to the target game at the specified entrance
 */
void Context_RequestSwitch(GameId target, uint16_t entrance);

/**
 * Check if a game switch has been requested
 */
bool Context_HasPendingSwitch(void);

/**
 * Process the pending game switch (called by main loop)
 * This coordinates freezing current game state and launching target game
 */
void Context_ProcessSwitch(void);

/**
 * Check if a switch is currently being processed
 */
bool Context_IsSwitchInProgress(void);

/**
 * Set the current game (used during initialization)
 */
void Context_SetCurrentGame(GameId game);

/**
 * Get the current game
 */
GameId Context_GetCurrentGame(void);

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
