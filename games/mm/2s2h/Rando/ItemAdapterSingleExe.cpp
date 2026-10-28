/**
 * ItemAdapterSingleExe.cpp — MM's item adapter for the unified item tracker
 * (#458 U1; ADR 0002 and ADR 0008 rule 5 as amended 2026-09-30). The OoT twin
 * is games/oot/soh/Enhancements/randomizer/ItemAdapterSingleExe.cpp.
 *
 * The view (src/common/combo_item_view.c) picks a buffer — MM's live save
 * while MM is played, otherwise MM's frozen shadow — and hands it here. This TU
 * reads that buffer through MM's own layout and returns display rows; no
 * ITEM_*, QUEST_*, SLOT_* or RI_* value leaves it, only strings and counts.
 *
 * THE ACCESSOR MACROS OVER ANY BUFFER. MM's accessor macros (INV_CONTENT, AMMO,
 * CUR_UPG_VALUE, CUR_CAPACITY, GET_CUR_EQUIP_VALUE, CHECK_QUEST_ITEM; z64save.h
 * and macros.h) name the global `gSaveContext`. After the last #include below,
 * `gSaveContext` is redefined to `(*src)`, so every macro expanded in a row
 * derivation reads the parameter `src` — the buffer the view handed over — and
 * never the global. The redefinition follows every include, so no header ever
 * sees it. Functions are NOT macros: MM's native rows call
 * Rando::IsItemObtainable and Inventory_GetSkullTokenCount, which read the
 * global in their own TUs, so every row here is derived from the macros and
 * fields alone (no MM function is called with a save in mind).
 *
 * The live source is defined BEFORE the redefinition, since it is the one
 * place that must name the real global.
 *
 * WHICH ROWS. MM's native Item Tracker default preset (ItemTrackerSettings.cpp,
 * LoadAvailableWindows + ApplyDefaultItemPreset) expands to 83 rows: Inventory
 * 24 slots, Masks 24, Songs 10, Quest 10, Tokens 2, Stray Fairies 5, Dungeon 8.
 * The list here is 84 rows, in the native order: the 15 item slots of
 * Inventory (the three trade slots and the six bottle slots are left out; one
 * Bottles row counts the bottles held instead), all 24 Masks, the 10 Songs, the
 * 10 Quest rows plus a heart-piece count (n/4), and (#1140) MM's Tokens (Swamp,
 * Ocean, against the file's frozen minimum or the Spider House's 30), Stray
 * Fairies (Clock Town as an item, four temple counts against the frozen
 * minimum or 15) and Dungeon, 4 wide with one column per temple: boss keys,
 * small keys (found in a randomizer file, else held, against 1/3/1/4; lit while
 * one is held), maps, compasses. Left out: the trade items (transient quest
 * objects) and the per-bottle contents (consumables).
 *
 * MORE GROUPS, BY THE FILE (#1072, #1140). After those rows come MM's
 * shuffle-gated groups in MM's order, each shown only when the buffer's own
 * FROZEN option is on in a randomizer file (IS_RANDO + the option byte, read
 * from the buffer, never the live CVar): the five ocarina buttons ("Ocarina
 * Buttons", RO_SHUFFLE_OCARINA_BUTTONS, the test the note gate in
 * ActorBehavior/Player.cpp applies), "Boss Souls" (RO_SHUFFLE_BOSS_SOULS;
 * Majora's hidden under RO_SHUFFLE_TRIFORCE_PIECES, as GeneratePools leaves it
 * out), one "Enemy Souls" count n/47 (RO_SHUFFLE_ENEMY_SOULS), "Owl Statues"
 * (RO_SHUFFLE_OWL_STATUES), "Time" (RO_CLOCK_SHUFFLE) and "Misc", the ability
 * to swim (RO_SHUFFLE_SWIM). The adapter's `countFor` reports them and its
 * gated indices are contiguous (kMMItemGatedGroups). The group strings are
 * MM's own names and the contract with the overlay's per-group visibility
 * keys: spell them exactly.
 *
 * Names are MM's own (Rando::StaticData::Items, the table MM's tracker tooltips
 * read). Icon keys are resource paths (MM_gItemIcons, GetIconTexturePath), the
 * keys MM's tracker hands the Gui's texture map; MM has no faded variant (its
 * tracker draws the same texture at 40% alpha), so iconKeyFaded is the same
 * key and the drawer fades it. These icons load only after MM's first boot
 * (TrackersGuiSingleExe.cpp), so a drawer must fall back to `name` before then.
 *
 * Lives in 2s2h/Rando/ (glob-collected into 2ship_rando, WHOLE_ARCHIVE) and is
 * referenced by name from Combo_TrackerWindow_Init, so no elision mode leaves
 * the view without an MM adapter.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "Rando/Rando.h"               // SaveContext (via variables.h), StaticData::Items, GetIconTexturePath
#include "Rando/ActorBehavior/Souls.h" // SOUL_RI_TO_RANDO_INF

extern "C" {
#include <macros.h>                            // CUR_CAPACITY
#include "overlays/actors/ovl_En_Si/z_en_si.h" // SPIDER_HOUSE_TOKENS_REQUIRED
}
#include "assets/interface/parameter_static/parameter_static.h" // dgRupeeCounterIconTex

// src/common. Included outside any extern "C" block: the headers manage their
// own linkage (matching TrackerAdapterSingleExe.cpp).
#include "combo_item_view.h"
#include "foreign_items.h" // RSBS_XITEM_* / RSBS_XFAMILY_SLOT_*: shared families a row is MM's copy of (#1065, #1067)
#include "game.h"          // MM_SAVE_CONTEXT_SIZE
#include "shared_resources.h" // Combo_SaveIsLiveFile, Combo_MakeHealthQuarters (the Shared column's live view, #971)

// The view hands this TU either the unified live storage (unified_save.c,
// alignas(16)) or MM's shadow, a std::vector<uint8_t> buffer (context.cpp),
// which operator new aligns to __STDCPP_DEFAULT_NEW_ALIGNMENT__. Reading either
// as a SaveContext is sound only while the struct asks for no more than that.
static_assert(alignof(SaveContext) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__,
              "MM's shadow is a std::vector<uint8_t> buffer; SaveContext must not need stricter alignment");
static_assert(alignof(SaveContext) <= 16, "the unified gSaveContext storage is alignas(16)");
static_assert(sizeof(SaveContext) <= (size_t)MM_SAVE_CONTEXT_SIZE,
              "MM's SaveContext no longer fits the shadow blob the item adapter reads");

namespace {

/**
 * MM's live save for the view's LIVE read, or NULL: only while MM's play state
 * is loaded — the rule MM's check-tracker adapter uses (MMTrackerLiveSave,
 * TrackerAdapterSingleExe.cpp). That excludes the title screen's unmarked
 * bootstrap save and file select, which scans each slot through the live
 * buffer; neither is the player's world.
 */
const void* MMItemLiveSave(void) {
    return (MM_gPlayState != nullptr) ? static_cast<const void*>(&gSaveContext) : nullptr;
}

/** MM's pause menu is open (#458 U2: the overlay's "show only while paused"). */
bool MMItemPaused(void) {
    return MM_gPlayState != nullptr && MM_gPlayState->pauseCtx.state != PAUSE_STATE_OFF;
}

} // namespace

// ============================================================================
// From here on the accessor macros read the handed buffer, never the global.
// ============================================================================
#define gSaveContext (*src)

namespace {

bool MMItemAligned(const void* src) {
    return src != nullptr && (reinterpret_cast<uintptr_t>(src) % alignof(SaveContext)) == 0;
}

// ---- names and icon keys ------------------------------------------------------

/** MM's display name for a vanilla item: the first Rando item carrying it, the
 *  rule GetItemIdFromVanillaItemId applies for MM's own tracker tooltips.
 *  Built once (StaticData::Items is a constant table) instead of a map scan per
 *  row per frame. */
const char* MMItemVanillaName(uint32_t itemId) {
    static std::array<const char*, 256> sNames{};
    static bool sBuilt = false;
    if (!sBuilt) {
        for (const auto& [randoItemId, item] : Rando::StaticData::Items) {
            const uint32_t vanilla = (uint32_t)item.itemId;
            if (vanilla < sNames.size() && sNames[vanilla] == nullptr) {
                sNames[vanilla] = item.name;
            }
        }
        // No Rando item carries the bare bomb or bombchu slot item (MM's table
        // gives bombs and bombchus by quantity: ITEM_BOMBS_5, ITEM_BOMBCHUS_1,
        // ...), so MM's own tooltip resolves these two slots to RI_UNKNOWN.
        // Name them plainly instead.
        if (sNames[ITEM_BOMB] == nullptr) {
            sNames[ITEM_BOMB] = "Bombs";
        }
        if (sNames[ITEM_BOMBCHU] == nullptr) {
            sNames[ITEM_BOMBCHU] = "Bombchus";
        }
        sBuilt = true;
    }
    return (itemId < sNames.size() && sNames[itemId] != nullptr) ? sNames[itemId] : "Unknown Item";
}

/** MM's display name for a Rando item (never inserts into the table). */
const char* MMItemRandoName(RandoItemId randoItemId) {
    const auto it = Rando::StaticData::Items.find(randoItemId);
    return (it != Rando::StaticData::Items.end() && it->second.name != nullptr) ? it->second.name : "Unknown Item";
}

/** The icon resource path MM's tracker draws for a vanilla item, or NULL. */
const char* MMItemIcon(uint32_t itemId) {
    constexpr size_t kIcons = sizeof(MM_gItemIcons) / sizeof(MM_gItemIcons[0]);
    return (itemId < kIcons) ? reinterpret_cast<const char*>(MM_gItemIcons[itemId]) : nullptr;
}

/** The icon resource path MM's tracker draws for a Rando item. */
const char* MMItemRandoIcon(RandoItemId randoItemId) {
    if (Rando::StaticData::Items.count(randoItemId) == 0) {
        return nullptr;
    }
    return Rando::StaticData::GetIconTexturePath(randoItemId);
}

// MM's tracker draws an item not held as the same icon at 40% alpha
// (ItemTracker.cpp GetImageObject: textureColor.w = 0.4f), #458 U3.
constexpr float kMMItemFadedAlpha = 0.4f;

void MMItemFillVanilla(ComboItemRow* out, uint32_t itemId) {
    out->name = MMItemVanillaName(itemId);
    out->iconKey = MMItemIcon(itemId);
    out->iconKeyFaded = out->iconKey; // MM fades by alpha, not by texture
    out->fadedAlpha = kMMItemFadedAlpha;
}

void MMItemFillRando(ComboItemRow* out, RandoItemId randoItemId) {
    out->name = MMItemRandoName(randoItemId);
    out->iconKey = MMItemRandoIcon(randoItemId);
    out->iconKeyFaded = out->iconKey;
    out->fadedAlpha = kMMItemFadedAlpha;
}

// ---- the curated rows --------------------------------------------------------

enum MMItemRowKind : uint8_t {
    MMRK_SLOT,         // the inventory slot of base item `item`
    MMRK_SLOT_AMMO,    // ...plus its ammo, capped by upgrade `data`
    MMRK_SLOT_BOMBCHU, // ...bombchus: the bomb bag caps them (z_parameter.c Item_Give)
    MMRK_SLOT_FIXED,   // ...plus its ammo, capped at `data`
    MMRK_BOTTLES,      // how many of the six bottle slots hold a bottle
    MMRK_QUEST,        // quest bit `data`, named by Rando item `rando`
    MMRK_LULLABY,      // Goron Lullaby: the intro, then the full song
    MMRK_SWORD,        // sword equip value
    MMRK_SHIELD,       // shield equip value
    MMRK_MAGIC,        // isMagicAcquired / isDoubleMagicAcquired
    MMRK_DOUBLE_DEFENSE,
    MMRK_WALLET, // UPG_WALLET tier, rupees
    // #1140
    MMRK_HEART_PIECES,     // questItems' top nibble, against 4
    MMRK_TOKENS,           // skullTokenCount >> `data` (16 Swamp, 0 Ocean), against the frozen minimum
    MMRK_FAIRY_CLOCK_TOWN, // WEEKEVENTREG_08_80
    MMRK_FAIRY,            // strayFairies[`data`], against the frozen minimum
    MMRK_DUNGEON_ITEM,     // dungeon item `item` (DUNGEON_BOSS_KEY, _MAP, _COMPASS) of temple `data`
    MMRK_SMALL_KEY,        // small keys of temple `data`, against `item`
};

// Heart pieces occupy the TOP NIBBLE of questItems (the pool's canonical health quantity,
// MM_ReadHealthQuarters in GameExports_SingleExe.cpp).
constexpr uint32_t kMMHeartPieceShift = 28u;
constexpr uint32_t kMMHeartPieceMask = 0xF0000000u;

// MM's tracker tints these icons (ShipUtils.cpp randoItemColorMap; the alpha is the
// held/faded rule's, so 0xFF here): the Clock Town fairy, each temple's keys, the half-days.
constexpr uint32_t kMMTintClockTownFairy = 0xFFE680FFu;
constexpr uint32_t kMMTintTemple[4] = { 0xE6548FFFu, 0x1A8A29FFu, 0x9C0ADBFFu, 0x94A626FFu };
constexpr uint32_t kMMTintDay = 0xFFE64DFFu;
constexpr uint32_t kMMTintNight = 0x80B3FFFFu;

struct MMItemRowDef {
    const char* group;
    MMItemRowKind kind;
    uint8_t item;      // vanilla ItemId of the slot's base item
    uint32_t data;     // upgrade type, fixed cap or quest bit
    RandoItemId rando; // naming item for the Rando-keyed rows
};

#define MMI_SLOT(group, item) \
    { group, MMRK_SLOT, (uint8_t)(item), 0, RI_UNKNOWN }
#define MMI_AMMO(item, upg) \
    { "Inventory", MMRK_SLOT_AMMO, (uint8_t)(item), (uint32_t)(upg), RI_UNKNOWN }
#define MMI_FIXED(item, cap) \
    { "Inventory", MMRK_SLOT_FIXED, (uint8_t)(item), (uint32_t)(cap), RI_UNKNOWN }
#define MMI_QUEST(group, bit, rando) \
    { group, MMRK_QUEST, 0, (uint32_t)(bit), rando }
#define MMI_KIND(group, kind, rando) \
    { group, kind, 0, 0, rando }
#define MMI_FAIRY(temple) \
    { "Stray Fairies", MMRK_FAIRY, 0, DUNGEON_SCENE_INDEX_##temple##_TEMPLE, RI_##temple##_STRAY_FAIRY }
#define MMI_DUNGEON(temple, kind) \
    { "Dungeon", MMRK_DUNGEON_ITEM, DUNGEON_##kind, DUNGEON_SCENE_INDEX_##temple##_TEMPLE, RI_##temple##_##kind }
#define MMI_KEYS(temple, keys) \
    { "Dungeon", MMRK_SMALL_KEY, keys, DUNGEON_SCENE_INDEX_##temple##_TEMPLE, RI_##temple##_SMALL_KEY }

const MMItemRowDef kMMItemRows[] = {
    // Inventory: the item slots of the native "Inventory" group, in slot order
    MMI_SLOT("Inventory", ITEM_OCARINA_OF_TIME),
    MMI_AMMO(ITEM_BOW, UPG_QUIVER),
    MMI_SLOT("Inventory", ITEM_ARROW_FIRE),
    MMI_SLOT("Inventory", ITEM_ARROW_ICE),
    MMI_SLOT("Inventory", ITEM_ARROW_LIGHT),
    MMI_AMMO(ITEM_BOMB, UPG_BOMB_BAG),
    { "Inventory", MMRK_SLOT_BOMBCHU, (uint8_t)ITEM_BOMBCHU, 0, RI_UNKNOWN },
    MMI_AMMO(ITEM_DEKU_STICK, UPG_DEKU_STICKS),
    MMI_AMMO(ITEM_DEKU_NUT, UPG_DEKU_NUTS),
    MMI_FIXED(ITEM_MAGIC_BEANS, 20), // z_parameter.c Item_Give: at most 20
    MMI_FIXED(ITEM_POWDER_KEG, 1),   // z_parameter.c Inventory_ChangeAmmo: at most 1
    MMI_SLOT("Inventory", ITEM_PICTOGRAPH_BOX),
    MMI_SLOT("Inventory", ITEM_LENS_OF_TRUTH),
    MMI_SLOT("Inventory", ITEM_HOOKSHOT),
    MMI_SLOT("Inventory", ITEM_SWORD_GREAT_FAIRY),
    { "Inventory", MMRK_BOTTLES, (uint8_t)ITEM_BOTTLE, 6, RI_BOTTLE_EMPTY },
    // Masks: the native "Masks" group, in slot order
    MMI_SLOT("Masks", ITEM_MASK_POSTMAN),
    MMI_SLOT("Masks", ITEM_MASK_ALL_NIGHT),
    MMI_SLOT("Masks", ITEM_MASK_BLAST),
    MMI_SLOT("Masks", ITEM_MASK_STONE),
    MMI_SLOT("Masks", ITEM_MASK_GREAT_FAIRY),
    MMI_SLOT("Masks", ITEM_MASK_DEKU),
    MMI_SLOT("Masks", ITEM_MASK_KEATON),
    MMI_SLOT("Masks", ITEM_MASK_BREMEN),
    MMI_SLOT("Masks", ITEM_MASK_BUNNY),
    MMI_SLOT("Masks", ITEM_MASK_DON_GERO),
    MMI_SLOT("Masks", ITEM_MASK_SCENTS),
    MMI_SLOT("Masks", ITEM_MASK_GORON),
    MMI_SLOT("Masks", ITEM_MASK_ROMANI),
    MMI_SLOT("Masks", ITEM_MASK_CIRCUS_LEADER),
    MMI_SLOT("Masks", ITEM_MASK_KAFEIS_MASK),
    MMI_SLOT("Masks", ITEM_MASK_COUPLE),
    MMI_SLOT("Masks", ITEM_MASK_TRUTH),
    MMI_SLOT("Masks", ITEM_MASK_ZORA),
    MMI_SLOT("Masks", ITEM_MASK_KAMARO),
    MMI_SLOT("Masks", ITEM_MASK_GIBDO),
    MMI_SLOT("Masks", ITEM_MASK_GARO),
    MMI_SLOT("Masks", ITEM_MASK_CAPTAIN),
    MMI_SLOT("Masks", ITEM_MASK_GIANT),
    MMI_SLOT("Masks", ITEM_MASK_FIERCE_DEITY),
    // Songs: the native "Songs" group, in its order
    MMI_QUEST("Songs", QUEST_SONG_TIME, RI_SONG_TIME),
    MMI_QUEST("Songs", QUEST_SONG_HEALING, RI_SONG_HEALING),
    MMI_QUEST("Songs", QUEST_SONG_EPONA, RI_SONG_EPONA),
    MMI_QUEST("Songs", QUEST_SONG_SOARING, RI_SONG_SOARING),
    MMI_QUEST("Songs", QUEST_SONG_STORMS, RI_SONG_STORMS),
    MMI_QUEST("Songs", QUEST_SONG_SONATA, RI_SONG_SONATA),
    MMI_KIND("Songs", MMRK_LULLABY, RI_SONG_LULLABY),
    MMI_QUEST("Songs", QUEST_SONG_BOSSA_NOVA, RI_SONG_NOVA),
    MMI_QUEST("Songs", QUEST_SONG_ELEGY, RI_SONG_ELEGY),
    MMI_QUEST("Songs", QUEST_SONG_OATH, RI_SONG_OATH),
    // Quest: the native "Quest" group, in its order
    MMI_QUEST("Quest", QUEST_REMAINS_ODOLWA, RI_REMAINS_ODOLWA),
    MMI_QUEST("Quest", QUEST_REMAINS_GOHT, RI_REMAINS_GOHT),
    MMI_QUEST("Quest", QUEST_REMAINS_GYORG, RI_REMAINS_GYORG),
    MMI_QUEST("Quest", QUEST_REMAINS_TWINMOLD, RI_REMAINS_TWINMOLD),
    MMI_QUEST("Quest", QUEST_BOMBERS_NOTEBOOK, RI_BOMBERS_NOTEBOOK),
    MMI_KIND("Quest", MMRK_SWORD, RI_SWORD_KOKIRI),
    MMI_KIND("Quest", MMRK_SHIELD, RI_SHIELD_HERO),
    MMI_KIND("Quest", MMRK_MAGIC, RI_SINGLE_MAGIC),
    MMI_KIND("Quest", MMRK_DOUBLE_DEFENSE, RI_DOUBLE_DEFENSE),
    MMI_KIND("Quest", MMRK_WALLET, RI_WALLET_ADULT),
    MMI_KIND("Quest", MMRK_HEART_PIECES, RI_HEART_PIECE),
    // Tokens, Stray Fairies and Dungeon (#1140): the rest of MM's default preset
    // (ItemTrackerSettings.cpp ApplyDefaultItemPreset), in its order.
    { "Tokens", MMRK_TOKENS, 0, 16, RI_GS_TOKEN_SWAMP },
    { "Tokens", MMRK_TOKENS, 0, 0, RI_GS_TOKEN_OCEAN },
    MMI_KIND("Stray Fairies", MMRK_FAIRY_CLOCK_TOWN, RI_CLOCK_TOWN_STRAY_FAIRY),
    MMI_FAIRY(WOODFALL),
    MMI_FAIRY(SNOWHEAD),
    MMI_FAIRY(GREAT_BAY),
    MMI_FAIRY(STONE_TOWER),
    // Dungeon, 4 wide: one column per temple, one line per item kind.
    MMI_DUNGEON(WOODFALL, BOSS_KEY),
    MMI_DUNGEON(SNOWHEAD, BOSS_KEY),
    MMI_DUNGEON(GREAT_BAY, BOSS_KEY),
    MMI_DUNGEON(STONE_TOWER, BOSS_KEY),
    MMI_KEYS(WOODFALL, 1),
    MMI_KEYS(SNOWHEAD, 3),
    MMI_KEYS(GREAT_BAY, 1),
    MMI_KEYS(STONE_TOWER, 4),
    MMI_DUNGEON(WOODFALL, MAP),
    MMI_DUNGEON(SNOWHEAD, MAP),
    MMI_DUNGEON(GREAT_BAY, MAP),
    MMI_DUNGEON(STONE_TOWER, MAP),
    MMI_DUNGEON(WOODFALL, COMPASS),
    MMI_DUNGEON(SNOWHEAD, COMPASS),
    MMI_DUNGEON(GREAT_BAY, COMPASS),
    MMI_DUNGEON(STONE_TOWER, COMPASS),
};

#undef MMI_SLOT
#undef MMI_AMMO
#undef MMI_FIXED
#undef MMI_QUEST
#undef MMI_KIND
#undef MMI_FAIRY
#undef MMI_DUNGEON
#undef MMI_KEYS

constexpr int kMMItemRowCount = (int)(sizeof(kMMItemRows) / sizeof(kMMItemRows[0]));

// The ocarina-button rows (#1072), after the table above: button i is the Rando
// item RI_OCARINA_BUTTON_A + i and the randoInf flag RANDO_INF_OBTAINED_OCARINA_BUTTON_A
// + i (A, C-Down, C-Right, C-Left, C-Up; ConvertItem.cpp and GiveItem.cpp index
// both the same way). Every gate on these rows is MMItemButtonRowsShown.
constexpr int kMMButtonRowCount = 5;
constexpr const char* kMMButtonGroup = "Ocarina Buttons";

// MM's tracker tints the white button glyphs (ShipUtils.cpp randoItemColorMap):
// the A button blue, the four C buttons yellow. 0xRRGGBBAA.
constexpr uint32_t kMMButtonTintA = 0x167ECBFFu;
constexpr uint32_t kMMButtonTintC = 0xD6C417FFu;

/** Does this file shuffle the ocarina buttons? The frozen option of a randomizer
 *  file, as the note gate reads it; the one place to change when the rows move
 *  to the Shared column. */
bool MMItemButtonRowsShown(const SaveContext* src) {
    return IS_RANDO && RANDO_SAVE_OPTIONS[RO_SHUFFLE_OCARINA_BUTTONS] != 0;
}

/** Does the buffer's save hold ocarina button `button` (0..4)? Flags_GetRandoInf's
 *  read, over the handed buffer. */
bool MMItemButtonHeld(const SaveContext* src, int button) {
    const int flag = RANDO_INF_OBTAINED_OCARINA_BUTTON_A + button;
    return (gSaveContext.save.shipSaveInfo.rando.randoInf[flag >> 4] & (1 << (flag & 0xF))) != 0;
}

bool MMItemDeriveButton(const SaveContext* src, int button, ComboItemRow* out) {
    if (button < 0 || button >= kMMButtonRowCount || !MMItemButtonRowsShown(src)) {
        return false;
    }
    out->group = kMMButtonGroup;
    MMItemFillRando(out, (RandoItemId)(RI_OCARINA_BUTTON_A + button));
    out->iconTint = (button == 0) ? kMMButtonTintA : kMMButtonTintC;
    out->have = MMItemButtonHeld(src, button);
    // #1067: each button is MM's copy of a Shared Ocarina Buttons family; under
    // the frozen rule the view draws it once, in the Shared group.
    static const uint32_t kSlots[kMMButtonRowCount] = {
        RSBS_XITEM_SHARED_OCARINA_BUTTONS, RSBS_XFAMILY_SLOT_OCARINA_C_DOWN, RSBS_XFAMILY_SLOT_OCARINA_C_RIGHT,
        RSBS_XFAMILY_SLOT_OCARINA_C_LEFT, RSBS_XFAMILY_SLOT_OCARINA_C_UP
    };
    out->sharedFamily = kSlots[button];
    return true;
}

/** An inventory slot: the held item names the row, the empty slot falls back to
 *  the base item (MM's tracker: safeItemsForInventorySlot[slot][0]). */
void MMItemFillSlot(const SaveContext* src, const MMItemRowDef& def, ComboItemRow* out) {
    const uint8_t content = INV_CONTENT(def.item);
    out->have = content != ITEM_NONE;
    MMItemFillVanilla(out, out->have ? content : def.item);
}

/** The shared-resource kind of capacity upgrade `upg` (#967), or NONE when the pair does not share it. */
uint8_t MMItemSharedTierKindOf(uint32_t upg) {
    switch (upg) {
        case UPG_QUIVER:
            return RSBS_SHARED_RES_QUIVER_TIER;
        case UPG_BOMB_BAG:
            return RSBS_SHARED_RES_BOMB_BAG_TIER;
        case UPG_DEKU_STICKS:
            return RSBS_SHARED_RES_STICK_TIER;
        case UPG_DEKU_NUTS:
            return RSBS_SHARED_RES_NUT_TIER;
        default:
            return RSBS_SHARED_RES_NONE;
    }
}

bool MMItemDerive(const SaveContext* src, const MMItemRowDef& def, ComboItemRow* out) {
    out->group = def.group;
    if (std::strcmp(def.group, "Songs") == 0) {
        out->iconAspect = 32.0f / 46.0f; // GetImageObject: a song's texture is 32 wide in the 46 px cell
    }
    switch (def.kind) {
        case MMRK_SLOT:
            MMItemFillSlot(src, def, out);
            if (def.item == (uint8_t)ITEM_HOOKSHOT && out->have) {
                out->identifier = 'H'; // #1143: SoH's Hookshot Identifier; MM has no Longshot
            }
            return true;
        case MMRK_SLOT_AMMO:
        case MMRK_SLOT_BOMBCHU:
        case MMRK_SLOT_FIXED:
            MMItemFillSlot(src, def, out);
            if (def.kind == MMRK_SLOT_AMMO) {
                // This game's copy of a shared capacity tier: the view dims it once the Shared column holds it.
                out->sharedKind = MMItemSharedTierKindOf(def.data);
                out->sharedTier = (uint8_t)CUR_UPG_VALUE(def.data);
            }
            if (out->have) {
                out->count = AMMO(def.item);
                out->max = (def.kind == MMRK_SLOT_FIXED)     ? (int)def.data
                           : (def.kind == MMRK_SLOT_BOMBCHU) ? (int)CUR_CAPACITY(UPG_BOMB_BAG)
                                                             : (int)CUR_CAPACITY(def.data);
            }
            // #1143: SoH's maxCapacity, the largest bag. A fixed cap never grows (0), and bombchus
            // are SoH's static ammo (DrawItemCount's shouldDisplayAmmo): no ceiling of their own,
            // so every count mode shows the amount (of the bag's capacity), never the bag's tier.
            out->capacityMax =
                (def.kind == MMRK_SLOT_FIXED || def.kind == MMRK_SLOT_BOMBCHU) ? 0 : (int)CAPACITY(def.data, 3);
            return true;
        case MMRK_BOTTLES: {
            MMItemFillRando(out, def.rando);
            out->name = "Bottles";
            int held = 0;
            for (int slot = SLOT_BOTTLE_1; slot <= SLOT_BOTTLE_6; slot++) {
                if (gSaveContext.save.saveInfo.inventory.items[slot] != ITEM_NONE) {
                    held++;
                }
            }
            out->have = held > 0;
            out->count = held;
            out->max = (int)def.data;
            return true;
        }
        case MMRK_QUEST:
            MMItemFillRando(out, def.rando);
            out->have = CHECK_QUEST_ITEM(def.data) != 0;
            return true;
        case MMRK_LULLABY: {
            // Progressive (RI_PROGRESSIVE_LULLABY): the row names the tier held.
            const bool full = CHECK_QUEST_ITEM(QUEST_SONG_LULLABY) != 0;
            const bool intro = CHECK_QUEST_ITEM(QUEST_SONG_LULLABY_INTRO) != 0;
            MMItemFillRando(out, (!full && intro) ? RI_SONG_LULLABY_INTRO : RI_SONG_LULLABY);
            out->have = full || intro;
            return true;
        }
        case MMRK_SWORD: {
            const u32 tier = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
            static const RandoItemId kSwords[] = { RI_SWORD_KOKIRI, RI_SWORD_KOKIRI, RI_SWORD_RAZOR, RI_SWORD_GILDED };
            MMItemFillRando(out, kSwords[tier <= EQUIP_VALUE_SWORD_GILDED ? tier : EQUIP_VALUE_SWORD_GILDED]);
            out->have = tier > EQUIP_VALUE_SWORD_NONE;
            if (tier <= EQUIP_VALUE_SWORD_KOKIRI) {
                // The shared Kokiri Sword (#886): this row is its copy while the sword held is the
                // Kokiri one; a Razor or Gilded Sword is MM's alone and stays lit.
                out->sharedKind = RSBS_SHARED_RES_KOKIRI_SWORD;
                out->sharedTier = (tier == EQUIP_VALUE_SWORD_KOKIRI) ? 1 : 0;
            }
            return true;
        }
        case MMRK_SHIELD: {
            const u32 tier = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
            MMItemFillRando(out, tier >= EQUIP_VALUE_SHIELD_MIRROR ? RI_SHIELD_MIRROR : RI_SHIELD_HERO);
            out->have = tier > EQUIP_VALUE_SHIELD_NONE;
            return true;
        }
        case MMRK_MAGIC: {
            const bool magic = gSaveContext.save.saveInfo.playerData.isMagicAcquired != 0;
            const bool doubleMagic = gSaveContext.save.saveInfo.playerData.isDoubleMagicAcquired != 0;
            MMItemFillRando(out, doubleMagic ? RI_DOUBLE_MAGIC : RI_SINGLE_MAGIC);
            out->have = magic || doubleMagic;
            return true;
        }
        case MMRK_DOUBLE_DEFENSE:
            MMItemFillRando(out, def.rando);
            out->have = gSaveContext.save.saveInfo.playerData.doubleDefense != 0;
            return true;
        case MMRK_WALLET: {
            // MM's tracker: held from the Adult's Wallet on; the rupees are shown
            // against the current wallet's capacity at every tier.
            const u32 tier = CUR_UPG_VALUE(UPG_WALLET);
            MMItemFillRando(out, tier >= 2 ? RI_WALLET_GIANT : RI_WALLET_ADULT);
            out->have = tier >= 1;
            out->count = gSaveContext.save.saveInfo.playerData.rupees;
            out->max = (int)CUR_CAPACITY(UPG_WALLET);
            // #1143: the Giant's 500, or OoT's Tycoon's 999 once the shared tier reaches it (#1283), as OoT's row.
            out->capacityMax = (int)CAPACITY(UPG_WALLET, tier >= 3 ? 3 : 2);
            return true;
        }
        case MMRK_HEART_PIECES: {
            MMItemFillRando(out, def.rando);
            out->name = "Heart Pieces";
            out->count =
                (int)((gSaveContext.save.saveInfo.inventory.questItems & kMMHeartPieceMask) >> kMMHeartPieceShift);
            out->max = 4;
            out->have = out->count > 0;
            return true;
        }
        case MMRK_TOKENS:
            // Inventory_GetSkullTokenCount: Swamp in the high half, Ocean in the low. The ceiling is
            // MM's tracker's (ItemTracker.cpp GetItemCounts); MM's glow backdrop has no row field.
            MMItemFillRando(out, def.rando);
            out->count = (int)((gSaveContext.save.saveInfo.skullTokenCount >> def.data) & 0xFFFFu);
            out->max = IS_RANDO ? (int)RANDO_SAVE_OPTIONS[RO_MINIMUM_SKULLTULA_TOKENS] : SPIDER_HOUSE_TOKENS_REQUIRED;
            out->have = out->count > 0;
            return true;
        case MMRK_FAIRY_CLOCK_TOWN:
            // GiveItem.cpp: the Clock Town fairy is a week-event bit; its icon is the Stone Tower one.
            MMItemFillRando(out, def.rando);
            out->iconTint = kMMTintClockTownFairy;
            out->have = CHECK_WEEKEVENTREG(WEEKEVENTREG_08_80) != 0;
            return true;
        case MMRK_FAIRY: {
            MMItemFillRando(out, def.rando);
            const s8 held = gSaveContext.save.saveInfo.inventory.strayFairies[def.data];
            out->count = held < 0 ? 0 : (int)held;
            out->max = IS_RANDO ? (int)RANDO_SAVE_OPTIONS[RO_MINIMUM_STRAY_FAIRIES] : STRAY_FAIRY_SCATTERED_TOTAL;
            out->have = out->count > 0;
            return true;
        }
        case MMRK_DUNGEON_ITEM:
            // GetIconTexturePath, never MM_gItemIcons[ITEM_COMPASS]: that slot holds the map's
            // texture and ITEM_DUNGEON_MAP's the compass's (z_inventory.c).
            MMItemFillRando(out, def.rando);
            if (def.item == (uint8_t)DUNGEON_BOSS_KEY) {
                out->iconTint = kMMTintTemple[def.data];
            }
            out->have = CHECK_DUNGEON_ITEM(def.item, def.data) != 0;
            return true;
        case MMRK_SMALL_KEY: {
            // Collected: the keys found in a randomizer file (foundDungeonKeys, written by
            // Rando::GiveItem and never by vanilla play), otherwise the keys held; a temple never
            // entered holds -1. Lit while a key is in hand (MM's tracker: DUNGEON_KEY_COUNT > 0).
            MMItemFillRando(out, def.rando);
            out->iconTint = kMMTintTemple[def.data];
            const s8 heldRaw = DUNGEON_KEY_COUNT(def.data);
            const int held = heldRaw < 0 ? 0 : (int)heldRaw;
            const s8 foundRaw = gSaveContext.save.shipSaveInfo.rando.foundDungeonKeys[def.data];
            out->count = IS_RANDO ? (foundRaw < 0 ? 0 : (int)foundRaw) : held;
            out->max = (int)def.item;
            out->countKind = (uint8_t)COMBO_ITEM_COUNT_KIND_KEYS; // #1143: SoH's Key Count Tracking reads these
            out->held = held;
            out->have = held > 0;
            return true;
        }
    }
    return false;
}

// ---- the shuffle-gated groups (#1072, #1140) -----------------------------------

/** Is the buffer's frozen option `option` on in a randomizer file? GeneratePools' test, over the buffer. */
bool MMItemOptionOn(const SaveContext* src, RandoOptionId option) {
    return IS_RANDO && RANDO_SAVE_OPTIONS[option] == RO_GENERIC_YES;
}

/** Flags_GetRandoInf's read, over the handed buffer. */
bool MMItemRandoInf(const SaveContext* src, int flag) {
    return (gSaveContext.save.shipSaveInfo.rando.randoInf[flag >> 4] & (1 << (flag & 0xF))) != 0;
}

int MMItemButtonRows(const SaveContext* src) {
    return MMItemButtonRowsShown(src) ? kMMButtonRowCount : 0;
}

// Boss Souls, in MM's order (ItemTrackerSettings.cpp); Majora last, so Triforce Hunt drops the tail.
const RandoItemId kMMBossSouls[] = { RI_SOUL_BOSS_ODOLWA, RI_SOUL_BOSS_GOHT, RI_SOUL_BOSS_GYORG, RI_SOUL_BOSS_TWINMOLD,
                                     RI_SOUL_BOSS_MAJORA };

int MMItemBossSoulRows(const SaveContext* src) {
    if (!MMItemOptionOn(src, RO_SHUFFLE_BOSS_SOULS)) {
        return 0;
    }
    return MMItemOptionOn(src, RO_SHUFFLE_TRIFORCE_PIECES) ? 4 : 5; // GeneratePools: no Majora's soul then
}

bool MMItemDeriveBossSoul(const SaveContext* src, int i, ComboItemRow* out) {
    MMItemFillRando(out, kMMBossSouls[i]);
    out->have = MMItemRandoInf(src, SOUL_RI_TO_RANDO_INF(kMMBossSouls[i]));
    return true;
}

// The 47 enemy-soul items. Counted through SOUL_RI_TO_RANDO_INF, the flag GiveItem sets: the 48
// named RANDO_INF_OBTAINED_SOUL_OF_ENEMY_* flags sit one off from PEAHAT on, and the last is set
// by nothing.
constexpr int kMMEnemySoulCount = (int)RI_SOUL_ENEMY_WOLFOS - (int)RI_SOUL_ENEMY_ALIEN + 1;
static_assert(kMMEnemySoulCount == 47, "MM's enemy-soul item range changed; the Enemy Souls ceiling follows it");

int MMItemEnemySoulRows(const SaveContext* src) {
    return MMItemOptionOn(src, RO_SHUFFLE_ENEMY_SOULS) ? 1 : 0;
}

bool MMItemDeriveEnemySouls(const SaveContext* src, int, ComboItemRow* out) {
    MMItemFillRando(out, RI_SOUL_ENEMY_ALIEN); // every enemy soul shares the skull icon
    out->name = "Enemy Souls";
    int held = 0;
    for (int ri = RI_SOUL_ENEMY_ALIEN; ri <= RI_SOUL_ENEMY_WOLFOS; ri++) {
        held += MMItemRandoInf(src, SOUL_RI_TO_RANDO_INF(ri)) ? 1 : 0;
    }
    out->count = held;
    out->max = kMMEnemySoulCount;
    out->have = held > 0;
    return true;
}

// Owl Statues, in MM's order, with the owl each activates (GiveItem.cpp Sram_ActivateOwl).
struct MMItemOwl {
    RandoItemId rando;
    uint8_t warp;
};
const MMItemOwl kMMOwls[] = {
    { RI_OWL_CLOCK_TOWN_SOUTH, OWL_WARP_CLOCK_TOWN },       { RI_OWL_MILK_ROAD, OWL_WARP_MILK_ROAD },
    { RI_OWL_SOUTHERN_SWAMP, OWL_WARP_SOUTHERN_SWAMP },     { RI_OWL_WOODFALL, OWL_WARP_WOODFALL },
    { RI_OWL_MOUNTAIN_VILLAGE, OWL_WARP_MOUNTAIN_VILLAGE }, { RI_OWL_SNOWHEAD, OWL_WARP_SNOWHEAD },
    { RI_OWL_GREAT_BAY_COAST, OWL_WARP_GREAT_BAY_COAST },   { RI_OWL_ZORA_CAPE, OWL_WARP_ZORA_CAPE },
    { RI_OWL_IKANA_CANYON, OWL_WARP_IKANA_CANYON },         { RI_OWL_STONE_TOWER, OWL_WARP_STONE_TOWER },
};

int MMItemOwlRows(const SaveContext* src) {
    return MMItemOptionOn(src, RO_SHUFFLE_OWL_STATUES) ? (int)(sizeof(kMMOwls) / sizeof(kMMOwls[0])) : 0;
}

bool MMItemDeriveOwl(const SaveContext* src, int i, ComboItemRow* out) {
    MMItemFillRando(out, kMMOwls[i].rando);
    out->iconAspect = 46.0f / 24.0f; // MM's tracker: the owl face drawn 24 px tall in its 46 px cell
    out->have = GET_OWL_STATUE_ACTIVATED(kMMOwls[i].warp) != 0;
    return true;
}

// Time: the six half-days in the clock's order; half-day i is RANDO_INF_OBTAINED_CLOCK_DAY_1 + i
// (GiveItem.cpp, ClockShuffle's half-day index).
const RandoItemId kMMHalfDays[] = { RI_TIME_DAY_1,   RI_TIME_NIGHT_1, RI_TIME_DAY_2,
                                    RI_TIME_NIGHT_2, RI_TIME_DAY_3,   RI_TIME_NIGHT_3 };

int MMItemTimeRows(const SaveContext* src) {
    return MMItemOptionOn(src, RO_CLOCK_SHUFFLE) ? (int)(sizeof(kMMHalfDays) / sizeof(kMMHalfDays[0])) : 0;
}

bool MMItemDeriveHalfDay(const SaveContext* src, int i, ComboItemRow* out) {
    MMItemFillRando(out, kMMHalfDays[i]);
    out->iconTint = (i % 2 == 0) ? kMMTintDay : kMMTintNight;
    out->have = MMItemRandoInf(src, RANDO_INF_OBTAINED_CLOCK_DAY_1 + i);
    return true;
}

int MMItemMiscRows(const SaveContext* src) {
    return MMItemOptionOn(src, RO_SHUFFLE_SWIM) ? 1 : 0;
}

bool MMItemDeriveSwim(const SaveContext* src, int, ComboItemRow* out) {
    MMItemFillRando(out, RI_ABILITY_SWIM); // the Zora Mask's icon, GetIconTexturePath's pick
    out->have = MMItemRandoInf(src, RANDO_INF_OBTAINED_SWIM);
    return true;
}

/** One gated group: its rows for a buffer (0 while its option is off) and row `i` of it. */
struct MMItemGatedGroup {
    const char* group;
    int (*rows)(const SaveContext* src);
    bool (*derive)(const SaveContext* src, int i, ComboItemRow* out);
};

// MM's order (ItemTrackerSettings.cpp LoadAvailableWindows). The strings are MM's group names.
const MMItemGatedGroup kMMItemGatedGroups[] = {
    { kMMButtonGroup, MMItemButtonRows, MMItemDeriveButton },
    { "Boss Souls", MMItemBossSoulRows, MMItemDeriveBossSoul },
    { "Enemy Souls", MMItemEnemySoulRows, MMItemDeriveEnemySouls },
    { "Owl Statues", MMItemOwlRows, MMItemDeriveOwl },
    { "Time", MMItemTimeRows, MMItemDeriveHalfDay },
    { "Misc", MMItemMiscRows, MMItemDeriveSwim },
};

/** How many gated rows this buffer shows. */
int MMItemGatedRowCount(const SaveContext* src) {
    int rows = 0;
    for (const MMItemGatedGroup& g : kMMItemGatedGroups) {
        rows += g.rows(src);
    }
    return rows;
}

/** Gated row `index` (0-based over the groups shown), or false past the last. */
bool MMItemDeriveGated(const SaveContext* src, int index, ComboItemRow* out) {
    for (const MMItemGatedGroup& g : kMMItemGatedGroups) {
        const int rows = g.rows(src);
        if (index < rows) {
            out->group = g.group;
            return g.derive(src, index, out);
        }
        index -= rows;
    }
    return false;
}

// ---- the vtable ----------------------------------------------------------------

int MMItemCount(void) {
    return kMMItemRowCount;
}

/** The rows this buffer shows: the fixed rows plus the gated groups its file's
 *  frozen options switch on (#1072, #1140). */
int MMItemCountFor(const void* buf) {
    if (!MMItemAligned(buf)) {
        return kMMItemRowCount;
    }
    const SaveContext* src = static_cast<const SaveContext*>(buf);
    return kMMItemRowCount + MMItemGatedRowCount(src);
}

bool MMItemHasSave(const void* buf) {
    if (!MMItemAligned(buf)) {
        return false;
    }
    const SaveContext* src = static_cast<const SaveContext*>(buf);
    // MM's "newf" sentinel (SaveManager.cpp's RsbsRegisterMMMetaOnce, the check
    // tracker's descriptor): an all-zero shadow is "no data", not empty pockets.
    static const char kNewf[6] = { 'Z', 'E', 'L', 'D', 'A', '3' };
    return std::memcmp(gSaveContext.save.saveInfo.playerData.newf, kNewf, sizeof(kNewf)) == 0;
}

/** The shared family (foreign_items.h) a slot row's item is MM's own copy of,
 *  or 0: the view draws an armed family once, in the Shared group (#1065). */
uint32_t MMItemSharedFamily(const MMItemRowDef& def) {
    if (def.kind == MMRK_SLOT && def.item == (uint8_t)ITEM_LENS_OF_TRUTH) {
        return RSBS_XITEM_SHARED_LENS;
    }
    if (def.kind == MMRK_QUEST && def.data == (uint32_t)QUEST_SONG_EPONA) {
        return RSBS_XITEM_SHARED_SONG_EPONA; // #1066
    }
    if (def.kind == MMRK_QUEST && def.data == (uint32_t)QUEST_SONG_STORMS) {
        return RSBS_XITEM_SHARED_SONG_STORMS; // #1066
    }
    if (def.kind == MMRK_QUEST && def.data == (uint32_t)QUEST_SONG_TIME) {
        return RSBS_XITEM_SHARED_SONG_TIME; // #1069
    }
    if (def.kind == MMRK_SLOT && def.item == (uint8_t)ITEM_ARROW_FIRE) {
        return RSBS_XITEM_SHARED_ARROW_FIRE; // #968
    }
    if (def.kind == MMRK_SLOT && def.item == (uint8_t)ITEM_ARROW_ICE) {
        return RSBS_XITEM_SHARED_ARROW_ICE; // #968
    }
    if (def.kind == MMRK_SLOT && def.item == (uint8_t)ITEM_ARROW_LIGHT) {
        return RSBS_XITEM_SHARED_ARROW_LIGHT; // #968
    }
    if (def.kind == MMRK_SLOT && def.item == (uint8_t)ITEM_MASK_GORON) {
        return RSBS_XITEM_SHARED_GORON; // #978
    }
    if (def.kind == MMRK_SLOT && def.item == (uint8_t)ITEM_MASK_ZORA) {
        return RSBS_XITEM_SHARED_ZORA; // #978
    }
    if (def.kind == MMRK_SLOT && def.item == (uint8_t)ITEM_MASK_BUNNY) {
        return RSBS_XITEM_SHARED_BUNNY; // #978
    }
    if (def.kind == MMRK_SLOT && def.item == (uint8_t)ITEM_MASK_KEATON) {
        return RSBS_XITEM_SHARED_KEATON; // #978
    }
    if (def.kind == MMRK_SLOT && def.item == (uint8_t)ITEM_MASK_TRUTH) {
        return RSBS_XITEM_SHARED_TRUTH; // #978
    }
    return 0u;
}

bool MMItemRowAt(const void* buf, int index, ComboItemRow* out) {
    if (!MMItemAligned(buf) || out == nullptr || index < 0) {
        return false;
    }
    const SaveContext* src = static_cast<const SaveContext*>(buf);
    const uint8_t freshness = out->freshness; // the view's field; left alone
    std::memset(out, 0, sizeof(*out));
    out->freshness = freshness;
    if (index >= kMMItemRowCount) {
        return MMItemDeriveGated(src, index - kMMItemRowCount, out);
    }
    if (!MMItemDerive(src, kMMItemRows[index], out)) {
        return false;
    }
    out->sharedFamily = MMItemSharedFamily(kMMItemRows[index]);
    return true;
}

} // namespace

#undef gSaveContext
// ============================================================================
// Below: the real global again (nothing here reads a save).
// ============================================================================

namespace {

/** MM's own tracker tables (ItemTrackerSettings.cpp LoadAvailableWindows): the
 *  Inventory, Masks, Enemy Souls, Time and Misc tables are 6 wide; Songs, Quest,
 *  Stray Fairies, Ocarina Buttons, Boss Souls and Owl Statues 5; Dungeon 4;
 *  Tokens 2. */
int MMItemGroupColumns(const char* group) {
    if (group == nullptr) {
        return 6;
    }
    if (std::strcmp(group, "Tokens") == 0) {
        return 2;
    }
    if (std::strcmp(group, "Dungeon") == 0) {
        return 4;
    }
    if (std::strcmp(group, "Songs") == 0 || std::strcmp(group, "Quest") == 0 ||
        std::strcmp(group, "Stray Fairies") == 0 || std::strcmp(group, kMMButtonGroup) == 0 ||
        std::strcmp(group, "Boss Souls") == 0 || std::strcmp(group, "Owl Statues") == 0) {
        return 5;
    }
    return 6;
}

/**
 * MM's grid (ItemTracker.cpp DrawItemTrackerGroup): one table per group, its
 * 46 px cells (ITEM_TEXTURE_SIZE at Scale 1) spaced by the table's default
 * cell padding, 8 px across and 4 px down (measured on MM's own window, 54 and
 * 50 px pitch), the count bottom right inside the cell (DrawItemCounts).
 */
const ComboItemGridStyle kMMItemGrid = {
    46.0f, 8.0f, 6, true, MMItemGroupColumns, (uint8_t)COMBO_ITEM_COUNT_MM, 4.0f,
};

/**
 * The tint MM's own UI draws its grayscale rupee counter icon in
 * (CheckTracker.cpp: a freestanding rupee check, ImVec4(0.78f, 1, 0.39f, 1)).
 */
constexpr uint32_t kMMRupeeTint = 0xC7FF63FFu;

/**
 * MM's own icon for a shared-pool row (#458 U3): the textures MM's
 * LoadGuiTextures registers under their resource paths (MM_gItemIcons, and
 * the rupee counter icon among its miscellaneous textures), the ones MM's
 * tracker and its Rando item table draw. The wallet prints its capacity at
 * every tier, as MM's tracker does (GetItemCounts: CUR_CAPACITY(UPG_WALLET)).
 * It stays held at tier 0, the child's wallet every file holds (the pool's
 * rule, combo_item_view.c), where MM's own tracker fades it.
 */
void MMItemSharedIcon(uint8_t kind, uint16_t tier, ComboItemRow* row) {
    const char* key = nullptr;
    uint32_t tint = 0;
    switch (kind) {
        case RSBS_SHARED_RES_RUPEES:
            key = dgRupeeCounterIconTex;
            tint = kMMRupeeTint;
            break;
        case RSBS_SHARED_RES_WALLET_TIER:
            key = MMItemIcon(tier >= 2 ? ITEM_WALLET_GIANT : ITEM_WALLET_ADULT);
            row->iconNumber = (int)CAPACITY(UPG_WALLET, tier > 3 ? 3 : tier);
            break;
        case RSBS_SHARED_RES_HEALTH_QUARTERS:
            // The pooled pieces toward the next heart (#1159) draw the piece MM's Quest row does.
            key = (tier == COMBO_ITEM_SHARED_ICON_HEART_PIECE) ? MMItemRandoIcon(RI_HEART_PIECE)
                                                               : MMItemIcon(ITEM_HEART_CONTAINER);
            break;
        case RSBS_SHARED_RES_DOUBLE_DEFENSE:
            key = MMItemRandoIcon(RI_DOUBLE_DEFENSE);
            break;
        case RSBS_SHARED_RES_MAGIC_LEVEL:
            key = MMItemIcon(tier >= 2 ? ITEM_MAGIC_JAR_BIG : ITEM_MAGIC_JAR_SMALL);
            break;
        case RSBS_SHARED_RES_ARROW_COUNT:
            // The quiver tiers as the player knows them (#967): the Bow, then
            // the Big and Biggest Quivers.
            key = MMItemIcon(tier >= 3 ? ITEM_QUIVER_50 : (tier == 2 ? ITEM_QUIVER_40 : ITEM_BOW));
            break;
        case RSBS_SHARED_RES_BOMB_COUNT:
            key = MMItemIcon(tier >= 3 ? ITEM_BOMB_BAG_40 : (tier == 2 ? ITEM_BOMB_BAG_30 : ITEM_BOMB_BAG_20));
            break;
        case RSBS_SHARED_RES_BOMBCHU_COUNT:
            key = MMItemIcon(ITEM_BOMBCHU);
            break;
        case RSBS_SHARED_RES_STICK_COUNT:
            key = MMItemIcon(ITEM_DEKU_STICK);
            break;
        case RSBS_SHARED_RES_NUT_COUNT:
            key = MMItemIcon(ITEM_DEKU_NUT);
            break;
        case RSBS_SHARED_RES_HOOKSHOT_TIER:
            key = MMItemIcon(ITEM_HOOKSHOT);
            break;
        case RSBS_SHARED_RES_OCARINA_TIER:
            key = MMItemIcon(ITEM_OCARINA_OF_TIME);
            break;
        case RSBS_SHARED_RES_KOKIRI_SWORD:
            key = MMItemRandoIcon(RI_SWORD_KOKIRI);
            break;
        case RSBS_SHARED_RES_TRIFORCE_PIECES:
            key = MMItemRandoIcon(RI_TRIFORCE_PIECE);
            break;
        case RSBS_SHARED_RES_BOTTLE_COUNT: // #970
            key = MMItemIcon(ITEM_BOTTLE);
            break;
        default:
            break;
    }
    row->iconKey = key;
    row->iconKeyFaded = key;
    row->iconAspect = 0.0f;
    row->fadedAlpha = kMMItemFadedAlpha;
    row->iconTint = tint;
}

// The live view reads the HANDED buffer (the view's liveSave, which production points at the process
// global and the ROM-free test points at an authored one), so the accessor macros are redirected again.
#define gSaveContext (*src)

/**
 * The Shared column's LIVE view (#971), the twin of OoT's OoTItemSharedLive: the
 * value of shared kind `kind` in the save `src` over MM_HarvestSharedResources'
 * arithmetic (GameExports_SingleExe.cpp), READ ONLY: no rupee accumulator settle,
 * no pool or watermark write, nothing stored in `src`. Gated like the harvest, on
 * a played file (Combo_SaveIsLiveFile over the buffer's own game mode: MM plays
 * real frames under its title screen, whose bootstrap save is not the player's).
 * MM has no longshot, so its hookshot tier tops out at 1. False for a kind this
 * view has no reading of.
 */
bool MMItemSharedLive(const void* srcv, uint8_t kind, uint16_t* out) {
    if (out == nullptr || !MMItemAligned(srcv)) {
        return false;
    }
    const SaveContext* src = static_cast<const SaveContext*>(srcv);
    if (!Combo_SaveIsLiveFile(GAME_MM, (int32_t)gSaveContext.gameMode)) {
        return false;
    }
    const auto ammoOf = [src](uint8_t item) -> uint16_t {
        const s8 held = AMMO(item);
        return held < 0 ? 0u : (uint16_t)held;
    };
    switch (kind) {
        case RSBS_SHARED_RES_RUPEES:
            // The HUD's count: a pending rupeeAccumulator (settled by the harvest) lags a few frames.
            *out = gSaveContext.save.saveInfo.playerData.rupees < 0
                       ? 0u
                       : (uint16_t)gSaveContext.save.saveInfo.playerData.rupees;
            return true;
        case RSBS_SHARED_RES_WALLET_TIER:
            *out = (uint16_t)CUR_UPG_VALUE(UPG_WALLET);
            return true;
        case RSBS_SHARED_RES_HEALTH_QUARTERS: {
            const uint16_t pieces =
                (uint16_t)((gSaveContext.save.saveInfo.inventory.questItems & kMMHeartPieceMask) >> kMMHeartPieceShift);
            const s16 rawCapacity = gSaveContext.save.saveInfo.playerData.healthCapacity;
            *out = Combo_MakeHealthQuarters(rawCapacity < 0 ? 0u : (uint16_t)rawCapacity, pieces);
            return true;
        }
        case RSBS_SHARED_RES_DOUBLE_DEFENSE:
            *out = gSaveContext.save.saveInfo.playerData.doubleDefense ? 1u : 0u;
            return true;
        case RSBS_SHARED_RES_MAGIC_LEVEL:
            *out = !gSaveContext.save.saveInfo.playerData.isMagicAcquired
                       ? 0u
                       : (gSaveContext.save.saveInfo.playerData.isDoubleMagicAcquired ? 2u : 1u);
            return true;
        case RSBS_SHARED_RES_ARROW_COUNT:
            *out = ammoOf(ITEM_BOW);
            return true;
        case RSBS_SHARED_RES_BOMB_COUNT:
            *out = ammoOf(ITEM_BOMB);
            return true;
        case RSBS_SHARED_RES_BOMBCHU_COUNT:
            *out = ammoOf(ITEM_BOMBCHU);
            return true;
        case RSBS_SHARED_RES_STICK_COUNT:
            *out = ammoOf(ITEM_DEKU_STICK);
            return true;
        case RSBS_SHARED_RES_NUT_COUNT:
            *out = ammoOf(ITEM_DEKU_NUT);
            return true;
        case RSBS_SHARED_RES_HOOKSHOT_TIER:
            *out = (INV_CONTENT(ITEM_HOOKSHOT) == ITEM_HOOKSHOT) ? 1u : 0u;
            return true;
        default:
            return false;
    }
}

#undef gSaveContext

} // namespace

extern "C" void MM_ItemAdapter_Register(void) {
    static const ComboItemOps kOps = {
        MMItemCount,  MMItemRowAt,      MMItemHasSave,  MMItemLiveSave,   MMItemPaused,
        &kMMItemGrid, MMItemSharedIcon, MMItemCountFor, MMItemSharedLive,
    };
    Combo_Item_RegisterOps((uint8_t)GAME_MM, &kOps);
}

// ============================================================================
// ROM-free lock support (redship --test combo-item-view)
// ============================================================================
//
// The lock lives in src/common/tests and must not see MM's layout, so the
// authoring happens here and only an opaque buffer crosses back.

/**
 * Write a started MM save into `buf` (zeroed first). Variant 0, the "shadow"
 * world: the Hero's Bow with a tier-2 quiver holding 25 arrows, 7 Magic Beans,
 * two bottles (one empty, one Red Potion), the Deku Mask and the Bunny Hood,
 * the Song of Healing, the Goron Lullaby intro only, Odolwa's Remains, the
 * Razor Sword, the Hero's Shield, magic, and an Adult's Wallet holding 150
 * rupees. Variant 1, the "live" world: the Hookshot, the Zora Mask, the Song
 * of Soaring and the Kokiri Sword only. Variant 2, the "capacity tier" world
 * (#967): the Bow (tier-1 quiver, 12 arrows), bombs (tier-2 bag, 7), Deku
 * Sticks (tier 1, 5), Deku Nuts (tier 3, 9) and the Kokiri Sword. Variant 3,
 * the bombchu world (#1143): variant 2 plus 13 Bombchus. Returns 1, or
 * 0 when `size` cannot hold a SaveContext or `buf` is misaligned.
 */
extern "C" int MM_ItemAdapter_TestAuthorSave(void* buf, size_t size, int variant) {
    if (buf == nullptr || size < sizeof(SaveContext) || !MMItemAligned(buf)) {
        return 0;
    }
    std::memset(buf, 0, size);
    SaveContext* s = static_cast<SaveContext*>(buf);
    static const char kNewf[6] = { 'Z', 'E', 'L', 'D', 'A', '3' };
    std::memcpy(s->save.saveInfo.playerData.newf, kNewf, sizeof(kNewf));
    std::memset(s->save.saveInfo.inventory.items, ITEM_NONE, sizeof(s->save.saveInfo.inventory.items));
    auto& inv = s->save.saveInfo.inventory;

    if (variant == 0) {
        inv.items[SLOT_BOW] = ITEM_BOW;
        inv.ammo[SLOT_BOW] = 25;
        inv.upgrades |= (2u << MM_gUpgradeShifts[UPG_QUIVER]);
        inv.items[SLOT_MAGIC_BEANS] = ITEM_MAGIC_BEANS;
        inv.ammo[SLOT_MAGIC_BEANS] = 7;
        inv.items[SLOT_BOTTLE_1] = ITEM_BOTTLE;
        inv.items[SLOT_BOTTLE_3] = ITEM_POTION_RED;
        inv.items[SLOT_MASK_DEKU] = ITEM_MASK_DEKU;
        inv.items[SLOT_MASK_BUNNY] = ITEM_MASK_BUNNY;
        inv.questItems |= (1u << QUEST_SONG_HEALING) | (1u << QUEST_SONG_LULLABY_INTRO) | (1u << QUEST_REMAINS_ODOLWA);
        s->save.saveInfo.equips.equipment |= (u16)(EQUIP_VALUE_SWORD_RAZOR << MM_gEquipShifts[EQUIP_TYPE_SWORD]);
        s->save.saveInfo.equips.equipment |= (u16)(EQUIP_VALUE_SHIELD_HERO << MM_gEquipShifts[EQUIP_TYPE_SHIELD]);
        s->save.saveInfo.playerData.isMagicAcquired = 1;
        inv.upgrades |= (1u << MM_gUpgradeShifts[UPG_WALLET]);
        s->save.saveInfo.playerData.rupees = 150;
    } else if (variant == 2 || variant == 3) {
        // The capacity-tier world (#967), OoT's twin: the Bow with a tier-1
        // quiver holding 12 arrows, bombs with a tier-2 bag holding 7, 5 Deku
        // Sticks (tier 1), 9 Deku Nuts (tier 3), and the Kokiri Sword.
        inv.items[SLOT_BOW] = ITEM_BOW;
        inv.ammo[SLOT_BOW] = 12;
        inv.upgrades |= (1u << MM_gUpgradeShifts[UPG_QUIVER]);
        inv.items[SLOT_BOMB] = ITEM_BOMB;
        inv.ammo[SLOT_BOMB] = 7;
        inv.upgrades |= (2u << MM_gUpgradeShifts[UPG_BOMB_BAG]);
        inv.items[SLOT_DEKU_STICK] = ITEM_DEKU_STICK;
        inv.ammo[SLOT_DEKU_STICK] = 5;
        inv.upgrades |= (1u << MM_gUpgradeShifts[UPG_DEKU_STICKS]);
        inv.items[SLOT_DEKU_NUT] = ITEM_DEKU_NUT;
        inv.ammo[SLOT_DEKU_NUT] = 9;
        inv.upgrades |= (3u << MM_gUpgradeShifts[UPG_DEKU_NUTS]);
        s->save.saveInfo.equips.equipment |= (u16)(EQUIP_VALUE_SWORD_KOKIRI << MM_gEquipShifts[EQUIP_TYPE_SWORD]);
        if (variant == 3) {
            inv.items[SLOT_BOMBCHU] = ITEM_BOMBCHU;
            inv.ammo[SLOT_BOMBCHU] = 13;
        }
    } else {
        inv.items[SLOT_HOOKSHOT] = ITEM_HOOKSHOT;
        inv.items[SLOT_MASK_ZORA] = ITEM_MASK_ZORA;
        inv.questItems |= (1u << QUEST_SONG_SOARING);
        s->save.saveInfo.equips.equipment |= (u16)(EQUIP_VALUE_SWORD_KOKIRI << MM_gEquipShifts[EQUIP_TYPE_SWORD]);
    }
    return 1;
}

/**
 * Author the ocarina-button world of #1072 into `buf`: the variant-0 save above
 * with the file's frozen shuffle option and the button bits set from `mode` and
 * `heldMask`. `mode` 0: the option off (a vanilla button world). 1: the option
 * on in a randomizer file. 2: the option on in a file that is not a randomizer
 * file (saveType vanilla), the gate IS_RANDO excludes. `heldMask` bit i is the
 * i-th button of the shuffle's order (A, C-Down, C-Right, C-Left, C-Up, the
 * RANDO_INF_OBTAINED_OCARINA_BUTTON_* order) held. Returns 1, or 0 as above.
 */
extern "C" int MM_ItemAdapter_TestAuthorOcarinaButtons(void* buf, size_t size, int mode, unsigned heldMask) {
    if (MM_ItemAdapter_TestAuthorSave(buf, size, 0) != 1) {
        return 0;
    }
    SaveContext* s = static_cast<SaveContext*>(buf);
    s->save.shipSaveInfo.saveType = (mode == 2) ? SAVETYPE_VANILLA : SAVETYPE_RANDO;
    s->save.shipSaveInfo.rando.randoSaveOptions[RO_SHUFFLE_OCARINA_BUTTONS] =
        (mode == 0) ? RO_GENERIC_OFF : RO_GENERIC_YES;
    for (int i = 0; i < 5; i++) {
        if ((heldMask & (1u << i)) != 0) {
            const int flag = RANDO_INF_OBTAINED_OCARINA_BUTTON_A + i;
            s->save.shipSaveInfo.rando.randoInf[flag >> 4] |= (u16)(1u << (flag & 0xF));
        }
    }
    return 1;
}

/**
 * Author the tracker-rows world of #1140 into `buf`: the variant-0 save above
 * plus Swamp 12 / Ocean 5 Skulltula tokens; stray fairies Woodfall 10,
 * Snowhead 3, Great Bay 0, Stone Tower 15; small keys held 0 / 1 / none / 2 and
 * found 1 / 3 / 0 / 2 (the found counts are authored in BOTH modes, so a
 * vanilla read that took them would show); the Woodfall boss key and map, the
 * Snowhead compass, all three Stone Tower items; three heart pieces; the souls
 * of Odolwa and Gyorg and RI_SOUL_ENEMY_WOLFOS given through
 * SOUL_RI_TO_RANDO_INF, plus the named RANDO_INF_OBTAINED_SOUL_OF_ENEMY_WOLFOS
 * flag no item sets; the Clock Town and Stone Tower owls; Day 1 and Night 2;
 * the ability to swim. Every flag is set whatever the options say.
 *
 * `rando` 1: a randomizer file (SAVETYPE_RANDO) whose frozen
 * RO_MINIMUM_SKULLTULA_TOKENS is 20 and RO_MINIMUM_STRAY_FAIRIES 10; 0: a
 * vanilla file carrying the same option bytes. `options` bit 0
 * RO_SHUFFLE_BOSS_SOULS, 1 RO_SHUFFLE_ENEMY_SOULS, 2 RO_SHUFFLE_OWL_STATUES,
 * 3 RO_CLOCK_SHUFFLE, 4 RO_SHUFFLE_SWIM, 5 RO_SHUFFLE_TRIFORCE_PIECES (each
 * RO_GENERIC_YES when set), bit 6 the Clock Town fairy's WEEKEVENTREG_08_80.
 * Returns 1, or 0 as above.
 */
extern "C" int MM_ItemAdapter_TestAuthorTrackerWorld(void* buf, size_t size, int rando, unsigned options) {
    if (MM_ItemAdapter_TestAuthorSave(buf, size, 0) != 1) {
        return 0;
    }
    SaveContext* s = static_cast<SaveContext*>(buf);
    auto& info = s->save.saveInfo;
    auto& inv = info.inventory;
    auto& r = s->save.shipSaveInfo.rando;
    s->save.shipSaveInfo.saveType = rando ? SAVETYPE_RANDO : SAVETYPE_VANILLA;
    r.randoSaveOptions[RO_MINIMUM_SKULLTULA_TOKENS] = 20;
    r.randoSaveOptions[RO_MINIMUM_STRAY_FAIRIES] = 10;
    static const RandoOptionId kOptions[6] = {
        RO_SHUFFLE_BOSS_SOULS, RO_SHUFFLE_ENEMY_SOULS, RO_SHUFFLE_OWL_STATUES,
        RO_CLOCK_SHUFFLE,      RO_SHUFFLE_SWIM,        RO_SHUFFLE_TRIFORCE_PIECES
    };
    for (int i = 0; i < 6; i++) {
        r.randoSaveOptions[kOptions[i]] = ((options & (1u << i)) != 0) ? RO_GENERIC_YES : RO_GENERIC_NO;
    }
    if ((options & 0x40u) != 0) {
        info.weekEventReg[WEEKEVENTREG_08_80 >> 8] |= (u8)(WEEKEVENTREG_08_80 & 0xFF);
    }
    info.skullTokenCount = (12u << 16) | 5u;
    inv.strayFairies[DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE] = 10;
    inv.strayFairies[DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE] = 3;
    inv.strayFairies[DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE] = 0;
    inv.strayFairies[DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE] = 15;
    std::memset(inv.dungeonKeys, -1, sizeof(inv.dungeonKeys)); // a temple never entered holds -1
    inv.dungeonKeys[DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE] = 0;
    inv.dungeonKeys[DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE] = 1;
    inv.dungeonKeys[DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE] = 2;
    r.foundDungeonKeys[DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE] = 1;
    r.foundDungeonKeys[DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE] = 3;
    r.foundDungeonKeys[DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE] = 0;
    r.foundDungeonKeys[DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE] = 2;
    inv.dungeonItems[DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE] = (u8)((1u << DUNGEON_BOSS_KEY) | (1u << DUNGEON_MAP));
    inv.dungeonItems[DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE] = (u8)(1u << DUNGEON_COMPASS);
    inv.dungeonItems[DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE] =
        (u8)((1u << DUNGEON_BOSS_KEY) | (1u << DUNGEON_COMPASS) | (1u << DUNGEON_MAP));
    inv.questItems |= (3u << 28); // three heart pieces: the top nibble
    const auto setInf = [&r](int flag) { r.randoInf[flag >> 4] |= (u16)(1u << (flag & 0xF)); };
    setInf(SOUL_RI_TO_RANDO_INF(RI_SOUL_BOSS_ODOLWA));
    setInf(SOUL_RI_TO_RANDO_INF(RI_SOUL_BOSS_GYORG));
    setInf(SOUL_RI_TO_RANDO_INF(RI_SOUL_ENEMY_WOLFOS));
    setInf(RANDO_INF_OBTAINED_SOUL_OF_ENEMY_WOLFOS); // the named flag the macro never reaches
    info.playerData.owlActivationFlags |= (u16)((1u << OWL_WARP_CLOCK_TOWN) | (1u << OWL_WARP_STONE_TOWER));
    setInf(RANDO_INF_OBTAINED_CLOCK_DAY_1);
    setInf(RANDO_INF_OBTAINED_CLOCK_NIGHT_2);
    setInf(RANDO_INF_OBTAINED_SWIM);
    return 1;
}

#endif // RSBS_SINGLE_EXECUTABLE
