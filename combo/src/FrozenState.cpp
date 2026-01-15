#include "combo/FrozenState.h"
#include <cstring>
#include <stdexcept>

namespace Combo {

// Global instance
FrozenStateManager gFrozenStates;

// ============================================================================
// FrozenGameState
// ============================================================================

FrozenGameState::FrozenGameState(Game g, size_t size)
    : game(g)
    , returnEntrance(0)
    , saveContext(size, 0)  // Zero-initialize
    , hasBeenFrozen(false)
{
}

// ============================================================================
// FrozenStateManager
// ============================================================================

FrozenStateManager::FrozenStateManager()
    : mOoTState()
    , mMMState()
    , mInitialized(false)
{
}

void FrozenStateManager::Initialize() {
    if (mInitialized) {
        return;
    }

    // Initialize both states with zero-padded SaveContext buffers
    mOoTState = FrozenGameState(Game::OoT, OOT_SAVE_CONTEXT_SIZE);
    mMMState = FrozenGameState(Game::MM, MM_SAVE_CONTEXT_SIZE);
    mInitialized = true;
}

void FrozenStateManager::FreezeState(Game game, uint16_t returnEntrance,
                                      const void* saveContextData, size_t size) {
    if (!mInitialized) {
        Initialize();
    }

    FrozenGameState& state = GetState(game);

    // Validate size matches expected
    size_t expectedSize = (game == Game::OoT) ? OOT_SAVE_CONTEXT_SIZE : MM_SAVE_CONTEXT_SIZE;
    if (size != expectedSize) {
        // Size mismatch - use minimum of both to avoid overflow
        size = std::min(size, expectedSize);
    }

    // Ensure buffer is sized correctly
    if (state.saveContext.size() != expectedSize) {
        state.saveContext.resize(expectedSize, 0);
    }

    // Copy the SaveContext data
    std::memcpy(state.saveContext.data(), saveContextData, size);
    state.returnEntrance = returnEntrance;
    state.hasBeenFrozen = true;
}

bool FrozenStateManager::RestoreState(Game game, void* saveContextData, size_t size) {
    if (!mInitialized) {
        Initialize();
    }

    const FrozenGameState& state = GetState(game);

    // Can only restore if we've frozen at least once
    if (!state.hasBeenFrozen) {
        return false;
    }

    // Validate size
    size_t expectedSize = (game == Game::OoT) ? OOT_SAVE_CONTEXT_SIZE : MM_SAVE_CONTEXT_SIZE;
    if (size != expectedSize) {
        size = std::min(size, expectedSize);
    }

    // Copy the SaveContext data back
    std::memcpy(saveContextData, state.saveContext.data(), size);
    return true;
}

bool FrozenStateManager::HasFrozenState(Game game) const {
    if (!mInitialized) {
        return false;
    }
    return GetState(game).hasBeenFrozen;
}

uint16_t FrozenStateManager::GetReturnEntrance(Game game) const {
    if (!mInitialized) {
        return 0;
    }
    return GetState(game).returnEntrance;
}

void FrozenStateManager::ClearFrozenState(Game game) {
    if (!mInitialized) {
        return;
    }

    FrozenGameState& state = GetState(game);
    state.hasBeenFrozen = false;
    state.returnEntrance = 0;
    // Zero out the SaveContext but keep the buffer allocated
    std::memset(state.saveContext.data(), 0, state.saveContext.size());
}

void FrozenStateManager::ClearAll() {
    ClearFrozenState(Game::OoT);
    ClearFrozenState(Game::MM);
}

const void* FrozenStateManager::GetOoTSaveContext() const {
    if (!mInitialized || mOoTState.saveContext.empty()) {
        return nullptr;
    }
    return mOoTState.saveContext.data();
}

const void* FrozenStateManager::GetMMSaveContext() const {
    if (!mInitialized || mMMState.saveContext.empty()) {
        return nullptr;
    }
    return mMMState.saveContext.data();
}

void FrozenStateManager::UpdateShadowCopy(Game game, const void* saveContextData, size_t size) {
    if (!mInitialized) {
        Initialize();
    }

    FrozenGameState& state = GetState(game);

    size_t expectedSize = (game == Game::OoT) ? OOT_SAVE_CONTEXT_SIZE : MM_SAVE_CONTEXT_SIZE;
    if (size != expectedSize) {
        size = std::min(size, expectedSize);
    }

    if (state.saveContext.size() != expectedSize) {
        state.saveContext.resize(expectedSize, 0);
    }

    // Update the shadow copy (doesn't set hasBeenFrozen - that's for actual freezes)
    std::memcpy(state.saveContext.data(), saveContextData, size);
}

FrozenGameState& FrozenStateManager::GetState(Game game) {
    if (game == Game::OoT) {
        return mOoTState;
    } else if (game == Game::MM) {
        return mMMState;
    }
    // Fallback - shouldn't happen
    return mOoTState;
}

const FrozenGameState& FrozenStateManager::GetState(Game game) const {
    if (game == Game::OoT) {
        return mOoTState;
    } else if (game == Game::MM) {
        return mMMState;
    }
    return mOoTState;
}

} // namespace Combo

// ============================================================================
// C API implementation
// ============================================================================

extern "C" {

void Combo_InitFrozenStates(void) {
    Combo::gFrozenStates.Initialize();
}

void Combo_FreezeState(const char* gameId, uint16_t returnEntrance,
                       const void* saveContext, size_t size) {
    Combo::Game game = Combo_GameFromId(gameId);
    if (game == Combo::Game::None) {
        return;
    }
    Combo::gFrozenStates.FreezeState(game, returnEntrance, saveContext, size);
}

int Combo_RestoreState(const char* gameId, void* saveContext, size_t size) {
    Combo::Game game = Combo_GameFromId(gameId);
    if (game == Combo::Game::None) {
        return 0;
    }
    return Combo::gFrozenStates.RestoreState(game, saveContext, size) ? 1 : 0;
}

int Combo_HasFrozenState(const char* gameId) {
    Combo::Game game = Combo_GameFromId(gameId);
    if (game == Combo::Game::None) {
        return 0;
    }
    return Combo::gFrozenStates.HasFrozenState(game) ? 1 : 0;
}

uint16_t Combo_GetFrozenReturnEntrance(const char* gameId) {
    Combo::Game game = Combo_GameFromId(gameId);
    if (game == Combo::Game::None) {
        return 0;
    }
    return Combo::gFrozenStates.GetReturnEntrance(game);
}

void Combo_ClearFrozenState(const char* gameId) {
    Combo::Game game = Combo_GameFromId(gameId);
    if (game == Combo::Game::None) {
        return;
    }
    Combo::gFrozenStates.ClearFrozenState(game);
}

const void* Combo_GetOoTSaveContext(void) {
    return Combo::gFrozenStates.GetOoTSaveContext();
}

const void* Combo_GetMMSaveContext(void) {
    return Combo::gFrozenStates.GetMMSaveContext();
}

void Combo_UpdateShadowCopy(const char* gameId, const void* saveContext, size_t size) {
    Combo::Game game = Combo_GameFromId(gameId);
    if (game == Combo::Game::None) {
        return;
    }
    Combo::gFrozenStates.UpdateShadowCopy(game, saveContext, size);
}

} // extern "C"
