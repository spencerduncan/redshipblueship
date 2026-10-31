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
 * and before every save, so its rows are STALE, never live. It also draws each
 * SHARED FAMILY the frozen rules arm (#976; #1065: the Lens of Truth, #1067: the
 * ocarina buttons) once, held when the pool or either game's own copy holds it,
 * and the games' own rows for that family are then left out of their sections.
 *
 * A SHARED TIER SHOWS ONCE (#967). The quiver, the bomb bag, the stick and nut
 * bags and (under the shared-sword rule) the Kokiri Sword are one item for the
 * pair, held in the pool. Each game's row for one names its shared kind and the
 * tier its save holds (sharedKind, sharedTier); Combo_ItemRowAt then returns the
 * row dimmed (nothing held, no count) once the Shared column holds that tier, and
 * the Shared row for it takes the ACTIVE game's live tier and count when they are
 * ahead of the pool (the pool is written at a game switch or a save, so a bow
 * found a minute ago is in the live save only). A copy ahead of the pool stays
 * lit: it is the only place the find shows.
 *
 * THE SHARED COLUMN IS LIVE FOR THE ACTIVE GAME (#971). Every other shared kind
 * (the hookshot, the wallet, magic, double defense, hearts, and the rupee and
 * ammo counts) is read the same way, through ComboItemOps::sharedLive over the
 * active game's LIVE buffer, with no harvest: a MONOTONIC kind shows
 * max(pool, live), a CONSUMABLE count shows the live game's own value (an ammo
 * count only while the live tier is at least the pool's). The pool is never
 * written by a draw, and the inactive game keeps its snapshot.
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
    // How the icon is drawn (#458 U3), set by whoever produced the icon keys:
    float iconAspect;  // the icon's width over its height; 0 is square (a song's note is narrower)
    float fadedAlpha;  // not held: 0 draws iconKeyFaded as it is (SoH's own faded textures); otherwise
                       // iconKeyFaded is drawn at this alpha (MM's tracker: the same icon at 0.4)
    uint32_t iconTint; // 0xRRGGBBAA the icon is multiplied by; 0 draws it as it is (MM tints its rupee green)
    // How the number on the icon is drawn (#458 U3), set by whoever produced the row:
    uint8_t countAccent; // ComboItemCountAccent: the tone of the ceiling, and of the amount once it is full
    int iconNumber;      // 0: count (and max) as usual; > 0: this number alone (a wallet's capacity, as
                         // both native trackers print it); < 0: no number (the icon itself shows the tier)
    // The shared copy (#967), set by the adapter on a row that is THIS game's copy of a capacity tier the
    // pair shares (the quiver, bomb bag, stick and nut bags, the Kokiri Sword): the shared-resource kind
    // (RSBS_SHARED_RES_*; 0: the row is not one) and the tier this save holds in it. The view draws the
    // copy dimmed once the Shared column holds the tier, so an item never reads as held twice.
    uint8_t sharedKind;
    uint8_t sharedTier;
    // The shared-family slot (foreign_items.h: an RSBS_XITEM_* family bit or an
    // RSBS_XFAMILY_SLOT_*) this row is the game's own copy of, set by the
    // adapter; 0 for every other row. While the frozen rules arm that slot the
    // view draws it ONCE, in the Shared group, and leaves this game's row out
    // (#1065's Lens of Truth and #1067's ocarina buttons the first).
    uint32_t sharedFamily;
    // How SoH's main window lays a row out (#1141, lane IT-3; the row lanes IT-2/IT-4 set them):
    const char* label; // a short name drawn over the icon (SoH's DrawItem: the boss-soul, jabber-nut, ocarina-
                       // button and overworld-key short names, ComboItemLabelPos); NULL: none
    bool pad;          // a blank cell: it takes its place in the grid and draws nothing (SoH's ITEM_NONE, the
                       // blanks of its column-major dungeon items and of a section padded to its line's end)
    // What SoH's count settings read (#1143, lane IT-4; DrawItemCount's ItemTrackerNumbers), set by the adapter:
    uint8_t countKind; // ComboItemCountKind: what `count` and `max` are
    int capacityMax;   // AMMO: the most `max` can grow to (SoH's maxCapacity: a quiver's 50, a wallet's 500); 0:
                       // `max` never grows, so the amount is shown (SoH's bombchus, beans, tokens and hearts).
                       // TRIFORCE: the hunt's total pieces
    int held;          // KEYS: the keys in hand (SoH's currentAmmo); `count` is the keys collected
    char identifier;   // the letter SoH's Hookshot Identifier draws on the icon ('H', 'L'); 0: none
    // Customizable Item Tracker slot addressing and per-group layout:
    const char*
        slotKey; // stable, seed-independent item slug within its origin ("bow", "remains_odolwa", ...); NULL: none
    uint8_t customCols; // 0: use the section's ComboItemGridStyle; 1..12: custom group's column width
    bool customOwnLine; // true on the first slot of a custom group so ComboItemIconLayout starts a fresh line
} ComboItemRow;

/** What a row's `count` and `max` are, for SoH's count settings (#1143). */
typedef enum {
    COMBO_ITEM_COUNT_KIND_AMMO = 0,     // the amount and the capacity it is held to (every row but these)
    COMBO_ITEM_COUNT_KIND_KEYS = 1,     // a dungeon's small keys: collected, of the dungeon's keys (Key Counts)
    COMBO_ITEM_COUNT_KIND_TRIFORCE = 2, // the triforce pieces: collected, of the required (Triforce Piece Count)
} ComboItemCountKind;

/** The tone SoH's DrawItemCount gives a row's ceiling and its full amount (#458 U3). */
typedef enum {
    COMBO_ITEM_ACCENT_GREEN = 0, // every row but one
    COMBO_ITEM_ACCENT_RED = 1,   // the Gold Skulltula Tokens (QUEST_SKULL_TOKEN's IM_COL_RED)
} ComboItemCountAccent;

/** Where a section's count is drawn on its icon (#458 U3): each game's own tracker's rule. */
typedef enum {
    COMBO_ITEM_COUNT_SOH = 0, // SoH's DrawItemCount: centred under the icon, "count/max" with SoH's colours
    COMBO_ITEM_COUNT_MM = 1,  // MM's DrawItemCounts: the count alone, bottom right inside the cell
} ComboItemCountStyle;

/**
 * How a game's own item tracker lays out its icons (#458 U3), so the overlay's
 * section for that game looks like that tracker. Sizes are at the overlay's
 * default IconSize (36, SoH's default); the overlay scales them together.
 */
typedef struct ComboItemGridStyle {
    float cellPx;    // an icon cell's edge (SoH's IconSize 36; MM's ITEM_TEXTURE_SIZE 46)
    float gapPx;     // between two cells of a line (SoH's IconSpacing 12; MM's table: two CellPadding.x, 8)
    int columns;     // icons per line
    bool groupLines; // each group starts its own line (MM: one table per group); false: rows flow on (SoH)
    // Optional: a group's own icons per line (MM's Songs and Quest tables are 5 wide). NULL: `columns`.
    int (*groupColumns)(const char* group);
    uint8_t countStyle; // ComboItemCountStyle
    float lineGapPx;    // between two lines; 0: gapPx (SoH's grid is square; MM's table rows: two CellPadding.y, 4)
    // Optional (#1141): true for a group that takes lines of its own in a flowing grid: it starts a fresh
    // line, and the group after it does too (SoH pads each of its optional sections onto a fresh line of
    // its main window). NULL, or false for a group: it flows on, as SoH's main sections do.
    bool (*groupOwnLine)(const char* group);
} ComboItemGridStyle;

/** SoH's main Item Tracker grid (randomizer_item_tracker.cpp DrawItemsInRows: six 36 px icons a line,
 *  12 px apart, the sections flowing on): the grid of an adapter that names none. */
extern const ComboItemGridStyle kComboItemSohGrid;

/**
 * A game's item adapter, registered from that game's own TU. Every member is
 * called on the game thread.
 */
typedef struct ComboItemOps {
    // Number of rows that are always there; fixed for the adapter's lifetime.
    // Rows a save adds on top (see countFor) come after these, so an index below
    // this answers for every save.
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
    // Is this game's pause menu open right now (#458 U2: the overlay's "show
    // only while paused" option, SoH's ShowOnlyPaused)? May be NULL: the game is
    // then never paused. The view calls it only while this game is active.
    bool (*paused)(void);
    // How this game's own tracker lays its icons out (#458 U3). May be NULL:
    // kComboItemSohGrid.
    const ComboItemGridStyle* grid;
    // This game's own icon for a shared-pool row (#458 U3): fill `row`'s
    // iconKey, iconKeyFaded, iconAspect, fadedAlpha and iconTint for shared
    // resource `kind` (RSBS_SHARED_RES_*) at `tier` (the row's tier, or its
    // count for a tier kind), and `iconNumber` for the number this game's own
    // tracker prints on it (the wallet's capacity). Leaves the rest alone. May
    // be NULL, and may leave the keys NULL for a kind the game has no icon for:
    // the row is then text. RSBS_SHARED_RES_HEALTH_QUARTERS at tier
    // COMBO_ITEM_SHARED_ICON_HEART_PIECE asks for the heart piece (#1159).
    void (*sharedIcon)(uint8_t kind, uint16_t tier, ComboItemRow* row);
    // The rows THIS save shows (#1072): count() plus the rows a save's frozen
    // options switch on (MM's ocarina buttons, shown only when the file shuffles
    // them). Never below count(); indices count()..countFor()-1 are valid for
    // that `src` and contiguous, so a drawer that stops at the first refused row
    // sees them all. May be NULL: count() is then the answer for every save.
    int (*countFor)(const void* src);
    // The LIVE view of shared resource `kind` (RSBS_SHARED_RES_*, #971): what this
    // game's save `src` holds of it right now, written to `*out` and answered true,
    // or false when the game has no reading of that kind (or `src` is not a played
    // file). A READ ONLY op over the handed buffer: it reuses the harvest's readers'
    // arithmetic but never harvests, applies or writes the pool, the save or the
    // watermarks. The view calls it only for the ACTIVE game's LIVE buffer, so the
    // Shared column shows a find made since the last save or crossing. May be NULL:
    // the column is then the pool alone.
    bool (*sharedLive)(const void* src, uint8_t kind, uint16_t* out);
    // Full addressable catalog for the Customizable Item Tracker: every item this
    // adapter can track, regardless of whether a save is loaded or whether gated
    // shuffle options are on in the current seed. May be NULL.
    int (*catalogCount)(void);
    // Fill `out` for catalog item `catalogIndex` (0 .. catalogCount()-1). When
    // `src` is NULL (no save loaded, or settings preview), fills static metadata
    // (`group`, `name`, `label`, `slotKey`, `iconKey`, `iconKeyFaded`, `iconAspect`,
    // `fadedAlpha`, `iconTint`, `countKind`, `max`) with `have = false`. When `src`
    // is non-NULL, derives live/shadow ownership from `src`, and if the item belongs
    // to a shuffle-gated group whose option is OFF in `src`, sets `out->have = true`
    // (matching 2Ship's !IsItemObtainable rule so custom "Key Items" dungeon solver
    // grids show unshuffled Boss Souls as obtained rather than missing).
    bool (*catalogRowAt)(const void* src, int catalogIndex, ComboItemRow* out);
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

/**
 * Register MM's item adapter. DEFINED MM-SIDE
 * (games/mm/2s2h/Rando/ItemAdapterSingleExe.cpp), declared here for the same
 * reason (#458 U1b).
 */
void MM_ItemAdapter_Register(void);

// ============================================================================
// Per-game reads (all null-safe; game is GAME_OOT or GAME_MM)
// ============================================================================

/** Which source `game`'s rows come from right now (see the header comment). */
uint8_t Combo_ItemFreshness(uint8_t game);

/** Rows for `game`; 0 whenever its data is UNAVAILABLE. A row whose shared
 *  family the Shared group draws is not counted (and Combo_ItemRowAt skips it). */
int Combo_ItemCount(uint8_t game);

/** Fill `out` with `game`'s row `index`, read from the source the view picks for
 *  this call. False for a NULL `out`, an out-of-range index, or no data. */
bool Combo_ItemRowAt(uint8_t game, int index, ComboItemRow* out);

/** Is the ACTIVE game's pause menu open (its adapter's `paused`)? False under
 *  GAME_NONE, for an unregistered adapter, and for one with no `paused`. The
 *  inactive game is never asked (#458 U2). */
bool Combo_ItemActiveGamePaused(void);

/** `game`'s grid (its adapter's, else kComboItemSohGrid). Never NULL. */
const ComboItemGridStyle* Combo_ItemGridStyle(uint8_t game);

/**
 * Majora's Mask's per-group visibility (#1141, lane IT-3): the Preference key
 * that shows or hides `game`'s group `group` in the overlay, or NULL for a
 * group no key governs. MM's Tokens, Stray Fairies, Dungeon, Boss Souls, Enemy
 * Souls, Owl Statues, Time and Misc groups each have one,
 * gCombo.Tracker.Items.MM.<the group name without spaces> (the group strings
 * are the MM adapter's, ItemAdapterSingleExe.cpp). OoT's groups are governed by
 * SoH's own DisplayType keys, which the overlay reads
 * (ComboItemTrackerWindow.h), never by these.
 */
const char* Combo_ItemGroupVisibilityKey(uint8_t game, const char* group);

/** Whether the player shows `game`'s group `group`: its key's value, default
 *  shown; true for a group no key governs. */
bool Combo_ItemGroupShown(uint8_t game, const char* group);

/** The caption a game's section draws under its header for a freshness: none,
 *  "" for every game and freshness (#927: a section is labelled by its header
 *  alone, as SoH's trackers label theirs). Never NULL. */
const char* Combo_ItemFreshnessLabel(uint8_t game, uint8_t freshness);

// ============================================================================
// The Shared group: the cross-game resource pool
// ============================================================================

/** Group name of the shared-resource rows. */
#define COMBO_ITEM_SHARED_GROUP "Shared"

/** The `tier` sharedIcon is handed for the Shared "Heart Pieces" row (#1159): its
 *  kind is RSBS_SHARED_RES_HEALTH_QUARTERS, and its icon is the game's heart piece
 *  where the Hearts row's is the heart container. */
#define COMBO_ITEM_SHARED_ICON_HEART_PIECE 0xFFFFu

/** STALE while the pool holds any resource, UNAVAILABLE while it is empty (no
 *  game has been harvested yet). Never LIVE: the pool advances only at a game
 *  switch or a save. */
uint8_t Combo_ItemSharedFreshness(void);

/** The caption the Shared section draws under its header: none, "" whatever
 *  the pool holds (#927). Never NULL. */
const char* Combo_ItemSharedLabel(void);

/** Shared rows: one per resource this world shares (the triforce pieces only
 *  when armed), then one per shared family the frozen rules arm; 0 while the
 *  pool is empty. */
int Combo_ItemSharedCount(void);

/** Fill `out` with shared row `index`, its icon the active game's own (OoT's
 *  while no game is active; #458 U3). False for a NULL `out`, an out-of-range
 *  index, or an empty pool. */
bool Combo_ItemSharedRowAt(int index, ComboItemRow* out);

/** Combo_ItemSharedRowAt with the icon from `iconGame`'s adapter (GAME_OOT or
 *  GAME_MM): the overlay's fallback when the active game's icons are not loaded.
 *  An unregistered adapter, or one without `sharedIcon`, leaves the keys NULL. */
bool Combo_ItemSharedRowAtWithIcons(int index, uint8_t iconGame, ComboItemRow* out);

/** The Shared section's grid: SoH's icons, four a line, so the pool stands as a
 *  column beside the two games' grids (#458 U3). */
extern const ComboItemGridStyle kComboItemSharedGrid;

// ============================================================================
// Addressable item catalog (for the Customizable Item Tracker)
// ============================================================================

/** Total addressable catalog items for `game` (GAME_OOT or GAME_MM), regardless
 *  of whether a save is loaded or whether gated options are on; 0 if unregistered. */
int Combo_ItemCatalogCount(uint8_t game);

/** Fill `out` with `game`'s catalog item `catalogIndex` (0 .. Combo_ItemCatalogCount(game)-1).
 *  Uses the active live/shadow save when available (with unshuffled gated items
 *  resolved as `have = true`), or static metadata with `have = false` when no
 *  save is available. Never omits shared items so custom grids keep alignment. */
bool Combo_ItemCatalogRowAt(uint8_t game, int catalogIndex, ComboItemRow* out);

/** Fill `out` with `game`'s catalog item matching `slotKey` (e.g. "bow",
 *  "remains_odolwa", "boss_soul_odolwa"). False if `slotKey` is NULL or unknown. */
bool Combo_ItemCatalogRowByKey(uint8_t game, const char* slotKey, ComboItemRow* out);

/** Total addressable catalog items in the Shared pool (resources + shared families). */
int Combo_ItemSharedCatalogCount(void);

/** Fill `out` with shared catalog item `catalogIndex` (0 .. Combo_ItemSharedCatalogCount()-1),
 *  using `iconGame`'s icons (GAME_OOT or GAME_MM). Works both with and without an
 *  active save/pool so the settings editor can preview all shared slots. */
bool Combo_ItemSharedCatalogRowAt(int catalogIndex, uint8_t iconGame, ComboItemRow* out);

/** Fill `out` with the shared catalog item matching `slotKey` (e.g. "rupees",
 *  "lens", "ocarina_btn_a"). False if `slotKey` is NULL or unknown. */
bool Combo_ItemSharedCatalogRowByKey(const char* slotKey, uint8_t iconGame, ComboItemRow* out);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_ITEM_VIEW_H
