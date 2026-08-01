/**
 * @file combo_tracker_view.h
 * @brief View model for the combo tracker: both games' check progress through
 *        per-game adapters (#458; ADR 0002, ADR 0008).
 *
 * WHAT PROBLEM THIS SOLVES. Every tracker in the binary shows exactly one
 * game: OoT's trackers read the live heap Rando::Context, MM's read the live
 * MM gSaveContext behind an active-game gate. Nothing can show the INACTIVE
 * game's progress, even though the data is resident the whole time — OoT is
 * suspended (not shut down) during MM so its heap survives, and MM's check
 * completion travels inside the frozen shadow blob as in-save POD. This model
 * is the read-only projection of both, plus the cross-game identity and
 * placements already in gComboCtx.
 *
 * PER-GAME ADAPTERS, NEVER A MERGED ID SPACE (ADR 0002). The two check models
 * are irreconcilable by construction: MM keys an in-save POD table by
 * RandoCheckId, OoT keys a heap array by RandomizerCheck, and the raw u16s
 * collide freely. So each game registers its own adapter from a TU where its
 * layout/enums are in scope — the RsbsGameMetaDesc offset-descriptor pattern
 * (save.h) for MM, an accessor vtable for OoT — and every read below takes the
 * GameId. A ComboTrackerCheckRow.checkId is game-local: it is only meaningful
 * inside the panel of the game it came from and must never be compared across
 * games. The game argument IS the tag.
 *
 * FRESHNESS IS A FIELD, NOT A COMMENT. The tracker's whole point is showing
 * data that may be stale, so every summary carries an explicit freshness the
 * window must label:
 *   - MM reads the frozen shadow blob (Context_GetMMSaveContext), which is
 *     written at freeze/save time. That path is NEVER reported live, even
 *     while MM is the active game — the shadow lags the live save.
 *   - OoT reads the heap Rando::Context, which is live while OoT runs and
 *     exactly as-of-suspend while MM runs (the suspend machinery is
 *     audio+graph only; the heap is not torn down).
 *   - A game with nothing to read (never booted, no adapter registered, MM
 *     shadow still all-zero) reports UNAVAILABLE — distinct from "a world
 *     with zero checks collected", which the window must not conflate.
 *
 * Locked ROM-free by the ComboTrackerView CTest
 * (src/common/tests/test_combo_tracker_view.c): adapters driven over an
 * authored MM shadow blob and an authored OoT heap context, plus the
 * unregistered/never-booted null-safety.
 */

#ifndef RSBS_COMMON_COMBO_TRACKER_VIEW_H
#define RSBS_COMMON_COMBO_TRACKER_VIEW_H

#include "foreign_items.h" // SharedItem, gComboCtx, placement accessors, GameId

#ifdef __cplusplus
extern "C" {
#endif

/** How current a panel's data is. See the header comment: MM's shadow path is
 *  never LIVE, OoT's heap is LIVE only while OoT is the active game. */
typedef enum {
    COMBO_TRACKER_FRESH_UNAVAILABLE = 0, // nothing to read; NOT "zero progress"
    COMBO_TRACKER_FRESH_LIVE = 1,        // reading the running game's live state
    COMBO_TRACKER_FRESH_STALE = 2,       // last freeze/save (MM) or suspend (OoT)
} ComboTrackerFreshness;

/**
 * Label for a freshness value, per game (the stale wording differs: MM's
 * shadow is "as of last freeze/save", OoT's suspended heap is "as of
 * suspend"). Never NULL — out-of-range yields a visible placeholder rather
 * than a crash in a printf-family call.
 */
const char* Combo_TrackerFreshnessLabel(uint8_t game, uint8_t freshness);

/**
 * The combo identity header: the pairing key and both directions' placement
 * counts. `paired == false` means the worlds were never paired — the other
 * fields are then whatever gComboCtx holds (typically 0) and must not be shown
 * as a real pairing. The per-game panels are independent of pairing: a solo
 * OoT rando session has progress worth showing with no paired MM world.
 */
typedef struct {
    bool paired;                      // Combo_ForeignPairingActive()
    uint32_t sharedRandoSeed;         // gComboCtx.sharedRandoSeed
    uint32_t sharedRandoSettingsHash; // gComboCtx.sharedRandoSettingsHash
    uint32_t mmProfileDigest;         // gComboCtx.mmProfileDigest (0 = identity not frozen, #498/#564)
    int mmHostedForeign;              // OoT items placed into MM checks
    int ootHostedForeign;             // MM items placed into OoT checks
} ComboTrackerIdentity;

/** Fill `out` with the identity header. NULL `out` is ignored. */
void Combo_TrackerIdentity(ComboTrackerIdentity* out);

/**
 * One game's progress summary. `freshness == COMBO_TRACKER_FRESH_UNAVAILABLE`
 * means every other field is zero and the panel should say so; `hasWorld ==
 * false` with data available means the resident save is not a randomized one.
 */
typedef struct {
    uint8_t freshness; // ComboTrackerFreshness; owned by the view, not the adapter
    bool hasWorld;     // a randomized world is resident
    uint32_t seed;     // that game's own final seed (0 when none)
    int totalChecks;   // walkable row indices, [0, totalChecks)
    int shuffled;      // checks the seed placed an item on
    int obtained;      // shuffled checks already collected
    int skipped;       // shuffled checks the player marked skipped
} ComboTrackerGameSummary;

/**
 * One check, as its own game's panel renders it. `checkId` is GAME-LOCAL
 * (MM RandoCheckId / OoT RandomizerCheck) and must never cross panels — see
 * the header comment. `name` may be NULL when the game has no name table
 * loaded (e.g. OoT static data before OoT's first boot); render the id then.
 */
typedef struct {
    uint16_t checkId;
    const char* name; // may be NULL; storage is the owning game's static table
    bool shuffled;
    bool obtained;
    bool skipped;
} ComboTrackerCheckRow;

// ============================================================================
// MM adapter: an offset descriptor over the frozen MM shadow blob
// ============================================================================
//
// MM's check completion is in-save POD (RANDO_SAVE_CHECKS inside
// ShipSaveInfo), so the whole table rides the shadow blob Context_
// GetMMSaveContext() hands out. Common code walks that blob at offsets the MM
// TU registers — the RsbsGameMetaDesc pattern — so this file never includes
// z64save.h and a layout change on MM's side updates the descriptor and its
// static_assert tripwires in the same TU (games/mm/2s2h/Rando/
// TrackerAdapterSingleExe.cpp).

typedef struct ComboMMTrackerDesc {
    // 'ZELDA3' new-file marker: mismatch means the shadow holds no MM save at
    // all (all-zero until the first freeze), which reads as UNAVAILABLE.
    uint32_t newfOffset;
    uint32_t newfLen; // <= 8
    uint8_t newf[8];
    uint32_t saveTypeOffset; // u32 read; == saveTypeRando means a rando save
    uint32_t saveTypeRando;  // MM's SAVETYPE_RANDO value
    uint32_t finalSeedOffset;
    uint32_t checkTableOffset; // randoSaveChecks[0]
    uint32_t checkStride;      // sizeof(RandoSaveCheck)
    uint32_t checkCount;       // RC_MAX
    uint32_t shuffledOffset;   // one-byte flags within a check row
    uint32_t obtainedOffset;
    uint32_t skippedOffset;
    // Display name for a check id, or NULL. Supplied by the MM TU (it resolves
    // through Rando::StaticData) so the id->name table never crosses into
    // common code. May itself be NULL.
    const char* (*checkName)(uint16_t checkId);
} ComboMMTrackerDesc;

/**
 * Install MM's descriptor (copied; the caller's storage is not retained).
 * Rejects, with a stderr complaint, geometry that would read outside the
 * MM_SAVE_CONTEXT_SIZE blob or outside a row's stride — belt and braces under
 * the MM TU's static_asserts. Passing NULL un-registers, so a test can restore
 * the registry rather than leave process-global state behind.
 */
void Combo_Tracker_RegisterMM(const ComboMMTrackerDesc* desc);

/** The registered MM descriptor, or NULL. Read-only; exposed so the ROM-free
 *  lock can author a shadow blob at the REAL registered offsets. */
const ComboMMTrackerDesc* Combo_Tracker_GetMMDesc(void);

/**
 * Build and register MM's descriptor. DEFINED MM-SIDE
 * (games/mm/2s2h/Rando/TrackerAdapterSingleExe.cpp), declared here because
 * the combo entry point is what calls it. A call rather than a file-scope
 * registrar for the OptionsUiSingleExe reason: the name resolver reads
 * Rando::StaticData::CheckNames, whose population must not race static init.
 */
void MM_TrackerAdapter_Register(void);

// ============================================================================
// OoT adapter: an accessor vtable over the heap Rando::Context
// ============================================================================
//
// OoT's check status lives on the HEAP (Rando::Context), not in the
// SaveContext blob — and OOT_SAVE_CONTEXT_SIZE has ~1KB slack, so the blob
// route is not available even in principle. OoT is suspended, not shut down,
// during MM, so the heap survives and these accessors stay valid while MM
// runs. Every function must be null-safe for the never-booted case (the heap
// context is created lazily; Rando::Context::GetInstance() is NULL until
// something creates it).

typedef struct ComboOoTTrackerOps {
    // Fill everything except `freshness` (the view owns freshness). Returns
    // false — leaving `out` untouched — when no heap context exists.
    bool (*summary)(ComboTrackerGameSummary* out);
    // Walkable row indices; 0 when no heap context exists.
    int (*checkCount)(void);
    // Row `index`; false when out of range or no heap context exists.
    bool (*checkAt)(int index, ComboTrackerCheckRow* out);
    // Display name for a check id, or NULL (never-initialized static data).
    const char* (*checkName)(uint16_t checkId);
} ComboOoTTrackerOps;

/**
 * Install OoT's accessor vtable (the pointer is retained; the OoT TU passes a
 * static). Passing NULL un-registers. A vtable with any NULL member is
 * rejected with a stderr complaint — a half-registered adapter would turn
 * "unavailable" into a null call through the window's draw path.
 */
void Combo_Tracker_RegisterOoT(const ComboOoTTrackerOps* ops);

/**
 * Register OoT's accessor vtable. DEFINED OoT-SIDE
 * (games/oot/soh/Enhancements/randomizer/TrackerAdapterSingleExe.cpp),
 * declared here because the combo entry point is what calls it.
 */
void OoT_TrackerAdapter_Register(void);

// ============================================================================
// The reads the window performs (all null-safe; game is GAME_OOT or GAME_MM)
// ============================================================================

/** Fill `out` with `game`'s summary. An unregistered adapter, a never-booted
 *  OoT, or an MM shadow with no save all yield UNAVAILABLE zeros, never a
 *  crash. NULL `out` is ignored. */
void Combo_TrackerGameSummary(uint8_t game, ComboTrackerGameSummary* out);

/** Walkable row indices for `game`; 0 whenever its data is UNAVAILABLE. */
int Combo_TrackerCheckCount(uint8_t game);

/** Row `index` of `game`'s check table (raw table order, unshuffled rows
 *  included — the renderer filters). False out of range / unavailable. */
bool Combo_TrackerCheckAt(uint8_t game, int index, ComboTrackerCheckRow* out);

/** Display name for `game`'s check `checkId`, or NULL. */
const char* Combo_TrackerCheckName(uint8_t game, uint16_t checkId);

/**
 * One cross-game placement, from whichever direction's table `hostGame`
 * selects (the direction IS the accessor — the two tables are separate key
 * spaces, ADR 0009 decision 3). `itemName` is never NULL (pinned-pool name or
 * a visible placeholder); `hostCheckName` may be NULL.
 */
typedef struct {
    uint8_t hostGame;          // game whose world holds the check
    uint16_t hostCheckId;      // that game's check id
    const char* hostCheckName; // resolved via that game's adapter; may be NULL
    uint8_t originGame;        // the item's id-space owner
    uint16_t itemId;
    const char* itemName; // never NULL
    bool redeemed;        // the origin game has already awarded this crossing
} ComboTrackerForeignRow;

/** Occupied placement slots hosted by `hostGame`; 0 when the worlds are not
 *  paired (same honesty rule as the spoiler view: "not paired" must never
 *  render as "no crossings"). */
int Combo_TrackerForeignCount(uint8_t hostGame);

/** Fill `out` with `hostGame`'s crossing `index` (slot order). False for a
 *  NULL `out`, an out-of-range index, or an unpaired world. */
bool Combo_TrackerForeignRowAt(uint8_t hostGame, int index, ComboTrackerForeignRow* out);

/** Fallback ComboTrackerForeignRow.itemName for a placement whose item is not
 *  in its origin's pinned pool (the spoiler view's placeholder rule). */
#define RSBS_TRACKER_UNKNOWN_ITEM_NAME "Unknown Foreign Item"

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_TRACKER_VIEW_H
