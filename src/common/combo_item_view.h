/**
 * @file combo_item_view.h
 * @brief View model for the unified item tracker: both games' items through
 *        per-game adapters, plus the shared-resource pool (#458 U1; ADR 0002,
 *        ADR 0008).
 *
 * WHAT PROBLEM THIS SOLVES. Each native item tracker shows one game, reads the
 * global gSaveContext through that game's accessor macros and gates on a
 * non-NULL play state, so neither can show the INACTIVE game's items. That data
 * is resident the whole time: the inactive game's SaveContext image sits in its
 * frozen shadow (Context_GetOoTSaveContext / Context_GetMMSaveContext), written
 * at every game switch and save. This model hands each game's adapter a buffer
 * (the live save of the active game, otherwise that game's shadow) and gets back
 * display rows, so one overlay can show both games.
 *
 * THE SOURCE PICK IS THE VIEW'S (ADR 0008 rule 5, amended 2026-09-30). For a
 * game G, per call:
 *   - LIVE: G is the active game (Context_GetCurrentGame), G's adapter hands
 *     out its live save (it does only while G's play state is loaded), and the
 *     adapter says that buffer holds a started save.
 *   - STALE: otherwise, G's frozen shadow, when the adapter says it holds a
 *     started save.
 *   - UNAVAILABLE: neither. Distinct from "an empty inventory".
 * The view never asks a game that is not active for its live save, so the
 * inactive game's rows always come from its shadow and are never LIVE.
 *
 * PER-GAME ADAPTERS, NO MERGED ID SPACE (ADR 0002, amended 2026-09-30). An
 * adapter lives in its own game's TU, reads the buffer it is handed through its
 * own layout, and returns rows that carry no game-local item id: group, name,
 * icon keys (opaque strings the adapter produced), have, count, max. The game
 * argument of every read is the origin tag. Common code names no layout.
 *
 * THE SHARED GROUP. The shared-resource pool (shared_resources.h) is a third
 * source, origin-neutral by construction: rupees, hearts, magic, ammo and the
 * rest are one quantity across both games. It is harvested at every game switch
 * and before every save, so its rows are labelled "As of the last switch or
 * save", never live.
 *
 * FRESHNESS IS A FIELD. Every row carries the freshness the view set for its
 * source; the adapter never sets it.
 *
 * Locked ROM-free by the ComboItemView CTest
 * (src/common/tests/test_combo_item_view.c): authored shadows per game, the
 * live-vs-shadow pick, never LIVE for the other game, the pool label, and every
 * buffer the lock touches restored.
 */

#ifndef RSBS_COMMON_COMBO_ITEM_VIEW_H
#define RSBS_COMMON_COMBO_ITEM_VIEW_H

#include "combo_tracker_view.h" // ComboTrackerFreshness, GameId

#ifdef __cplusplus
extern "C" {
#endif

/**
 * One item, as the overlay draws it. Every string is static storage owned by
 * whoever produced the row (the adapter's TU, or this view for the Shared
 * group). `group` and `name` are never NULL; an icon key may be NULL, and the
 * drawer then shows `name` as text.
 */
typedef struct {
    const char* group;        // section the row belongs to ("Inventory", "Songs", "Shared", ...)
    const char* name;         // player-facing name of what the row shows now (a progressive row names its tier)
    const char* iconKey;      // texture key to draw when `have`; opaque, never parsed by common code
    const char* iconKeyFaded; // texture key to draw when not `have`
    bool have;                // the player holds it
    int count;                // current quantity (ammo, tokens, rupees); 0 when the row has no count
    int max;                  // ceiling for `count` right now; 0 when there is none to show
    uint8_t freshness;        // ComboTrackerFreshness; set by the view, never by the adapter
} ComboItemRow;

/**
 * A game's item adapter, registered from that game's own TU. Every member is
 * called on the game thread.
 */
typedef struct ComboItemOps {
    // Number of rows; fixed for the adapter's lifetime.
    int (*count)(void);
    // Fill row `index` from `src` (a SaveContext image of this game) and nothing
    // else: never the global save. Leaves `out->freshness` alone. False when
    // `index` is out of range or `src` is unusable.
    bool (*rowAt)(const void* src, int index, ComboItemRow* out);
    // Does `src` hold a started save of this game (its new-file marker)? An
    // all-zero shadow (the game never ran this session) answers false.
    bool (*hasSave)(const void* src);
    // The game's live save, or NULL when it must not be read (title screen,
    // file select: no play state loaded). May itself be NULL: the shadow is then
    // the only source. The view calls it only while this game is active.
    const void* (*liveSave)(void);
} ComboItemOps;

/**
 * Install `game`'s adapter (GAME_OOT or GAME_MM). The pointer is retained: pass
 * a static. NULL un-registers, so a test can restore the registry. A vtable
 * with a NULL `count`, `rowAt` or `hasSave`, or a game that is not OoT or MM, is
 * rejected with a stderr complaint and leaves the registry unchanged.
 */
void Combo_Item_RegisterOps(uint8_t game, const ComboItemOps* ops);

/** The registered adapter for `game`, or NULL. Exposed so the lock can wrap the
 *  production adapter with a test live source. */
const ComboItemOps* Combo_Item_GetOps(uint8_t game);

/**
 * Register OoT's item adapter. DEFINED OoT-SIDE
 * (games/oot/soh/Enhancements/randomizer/ItemAdapterSingleExe.cpp), declared
 * here because the combo entry point (Combo_TrackerWindow_Init) calls it.
 */
void OoT_ItemAdapter_Register(void);

// ============================================================================
// Per-game reads (all null-safe; game is GAME_OOT or GAME_MM)
// ============================================================================

/** Which source `game`'s rows come from right now (see the header comment). */
uint8_t Combo_ItemFreshness(uint8_t game);

/** Rows for `game`; 0 whenever its data is UNAVAILABLE. */
int Combo_ItemCount(uint8_t game);

/** Fill `out` with `game`'s row `index`, read from the source the view picks for
 *  this call. False for a NULL `out`, an out-of-range index, or no data. */
bool Combo_ItemRowAt(uint8_t game, int index, ComboItemRow* out);

/** Player wording for a per-game freshness, no closing period: "Updated live",
 *  "As of the last game switch or save" (MM's reads "As of file creation" in
 *  the same case the check tracker's does), "No data". Never NULL. */
const char* Combo_ItemFreshnessLabel(uint8_t game, uint8_t freshness);

// ============================================================================
// The Shared group: the cross-game resource pool
// ============================================================================

/** Group name of the shared-resource rows. */
#define COMBO_ITEM_SHARED_GROUP "Shared"

/** Label of the shared group while the pool holds anything. */
#define COMBO_ITEM_SHARED_LABEL "As of the last switch or save"

/** STALE while the pool holds any resource, UNAVAILABLE while it is empty (no
 *  game has been harvested yet). Never LIVE: the pool advances only at a game
 *  switch or a save. */
uint8_t Combo_ItemSharedFreshness(void);

/** COMBO_ITEM_SHARED_LABEL, or "No data" while the pool is empty. Never NULL. */
const char* Combo_ItemSharedLabel(void);

/** Shared rows: one per resource this world shares (the ocarina and the
 *  triforce pieces only when armed); 0 while the pool is empty. */
int Combo_ItemSharedCount(void);

/** Fill `out` with shared row `index`. Icon keys are NULL (text). False for a
 *  NULL `out`, an out-of-range index, or an empty pool. */
bool Combo_ItemSharedRowAt(int index, ComboItemRow* out);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_ITEM_VIEW_H
