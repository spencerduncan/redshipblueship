/**
 * @file shared_items.h
 * @brief Cross-game shared-item producers and consumers (ADR 0002, Lane A1).
 *
 * The carrier for cross-game items is `gComboCtx.sharedItemsTagged[]`, a
 * process-global array shared by both games in the single exe and serialized
 * into every `.redsave` (ADR 0002 / context.h). The freeze/restore machinery
 * does NOT carry it — it moves only a SaveContext blob — so producers and
 * consumers read and write the array directly at game-side hook points and the
 * array simply crosses the switch by being process-global.
 *
 * The data model is a small state machine per entry:
 *
 *   record/commit (producer) --> occupied, flags == 0 (not redeemed)
 *   redeem (consumer)         --> occupied, flags |= RSBS_SHARED_ITEM_REDEEMED
 *
 * An entry is occupied iff `originGame != GAME_NONE`; `originGame` is the game
 * whose id-space `id` belongs to (GAME_OOT => id is an OoT RandomizerGet RG_*,
 * GAME_MM => id is an MM RandoItemId RI_*). A redeemed entry stays in the array
 * as the durable record of the crossing — it is never cleared — so a round trip
 * can never award the same entry twice.
 *
 * Direction of flow (the concrete cross-game rando semantics this plumbs):
 *   - The game the player is IN records a foreign pickup — an item whose
 *     id-space belongs to the OTHER game — tagged with that other game as
 *     `originGame`. (Lane C's give path; here for Lane A1 it is the producer
 *     API + the round-trip test.)
 *   - When the player next ARRIVES in the origin game, that game's consumer
 *     awards every un-redeemed entry tagged for it and marks it redeemed.
 *
 * Plain `.redsave` load without a switch (decided, documented): the consumers
 * run only at the presence-gated startup-entrance consumption points, which a
 * cold boot or a plain load never reaches. Un-redeemed items therefore persist
 * in the loaded `gComboCtx` and are awarded on the NEXT switch into their
 * origin game — an explicit "applies on next switch only" semantic. No item is
 * lost (the array is serialized and REDEEMED tracks what is done) and none is
 * double-awarded. We deliberately do NOT redeem at load time: awarding routes
 * through the game's live item-give machinery, which is only valid once the
 * game has reached gameplay — exactly what the consumption point guarantees.
 */

#ifndef RSBS_COMMON_SHARED_ITEMS_H
#define RSBS_COMMON_SHARED_ITEMS_H

#include "context.h" // SharedItem, gComboCtx, GameId, RSBS_SHARED_ITEM_*, RSBS_SHARED_ITEM_CAP

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Award callback used by the consumer. Invoked once per redeemed entry, in slot
 * order, BEFORE the entry's RSBS_SHARED_ITEM_REDEEMED bit is set (so a callback
 * that itself inspects the array sees the entry as not-yet-redeemed). `item` is
 * a read-only view of the entry being awarded. Lane C supplies the real give
 * (OoT: Randomizer_Item_Give; MM: its give path); Lane A1 wires a logging
 * placeholder at the game sites. May be NULL to redeem without awarding.
 */
typedef void (*ComboSharedItemAward)(const SharedItem* item, void* ctx);

/**
 * PRODUCER (direct write). Record a cross-game item into
 * gComboCtx.sharedItemsTagged, visible immediately in the shared context and
 * persisted by the next `.redsave` save.
 *
 * @param originGame the id-space owner (GAME_OOT => `id` is RG_*, GAME_MM => RI_*)
 * @param id         the item id within originGame's id-space
 * @return the slot index used (>= 0), or -1 if originGame is not a real game or
 *         the array is full.
 *
 * De-dups: if an identical (originGame, id) entry already exists and is NOT yet
 * redeemed, its slot is returned unchanged rather than duplicated — so a
 * re-fired producer (the Combo_CheckEntranceSwitch wasAlreadyPending re-entry,
 * or two suspends with no consume between them) cannot create doubles. A
 * matching entry that is already redeemed does NOT block a fresh record: the
 * same item legitimately crossing again gets a new slot.
 */
int Combo_RecordSharedItem(GameId originGame, uint16_t id);

/**
 * PRODUCER (deferred stage). Enqueue an item into a process-global RAM outbox,
 * to be flushed into gComboCtx by Combo_CommitStagedSharedItems at the next
 * Game_Suspend. A caller that would rather hand off at the switch boundary than
 * touch the serialized array mid-play uses this.
 *
 * @return true if staged, false if the outbox is full or originGame is invalid.
 *
 * NOTE: the outbox is RAM-only. A save taken between staging and the next
 * suspend does NOT persist a staged item — use Combo_RecordSharedItem for
 * immediate persistence.
 */
bool Combo_StageSharedItem(GameId originGame, uint16_t id);

/**
 * PRODUCER HOOK. Flush the outbox into gComboCtx.sharedItemsTagged (through
 * Combo_RecordSharedItem, so the same de-dup applies), emptying the outbox.
 *
 * Called from BOTH OoT_Game_Suspend and MM_Game_Suspend — NOT from
 * Combo_CheckEntranceSwitch. The F10 hot-swap path bypasses the entrance hook
 * entirely, so a producer that lived only there would drop every hotkey
 * switch's staged writes; Game_Suspend is on both switch paths. Idempotent and
 * safe to call on an empty outbox.
 *
 * @return the number of items committed to gComboCtx this call.
 */
int Combo_CommitStagedSharedItems(void);

/**
 * CONSUMER HOOK. Award every occupied, un-redeemed entry whose
 * originGame == arrivingGame via `award`, then set RSBS_SHARED_ITEM_REDEEMED on
 * each. Entries are NOT cleared: they remain as the serialized record of the
 * crossing.
 *
 * Called from each game's presence-gated startup-entrance consumption point
 * (OoT_Play_Init / MM_Play_ConsumeStartupEntrance), which run only on a switch
 * arrival. See the file header for the plain-load semantics.
 *
 * @return the number of entries redeemed this call.
 */
int Combo_RedeemSharedItemsForGame(GameId arrivingGame, ComboSharedItemAward award, void* ctx);

/**
 * Count occupied entries whose originGame == game. When includeRedeemed is
 * false, only un-redeemed entries are counted. Read-only (trackers / tests).
 */
int Combo_CountSharedItems(GameId game, bool includeRedeemed);

/**
 * Clear the process-global outbox WITHOUT touching gComboCtx.sharedItemsTagged.
 * For test isolation and as a defensive reset alongside a fresh combo init.
 */
void Combo_ClearSharedItemOutbox(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_SHARED_ITEMS_H
