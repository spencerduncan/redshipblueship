/**
 * @file switch.cpp
 * @brief Freeze/consume policy for cross-game switches (single-exe build).
 *
 * There are two ways to leave a game:
 *
 * 1. **Entrance switch** — the player walks through a linked door. The freeze
 *    happens inside `Combo_CheckEntranceSwitch`
 *    (games/oot/soh/GameExports_SingleExe.cpp), which has the departing game's
 *    live SaveContext and a link-derived return entrance.
 *
 * 2. **Hot swap (F10)** — the player presses the debug hotkey. There is no
 *    entrance link and no game-side hook, so the launcher has to drive the
 *    freeze itself. `Switch_PrepareHotSwap` below is that driver.
 *
 * Both paths end at the same restore point: the target's `Play_Init` consumes
 * the tagged startup entrance and re-applies the frozen save via
 * `Combo_ConsumeFrozenState`, which retires the blob as it hands it over.
 *
 * The freeze and the consume are deliberately symmetric. If a blob is ever
 * created without a matching consume, or consumed without being retired, the
 * next return leg re-applies a stale snapshot and silently rolls the player's
 * progress back (issue #364).
 *
 * NOTE: the legacy `Context_ProcessSwitch` / `OoT_FreezeState` /
 * `MM_FreezeState` / `*_ResumeFromContext` path that used to live here has
 * been removed. It had zero callers, and two of the four symbols it referenced
 * (`MM_FreezeState`, `MM_ResumeFromContext`) live in games/mm/2s2h/BenPort.cpp,
 * which is excluded from the single-exe build — so this translation unit could
 * only stay linkable as long as nothing pulled it in. Its declarations in
 * context.h are now dangling; removing them is a context.h change and belongs
 * to whoever owns that file.
 */

#include "context.h"
#include "entrance.h"
#include <cstdio>
#include <cstddef>

extern "C" {

/**
 * Where a hot-swapped game should spawn when the player comes back to it.
 *
 * An F10 switch has no entrance link to derive a return from, so we use the
 * same arrival spawn the *entrance* portal would have produced for that game.
 * That keeps both departure paths landing the player in the same place, and —
 * more importantly — guarantees an id that is in range for the target's
 * entrance table. A bogus id here reaches `gSaveContext.entranceIndex`, which
 * is a direct linear index into `gEntranceTable`, i.e. an out-of-bounds read
 * (this is the #356 class).
 *
 * @return the return entrance, or 0 for a game with no defined portal — the
 *         caller must treat that as "cannot hot swap", not as entrance 0x0000.
 */
uint16_t Switch_GetHotSwapReturnEntrance(GameId departing) {
    switch (departing) {
        case GAME_OOT:
            return OOT_ENTR_MARKET_FROM_MASK_SHOP;
        case GAME_MM:
            return MM_ENTR_SOUTH_CLOCK_TOWN_0;
        default:
            return 0;
    }
}

/**
 * Freeze the departing game for an F10 hot swap.
 *
 * Before this existed the F10 path froze nothing at all, while
 * rsbs/src/main.cpp still consulted `Context_HasFrozenState(target)` to decide
 * whether to set a startup entrance. Once any entrance switch had populated a
 * blob, every later F10 return re-applied that same ancient snapshot and the
 * player lost everything earned since — silently, because the restore looks
 * exactly like a successful resume (issue #364).
 *
 * @param departing   the game being left (must be a real game)
 * @param saveContext the departing game's live SaveContext storage
 * @param size        bytes available at @p saveContext; Context_FreezeState
 *                    clamps this to the per-game blob capacity
 * @return 1 if a fresh blob is now recorded, 0 if the caller must refuse the
 *         switch rather than proceed into a stale restore
 */
int Switch_PrepareHotSwap(GameId departing, const void* saveContext, size_t size) {
    if (departing != GAME_OOT && departing != GAME_MM) {
        fprintf(stderr, "[Switch] Hot swap refused: no active game to freeze\n");
        return 0;
    }
    if (saveContext == NULL || size == 0) {
        fprintf(stderr, "[Switch] Hot swap refused: %s has no SaveContext to freeze\n", Game_ToString(departing));
        return 0;
    }

    uint16_t returnEntrance = Switch_GetHotSwapReturnEntrance(departing);

    Context_FreezeState(departing, returnEntrance, saveContext, size);

    if (!Context_HasFrozenState(departing)) {
        fprintf(stderr, "[Switch] Hot swap refused: freeze of %s did not take\n", Game_ToString(departing));
        return 0;
    }

    fprintf(stderr, "[Switch] Hot swap froze %s, return entrance 0x%04X\n", Game_ToString(departing), returnEntrance);
    return 1;
}

/**
 * Apply a game's frozen save and retire it in the same step.
 *
 * Called from each game's startup-entrance consumption point
 * (games/oot/src/code/z_play.c, games/mm/src/code/z_play.c) — the first place
 * after the boot chain's SaveContext wipes and before the save is interpreted.
 *
 * Retiring the blob here is what makes a frozen state single-use. A blob that
 * survives its own consumption is indistinguishable, at the next return leg,
 * from a blob that was just created — so a game re-entered without a fresh
 * freeze would silently rewind to it. The clear runs even when the restore
 * reports failure: a blob that could not be applied must not linger to be
 * retried against different state later.
 *
 * @return 1 if a frozen save was applied, 0 if there was none (first entry).
 */
int Combo_ConsumeFrozenState(const char* gameId, void* saveContext, size_t size) {
    if (gameId == NULL || saveContext == NULL || size == 0) {
        return 0;
    }
    if (!Combo_HasFrozenState(gameId)) {
        return 0;
    }

    int restored = Combo_RestoreState(gameId, saveContext, size);
    Combo_ClearFrozenState(gameId);

    fprintf(stderr, "[Switch] Consumed frozen %s state (restored=%d), blob retired\n", gameId, restored);
    return restored;
}

} // extern "C"

// Context_SetCurrentGame / Context_GetCurrentGame live in context.cpp rather
// than here. That split predates this file having any live code; it is kept
// because context.cpp is the lower-level TU and nothing about the switch
// policy above needs to be in the same object as the current-game tracker.
