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
#include "foreign_items.h"    // RSBS_XITEM_* / RSBS_XFAMILY_SLOT_*: the shared families the Shared group draws
#include "shared_resources.h" // Combo_GetSharedResource, Combo_SharedResourceKindArmed

#include <stdio.h>
#include <string.h>

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

/**
 * The shared families the Shared group draws instead of the games' own rows
 * (#1067): every family slot the frozen rules arm, while the Shared group is
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
    int seen = 0;
    for (int i = 0; i < n; i++) {
        if (!RowLeftOut(ops, src, i, drawnShared) && seen++ == index) {
            return i;
        }
    }
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
} SharedRowDef;

static const SharedRowDef kSharedRows[] = {
    { "Rupees", RSBS_SHARED_RES_RUPEES, RSBS_SHARED_RES_NONE, NULL, 0, 1, false, false },
    // The wallets' capacities differ per game (MM's largest holds 500, OoT's
    // 999), so the tier shows with its own ceiling rather than as a rupee max.
    // Tier 0 is the child's wallet every file starts with, so the row is held
    // at every tier (count = the tier, 0..3), agreeing with OoT's own wallet
    // row outside wallet shuffle. The pool carries no "no wallet yet" state
    // (wallet shuffle's flag is per game, not pooled), so under wallet shuffle
    // the per-game row is the authority for whether a wallet is held.
    { "Wallet", RSBS_SHARED_RES_WALLET_TIER, RSBS_SHARED_RES_NONE, NULL, 3, 1, true, true },
    // Whole hearts only: the pieces toward the next heart are truncated (5
    // hearts and 2 pieces reads 5). The per-game "Pieces of Heart" row carries
    // the pieces.
    { "Hearts", RSBS_SHARED_RES_HEALTH_QUARTERS, RSBS_SHARED_RES_NONE, NULL,
      (uint16_t)(RSBS_SHARED_RES_MAX_HEALTH_QUARTERS / 16u), 16, false, false },
    { "Double Defense", RSBS_SHARED_RES_DOUBLE_DEFENSE, RSBS_SHARED_RES_NONE, NULL, 0, 1, false, false },
    { "Magic", RSBS_SHARED_RES_MAGIC_LEVEL, RSBS_SHARED_RES_NONE, NULL, 2, 1, false, true },
    { "Arrows", RSBS_SHARED_RES_ARROW_COUNT, RSBS_SHARED_RES_QUIVER_TIER, kQuiverCap, 0, 1, false, false },
    { "Bombs", RSBS_SHARED_RES_BOMB_COUNT, RSBS_SHARED_RES_BOMB_BAG_TIER, kBombBagCap, 0, 1, false, false },
    // The bombchu cap is per game (see the shims), not a shared kind.
    { "Bombchus", RSBS_SHARED_RES_BOMBCHU_COUNT, RSBS_SHARED_RES_NONE, NULL, 0, 1, false, false },
    { "Deku Sticks", RSBS_SHARED_RES_STICK_COUNT, RSBS_SHARED_RES_STICK_TIER, kStickCap, 0, 1, false, false },
    { "Deku Nuts", RSBS_SHARED_RES_NUT_COUNT, RSBS_SHARED_RES_NUT_TIER, kNutCap, 0, 1, false, false },
    { "Hookshot", RSBS_SHARED_RES_HOOKSHOT_TIER, RSBS_SHARED_RES_NONE, NULL, 2, 1, false, true },
    // #970: how many bottles the pair holds, up to OoT's four; a row only in a
    // world whose frozen Shared Bottles rule shares the count. What each bottle
    // holds is each game's own, so the per-game bottle rows stay.
    { "Bottles", RSBS_SHARED_RES_BOTTLE_COUNT, RSBS_SHARED_RES_NONE, NULL, 4, 1, false, false },
    // No "Ocarina" row: #879 retired the Shared Ocarina rule, and the kind is
    // never armed.
    { "Triforce Pieces", RSBS_SHARED_RES_TRIFORCE_PIECES, RSBS_SHARED_RES_NONE, NULL, 0, 1, false, false },
};
#define SHARED_ROW_DEF_COUNT ((int)(sizeof(kSharedRows) / sizeof(kSharedRows[0])))

// The current and magic meters are left out on purpose: the HUD shows them, and
// they move every frame the pool cannot see.

static uint16_t SharedValue(uint8_t kind) {
    uint16_t v = 0;
    return Combo_GetSharedResource(kind, &v) ? v : 0;
}

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

// ---- the shared families (#976's OR discipline; #1067 on) -------------------

/** One shared family the Shared group can draw, after the resource rows. Its
 *  icon is the icon game's own row for it (the adapter row whose sharedFamily
 *  is this slot), so no adapter grows a second icon table. */
typedef struct {
    uint32_t family; // the family slot (an RSBS_XITEM_* family bit or an RSBS_XFAMILY_SLOT_*)
    const char* name;
} SharedFamilyRowDef;

static const SharedFamilyRowDef kSharedFamilyRows[] = {
    // #1067: the five ocarina buttons, in MM's tracker order.
    { RSBS_XITEM_SHARED_OCARINA_BUTTONS, "A Button" },       { RSBS_XFAMILY_SLOT_OCARINA_C_DOWN, "C Down Button" },
    { RSBS_XFAMILY_SLOT_OCARINA_C_RIGHT, "C Right Button" }, { RSBS_XFAMILY_SLOT_OCARINA_C_LEFT, "C Left Button" },
    { RSBS_XFAMILY_SLOT_OCARINA_C_UP, "C Up Button" },
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
    for (uint8_t g = (uint8_t)GAME_OOT; g <= (uint8_t)GAME_MM; g++) {
        if (FamilyRowOf(g, def->family, &own) && own.have) {
            have = true;
        }
    }
    memset(out, 0, sizeof(*out));
    if (FamilyRowOf(iconGame, def->family, &own)) {
        out->iconKey = own.iconKey;
        out->iconKeyFaded = own.iconKeyFaded;
        out->iconAspect = own.iconAspect;
        out->fadedAlpha = own.fadedAlpha;
        out->iconTint = own.iconTint;
    }
    out->group = COMBO_ITEM_SHARED_GROUP;
    out->name = def->name;
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
    out->iconKey = NULL; // the icon game's own, below; NULL draws the row as text
    out->iconKeyFaded = NULL;
    out->count = (int)(SharedValue(def->countKind) / def->unitsPerCount);
    uint16_t iconTier = (uint16_t)out->count;
    if (def->tierKind != RSBS_SHARED_RES_NONE) {
        uint16_t tier = SharedValue(def->tierKind);
        if (tier > 3) {
            tier = 3;
        }
        out->have = tier > 0;
        out->max = (int)def->tierCaps[tier];
        iconTier = tier;
    } else {
        out->have = def->heldAtZero || out->count > 0;
        out->max = (int)def->fixedMax;
    }
    out->iconNumber = def->tierIcon ? -1 : 0;
    const ComboItemOps* ops = Combo_Item_GetOps(iconGame);
    if (ops != NULL && ops->sharedIcon != NULL) {
        ops->sharedIcon(def->countKind, iconTier, out);
    }
    out->freshness = COMBO_TRACKER_FRESH_STALE;
    return true;
}
