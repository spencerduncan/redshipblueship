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

// ============================================================================
// THE SINGLE-OWNER ITEM CLASSIFICATION TABLE (ADR 0010 answer O8; #645, #500)
// ============================================================================
//
// WHAT IT ANSWERS. For every item either game's fill can place, ONE fill class:
// progression, junk, renewable or trap. The combined fill (increment 3's single
// bag) needs that answer for BOTH games in one place — which items it must
// assume to prove the goal, which ones it may use to fill the hosts left over
// once the bag is placed, and which ones may never leave their own game — and
// O8 decided where the answer lives: here, "one owner per shared item ... never
// a per-game duplicate that can disagree with itself".
//
// ONE CLASS PER CROSS-GAME SHARED QUANTITY (#731). Rows are keyed per (origin,
// id), and each game classifies its OWN copy of a #525 shared quantity by its own
// fill predicate. For three quantities the two predicates disagree:
//   - bombchus (RSBS_SHARED_RES_BOMBCHU_COUNT): OoT RG_BOMBCHU_5/10/20 are
//     advancement (item_list.cpp) -> PROGRESSION; MM RI_BOMBCHU_5/10 and
//     RI_BOMBCHU are RITYPE_JUNK (Items.cpp) -> RENEWABLE;
//   - double defense (RSBS_SHARED_RES_DOUBLE_DEFENSE): OoT RG_DOUBLE_DEFENSE is
//     advancement -> PROGRESSION; MM RI_DOUBLE_DEFENSE is RITYPE_HEALTH -> JUNK;
//   - heart pieces and containers (RSBS_SHARED_RES_HEALTH_QUARTERS): OoT
//     PROGRESSION, MM RITYPE_HEALTH -> JUNK.
// That is OoTMM's SHARED_BOMBCHU shape (one quantity, two answers), and the owner
// now REFUSES it rather than storing it. Each source tags, per row, the ONE #525
// kind its give feeds (`ComboItemClassRow.sharedKind`; only the game TUs can name
// those ids under ADR 0002), and the owner applies ONE rule in ONE place:
//
//   THE RECONCILED CLASS OF A SHARED KIND is the highest-ranked class among ALL
//   rows of EVERY registered origin that feed it, ranked
//   PROGRESSION > RENEWABLE > JUNK. Every row feeding the kind answers that class
//   through Combo_ItemClassOf, whichever origin it belongs to.
//
// PROGRESSION WINS because it is the only sound direction: an MM bombchu pack
// refills the count OoT's bombchu logic reads, so classing it as filler would let
// the bag put something a logic term needs anywhere, with no logic (the per-game
// junk pass places filler blind). The reverse error only costs the proof some
// work. A TRAP row can never feed a shared kind — a trap is a per-game
// punishment, not a quantity — so a source that tags one is stored as an invalid
// row (unclassified), which S2 of the shared-item-class lock counts.
//
// WHAT THE RECONCILIATION DECIDES FOR MM'S HEARTS (#733). MM's own fill predicate
// files RI_HEART_PIECE / RI_HEART_CONTAINER as RITYPE_HEALTH (non-advancement),
// but MM's graph reads health: Regions/East.cpp gates RC_IKANA_CANYON_GHOST_HUT_
// PIECE_OF_HEART and RC_ENEMY_DROP_POE_SISTER on CHECK_MAX_HP(4), found by the O6
// grow-check (an MM heart grant grew the closure by exactly two checks). They feed
// RSBS_SHARED_RES_HEALTH_QUARTERS, which OoT's heart rows feed as PROGRESSION, so
// the reconciled class is PROGRESSION and the bag carries every MM heart copy as a
// REQUIRED row. That is the decision, taken over the alternative of modelling
// max-HP as a derived quantity the proof assumes: the proof then reads hearts
// exactly as MM's own give path produces them (the engine grants the copy and
// CHECK_MAX_HP reads the save), with no second model of health to keep in step.
// combo-logic-bag-composition locks it over MM's real engine (the two checks are
// reached with the heart rows assumed and NOT reached without them).
//
// Combo_ItemClassSourceOf keeps the source's own, unreconciled answer, for the
// locks that check a source against its own game's fill predicate. Everything
// that DECIDES something reads Combo_ItemClassOf.
//
// OWNER VERSUS SOURCE, AND THE ADR 0002 ARGUMENT. Deciding a class means naming
// `RG_*` / `RI_*` and reading each game's item table (`Item::IsAdvancement()`,
// `RandoStaticItem::randoItemType`), which ADR 0002 confines to that game's own
// TUs — this pair includes no game header and must not start. So each game is
// the single SOURCE of its own rows: it registers a classify function, over its
// own id space, from a file-scope registrar in its combo-logic engine TU
// (OoT: ComboLogicEngineOoT.cpp; MM: ComboLogicEngineSingleExe.cpp). And this
// pair is the single OWNER of the answer: it walks the source ONCE, stores the
// rows, and every consumer reads the stored row, never the source. What crosses
// the boundary is (origin, id, class, arming bits) — scalars, exactly what a
// SharedItem already is. Two properties make "single" enforceable rather than
// hoped for:
//   - ONE SOURCE PER ORIGIN. A second registration for an origin that already
//     has one is REFUSED (and counted), not replaced — unlike the foreign-pool
//     registry, which replaces and logs. Two sources for one id space is the
//     "duplicate that can disagree" O8 forbids, so there is no winner to pick.
//   - THE OWNER CAN CATCH ITS SOURCE DIVERGING. Combo_ItemClassVerify re-walks
//     the source and counts every id whose answer no longer matches the stored
//     row, so a source that is not a pure function of its static item table (a
//     classify that read a live setting, say) goes red instead of quietly
//     answering differently at fill time than it did when the table was built.
//
// WHEN THE ROWS ARE TAKEN. At the FIRST QUERY after registration, not inside the
// registrar: neither item table is guaranteed initialised when a file-scope
// registrar runs (OoT's is filled by Rando::StaticData::InitItemTable at OTR
// bring-up; MM's is a static std::map in another TU, unordered against this
// one). A source that is not ready yet says so (-1) and the owner builds nothing
// and caches nothing, so a premature query answers NONE instead of freezing an
// empty table for the rest of the process.
//
// THE FOUR CLASSES, precedence first — the first rule that matches decides:
//   1. TRAP        a punishment the host game disguises with its own machinery
//                  (OoT RG_ICE_TRAP via GetJunkItem()/possibleIceTrapModels; MM
//                  RI_TRAP via gRando.Traps / OfferTrapItem). FIRST, because MM's
//                  own fill predicate calls RI_TRAP non-junk (it is typed
//                  RITYPE_LESSER), so under any other order a trap would be
//                  PROGRESSION — and a trap in the assumed bag is an item the
//                  proof "has" that the player never gets.
//   2. PROGRESSION the game's OWN fill predicate says it may unlock something:
//                  OoT `Item::IsAdvancement()`; MM "randoItemType is neither
//                  RITYPE_JUNK nor RITYPE_HEALTH" (GlitchlessLogic.cpp's
//                  non-junk test). Taken verbatim, including where it is
//                  generous (MM calls maps, owl statues and a gold-dust refill
//                  non-junk): classing an item the fill considers useful as
//                  filler would let the bag put something logic needs anywhere,
//                  which is unsound, while the generous reading only costs the
//                  proof some work. Progression outranks renewable for the same
//                  reason — OoT's bombchu packs are advancement AND regainable.
//   3. RENEWABLE   not progression, and its effect can be regained in play:
//                  rupees, ammo, refills, recovery hearts, anything a shop
//                  sells again.
//   4. JUNK        everything else a fill places: maps and compasses outside
//                  logic, heart pieces MM's fill treats as filler, MM's RI_JUNK
//                  cover item.
//
// WHY TRAP IS A FOURTH CLASS AND NOT JUNK. O8 named three classes; the fourth is
// justified only because the bag must treat the two differently, and it must:
//   - JUNK (and RENEWABLE) is the SURPLUS ABSORBER. When copies outnumber hosts —
//     OoT's RO_ITEM_POOL_PLENTIFUL and MM's RO_PLENTIFUL_ITEMS both add surplus
//     progression copies and let filler make room (MM's plentiful pass counts
//     exactly the JUNK+HEALTH entries as `replaceableItems`) — junk is what may be
//     dropped or replaced, and when hosts outnumber the bag it is what fills
//     them, drawn from the HOST game's own junk.
//   - A TRAP is neither. Its count is fixed by a frozen setting (MM
//     RO_TRAP_AMOUNT; OoT's ice-trap pool mode and percentage), so shedding one
//     to make room changes the world the settings describe; and it is only a trap
//     in its own game, whose models disguise it — this increment no trap crosses (a
//     foreign trap the host cannot render is out of scope, per the 2026-09-26
//     operator note), which Combo_ItemClassMayCrossUnder enforces.
// Junk does not cross either (criterion 2), but for a different reason and with
// a different consequence: junk is interchangeable with the host's own junk, a
// trap is interchangeable with nothing.
//
// THE CLASS IS NOT A MULTIPLICITY. The class is per (origin, id); how many
// copies of an id the bag holds (several small keys, plentiful's surplus copies)
// is the bag's business (the 2026-09-26 multiplicity ruling). A surplus copy of a
// progression item is still progression — whether the proof needs it is a
// question about the bag, not about the item.
//
// NOT TO BE CONFUSED WITH RSBS_ITEMCLASS_* (foreign_items.h). Those are ADR 0011's
// frozen, user-armed SELECTION bits over the hand-adjudicated foreign pools
// (songs, masks, dungeon items ...). These are the fill's structural classes over
// every item. Orthogonal axes; neither is derived from the other.

/** The fill classes. Pinned values (a lock prints and compares them), append-only. */
#define RSBS_FILL_CLASS_NONE 0u        /* not a fill item in this id space (gap, sentinel, event row) */
#define RSBS_FILL_CLASS_PROGRESSION 1u /* the game's own fill predicate says it may unlock something */
#define RSBS_FILL_CLASS_JUNK 2u        /* filler that is not regainable in play */
#define RSBS_FILL_CLASS_RENEWABLE 3u   /* filler whose effect a player can regain in play */
#define RSBS_FILL_CLASS_TRAP 4u        /* a per-game punishment disguised by its own game */
/** One past the highest class. */
#define RSBS_FILL_CLASS_COUNT 5u

// ----------------------------------------------------------------------------
// SETTINGS-CONDITIONAL CROSSING: arming conditions, NOT baked into the class
// ----------------------------------------------------------------------------
//
// The class is a property of the item. Whether a progression item may enter the
// union bag and cross games is ALSO a property of the frozen settings: with OoT
// keysanity confined to the item's own dungeon, OoT's small keys are placed by a
// restricted pass and never reach the general pass the union bag is drawn from
// (ADR 0010's O4 amendment 2), so they do not cross; with MM soul shuffle
// unarmed, an MM soul give is a bare flag with no meaning (ADR 0011 criterion 3).
// Baking either into the static table would make the table a function of
// settings, which is the SHARED_BOMBCHU wart again (one item, two answers
// depending on where you ask). So each row carries the
// ARMING CONDITIONS under which it may roam, and Combo_ItemClassMayCrossUnder is
// the predicate over a frozen world's armed set.
//
// The low half is the give-capability families, bit-for-bit the RSBS_GIVECAP_*
// values of foreign_items.h (asserted in shared_items.c), so the frozen profile
// MM already publishes arms them with no translation. The high half is the
// CONFINEMENT families: settings that can confine an item family to a restricted
// placement pass. Only a game with restricted passes tags them (OoT does; MM's
// fill places its whole pool in one pass, so its rows carry none).
//
// ONE SETTING PER CONFINEMENT BIT, AND ONLY THE VALUE THAT HANDS THE FAMILY TO
// THE GENERAL PASS ARMS IT. A confinement bit means "the ONE setting named below
// hands this family to the GENERAL placement pass" (the pass the union bag is
// drawn from) and nothing weaker. OoT's own-dungeon, any-dungeon and overworld
// values are all RESTRICTED passes (3drando/fill.cpp RandomizeOwnDungeon /
// RandomizeDungeonItems), and vanilla / start-with are fixed placements that
// never enter any pool, so for the key, map, song and reward families a
// publisher arms the bit only for the setting's anywhere value. The families are
// split by the setting that actually confines them, because OoT's item types
// are not: ITEMTYPE_SMALLKEY covers the dungeon keys (RSK_KEYSANITY) and the
// treasure-game keys (no confinement at all: RSK_SHUFFLE_CHEST_MINIGAME only
// decides whether they enter the pool, item_pool.cpp, and no restricted pass
// names them), ITEMTYPE_FORTRESS_SMALLKEY follows RSK_GERUDO_KEYS, and
// ITEMTYPE_BOSSKEY covers the dungeon boss keys (RSK_BOSS_KEYSANITY) and Ganon's
// (RSK_GANONS_BOSS_KEY). One bit over two settings could not express
// keysanity=anywhere with gerudo keys=any-dungeon: the fortress keys would read
// as roaming while OoT's fill still confined them, which is the unsound
// direction.
#define RSBS_FILL_ARM_SOULS 0x00000001u           /* == RSBS_GIVECAP_SOULS */
#define RSBS_FILL_ARM_OCARINA_BUTTONS 0x00000002u /* == RSBS_GIVECAP_OCARINA_BUTTONS */
#define RSBS_FILL_ARM_SWIM 0x00000004u            /* == RSBS_GIVECAP_SWIM */
#define RSBS_FILL_ARM_CLOCKS 0x00000008u          /* == RSBS_GIVECAP_CLOCKS */
#define RSBS_FILL_ARM_GIVECAPS_MASK 0x0000FFFFu
/** OoT dungeon small keys and key rings (the dungeon list's GetSmallKey /
 *  GetKeyRing) are placed by the general pass: RSK_KEYSANITY == ANYWHERE. */
#define RSBS_FILL_ARM_SMALL_KEYS_ROAM 0x00010000u
/** OoT dungeon boss keys, Ganon's excepted, are placed by the general pass:
 *  RSK_BOSS_KEYSANITY == ANYWHERE. */
#define RSBS_FILL_ARM_BOSS_KEYS_ROAM 0x00020000u
/** OoT maps and compasses are placed by the general pass:
 *  RSK_SHUFFLE_MAPANDCOMPASS == ANYWHERE. */
#define RSBS_FILL_ARM_MAPS_ROAM 0x00040000u
/** OoT songs are placed by the general pass: RSK_SHUFFLE_SONGS == ANYWHERE
 *  (song-locations and dungeon-rewards are restricted passes, off is vanilla). */
#define RSBS_FILL_ARM_SONGS_ROAM 0x00080000u
/** OoT skulltula tokens left in the pool are placed by the general pass:
 *  RSK_SHUFFLE_TOKENS != OFF. The setting decides how many copies enter the pool
 *  (the rest are fixed placements, 3drando/item_pool.cpp), never a restricted
 *  pass, so any shuffled value arms it. */
#define RSBS_FILL_ARM_TOKENS_ROAM 0x00100000u
/** OoT dungeon rewards are placed by the general pass:
 *  RSK_SHUFFLE_DUNGEON_REWARDS == ANYWHERE. */
#define RSBS_FILL_ARM_REWARDS_ROAM 0x00200000u
/** A per-world goal quantity (triforce pieces). No frozen record arms it this
 *  increment: whether a goal counter may cross is an undecided design question,
 *  not a setting, so such rows never cross. */
#define RSBS_FILL_ARM_WORLD_EVENT 0x00400000u
/** A shop's own stock row (OoT RG_BUY_*): placed only into shop slots, and no
 *  setting lets it roam, so nothing arms it. */
#define RSBS_FILL_ARM_SHOP_STOCK 0x00800000u
/** OoT's Gerudo Fortress small keys and key ring are placed by the general pass:
 *  RSK_GERUDO_KEYS == ANYWHERE (any-dungeon / overworld are restricted passes,
 *  vanilla is the carpenters' fixed placement). */
#define RSBS_FILL_ARM_GERUDO_KEYS_ROAM 0x01000000u
/** OoT's Ganon's Castle boss key is placed by the general pass:
 *  RSK_GANONS_BOSS_KEY == ANYWHERE (own-dungeon, any-dungeon and overworld are
 *  restricted passes; vanilla, start-with and every LACS / token value are fixed
 *  placements). */
#define RSBS_FILL_ARM_GANON_BOSS_KEY_ROAM 0x02000000u

/** One source row. */
typedef struct {
    uint8_t fillClass;  /* RSBS_FILL_CLASS_* */
    uint32_t armedBy;   /* RSBS_FILL_ARM_* the item needs armed to roam; 0 = unconditional */
    uint8_t sharedKind; /* the RSBS_SHARED_RES_* kind this item's give feeds (context.h); 0 = none */
} ComboItemClassRow;

/** Shared kinds the owner reconciles: [1, RSBS_ITEM_CLASS_SHARED_KIND_CAP). A row
 *  tagging a kind at or past it is stored as invalid (unclassified). */
#define RSBS_ITEM_CLASS_SHARED_KIND_CAP 32u

/** Classify ONE id of the source's own id space.
 *  @return  1 — a fill item; `*out` holds its row (fillClass never NONE);
 *           0 — no fill item: a gap, a sentinel, or a row no fill ever draws
 *               (OoT's ITEMTYPE_EVENT rows; MM's draw-only aliases). `*out` is
 *               set to { NONE, 0 };
 *          -1 — the source is not READY (its item table is not initialised). */
typedef int (*ComboItemClassifyFn)(uint16_t id, ComboItemClassRow* out);

/** 2 (#731): the row gained `sharedKind`, and the owner's answer became the
 *  RECONCILED class. A version-1 source would leave the new byte zero, which is
 *  silently "feeds no kind", so the number moves and a stale source is refused. */
#define RSBS_ITEM_CLASS_SOURCE_ABI 2u
/** The owner stores rows for ids in [0, RSBS_ITEM_CLASS_ID_CAP); a source whose
 *  id space is wider is refused at registration rather than truncated. */
#define RSBS_ITEM_CLASS_ID_CAP 1024u

/** What a game registers. `idSpace` is the EXCLUSIVE bound of its id space
 *  (RG_MAX / RI_MAX). */
typedef struct {
    uint32_t abiVersion; /* RSBS_ITEM_CLASS_SOURCE_ABI */
    uint16_t idSpace;
    ComboItemClassifyFn classify;
} ComboItemClassSource;

/**
 * Register @p source as THE classification source for @p originGame. Called once
 * per game from a file-scope registrar in its combo-logic engine TU.
 *
 * REFUSED (returns false, logs, and increments the durable-for-the-process
 * refusal count) when: the origin is not a real game; @p source is NULL; the ABI,
 * the classify pointer or the id space is invalid; or the origin ALREADY HAS a
 * source — a second registration never replaces the first (see the section
 * header). There is no production way to remove a source: NULL is refused, not
 * an un-registration, so "un-register, then register another" cannot launder a
 * replacement past the refusal.
 */
bool Combo_RegisterItemClassSource(uint8_t originGame, const ComboItemClassSource* source);

/**
 * TEST-ONLY: remove @p originGame's source and discard its built table, so a lock
 * can install a synthetic source and then restore the real one. Every call logs a
 * line naming the origin, and increments the counter
 * Combo_ItemClassUnregistrations() reads, so a replacement done through this door
 * is never silent. Only src/common/tests/ calls it.
 * @return true when a source was removed, false when the origin had none or is
 *         not a real game.
 */
bool Combo_TestUnregisterItemClassSource(uint8_t originGame);

/** Number of Combo_TestUnregisterItemClassSource removals this process. */
uint32_t Combo_ItemClassUnregistrations(void);

/** The registered source for @p originGame, or NULL. */
const ComboItemClassSource* Combo_GetItemClassSource(uint8_t originGame);

/** Number of refused registrations this process (read-only; locks). */
uint32_t Combo_ItemClassRefusedRegistrations(void);

/**
 * Build (or return the already-built) owner table for @p originGame by walking
 * its source once over [0, idSpace).
 * @return the number of FILL ITEMS stored (>= 0); -1 when there is no source or
 *         the source is not ready (nothing is cached — a later call retries).
 */
int Combo_ItemClassBuild(uint8_t originGame);

/** THE OWNER'S ANSWER: @p item's fill class, from the stored table (built on
 *  first use), RECONCILED across origins when the item feeds a shared kind (see
 *  "ONE CLASS PER CROSS-GAME SHARED QUANTITY" above). RSBS_FILL_CLASS_NONE for an
 *  unknown origin, an id outside the source's id space, a non-fill id, or a source
 *  that is not ready — and, for a row that feeds a shared kind, while ANY
 *  registered origin's table cannot be built yet (a reconciliation over half the
 *  rows would be a different answer later). `flags` is ignored — the class
 *  belongs to (originGame, id). */
uint8_t Combo_ItemClassOf(SharedItem item);

/** The SOURCE's own stored class for @p item, BEFORE shared-kind reconciliation:
 *  what that game's fill predicate said. For locks that check a source against its
 *  own predicate; nothing that decides anything may read it. */
uint8_t Combo_ItemClassSourceOf(SharedItem item);

/** The RSBS_SHARED_RES_* kind @p item feeds (0 = none, or unknown). */
uint8_t Combo_ItemClassSharedKind(SharedItem item);

/** The reconciled class of shared kind @p kind: the highest-ranked class
 *  (PROGRESSION > RENEWABLE > JUNK) over every registered origin's rows feeding
 *  it. RSBS_FILL_CLASS_NONE when no row feeds it, the kind is out of range, or a
 *  registered table cannot be built yet. */
uint8_t Combo_ItemClassKindClass(uint8_t kind);

/** How many of @p originGame's fill items feed shared kind @p kind (-1 when the
 *  table cannot be built). */
int Combo_ItemClassKindRows(uint8_t originGame, uint8_t kind);

/** The stored arming conditions of @p item (0 when unconditional or unknown). */
uint32_t Combo_ItemClassArmedBy(SharedItem item);

/** How many fill items of @p originGame's table have RECONCILED class
 *  @p fillClass (-1 when the table cannot be built). */
int Combo_ItemClassCount(uint8_t originGame, uint8_t fillClass);

/**
 * Re-walk @p originGame's source and count the ids whose (class, armedBy,
 * sharedKind) differ from the stored row — the SOURCE's answers, before
 * reconciliation — including an id the source now calls a fill item that
 * the table does not hold, or the reverse.
 * @return 0 when source and owner agree; > 0 the number of diverging ids; -1 when
 *         the table cannot be built or the source became not ready.
 */
int Combo_ItemClassVerify(uint8_t originGame);

/** A class's name ("progression", "junk", "renewable", "trap", "(none)"), or
 *  "(unknown)" past the table. Never NULL. */
const char* Combo_ItemClassName(uint8_t fillClass);

/**
 * THE SETTINGS-CONDITIONAL PREDICATE: may @p item enter the union bag and cross
 * games in a world whose frozen settings arm @p armed (an OR of RSBS_FILL_ARM_*)?
 *
 * True only for an item whose RECONCILED class (Combo_ItemClassOf) is PROGRESSION
 * and whose every arming condition is in @p armed. Never for junk or renewable
 * (criterion 2: junk is what a foreign host degrades to; a renewable is a #525
 * shared quantity nothing logical reads) and never for a trap (see the section
 * header). A shared-kind row whose own game calls it filler but whose kind
 * reconciles to PROGRESSION (MM's bombchus, double defense, hearts) answers as
 * progression here too — one quantity, one answer. A pure function of the stored
 * rows and its argument.
 */
bool Combo_ItemClassMayCrossUnder(SharedItem item, uint32_t armed);

/**
 * The armed set a FROZEN world publishes for crossings originating in
 * @p originGame, as far as this tree publishes one today: the give-capability
 * bits of that game's frozen profile when published (foreign_items.h,
 * Combo_ForeignGiveCaps), and NOTHING for the confinement families — no game
 * publishes its keysanity / song / token / reward confinement to src/common yet,
 * so every row that needs one reads as unarmed, which is the conservative answer
 * (the item stays in its own game). Publishing those is the wiring lane's job,
 * not a guess this function makes.
 */
uint32_t Combo_ItemClassArmedFromFrozen(uint8_t originGame);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_SHARED_ITEMS_H
