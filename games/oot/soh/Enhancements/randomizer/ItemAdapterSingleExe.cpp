/**
 * ItemAdapterSingleExe.cpp — OoT's item adapter for the unified item tracker
 * (#458 U1; ADR 0002 and ADR 0008 rule 5 as amended 2026-09-30).
 *
 * The view (src/common/combo_item_view.c) picks a buffer — OoT's live save
 * while OoT is played, otherwise OoT's frozen shadow — and hands it here. This
 * TU reads that buffer through OoT's own layout and returns display rows; no
 * ITEM_*, QUEST_* or RG_* value leaves it, only strings and counts.
 *
 * THE ACCESSOR MACROS OVER ANY BUFFER. OoT's accessor macros (INV_CONTENT,
 * AMMO, CUR_UPG_VALUE, CUR_CAPACITY, CHECK_QUEST_ITEM, IS_RANDO; macros.h and
 * z64save.h) name the global `gSaveContext`. After the last #include below,
 * `gSaveContext` is redefined to `(*src)`, so every macro expanded in a row
 * derivation reads the parameter `src` — the buffer the view handed over —
 * and never the global. The redefinition follows every include, so no header
 * ever sees it (a header declaring `extern SaveContext gSaveContext;` after it
 * would declare a pointer named `src` instead). Functions are NOT macros:
 * Flags_GetRandomizerInf reads the global in its own TU, so the one
 * randomizerInf read here is re-derived over `src` (RandInfOf below).
 *
 * The live source is defined BEFORE the redefinition, since it is the one
 * place that must name the real global.
 *
 * Row definitions are copied from the native Item Tracker's default main-window
 * sections (randomizer_item_tracker.cpp: inventoryItems, equipmentItems,
 * miscItems, dungeonRewardStones/Medallions, songItems), 66 rows. The icon keys
 * are the native tracker's texture names (ImGuiUtils.cpp itemMapping /
 * questMapping / songMapping: "ITEM_X" and "ITEM_X_Faded"), so a drawer can
 * hand them to the Gui's texture map as-is once OoT has booted.
 *
 * Lives in soh/Enhancements/randomizer/ (soh_rando, WHOLE_ARCHIVE) and is
 * referenced by name from Combo_TrackerWindow_Init, so no elision mode leaves
 * the view without an OoT adapter.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>

#include <libultraship/libultraship.h>

extern "C" {
#include <z64.h>
#include "variables.h"
#include "macros.h"
extern PlayState* OoT_gPlayState;
}

#include "soh/GameVersions.h"
#include "soh/SaveManager.h" // SaveManager::Instance, for the new-file lock's seam (#873)
// Declared only in SaveManager.h's C branch (#ifndef __cplusplus).
extern "C" void Save_InitFile(int isDebug);

// src/common. Included outside any extern "C" block: the headers manage their
// own linkage (matching TrackerAdapterSingleExe.cpp).
#include "combo_item_view.h"
#include "game.h" // OOT_SAVE_CONTEXT_SIZE

// The view hands this TU either the unified live storage (unified_save.c,
// alignas(16)) or OoT's shadow, a std::vector<uint8_t> buffer (context.cpp),
// which operator new aligns to __STDCPP_DEFAULT_NEW_ALIGNMENT__. Reading either
// as a SaveContext is sound only while the struct asks for no more than that.
static_assert(alignof(SaveContext) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__,
              "OoT's shadow is a std::vector<uint8_t> buffer; SaveContext must not need stricter alignment");
static_assert(alignof(SaveContext) <= 16, "the unified gSaveContext storage is alignas(16)");
static_assert(sizeof(SaveContext) <= (size_t)OOT_SAVE_CONTEXT_SIZE,
              "OoT's SaveContext no longer fits the shadow blob the item adapter reads");

namespace {

/**
 * OoT's live save for the view's LIVE read, or NULL: only while a play state is
 * loaded and the save is a played file (GAMEMODE_NORMAL). That excludes the
 * title screen, whose attract demo runs a play state over the debug save under
 * GAMEMODE_TITLE_SCREEN, and file select, which has no play state — the same
 * test GameInteractor::IsSaveLoaded makes before the native tracker reads.
 */
const void* OoTItemLiveSave(void) {
    if (OoT_gPlayState == nullptr || gSaveContext.gameMode != GAMEMODE_NORMAL) {
        return nullptr;
    }
    return static_cast<const void*>(&gSaveContext);
}

/** OoT's pause menu is open: the native Item Tracker's ShowOnlyPaused test
 *  (randomizer_item_tracker.cpp, ItemTrackerWindow::DrawElement), #458 U2. */
bool OoTItemPaused(void) {
    return OoT_gPlayState != nullptr && OoT_gPlayState->pauseCtx.state > 0;
}

} // namespace

// ============================================================================
// From here on the accessor macros read the handed buffer, never the global.
// ============================================================================
#define gSaveContext (*src)

namespace {

/** Flags_GetRandomizerInf, re-derived over `src` (it is a function in
 *  z_actor.c and reads the global). */
bool RandInfOf(const SaveContext* src, RandomizerInf flag) {
    return (src->ship.randomizerInf[flag >> 4] & (1 << (flag & 0xF))) != 0;
}

bool Aligned(const void* src) {
    return src != nullptr && (reinterpret_cast<uintptr_t>(src) % alignof(SaveContext)) == 0;
}

// ---- icon keys and names for inventory contents ----------------------------

struct ItemKey {
    uint8_t id;
    const char* key;
    const char* faded;
    const char* name;
};

#define IK(id, name) \
    { (uint8_t)(id), #id, #id "_Faded", name }

const ItemKey kItemKeys[] = {
    IK(ITEM_STICK, "Deku Sticks"),
    IK(ITEM_NUT, "Deku Nuts"),
    IK(ITEM_BOMB, "Bombs"),
    IK(ITEM_BOW, "Fairy Bow"),
    IK(ITEM_ARROW_FIRE, "Fire Arrows"),
    IK(ITEM_DINS_FIRE, "Din's Fire"),
    IK(ITEM_SLINGSHOT, "Fairy Slingshot"),
    IK(ITEM_OCARINA_FAIRY, "Fairy Ocarina"),
    IK(ITEM_OCARINA_TIME, "Ocarina of Time"),
    IK(ITEM_BOMBCHU, "Bombchus"),
    IK(ITEM_HOOKSHOT, "Hookshot"),
    IK(ITEM_LONGSHOT, "Longshot"),
    IK(ITEM_ARROW_ICE, "Ice Arrows"),
    IK(ITEM_FARORES_WIND, "Farore's Wind"),
    IK(ITEM_BOOMERANG, "Boomerang"),
    IK(ITEM_LENS, "Lens of Truth"),
    IK(ITEM_BEAN, "Magic Beans"),
    IK(ITEM_HAMMER, "Megaton Hammer"),
    IK(ITEM_ARROW_LIGHT, "Light Arrows"),
    IK(ITEM_NAYRUS_LOVE, "Nayru's Love"),
    IK(ITEM_BOTTLE, "Empty Bottle"),
    IK(ITEM_POTION_RED, "Red Potion"),
    IK(ITEM_POTION_GREEN, "Green Potion"),
    IK(ITEM_POTION_BLUE, "Blue Potion"),
    IK(ITEM_FAIRY, "Bottled Fairy"),
    IK(ITEM_FISH, "Fish"),
    IK(ITEM_MILK_BOTTLE, "Lon Lon Milk"),
    IK(ITEM_LETTER_RUTO, "Ruto's Letter"),
    IK(ITEM_BLUE_FIRE, "Blue Fire"),
    IK(ITEM_BUG, "Bugs"),
    IK(ITEM_BIG_POE, "Big Poe"),
    IK(ITEM_MILK_HALF, "Lon Lon Milk (Half)"),
    IK(ITEM_POE, "Poe"),
    IK(ITEM_WEIRD_EGG, "Weird Egg"),
    IK(ITEM_CHICKEN, "Chicken"),
    IK(ITEM_LETTER_ZELDA, "Zelda's Letter"),
    IK(ITEM_MASK_KEATON, "Keaton Mask"),
    IK(ITEM_MASK_SKULL, "Skull Mask"),
    IK(ITEM_MASK_SPOOKY, "Spooky Mask"),
    IK(ITEM_MASK_BUNNY, "Bunny Hood"),
    IK(ITEM_MASK_GORON, "Goron Mask"),
    IK(ITEM_MASK_ZORA, "Zora Mask"),
    IK(ITEM_MASK_GERUDO, "Gerudo Mask"),
    IK(ITEM_MASK_TRUTH, "Mask of Truth"),
    IK(ITEM_SOLD_OUT, "Sold Out"),
    IK(ITEM_POCKET_EGG, "Pocket Egg"),
    IK(ITEM_POCKET_CUCCO, "Pocket Cucco"),
    IK(ITEM_COJIRO, "Cojiro"),
    IK(ITEM_ODD_MUSHROOM, "Odd Mushroom"),
    IK(ITEM_ODD_POTION, "Odd Potion"),
    IK(ITEM_SAW, "Poacher's Saw"),
    IK(ITEM_SWORD_BROKEN, "Broken Goron's Sword"),
    IK(ITEM_PRESCRIPTION, "Prescription"),
    IK(ITEM_FROG, "Eyeball Frog"),
    IK(ITEM_EYEDROPS, "World's Finest Eyedrops"),
    IK(ITEM_CLAIM_CHECK, "Claim Check"),
    IK(ITEM_BRACELET, "Goron's Bracelet"),
    IK(ITEM_GAUNTLETS_SILVER, "Silver Gauntlets"),
    IK(ITEM_GAUNTLETS_GOLD, "Golden Gauntlets"),
    IK(ITEM_SCALE_SILVER, "Silver Scale"),
    IK(ITEM_SCALE_GOLDEN, "Golden Scale"),
    IK(ITEM_WALLET_ADULT, "Adult's Wallet"),
    IK(ITEM_WALLET_GIANT, "Giant's Wallet"),
    IK(ITEM_HEART_CONTAINER, "Heart Containers"),
    IK(ITEM_HEART_PIECE, "Pieces of Heart"),
    IK(ITEM_MAGIC_SMALL, "Magic Meter"),
    IK(ITEM_MAGIC_LARGE, "Double Magic"),
};

#undef IK

const ItemKey* ItemKeyFor(uint32_t id) {
    for (const ItemKey& k : kItemKeys) {
        if (k.id == id) {
            return &k;
        }
    }
    return nullptr;
}

// ---- the curated rows --------------------------------------------------------

enum RowKind : uint8_t {
    RK_INV,        // an inventory slot; `item` names the slot and the empty row
    RK_INV_AMMO,   // ...plus its ammo, capped by upgrade `data`
    RK_INV_FIXED,  // ...plus its ammo, capped at `data` once held
    RK_BOTTLE,     // bottle slot `data` (0..3)
    RK_EQUIP,      // owned-equipment bit mask `data`
    RK_QUEST,      // quest bit `data`
    RK_STRENGTH,   // UPG_STRENGTH tier
    RK_SCALE,      // UPG_SCALE tier
    RK_WALLET,     // UPG_WALLET tier, rupees
    RK_HEART_CONT, // ship.stats.heartContainers of 8
    RK_HEART_PIECE,
    RK_MAGIC,
    RK_TOKENS,
};

struct RowDef {
    const char* group;
    RowKind kind;
    uint8_t item;     // ITEM_* for inventory rows and icon lookup
    uint32_t data;    // upgrade type, bit mask, quest bit, bottle index or fixed cap
    const char* name; // fixed name (equipment/quest rows), or the empty-slot name when non-NULL
    const char* key;  // fixed icon key (equipment/quest rows)
    const char* keyFaded;
};

#define INV(item) \
    { "Inventory", RK_INV, (uint8_t)(item), 0, nullptr, nullptr, nullptr }
#define INV_AMMO(item, upg) \
    { "Inventory", RK_INV_AMMO, (uint8_t)(item), (uint32_t)(upg), nullptr, nullptr, nullptr }
#define INV_FIXED(item, cap) \
    { "Inventory", RK_INV_FIXED, (uint8_t)(item), (uint32_t)(cap), nullptr, nullptr, nullptr }
#define INV_NAMED(item, emptyName) \
    { "Inventory", RK_INV, (uint8_t)(item), 0, emptyName, nullptr, nullptr }
#define BOTTLE(n) \
    { "Inventory", RK_BOTTLE, (uint8_t)ITEM_BOTTLE, (uint32_t)(n), "Bottle", nullptr, nullptr }
#define EQUIP(id, mask, name) \
    { "Equipment", RK_EQUIP, (uint8_t)(id), (uint32_t)(mask), name, #id, #id "_Faded" }
#define QUEST(group, id, name) \
    { group, RK_QUEST, 0, (uint32_t)(id), name, #id, #id "_Faded" }
#define MISC(kind, item) \
    { "Misc", kind, (uint8_t)(item), 0, nullptr, nullptr, nullptr }

// Order and membership follow the native tracker's default sections; the
// setting-dependent sections (dungeon items, Greg, triforce pieces, boss and
// bean souls, ocarina buttons, overworld keys, the fishing pole, jabber nuts,
// action shuffle) are left out of v1.
const RowDef kRows[] = {
    // Inventory (inventoryItems)
    INV_AMMO(ITEM_STICK, UPG_STICKS),
    INV_AMMO(ITEM_NUT, UPG_NUTS),
    INV_AMMO(ITEM_BOMB, UPG_BOMB_BAG),
    INV_AMMO(ITEM_BOW, UPG_QUIVER),
    INV(ITEM_ARROW_FIRE),
    INV(ITEM_DINS_FIRE),
    INV_AMMO(ITEM_SLINGSHOT, UPG_BULLET_BAG),
    INV(ITEM_OCARINA_FAIRY),
    INV_FIXED(ITEM_BOMBCHU, 50),
    INV(ITEM_HOOKSHOT),
    INV(ITEM_ARROW_ICE),
    INV(ITEM_FARORES_WIND),
    INV(ITEM_BOOMERANG),
    INV(ITEM_LENS),
    INV_FIXED(ITEM_BEAN, 10),
    INV(ITEM_HAMMER),
    INV(ITEM_ARROW_LIGHT),
    INV(ITEM_NAYRUS_LOVE),
    BOTTLE(0),
    BOTTLE(1),
    BOTTLE(2),
    BOTTLE(3),
    INV_NAMED(ITEM_POCKET_EGG, "Adult Trade Item"),
    INV_NAMED(ITEM_MASK_KEATON, "Child Trade Item"),
    // Equipment (equipmentItems): the owned-equipment bits the native rows test
    EQUIP(ITEM_SWORD_KOKIRI, 1 << 0, "Kokiri Sword"),
    EQUIP(ITEM_SWORD_MASTER, 1 << 1, "Master Sword"),
    EQUIP(ITEM_SWORD_BGS, 1 << 2, "Biggoron's Sword"),
    EQUIP(ITEM_TUNIC_KOKIRI, 1 << 8, "Kokiri Tunic"),
    EQUIP(ITEM_TUNIC_GORON, 1 << 9, "Goron Tunic"),
    EQUIP(ITEM_TUNIC_ZORA, 1 << 10, "Zora Tunic"),
    EQUIP(ITEM_SHIELD_DEKU, 1 << 4, "Deku Shield"),
    EQUIP(ITEM_SHIELD_HYLIAN, 1 << 5, "Hylian Shield"),
    EQUIP(ITEM_SHIELD_MIRROR, 1 << 6, "Mirror Shield"),
    EQUIP(ITEM_BOOTS_KOKIRI, 1 << 12, "Kokiri Boots"),
    EQUIP(ITEM_BOOTS_IRON, 1 << 13, "Iron Boots"),
    EQUIP(ITEM_BOOTS_HOVER, 1 << 14, "Hover Boots"),
    // Misc (miscItems)
    MISC(RK_STRENGTH, ITEM_BRACELET),
    MISC(RK_SCALE, ITEM_SCALE_SILVER),
    MISC(RK_WALLET, ITEM_WALLET_ADULT),
    MISC(RK_HEART_CONT, ITEM_HEART_CONTAINER),
    MISC(RK_HEART_PIECE, ITEM_HEART_PIECE),
    MISC(RK_MAGIC, ITEM_MAGIC_SMALL),
    QUEST("Misc", QUEST_GERUDO_CARD, "Gerudo's Card"),
    { "Misc", RK_TOKENS, 0, (uint32_t)QUEST_SKULL_TOKEN, "Gold Skulltula Tokens", "QUEST_SKULL_TOKEN",
      "QUEST_SKULL_TOKEN_Faded" },
    QUEST("Misc", QUEST_STONE_OF_AGONY, "Stone of Agony"),
    // Dungeon rewards (dungeonRewardStones, dungeonRewardMedallions)
    QUEST("Dungeon Rewards", QUEST_KOKIRI_EMERALD, "Kokiri's Emerald"),
    QUEST("Dungeon Rewards", QUEST_GORON_RUBY, "Goron's Ruby"),
    QUEST("Dungeon Rewards", QUEST_ZORA_SAPPHIRE, "Zora's Sapphire"),
    QUEST("Dungeon Rewards", QUEST_MEDALLION_FOREST, "Forest Medallion"),
    QUEST("Dungeon Rewards", QUEST_MEDALLION_FIRE, "Fire Medallion"),
    QUEST("Dungeon Rewards", QUEST_MEDALLION_WATER, "Water Medallion"),
    QUEST("Dungeon Rewards", QUEST_MEDALLION_SPIRIT, "Spirit Medallion"),
    QUEST("Dungeon Rewards", QUEST_MEDALLION_SHADOW, "Shadow Medallion"),
    QUEST("Dungeon Rewards", QUEST_MEDALLION_LIGHT, "Light Medallion"),
    // Songs (songItems)
    QUEST("Songs", QUEST_SONG_LULLABY, "Zelda's Lullaby"),
    QUEST("Songs", QUEST_SONG_EPONA, "Epona's Song"),
    QUEST("Songs", QUEST_SONG_SARIA, "Saria's Song"),
    QUEST("Songs", QUEST_SONG_SUN, "Sun's Song"),
    QUEST("Songs", QUEST_SONG_TIME, "Song of Time"),
    QUEST("Songs", QUEST_SONG_STORMS, "Song of Storms"),
    QUEST("Songs", QUEST_SONG_MINUET, "Minuet of Forest"),
    QUEST("Songs", QUEST_SONG_BOLERO, "Bolero of Fire"),
    QUEST("Songs", QUEST_SONG_SERENADE, "Serenade of Water"),
    QUEST("Songs", QUEST_SONG_REQUIEM, "Requiem of Spirit"),
    QUEST("Songs", QUEST_SONG_NOCTURNE, "Nocturne of Shadow"),
    QUEST("Songs", QUEST_SONG_PRELUDE, "Prelude of Light"),
};

#undef INV
#undef INV_AMMO
#undef INV_FIXED
#undef INV_NAMED
#undef BOTTLE
#undef EQUIP
#undef QUEST
#undef MISC

constexpr int kRowCount = (int)(sizeof(kRows) / sizeof(kRows[0]));

/** Name and icon keys from item `id`'s entry; the empty-slot name overrides. */
void FillFromItem(ComboItemRow* out, uint32_t id, const char* nameOverride) {
    const ItemKey* k = ItemKeyFor(id);
    out->name = (nameOverride != nullptr) ? nameOverride : (k != nullptr ? k->name : "Unknown Item");
    out->iconKey = (k != nullptr) ? k->key : nullptr;
    out->iconKeyFaded = (k != nullptr) ? k->faded : nullptr;
}

/** An inventory slot's content: the held item names the row, the empty slot
 *  falls back to the base item. */
void FillSlot(ComboItemRow* out, uint32_t content, const RowDef& def) {
    out->have = content != ITEM_NONE;
    if (out->have && ItemKeyFor(content) != nullptr) {
        FillFromItem(out, content, nullptr);
    } else {
        FillFromItem(out, def.item, def.name);
    }
}

bool Derive(const SaveContext* src, const RowDef& def, ComboItemRow* out) {
    out->group = def.group;
    switch (def.kind) {
        case RK_INV:
            FillSlot(out, INV_CONTENT(def.item), def);
            return true;
        case RK_INV_AMMO:
            FillSlot(out, INV_CONTENT(def.item), def);
            if (out->have) {
                out->count = AMMO(def.item);
                out->max = CUR_CAPACITY(def.data);
            }
            return true;
        case RK_INV_FIXED:
            FillSlot(out, INV_CONTENT(def.item), def);
            if (out->have) {
                out->count = AMMO(def.item);
                out->max = (int)def.data;
            }
            return true;
        case RK_BOTTLE:
            FillSlot(out, gSaveContext.inventory.items[SLOT(ITEM_BOTTLE) + def.data], def);
            return true;
        case RK_EQUIP:
            out->name = def.name;
            out->iconKey = def.key;
            out->iconKeyFaded = def.keyFaded;
            out->have = (gSaveContext.inventory.equipment & def.data) != 0;
            return true;
        case RK_QUEST:
        case RK_TOKENS:
            out->name = def.name;
            out->iconKey = def.key;
            out->iconKeyFaded = def.keyFaded;
            out->have = CHECK_QUEST_ITEM(def.data) != 0;
            if (def.kind == RK_TOKENS) {
                out->count = gSaveContext.inventory.gsTokens;
                out->max = 100;
            }
            return true;
        case RK_STRENGTH: {
            const s32 tier = CUR_UPG_VALUE(UPG_STRENGTH);
            FillFromItem(out,
                         tier >= 3   ? ITEM_GAUNTLETS_GOLD
                         : tier == 2 ? ITEM_GAUNTLETS_SILVER
                                     : ITEM_BRACELET,
                         nullptr);
            out->have = tier > 0;
            return true;
        }
        case RK_SCALE: {
            const s32 tier = CUR_UPG_VALUE(UPG_SCALE);
            FillFromItem(out, tier >= 2 ? ITEM_SCALE_GOLDEN : ITEM_SCALE_SILVER, nullptr);
            out->have = tier > 0;
            return true;
        }
        case RK_WALLET: {
            // The native row: a randomized file without the wallet shuffle's
            // find holds no wallet at all.
            const s32 tier = CUR_UPG_VALUE(UPG_WALLET);
            static const char* const kWalletNames[4] = { "Child's Wallet", "Adult's Wallet", "Giant's Wallet",
                                                         "Tycoon's Wallet" };
            FillFromItem(out, tier >= 2 ? ITEM_WALLET_GIANT : ITEM_WALLET_ADULT, kWalletNames[tier & 3]);
            out->have = !IS_RANDO || RandInfOf(src, RAND_INF_HAS_WALLET);
            out->count = gSaveContext.rupees;
            out->max = out->have ? CUR_CAPACITY(UPG_WALLET) : 0;
            return true;
        }
        case RK_HEART_CONT:
            FillFromItem(out, ITEM_HEART_CONTAINER, nullptr);
            out->count = gSaveContext.ship.stats.heartContainers;
            out->max = 8;
            out->have = out->count > 0;
            return true;
        case RK_HEART_PIECE:
            FillFromItem(out, ITEM_HEART_PIECE, nullptr);
            out->count = gSaveContext.ship.stats.heartPieces;
            out->max = 36;
            out->have = out->count > 0;
            return true;
        case RK_MAGIC:
            FillFromItem(out, gSaveContext.magicLevel >= 2 ? ITEM_MAGIC_LARGE : ITEM_MAGIC_SMALL, nullptr);
            out->have = gSaveContext.magicLevel > 0;
            return true;
    }
    return false;
}

// ---- the vtable ----------------------------------------------------------------

int OoTItemCount(void) {
    return kRowCount;
}

bool OoTItemHasSave(const void* buf) {
    if (!Aligned(buf)) {
        return false;
    }
    const SaveContext* src = static_cast<const SaveContext*>(buf);
    static const char kNewf[6] = { 'Z', 'E', 'L', 'D', 'A', 'Z' };
    return std::memcmp(gSaveContext.newf, kNewf, sizeof(kNewf)) == 0;
}

bool OoTItemRowAt(const void* buf, int index, ComboItemRow* out) {
    if (!Aligned(buf) || out == nullptr || index < 0 || index >= kRowCount) {
        return false;
    }
    const SaveContext* src = static_cast<const SaveContext*>(buf);
    const uint8_t freshness = out->freshness; // the view's field; left alone
    std::memset(out, 0, sizeof(*out));
    out->freshness = freshness;
    return Derive(src, kRows[index], out);
}

} // namespace

#undef gSaveContext
// ============================================================================
// Below: the real global again (nothing here reads a save).
// ============================================================================

extern "C" void OoT_ItemAdapter_Register(void) {
    static const ComboItemOps kOps = {
        OoTItemCount, OoTItemRowAt, OoTItemHasSave, OoTItemLiveSave, OoTItemPaused,
    };
    Combo_Item_RegisterOps((uint8_t)GAME_OOT, &kOps);
}

// ============================================================================
// ROM-free lock support (redship --test combo-item-view)
// ============================================================================
//
// The lock lives in src/common/tests and must not see OoT's layout, so the
// authoring happens here and only an opaque buffer crosses back.

/**
 * Write a started OoT save into `buf` (zeroed first). Variant 0, the "shadow"
 * world: the Longshot, the Fairy Bow with a tier-2 quiver holding 35 arrows,
 * a Red Potion in bottle 1, the Kokiri Sword, Zelda's Lullaby, the Forest
 * Medallion, 17 Gold Skulltula Tokens, 5 pieces of heart and an Adult's Wallet
 * holding 123 rupees, on a vanilla (not randomized) file. Variant 1, the "live"
 * world: the Hookshot and the Song of Time only. Returns 1, or 0 when `size`
 * cannot hold a SaveContext or `buf` is misaligned.
 */
extern "C" int OoT_ItemAdapter_TestAuthorSave(void* buf, size_t size, int variant) {
    if (buf == nullptr || size < sizeof(SaveContext) || !Aligned(buf)) {
        return 0;
    }
    std::memset(buf, 0, size);
    SaveContext* s = static_cast<SaveContext*>(buf);
    static const char kNewf[6] = { 'Z', 'E', 'L', 'D', 'A', 'Z' };
    std::memcpy(s->newf, kNewf, sizeof(kNewf));
    std::memset(s->inventory.items, ITEM_NONE, sizeof(s->inventory.items));
    s->ship.quest.id = QUEST_NORMAL;

    if (variant == 0) {
        s->inventory.items[SLOT_HOOKSHOT] = ITEM_LONGSHOT;
        s->inventory.items[SLOT_BOW] = ITEM_BOW;
        s->inventory.ammo[SLOT_BOW] = 35;
        s->inventory.items[SLOT_BOTTLE_1] = ITEM_POTION_RED;
        s->inventory.upgrades |= (2u << OoT_gUpgradeShifts[UPG_QUIVER]);
        s->inventory.upgrades |= (1u << OoT_gUpgradeShifts[UPG_WALLET]);
        s->rupees = 123;
        s->inventory.equipment |= (1 << 0); // Kokiri Sword, the native row's bit
        s->inventory.questItems |=
            (1u << QUEST_SONG_LULLABY) | (1u << QUEST_MEDALLION_FOREST) | (1u << QUEST_SKULL_TOKEN);
        s->inventory.gsTokens = 17;
        s->ship.stats.heartPieces = 5;
    } else {
        s->inventory.items[SLOT_HOOKSHOT] = ITEM_HOOKSHOT;
        s->inventory.questItems |= (1u << QUEST_SONG_TIME);
    }
    return 1;
}

/**
 * Write into `buf` (zeroed first) the save Ship of Harkinian's own new-file
 * path authors: Save_InitFile(false), which OoT_Sram_InitNewSave runs when the
 * player creates a file, over a zeroed gSaveContext so nothing but that path's
 * writes is in it (#873). The live gSaveContext is put back afterwards.
 *
 * Needs the full OoT bring-up (the init reads OTRGlobals and dispatches
 * SaveManager::Instance's init functions), so the rando-tier row
 * combo-item-view-new-file calls it. InitFileNormal picks the name fill by the
 * first mounted game version; a ROM-free tier mounts none, so an NTSC version
 * is added for the call and the archive manager is rebuilt from its archives
 * afterwards, which drops the added version again. Returns 1, or 0 when the
 * bring-up is missing, `size` cannot hold a SaveContext or `buf` is misaligned.
 * `outNewfMarked`, when non-NULL, receives whether the authored save carries
 * the "ZELDAZ" bytes in `newf`.
 */
namespace {
// AddGameVersion and ResetVirtualFileSystem are protected. A using-declaration
// in a derived class makes them nameable, and the pointer-to-member formed
// through it has the base's type, so it is called on the real ArchiveManager
// without a cast to a type the object is not.
struct ArchiveVersionAccess : Ship::ArchiveManager {
    using Ship::ArchiveManager::AddGameVersion;
    using Ship::ArchiveManager::ResetVirtualFileSystem;
};
} // namespace

extern "C" int OoT_ItemAdapter_TestAuthorNewFile(void* buf, size_t size, int* outNewfMarked) {
    if (buf == nullptr || size < sizeof(SaveContext) || !Aligned(buf) || SaveManager::Instance == nullptr ||
        Ship::Context::GetInstance() == nullptr || Ship::Context::GetInstance()->GetResourceManager() == nullptr) {
        return 0;
    }
    auto archives = Ship::Context::GetInstance()->GetResourceManager()->GetArchiveManager();
    const bool addedVersion = archives->GetGameVersions().empty();
    if (addedVersion) {
        void (Ship::ArchiveManager::*add)(uint32_t) = &ArchiveVersionAccess::AddGameVersion;
        ((*archives).*add)(OOT_NTSC_US_10);
    }

    const auto saved = std::make_unique<SaveContext>(gSaveContext);
    std::memset(&gSaveContext, 0, sizeof(SaveContext));
    Save_InitFile(0);
    std::memset(buf, 0, size);
    std::memcpy(buf, &gSaveContext, sizeof(SaveContext));
    gSaveContext = *saved;

    if (addedVersion) {
        void (Ship::ArchiveManager::*reset)() = &ArchiveVersionAccess::ResetVirtualFileSystem;
        ((*archives).*reset)();
    }
    if (outNewfMarked != nullptr) {
        static const char kNewf[6] = { 'Z', 'E', 'L', 'D', 'A', 'Z' };
        *outNewfMarked = std::memcmp(static_cast<const SaveContext*>(buf)->newf, kNewf, sizeof(kNewf)) == 0 ? 1 : 0;
    }
    return 1;
}

#endif // RSBS_SINGLE_EXECUTABLE
