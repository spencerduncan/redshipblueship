/**
 * @file shared_resources.h
 * @brief Shared cross-game RESOURCES — one quantity spanning both games (#525).
 *
 * A shared resource is NOT a shared item. `shared_items.h` carries a one-way,
 * single-use crossing: an item found in one game is awarded once in the other
 * and the entry is retired. A shared RESOURCE is one continuous quantity that
 * both games read and write for the whole run — capacity AND current value
 * together. Copying OoTMM's user-facing behavior:
 *
 *   "Receiving a heart piece, heart container or double defense affects the
 *    maximum health/defence of both games."
 *   "Current health is tracked as if OoT and MM were one game with a single
 *    health bar."
 *
 * v1 shares rupees and hearts; the #525 optional tier added magic — meter
 * level and current magic, "current magic is tracked as if OoT and MM were one
 * game with a single magic meter". That is what justifies the matching pool
 * shrink in `kForeignPoolMMV1` (#525): with one wallet, one health bar and one
 * magic meter spanning both games, MM's wallet/heart/double-defense/magic rows
 * are no longer separate items to cross — they ARE the shared resource, so
 * shipping them as foreign placements too would hand the player a second copy
 * of a quantity they already have. The ammo upgrades are the queued remainder
 * of the same class.
 *
 * ---------------------------------------------------------------------------
 * THE TWO MERGE DISCIPLINES
 * ---------------------------------------------------------------------------
 * Picking the wrong one is a correctness bug, not a style choice.
 *
 *   MONOTONIC — wallet tier, health quarters, double defense, magic level. A
 *   capacity that only ever grows. Harvest is `shared = max(shared, live)`,
 *   apply is `live = max(live, shared)`. Idempotent in both directions; decay
 *   is impossible by construction, so no bookkeeping is needed.
 *
 *   CONSUMABLE — rupee count, current health, current magic. A quantity the
 *   player spends. Harvest takes a DELTA against a watermark recorded at apply
 *   time, never a raw copy.
 *
 * THE WATERMARK IS MANDATORY. MM's tier-3 wallet holds 500 and OoT's holds 999
 * (`gUpgradeCapacities`, both games). Under a naive `shared = live` harvest,
 * arriving in MM with 800 shared rupees clamps the live count to 500, and the
 * next suspend harvests that 500 straight back over the 800 — the player
 * silently loses 300 rupees on EVERY round trip. With the watermark, apply
 * records what it actually materialized (500) and harvest contributes only what
 * changed since (0), so the 300 that never fit in MM's wallet stays in the pool
 * and comes back intact in OoT.
 *
 * The watermark is RAM-only and per (kind, game): it describes how much of the
 * shared pool is currently materialized in the LIVE save, which is a property
 * of this process's session, not of the saved world.
 *
 * ---------------------------------------------------------------------------
 * THE FIRST-HARVEST SEED, and why it is not an edge case
 * ---------------------------------------------------------------------------
 * Because the watermark is RAM-only, a game that has never been APPLIED to in
 * this process has no watermark — and harvesting a delta against a default of
 * zero would dump its entire live balance into the pool. Whether that is right
 * depends on a distinction the slot array already records:
 *
 *   - Slot EMPTY (kind == RSBS_SHARED_RES_NONE): nothing has ever been shared.
 *     A cold boot that earned 200 rupees genuinely has 200 to contribute, so
 *     seed the watermark at 0 and let the whole balance enter the pool.
 *   - Slot OCCUPIED: the pool was restored from a `.redsave`, which is only
 *     ever written AFTER a harvest — so the live balance is money the pool has
 *     already counted. Seed the watermark at the live value; the delta is zero
 *     and the balance is not double-counted.
 *
 * Without that split, loading a `.redsave` and switching games doubles the
 * player's rupees. The discriminator costs nothing because occupancy is already
 * the kind tag (see ComboSharedResource — a bare `uint16_t` could not express
 * it, since 0 rupees is a legal value).
 *
 * ---------------------------------------------------------------------------
 * DIRECTION: HARVEST WRITES, APPLY READS
 * ---------------------------------------------------------------------------
 * Only harvest writes `gComboCtx.sharedResources`; apply only reads it (plus
 * the RAM watermark). Keep it that way — it is what makes the wiring auditable:
 * every pool mutation is at a suspend or a pre-save point, never mid-scene. An
 * arriving game holding MORE than the pool is not lost, it is picked up by that
 * game's own next harvest.
 *
 * WIRING (both games, see each GameExports_SingleExe.cpp):
 *   - Harvest at `Game_Suspend`, beside `Combo_CommitStagedSharedItems()`, and
 *     before every `.redsave` write. Game_Suspend is the only point on BOTH the
 *     entrance and the F10 hot-swap path while the live `gSaveContext` still
 *     belongs to the departing game.
 *   - Apply at each game's presence-gated startup-entrance point in `z_play.c`,
 *     beside the shared-item consumer — the first point after the boot chain's
 *     last `gSaveContext` wipe. NOT `Game_Resume`: both restores are
 *     re-authored afterwards by `Opening_Init` / `MM_Sram_InitNewSave`.
 *
 * THREADING: game thread only, exactly like shared_items.h.
 */

#ifndef RSBS_COMMON_SHARED_RESOURCES_H
#define RSBS_COMMON_SHARED_RESOURCES_H

#include "context.h" // ComboSharedResource, gComboCtx, GameId, RSBS_SHARED_RES_*

#ifdef __cplusplus
extern "C" {
#endif

/**
 * One past the highest defined RSBS_SHARED_RES_* kind. Sizes the RAM watermark
 * table, which is indexed by KIND (stable) rather than by slot (slots are found
 * by scan and a future compaction could move them).
 */
#define RSBS_SHARED_RES_KIND_COUNT 8u

/**
 * Canonical heart quantity ceiling, in health units (0x10 per heart, 4 per
 * quarter — so a heart PIECE is worth 4 and `RSBS_SHARED_RES_HEALTH_QUARTERS`
 * is `healthCapacity + pieces * 4`). 320 == 20 hearts.
 *
 * Clamping here is required, not defensive: NEITHER game's give path clamps
 * health capacity, and a quantity accumulated across two pools of heart pieces
 * and containers can exceed 20 hearts easily. The life meter past 20 hearts is
 * untested in both ports.
 */
#define RSBS_SHARED_RES_MAX_HEALTH_QUARTERS 320u

/**
 * Build the canonical heart quantity from a game's two fields, clamped to
 * RSBS_SHARED_RES_MAX_HEALTH_QUARTERS.
 *
 * `capacity` is the game's healthCapacity (0x10 per heart); `pieces` is the
 * un-converted heart-piece count from the TOP NIBBLE of questItems — which is
 * where BOTH games keep it (OoT writes `1 << (QUEST_HEART_PIECE + 4)`, MM's
 * QUEST_HEART_PIECE_COUNT is 0x1C), and that agreement is what makes one shared
 * quantity possible at all.
 *
 * Why one number instead of two shared fields: the games CONVERT at different
 * times. MM turns 4 pieces into a container immediately inside Item_GiveImpl;
 * OoT defers to textbox close, with a third site in rando. So OoT can sit at
 * `pieces == 4` with capacity not yet bumped — a state MM never produces, and an
 * F10 swap can land on exactly that frame. Folded into a single quantity the
 * divergence dissolves: the threshold becomes arithmetic instead of a state
 * machine, and neither game can hand the other a combination it cannot express.
 */
uint16_t Combo_MakeHealthQuarters(uint16_t capacity, uint16_t pieces);

/**
 * Split the canonical quantity back into a game's two fields — the exact
 * inverse of Combo_MakeHealthQuarters for any input it can produce.
 *
 * Lives here rather than in each game's shim so the arithmetic has ONE
 * definition; two copies of a normalize/denormalize pair is how the halves
 * drift. Either out-pointer may be NULL.
 */
void Combo_SplitHealthQuarters(uint16_t quarters, uint16_t* outCapacity, uint16_t* outPieces);

/**
 * HARVEST. Fold `liveValue` — the departing game's live value for `kind` — into
 * the shared pool, using that kind's merge discipline. Creates the slot on
 * first use.
 *
 * The caller is responsible for handing over a SETTLED value. For rupees that
 * means folding `rupeeAccumulator` into the count and zeroing it first: both
 * games write the accumulator rather than the count and drain it one per frame,
 * so a pending accumulator would otherwise be harvested as nothing here and
 * then drain into the same game later — under-count now, double-count later.
 *
 * Idempotent for both disciplines: a second call with an unchanged `liveValue`
 * is a no-op (monotonic re-maxes to the same number; consumable computes a zero
 * delta because the first call advanced the watermark).
 *
 * No-op if `game` is not a real game or `kind` is not a real resource. If the
 * slot array is full of other resources the harvest is dropped — the array is
 * sized with headroom for the whole class, so this cannot happen with today's
 * five resources.
 */
void Combo_HarvestSharedResource(GameId game, uint8_t kind, uint16_t liveValue);

/**
 * APPLY. Reconcile the arriving game's live value against the shared pool.
 *
 * @param game       the arriving game (owns the watermark that results)
 * @param kind       RSBS_SHARED_RES_*
 * @param cap        the ARRIVING GAME'S OWN ceiling for this resource — its
 *                   wallet capacity, its health capacity, 1 for a flag. The
 *                   games' ceilings genuinely differ (MM's tier-3 wallet is
 *                   500, OoT's is 999); the pool holds the true total and this
 *                   is what the arriving game can actually display.
 * @param liveValue  in/out: the game's live value, overwritten on success
 *
 * @return true if a shared value existed and `*liveValue` was written; false if
 *         the resource has never been shared, in which case `*liveValue` is
 *         untouched and no watermark is recorded — the game's existing balance
 *         then joins the pool at its next harvest, exactly as a cold boot's
 *         would.
 *
 * MONOTONIC raises `*liveValue` to `min(shared, cap)` and never lowers it.
 * CONSUMABLE sets `*liveValue` to `min(shared, cap)` and records exactly that
 * as the watermark — the excess stays in the pool rather than being lost.
 */
bool Combo_ApplySharedResource(GameId game, uint8_t kind, uint16_t cap, uint16_t* liveValue);

/**
 * Read the shared pool's value for `kind` (trackers / tests / the cycle-safe
 * rupee hook). Returns false and leaves `*outValue` untouched when the resource
 * has never been shared.
 */
bool Combo_GetSharedResource(uint8_t kind, uint16_t* outValue);

/**
 * Number of occupied shared-resource slots (read-only; trackers / tests).
 */
int Combo_CountSharedResources(void);

/**
 * Drop every RAM-only watermark WITHOUT touching gComboCtx.sharedResources.
 *
 * This is the "a new session owns the live save now" reset, and it is exactly
 * what makes the first-harvest seed above correct after a `.redsave` load:
 * with no watermark, an occupied slot seeds at the live value (delta zero)
 * instead of re-contributing money the pool already counted. Also used for test
 * isolation.
 */
void Combo_ResetSharedResourceWatermarks(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_SHARED_RESOURCES_H
