/**
 * @file context.h
 * @brief Game context and state management for single-executable architecture
 *
 * Manages game state preservation for cross-game switching at room transitions.
 * Also provides read-only access to both games' states for tracking systems.
 *
 * Adapted from combo/src/FrozenState.cpp for single-executable architecture.
 *
 * Design:
 * - Both SaveContexts always in memory (N64 games fit in cache: ~5KB + ~18KB)
 * - Zero-padded on startup so trackers always have valid memory to read
 * - HasFrozenState() distinguishes first-switch (fresh init) from restore
 * - FreezeState() called before switch, RestoreState() after
 */

#ifndef RSBS_COMMON_CONTEXT_H
#define RSBS_COMMON_CONTEXT_H

#include "game.h"

#ifdef __cplusplus
// For the SharedItem raw-assignment static_asserts below. Included BEFORE the
// extern "C" block on purpose: wrapping a std header in C linkage is ill-formed.
#include <type_traits>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Dual-language static_assert: context.h is compiled as both C and C++ (that
// is also why the shared types below are plain structs — see SharedItem).
#ifdef __cplusplus
#define RSBS_CTX_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define RSBS_CTX_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

/**
 * Initialize the frozen state manager
 * Should be called once at startup
 */
void Context_InitFrozenStates(void);

/**
 * Freeze current game state before switching
 * @param game Which game's state to freeze
 * @param returnEntrance Where to spawn when returning
 * @param saveContext Pointer to the game's SaveContext
 * @param size Size of the SaveContext
 */
void Context_FreezeState(GameId game, uint16_t returnEntrance,
                         const void* saveContext, size_t size);

/**
 * Restore frozen state when returning to a game
 * @param game Which game's state to restore
 * @param saveContext Pointer to the game's SaveContext (will be written to)
 * @param size Size of the SaveContext buffer
 * @return 1 if state was restored, 0 if no frozen state (first switch)
 */
int Context_RestoreState(GameId game, void* saveContext, size_t size);

/**
 * Check if a game has been frozen at least once
 * @return 1 if frozen state exists, 0 if first switch
 */
int Context_HasFrozenState(GameId game);

/**
 * Get return entrance for a frozen game
 */
uint16_t Context_GetFrozenReturnEntrance(GameId game);

/**
 * Clear frozen state for a game
 */
void Context_ClearFrozenState(GameId game);

/**
 * Clear all frozen states
 */
void Context_ClearAllFrozenStates(void);

/**
 * Get read-only pointer to OoT SaveContext (for trackers)
 */
const void* Context_GetOoTSaveContext(void);

/**
 * Get read-only pointer to MM SaveContext (for trackers)
 */
const void* Context_GetMMSaveContext(void);

/**
 * Update shadow copy of active game's SaveContext (for trackers)
 */
void Context_UpdateShadowCopy(GameId game, const void* saveContext, size_t size);

/**
 * Arm the ALREADY-RESIDENT shadow bytes for @p game as a frozen blob, so the
 * restore path will hand them to that game's live SaveContext.
 *
 * This is the missing inverse of the freeze machinery. Shadow copy and frozen
 * blob are the same storage; the only thing separating them is hasBeenFrozen,
 * which FreezeState alone used to set. A .redsave Load commits through
 * Context_UpdateShadowCopy, which does not set it, and EVERY consumer that can
 * move a blob into a live gSaveContext gates on it (Context_RestoreState,
 * Combo_ConsumeFrozenState, MM_Game_Resume). So a faithfully loaded save sat in
 * memory byte-exact and structurally unreachable, which is the read-side half
 * of the operator's "MM will be reset after game restart".
 *
 * REFUSES an all-zero blob, returning 0. That refusal is load-bearing: a slot
 * saved before the player ever entered MM has an all-zero Tier-3, and arming it
 * would restore a zeroed SaveContext over the bootstrap file MM's title chain
 * authors — strictly worse than the cold boot it replaces. "No bytes" and
 * "bytes that are all zero" are indistinguishable here, so zero means absent.
 *
 * @param returnEntrance recorded verbatim; this call does not invent one. Where
 *        a resumed game actually spawns is that game's own spawn policy (MM's
 *        owl / new-cycle rules), not a property of arming.
 * @return 1 if the blob was armed, 0 if it was empty or the game is invalid.
 */
int Context_ArmShadowAsFrozen(GameId game, uint16_t returnEntrance);

// ============================================================================
// Combo context
// ============================================================================

#define COMBO_CONTEXT_MAGIC "OoT+MM<3"

/**
 * FIXED on-disk size of the .redsave Tier-1 (ComboContext) record.
 *
 * The serialized record is decoupled from sizeof(ComboContext) on purpose.
 * save.cpp always writes exactly this many bytes — the live struct followed by
 * zero padding — and Load reads the size the header stored, zero-extending a
 * shorter record. That is the same size-field-driven scheme the OoT/MM tiers
 * already use, and it is what lets ComboContext GROW without invalidating every
 * existing .redsave.
 *
 * The growth contract, which Lane A (origin-tagged sharedItems) depends on:
 *   - New fields are APPENDED (or carved out of `reserved`), never inserted
 *     between existing members. Every already-shipped field must keep its
 *     offset, because a short legacy record is loaded as a byte PREFIX of the
 *     current layout.
 *   - sizeof(ComboContext) must stay within this budget. Exceeding it is a
 *     compile-time error (static_assert below), not a silent format break: the
 *     fix is to raise both this constant and RSBS_SAVE_VERSION together.
 */
#define RSBS_COMBO_CONTEXT_RECORD_SIZE 1024u

// ============================================================================
// Origin-tagged shared items (ADR 0002)
// ============================================================================

/**
 * Capacity of ComboContext.sharedItemsTagged. Sized generously ONCE (ADR 0002):
 * 64 entries x 4 bytes take 256 of the original 640 headroom bytes. Growing it
 * later would be legal under the growth contract (carve more of `reserved`)
 * but is format churn we choose to avoid by not starting small.
 */
#define RSBS_SHARED_ITEM_CAP 64u

/**
 * SharedItem.flags bit: the ORIGIN game has redeemed (actually awarded) this
 * entry. Lane C's give path records a foreign pickup with flags == 0; the
 * origin game's consumer sets this bit when it hands the item to the player,
 * so a round trip can never award the same entry twice. All other bits are
 * reserved and must keep the "0 == unset" property when assigned, because a
 * zero-extended legacy record reads every flag as 0.
 */
#define RSBS_SHARED_ITEM_REDEEMED 0x01u

/**
 * SharedItem.flags bit: this entry was recorded by an identified SOURCE
 * through Combo_SubmitSourcedGrant (ADR 0005), not by an in-process producer.
 * The bit keeps the two producer classes' idempotency domains disjoint:
 * Combo_RecordSharedItem's content de-dup skips sourced entries, so a local
 * pickup can never be silently merged into (and lost against) a peer's gift
 * of the same item. Zero == unset holds: every legacy entry was recorded
 * in-process, which is exactly what a zero bit means.
 */
#define RSBS_SHARED_ITEM_SOURCED 0x02u

/**
 * Capacity of ComboContext.foreignPlacements (Lane C1, #392): how many MM
 * checks can host a foreign item at once. The MVP pins ~4 OoT progression
 * items into MM checks, so 8 is generous for the shipped pool.
 *
 * DO NOT BUMP THIS CONSTANT to get more capacity. grantCursors (ADR 0005) and
 * sharedItemOverflowCount are carved immediately after foreignPlacements, so
 * widening the array in place moves both of them (and anything carved after)
 * off the offset every shipped .redsave stored them at. That break has no
 * runtime signal whatsoever: Load's only content check is the inner
 * COMBO_CONTEXT_MAGIC at offset 0, which survives any tail shift, and the CRC
 * passes because the bytes are unchanged - only their interpretation moved.
 * Per ADR 0005 section 1 a shifted cursor means either re-accepting duplicate
 * grants or permanently refusing legitimate ones.
 *
 * The offset asserts below are written as LITERAL byte offsets (672, 736)
 * precisely so that a bump breaks the build. An earlier revision expressed
 * them in terms of this constant, which made them follow the bump instead of
 * catching it - a widen from 8 to 40 compiled completely clean and silently
 * orphaned every shipped save's grant cursors.
 *
 * To add capacity, carve a SECOND placement block from the front of reserved[]
 * (ComboForeignPlacement foreignPlacementsExt[N], declared immediately before
 * reserved[]) and span BOTH blocks in every accessor in foreign_items.c - the
 * duplicate scan and the first-free scan must each cover the whole logical
 * array in one pass, or a cross-block duplicate escapes. Append-only, prior
 * offsets fixed, zero still unset. Same prescription as RSBS_GRANT_SOURCE_CAP
 * below; see ADR 0005 section 1 for the identical hazard on the cursor array.
 */
#define RSBS_FOREIGN_PLACEMENT_CAP 8u

/**
 * Capacity of ComboContext.grantCursors (ADR 0005, netplay 1a #460): how many
 * distinct GRANT SOURCES can hold a delivery cursor at once. A source is one
 * remote authority feeding Combo_SubmitSourcedGrant — an Archipelago server
 * counts as ONE source regardless of room size, and a P2P co-op session uses
 * one source per peer, so 8 is generous for every planned topology.
 *
 * DO NOT BUMP THIS CONSTANT to get more capacity. sharedItemOverflowCount is
 * carved immediately after grantCursors, so widening the array in place moves
 * that field (and anything carved after it) off the offset every shipped
 * .redsave stored it at — a format break that the static asserts below CANNOT
 * catch, because they pin the offset relative to RSBS_GRANT_SOURCE_CAP and so
 * simply follow the bump. To add capacity, carve a SECOND cursor block from
 * the front of reserved[] and consult both in the lookup: append-only, prior
 * offsets fixed, zero still unset. See ADR 0005 §1.
 */
#define RSBS_GRANT_SOURCE_CAP 8u

/**
 * Capacity of ComboContext.sharedResources (#525): how many distinct SHARED
 * RESOURCES can be tracked at once. A shared resource is not an item that
 * crosses once — it is ONE QUANTITY spanning both games, capacity and current
 * value together ("current health is tracked as if OoT and MM were one game
 * with a single health bar"). v1 occupies five of the eight slots (rupees,
 * wallet tier, health quarters, current health, double defense); magic and the
 * ammo upgrades are the queued members of the same class.
 *
 * DO NOT BUMP THIS CONSTANT to get more capacity, for the reason
 * RSBS_GRANT_SOURCE_CAP spells out: the array is carved from the front of
 * reserved[] and anything carved after it inherits its position, so a widen in
 * place moves those fields off the offset every shipped .redsave stored them
 * at. Carve a second block from the front of reserved[] and span both in every
 * accessor instead. See ADR 0005 §1 for the identical hazard.
 */
#define RSBS_SHARED_RESOURCE_CAP 8u

/**
 * One cross-game item, tagged with the game whose id-space `id` belongs to.
 *
 * Why a struct and not a packed integer (ADR 0002): OoT's RandomizerGet (RG_*)
 * and MM's RandoItemId (RI_*) are unrelated enumerations that must never alias
 * by raw integer. A game tag bit-packed into a uint16_t would make an untagged
 * read *almost* work — exactly how the #356 entrance-id leak behaved. As a
 * plain struct, assigning a raw integer into a SharedItem is ill-formed in
 * BOTH C and C++ (neither language converts arithmetic types to struct types),
 * so an untagged id cannot cross this boundary by accident. The static_asserts
 * below lock the layout — these bytes are .redsave format — and, on the C++
 * side, prove the raw-assignment rejection.
 *
 * Zero means unset for every member (growth contract): a slot is occupied iff
 * originGame != GAME_NONE. There is deliberately NO separate count field — a
 * count would be a second source of truth that a zero-extended legacy record
 * could contradict.
 */
typedef struct {
    uint8_t originGame;  // GameId that owns the id-space; GAME_NONE (0) = empty slot
    uint8_t flags;       // RSBS_SHARED_ITEM_* bits; 0 = default (present, not redeemed)
    uint16_t id;         // RG_* if originGame==GAME_OOT, RI_* if GAME_MM; 0 when empty
} SharedItem;

RSBS_CTX_STATIC_ASSERT(sizeof(SharedItem) == 4,
                       "SharedItem is serialized raw inside the .redsave Tier-1 record; its layout is format");
RSBS_CTX_STATIC_ASSERT(offsetof(SharedItem, originGame) == 0 && offsetof(SharedItem, flags) == 1 &&
                           offsetof(SharedItem, id) == 2,
                       "SharedItem member offsets are .redsave format and must not move");

/**
 * One cross-game item PLACEMENT: an MM check that hosts an item from another
 * game's id-space (Lane C1, #392; MVP direction is OoT items in MM checks).
 *
 * Why this lives in gComboCtx and not in MM's own rando save table (ADR 0002):
 * MM's RandoSaveCheckInfo.randoItemId is an MM RandoItemId — a raw RG_* stored
 * there would alias MM's id-space, the #356 bug class. The MM save table keeps
 * a legal junk-class MM item at the hosting check; THIS table, keyed by the MM
 * RandoCheckId, is what says "that check actually yields a foreign item", with
 * the item carried as an origin-tagged SharedItem at every boundary.
 *
 * Zero means unset for every member (growth contract): a slot is occupied iff
 * item.originGame != GAME_NONE (mmCheckId 0 == MM's RC_UNKNOWN, which never
 * hosts anything). A zero-extended legacy record therefore reads as "no
 * foreign placements", which is correct: pre-C1 worlds have none.
 */
typedef struct {
    uint16_t mmCheckId; // MM RandoCheckId hosting the foreign item; RC_UNKNOWN (0) when empty
    SharedItem item;    // the foreign item, origin-tagged; originGame == GAME_NONE marks an empty slot
} ComboForeignPlacement;

RSBS_CTX_STATIC_ASSERT(sizeof(ComboForeignPlacement) == 6,
                       "ComboForeignPlacement is serialized raw inside the .redsave Tier-1 record; its layout is "
                       "format");
RSBS_CTX_STATIC_ASSERT(offsetof(ComboForeignPlacement, mmCheckId) == 0 && offsetof(ComboForeignPlacement, item) == 2,
                       "ComboForeignPlacement member offsets are .redsave format and must not move");

/**
 * One grant source's delivery cursor (ADR 0005, netplay 1a #460).
 *
 * A SOURCE is a producer of item grants with its own identity and its own
 * monotonic sequence numbering — an in-process cross-game hand-off is NOT a
 * source (it keeps using Combo_RecordSharedItem's content de-dup); a network
 * peer or an Archipelago server is. `lastSeq` is the highest sequence number
 * this save has ACCEPTED from the source: a retransmitted grant (seq <=
 * lastSeq) is decidably a duplicate, a fresh grant (seq == lastSeq + 1) is
 * decidably new — even when both carry the same (originGame, id). Persisting
 * the cursor in the .redsave is what keeps that decision correct across a
 * save/load: a reload can never re-open a duplicate-delivery window.
 *
 * Zero means unset for every member (growth contract): a slot is occupied iff
 * sourceKey != 0, and an occupied slot always has lastSeq >= 1 (a cursor is
 * only created by accepting seq 1). A zero-extended legacy record therefore
 * reads as "no sources have ever delivered", which is correct.
 */
typedef struct {
    uint32_t sourceKey; // transport-assigned nonzero source identity; 0 = empty slot
    uint32_t lastSeq;   // highest accepted sequence number from this source; 0 = none
} ComboGrantSourceCursor;

RSBS_CTX_STATIC_ASSERT(sizeof(ComboGrantSourceCursor) == 8,
                       "ComboGrantSourceCursor is serialized raw inside the .redsave Tier-1 record; its layout is "
                       "format");
RSBS_CTX_STATIC_ASSERT(offsetof(ComboGrantSourceCursor, sourceKey) == 0 &&
                           offsetof(ComboGrantSourceCursor, lastSeq) == 4,
                       "ComboGrantSourceCursor member offsets are .redsave format and must not move");

/**
 * Which shared resource a ComboSharedResource slot holds (#525). Zero is the
 * EMPTY marker, never a real resource — occupancy rides this tag exactly as
 * SharedItem's occupancy rides originGame != GAME_NONE.
 *
 * WHY THE TAG EXISTS AT ALL, rather than a bare `uint16_t sharedRupees` field.
 * The growth contract for everything carved from reserved[] is "zero means
 * unset" (see the reserved[] comment below), and 0 rupees is a perfectly legal
 * player state. A bare scalar therefore cannot distinguish "this save predates
 * shared resources" from "this player is broke" — and getting that wrong
 * resurrects a stale balance on a legacy record, or discards a real zero. With
 * the kind tag, a zero-extended legacy record reads as eight EMPTY slots, which
 * is unambiguous and correct.
 *
 * These values are .redsave format. Append only; never renumber.
 */
enum {
    RSBS_SHARED_RES_NONE = 0,             // empty slot (the growth contract's "unset")
    RSBS_SHARED_RES_RUPEES = 1,           // CONSUMABLE: the single shared rupee pool
    RSBS_SHARED_RES_WALLET_TIER = 2,      // MONOTONIC: wallet upgrade level (both games clamp against their own)
    RSBS_SHARED_RES_HEALTH_QUARTERS = 3,  // MONOTONIC: capacity + pieces*4, the canonical heart quantity
    RSBS_SHARED_RES_HEALTH_CURRENT = 4,   // CONSUMABLE: the single shared health bar, in 0x10-per-heart units
    RSBS_SHARED_RES_DOUBLE_DEFENSE = 5,   // MONOTONIC: 0/1 flag; each game sets its OWN differently-named field pair
};

/**
 * Merge discipline for a shared-resource slot. Set on the slot at first write
 * and stable thereafter, so a reader can tell the two disciplines apart without
 * a switch on `kind` — the byte is descriptive, `kind` is authoritative.
 */
#define RSBS_SHARED_RES_F_MONOTONIC 0x01u // max-merge both ways; decay is impossible by construction

/**
 * One shared cross-game resource (#525): a single quantity that spans both
 * games, harvested from the departing game at suspend and applied to the
 * arriving game at its startup entrance.
 *
 * TWO MERGE DISCIPLINES, and picking the wrong one is a correctness bug:
 *
 *   - MONOTONIC (wallet tier, health quarters, double defense) — a capacity
 *     that only ever grows. Harvest is `shared = max(shared, live)`, apply is
 *     `live = max(live, shared)`. Idempotent; decay impossible.
 *   - CONSUMABLE (rupee count, current health) — a quantity the player spends.
 *     Harvest takes a DELTA against a RAM-only watermark recorded at apply
 *     time, never a raw copy. That watermark is mandatory, not an optimization:
 *     MM's tier-3 wallet holds 500 and OoT's holds 999, so a naive
 *     `shared = live` copy clamps an 800-rupee arrival to 500 and harvests 500
 *     back, silently costing the player 300 rupees on EVERY round trip.
 *
 * Zero means unset for every member (growth contract): a slot is occupied iff
 * kind != RSBS_SHARED_RES_NONE. There is deliberately no count field — a count
 * would be a second source of truth a zero-extended legacy record contradicts.
 */
typedef struct {
    uint8_t kind;   // RSBS_SHARED_RES_*; RSBS_SHARED_RES_NONE (0) = empty slot
    uint8_t flags;  // RSBS_SHARED_RES_F_* bits; 0 = consumable (delta-harvested)
    uint16_t value; // the shared quantity, in that resource's own units; 0 when empty
} ComboSharedResource;

RSBS_CTX_STATIC_ASSERT(sizeof(ComboSharedResource) == 4,
                       "ComboSharedResource is serialized raw inside the .redsave Tier-1 record; its layout is format");
RSBS_CTX_STATIC_ASSERT(offsetof(ComboSharedResource, kind) == 0 && offsetof(ComboSharedResource, flags) == 1 &&
                           offsetof(ComboSharedResource, value) == 2,
                       "ComboSharedResource member offsets are .redsave format and must not move");

typedef struct {
    char magic[8];        // "OoT+MM<3"
    uint32_t version;
    bool switchRequested;
    GameId targetGame;
    uint16_t targetEntrance;
    GameId sourceGame;
    uint16_t sourceEntrance;
    // KEPT (ADR 0002): origin-neutral event bits, so the shape has no id-space
    // aliasing hazard. Still unwired; which bit means what is assigned by Lane
    // A1 and later — until something writes a bit, every word stays zero.
    uint32_t sharedFlags[64];
    // RETIRED IN PLACE (ADR 0002): never wired — zero non-test producers or
    // consumers ever shipped, and the un-tagged uint16_t shape is exactly the
    // raw-integer aliasing hazard the ADR exists to prevent. The bytes stay at
    // this shipped offset because a legacy .redsave loads as a byte PREFIX of
    // this layout. Do not wire; use sharedItemsTagged below instead.
    uint16_t sharedItems[32];
    // RETIRED IN PLACE (ADR 0002): dead plumbing. ComboContext_Init still sets
    // -1 and it still serializes, but nothing ever assigned the active slot
    // index. Kept at its shipped offset; do not wire.
    int32_t saveSlot;

    // Cross-game rando state propagation — Lane B's carrier (ADR 0002): set in
    // the LIVE path at OoT generation time, read by MM's consumption path when
    // Lane C makes it reachable.
    bool sourceIsRando;        // Source game is in randomizer mode
    uint32_t sharedRandoSeed;  // Shared seed for synchronization

    // Origin-tagged cross-game items (ADR 0002), carved from the FRONT of the
    // original reserved[640] headroom so every field above keeps its shipped
    // offset. All-zero = every slot unset (originGame == GAME_NONE), which is
    // exactly what a zero-extended legacy record must read as.
    SharedItem sharedItemsTagged[RSBS_SHARED_ITEM_CAP];

    // Lane B settings digest (ADR 0002 §3: "If B needs a settings digest beyond
    // the u32 seed, carve it from reserved under the growth contract"). This is
    // the FNV-1a hash of OoT's finalized rando settings string — the second half
    // of the "one seed + one pinned settings profile" reproducibility contract:
    // sharedRandoSeed alone does NOT determine the fill, because Playthrough_Init
    // re-seeds the RNG with Hash(seed + settings-string) before placing items, so
    // the same numeric seed reproduces a world only under the same settings. MM
    // (Lane C) reads this to VERIFY both games were generated under the agreed
    // pinned profile before pairing; a mismatch means the worlds do not pair.
    // Seed-independent (a fixed profile hashes the same across seeds) and build-
    // independent (computed before the DontGenerateSpoiler build-version mixing).
    // Zero == unset: a non-rando or zero-extended legacy record reads 0, which
    // MM treats as "no profile recorded" — the growth contract's required
    // meaning of zero for every carved field. Carved from the FRONT of the old
    // reserved[384] (its offset is exactly where reserved used to begin), so
    // every field above keeps its shipped offset and the record size is
    // unchanged.
    uint32_t sharedRandoSettingsHash;

    // Lane C1 (#392): cross-game item placements for the paired world, written
    // by MM's OnFileCreate placement pass when a paired rando world generates,
    // read by MM's give path (which check yields a foreign item) and by both
    // spoiler surfaces. Carved from the FRONT of the old reserved[380] under
    // the growth contract: all-zero = no foreign placements, which is exactly
    // what a zero-extended pre-C1 record must read as. See foreign_items.h for
    // the accessors; raw entries must always carry the origin tag (ADR 0002).
    ComboForeignPlacement foreignPlacements[RSBS_FOREIGN_PLACEMENT_CAP];

    // Netplay 1a (ADR 0005, #460): per-source grant-delivery cursors, written
    // by Combo_SubmitSourcedGrant when it accepts a grant, read to decide
    // retransmit-vs-new. Carved from the FRONT of the old reserved[332] under
    // the growth contract: all-zero = no sources have ever delivered, which is
    // exactly what a zero-extended pre-netplay record must read as. Lives in
    // the SAME Tier-1 record as sharedItemsTagged on purpose: the array and
    // the cursors must be saved, loaded, and invalidated (#440) atomically —
    // a cursor without its items refuses re-delivery of lost grants, items
    // without their cursor re-accept duplicates.
    ComboGrantSourceCursor grantCursors[RSBS_GRANT_SOURCE_CAP];

    // Netplay 1a (ADR 0005, #460): durable count of shared-item records
    // REFUSED for capacity (array full even after reclaiming redeemed slots).
    // The anti-silent-loss signal: for the in-process producer a refusal is a
    // genuinely lost item; for a sourced grant it marks backpressure (the
    // grant stays owed by the source because its cursor did not advance).
    // Zero == unset (no refusal has ever happened), per the growth contract.
    uint32_t sharedItemOverflowCount;

    // Phase 3.1 (#493): the REVERSE direction's placement table — OoT checks
    // hosting MM items. Carved from the FRONT of the old reserved[264] under
    // the growth contract (ADR 0009 claim 1): all-zero = no reverse
    // placements, which is exactly what a zero-extended pre-3.1 record must
    // read as.
    //
    // READ THE KEY CAREFULLY. This reuses ComboForeignPlacement, whose member
    // is *named* mmCheckId, but in THIS table that u16 holds an OoT
    // RandomizerCheck (RC_MAX fits a u16). The struct is reused rather than
    // reshaped because its size and member offsets are static_asserted
    // .redsave format and cannot gain a hostGame byte in place; giving the
    // reverse direction its own 8-byte tagged struct would force the record
    // size and RSBS_SAVE_VERSION up plus a migration hook that does not exist
    // (save.cpp's Tier-1 parse is one flat memcpy). See ADR 0009 decision 3.
    //
    // The two tables are SEPARATE KEY SPACES, not one array split in two: an
    // OoT RC and an MM RC are unrelated enumerations that collide freely as
    // raw u16s, which is precisely why a single table with no host
    // discriminator would false-positive across directions. Never look one up
    // with the other's accessor. The direction IS the accessor.
    ComboForeignPlacement foreignPlacementsOoT[RSBS_FOREIGN_PLACEMENT_CAP];

    // Phase 3.1 Lane 4 (#497 step 7 / #499 step 4): the digest of the MM
    // randomizer option profile the paired world was generated under. ADR 0009
    // claim 3, size CONFIRMED at 4 bytes — a u32 by analogy with
    // sharedRandoSettingsHash, not a record of the options themselves (ADR 0009
    // decision 1: CVars author, a digest identifies).
    //
    // WHY IT HAS TO EXIST AT ALL. sharedRandoSettingsHash fingerprints *OoT's*
    // settings string only (playthrough.cpp). Once the MM profile is a player
    // CHOICE rather than the StaticData defaults falling through, two peers can
    // agree on sharedRandoSeed AND sharedRandoSettingsHash and still generate
    // different MM halves — with nothing able to detect it. This field is the
    // missing term: the paired world's identity now covers both halves.
    //
    // Computed by Rando::Foreign::ResolvePairedProfile() from the SAME string
    // MixPairedFinalSeed() hashes, so "changing an MM option changes the derived
    // MM world" and "changing an MM option changes the recorded identity" cannot
    // drift apart — they are one input.
    //
    // Zero == unset (no paired profile has been resolved), the growth contract's
    // required meaning: a non-rando or zero-extended legacy record reads 0.
    // Carved from the FRONT of the old reserved[216] under that contract, so
    // every field above keeps its shipped offset and the record size is
    // unchanged.
    uint32_t mmProfileDigest;

    // #525: the shared cross-game RESOURCES — one quantity spanning both games,
    // capacity and current value together, as opposed to sharedItemsTagged's
    // one-way single-use crossings. Carved from the FRONT of the old
    // reserved[212] under the growth contract: all-zero = every slot EMPTY,
    // which is exactly what a zero-extended pre-#525 record must read as.
    //
    // The kind tag is what makes that work, and it is the whole reason this is
    // an array of tagged slots rather than a handful of bare scalars: 0 rupees
    // is a legal player state, so a bare `uint16_t sharedRupees` could not tell
    // a legacy record from a broke player. See ComboSharedResource.
    //
    // Occupancy and merge discipline are the struct's business; shared_resources.c
    // owns every read and write. Nothing here is a per-game mirror — there is
    // ONE value, and each game's harvest/apply shim reconciles its own live
    // gSaveContext against it at the switch boundary.
    ComboSharedResource sharedResources[RSBS_SHARED_RESOURCE_CAP];

    // Headroom. Carve new fields from the FRONT of this array (as
    // sharedItemsTagged, sharedRandoSettingsHash, foreignPlacements, the
    // grant cursors, and the reverse placement table were) so the struct
    // stays inside
    // RSBS_COMBO_CONTEXT_RECORD_SIZE; the on-disk record size does not change
    // either way, so old saves keep loading. Zeroed by ComboContext_Init,
    // which is what makes a zero-extended legacy record indistinguishable from
    // a freshly-initialized one — every field carved from here must keep
    // "zero means unset".
    //
    // 264 - 48 (foreignPlacementsOoT) - 4 (mmProfileDigest) - 32 (sharedResources).
    // ADR 0009 publishes the remaining
    // allocation across the other claimants and sets a 64-byte floor:
    // Test_SaveComboRecordFixed's scribble loop iterates sizeof(reserved), so
    // at zero it degenerates to zero iterations and passes vacuously, retiring
    // the only test that proves headroom round-trips at all.
    uint8_t reserved[180];
} ComboContext;

/**
 * The Tier-1 byte length PRE-CARVE builds (Phase 2 headroom fix #399 through
 * the ADR 0002 carve) populated meaningfully: every field up to, but not
 * including, the then-reserved[640] headroom. Test_SaveComboLegacyRecord
 * crafts its "legacy" record at exactly this length.
 *
 * This is a pinned NUMBER, deliberately not offsetof(ComboContext, reserved):
 * carving fields out of `reserved` moves that offsetof forward, which would
 * silently redefine "legacy" to include the new fields and leave the true
 * shipped pre-carve prefix untested. The static_asserts below tie the number
 * to the live layout so neither can drift without a build break.
 */
#define RSBS_COMBO_CONTEXT_PRECARVE_SIZE 364u

// Deliberately `<=`, not `==`: the on-disk record is padded to a fixed size, so
// the struct only has to FIT the budget. An exact-match assert would turn a
// harmless ABI padding difference into a build break for no benefit.
RSBS_CTX_STATIC_ASSERT(sizeof(ComboContext) <= RSBS_COMBO_CONTEXT_RECORD_SIZE,
                       "ComboContext outgrew its .redsave Tier-1 record budget; raise "
                       "RSBS_COMBO_CONTEXT_RECORD_SIZE and RSBS_SAVE_VERSION together");
// The carve must begin exactly where the pre-carve reserved[] began. If this
// fires, either a field was INSERTED before sharedItemsTagged (which breaks
// the byte-prefix property every shipped .redsave relies on) or the compiler
// laid the struct out differently than every build that ever wrote a save.
RSBS_CTX_STATIC_ASSERT(offsetof(ComboContext, sharedItemsTagged) == RSBS_COMBO_CONTEXT_PRECARVE_SIZE,
                       "sharedItemsTagged must sit exactly at the pre-carve reserved[] offset; "
                       "moving it orphans every shipped .redsave");
// sharedRandoSettingsHash is the first field carved from the front of the old
// reserved[]; it must sit immediately after the tagged-item array with no gap,
// so it occupies bytes that shipped .redsaves stored as zero (unset).
RSBS_CTX_STATIC_ASSERT(offsetof(ComboContext, sharedRandoSettingsHash) ==
                           RSBS_COMBO_CONTEXT_PRECARVE_SIZE + RSBS_SHARED_ITEM_CAP * sizeof(SharedItem),
                       "sharedRandoSettingsHash must be carved from the FRONT of the old reserved[] "
                       "(contiguous with the tagged-item array); moving it changes .redsave format");
// foreignPlacements is the next carve from the front of the old reserved[]:
// it must sit immediately after the settings digest with no gap, occupying
// bytes every shipped .redsave stored as zero (unset).
RSBS_CTX_STATIC_ASSERT(offsetof(ComboContext, foreignPlacements) ==
                           RSBS_COMBO_CONTEXT_PRECARVE_SIZE + RSBS_SHARED_ITEM_CAP * sizeof(SharedItem) +
                               sizeof(uint32_t),
                       "foreignPlacements must be carved from the FRONT of the old reserved[] "
                       "(contiguous with the settings digest); moving it changes .redsave format");
// grantCursors and sharedItemOverflowCount are pinned to LITERAL byte offsets,
// deliberately not to expressions in RSBS_FOREIGN_PLACEMENT_CAP. Both fields
// are carved AFTER foreignPlacements, so an in-place widen of that array moves
// them; an assert phrased in terms of the cap moves with the bump and never
// fires, which is exactly the silent .redsave break #490 describes. Written as
// integers, the same bump is a build error. ADR 0005 names 672 explicitly.
//
// If either literal ever has to change, the field genuinely moved and every
// shipped .redsave is being reinterpreted: that is a format generation, not an
// assert edit. Raise RSBS_SAVE_VERSION and write the migration first.
RSBS_CTX_STATIC_ASSERT(offsetof(ComboContext, grantCursors) == 672u,
                       "grantCursors lives at .redsave byte offset 672 in every shipped save; if this "
                       "fires, a field before it grew in place - carve from reserved[] instead");
RSBS_CTX_STATIC_ASSERT(offsetof(ComboContext, sharedItemOverflowCount) == 736u,
                       "sharedItemOverflowCount lives at .redsave byte offset 736 in every shipped "
                       "save; if this fires, a field before it grew in place - carve from reserved[] "
                       "instead");
// Kept alongside the literals: this one states the CONTIGUITY intent (no gap,
// no field slipped between the placement table and the cursors) that a bare
// integer does not express. The literals catch an in-place widen; this catches
// a field inserted between them.
RSBS_CTX_STATIC_ASSERT(offsetof(ComboContext, sharedItemOverflowCount) ==
                           offsetof(ComboContext, grantCursors) +
                               RSBS_GRANT_SOURCE_CAP * sizeof(ComboGrantSourceCursor),
                       "sharedItemOverflowCount must sit immediately after the grant cursors; moving "
                       "it changes .redsave format");
// The reverse placement table is the next carve from the front of reserved[]:
// it must sit immediately after the overflow count with no gap, occupying
// bytes every shipped .redsave stored as zero (unset). Pinned to the literal
// 740 for the same reason 672 and 736 are — anything carved after it (ADR
// 0009's claims 2 through 4) inherits its position, so a field growing in
// place ahead of it must break the build rather than slide them.
RSBS_CTX_STATIC_ASSERT(offsetof(ComboContext, foreignPlacementsOoT) == 740u,
                       "foreignPlacementsOoT lives at .redsave byte offset 740; if this fires, a "
                       "field before it grew in place - carve from reserved[] instead");
RSBS_CTX_STATIC_ASSERT(offsetof(ComboContext, foreignPlacementsOoT) ==
                           offsetof(ComboContext, sharedItemOverflowCount) + sizeof(uint32_t),
                       "foreignPlacementsOoT must be carved from the FRONT of reserved[] (contiguous "
                       "with the overflow count); moving it changes .redsave format");
// The MM option-profile digest is the next carve (ADR 0009 claim 3, 4 bytes).
// Pinned to the literal 788 for the same reason 672, 736 and 740 are: anything
// carved after it inherits its position, so a field growing in place ahead of
// it must break the build rather than slide it.
RSBS_CTX_STATIC_ASSERT(offsetof(ComboContext, mmProfileDigest) == 788u,
                       "mmProfileDigest lives at .redsave byte offset 788; if this fires, a field "
                       "before it grew in place - carve from reserved[] instead");
RSBS_CTX_STATIC_ASSERT(offsetof(ComboContext, mmProfileDigest) ==
                           offsetof(ComboContext, foreignPlacementsOoT) +
                               RSBS_FOREIGN_PLACEMENT_CAP * sizeof(ComboForeignPlacement),
                       "mmProfileDigest must be carved from the FRONT of reserved[] (contiguous with "
                       "the reverse placement table); moving it changes .redsave format");
// The shared-resource slots are the next carve (#525, 32 bytes). Pinned to the
// literal 792 for the same reason 672, 736, 740 and 788 are: anything carved
// after it inherits its position, so a field growing in place ahead of it must
// break the build rather than slide it.
RSBS_CTX_STATIC_ASSERT(offsetof(ComboContext, sharedResources) == 792u,
                       "sharedResources lives at .redsave byte offset 792; if this fires, a field "
                       "before it grew in place - carve from reserved[] instead");
RSBS_CTX_STATIC_ASSERT(offsetof(ComboContext, sharedResources) ==
                           offsetof(ComboContext, mmProfileDigest) + sizeof(uint32_t),
                       "sharedResources must be carved from the FRONT of reserved[] (contiguous with "
                       "the MM profile digest); moving it changes .redsave format");
RSBS_CTX_STATIC_ASSERT(offsetof(ComboContext, reserved) ==
                           offsetof(ComboContext, sharedResources) +
                               RSBS_SHARED_RESOURCE_CAP * sizeof(ComboSharedResource),
                       "the tagged-item array, the settings digest, both foreign-placement tables, "
                       "the grant cursors, the overflow count, the MM profile digest, the shared "
                       "resource slots, and the remaining headroom must stay contiguous (no padding, "
                       "no fields slipped between them)");
// ADR 0009's floor. reserved[] is what Test_SaveComboRecordFixed scribbles to
// prove Tier-1 headroom round-trips; at zero that loop runs zero times and the
// test passes vacuously, so the carve budget stops here rather than there.
RSBS_CTX_STATIC_ASSERT(sizeof(((ComboContext*)0)->reserved) >= 64u,
                       "reserved[] must keep at least 64 bytes: Test_SaveComboRecordFixed's headroom "
                       "round-trip degenerates to a vacuous pass when it empties (ADR 0009)");

#ifdef __cplusplus
// The raw-assignment-fails proof (ADR 0002). In C this is a constraint
// violation by construction — C has no arithmetic-to-struct conversions at all
// — so only the C++ view needs an explicit probe (C++ could later grow a
// converting constructor or assignment operator; these asserts forbid that).
static_assert(!std::is_convertible<int, SharedItem>::value,
              "a raw integer must never implicitly become a SharedItem — tag the origin game");
static_assert(!std::is_assignable<SharedItem&, int>::value && !std::is_assignable<SharedItem&, unsigned>::value,
              "a raw integer must never be assignable into a SharedItem — tag the origin game");
static_assert(!std::is_assignable<SharedItem&, GameId>::value,
              "a bare GameId is not a SharedItem either; populate the struct members explicitly");
static_assert(!std::is_convertible<int, ComboForeignPlacement>::value &&
                  !std::is_assignable<ComboForeignPlacement&, int>::value,
              "a raw integer must never become a foreign placement — the item member carries the origin tag");
static_assert(!std::is_convertible<int, ComboSharedResource>::value &&
                  !std::is_assignable<ComboSharedResource&, int>::value,
              "a raw integer must never become a shared resource — the kind tag is what separates "
              "an unset slot from a legitimate zero");
static_assert(std::is_trivially_copyable<ComboContext>::value,
              "gComboCtx is serialized with memcpy; ComboContext must stay trivially copyable");
#endif

extern ComboContext gComboCtx;
extern GameId gCurrentGame;

void ComboContext_Init(void);
void ComboContext_RequestSwitch(GameId target, uint16_t entrance);
bool ComboContext_IsSwitchPending(void);
void ComboContext_ClearSwitch(void);

// ============================================================================
// High-level context API (for switch.cpp and external use)
// ============================================================================

/**
 * Initialize all context systems (frozen states + combo context)
 */
void Context_Init(void);

/**
 * Request a game switch to the target game at the specified entrance
 */
void Context_RequestSwitch(GameId target, uint16_t entrance);

/**
 * Check if a game switch has been requested
 */
bool Context_HasPendingSwitch(void);

// ============================================================================
// Session invalidation (#440)
// ============================================================================
//
// THE TRANSITION MATRIX. What survives, and what the next first-MM-entry does.
//
// "Pairs?" is MM_Rando_PairOnCrossGameArrival's decision
// (games/mm/2s2h/GameExports_SingleExe.cpp, #447/d075aeaf). It reads exactly
// two things, both of them src/common state: Combo_ForeignPairingActive() and
// whether Combo_ConsumeFrozenState returned a blob. That is why a stale blob
// is not merely cosmetic post-#447 — it answers "this MM save already exists"
// for a save that does not, and the new seed DECLINES TO PAIR. Fixing the
// invalidation restores pairing with no change to that guard.
//
//   transition          | frozen blobs | shadows  | crossings | seed stamp | pairs?
//   --------------------+--------------+----------+-----------+------------+--------
//   cold boot           | none (init)  | zeroed   | none      | none       | n/a until a seed exists
//   reset -> same slot  | DROPPED      | reloaded | reloaded  | reloaded   | yes, from that slot's .redsave
//   reset -> diff slot  | DROPPED      | reloaded | reloaded  | reloaded   | yes, from that slot's .redsave
//   reset -> new file   | DROPPED      | zeroed   | DROPPED   | kept iff   | YES for a rando file (the
//                       |              |          |           | rando file | operator's case); declines
//                       |              |          |           |            | with no-paired-oot-world for
//                       |              |          |           |            | a vanilla file, correctly
//   cross-game arrival  | PRESERVED    | preserved| preserved | preserved  | declines (existing save) —
//                       |              |          |           |            | correct: it IS the player's
//                       |              |          |           |            | own restored MM session
//
// Note the last row: an arrival walks the SAME OoT title chain a soft reset
// does, so Context_InvalidateSessionOnReturnToTitle self-suppresses on a
// pending startup entrance. Without that guard this fix would eat the blob
// every return leg is on its way to consume — a strictly worse bug.
//
// "reset -> same slot" and "reset -> diff slot" are deliberately identical:
// the .redsave is per-OoT-slot but these globals are process singletons, so
// the only safe rule is clear-then-reload for BOTH. Treating "same slot" as a
// fast path that skips the clear is precisely how a slot with no companion
// .redsave would keep inheriting the previous session.

/**
 * What Context_InvalidateSessionState does with the GENERATION-AUTHORED state:
 * the Lane B seed stamp (sourceIsRando / sharedRandoSeed /
 * sharedRandoSettingsHash) and, since #534, the reverse placement table
 * (foreignPlacementsOoT) that generation derives from that stamp.
 *
 * These are the one part of gComboCtx that is NOT authored by the session
 * being torn down: Playthrough_Init writes the stamp at GENERATION time and
 * immediately derives the reverse placements from it (OoT_PlaceForeignItems),
 * which for a new file happens BEFORE the file is created (generate a seed in
 * the menu, then name the file). So the new-file call site must keep what
 * generation just authored, while a return-to-title must drop a stamp that
 * nothing stands behind. Making that an explicit argument rather than a
 * hidden policy means every call site has to state which situation it is in.
 */
typedef enum {
    /**
     * Drop the stamp (and the reverse placement table with it). Correct
     * wherever no generation stands behind it: a soft reset to title, and a
     * NON-rando new file. A surviving stamp is not inert —
     * Combo_ForeignPairingActive() is literally
     * `sourceIsRando && sharedRandoSettingsHash != 0`, so a dead session's
     * stamp makes MM believe a paired world exists for a seed that is gone.
     */
    RSBS_SEED_STAMP_DROP = 0,
    /**
     * Keep the stamp AND the reverse placement table (#534) — they were
     * authored together and are only coherent together. Correct ONLY where
     * generation has already authored them for the file now being created
     * (OoT_Sram_InitSave on a randomizer file). The FORWARD table
     * (foreignPlacements) is dropped even here: MM re-authors it at its own
     * OnFileCreate on the next arrival, so at file-creation time it can only
     * hold a dead session's rows.
     */
    RSBS_SEED_STAMP_KEEP = 1,
} ComboSeedStampPolicy;

/**
 * Retire every trace of the cross-game session that just ended (#440).
 *
 * This is the missing INVERSE of the freeze machinery. Cross-game session state
 * is process-global — the frozen blobs, both shadow copies, and every session
 * field of gComboCtx — and before this existed nothing invalidated any of it on
 * a soft reset or a new game. PR #400 made Combo_ConsumeFrozenState retire the
 * blob it hands over, which stops a blob being consumed TWICE, but a dead
 * session's blob was still sitting there for the FIRST consume of the next
 * session. That is the operator's #440 repro exactly: play a seed on slot 3,
 * soft reset, start a NEW seed on slot 1, walk into the Happy Mask Shop, and
 * MM comes up on the previous seed's clock with its stray fairies collected.
 *
 * Clears, in order:
 *   - both frozen blobs AND both shadow copies. In FrozenStateManager these are
 *     the SAME storage: Context_UpdateShadowCopy writes the bytes without
 *     setting hasBeenFrozen, and Context_ClearFrozenState memsets the buffer as
 *     well as clearing the flag. So one ClearAll covers both halves of the
 *     issue's "frozen blobs both directions, both shadows".
 *   - the RAM-only shared-item staging outbox (shared_items.c), which holds
 *     pickups staged but not yet committed when the session died.
 *   - every session field of gComboCtx: the switch request, source game and
 *     entrance, sharedFlags, the retired-in-place sharedItems/saveSlot, and —
 *     the two the netplay spike (#460) cares about — sharedItemsTagged and
 *     foreignPlacements. A stale sharedItemsTagged is how another player's
 *     grants from a dead room would reach a fresh seed.
 *
 * The seed stamp and the generation-authored reverse placement table
 * (foreignPlacementsOoT, #534) are governed by @p seedPolicy; see
 * ComboSeedStampPolicy.
 *
 * Deliberately does NOT touch gCurrentGame (which game is running right now is
 * still true) and does NOT arm a frozen blob from anything.
 *
 * NOTE (corrected): this used to claim arming was "the exclusive job of a real
 * freeze, which keeps the 'a plain .redsave load applies on the next switch
 * only' semantics from #419/#420 intact". That contract was never implementable
 * as written — the next cross-game switch freezes the DEPARTING game
 * (Combo_CheckEntranceSwitch resolves its gameId from Context_GetCurrentGame),
 * so it never arms the ARRIVING side, and a loaded blob was therefore not
 * applied on the next switch or on any other occasion. Arming a loaded slot is
 * now an explicit, separate step: Context_ArmShadowAsFrozen. What this function
 * still guarantees is narrower and true: invalidation itself never arms
 * anything.
 */
void Context_InvalidateSessionState(ComboSeedStampPolicy seedPolicy);

// ---- Call-site-shaped entry points ---------------------------------------
// The three transitions that end a session, each named for the thing that
// happens rather than for the policy it selects. Game-side call sites are plain
// C in games/oot/src/**, which do NOT have src/common on their include path and
// so declare these locally as `extern` (the same convention z_play.c uses for
// Combo_*). That is why none of them take ComboSeedStampPolicy: an enum
// re-declared by hand in another TU is an ABI hazard for no benefit. Keeping
// the policy here also means the "is this really a session end?" judgement is
// written once, in a TU the headless tests link.

/**
 * Return to the title screen (soft reset, or a cold boot's first pass).
 *
 * SUPPRESSED when a cross-game startup entrance is pending, and that guard is
 * the whole subtlety of this hook. A cross-game arrival ALSO passes through
 * OoT's title chain — TitleSetup -> Title (which fast-forwards on
 * Combo_HasStartupEntranceForGame) -> Opening -> Play_Init, which is where
 * Combo_ConsumeFrozenState finally applies the blob. Invalidating
 * unconditionally here would destroy the very blob the arrival is on its way to
 * consume, turning every return leg into a lost session. rsbs/src/main.cpp sets
 * the startup entrance BEFORE GameRunner_SwitchTo, so it is reliably visible by
 * the time the target's title chain runs — for entrance switches and for F10
 * hot-swap returns alike.
 *
 * Uses RSBS_SEED_STAMP_DROP: at the title no file is active, so nothing stands
 * behind a stamp. A seed generated afterwards re-stamps via Playthrough_Init;
 * an existing slot re-stamps from its .redsave on load.
 *
 * @return 1 if the session was invalidated, 0 if suppressed for an arrival.
 */
int Context_InvalidateSessionOnReturnToTitle(void);

/**
 * A new OoT file is being created (OoT_Sram_InitSave).
 *
 * @param isRandoFile non-zero iff the file being created is a randomizer file
 *        whose seed has already been generated. That is the ONLY case where the
 *        seed stamp and the reverse placement table are kept: generation ran
 *        moments ago, in the menu, and authored both FOR this file (#534). A
 *        vanilla new file passes 0 and the stamp goes with the rest of the dead
 *        session — otherwise Combo_ForeignPairingActive() would still report a
 *        paired world.
 */
void Context_InvalidateSessionOnNewGame(int isRandoFile);

/**
 * An existing slot is being loaded, BEFORE its .redsave is read back.
 *
 * The clear half of the issue's clear-and-reload semantics, and it matters most
 * in the case that looks like it needs it least: the .redsave is per-OoT-slot
 * (redship_slot{0,1,2}.redsave) while these globals are process singletons, so
 * loading a slot that has NO companion .redsave used to leave the previous
 * session's gComboCtx and shadows in place and silently adopt them as if they
 * belonged to the slot. Clearing first makes "no .redsave" mean "no cross-game
 * state", which is the truth.
 *
 * Does NOT arm a frozen blob itself — this is the CLEAR half. The caller's
 * RsbsSave_Load repopulates gComboCtx and both shadows and performs its own
 * explicit arming step (Context_ArmShadowAsFrozen) for a tier that actually
 * carries data. Ordering matters and is already correct at the call site: the
 * clear runs BEFORE the load, so it never wipes the bytes just read.
 */
void Context_InvalidateSessionOnSlotLoad(void);

// Context_ProcessSwitch() / Context_IsSwitchInProgress() used to be declared
// here. Their implementations were removed from switch.cpp (zero callers, and
// they depended on TUs excluded from the single-exe link); the dangling
// declarations were removed with ADR 0002. The live switch policy is
// Switch_PrepareHotSwap / Combo_ConsumeFrozenState in switch.cpp.

/**
 * Set the current game (used during initialization)
 */
void Context_SetCurrentGame(GameId game);

/**
 * Get the current game
 */
GameId Context_GetCurrentGame(void);

// ============================================================================
// Legacy C API compatibility (maps to new functions)
// These match the existing combo/ API for easier transition
// ============================================================================

// Combo_* functions for legacy compatibility
#define Combo_InitFrozenStates Context_InitFrozenStates
void Combo_FreezeState(const char* gameId, uint16_t returnEntrance,
                       const void* saveContext, size_t size);
int Combo_RestoreState(const char* gameId, void* saveContext, size_t size);
int Combo_HasFrozenState(const char* gameId);
uint16_t Combo_GetFrozenReturnEntrance(const char* gameId);
void Combo_ClearFrozenState(const char* gameId);
#define Combo_GetOoTSaveContext Context_GetOoTSaveContext
#define Combo_GetMMSaveContext Context_GetMMSaveContext
void Combo_UpdateShadowCopy(const char* gameId, const void* saveContext, size_t size);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_CONTEXT_H
