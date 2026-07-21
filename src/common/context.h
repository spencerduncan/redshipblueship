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
 * Capacity of ComboContext.foreignPlacements (Lane C1, #392). The MVP pins ~4
 * OoT progression items into MM checks; 8 slots leave headroom without
 * spending reserved[] bytes we would rather keep (growing later is legal under
 * the growth contract).
 */
#define RSBS_FOREIGN_PLACEMENT_CAP 8u

/**
 * Capacity of ComboContext.grantCursors (ADR 0005, netplay 1a #460): how many
 * distinct GRANT SOURCES can hold a delivery cursor at once. A source is one
 * remote authority feeding Combo_SubmitSourcedGrant — an Archipelago server
 * counts as ONE source regardless of room size, and a P2P co-op session uses
 * one source per peer, so 8 is generous for every planned topology. Growing
 * later is legal under the growth contract (carve more of reserved[]).
 */
#define RSBS_GRANT_SOURCE_CAP 8u

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

    // Headroom. Carve new fields from the FRONT of this array (as
    // sharedItemsTagged, sharedRandoSettingsHash, foreignPlacements, and the
    // grant cursors were) so the struct stays inside
    // RSBS_COMBO_CONTEXT_RECORD_SIZE; the on-disk record size does not change
    // either way, so old saves keep loading. Zeroed by ComboContext_Init,
    // which is what makes a zero-extended legacy record indistinguishable from
    // a freshly-initialized one — every field carved from here must keep
    // "zero means unset".
    uint8_t reserved[264];
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
// grantCursors is the next carve from the front of the old reserved[]: it must
// sit immediately after the foreign-placement table with no gap, occupying
// bytes every shipped .redsave stored as zero (unset).
RSBS_CTX_STATIC_ASSERT(offsetof(ComboContext, grantCursors) ==
                           RSBS_COMBO_CONTEXT_PRECARVE_SIZE + RSBS_SHARED_ITEM_CAP * sizeof(SharedItem) +
                               sizeof(uint32_t) +
                               RSBS_FOREIGN_PLACEMENT_CAP * sizeof(ComboForeignPlacement),
                       "grantCursors must be carved from the FRONT of the old reserved[] (contiguous "
                       "with the foreign-placement table); moving it changes .redsave format");
RSBS_CTX_STATIC_ASSERT(offsetof(ComboContext, sharedItemOverflowCount) ==
                           offsetof(ComboContext, grantCursors) +
                               RSBS_GRANT_SOURCE_CAP * sizeof(ComboGrantSourceCursor),
                       "sharedItemOverflowCount must sit immediately after the grant cursors; moving "
                       "it changes .redsave format");
RSBS_CTX_STATIC_ASSERT(offsetof(ComboContext, reserved) ==
                           offsetof(ComboContext, sharedItemOverflowCount) + sizeof(uint32_t),
                       "the tagged-item array, the settings digest, the foreign-placement table, the "
                       "grant cursors, the overflow count, and the remaining headroom must stay "
                       "contiguous (no padding, no fields slipped between them)");

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
