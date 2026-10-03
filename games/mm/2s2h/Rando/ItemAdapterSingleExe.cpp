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
 * The curated v1 list here is 60 rows, in the native order: the 15 item slots
 * of Inventory (the three trade slots and the six bottle slots are left out;
 * one Bottles row counts the bottles held instead), all 24 Masks, the 10 Songs
 * and the 10 Quest rows. Left out: the trade items (transient quest objects),
 * the per-bottle contents (consumables), the Skulltula tokens and stray fairies
 * (count goals whose ceilings are seed options), and the Dungeon keys
 * (setting-dependent, as OoT's adapter leaves out its dungeon items).
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

#include "Rando/Rando.h" // SaveContext (via variables.h), StaticData::Items, GetIconTexturePath

extern "C" {
#include <macros.h> // CUR_CAPACITY
}

// src/common. Included outside any extern "C" block: the headers manage their
// own linkage (matching TrackerAdapterSingleExe.cpp).
#include "combo_item_view.h"
#include "game.h" // MM_SAVE_CONTEXT_SIZE

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

void MMItemFillVanilla(ComboItemRow* out, uint32_t itemId) {
    out->name = MMItemVanillaName(itemId);
    out->iconKey = MMItemIcon(itemId);
    out->iconKeyFaded = out->iconKey; // MM fades by alpha, not by texture
}

void MMItemFillRando(ComboItemRow* out, RandoItemId randoItemId) {
    out->name = MMItemRandoName(randoItemId);
    out->iconKey = MMItemRandoIcon(randoItemId);
    out->iconKeyFaded = out->iconKey;
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
};

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
};

#undef MMI_SLOT
#undef MMI_AMMO
#undef MMI_FIXED
#undef MMI_QUEST
#undef MMI_KIND

constexpr int kMMItemRowCount = (int)(sizeof(kMMItemRows) / sizeof(kMMItemRows[0]));

/** An inventory slot: the held item names the row, the empty slot falls back to
 *  the base item (MM's tracker: safeItemsForInventorySlot[slot][0]). */
void MMItemFillSlot(const SaveContext* src, const MMItemRowDef& def, ComboItemRow* out) {
    const uint8_t content = INV_CONTENT(def.item);
    out->have = content != ITEM_NONE;
    MMItemFillVanilla(out, out->have ? content : def.item);
}

bool MMItemDerive(const SaveContext* src, const MMItemRowDef& def, ComboItemRow* out) {
    out->group = def.group;
    switch (def.kind) {
        case MMRK_SLOT:
            MMItemFillSlot(src, def, out);
            return true;
        case MMRK_SLOT_AMMO:
        case MMRK_SLOT_BOMBCHU:
        case MMRK_SLOT_FIXED:
            MMItemFillSlot(src, def, out);
            if (out->have) {
                out->count = AMMO(def.item);
                out->max = (def.kind == MMRK_SLOT_FIXED)     ? (int)def.data
                           : (def.kind == MMRK_SLOT_BOMBCHU) ? (int)CUR_CAPACITY(UPG_BOMB_BAG)
                                                             : (int)CUR_CAPACITY(def.data);
            }
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
            return true;
        }
    }
    return false;
}

// ---- the vtable ----------------------------------------------------------------

int MMItemCount(void) {
    return kMMItemRowCount;
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

bool MMItemRowAt(const void* buf, int index, ComboItemRow* out) {
    if (!MMItemAligned(buf) || out == nullptr || index < 0 || index >= kMMItemRowCount) {
        return false;
    }
    const SaveContext* src = static_cast<const SaveContext*>(buf);
    const uint8_t freshness = out->freshness; // the view's field; left alone
    std::memset(out, 0, sizeof(*out));
    out->freshness = freshness;
    return MMItemDerive(src, kMMItemRows[index], out);
}

} // namespace

#undef gSaveContext
// ============================================================================
// Below: the real global again (nothing here reads a save).
// ============================================================================

extern "C" void MM_ItemAdapter_Register(void) {
    static const ComboItemOps kOps = {
        MMItemCount, MMItemRowAt, MMItemHasSave, MMItemLiveSave, MMItemPaused,
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
 * of Soaring and the Kokiri Sword only. Returns 1, or 0 when `size` cannot hold
 * a SaveContext or `buf` is misaligned.
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
    } else {
        inv.items[SLOT_HOOKSHOT] = ITEM_HOOKSHOT;
        inv.items[SLOT_MASK_ZORA] = ITEM_MASK_ZORA;
        inv.questItems |= (1u << QUEST_SONG_SOARING);
        s->save.saveInfo.equips.equipment |= (u16)(EQUIP_VALUE_SWORD_KOKIRI << MM_gEquipShifts[EQUIP_TYPE_SWORD]);
    }
    return 1;
}

#endif // RSBS_SINGLE_EXECUTABLE
