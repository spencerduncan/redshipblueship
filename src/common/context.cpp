/**
 * @file context.cpp
 * @brief Game context and state management for single-executable architecture
 *
 * Adapted from combo/src/FrozenState.cpp for the unified build.
 * Manages SaveContext preservation for both games during cross-game switching.
 */

#include "context.h"
#include <cstring>
#include <vector>
#include <algorithm>

// ============================================================================
// Internal state management
// ============================================================================

namespace {

/**
 * State for a single game, preserved at room transition
 */
struct FrozenGameState {
    GameId game = GAME_NONE;
    uint16_t returnEntrance = 0;
    std::vector<uint8_t> saveContext;
    bool hasBeenFrozen = false;

    FrozenGameState() = default;

    FrozenGameState(GameId g, size_t size)
        : game(g)
        , returnEntrance(0)
        , saveContext(size, 0)  // Zero-initialize
        , hasBeenFrozen(false)
    {}
};

/**
 * Manager for frozen state of both games
 */
class FrozenStateManager {
public:
    void Initialize() {
        if (mInitialized) return;

        mOoTState = FrozenGameState(GAME_OOT, OOT_SAVE_CONTEXT_SIZE);
        mMMState = FrozenGameState(GAME_MM, MM_SAVE_CONTEXT_SIZE);
        mInitialized = true;
    }

    void FreezeState(GameId game, uint16_t returnEntrance,
                     const void* saveContextData, size_t size) {
        if (!mInitialized) Initialize();

        FrozenGameState& state = GetState(game);
        size_t expectedSize = (game == GAME_OOT) ? OOT_SAVE_CONTEXT_SIZE : MM_SAVE_CONTEXT_SIZE;

        if (size != expectedSize) {
            size = std::min(size, expectedSize);
        }

        if (state.saveContext.size() != expectedSize) {
            state.saveContext.resize(expectedSize, 0);
        }

        std::memcpy(state.saveContext.data(), saveContextData, size);
        state.returnEntrance = returnEntrance;
        state.hasBeenFrozen = true;
    }

    bool RestoreState(GameId game, void* saveContextData, size_t size) {
        if (!mInitialized) Initialize();

        const FrozenGameState& state = GetState(game);

        if (!state.hasBeenFrozen) {
            return false;
        }

        size_t expectedSize = (game == GAME_OOT) ? OOT_SAVE_CONTEXT_SIZE : MM_SAVE_CONTEXT_SIZE;
        if (size != expectedSize) {
            size = std::min(size, expectedSize);
        }

        std::memcpy(saveContextData, state.saveContext.data(), size);
        return true;
    }

    bool HasFrozenState(GameId game) const {
        if (!mInitialized) return false;
        return GetState(game).hasBeenFrozen;
    }

    uint16_t GetReturnEntrance(GameId game) const {
        if (!mInitialized) return 0;
        return GetState(game).returnEntrance;
    }

    void ClearFrozenState(GameId game) {
        if (!mInitialized) return;

        FrozenGameState& state = GetState(game);
        state.hasBeenFrozen = false;
        state.returnEntrance = 0;
        std::memset(state.saveContext.data(), 0, state.saveContext.size());
    }

    void ClearAll() {
        ClearFrozenState(GAME_OOT);
        ClearFrozenState(GAME_MM);
    }

    const void* GetOoTSaveContext() const {
        if (!mInitialized || mOoTState.saveContext.empty()) {
            return nullptr;
        }
        return mOoTState.saveContext.data();
    }

    const void* GetMMSaveContext() const {
        if (!mInitialized || mMMState.saveContext.empty()) {
            return nullptr;
        }
        return mMMState.saveContext.data();
    }

    void UpdateShadowCopy(GameId game, const void* saveContextData, size_t size) {
        if (!mInitialized) Initialize();

        FrozenGameState& state = GetState(game);
        size_t expectedSize = (game == GAME_OOT) ? OOT_SAVE_CONTEXT_SIZE : MM_SAVE_CONTEXT_SIZE;

        if (size != expectedSize) {
            size = std::min(size, expectedSize);
        }

        if (state.saveContext.size() != expectedSize) {
            state.saveContext.resize(expectedSize, 0);
        }

        std::memcpy(state.saveContext.data(), saveContextData, size);
    }

private:
    FrozenGameState& GetState(GameId game) {
        return (game == GAME_OOT) ? mOoTState : mMMState;
    }

    const FrozenGameState& GetState(GameId game) const {
        return (game == GAME_OOT) ? mOoTState : mMMState;
    }

    FrozenGameState mOoTState;
    FrozenGameState mMMState;
    bool mInitialized = false;
};

// Global instance
FrozenStateManager gFrozenStates;

} // anonymous namespace

// ============================================================================
// ComboContext implementation (from rsbs/src/combo_context.c)
// ============================================================================

#define COMBO_CONTEXT_VERSION 1

ComboContext gComboCtx;
GameId gCurrentGame = GAME_NONE;

extern "C" {

void ComboContext_Init(void) {
    memset(&gComboCtx, 0, sizeof(ComboContext));
    memcpy(gComboCtx.magic, COMBO_CONTEXT_MAGIC, 8);
    gComboCtx.version = COMBO_CONTEXT_VERSION;
    gComboCtx.switchRequested = false;
    gComboCtx.targetGame = GAME_NONE;
    gComboCtx.targetEntrance = 0;
    gComboCtx.sourceGame = GAME_NONE;
    gComboCtx.sourceEntrance = 0;
    gComboCtx.saveSlot = -1;
}

void ComboContext_RequestSwitch(GameId target, uint16_t entrance) {
    gComboCtx.switchRequested = true;
    gComboCtx.targetGame = target;
    gComboCtx.targetEntrance = entrance;
}

bool ComboContext_IsSwitchPending(void) {
    return gComboCtx.switchRequested;
}

void ComboContext_ClearSwitch(void) {
    gComboCtx.switchRequested = false;
    gComboCtx.targetGame = GAME_NONE;
    gComboCtx.targetEntrance = 0;
}

// ============================================================================
// High-level context API
// ============================================================================

void Context_Init(void) {
    ComboContext_Init();
    Context_InitFrozenStates();
    gCurrentGame = GAME_NONE;
}

void Context_RequestSwitch(GameId target, uint16_t entrance) {
    gComboCtx.sourceGame = gCurrentGame;
    ComboContext_RequestSwitch(target, entrance);
}

bool Context_HasPendingSwitch(void) {
    return ComboContext_IsSwitchPending();
}

// Note: Context_ProcessSwitch() is implemented in switch.cpp

// ============================================================================
// C API implementation
// ============================================================================

void Context_InitFrozenStates(void) {
    gFrozenStates.Initialize();
}

void Context_FreezeState(GameId game, uint16_t returnEntrance,
                         const void* saveContext, size_t size) {
    gFrozenStates.FreezeState(game, returnEntrance, saveContext, size);
}

int Context_RestoreState(GameId game, void* saveContext, size_t size) {
    return gFrozenStates.RestoreState(game, saveContext, size) ? 1 : 0;
}

int Context_HasFrozenState(GameId game) {
    return gFrozenStates.HasFrozenState(game) ? 1 : 0;
}

uint16_t Context_GetFrozenReturnEntrance(GameId game) {
    return gFrozenStates.GetReturnEntrance(game);
}

void Context_ClearFrozenState(GameId game) {
    gFrozenStates.ClearFrozenState(game);
}

void Context_ClearAllFrozenStates(void) {
    gFrozenStates.ClearAll();
}

const void* Context_GetOoTSaveContext(void) {
    return gFrozenStates.GetOoTSaveContext();
}

const void* Context_GetMMSaveContext(void) {
    return gFrozenStates.GetMMSaveContext();
}

void Context_UpdateShadowCopy(GameId game, const void* saveContext, size_t size) {
    gFrozenStates.UpdateShadowCopy(game, saveContext, size);
}

// ============================================================================
// Legacy Combo_* API compatibility
// ============================================================================

void Combo_FreezeState(const char* gameId, uint16_t returnEntrance,
                       const void* saveContext, size_t size) {
    GameId game = Game_FromString(gameId);
    if (game == GAME_NONE) return;
    Context_FreezeState(game, returnEntrance, saveContext, size);
}

int Combo_RestoreState(const char* gameId, void* saveContext, size_t size) {
    GameId game = Game_FromString(gameId);
    if (game == GAME_NONE) return 0;
    return Context_RestoreState(game, saveContext, size);
}

int Combo_HasFrozenState(const char* gameId) {
    GameId game = Game_FromString(gameId);
    if (game == GAME_NONE) return 0;
    return Context_HasFrozenState(game);
}

uint16_t Combo_GetFrozenReturnEntrance(const char* gameId) {
    GameId game = Game_FromString(gameId);
    if (game == GAME_NONE) return 0;
    return Context_GetFrozenReturnEntrance(game);
}

void Combo_ClearFrozenState(const char* gameId) {
    GameId game = Game_FromString(gameId);
    if (game == GAME_NONE) return;
    Context_ClearFrozenState(game);
}

void Combo_UpdateShadowCopy(const char* gameId, const void* saveContext, size_t size) {
    GameId game = Game_FromString(gameId);
    if (game == GAME_NONE) return;
    Context_UpdateShadowCopy(game, saveContext, size);
}

} // extern "C"
