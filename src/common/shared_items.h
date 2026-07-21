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
 * TWO PRODUCER CLASSES (ADR 0005, netplay 1a #460) — choosing the wrong one is
 * a correctness bug, not a style choice:
 *
 *   - IN-PROCESS producers (a give path re-firing inside this process) use
 *     Combo_RecordSharedItem / Combo_StageSharedItem. Their idempotency is
 *     CONTENT de-dup: an identical un-redeemed (originGame, id) is the same
 *     event re-observed, so it merges. Correct here, and ONLY here.
 *   - SOURCED producers (anything with its own identity and its own delivery
 *     stream: a network peer, an Archipelago server, a loopback test feed) use
 *     Combo_SubmitSourcedGrant. Their idempotency is a per-source monotonic
 *     CURSOR: a retransmit (seq <= cursor) is dropped, a genuinely new grant
 *     of the same item (fresh seq) is recorded WITHOUT content de-dup — two
 *     peers gifting you the same item is two items. A sourced producer must
 *     never call Combo_RecordSharedItem directly: content de-dup would
 *     silently merge distinct gifts.
 *
 *     The two domains never mix: sourced entries carry
 *     RSBS_SHARED_ITEM_SOURCED, and content de-dup skips them, so a peer's
 *     pending gift can never swallow a genuine local pickup of the same item
 *     (nor vice versa).
 *
 * REDEMPTION SAFE POINTS (ADR 0005). Combo_RedeemSharedItemsForGame is safe at
 * any point where ALL of the following hold for `arrivingGame`, not only at a
 * game switch: (1) it is the active game, on the game thread; (2) a save is
 * loaded and normal gameplay has been reached, so the award callback's give
 * machinery is valid; (3) the caller passes that game's real award callback.
 * The presence-gated startup-entrance consumption points are the two wired
 * safe points today. A gameplay-gated frame tick is the third, DEFINED safe
 * point — it gets wired together with the first producer that can target the
 * active game mid-session (the 1b transport), because until such a producer
 * exists there is nothing for a tick to redeem: every in-process producer
 * records for the game you are leaving. Single-use is independent of the
 * trigger: RSBS_SHARED_ITEM_REDEEMED makes redemption idempotent under any
 * interleaving of safe points.
 *
 * Plain `.redsave` load without a switch: unchanged for everything wired
 * today. Un-redeemed items persist in the loaded `gComboCtx` and are awarded
 * at the next safe point — which, with only arrival points wired, is the next
 * switch into their origin game. We deliberately do NOT redeem at load time:
 * awarding routes through the game's live item-give machinery, which is only
 * valid once the game has reached gameplay — exactly what every safe point
 * above guarantees.
 *
 * CAPACITY (ADR 0005). The durable array holds RSBS_SHARED_ITEM_CAP entries.
 * When it is full, recording first RECLAIMS the oldest REDEEMED entry (the
 * array compacts, preserving relative order, and the freed tail slot takes
 * the new record) — a redeemed entry is an informational record of a done
 * crossing, an un-redeemed entry is an undelivered item; only the former may
 * be evicted, and ADR 0005 amends A1's "entries are never cleared" note
 * accordingly. If every entry is un-redeemed, the record is REFUSED — loudly:
 * the durable gComboCtx.sharedItemOverflowCount increments (serialized, so
 * the signal survives save/load) and the refusal is logged. For a sourced
 * grant a refusal is backpressure, not loss: the cursor does not advance, so
 * the source still owes the grant and a later retransmit is accepted. For an
 * in-process producer a refusal IS a lost item (the pickup already happened);
 * the counter is what keeps that loss diagnosable. Slot indices returned by
 * the record APIs are transient (reclamation compacts the array) — never
 * store them.
 *
 * THREADING: everything in this header runs on the game thread only. A
 * transport receiving off-thread must marshal onto the game thread before
 * calling Combo_SubmitSourcedGrant (that is part of the seam's contract).
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
 * IN-PROCESS PRODUCER (direct write). Record a cross-game item into
 * gComboCtx.sharedItemsTagged, visible immediately in the shared context and
 * persisted by the next `.redsave` save.
 *
 * @param originGame the id-space owner (GAME_OOT => `id` is RG_*, GAME_MM => RI_*)
 * @param id         the item id within originGame's id-space
 * @return the slot index used (>= 0), or -1 if originGame is not a real game or
 *         the array is full of un-redeemed entries (after attempting
 *         reclamation; the refusal increments the durable overflow count).
 *         Returned indices are transient — reclamation compacts the array —
 *         so treat them as success/failure only, never store them.
 *
 * De-dups BY CONTENT: if an identical (originGame, id) entry already exists and
 * is NOT yet redeemed, its slot is returned unchanged rather than duplicated —
 * so a re-fired producer (the Combo_CheckEntranceSwitch wasAlreadyPending
 * re-entry, or two suspends with no consume between them) cannot create
 * doubles. A matching entry that is already redeemed does NOT block a fresh
 * record: the same item legitimately crossing again gets a new slot.
 *
 * Content de-dup is only correct for in-process re-fired producers. A SOURCED
 * producer (network peer, Archipelago server) must go through
 * Combo_SubmitSourcedGrant instead, where retransmit-vs-new is decided by the
 * source's cursor and two distinct grants of the same item both record.
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
 * originGame == arrivingGame via `award`, in slot order — which is acceptance
 * order, and reclamation preserves it — then set RSBS_SHARED_ITEM_REDEEMED on
 * each. Received-order awarding is a contract, not an accident: the give paths
 * resolve progressive items against the live save, so order changes WHAT the
 * player receives. Redeemed entries stay in the array as the serialized record
 * of the crossing until capacity pressure reclaims them (file header).
 *
 * Wired today at each game's presence-gated startup-entrance consumption point
 * (OoT_Play_Init / MM_Play_ConsumeStartupEntrance), which run only on a switch
 * arrival. Safe at ANY redemption safe point (file header): it has no
 * dependency on switch machinery, and RSBS_SHARED_ITEM_REDEEMED makes it
 * idempotent under any interleaving of safe points.
 *
 * @return the number of entries redeemed this call.
 */
int Combo_RedeemSharedItemsForGame(GameId arrivingGame, ComboSharedItemAward award, void* ctx);

// ============================================================================
// Sourced grants (ADR 0005, netplay 1a #460) — the producer seam a transport
// writes against. No transport lives in this layer: the only callers today are
// the CI locks, and the 1b transport plugs in here without this file changing.
// ============================================================================

/**
 * Result of Combo_SubmitSourcedGrant. Everything except RSBS_GRANT_ACCEPTED
 * leaves gComboCtx completely unchanged (no item recorded, no cursor moved).
 */
typedef enum {
    RSBS_GRANT_ACCEPTED = 0,   // recorded; the source's cursor advanced to seq
    RSBS_GRANT_DUPLICATE = 1,  // seq <= cursor: retransmit of a delivered grant; drop it (idempotent success)
    RSBS_GRANT_GAP = 2,        // seq skips ahead: predecessors missing; resync/resend the source in order
    RSBS_GRANT_RETRY_FULL = 3, // array full of un-redeemed items: backpressure — the source still owes this
                               // grant (cursor unmoved) and must re-offer it later
    RSBS_GRANT_NO_SOURCE_SLOT = 4, // all RSBS_GRANT_SOURCE_CAP cursor slots taken by other sources
    RSBS_GRANT_REJECTED = 5,   // malformed: sourceKey 0, seq 0, or originGame not a real game
} ComboGrantResult;

/**
 * SOURCED PRODUCER. Submit one grant from an identified source, idempotently.
 *
 * @param sourceKey  nonzero identity of the source WITHIN the current world /
 *                   session (the transport derives it — e.g. a hash of room id
 *                   + peer slot; an Archipelago server is one source). Cursor
 *                   state is per-key and lives in gComboCtx, so #440-class
 *                   invalidation wipes keys and items together.
 * @param seq        the source's own monotonic sequence number for this grant,
 *                   starting at 1 for the source's first grant, dense (no
 *                   holes). A server-authoritative cursor maps directly: an
 *                   Archipelago ReceivedItems index i is seq i + 1.
 * @param originGame / @param id  exactly Combo_RecordSharedItem's parameters.
 *
 * Acceptance is STRICTLY in-order per source (seq must be cursor + 1; a new
 * source must start at 1). That is what makes every case decidable: a
 * retransmit is DUPLICATE, a genuine second gift has a fresh seq and records
 * without content de-dup, a lost message surfaces as GAP instead of silent
 * reordering, and a capacity refusal (RETRY_FULL) leaves the cursor unmoved so
 * the grant is retried rather than lost. On DUPLICATE the grant was already
 * recorded by an earlier accept; whether it has been redeemed since is the
 * consumer's business — the producer seam never re-records it.
 *
 * Game thread only (file header). See ComboGrantResult for the outcomes.
 */
ComboGrantResult Combo_SubmitSourcedGrant(uint32_t sourceKey, uint32_t seq, GameId originGame, uint16_t id);

/**
 * The source's persisted delivery cursor: the highest seq ever ACCEPTED from
 * sourceKey, or 0 if the source is unknown (nothing accepted yet — a source
 * only exists once its seq 1 is accepted). A transport resumes delivery at
 * cursor + 1 after a reconnect or a .redsave load; an Archipelago client hands
 * this to Sync as its ReceivedItems index.
 */
uint32_t Combo_GetGrantCursor(uint32_t sourceKey);

/**
 * Number of occupied grant-cursor slots (read-only; trackers / tests).
 */
int Combo_CountGrantSources(void);

/**
 * Durable count of shared-item records refused for capacity (read-only view of
 * gComboCtx.sharedItemOverflowCount; serialized in every .redsave). Nonzero
 * means the array hit capacity with every slot un-redeemed: for in-process
 * records that is a lost item, for sourced grants a backpressure event. Reset
 * only by ComboContext_Init (fresh world / #440 invalidation).
 */
uint32_t Combo_GetSharedItemOverflowCount(void);

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
