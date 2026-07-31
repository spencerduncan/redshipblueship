/**
 * @file context.cpp
 * @brief Game context and state management for single-executable architecture
 *
 * Adapted from combo/src/FrozenState.cpp for the unified build.
 * Manages SaveContext preservation for both games during cross-game switching.
 */

#include "context.h"
// Context_InvalidateSessionState also has to drain the RAM-only shared-item
// staging outbox, which lives in shared_items.c. Included here (a .cpp) rather
// than from context.h so the header keeps its no-dependency shape.
#include "shared_items.h"
#include "shared_resources.h"
// Combo_CountForeignPlacementsOoT() — invalidation reports how much of the
// generation-authored reverse table each policy kept (#534). Same .cpp-only
// placement rationale as shared_items.h above.
#include "foreign_items.h"
// Combo_HasStartupEntrance() — the discriminator that keeps the return-to-title
// hook from eating a cross-game arrival's blob.
#include "entrance.h"

// The unified save's session-scoped active slot. Declared rather than pulled in
// via save.h so the context layer keeps no compile-time dependency on the save
// layer — save.h already depends on context.h, and the reverse include would
// make that circular. Same convention game-side TUs use to reach Combo_*.
extern "C" void RsbsSave_SetActiveSlot(int slot);

#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include <stdexcept>

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

    // The shadow buffers are sized at the *_SAVE_CONTEXT_SIZE capacities from
    // game.h, which are asserted (in each game's GameExports_SingleExe.cpp) to
    // be >= that game's real sizeof(SaveContext). Callers pass
    // sizeof(gSaveContext) as seen by their own TU, so the min() below only
    // ever trims a caller that claims MORE than the capacity — it must never
    // trim real save data (that was the pre-fix truncation bug: the OoT
    // capacity was the N64 0x1428, and every freeze dropped SoH's ship.*
    // state). The tail beyond `size` is zeroed so the blob is deterministic
    // for serialization/CRC regardless of what was frozen before.
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
        std::memset(state.saveContext.data() + size, 0, expectedSize - size);
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

        // Unlike FreezeState, do NOT zero the tail: callers legitimately push
        // partial updates (MM's SaveManager mirrors just the Save substruct at
        // offset 0), relying on the rest of the shadow keeping the last full
        // snapshot's bytes.
        std::memcpy(state.saveContext.data(), saveContextData, size);
    }

    // Arm the shadow bytes that are ALREADY resident as a frozen blob, without
    // recopying them. This is the missing inverse of the freeze machinery: a
    // .redsave Load commits through UpdateShadowCopy, which deliberately does
    // not set hasBeenFrozen, and every consumer that can move a blob into a
    // live gSaveContext gates on that flag (RestoreState above,
    // Combo_ConsumeFrozenState, MM_Game_Resume). Before this there was no way
    // to arm an existing shadow at all, so a faithfully loaded save sat in
    // memory byte-exact and structurally unreachable.
    //
    // REFUSES an all-zero blob, and that refusal is load-bearing rather than
    // defensive: a slot saved before the player ever entered MM has an all-zero
    // Tier-3, and arming that would restore a zeroed SaveContext over the
    // bootstrap file MM's title chain authors — strictly worse than the cold
    // boot it would replace. "No bytes" and "bytes that happen to be zero" are
    // indistinguishable here by construction, so zero means absent.
    //
    // Does NOT invent a returnEntrance beyond what the caller supplies; where a
    // resumed game actually spawns is that game's own spawn policy, not a
    // property of the blob's arming.
    bool ArmShadowAsFrozen(GameId game, uint16_t returnEntrance) {
        if (!mInitialized) Initialize();

        FrozenGameState& state = GetState(game);
        if (state.saveContext.empty()) {
            return false;
        }

        bool anyNonZero = false;
        for (uint8_t b : state.saveContext) {
            if (b != 0) {
                anyNonZero = true;
                break;
            }
        }
        if (!anyNonZero) {
            return false;
        }

        state.returnEntrance = returnEntrance;
        state.hasBeenFrozen = true;
        return true;
    }

private:
    FrozenGameState& GetState(GameId game) {
        if (game == GAME_OOT) {
            return mOoTState;
        } else if (game == GAME_MM) {
            return mMMState;
        }
        throw std::invalid_argument("GetState called with invalid game");
    }

    const FrozenGameState& GetState(GameId game) const {
        if (game == GAME_OOT) {
            return mOoTState;
        } else if (game == GAME_MM) {
            return mMMState;
        }
        throw std::invalid_argument("GetState called with invalid game");
    }

    FrozenGameState mOoTState;
    FrozenGameState mMMState;
    bool mInitialized = false;
};

// Global instance
FrozenStateManager gFrozenStates;

} // anonymous namespace

// ============================================================================
// ComboContext implementation
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
    gComboCtx.saveSlot = -1;  // RETIRED IN PLACE (ADR 0002) — the -1 stamp is shipped behavior, kept as-is
    gComboCtx.sourceIsRando = false;
    gComboCtx.sharedRandoSeed = 0;
    gComboCtx.sharedRandoSettingsHash = 0;  // Lane B (ADR 0002 §3): 0 == no profile recorded
    // sharedItemsTagged, foreignPlacements, grantCursors,
    // sharedItemOverflowCount, and the remaining reserved[] headroom are
    // covered by the memset above: all-zero IS the initialized state (every
    // slot unset, originGame == GAME_NONE, no source has ever delivered).
    // That equivalence is load-bearing — it is what makes a zero-extended
    // legacy .redsave record indistinguishable from a fresh init (ADR 0002
    // growth contract). It is ALSO the invalidation atomicity the sourced-
    // grant model requires (ADR 0005 / #440): one memset retires items and
    // their delivery cursors together, so a dead session can neither replay
    // its grants nor block a fresh session's deliveries.
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

// ----------------------------------------------------------------------------
// Session invalidation (#440). See context.h for the contract and the operator
// repro this exists to close.
// ----------------------------------------------------------------------------

void Context_InvalidateSessionState(ComboSeedStampPolicy seedPolicy) {
    // Snapshot the generation-authored state BEFORE the re-init below wipes
    // it. These fields are the only part of gComboCtx that can legitimately
    // outlive the session, and only in the KEEP case — see ComboSeedStampPolicy.
    const bool savedSourceIsRando = gComboCtx.sourceIsRando;
    const uint32_t savedSeed = gComboCtx.sharedRandoSeed;
    const uint32_t savedSettingsHash = gComboCtx.sharedRandoSettingsHash;
    // #534: the reverse placement table (OoT checks hosting MM items, #524)
    // travels WITH the stamp because it has the same author and the same
    // moment of authorship: Playthrough_Init stamps the pairing identity and
    // immediately derives these placements from it (OoT_PlaceForeignItems),
    // both BEFORE the file being created exists. Nothing re-places the
    // reverse table after generation — unlike the FORWARD table
    // (foreignPlacements), which is deliberately NOT snapshotted because at
    // this point it can only hold a DEAD session's rows, so keeping it would
    // resurrect them. That is the whole reason. "MM re-authors it at its own
    // OnFileCreate on the next arrival" used to be given alongside it and is
    // no longer a reason for anything: it describes the deferred-generation
    // model #564 retires, in which arrival authors the MM half. Under
    // one-game semantics arrival becomes hydrate-or-refuse, and when the
    // creation event authors the forward table too it joins the KEEP set by
    // the rule in ComboSeedStampPolicy (context.h) rather than by an
    // exception written here. Wiping the reverse table here made #524 inert
    // in every real playthrough: the new file's first .redsave write then
    // persisted the zeroed table.
    ComboForeignPlacement savedPlacementsOoT[RSBS_FOREIGN_PLACEMENT_CAP];
    memcpy(savedPlacementsOoT, gComboCtx.foreignPlacementsOoT, sizeof(savedPlacementsOoT));

    // Frozen blobs and shadow copies in one call — they are the same storage
    // (FrozenStateManager::ClearFrozenState memsets the buffer AND clears
    // hasBeenFrozen). This is the half that fixes the operator's symptom: the
    // dead session's MM blob no longer survives to be consumed by the next
    // session's first Happy Mask Shop entry.
    Context_ClearAllFrozenStates();

    // Pickups staged but never committed when the session died. Committed ones
    // live in sharedItemsTagged and go with the ComboContext_Init below; this
    // outbox is RAM-only and would otherwise drain into the NEXT session's
    // array at its first suspend.
    Combo_ClearSharedItemOutbox();

    // Same reasoning for the shared-resource watermarks (#525): RAM-only, they
    // describe how much of the dead session's pool was materialized in its live
    // save. Carrying one into a fresh session would make the next harvest
    // measure a delta against a balance that no longer exists.
    Combo_ResetSharedResourceWatermarks();

    // Every remaining field of gComboCtx is session state, so the initializer
    // IS the invalidator — there is no per-field clear to drift out of sync
    // with the struct. Fields carved from reserved[] in the future are covered
    // automatically, which is the growth contract's "zero means unset" rule
    // doing exactly what it was written for. Re-stamps magic/version too.
    ComboContext_Init();

    if (seedPolicy == RSBS_SEED_STAMP_KEEP) {
        gComboCtx.sourceIsRando = savedSourceIsRando;
        gComboCtx.sharedRandoSeed = savedSeed;
        gComboCtx.sharedRandoSettingsHash = savedSettingsHash;
        // The other generation-authored artifact (#534) — see the snapshot
        // note above. KEEP-only on purpose: on a DROP path a populated table
        // belongs to a dead session and must go with it.
        memcpy(gComboCtx.foreignPlacementsOoT, savedPlacementsOoT, sizeof(gComboCtx.foreignPlacementsOoT));
    }

    // The unified save's ACTIVE SLOT is session state too, and it lived outside
    // this function's reach: SaveManager owns it, three sites set it, and
    // nothing ever cleared it. "Which .redsave am I part of" therefore outlived
    // the session it described — load slot 0, quit to title, F10 into a fresh
    // bootstrap MM session, owl-save, and the capture wrote MM's throwaway
    // state straight over redship_slot0.redsave, destroying that slot's MM
    // progress and its rando pairing identity.
    //
    // Declared here rather than by including save.h: this keeps the context
    // layer free of a compile-time dependency on the save layer (save.h already
    // depends on context.h), matching how game-side TUs reach Combo_*.
    // Re-establishing a slot is the job of whatever opens one next — OoT's
    // OnLoadFile / OnSaveFile hooks, or the departure publish.
    RsbsSave_SetActiveSlot(-1);

    fprintf(stderr,
            "[Context] Session state invalidated (seed stamp %s: rando=%d seed=%u settings=%08X "
            "reversePlacements=%d)\n",
            (seedPolicy == RSBS_SEED_STAMP_KEEP) ? "kept" : "dropped", (int)gComboCtx.sourceIsRando,
            (unsigned)gComboCtx.sharedRandoSeed, (unsigned)gComboCtx.sharedRandoSettingsHash,
            Combo_CountForeignPlacementsOoT());
}

int Context_InvalidateSessionOnReturnToTitle(void) {
    // The arrival guard. See the header: a cross-game arrival walks the SAME
    // title chain, so without this the hook would eat the blob that arrival is
    // about to consume. Untagged (not ...ForGame) on purpose — any pending
    // cross-game entrance, for either game, means a switch is in flight and
    // this pass through the title is boot chain, not a player-visible reset.
    if (Combo_HasStartupEntrance()) {
        fprintf(stderr, "[Context] Return-to-title invalidation skipped: cross-game arrival in flight\n");
        return 0;
    }
    fprintf(stderr, "[Context] Return to title: retiring cross-game session state\n");
    Context_InvalidateSessionState(RSBS_SEED_STAMP_DROP);
    return 1;
}

void Context_InvalidateSessionOnNewGame(int isRandoFile) {
    fprintf(stderr, "[Context] New file (rando=%d): retiring cross-game session state\n", isRandoFile);
    // A rando file's stamp was authored by generation minutes ago and belongs
    // to the file being created, not to the session being discarded.
    Context_InvalidateSessionState(isRandoFile ? RSBS_SEED_STAMP_KEEP : RSBS_SEED_STAMP_DROP);
}

void Context_InvalidateSessionOnSlotLoad(void) {
    fprintf(stderr, "[Context] Slot load: clearing before reload\n");
    // Always DROP: whatever this slot legitimately owns arrives from its own
    // .redsave immediately after. A slot with no .redsave must read as "no
    // cross-game state", not as "whatever the last session happened to leave".
    Context_InvalidateSessionState(RSBS_SEED_STAMP_DROP);
}

void Context_SetCurrentGame(GameId game) {
    gCurrentGame = game;
    fprintf(stderr, "[Context] Current game set to %s\n", Game_ToString(game));
}

GameId Context_GetCurrentGame(void) {
    return gCurrentGame;
}

// Note: the legacy Context_ProcessSwitch() / Context_IsSwitchInProgress() API
// is gone entirely. switch.cpp removed the implementations (zero callers, and
// they referenced OoT_/MM_FreezeState symbols from TUs excluded from the
// single-exe link), and ADR 0002 removed the dangling declarations from
// context.h. switch.cpp itself IS part of the single-exe build — it holds the
// live hot-swap freeze/consume policy (Switch_PrepareHotSwap /
// Combo_ConsumeFrozenState).

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

int Context_ArmShadowAsFrozen(GameId game, uint16_t returnEntrance) {
    if (game != GAME_OOT && game != GAME_MM) {
        return 0;
    }
    return gFrozenStates.ArmShadowAsFrozen(game, returnEntrance) ? 1 : 0;
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
