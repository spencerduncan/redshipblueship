#pragma once

#include "combo/Export.h"

/**
 * Frozen State Manager
 *
 * Manages game state preservation for cross-game switching at room transitions.
 * Also provides read-only access to both games' states for tracking systems.
 *
 * Design:
 * - Both SaveContexts always in memory (N64 games fit in cache: ~5KB + ~18KB)
 * - Zero-padded on startup so trackers always have valid memory to read
 * - HasFrozenState() distinguishes first-switch (fresh init) from restore
 * - FreezeState() called before switch, RestoreState() after
 */

#include "combo/CrossGameEntrance.h"
#include <cstdint>
#include <cstddef>
#include <vector>

namespace Combo {

// SaveContext sizes from the game headers
// OoT: z64save.h line 354: size = 0x1428
// MM:  z64save.h line 527: size = 0x48C8
constexpr size_t OOT_SAVE_CONTEXT_SIZE = 0x1428;  // 5160 bytes
constexpr size_t MM_SAVE_CONTEXT_SIZE = 0x48C8;   // 18632 bytes

/**
 * State for a single game, preserved at room transition
 */
struct COMBO_API FrozenGameState {
    Game game = Game::None;
    uint16_t returnEntrance = 0;     // Where to spawn when returning
    std::vector<uint8_t> saveContext; // Serialized SaveContext
    bool hasBeenFrozen = false;      // Has this game ever been frozen?

    FrozenGameState() = default;
    explicit FrozenGameState(Game g, size_t size);
};

/**
 * Manages frozen state for both games
 *
 * Thread safety: Not thread-safe. Designed for main game thread.
 */
class COMBO_API FrozenStateManager {
public:
    FrozenStateManager();

    /**
     * Initialize state storage for both games
     * Zero-pads the SaveContext buffers so trackers have valid memory
     */
    void Initialize();

    /**
     * Freeze current game state before switching
     * @param game Which game to freeze
     * @param returnEntrance Where to spawn when returning
     * @param saveContextData Pointer to the game's SaveContext
     * @param size Size of the SaveContext (should match expected size)
     */
    void FreezeState(Game game, uint16_t returnEntrance,
                     const void* saveContextData, size_t size);

    /**
     * Restore frozen state when returning to a game
     * @param game Which game to restore
     * @param saveContextData Pointer to the game's SaveContext (will be written to)
     * @param size Size of the SaveContext buffer
     * @return true if state was restored, false if no frozen state exists
     */
    bool RestoreState(Game game, void* saveContextData, size_t size);

    /**
     * Check if a game has been frozen at least once
     * Returns false on first switch (game should init fresh at entrance)
     */
    bool HasFrozenState(Game game) const;

    /**
     * Get the return entrance for a frozen game
     * @return The entrance, or 0 if no frozen state
     */
    uint16_t GetReturnEntrance(Game game) const;

    /**
     * Clear frozen state for a game (e.g., on game over)
     */
    void ClearFrozenState(Game game);

    /**
     * Clear all frozen state
     */
    void ClearAll();

    // ========================================================================
    // Read-only access for trackers
    // These are always valid (zero-padded initially, updated on freeze)
    // ========================================================================

    /**
     * Get read-only pointer to OoT SaveContext
     * Always valid (zero-padded if never frozen)
     */
    const void* GetOoTSaveContext() const;

    /**
     * Get read-only pointer to MM SaveContext
     * Always valid (zero-padded if never frozen)
     */
    const void* GetMMSaveContext() const;

    /**
     * Get the size of OoT SaveContext
     */
    size_t GetOoTSaveContextSize() const { return OOT_SAVE_CONTEXT_SIZE; }

    /**
     * Get the size of MM SaveContext
     */
    size_t GetMMSaveContextSize() const { return MM_SAVE_CONTEXT_SIZE; }

    /**
     * Update the shadow copy of the active game's SaveContext
     * Called periodically to keep tracker data fresh
     */
    void UpdateShadowCopy(Game game, const void* saveContextData, size_t size);

private:
    FrozenGameState& GetState(Game game);
    const FrozenGameState& GetState(Game game) const;

    FrozenGameState mOoTState;
    FrozenGameState mMMState;
    bool mInitialized = false;
};

// Global instance
extern COMBO_API FrozenStateManager gFrozenStates;

} // namespace Combo

// ============================================================================
// C API for games to call
// ============================================================================

extern "C" {

/**
 * Initialize the frozen state manager
 * Should be called once at combo startup
 */
COMBO_API void Combo_InitFrozenStates(void);

/**
 * Freeze current game state before switching
 * @param gameId "oot" or "mm"
 * @param returnEntrance Where to spawn when returning
 * @param saveContext Pointer to the game's SaveContext
 * @param size Size of the SaveContext
 */
COMBO_API void Combo_FreezeState(const char* gameId, uint16_t returnEntrance,
                                  const void* saveContext, size_t size);

/**
 * Restore frozen state when returning to a game
 * @param gameId "oot" or "mm"
 * @param saveContext Pointer to the game's SaveContext (will be written to)
 * @param size Size of the SaveContext buffer
 * @return 1 if state was restored, 0 if no frozen state (first switch)
 */
COMBO_API int Combo_RestoreState(const char* gameId, void* saveContext, size_t size);

/**
 * Check if a game has been frozen at least once
 * @return 1 if frozen state exists, 0 if first switch
 */
COMBO_API int Combo_HasFrozenState(const char* gameId);

/**
 * Get return entrance for a frozen game
 */
COMBO_API uint16_t Combo_GetFrozenReturnEntrance(const char* gameId);

/**
 * Clear frozen state for a game
 */
COMBO_API void Combo_ClearFrozenState(const char* gameId);

/**
 * Get read-only pointer to OoT SaveContext (for trackers)
 */
COMBO_API const void* Combo_GetOoTSaveContext(void);

/**
 * Get read-only pointer to MM SaveContext (for trackers)
 */
COMBO_API const void* Combo_GetMMSaveContext(void);

/**
 * Update shadow copy of active game's SaveContext (for trackers)
 */
COMBO_API void Combo_UpdateShadowCopy(const char* gameId, const void* saveContext, size_t size);

} // extern "C"
