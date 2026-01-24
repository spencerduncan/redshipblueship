#pragma once

/**
 * FrozenState.h - Compatibility header for legacy combo/ code
 *
 * This header provides the C++ classes expected by existing code
 * while the actual implementation has moved to src/common/context.h.
 *
 * For new code, prefer using src/common/context.h directly.
 */

#include "combo/Export.h"
#include "combo/Game.h"

// Include the new C API
// Use relative path since games may not have src/common in include path
extern "C" {
#include "../../../src/common/context.h"
}

#include <cstdint>
#include <cstring>
#include <map>
#include <vector>
#include <optional>

namespace Combo {

/**
 * Represents a frozen game state
 * This is a C++ wrapper around the C API in context.h
 */
struct COMBO_API FrozenGameState {
    std::vector<uint8_t> saveContextData;
    uint16_t returnEntrance;

    FrozenGameState() : returnEntrance(0) {}
    FrozenGameState(const void* data, size_t size, uint16_t entrance)
        : saveContextData(static_cast<const uint8_t*>(data),
                          static_cast<const uint8_t*>(data) + size),
          returnEntrance(entrance) {}
};

/**
 * Manages frozen states for all games
 * This is a C++ wrapper around the C API in context.h
 */
class COMBO_API FrozenStateManager {
public:
    FrozenStateManager() {
        Context_InitFrozenStates();
    }

    void FreezeState(Game game, uint16_t returnEntrance,
                     const void* saveContext, size_t size) {
        Context_FreezeState(ToGameId(game), returnEntrance, saveContext, size);
    }

    bool RestoreState(Game game, void* saveContext, size_t size) {
        return Context_RestoreState(ToGameId(game), saveContext, size) != 0;
    }

    bool HasFrozenState(Game game) const {
        return Context_HasFrozenState(ToGameId(game)) != 0;
    }

    std::optional<uint16_t> GetReturnEntrance(Game game) const {
        if (!HasFrozenState(game)) {
            return std::nullopt;
        }
        return Context_GetFrozenReturnEntrance(ToGameId(game));
    }

    void ClearState(Game game) {
        Context_ClearFrozenState(ToGameId(game));
    }

    void ClearAll() {
        Context_ClearAllFrozenStates();
    }

    // Read-only access for trackers
    const void* GetOoTSaveContext() const {
        return Context_GetOoTSaveContext();
    }

    const void* GetMMSaveContext() const {
        return Context_GetMMSaveContext();
    }

    void UpdateShadowCopy(Game game, const void* saveContext, size_t size) {
        Context_UpdateShadowCopy(ToGameId(game), saveContext, size);
    }
};

// Global frozen state manager
inline FrozenStateManager& GetFrozenStateManager() {
    static FrozenStateManager manager;
    return manager;
}

} // namespace Combo
