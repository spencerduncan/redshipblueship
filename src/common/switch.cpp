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
 * (`MM_FreezeState`, `MM_ResumeFromContext`) lived in games/mm/2s2h/BenPort.cpp,
 * which is excluded from the single-exe build — so this translation unit could
 * only stay linkable as long as nothing pulled it in. ADR 0002 removed its
 * dangling declarations from context.h. None of the game-port symbols exists
 * any more: `OoT_FreezeState` was deleted by #598 and MM's pair by #427, both
 * never compiled (each sat behind `#ifdef SINGLE_EXECUTABLE_BUILD`, which names
 * a CMake option, not a compile definition), so neither port keeps a copy of
 * that freeze-time shape.
 */

#include "context.h"
#include "entrance.h"
#include "save.h"
#include <chrono>
#include <cstdio>
#include <cstddef>

extern "C" {

// Per-game pre-freeze hooks behind Combo_FlushLiveStateForFreeze below. Each
// lives in its game's GameExports_SingleExe.cpp because only that TU can see
// the game's PlayState / SaveContext layout; this TU only orders them.
void OoT_Combo_FlushSceneFlagsForFreeze(void);
void MM_Combo_FlushSceneFlagsForFreeze(void);
void MM_Combo_ReviveDeadHealthForFreeze(void);
void OoT_Combo_ReviveDeadHealthForFreeze(void);
void OoT_Combo_ApplySceneExitWritesForFreeze(void);
void OoT_Combo_StampSavedSceneForFreeze(void);
// #837: whether the departing gSaveContext is a live file (Combo_SaveIsLiveFile
// on that game's gameMode), read in the game's own TU for the same reason.
int OoT_Combo_DepartureIsLiveFile(void);
int MM_Combo_DepartureIsLiveFile(void);

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
 * Flush the departing game's LIVE state into its gSaveContext, immediately
 * before that gSaveContext is frozen (#638, #626).
 *
 * WHY A SEPARATE STEP. Context_FreezeState copies gSaveContext verbatim, but
 * gSaveContext is not the whole of a game's state: both games keep the scene
 * flags of the CURRENT scene visit in the live PlayState
 * (play->actorCtx.sceneFlags on MM, play->actorCtx.flags on OoT) and copy them
 * into gSaveContext only on a scene transition, from Actor_CleanupContext via
 * Play_SaveCycleSceneFlags / Play_SaveSceneFlags. A cross-game departure never
 * reaches that copy: the entrance switch freezes the instant nextEntrance is
 * assigned (z_player.c / z_play.c, before the transition), and both switch
 * paths then stop the gamestate without running Play_Destroy. So every flag set
 * during the final scene visit -- a heart piece, a chest, a switch, a cleared
 * room -- was frozen as unset, restored as unset on the return leg, and the
 * pickup respawned (#635's heart-piece dupe is one instance of this class).
 *
 * The same seam is where MM revives a dead health bar before it is frozen
 * (#626): an F10 during MM's game-over screen bypasses the kaleido death exit
 * that PR #625 guarded, and would otherwise freeze health == 0 into MM's shadow
 * and hand the shared CONSUMABLE bar to OoT at zero. OoT has the same shape
 * (#664): its game-over screen is vanilla and holds health at 0 until the
 * continue leg, and F10 is polled ungated from its graph loop too, so OoT
 * revives at the same seam, to its own continue value.
 *
 * WHERE IT IS CALLED, AND WHERE IT DELIBERATELY IS NOT. Both production freeze
 * drivers call this immediately before their freeze:
 *   - Combo_CheckEntranceSwitch (games/oot/soh/GameExports_SingleExe.cpp),
 *     before its Combo_FreezeState -- the entrance path;
 *   - Combo_FreezeActiveGameForHotSwap (same TU), before Switch_PrepareHotSwap
 *     -- the F10 path, which is also the launcher's re-freeze
 *     (rsbs/src/main.cpp) and the owl-save / game-over exits' freeze.
 * It is NOT inside Switch_PrepareHotSwap: that function's contract is "freeze
 * THIS buffer", and the src/common tests (test_hotswap_freeze.c,
 * test_foreign_items.c) drive it with scratch buffers that are not gSaveContext.
 * A flush there would write the live gSaveContext while a scratch buffer is
 * frozen -- wrong for those tests and coupling an opaque-buffer policy to game
 * globals. The flush belongs at the two points that pass the REAL gSaveContext.
 *
 * Idempotent: each hook re-copies the same words, so a second call (the
 * game-over exit revives, then the launcher freeze revives again) is a no-op.
 * A game with no live PlayState (owl-save / game-over exits, headless tests)
 * flushes nothing and does not dereference anything. Writes no file itself:
 * the blob it corrects is the one Context_FreezeState captures, and since #837
 * that blob reaches disk at the crossing (Switch_CommitCrossing, below). Two
 * of the flush's jobs serve that commit: it notes whether the half about to be
 * frozen is a live file, which only a game TU can read, and on OoT it records
 * the departure scene as the save's scene (#850), which a reload reads to
 * decide where Link wakes.
 */
void Combo_FlushLiveStateForFreeze(GameId departing) {
    switch (departing) {
        case GAME_OOT:
            Context_NoteDepartureLiveFile(GAME_OOT, OoT_Combo_DepartureIsLiveFile());
            // #770 / #807: the save writes actor Destroys make on any scene
            // exit (windmill gear, Lake Hylia, a Light-Arrow-lit sun switch,
            // Player's linkAge); the departure runs no Destroy. Before the
            // flush, as in Actor_CleanupContext: a sun switch's Destroy unsets
            // a live switch flag that the scene-flag flush below must then
            // copy. (The live-file note above reads only gameMode, which no
            // Destroy writes, so its place relative to this seam is free.)
            OoT_Combo_ApplySceneExitWritesForFreeze();
            OoT_Combo_FlushSceneFlagsForFreeze();
            // #850: the save's scene, so a reload of the crossing commit places
            // Link as a save made here would (OoT_Sram_OpenSave keys on it).
            OoT_Combo_StampSavedSceneForFreeze();
            OoT_Combo_ReviveDeadHealthForFreeze();
            break;
        case GAME_MM:
            Context_NoteDepartureLiveFile(GAME_MM, MM_Combo_DepartureIsLiveFile());
            MM_Combo_FlushSceneFlagsForFreeze();
            MM_Combo_ReviveDeadHealthForFreeze();
            break;
        default:
            break;
    }
}

/**
 * Every cross-game crossing is a whole-file commit (#837; ADR 0009 decision 4c).
 * The contract is in context.h; the points that are easy to get wrong:
 *
 *   - WHERE. GameRunner_SwitchTo calls this between the departing suspend and
 *     the target's resume/init. Inside each Game_Suspend would be two call
 *     sites and no ROM-free row could reach it; main.cpp sees suspend and
 *     resume as one call. Before the suspend, Tier-1 would miss the staged
 *     pickups and the harvest; after the target's Play_Init, the target's
 *     shadow would already be consumed (zeroed) and its items redeemed.
 *   - WHAT. No harvest (the suspend just did it) and no shadow refresh: the
 *     departing half is the freeze, which is what that game resumes from, taken
 *     at the instant nextEntrance was set (the entrance path) or at F10.
 *     The commit adds nothing to it. Where Link wakes after a reload of this
 *     commit is OoT_Sram_OpenSave's savedSceneNum rule, and the freeze carries
 *     the scene OoT departed from because the flush above records it, as a
 *     save made there would (#850).
 *   - THE SKIP LINES never say "REFUSED": IntPairedFirstCrossing fails on that
 *     word in stderr, and its sibling rows cross with no file at all. The latch
 *     is checked here, before RsbsSave_Save, for the same reason: Save's own
 *     latch message names a refused slot in those words.
 */
int Switch_CommitCrossing(GameId departing, GameId target) {
    if ((departing != GAME_OOT && departing != GAME_MM) || (target != GAME_OOT && target != GAME_MM) ||
        departing == target) {
        return 0;
    }
    const char* from = Game_ToString(departing);
    const char* to = Game_ToString(target);
    if (!Context_HasFrozenState(departing)) {
        fprintf(stderr, "[Switch] crossing commit skipped (%s -> %s): %s has no departure freeze for this trip\n", from,
                to, from);
        return 0;
    }
    if (!Context_FrozenStateIsLiveFile(departing)) {
        fprintf(stderr,
                "[Switch] crossing commit skipped (%s -> %s): %s's frozen half is not a live file (title screen or "
                "file select)\n",
                from, to, from);
        return 0;
    }
    const int slot = RsbsSave_GetActiveSlot();
    if (slot < 0) {
        fprintf(stderr, "[Switch] crossing commit skipped (%s -> %s): no active slot (no file loaded, or a debug save)\n",
                from, to);
        return 0;
    }
    if (!RsbsSave_IsSlotWritable(slot)) {
        fprintf(stderr,
                "[Switch] crossing commit skipped (%s -> %s): slot %d is not writable this session (latched: not "
                "loaded, created or erased here, or not paired)\n",
                from, to, slot);
        return 0;
    }

    gComboCtx.sourceGame = target;
    const auto start = std::chrono::steady_clock::now();
    const int written = RsbsSave_Save(slot);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (!written) {
        fprintf(stderr,
                "[Switch] crossing commit FAILED (%s -> %s): slot %d was not written (see above); the crossing "
                "proceeds, %.2f ms\n",
                from, to, slot, ms);
        return 0;
    }
    fprintf(stderr, "[Switch] crossing commit: %s -> %s, slot %d, generation %u, %.2f ms\n", from, to, slot,
            (unsigned)gComboCtx.commitGeneration, ms);
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
