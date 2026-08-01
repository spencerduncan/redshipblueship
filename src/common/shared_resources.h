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
 * game with a single magic meter" — then ammo: the quiver, bomb-bag, stick
 * and nut capacity TIERS plus the arrow, bomb, bombchu, stick and nut COUNTS —
 * and finally the hookshot.
 * That is what justifies the matching pool shrink in `kForeignPoolMMV1`
 * (#525): with one wallet, one health bar, one magic meter, one quiver and one
 * hookshot spanning both games, MM's wallet/heart/double-defense/magic/ammo/
 * hookshot rows are no longer separate items to cross — they ARE the shared
 * resource, so shipping them as foreign placements too would hand the player a
 * second copy of a quantity they already have.
 *
 * THE HOOKSHOT IS WHY "RESOURCE" IS ABOUT REPRESENTATION, NOT VOCABULARY.
 * OoTMM calls it an item ("combines the Hookshots into two progressive items
 * for both games") and #525 filed it as one, but both games store it in a
 * single inventory byte, which is a monotonic tier: 0 none, 1 hookshot, 2
 * longshot. Modelled that way it needs no new machinery and cannot loop.
 * Modelled as a shared ITEM it would need a give-time producer in each game,
 * and each redemption calls the far game's give — an unbounded ping-pong that
 * `shared_items.h`'s content de-dup does NOT stop, because de-dup deliberately
 * skips entries that are already redeemed and redemption is the step closing
 * the cycle. (MM's OnItemGive is also a no-op stub, so that half would have
 * been dead on arrival — the #512/#517 class.) Harvest-at-suspend /
 * apply-at-arrival has no edge back to the producer at all.
 *
 * A CAPACITY TIER CARRIES THE ITEM WITH IT, copying OoTMM ("a Shared Bow
 * grants the ability to use the Hero's Bow in MM and the Fairy Bow in OoT").
 * Each game's apply authors its own inventory slot when a tier rises from
 * zero, because the two games disagree about who does that: MM's own tier
 * gives set INV_CONTENT (it treats quiver tier 1 AS owning the bow), while
 * OoT's tier-2/3 gives assume tier 1 already granted the item. That is also
 * why the bow rows left both pools — a bow crossing would mutate the shared
 * quiver tier, which criterion 6 forbids.
 *
 * ---------------------------------------------------------------------------
 * THE TWO MERGE DISCIPLINES
 * ---------------------------------------------------------------------------
 * Picking the wrong one is a correctness bug, not a style choice.
 *
 *   MONOTONIC — wallet tier, health quarters, double defense, magic level, the
 *   four ammo capacity tiers, and the hookshot tier. A capacity that only ever
 *   grows. Harvest is
 *   `shared = max(shared, live)`, apply is `live = max(live, shared)`.
 *   Idempotent in both directions; decay is impossible by construction, so no
 *   bookkeeping is needed.
 *
 *   CONSUMABLE — rupee count, current health, current magic, and the five ammo
 *   counts. A quantity the player spends. Harvest takes a DELTA against a
 *   watermark recorded at apply time, never a raw copy.
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
 *   - Every harvest shim opens with `Combo_SaveIsLiveFile()` and returns early
 *     when it is false. See that function for why the gate is mandatory rather
 *     than defensive: without it, F10 on the title screen harvests the ATTRACT
 *     DEMO's save.
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
#define RSBS_SHARED_RES_KIND_COUNT 18u

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
 * `gSaveContext.gameMode` values. BOTH ports spell this field and these
 * enumerators identically (games/oot/include/z64save.h, games/mm/include/-
 * z64save.h) and agree on 0..3; MM alone adds 4. Restated here as macros
 * because this TU deliberately includes no game headers — each shim
 * static_asserts its own enum against these, so a renumber upstream is a build
 * error rather than a silently inverted gate.
 */
#define RSBS_GAMEMODE_NORMAL 0
#define RSBS_GAMEMODE_TITLE_SCREEN 1
#define RSBS_GAMEMODE_FILE_SELECT 2
#define RSBS_GAMEMODE_END_CREDITS 3
#define RSBS_GAMEMODE_OWL_SAVE 4 // MM only

/**
 * Is the live `gSaveContext` a REAL LOADED FILE being played — as opposed to the
 * synthetic save a menu leaves resident?
 *
 * THIS GATE IS MANDATORY, AND IT IS NOT DEFENSIVE. While OoT sits on the title
 * screen, `gSaveContext` holds the ATTRACT DEMO's save: `Opening_SetupTitle-
 * Screen` (z_opening.c) calls `OoT_Sram_InitDebugSave`, and `SaveManager::-
 * InitFileDebug` authors 14 hearts, single magic, 150 rupees and
 * `inventory.upgrades == 0x125249` — which decodes to TIER 1 of all eight
 * upgrade slots (quiver, bomb bag, wallet, sticks, nuts and the rest), against a
 * new file's zeroes. `Combo_CheckHotSwap` is polled from graph.c EVERY FRAME
 * with no gameplay gate, so F10 on the title screen runs a full harvest against
 * that save. Every MONOTONIC kind it touches then sits in the pool permanently —
 * max-merge cannot decay by construction — and since A CAPACITY TIER CARRIES ITS
 * ITEM WITH IT (see above), the next arrival in either game materializes a bow, a
 * bomb bag and an upgraded wallet the player never earned. OoT's file-select
 * screen and MM's own title screen (`TitleSetup_SetupTitleScreen` ->
 * `MM_Sram_InitNewSave`) are the same class.
 *
 * WHY `gameMode` IS THE WHOLE TEST. It is the one field both games set to
 * GAMEMODE_NORMAL at the cross-game arrival itself — in z_play.c, inside the
 * same block that calls each game's apply — and to TITLE_SCREEN / FILE_SELECT on
 * every menu path. So no frame can observe a menu's save under a NORMAL mode.
 * It is also exactly what SoH's own `GameInteractor::IsSaveLoaded` leans on, for
 * this precise reason ("prevents debug saves from reporting true on title
 * screen"). MM additionally accepts GAMEMODE_OWL_SAVE — a real file mid-save,
 * reached from gameplay, and the mode one of MM's two `.redsave` capture points
 * fires in. END_CREDITS is excluded in both: the save is real but the session is
 * terminal, so there is nothing downstream for a harvest to serve.
 *
 * WHY `fileNum` IS DELIBERATELY NOT PART OF THIS. Two independent reasons, and
 * the second is the surprising one:
 *
 *   1. 0xFF IS OVERLOADED. It is OoT's title/map-select "no file" marker AND
 *      this project's mandatory cross-game sentinel, which MM's `fileNum` is
 *      PINNED to for the entire life of a legitimate cross-game session (see
 *      `Sram_FileNumHasFlashSlot` and the 0xFF gates in z_sram_NES.c). On the MM
 *      side it therefore carries no information at all, and rejecting it would
 *      suppress the whole feature in the direction it was written for.
 *
 *   2. AN OOT-ONLY fileNum RULE IS ACTIVELY WORSE THAN NONE. Gating OoT's
 *      harvest on a real slot while MM's harvest (which cannot be so gated) and
 *      BOTH applies stay live makes the flow one-way — and apply is not
 *      symmetric with harvest: for a CONSUMABLE it ASSIGNS
 *      (`*liveValue = applied`), so it can lower a live value. A slot-less OoT
 *      session would stop contributing while still receiving, and MM's harvest
 *      would walk OoT's 14-heart debug save down to MM's 3-heart bootstrap. That
 *      is a new corruption introduced by the guard, in the exact session
 *      `IntGameplayRoundtrip` runs in. Keeping the predicate symmetric across
 *      the two games is what keeps harvest and apply paired.
 *
 * KNOWN RESIDUAL, accepted deliberately: OoT's map-select debug save (fileNum
 * 0xFF) and Boss Rush (0xFE) do reach gameplay under GAMEMODE_NORMAL carrying
 * synthetic inventories nobody earned, and this predicate admits both. They are
 * admitted SYMMETRICALLY — both games harvest and both apply, which is the
 * pre-existing
 * behavior and cannot corrupt either live save — and neither can persist a pool
 * on its own, because OoT's `.redsave` hooks reject any fileNum outside 0..2 and
 * MM's capture needs an active slot only those hooks establish. Closing them
 * needs the APPLY gated on the same predicate, which is a separate change.
 *
 * A suppressed harvest must be a clean no-op on BOTH stores: an early return
 * taken BEFORE any `Combo_HarvestSharedResource` call, so neither the pool nor
 * the RAM watermark table is touched. That is what keeps the first-harvest seed
 * above intact — a skipped harvest leaves "has this game been applied to in this
 * process" exactly as it was, so the next real harvest applies the
 * empty/occupied rule to an unchanged world.
 *
 * @param game     the game whose gSaveContext this is
 * @param gameMode `gSaveContext.gameMode` (RSBS_GAMEMODE_*)
 */
bool Combo_SaveIsLiveFile(GameId game, int32_t gameMode);

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
 * No-op if `game` is not a real game or `kind` is not a real resource. If both
 * slot blocks are full of other resources the harvest is dropped — they are
 * sized with headroom for the whole class (twenty slots against today's
 * seventeen kinds), so this cannot happen without three more being added.
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
