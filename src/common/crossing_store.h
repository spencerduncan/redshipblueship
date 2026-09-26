/**
 * @file crossing_store.h
 * @brief The persistent record of which hosts of one game hold an item of the
 *        other, and which item (ADR 0010 accepted answer O7; epic #645).
 *
 * ============================================================================
 * THE PROBLEM THIS SOLVES
 * ============================================================================
 *
 * Under the single bag (ADR 0010 increment 3) the number of items hosted in the
 * other game is a FILL OUTCOME, not a pool size: the coordinator may put
 * hundreds of OoT items on MM checks and hundreds of MM items on OoT checks.
 * Each game's OWN placements already persist in its own save (OoT's
 * ItemLocation table, MM's RANDO_SAVE_CHECKS), and each foreign host physically
 * holds a legal junk cover of its own game (the engines' `place`, ADR 0002).
 * What had no home is the crossing itself: "host H of game A yields item I of
 * game B". The host game's give path needs it to award the foreign item, the
 * one spoiler needs it to print both directions, and an arrival or a load needs
 * it to rebuild the coordinator's tables. This store is that home.
 *
 * ============================================================================
 * WHERE IT LIVES ON DISK, AND WHY NOT IN reserved[] (the O7 decision)
 * ============================================================================
 *
 * O7 asked for the boundary carriers (an origin-tagged SharedItem, a host check
 * id, and game-neutral bounds) to be sized against ComboContext.reserved[]
 * under the append-only second-block rule and ADR 0009's 64-byte floor. The
 * live figure, measured at 4a3bf058 by compiling context.h: sizeof(ComboContext)
 * = 1004, reserved[108] at offset 896, RSBS_COMBO_CONTEXT_RECORD_SIZE 1024 (20
 * bytes of slack after the struct). Spendable without breaching the floor is
 * 108 - 64 = 44 bytes, or 64 if the trailing slack were appended as well. One
 * crossing record is 8 bytes (below), so Tier-1 could hold at most EIGHT
 * crossings across both directions. The single bag needs hundreds. Three
 * layouts were weighed:
 *
 *   (a) a new append-only .redsave block (THIS): a 16-byte header plus 8 bytes
 *       per crossing, variable length, written after Tier-3 from format version
 *       3 on. Empty world: 16 bytes. At the cap (RSBS_CROSSING_STORE_CAP per
 *       host game) it is 16 + 2 x cap x 8 bytes. Zero bytes of reserved[].
 *   (b) a foreign sentinel inside each game's own check record pointing into a
 *       compact list: 6 bytes per crossing for the list, but the list still
 *       needs a home (it is (a) minus the host id), the sentinel has to be
 *       written into BOTH ports' vendored save structures (MM's randoItemId,
 *       OoT's ItemLocation, whose durable copy is OoT's own .sav JSON: a
 *       second artifact that can skew against the .redsave, #531), and the
 *       degrade invariant ("if the crossing record is absent, the host yields
 *       the junk it really holds") dies, because the sentinel IS the item.
 *   (c) the O7 carve for the bounds only (counts + a digest, 8 bytes of
 *       reserved[]) with the records elsewhere: the records still need (a),
 *       and the bounds are then stored twice, as a Tier-1 count that a
 *       zero-extended legacy record or a torn write can contradict. The
 *       payload CRC already covers every byte of (a).
 *
 * (a) is chosen: it is the only layout whose capacity is set by the host caps
 * rather than by the floor, it keeps every raw id inside an origin-tagged
 * SharedItem (ADR 0002), it touches neither port's save layout (the
 * composition ruling), and it travels through the ONE commit choke point
 * (#569: SaveManager::StageCommit snapshots it with Tier-1 and both shadows, so
 * the crossing set and the world it describes are one commit). No per-game
 * "holds a foreign item" bitmap is persisted: it is derivable from the list,
 * and a second persisted source of truth is exactly what SharedItem's "no count
 * field" rule refuses.
 *
 * ============================================================================
 * FORMAT (little-endian, written a byte at a time, never a struct memcpy)
 * ============================================================================
 *
 *   header, 16 bytes:
 *     [0..3]   magic "RSXP"
 *     [4..5]   u16 blockFormat  == RSBS_CROSSING_BLOCK_FORMAT (1)
 *     [6..7]   u16 recordSize   == RSBS_CROSSING_RECORD_SIZE (8)
 *     [8..9]   u16 count of OoT-hosted crossings (MM-origin items in OoT checks)
 *     [10..11] u16 count of MM-hosted crossings  (OoT-origin items in MM checks)
 *     [12..15] u32 reserved, must be 0
 *   then the OoT-hosted records, then the MM-hosted records, 8 bytes each:
 *     [0..1]   u16 hostCheck   (the HOST game's check id; never 0 = RC_UNKNOWN)
 *     [2..3]   u16 itemClass   (the bag row's RSBS_ITEMCLASS_* bit, carried)
 *     [4]      u8  item.originGame (the OTHER game; a crossing by definition)
 *     [5]      u8  item.flags
 *     [6..7]   u16 item.id     (in the origin game's id-space)
 *
 * Record order is INSERTION order (the coordinator's fill order), because it is
 * world-visible and a hydrate that re-sorted it would not round-trip.
 *
 * ============================================================================
 * THREE WRITERS, THREE RULES
 * ============================================================================
 *
 *   Combo_Crossings_CaptureFromCoordinator  THE CREATION WRITER (lane K11 calls
 *       it; nothing in production does at this commit). Authors the store from
 *       the coordinator's tables, overwriting whatever was resident, and
 *       FREEZES it. A refusal leaves the store EMPTY and UNSET: a refused
 *       creation must not leave a previous world's crossings behind for the
 *       file it failed to create.
 *   Combo_Crossings_Replace  THE HYDRATE WRITER (the spoiler-load route).
 *       One-game semantics: the crossing set is world identity frozen at
 *       creation, so this writes into an UNSET store only (and freezes it), is
 *       a no-op when the rows are identical to a FROZEN set, and REFUSES a
 *       different set (the store is left untouched): divergence is corruption,
 *       never a choice to honour. All-or-nothing: one refused row refuses the
 *       set. Its only production-intended caller, the spoiler loader
 *       (MM_Rando_LoadCrossingsFromSpoiler), first refuses a section whose
 *       combo.identity does not name the live pairing (the #610 rule).
 *   Combo_Crossings_LoadBlock  THE .redsave LOAD. The slot's own record is
 *       authoritative (it follows Context_InvalidateSessionOnSlotLoad's clear),
 *       so it overwrites after validating every byte, and FREEZES the store. A
 *       malformed block refuses the whole load (RSBS_REFUSE_CROSSINGS) and
 *       changes nothing.
 *
 * FROZEN EMPTY IS NOT UNSET. A world frozen with zero crossings (a v3 block
 * with both counts 0, or a v1/v2 file, which no pre-crossing build could have
 * given crossings) is as frozen as one with hundreds: Replace refuses any
 * non-empty set into it. Only Combo_Crossings_Clear (cold boot, a DROP
 * invalidation, a refused capture) returns the store to UNSET.
 *
 * Session scope: the store is process RAM, like gComboCtx. Freeze/restore and
 * shadow arming never touch it (both games are in one process); that is a
 * property of where it lives, not of any code here, and the ROM-free lock on it
 * is a regression guard only. Session invalidation (context.cpp) KEEPs it on
 * the creation path and DROPs it on every other, the rule the reverse placement
 * table already follows. An ARRIVAL-time pairing refusal
 * (MM_Rando_PairOnCrossGameArrival's RsbsSave_RefuseSlotIdentity /
 * RsbsSave_RefuseSlotGeneration) does NOT clear it, exactly as it does not
 * clear the pinned reverse table or the identity stamp: the slot is latched
 * against writes, so nothing reaches disk. What the give path should do for a
 * refused half is lane K11's decision, not this store's.
 */

#ifndef RSBS_COMMON_CROSSING_STORE_H
#define RSBS_COMMON_CROSSING_STORE_H

#include "combo_logic.h" // RSBS_COMBO_LOGIC_PLACEMENT_CAP, ComboLogicPlacement
#include "context.h"     // SharedItem, GameId

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Crossings each HOST game can carry. Tied to the coordinator's own per-host
 * placement cap, because every placement the coordinator makes could in
 * principle be a crossing: a smaller store would refuse a world the fill is
 * allowed to produce. This is a RAM and acceptance bound, not a format
 * constant: the block is count-prefixed, so raising the cap moves no byte of
 * any file already written.
 */
#define RSBS_CROSSING_STORE_CAP RSBS_COMBO_LOGIC_PLACEMENT_CAP

#define RSBS_CROSSING_BLOCK_MAGIC "RSXP"
#define RSBS_CROSSING_BLOCK_FORMAT 1u
#define RSBS_CROSSING_BLOCK_HEADER_SIZE 16u
#define RSBS_CROSSING_RECORD_SIZE 8u
/** Largest block this build accepts (both host lists at the cap). */
#define RSBS_CROSSING_BLOCK_MAX_SIZE                                                                                   \
    ((size_t)RSBS_CROSSING_BLOCK_HEADER_SIZE + 2u * (size_t)RSBS_CROSSING_STORE_CAP * RSBS_CROSSING_RECORD_SIZE)

/* Status codes. 0 is success; every refusal is negative. */
#define RSBS_CROSSING_OK 0
#define RSBS_CROSSING_ERR_CAPACITY (-1)       /* more rows than RSBS_CROSSING_STORE_CAP for one host game */
#define RSBS_CROSSING_ERR_BAD_ROW (-2)        /* host 0, an unset origin, or an own-origin row */
#define RSBS_CROSSING_ERR_DUPLICATE_HOST (-3) /* one host listed twice within its game */
#define RSBS_CROSSING_ERR_NOT_PAIRED (-4)     /* capture with no live cross-game pairing */
#define RSBS_CROSSING_ERR_DIVERGED (-5)       /* a frozen, DIFFERENT set is already resident */
#define RSBS_CROSSING_ERR_MALFORMED (-6)      /* a serialized block failed validation */
#define RSBS_CROSSING_ERR_COORDINATOR (-7)    /* the coordinator refused the hydrated tables */

/** One crossing: `item` (never of the host's own origin) sits on `hostCheck`. */
typedef struct {
    uint16_t hostCheck; // in the HOST game's check id-space
    uint16_t itemClass; // the bag row's RSBS_ITEMCLASS_* bit, carried through unchanged
    SharedItem item;    // origin-tagged; item.originGame is the OTHER game
} ComboCrossing;

/** Human-readable name of an RSBS_CROSSING_* status. */
const char* Combo_Crossings_StatusName(int status);

/** Empty the store (both host games) and return it to UNSET (not frozen). */
void Combo_Crossings_Clear(void);

/**
 * True once a writer has frozen the store for the resident world (capture, an
 * accepted Replace, or a .redsave load, including a load of a world with no
 * crossings); false after Combo_Crossings_Clear. See "FROZEN EMPTY IS NOT UNSET".
 */
bool Combo_Crossings_IsFrozen(void);

/**
 * A counter bumped by EVERY write to the store (a commit or a clear). A caller
 * that samples it before and after a refused call can prove the call wrote
 * nothing, not merely that the end state happens to match (publish-then-
 * retract leaves the same end state and still bumps this twice).
 */
uint32_t Combo_Crossings_WriteGeneration(void);

/** Crossings hosted in `hostGame`, or 0 for a non-game. */
int Combo_Crossings_Count(GameId hostGame);

/** The `index`-th crossing hosted in `hostGame`, in insertion order. */
bool Combo_Crossings_At(GameId hostGame, int index, ComboCrossing* out);

/**
 * THE READ PATH. The foreign item `hostCheck` of `hostGame` yields, or NULL if
 * that host carries none. The pointer aliases the store (read-only use; valid
 * until the next write). Answers from the store only: the pinned tables in
 * gComboCtx are consulted by the foreign_items.h accessors, which fall back to
 * this one, so every existing give path reads both until lane K11 retires the
 * pools.
 */
const SharedItem* Combo_Crossings_Lookup(GameId hostGame, uint16_t hostCheck);

/**
 * THE HYDRATE WRITER (see the file header): all-or-nothing, and frozen. An
 * empty store takes the rows, an identical set is a no-op, a different set is
 * RSBS_CROSSING_ERR_DIVERGED with the store untouched. Every refusal leaves the
 * store exactly as it was.
 * @return the total crossing count (>= 0) or a negative status.
 */
int Combo_Crossings_Replace(const ComboCrossing* ootHosted, int ootCount, const ComboCrossing* mmHosted, int mmCount);

/**
 * THE CREATION WRITER (see the file header). Walks the coordinator's two
 * placement tables (Combo_Logic_PlacementAt) and keeps every row whose item is
 * not of the host's own origin. Requires a live pairing identity
 * (Combo_ForeignPairingActive). On success the store holds exactly those rows
 * in the coordinator's order and the return is the total crossing count (>= 0).
 * On ANY refusal the store is left EMPTY and the negative status is returned.
 */
int Combo_Crossings_CaptureFromCoordinator(void);

/**
 * THE HYDRATE PATH: rebuild both coordinator tables from the store, through
 * Combo_Logic_HydrateTables, which records the rows WITHOUT calling either
 * engine: the games' own tables already hold the result (each foreign host's
 * junk cover is in its own game's save), and an engine `place` issued at a load
 * would write into whichever SaveContext happens to be live.
 *
 * The rebuilt tables carry the CROSSING rows only. Own-origin placements live
 * in each game's own save and are not this store's to restore; nothing after
 * creation reads them from the coordinator.
 *
 * @return the number of rows hydrated (>= 0), or RSBS_CROSSING_ERR_COORDINATOR.
 */
int Combo_Crossings_HydrateCoordinator(void);

/** FNV-1a over the serialized block. Two stores with equal digests hold the
 *  same crossings in the same order. Printed in the spoiler section. */
uint32_t Combo_Crossings_Digest(void);

/**
 * The digest Combo_Crossings_Digest would report if the store held exactly
 * these rows, computed WITHOUT touching the store: a loader compares a printed
 * digest against its parsed rows before anything is committed. Counts outside
 * [0, RSBS_CROSSING_STORE_CAP] (or a NULL list with a positive count) return 0;
 * such rows are refused by every writer anyway.
 */
uint32_t Combo_Crossings_DigestRows(const ComboCrossing* ootHosted, int ootCount, const ComboCrossing* mmHosted,
                                    int mmCount);

/** Bytes Combo_Crossings_Serialize writes for the resident store. */
size_t Combo_Crossings_SerializedSize(void);

/** Serialize the store (format above) into `out`. Returns the byte count, or 0
 *  when `cap` is too small (nothing written). */
size_t Combo_Crossings_Serialize(uint8_t* out, size_t cap);

/**
 * From the 16-byte block header alone, the block's total size. The .redsave
 * reader calls this before it reads the records, so an oversized count is
 * refused before a byte of the body is read, never truncated.
 * @return RSBS_CROSSING_OK and *outTotal, or RSBS_CROSSING_ERR_MALFORMED /
 *         RSBS_CROSSING_ERR_CAPACITY.
 */
int Combo_Crossings_BlockSize(const uint8_t* header, size_t headerLen, size_t* outTotal);

/** Validate a whole serialized block without touching the store. */
int Combo_Crossings_ValidateBlock(const uint8_t* block, size_t len);

/**
 * THE .redsave LOAD WRITER: validate every byte of `block`, then overwrite the
 * store with it and freeze it. A refusal changes nothing. `block == NULL` with
 * `len == 0` is a legacy (pre-crossing, format version < 3) file: the store is
 * emptied and FROZEN EMPTY, because no build that wrote that file could have
 * given its world a crossing.
 */
int Combo_Crossings_LoadBlock(const uint8_t* block, size_t len);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_CROSSING_STORE_H
