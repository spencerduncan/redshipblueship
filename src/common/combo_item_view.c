/**
 * @file combo_item_view.c
 * @brief The unified item tracker's view model (#458 U1). See combo_item_view.h.
 *
 * Everything here is a pure read: each game's rows through the adapter that
 * game registered, over a buffer this file picks (the active game's live save
 * or that game's frozen shadow), and the shared-resource pool through
 * shared_resources.h. No game headers, no ImGui, no caching: the source is
 * picked again on every call, so a game switch or a .redsave Load shows on the
 * next frame and no stale copy outlives it.
 */

#include "combo_item_view.h"

#include "context.h"          // Context_GetCurrentGame, Context_Get{OoT,MM}SaveContext
#include "cvar_shared_keys.h" // RSBS_CVAR_COMBO_ITEMS_MM_*: MM's group visibility keys (#1141)
#include "foreign_items.h"    // RSBS_XITEM_* / RSBS_XFAMILY_SLOT_*: the shared families the Shared group draws
#include "shared_resources.h" // Combo_GetSharedResource, Combo_SharedResourceKindArmed
#include "triforce_hunt.h"    // Combo_TriforceHuntRequired / _Total: the pieces' count (#1143)

#include <stdio.h>
#include <string.h>

#include <libultraship/bridge/consolevariablebridge.h> // the group visibility keys (#1141)

// ============================================================================
// Adapter registry
// ============================================================================

// By pointer: each game's TU passes a static vtable.
static const ComboItemOps* sOoTOps = NULL;
static const ComboItemOps* sMMOps = NULL;

static const ComboItemOps** OpsSlot(uint8_t game) {
    if (game == (uint8_t)GAME_OOT) {
        return &sOoTOps;
    }
    if (game == (uint8_t)GAME_MM) {
        return &sMMOps;
    }
    return NULL;
}

void Combo_Item_RegisterOps(uint8_t game, const ComboItemOps* ops) {
    const ComboItemOps** slot = OpsSlot(game);
    if (slot == NULL) {
        fprintf(stderr, "[ComboItemView] REJECTED item adapter for game %u: not OoT or MM\n", (unsigned)game);
        return;
    }
    if (ops == NULL) {
        *slot = NULL;
        return;
    }
    // A half-registered adapter would turn "no data" into a NULL call through
    // the overlay's draw path. liveSave alone may be NULL (shadow-only).
    if (ops->count == NULL || ops->rowAt == NULL || ops->hasSave == NULL) {
        fprintf(stderr, "[ComboItemView] REJECTED item adapter for game %u: NULL member\n", (unsigned)game);
        return;
    }
    *slot = ops;
}

const ComboItemOps* Combo_Item_GetOps(uint8_t game) {
    const ComboItemOps** slot = OpsSlot(game);
    return (slot != NULL) ? *slot : NULL;
}

// SoH's DrawItemsInRows defaults (randomizer_item_tracker.cpp): IconSize 36,
// IconSpacing 12, six a line, every section flowing on into the next.
const ComboItemGridStyle kComboItemSohGrid = { 36.0f, 12.0f, 6, false, NULL, (uint8_t)COMBO_ITEM_COUNT_SOH, 0.0f };

// The pool: SoH's icons and counts, four a line, a column beside the two
// games' six-wide grids about as wide as its own freshness note.
const ComboItemGridStyle kComboItemSharedGrid = { 36.0f, 12.0f, 4, false, NULL, (uint8_t)COMBO_ITEM_COUNT_SOH, 0.0f };

const ComboItemGridStyle* Combo_ItemGridStyle(uint8_t game) {
    const ComboItemOps* ops = Combo_Item_GetOps(game);
    return (ops != NULL && ops->grid != NULL) ? ops->grid : &kComboItemSohGrid;
}

// ============================================================================
// The source pick (ADR 0008 rule 5, amended 2026-09-30)
// ============================================================================

static const void* ShadowFor(uint8_t game) {
    if (game == (uint8_t)GAME_OOT) {
        return Context_GetOoTSaveContext();
    }
    if (game == (uint8_t)GAME_MM) {
        return Context_GetMMSaveContext();
    }
    return NULL;
}

/**
 * The buffer `game`'s rows are read from on this call, or NULL.
 *
 *   - LIVE: `game` is the active game, its adapter hands out a live save (only
 *     while its play state is loaded), and that save holds a started file.
 *   - STALE: otherwise, `game`'s frozen shadow, when it holds a started file.
 *   - UNAVAILABLE: neither (NULL returned).
 *
 * liveSave is called only under the first condition, so the inactive game's
 * rows can never be live. `outFreshness` may be NULL.
 */
static const void* PickSource(uint8_t game, const ComboItemOps** outOps, uint8_t* outFreshness) {
    uint8_t freshness = COMBO_TRACKER_FRESH_UNAVAILABLE;
    const void* src = NULL;
    const ComboItemOps* ops = Combo_Item_GetOps(game);
    if (ops != NULL) {
        if (Context_GetCurrentGame() == (GameId)game && ops->liveSave != NULL) {
            const void* live = ops->liveSave();
            if (live != NULL && ops->hasSave(live)) {
                src = live;
                freshness = COMBO_TRACKER_FRESH_LIVE;
            }
        }
        if (src == NULL) {
            const void* shadow = ShadowFor(game);
            if (shadow != NULL && ops->hasSave(shadow)) {
                src = shadow;
                freshness = COMBO_TRACKER_FRESH_STALE;
            }
        }
    }
    if (outOps != NULL) {
        *outOps = ops;
    }
    if (outFreshness != NULL) {
        *outFreshness = freshness;
    }
    return src;
}

uint8_t Combo_ItemFreshness(uint8_t game) {
    uint8_t freshness = COMBO_TRACKER_FRESH_UNAVAILABLE;
    (void)PickSource(game, NULL, &freshness);
    return freshness;
}

/** The rows `src` shows: the adapter's per-save count when it has one (#1072), never below count(). */
static int ItemRowCount(const ComboItemOps* ops, const void* src) {
    const int base = ops->count();
    if (ops->countFor == NULL) {
        return base;
    }
    const int rows = ops->countFor(src);
    return (rows > base) ? rows : base;
}

// ============================================================================
// Shared capacity tiers (#967)
// ============================================================================

/** The pool's value for `kind`, 0 when the kind has never been shared. */
static uint16_t SharedValue(uint8_t kind) {
    uint16_t v = 0;
    return Combo_GetSharedResource(kind, &v) ? v : 0;
}

/**
 * The ACTIVE game's LIVE reading of shared kind `kind` (#971), or false: through
 * ComboItemOps::sharedLive over the buffer PickSource calls LIVE. A read only, and
 * never of the inactive game: its snapshot is the pool until it applies it.
 */
static bool LiveSharedRead(uint8_t kind, uint16_t* out) {
    const GameId active = Context_GetCurrentGame();
    if (active != GAME_OOT && active != GAME_MM) {
        return false;
    }
    const ComboItemOps* ops = NULL;
    uint8_t freshness = COMBO_TRACKER_FRESH_UNAVAILABLE;
    const void* src = PickSource((uint8_t)active, &ops, &freshness);
    if (src == NULL || freshness != COMBO_TRACKER_FRESH_LIVE || ops->sharedLive == NULL) {
        return false;
    }
    return ops->sharedLive(src, kind, out);
}

/** A quantity the player spends (shared_resources.h CONSUMABLE): the live game's value is the truth. */
static bool SharedKindConsumable(uint8_t kind) {
    switch (kind) {
        case RSBS_SHARED_RES_RUPEES:
        case RSBS_SHARED_RES_ARROW_COUNT:
        case RSBS_SHARED_RES_BOMB_COUNT:
        case RSBS_SHARED_RES_BOMBCHU_COUNT:
        case RSBS_SHARED_RES_STICK_COUNT:
        case RSBS_SHARED_RES_NUT_COUNT:
            return true;
        default:
            return false;
    }
}

/**
 * What the Shared column shows for `kind` (#971): the pool's value, brought up to
 * the active game's live reading when that is ahead for a MONOTONIC kind (the
 * hookshot, the wallet, magic, double defense, hearts: a capacity that only
 * grows, so a live save behind the pool never lowers it), and replaced by it for
 * a CONSUMABLE count. The pool itself is not written.
 */
static uint16_t SharedColumnValue(uint8_t kind) {
    const uint16_t pool = SharedValue(kind);
    uint16_t live = 0;
    if (!LiveSharedRead(kind, &live)) {
        return pool;
    }
    if (SharedKindConsumable(kind)) {
        return live;
    }
    return (live > pool) ? live : pool;
}

/**
 * The ACTIVE game's own copy of shared tier `kind`: the row of its LIVE save that
 * names `kind` (ComboItemRow::sharedKind). The pool is written at a game switch
 * or a save only, so a find made since is in the live save and nowhere else.
 * False when no game is active, the active game's rows are not LIVE, or none of
 * its rows names `kind`. Reads the adapter's raw rows, never the dimmed ones.
 */
static bool LiveCopyOf(uint8_t kind, uint16_t* outTier, int* outCount) {
    const GameId active = Context_GetCurrentGame();
    if (active != GAME_OOT && active != GAME_MM) {
        return false;
    }
    const ComboItemOps* ops = NULL;
    uint8_t freshness = COMBO_TRACKER_FRESH_UNAVAILABLE;
    const void* src = PickSource((uint8_t)active, &ops, &freshness);
    if (src == NULL || freshness != COMBO_TRACKER_FRESH_LIVE) {
        return false;
    }
    const int rows = ops->count();
    for (int i = 0; i < rows; i++) {
        ComboItemRow row;
        memset(&row, 0, sizeof(row));
        if (ops->rowAt(src, i, &row) && row.sharedKind == kind) {
            *outTier = row.sharedTier;
            *outCount = row.count;
            return true;
        }
    }
    return false;
}

/** The tier the Shared column shows for `kind` (see ReadSharedTier). */
typedef struct {
    uint16_t tier;
    bool live;     // the active game's live copy supplied it (it is at least the pool's)
    int liveCount; // that copy's ammo count, when `live`
} SharedTierRead;

/**
 * `kind`'s tier as the Shared column shows it: the pool's, raised to the active
 * game's live copy when that is at least the pool's. A live save behind the pool
 * (a game that has not yet received it) never lowers it.
 */
static SharedTierRead ReadSharedTier(uint8_t kind) {
    SharedTierRead read = { SharedValue(kind), false, 0 };
    uint16_t liveTier = 0;
    int liveCount = 0;
    if (LiveCopyOf(kind, &liveTier, &liveCount) && liveTier >= read.tier) {
        read.tier = liveTier;
        read.live = true;
        read.liveCount = liveCount;
    }
    return read;
}

/**
 * Dim a per-game copy of a shared tier once the Shared column holds that tier
 * (at least as high as this save's): nothing held, no count, so the item reads as
 * held once. A copy ahead of the pool stays lit.
 */
static void DimSharedCopy(ComboItemRow* row) {
    if (row->sharedKind == (uint8_t)RSBS_SHARED_RES_NONE ||
        Combo_ItemSharedFreshness() == COMBO_TRACKER_FRESH_UNAVAILABLE ||
        !Combo_SharedResourceKindArmed(row->sharedKind)) {
        return;
    }
    const SharedTierRead read = ReadSharedTier(row->sharedKind);
    if (read.tier == 0 || read.tier < row->sharedTier) {
        return;
    }
    row->have = false;
    row->count = 0;
    row->max = 0;
}

/**
 * The shared families the Shared group draws instead of the games' own rows
 * (#1065, #1067): every family slot the frozen rules arm, while the Shared group is
 * shown at all. Before the pool's first harvest there is no Shared group, so
 * each game keeps its own row and the item is never drawn nowhere.
 */
static uint32_t SharedFamiliesDrawnShared(void) {
    return (Combo_ItemSharedFreshness() != COMBO_TRACKER_FRESH_UNAVAILABLE) ? Combo_SharedFamiliesArmed() : 0u;
}

/** Is adapter row `index` of `src` a game's own copy of a family in `drawnShared`? */
static bool RowLeftOut(const ComboItemOps* ops, const void* src, int index, uint32_t drawnShared) {
    ComboItemRow row;
    memset(&row, 0, sizeof(row));
    return ops->rowAt(src, index, &row) && (row.sharedFamily & drawnShared) != 0u;
}

/** The adapter index of the `index`-th row the view shows for `src`, or -1. */
static int AdapterIndexOf(const ComboItemOps* ops, const void* src, int index) {
    const int n = ItemRowCount(ops, src);
    const uint32_t drawnShared = SharedFamiliesDrawnShared();
    if (drawnShared == 0u) {
        return (index < n) ? index : -1; // every rule off: the adapter's rows exactly
    }
    static const ComboItemOps* sLastOps = NULL;
    static const void* sLastSrc = NULL;
    static int sLastN = -1;
    static uint32_t sLastDrawnShared = 0u;
    static int sLastIndex = -1;
    static int sLastAdapterIndex = -1;

    int seen = 0;
    int start = 0;
    if (index > 0 && ops == sLastOps && src == sLastSrc && n == sLastN && drawnShared == sLastDrawnShared &&
        index == sLastIndex + 1 && sLastAdapterIndex >= 0) {
        seen = index;
        start = sLastAdapterIndex + 1;
    }
    for (int i = start; i < n; i++) {
        if (!RowLeftOut(ops, src, i, drawnShared) && seen++ == index) {
            sLastOps = ops;
            sLastSrc = src;
            sLastN = n;
            sLastDrawnShared = drawnShared;
            sLastIndex = index;
            sLastAdapterIndex = i;
            return i;
        }
    }
    sLastIndex = -1;
    sLastAdapterIndex = -1;
    return -1;
}

int Combo_ItemCount(uint8_t game) {
    const ComboItemOps* ops = NULL;
    const void* src = PickSource(game, &ops, NULL);
    if (src == NULL) {
        return 0;
    }
    const int n = ItemRowCount(ops, src);
    const uint32_t drawnShared = SharedFamiliesDrawnShared();
    if (drawnShared == 0u) {
        return n;
    }
    int shown = 0;
    for (int i = 0; i < n; i++) {
        shown += RowLeftOut(ops, src, i, drawnShared) ? 0 : 1;
    }
    return shown;
}

bool Combo_ItemRowAt(uint8_t game, int index, ComboItemRow* out) {
    if (out == NULL || index < 0) {
        return false;
    }
    const ComboItemOps* ops = NULL;
    uint8_t freshness = COMBO_TRACKER_FRESH_UNAVAILABLE;
    const void* src = PickSource(game, &ops, &freshness);
    // `index` counts the rows Combo_ItemCount counts: a family the Shared group
    // draws is walked past.
    const int adapterIndex = (src != NULL) ? AdapterIndexOf(ops, src, index) : -1;
    if (adapterIndex < 0) {
        return false;
    }
    ComboItemRow row;
    memset(&row, 0, sizeof(row));
    if (!ops->rowAt(src, adapterIndex, &row) || row.group == NULL || row.name == NULL) {
        return false; // an adapter must not hand out a row the drawer cannot label
    }
    row.freshness = freshness; // the view's call, never the adapter's
    DimSharedCopy(&row);       // a shared tier shows once, in the Shared column (#967)
    *out = row;
    return true;
}

bool Combo_ItemActiveGamePaused(void) {
    const GameId active = Context_GetCurrentGame();
    if (active != GAME_OOT && active != GAME_MM) {
        return false;
    }
    const ComboItemOps* ops = Combo_Item_GetOps((uint8_t)active);
    return ops != NULL && ops->paused != NULL && ops->paused();
}

// ============================================================================
// Majora's Mask's per-group visibility (#1141)
// ============================================================================

// MM's groups the player can hide, by the MM adapter's group string (the
// contract ItemAdapterSingleExe.cpp spells), and each one's key. Inventory,
// Masks, Songs, Quest and the Ocarina Buttons (gated by the file's own frozen
// option) have none.
static const struct {
    const char* group;
    const char* key;
} kMMGroupKeys[] = {
    { "Tokens", RSBS_CVAR_COMBO_ITEMS_MM_TOKENS },
    { "Stray Fairies", RSBS_CVAR_COMBO_ITEMS_MM_STRAY_FAIRIES },
    { "Dungeon", RSBS_CVAR_COMBO_ITEMS_MM_DUNGEON },
    { "Boss Souls", RSBS_CVAR_COMBO_ITEMS_MM_BOSS_SOULS },
    { "Enemy Souls", RSBS_CVAR_COMBO_ITEMS_MM_ENEMY_SOULS },
    { "Owl Statues", RSBS_CVAR_COMBO_ITEMS_MM_OWL_STATUES },
    { "Time", RSBS_CVAR_COMBO_ITEMS_MM_TIME },
    { "Misc", RSBS_CVAR_COMBO_ITEMS_MM_MISC },
};

const char* Combo_ItemGroupVisibilityKey(uint8_t game, const char* group) {
    if (game != (uint8_t)GAME_MM || group == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < sizeof(kMMGroupKeys) / sizeof(kMMGroupKeys[0]); i++) {
        if (strcmp(kMMGroupKeys[i].group, group) == 0) {
            return kMMGroupKeys[i].key;
        }
    }
    return NULL;
}

bool Combo_ItemGroupShown(uint8_t game, const char* group) {
    const char* key = Combo_ItemGroupVisibilityKey(game, group);
    return key == NULL || CVarGetInteger(key, 1) != 0;
}

const char* Combo_ItemFreshnessLabel(uint8_t game, uint8_t freshness) {
    // A section is labelled by its header alone, as SoH's item tracker labels
    // its sections (#927): no caption for any freshness. The Combo Tracker's
    // panels take the same answer from the same place.
    return Combo_TrackerFreshnessLabel(game, freshness);
}

// ============================================================================
// The Shared group
// ============================================================================

// Capacity rows the two games agree on (context.h's RSBS_SHARED_RES_* notes:
// both pack these tiers into inventory.upgrades with the same capacity rows).
static const uint16_t kQuiverCap[4] = { 0, 30, 40, 50 };
static const uint16_t kBombBagCap[4] = { 0, 20, 30, 40 };
static const uint16_t kStickCap[4] = { 0, 10, 20, 30 };
static const uint16_t kNutCap[4] = { 0, 20, 30, 40 };

/**
 * One shared row. `countKind` is the quantity shown; `tierKind` (optional)
 * decides `have` and, through `tierCaps`, `max`; otherwise `fixedMax` is the
 * ceiling and `have` is a non-zero count. `unitsPerCount` converts pool units
 * to what the player counts (health is 0x10 per heart). `heldAtZero` marks a
 * row whose tier 0 is itself an item every file holds, so `have` is always true
 * and `count` is the tier.
 */
typedef struct {
    const char* name;
    const char* slotKey;
    uint8_t countKind;
    uint8_t tierKind; // RSBS_SHARED_RES_NONE when the row has no tier
    const uint16_t* tierCaps;
    uint16_t fixedMax;
    uint16_t unitsPerCount;
    bool heldAtZero;
    // The count is a tier the icon itself shows (the magic jar, the longshot,
    // the wallet): neither native tracker prints a number on it
    // (#458 U3). The text and the tooltip keep "n/max"; the icon game's
    // sharedIcon may still name a number of its own (the wallet's capacity).
    bool tierIcon;
    // Non-zero: the row counts what is left over a whole multiple of this many
    // pool units, in `unitsPerCount` units (#1159: the pieces toward the next
    // heart, the quantity modulo 16 in 4s), and draws the icon sharedIcon names
    // for COMBO_ITEM_SHARED_ICON_HEART_PIECE.
    uint16_t remainderOf;
} SharedRowDef;

static const SharedRowDef kSharedRows[] = {
    { "Rupees", "rupees", RSBS_SHARED_RES_RUPEES, RSBS_SHARED_RES_NONE, NULL, 0, 1, false, false, 0 },
    // The tier shows with each game's own ceiling (both tier 3s hold 999 since
    // #1283) rather than as a rupee max.
    // Tier 0 is the child's wallet every file starts with, so the row is held
    // at every tier (count = the tier, 0..3), agreeing with OoT's own wallet
    // row outside wallet shuffle. The pool carries no "no wallet yet" state
    // (wallet shuffle's flag is per game, not pooled), so under wallet shuffle
    // the per-game row is the authority for whether a wallet is held.
    { "Wallet", "wallet", RSBS_SHARED_RES_WALLET_TIER, RSBS_SHARED_RES_NONE, NULL, 3, 1, true, true, 0 },
    // Whole hearts (5 hearts and 2 pieces reads 5); the row after it carries
    // the pieces.
    { "Hearts", "hearts", RSBS_SHARED_RES_HEALTH_QUARTERS, RSBS_SHARED_RES_NONE, NULL,
      (uint16_t)(RSBS_SHARED_RES_MAX_HEALTH_QUARTERS / 16u), 16, false, false, 0 },
    // #1159: the pieces toward the next heart, n/4. Health is one quantity for
    // the pair (Combo_MakeHealthQuarters: 4 units a piece, 16 a heart), so four
    // pieces make a heart whichever game each was found in; both native rows
    // count their own game's pieces only (OoT's a lifetime n/36).
    { "Heart Pieces", "heart_pieces", RSBS_SHARED_RES_HEALTH_QUARTERS, RSBS_SHARED_RES_NONE, NULL, 4, 4, false, false,
      16 },
    { "Double Defense", "double_defense", RSBS_SHARED_RES_DOUBLE_DEFENSE, RSBS_SHARED_RES_NONE, NULL, 0, 1, false,
      false, 0 },
    { "Magic", "magic", RSBS_SHARED_RES_MAGIC_LEVEL, RSBS_SHARED_RES_NONE, NULL, 2, 1, false, true, 0 },
    { "Arrows", "arrows", RSBS_SHARED_RES_ARROW_COUNT, RSBS_SHARED_RES_QUIVER_TIER, kQuiverCap, 0, 1, false, false, 0 },
    { "Bombs", "bombs", RSBS_SHARED_RES_BOMB_COUNT, RSBS_SHARED_RES_BOMB_BAG_TIER, kBombBagCap, 0, 1, false, false, 0 },
    // The bombchu cap is per game (see the shims), not a shared kind.
    { "Bombchus", "bombchus", RSBS_SHARED_RES_BOMBCHU_COUNT, RSBS_SHARED_RES_NONE, NULL, 0, 1, false, false, 0 },
    { "Deku Sticks", "sticks", RSBS_SHARED_RES_STICK_COUNT, RSBS_SHARED_RES_STICK_TIER, kStickCap, 0, 1, false, false,
      0 },
    { "Deku Nuts", "nuts", RSBS_SHARED_RES_NUT_COUNT, RSBS_SHARED_RES_NUT_TIER, kNutCap, 0, 1, false, false, 0 },
    { "Hookshot", "hookshot", RSBS_SHARED_RES_HOOKSHOT_TIER, RSBS_SHARED_RES_NONE, NULL, 2, 1, false, true, 0 },
    // The shared Kokiri Sword (#886): a row only in a world whose frozen record
    // turns the rule on (Combo_SharedResourceKindArmed). One item, so it draws
    // as the sword itself with no number.
    { "Kokiri Sword", "sword_kokiri", RSBS_SHARED_RES_KOKIRI_SWORD, RSBS_SHARED_RES_NONE, NULL, 0, 1, false, true, 0 },
    // #970: how many bottles the pair holds, up to OoT's four; a row only in a
    // world whose frozen Shared Bottles rule shares the count. What each bottle
    // holds is each game's own, so the per-game bottle rows stay.
    { "Bottles", "bottles", RSBS_SHARED_RES_BOTTLE_COUNT, RSBS_SHARED_RES_NONE, NULL, 4, 1, false, false, 0 },
    // No "Ocarina" row: #879 retired the Shared Ocarina rule, and the kind is
    // never armed.
    { "Triforce Pieces", "triforce_pieces", RSBS_SHARED_RES_TRIFORCE_PIECES, RSBS_SHARED_RES_NONE, NULL, 0, 1, false,
      false, 0 },
};
#define SHARED_ROW_DEF_COUNT ((int)(sizeof(kSharedRows) / sizeof(kSharedRows[0])))

// The current and magic meters are left out on purpose: the HUD shows them, and
// they move every frame the pool cannot see.

static bool SharedRowShown(const SharedRowDef* def) {
    // A conditional kind (the triforce pieces) is a row only in a world that
    // shares it.
    return Combo_SharedResourceKindArmed(def->countKind) &&
           (def->tierKind == RSBS_SHARED_RES_NONE || Combo_SharedResourceKindArmed(def->tierKind));
}

/** The `index`-th shown definition, or NULL. */
static const SharedRowDef* SharedDefAt(int index) {
    int seen = 0;
    for (int i = 0; i < SHARED_ROW_DEF_COUNT; i++) {
        if (!SharedRowShown(&kSharedRows[i])) {
            continue;
        }
        if (seen++ == index) {
            return &kSharedRows[i];
        }
    }
    return NULL;
}

uint8_t Combo_ItemSharedFreshness(void) {
    return (Combo_CountSharedResources() > 0) ? COMBO_TRACKER_FRESH_STALE : COMBO_TRACKER_FRESH_UNAVAILABLE;
}

const char* Combo_ItemSharedLabel(void) {
    // The Shared header labels the section; how fresh the pool is, is not a
    // caption the overlay draws (#927).
    return "";
}

// ---- the shared families (#976's OR discipline; #1065, #1067 on) -------------

/** One shared family the Shared group can draw, after the resource rows. Its
 *  icon is the icon game's own row for it (the adapter row whose sharedFamily
 *  is this slot), so no adapter grows a second icon table. */
typedef struct {
    uint32_t family; // the family slot (an RSBS_XITEM_* family bit or an RSBS_XFAMILY_SLOT_*)
    const char* name;
    const char* slotKey;
} SharedFamilyRowDef;

static const SharedFamilyRowDef kSharedFamilyRows[] = {
    { RSBS_XITEM_SHARED_LENS, "Lens of Truth", "lens" },                // #1065
    { RSBS_XITEM_SHARED_SONG_EPONA, "Epona's Song", "song_epona" },     // #1066
    { RSBS_XITEM_SHARED_SONG_STORMS, "Song of Storms", "song_storms" }, // #1066
    { RSBS_XITEM_SHARED_SONG_TIME, "Song of Time", "song_time" },       // #1069
    // #1067: the five ocarina buttons, in MM's tracker order.
    { RSBS_XITEM_SHARED_OCARINA_BUTTONS, "A Button", "ocarina_btn_a" },
    { RSBS_XFAMILY_SLOT_OCARINA_C_DOWN, "C Down Button", "ocarina_btn_c_down" },
    { RSBS_XFAMILY_SLOT_OCARINA_C_RIGHT, "C Right Button", "ocarina_btn_c_right" },
    { RSBS_XFAMILY_SLOT_OCARINA_C_LEFT, "C Left Button", "ocarina_btn_c_left" },
    { RSBS_XFAMILY_SLOT_OCARINA_C_UP, "C Up Button", "ocarina_btn_c_up" },
    { RSBS_XITEM_SHARED_ARROW_FIRE, "Fire Arrows", "arrow_fire" },    // #968
    { RSBS_XITEM_SHARED_ARROW_ICE, "Ice Arrows", "arrow_ice" },       // #968
    { RSBS_XITEM_SHARED_ARROW_LIGHT, "Light Arrows", "arrow_light" }, // #968
    { RSBS_XITEM_SHARED_GORON, "Goron Mask", "mask_goron" },          // #978
    { RSBS_XITEM_SHARED_ZORA, "Zora Mask", "mask_zora" },             // #978
    { RSBS_XITEM_SHARED_BUNNY, "Bunny Hood", "mask_bunny" },          // #978
    { RSBS_XITEM_SHARED_KEATON, "Keaton Mask", "mask_keaton" },       // #978
    { RSBS_XITEM_SHARED_TRUTH, "Mask of Truth", "mask_truth" },       // #978
};
#define SHARED_FAMILY_ROW_DEF_COUNT ((int)(sizeof(kSharedFamilyRows) / sizeof(kSharedFamilyRows[0])))

static int SharedResourceRowCount(void) {
    int n = 0;
    for (int i = 0; i < SHARED_ROW_DEF_COUNT; i++) {
        if (SharedRowShown(&kSharedRows[i])) {
            n++;
        }
    }
    return n;
}

/** `game`'s own row for `family`, read from the source the view picks for it
 *  (live or shadow). False when the game has no data or no such row. */
static bool FamilyRowOf(uint8_t game, uint32_t family, ComboItemRow* out) {
    const ComboItemOps* ops = NULL;
    const void* src = PickSource(game, &ops, NULL);
    if (src == NULL) {
        return false;
    }
    const int n = ItemRowCount(ops, src);
    for (int i = 0; i < n; i++) {
        memset(out, 0, sizeof(*out));
        if (ops->rowAt(src, i, out) && (out->sharedFamily & family) != 0u) {
            return true;
        }
    }
    return false;
}

/** Is family row `def` drawn? Its slot is armed and at least one game has a row
 *  for it (a button has rows only in a game that shuffles it: with a row in
 *  neither game there is nothing to show, not a missing button). */
static bool SharedFamilyRowShown(const SharedFamilyRowDef* def) {
    if ((Combo_SharedFamiliesArmed() & def->family) == 0u) {
        return false;
    }
    ComboItemRow own;
    return FamilyRowOf((uint8_t)GAME_OOT, def->family, &own) || FamilyRowOf((uint8_t)GAME_MM, def->family, &own);
}

/** The `index`-th family row shown, or NULL. */
static const SharedFamilyRowDef* SharedFamilyDefAt(int index) {
    int seen = 0;
    for (int i = 0; i < SHARED_FAMILY_ROW_DEF_COUNT; i++) {
        if (SharedFamilyRowShown(&kSharedFamilyRows[i]) && seen++ == index) {
            return &kSharedFamilyRows[i];
        }
    }
    return NULL;
}

int Combo_ItemSharedCount(void) {
    if (Combo_ItemSharedFreshness() == COMBO_TRACKER_FRESH_UNAVAILABLE) {
        return 0; // "nothing shared yet", not a row of zeroes
    }
    int n = SharedResourceRowCount();
    if (Combo_SharedFamiliesArmed() != 0u) {
        for (int i = 0; i < SHARED_FAMILY_ROW_DEF_COUNT; i++) {
            n += SharedFamilyRowShown(&kSharedFamilyRows[i]) ? 1 : 0;
        }
    }
    return n;
}

/** Fill `out` with family row `familyIndex`: held when the pool holds it or
 *  either game's own copy does (the pool only advances at a crossing or a save,
 *  so a button found this stint lights at once); its icon is `iconGame`'s own
 *  row's, and the row is text when that game has none. */
static bool SharedFamilyRowAt(int familyIndex, uint8_t iconGame, ComboItemRow* out) {
    const SharedFamilyRowDef* def = SharedFamilyDefAt(familyIndex);
    if (def == NULL) {
        return false;
    }
    bool have = (Combo_SharedFamiliesOwned() & def->family) != 0u;
    ComboItemRow own;
    if (!have) {
        for (uint8_t g = (uint8_t)GAME_OOT; g <= (uint8_t)GAME_MM; g++) {
            if (FamilyRowOf(g, def->family, &own) && own.have) {
                have = true;
                break;
            }
        }
    }
    memset(out, 0, sizeof(*out));
    // OoT's tracker has one child-trade row, not a row per mask (#978), so a
    // family with no row in the icon game takes the other game's icon.
    const uint8_t otherGame = (iconGame == (uint8_t)GAME_MM) ? (uint8_t)GAME_OOT : (uint8_t)GAME_MM;
    if (FamilyRowOf(iconGame, def->family, &own) || FamilyRowOf(otherGame, def->family, &own)) {
        out->iconKey = own.iconKey;
        out->iconKeyFaded = own.iconKeyFaded;
        out->iconAspect = own.iconAspect;
        out->fadedAlpha = own.fadedAlpha;
        out->iconTint = own.iconTint;
    }
    out->group = COMBO_ITEM_SHARED_GROUP;
    out->name = def->name;
    out->slotKey = def->slotKey;
    out->have = have;
    out->sharedFamily = def->family;
    out->freshness = COMBO_TRACKER_FRESH_STALE;
    return true;
}

bool Combo_ItemSharedRowAt(int index, ComboItemRow* out) {
    // The active game's own icons: its textures are the ones certainly loaded.
    const uint8_t iconGame = (Context_GetCurrentGame() == GAME_MM) ? (uint8_t)GAME_MM : (uint8_t)GAME_OOT;
    return Combo_ItemSharedRowAtWithIcons(index, iconGame, out);
}

bool Combo_ItemSharedRowAtWithIcons(int index, uint8_t iconGame, ComboItemRow* out) {
    if (out == NULL || index < 0 || Combo_ItemSharedFreshness() == COMBO_TRACKER_FRESH_UNAVAILABLE) {
        return false;
    }
    const SharedRowDef* def = SharedDefAt(index);
    if (def == NULL) {
        // Past the resource rows: the shared families the frozen rules arm.
        return SharedFamilyRowAt(index - SharedResourceRowCount(), iconGame, out);
    }
    memset(out, 0, sizeof(*out));
    out->group = COMBO_ITEM_SHARED_GROUP;
    out->name = def->name;
    out->slotKey = def->slotKey;
    out->iconKey = NULL; // the icon game's own, below; NULL draws the row as text
    out->iconKeyFaded = NULL;
    // An ammo count is bounded by its capacity tier, so it follows the live game only
    // below, where the tier is read; every other kind is live here (#971).
    uint16_t value =
        (def->tierKind != RSBS_SHARED_RES_NONE) ? SharedValue(def->countKind) : SharedColumnValue(def->countKind);
    if (def->remainderOf != 0u) {
        value = (uint16_t)(value % def->remainderOf); // #1159: what is left over the whole hearts
    }
    out->count = (int)(value / def->unitsPerCount);
    uint16_t iconTier = (def->remainderOf != 0u) ? (uint16_t)COMBO_ITEM_SHARED_ICON_HEART_PIECE : (uint16_t)out->count;
    if (def->tierKind != RSBS_SHARED_RES_NONE) {
        // The tier and the ammo of the active game's live copy when it is at
        // least the pool's: the pool is written at a switch or a save, so a
        // bow found since is in the live save only (#967).
        const SharedTierRead read = ReadSharedTier(def->tierKind);
        uint16_t tier = read.tier;
        if (tier > 3) {
            tier = 3;
        }
        if (read.live) {
            // The live game's own count (#971), the copy's row count when the
            // adapter has no live reading of the kind.
            uint16_t liveAmmo = 0;
            out->count = LiveSharedRead(def->countKind, &liveAmmo) ? (int)liveAmmo : read.liveCount;
        }
        out->have = tier > 0;
        out->max = (int)def->tierCaps[tier];
        out->capacityMax = (int)def->tierCaps[3]; // #1143: the most the tier grows to
        iconTier = tier;
    } else {
        if (def->countKind == (uint8_t)RSBS_SHARED_RES_KOKIRI_SWORD) {
            out->count = (int)ReadSharedTier(def->countKind).tier;
            iconTier = (uint16_t)out->count;
        }
        out->have = def->heldAtZero || out->count > 0;
        out->max = (int)def->fixedMax;
        // #1143: SoH's Hookshot Identifier names the tier ("H", "L"); the pieces count against the frozen
        // hunt's requirement and total (0 with no hunt armed).
        if (def->countKind == (uint8_t)RSBS_SHARED_RES_HOOKSHOT_TIER && out->count > 0) {
            out->identifier = out->count >= 2 ? 'L' : 'H';
        }
        if (def->countKind == (uint8_t)RSBS_SHARED_RES_TRIFORCE_PIECES) {
            out->countKind = (uint8_t)COMBO_ITEM_COUNT_KIND_TRIFORCE;
            out->max = (int)Combo_TriforceHuntRequired();
            out->capacityMax = (int)Combo_TriforceHuntTotal();
        }
    }
    out->iconNumber = def->tierIcon ? -1 : 0;
    const ComboItemOps* ops = Combo_Item_GetOps(iconGame);
    if (ops != NULL && ops->sharedIcon != NULL) {
        ops->sharedIcon(def->countKind, iconTier, out);
    }
    out->freshness = COMBO_TRACKER_FRESH_STALE;
    return true;
}

// ============================================================================
// Addressable item catalog (for the Customizable Item Tracker)
// ============================================================================

static bool CatalogFamilyRowOf(uint8_t game, uint32_t family, ComboItemRow* out) {
    if (FamilyRowOf(game, family, out)) {
        return true;
    }
    const ComboItemOps* ops = Combo_Item_GetOps(game);
    if (ops == NULL || ops->catalogCount == NULL || ops->catalogRowAt == NULL) {
        return false;
    }
    const int n = ops->catalogCount();
    for (int i = 0; i < n; i++) {
        memset(out, 0, sizeof(*out));
        if (ops->catalogRowAt(NULL, i, out) && (out->sharedFamily & family) != 0u) {
            return true;
        }
    }
    return false;
}

int Combo_ItemCatalogCount(uint8_t game) {
    const ComboItemOps* ops = Combo_Item_GetOps(game);
    if (ops == NULL || ops->catalogCount == NULL || ops->catalogRowAt == NULL) {
        return 0;
    }
    const int n = ops->catalogCount();
    return n > 0 ? n : 0;
}

bool Combo_ItemCatalogRowAt(uint8_t game, int catalogIndex, ComboItemRow* out) {
    if (out == NULL || catalogIndex < 0) {
        return false;
    }
    const ComboItemOps* ops = NULL;
    uint8_t freshness = COMBO_TRACKER_FRESH_UNAVAILABLE;
    const void* src = PickSource(game, &ops, &freshness);
    if (ops == NULL || ops->catalogCount == NULL || ops->catalogRowAt == NULL || catalogIndex >= ops->catalogCount()) {
        return false;
    }
    ComboItemRow row;
    memset(&row, 0, sizeof(row));
    if (!ops->catalogRowAt(src, catalogIndex, &row) || row.group == NULL || row.name == NULL) {
        return false;
    }
    row.freshness = freshness;
    // Custom groups never omit or dim a slot when a family or tier is shared:
    // instead, while a save is loaded and the family/tier is armed, reflect
    // whether the pair holds it (pool || live).
    if (src != NULL) {
        if (!row.have && row.sharedFamily != 0u && (Combo_SharedFamiliesArmed() & row.sharedFamily) != 0u) {
            if ((Combo_SharedFamiliesOwned() & row.sharedFamily) != 0u) {
                row.have = true;
            } else {
                const uint8_t otherGame = (game == (uint8_t)GAME_MM) ? (uint8_t)GAME_OOT : (uint8_t)GAME_MM;
                ComboItemRow own;
                if (FamilyRowOf(otherGame, row.sharedFamily, &own) && own.have) {
                    row.have = true;
                }
            }
        }
        if (!row.have && row.sharedKind != (uint8_t)RSBS_SHARED_RES_NONE &&
            Combo_ItemSharedFreshness() != COMBO_TRACKER_FRESH_UNAVAILABLE &&
            Combo_SharedResourceKindArmed(row.sharedKind)) {
            const SharedTierRead read = ReadSharedTier(row.sharedKind);
            if (read.tier > 0) {
                row.have = true;
            }
        }
    }
    *out = row;
    return true;
}

bool Combo_ItemCatalogRowByKey(uint8_t game, const char* slotKey, ComboItemRow* out) {
    if (slotKey == NULL || slotKey[0] == '\0' || out == NULL) {
        return false;
    }
    const ComboItemOps* ops = Combo_Item_GetOps(game);
    if (ops == NULL || ops->catalogCount == NULL || ops->catalogRowAt == NULL) {
        return false;
    }
    const int n = ops->catalogCount();
    if (n <= 0) {
        return false;
    }
    enum { kCatalogKeyCacheMax = 256 };
    typedef struct {
        const ComboItemOps* ops;
        bool (*catalogRowAt)(const void*, int, ComboItemRow*);
        int count;
        const char* keys[kCatalogKeyCacheMax];
    } CatalogKeyCache;
    static CatalogKeyCache sCache[2];
    CatalogKeyCache* cache = (game == (uint8_t)GAME_OOT) ? &sCache[0] : (game == (uint8_t)GAME_MM) ? &sCache[1] : NULL;
    if (cache != NULL && n <= kCatalogKeyCacheMax) {
        if (cache->ops != ops || cache->catalogRowAt != ops->catalogRowAt || cache->count != n) {
            cache->ops = ops;
            cache->catalogRowAt = ops->catalogRowAt;
            cache->count = n;
            for (int i = 0; i < n; i++) {
                ComboItemRow row;
                memset(&row, 0, sizeof(row));
                cache->keys[i] = ops->catalogRowAt(NULL, i, &row) ? row.slotKey : NULL;
            }
        }
        for (int i = 0; i < n; i++) {
            if (cache->keys[i] != NULL && strcmp(cache->keys[i], slotKey) == 0) {
                return Combo_ItemCatalogRowAt(game, i, out);
            }
        }
        return false;
    }
    for (int i = 0; i < n; i++) {
        ComboItemRow row;
        if (Combo_ItemCatalogRowAt(game, i, &row) && row.slotKey != NULL && strcmp(row.slotKey, slotKey) == 0) {
            *out = row;
            return true;
        }
    }
    return false;
}

int Combo_ItemSharedCatalogCount(void) {
    return SHARED_ROW_DEF_COUNT + SHARED_FAMILY_ROW_DEF_COUNT;
}

bool Combo_ItemSharedCatalogRowAt(int catalogIndex, uint8_t iconGame, ComboItemRow* out) {
    if (out == NULL || catalogIndex < 0 || catalogIndex >= Combo_ItemSharedCatalogCount()) {
        return false;
    }
    if (iconGame != (uint8_t)GAME_OOT && iconGame != (uint8_t)GAME_MM) {
        iconGame = (Context_GetCurrentGame() == GAME_MM) ? (uint8_t)GAME_MM : (uint8_t)GAME_OOT;
    }
    const uint8_t otherGame = (iconGame == (uint8_t)GAME_MM) ? (uint8_t)GAME_OOT : (uint8_t)GAME_MM;
    const bool hasPool = Combo_ItemSharedFreshness() != COMBO_TRACKER_FRESH_UNAVAILABLE;

    if (catalogIndex < SHARED_ROW_DEF_COUNT) {
        const SharedRowDef* def = &kSharedRows[catalogIndex];
        memset(out, 0, sizeof(*out));
        out->group = COMBO_ITEM_SHARED_GROUP;
        out->name = def->name;
        out->slotKey = def->slotKey;
        uint16_t iconTier = (def->remainderOf != 0u) ? (uint16_t)COMBO_ITEM_SHARED_ICON_HEART_PIECE : 1u;
        if (hasPool) {
            uint16_t value = (def->tierKind != RSBS_SHARED_RES_NONE) ? SharedValue(def->countKind)
                                                                     : SharedColumnValue(def->countKind);
            if (def->remainderOf != 0u) {
                value = (uint16_t)(value % def->remainderOf);
            }
            out->count = (int)(value / def->unitsPerCount);
            iconTier = (def->remainderOf != 0u) ? (uint16_t)COMBO_ITEM_SHARED_ICON_HEART_PIECE : (uint16_t)out->count;
            if (def->tierKind != RSBS_SHARED_RES_NONE) {
                const SharedTierRead read = ReadSharedTier(def->tierKind);
                uint16_t tier = read.tier > 3 ? 3 : read.tier;
                if (read.live) {
                    uint16_t liveAmmo = 0;
                    out->count = LiveSharedRead(def->countKind, &liveAmmo) ? (int)liveAmmo : read.liveCount;
                }
                out->have = tier > 0;
                out->max = (int)def->tierCaps[tier];
                out->capacityMax = (int)def->tierCaps[3];
                iconTier = tier;
            } else {
                if (def->countKind == (uint8_t)RSBS_SHARED_RES_KOKIRI_SWORD) {
                    out->count = (int)ReadSharedTier(def->countKind).tier;
                    iconTier = (uint16_t)out->count;
                }
                out->have = def->heldAtZero || out->count > 0;
                out->max = (int)def->fixedMax;
                if (def->countKind == (uint8_t)RSBS_SHARED_RES_HOOKSHOT_TIER && out->count > 0) {
                    out->identifier = out->count >= 2 ? 'L' : 'H';
                }
                if (def->countKind == (uint8_t)RSBS_SHARED_RES_TRIFORCE_PIECES) {
                    out->countKind = (uint8_t)COMBO_ITEM_COUNT_KIND_TRIFORCE;
                    out->max = (int)Combo_TriforceHuntRequired();
                    out->capacityMax = (int)Combo_TriforceHuntTotal();
                }
            }
        } else {
            out->have = false;
            out->count = 0;
            out->max = (int)def->fixedMax;
        }
        out->iconNumber = def->tierIcon ? -1 : 0;
        const ComboItemOps* ops = Combo_Item_GetOps(iconGame);
        if (ops != NULL && ops->sharedIcon != NULL) {
            ops->sharedIcon(def->countKind, iconTier, out);
        }
        if (out->iconKey == NULL) {
            const ComboItemOps* otherOps = Combo_Item_GetOps(otherGame);
            if (otherOps != NULL && otherOps->sharedIcon != NULL) {
                otherOps->sharedIcon(def->countKind, iconTier, out);
            }
        }
        out->freshness = hasPool ? COMBO_TRACKER_FRESH_STALE : COMBO_TRACKER_FRESH_UNAVAILABLE;
        return true;
    }

    const SharedFamilyRowDef* def = &kSharedFamilyRows[catalogIndex - SHARED_ROW_DEF_COUNT];
    bool have = hasPool && (Combo_SharedFamiliesOwned() & def->family) != 0u;
    ComboItemRow own;
    if (hasPool && !have) {
        for (uint8_t g = (uint8_t)GAME_OOT; g <= (uint8_t)GAME_MM; g++) {
            if (FamilyRowOf(g, def->family, &own) && own.have) {
                have = true;
                break;
            }
        }
    }
    memset(out, 0, sizeof(*out));
    if (CatalogFamilyRowOf(iconGame, def->family, &own) || CatalogFamilyRowOf(otherGame, def->family, &own)) {
        out->iconKey = own.iconKey;
        out->iconKeyFaded = own.iconKeyFaded;
        out->iconAspect = own.iconAspect;
        out->fadedAlpha = own.fadedAlpha;
        out->iconTint = own.iconTint;
    }
    out->group = COMBO_ITEM_SHARED_GROUP;
    out->name = def->name;
    out->slotKey = def->slotKey;
    out->have = have;
    out->sharedFamily = def->family;
    out->freshness = hasPool ? COMBO_TRACKER_FRESH_STALE : COMBO_TRACKER_FRESH_UNAVAILABLE;
    return true;
}

bool Combo_ItemSharedCatalogRowByKey(const char* slotKey, uint8_t iconGame, ComboItemRow* out) {
    if (slotKey == NULL || slotKey[0] == '\0' || out == NULL) {
        return false;
    }
    for (int i = 0; i < SHARED_ROW_DEF_COUNT; i++) {
        if (kSharedRows[i].slotKey != NULL && strcmp(kSharedRows[i].slotKey, slotKey) == 0) {
            return Combo_ItemSharedCatalogRowAt(i, iconGame, out);
        }
    }
    for (int i = 0; i < SHARED_FAMILY_ROW_DEF_COUNT; i++) {
        if (kSharedFamilyRows[i].slotKey != NULL && strcmp(kSharedFamilyRows[i].slotKey, slotKey) == 0) {
            return Combo_ItemSharedCatalogRowAt(SHARED_ROW_DEF_COUNT + i, iconGame, out);
        }
    }
    return false;
}
