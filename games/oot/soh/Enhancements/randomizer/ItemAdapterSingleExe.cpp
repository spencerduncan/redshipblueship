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
 * questMapping / songMapping / customItemsMapping / jabbernutMapping /
 * gregMapping: "ITEM_X" or "RG_X" and their "_Faded" twins), so a drawer can
 * hand them to the Gui's texture map as-is once OoT has booted.
 *
 * SoH'S OPTIONAL SECTIONS (#1142, #1158). After those come SoH's two compact
 * dungeon tables, both always rows of every file (the overlay shows one, by
 * DungeonItems.DisplayMaps, and only under DisplayType.DungeonItems): "Dungeon
 * Items" (itemTrackerDungeonsWithMapsCompact, 36 cells column-major with one
 * blank, Thieves' Hideout in cell 35) and "Dungeon Keys" (itemTrackerDungeons-
 * Compact, 15 cells). Then, through `countFor`, the groups a randomizer file
 * finds only under its frozen option, in SoH's main-window order: "Greg" (any
 * randomizer file), "Fishing Pole", "Bean Souls", "Boss Souls", "Jabber Nuts",
 * "Ocarina Buttons", "Overworld Keys", each on a line of its own (OoTItemGrid).
 * OoT's options are not in the save buffer: they are the loaded seed's
 * (Rando::Context, frozen into the file at creation and loaded with it), read
 * through OoTItemSeed, as are the small-key maxes of a randomizer file (its
 * Master Quest dungeons, its Gerudo Fortress option). Ownership is the buffer's own: the dungeon fields and
 * the randomizer-inf flags SoH's DrawItem / DrawDungeonItem test.
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
#include "soh/Enhancements/randomizer/SeedContext.h" // Rando::Context: the loaded seed's frozen options (#1142)
#include "soh/Enhancements/randomizer/dungeon.h"     // Rando::DungeonInfo::IsMQ: the seed's Master Quest dungeons
#include "soh/SaveManager.h"                         // SaveManager::Instance, for the new-file lock's seam (#873)
// Declared only in SaveManager.h's C branch (#ifndef __cplusplus).
extern "C" void Save_InitFile(int isDebug);
// SaveManager.cpp: whether an OoT SaveContext blob holds a created file (#873).
extern "C" int OoT_SaveBlobIsStarted(const uint8_t* blob, size_t blobSize);

// src/common. Included outside any extern "C" block: the headers manage their
// own linkage (matching TrackerAdapterSingleExe.cpp).
#include "combo_item_view.h"
#include "foreign_items.h"    // RSBS_XITEM_*: the shared families a row is OoT's copy of (#1065)
#include "game.h"             // OOT_SAVE_CONTEXT_SIZE
#include "shared_resources.h" // Combo_SaveIsLiveFile, Combo_MakeHealthQuarters (the Shared column's live view, #971)

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

/** The ROM-free lock's stand-in for the frozen options (OoT_ItemAdapter_TestOverrideOptions). */
bool sOoTItemTestOptionsOn = false;
unsigned sOoTItemTestOptions = 0u;

/**
 * The file's frozen seed (#1142): OoT keeps its options and its per-dungeon
 * Master Quest choice in the seed's context, frozen into the file at its
 * creation and loaded with it, never in the SaveContext the view hands over.
 * One adapter call reads the context once through this (the view walks every
 * row for each row it hands out, so a read per group would multiply). The
 * ROM-free lock stands in for it (OoT_ItemAdapter_TestOverrideOptions).
 */
struct OoTItemSeed {
    Rando::Context* ctx; // null under the test override, or with no seed loaded

    /** Option `key`; 0 (off) with no context. */
    uint8_t Option(RandomizerSettingKey key) const;
    /** Did the seed make dungeon `scene` Master Quest (ResourceMgr_IsSceneMasterQuest's randomizer leg)? */
    bool DungeonMQ(uint8_t scene) const;
};

OoTItemSeed OoTItemSeedRead(void) {
    return OoTItemSeed{ sOoTItemTestOptionsOn ? nullptr : Rando::Context::GetInstance().get() };
}

bool OoTItemSeed::DungeonMQ(uint8_t scene) const {
    if (sOoTItemTestOptionsOn) {
        return scene < 16 && (sOoTItemTestOptions & (1u << (16 + scene))) != 0;
    }
    if (ctx == nullptr) {
        return false;
    }
    Rando::Dungeons* dungeons = ctx->GetDungeons().get();
    Rando::DungeonInfo* dungeon = dungeons != nullptr ? dungeons->GetDungeonFromScene(scene) : nullptr;
    return dungeon != nullptr && dungeon->IsMQ();
}

uint8_t OoTItemSeed::Option(RandomizerSettingKey key) const {
    if (sOoTItemTestOptionsOn) {
        const unsigned b = sOoTItemTestOptions;
        switch (key) {
            case RSK_SHUFFLE_BEAN_SOULS:
                return (b & (1u << 0)) != 0 ? 1 : 0;
            case RSK_SHUFFLE_BOSS_SOULS:
                if ((b & (1u << 1)) == 0) {
                    return RO_BOSS_SOULS_OFF;
                }
                return (b & (1u << 2)) != 0 ? RO_BOSS_SOULS_ON_PLUS_GANON : RO_BOSS_SOULS_ON;
            case RSK_SHUFFLE_SPEAK:
                return (b & (1u << 3)) != 0 ? 1 : 0;
            case RSK_SHUFFLE_OCARINA_BUTTONS:
                return (b & (1u << 4)) != 0 ? 1 : 0;
            case RSK_LOCK_OVERWORLD_DOORS:
                return (b & (1u << 5)) != 0 ? 1 : 0;
            case RSK_SHUFFLE_FISHING_POLE:
                return (b & (1u << 6)) != 0 ? 1 : 0;
            case RSK_GERUDO_FORTRESS:
                return (uint8_t)((b >> 7) & 3u);
            default:
                return 0;
        }
    }
    return ctx != nullptr ? (uint8_t)ctx->GetOption(key).Get() : 0u;
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

/** INV_CONTENT(item) over `src`, or ITEM_NONE for an id past gItemSlots: SoH's
 *  custom ids (ITEM_FISHING_POLE, 0x59, on) name no inventory slot, and the
 *  macro would index past the table (ports SoH 7a7f942b8's bound; #983/#984). */
uint8_t InvContentOf(const SaveContext* src, int item) {
    if (item < 0 || item >= ARRAY_COUNT(OoT_gItemSlots)) {
        return ITEM_NONE;
    }
    return INV_CONTENT(item);
}

// ---- icon keys and names for inventory contents ----------------------------

struct ItemKey {
    uint8_t id;
    const char* key;
    const char* faded;
    const char* name;
};

#define IK(id, name) { (uint8_t)(id), #id, #id "_Faded", name }

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
    const char* slotKey;
    RowKind kind;
    uint8_t item;     // ITEM_* for inventory rows and icon lookup
    uint32_t data;    // upgrade type, bit mask, quest bit, bottle index or fixed cap
    const char* name; // fixed name (equipment/quest rows), or the empty-slot name when non-NULL
    const char* key;  // fixed icon key (equipment/quest rows)
    const char* keyFaded;
};

#define INV(slotKey, item) { "Inventory", slotKey, RK_INV, (uint8_t)(item), 0, nullptr, nullptr, nullptr }
#define INV_AMMO(slotKey, item, upg) \
    { "Inventory", slotKey, RK_INV_AMMO, (uint8_t)(item), (uint32_t)(upg), nullptr, nullptr, nullptr }
#define INV_FIXED(slotKey, item, cap) \
    { "Inventory", slotKey, RK_INV_FIXED, (uint8_t)(item), (uint32_t)(cap), nullptr, nullptr, nullptr }
#define INV_NAMED(slotKey, item, emptyName) \
    { "Inventory", slotKey, RK_INV, (uint8_t)(item), 0, emptyName, nullptr, nullptr }
#define BOTTLE(slotKey, n) \
    { "Inventory", slotKey, RK_BOTTLE, (uint8_t)ITEM_BOTTLE, (uint32_t)(n), "Bottle", nullptr, nullptr }
#define EQUIP(slotKey, id, mask, name) \
    { "Equipment", slotKey, RK_EQUIP, (uint8_t)(id), (uint32_t)(mask), name, #id, #id "_Faded" }
#define QUEST(group, slotKey, id, name) { group, slotKey, RK_QUEST, 0, (uint32_t)(id), name, #id, #id "_Faded" }
#define MISC(slotKey, kind, item) { "Misc", slotKey, kind, (uint8_t)(item), 0, nullptr, nullptr, nullptr }

// Order and membership follow the native tracker's default sections; the
// setting-dependent sections (dungeon items, Greg, triforce pieces, boss and
// bean souls, ocarina buttons, overworld keys, the fishing pole, jabber nuts,
// action shuffle) are left out of v1.
const RowDef kRows[] = {
    // Inventory (inventoryItems)
    INV_AMMO("sticks", ITEM_STICK, UPG_STICKS),
    INV_AMMO("nuts", ITEM_NUT, UPG_NUTS),
    INV_AMMO("bombs", ITEM_BOMB, UPG_BOMB_BAG),
    INV_AMMO("bow", ITEM_BOW, UPG_QUIVER),
    INV("arrow_fire", ITEM_ARROW_FIRE),
    INV("dins_fire", ITEM_DINS_FIRE),
    INV_AMMO("slingshot", ITEM_SLINGSHOT, UPG_BULLET_BAG),
    INV("ocarina", ITEM_OCARINA_FAIRY),
    INV_FIXED("bombchus", ITEM_BOMBCHU, 50),
    INV("hookshot", ITEM_HOOKSHOT),
    INV("arrow_ice", ITEM_ARROW_ICE),
    INV("farores_wind", ITEM_FARORES_WIND),
    INV("boomerang", ITEM_BOOMERANG),
    INV("lens", ITEM_LENS),
    INV_FIXED("beans", ITEM_BEAN, 10),
    INV("hammer", ITEM_HAMMER),
    INV("arrow_light", ITEM_ARROW_LIGHT),
    INV("nayrus_love", ITEM_NAYRUS_LOVE),
    BOTTLE("bottle_1", 0),
    BOTTLE("bottle_2", 1),
    BOTTLE("bottle_3", 2),
    BOTTLE("bottle_4", 3),
    INV_NAMED("trade_adult", ITEM_POCKET_EGG, "Adult Trade Item"),
    INV_NAMED("trade_child", ITEM_MASK_KEATON, "Child Trade Item"),
    // Equipment (equipmentItems): the owned-equipment bits the native rows test
    EQUIP("sword_kokiri", ITEM_SWORD_KOKIRI, 1 << 0, "Kokiri Sword"),
    EQUIP("sword_master", ITEM_SWORD_MASTER, 1 << 1, "Master Sword"),
    EQUIP("sword_bgs", ITEM_SWORD_BGS, 1 << 2, "Biggoron's Sword"),
    EQUIP("tunic_kokiri", ITEM_TUNIC_KOKIRI, 1 << 8, "Kokiri Tunic"),
    EQUIP("tunic_goron", ITEM_TUNIC_GORON, 1 << 9, "Goron Tunic"),
    EQUIP("tunic_zora", ITEM_TUNIC_ZORA, 1 << 10, "Zora Tunic"),
    EQUIP("shield_deku", ITEM_SHIELD_DEKU, 1 << 4, "Deku Shield"),
    EQUIP("shield_hylian", ITEM_SHIELD_HYLIAN, 1 << 5, "Hylian Shield"),
    EQUIP("shield_mirror", ITEM_SHIELD_MIRROR, 1 << 6, "Mirror Shield"),
    EQUIP("boots_kokiri", ITEM_BOOTS_KOKIRI, 1 << 12, "Kokiri Boots"),
    EQUIP("boots_iron", ITEM_BOOTS_IRON, 1 << 13, "Iron Boots"),
    EQUIP("boots_hover", ITEM_BOOTS_HOVER, 1 << 14, "Hover Boots"),
    // Misc (miscItems)
    MISC("strength", RK_STRENGTH, ITEM_BRACELET),
    MISC("scale", RK_SCALE, ITEM_SCALE_SILVER),
    MISC("wallet", RK_WALLET, ITEM_WALLET_ADULT),
    MISC("heart_containers", RK_HEART_CONT, ITEM_HEART_CONTAINER),
    MISC("heart_pieces", RK_HEART_PIECE, ITEM_HEART_PIECE),
    MISC("magic", RK_MAGIC, ITEM_MAGIC_SMALL),
    QUEST("Misc", "gerudo_card", QUEST_GERUDO_CARD, "Gerudo's Card"),
    { "Misc", "gs_tokens", RK_TOKENS, 0, (uint32_t)QUEST_SKULL_TOKEN, "Gold Skulltula Tokens", "QUEST_SKULL_TOKEN",
      "QUEST_SKULL_TOKEN_Faded" },
    QUEST("Misc", "stone_of_agony", QUEST_STONE_OF_AGONY, "Stone of Agony"),
    // Dungeon rewards (dungeonRewardStones, dungeonRewardMedallions)
    QUEST("Dungeon Rewards", "emerald", QUEST_KOKIRI_EMERALD, "Kokiri's Emerald"),
    QUEST("Dungeon Rewards", "ruby", QUEST_GORON_RUBY, "Goron's Ruby"),
    QUEST("Dungeon Rewards", "sapphire", QUEST_ZORA_SAPPHIRE, "Zora's Sapphire"),
    QUEST("Dungeon Rewards", "medallion_forest", QUEST_MEDALLION_FOREST, "Forest Medallion"),
    QUEST("Dungeon Rewards", "medallion_fire", QUEST_MEDALLION_FIRE, "Fire Medallion"),
    QUEST("Dungeon Rewards", "medallion_water", QUEST_MEDALLION_WATER, "Water Medallion"),
    QUEST("Dungeon Rewards", "medallion_spirit", QUEST_MEDALLION_SPIRIT, "Spirit Medallion"),
    QUEST("Dungeon Rewards", "medallion_shadow", QUEST_MEDALLION_SHADOW, "Shadow Medallion"),
    QUEST("Dungeon Rewards", "medallion_light", QUEST_MEDALLION_LIGHT, "Light Medallion"),
    // Songs (songItems)
    QUEST("Songs", "song_lullaby", QUEST_SONG_LULLABY, "Zelda's Lullaby"),
    QUEST("Songs", "song_epona", QUEST_SONG_EPONA, "Epona's Song"),
    QUEST("Songs", "song_saria", QUEST_SONG_SARIA, "Saria's Song"),
    QUEST("Songs", "song_sun", QUEST_SONG_SUN, "Sun's Song"),
    QUEST("Songs", "song_time", QUEST_SONG_TIME, "Song of Time"),
    QUEST("Songs", "song_storms", QUEST_SONG_STORMS, "Song of Storms"),
    QUEST("Songs", "song_minuet", QUEST_SONG_MINUET, "Minuet of Forest"),
    QUEST("Songs", "song_bolero", QUEST_SONG_BOLERO, "Bolero of Fire"),
    QUEST("Songs", "song_serenade", QUEST_SONG_SERENADE, "Serenade of Water"),
    QUEST("Songs", "song_requiem", QUEST_SONG_REQUIEM, "Requiem of Spirit"),
    QUEST("Songs", "song_nocturne", QUEST_SONG_NOCTURNE, "Nocturne of Shadow"),
    QUEST("Songs", "song_prelude", QUEST_SONG_PRELUDE, "Prelude of Light"),
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

/** The shared-resource kind of capacity upgrade `upg` (#967), or NONE when the pair does not share it:
 *  the quiver, bomb bag, stick bag and nut bag (shared_items.h TRIM_TO_SHARED_MAX). */
uint8_t SharedTierKindOf(uint32_t upg) {
    switch (upg) {
        case UPG_QUIVER:
            return RSBS_SHARED_RES_QUIVER_TIER;
        case UPG_BOMB_BAG:
            return RSBS_SHARED_RES_BOMB_BAG_TIER;
        case UPG_STICKS:
            return RSBS_SHARED_RES_STICK_TIER;
        case UPG_NUTS:
            return RSBS_SHARED_RES_NUT_TIER;
        default:
            return RSBS_SHARED_RES_NONE;
    }
}

bool Derive(const SaveContext* src, const RowDef& def, ComboItemRow* out) {
    out->group = def.group;
    out->slotKey = def.slotKey;
    switch (def.kind) {
        case RK_INV:
            FillSlot(out, InvContentOf(src, def.item), def);
            if (def.item == (uint8_t)ITEM_HOOKSHOT && out->have) {
                // #1143: SoH's Hookshot Identifier letter for the tier held.
                out->identifier = InvContentOf(src, def.item) == ITEM_LONGSHOT ? 'L' : 'H';
            }
            return true;
        case RK_INV_AMMO:
            FillSlot(out, InvContentOf(src, def.item), def);
            out->capacityMax = (int)CAPACITY(def.data, 3); // #1143: SoH's maxCapacity, the largest bag
            // This game's copy of a shared capacity tier: the view dims it once the Shared column holds it.
            out->sharedKind = SharedTierKindOf(def.data);
            out->sharedTier = (uint8_t)CUR_UPG_VALUE(def.data);
            if (out->have) {
                out->count = AMMO(def.item);
                out->max = CUR_CAPACITY(def.data);
            }
            return true;
        case RK_INV_FIXED:
            FillSlot(out, InvContentOf(src, def.item), def);
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
            if (def.item == ITEM_SWORD_KOKIRI) {
                // The shared Kokiri Sword (#886): the owned bit is the pool's 0/1.
                out->sharedKind = RSBS_SHARED_RES_KOKIRI_SWORD;
                out->sharedTier = out->have ? 1 : 0;
            }
            return true;
        case RK_QUEST:
        case RK_TOKENS:
            out->name = def.name;
            out->iconKey = def.key;
            out->iconKeyFaded = def.keyFaded;
            out->have = CHECK_QUEST_ITEM(def.data) != 0;
            if (std::strcmp(def.group, "Songs") == 0) {
                out->iconAspect = 1.0f / 1.5f; // DrawSong: a note iconSize / 1.5 wide, centred
            }
            if (def.kind == RK_TOKENS) {
                out->count = gSaveContext.inventory.gsTokens;
                out->max = 100;
                // DrawItemCount: QUEST_SKULL_TOKEN's ceiling, and its amount once
                // full, are IM_COL_RED where every other row's are green.
                out->countAccent = (uint8_t)COMBO_ITEM_ACCENT_RED;
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
            // #1143: SoH's maxCapacity, 500, or 999 with the Tycoon's Wallet in the pool; a snapshot cannot read
            // the frozen option, so the tycoon's tier is the sign of it.
            out->capacityMax = (int)CAPACITY(UPG_WALLET, tier >= 3 ? 3 : 2);
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

// ---- SoH's compact dungeon tables (#1142) ----------------------------------------

enum DungeonCellKind : uint8_t { DC_PAD, DC_SMALL_KEY, DC_BOSS_KEY, DC_MAP, DC_COMPASS };

struct DungeonCell {
    DungeonCellKind kind;
    uint8_t scene; // the dungeonItems / dungeonKeys index (Ganon's boss key: SCENE_GANONS_TOWER)
    const char* slotKey;
    const char* name;
    const char* label; // SoH's short name over the icon (DrawDungeonItem), or NULL
};

#define DSK(slotKey, scene, dname, label) { DC_SMALL_KEY, (uint8_t)(scene), slotKey, dname " Small Key", label }
#define DBK(slotKey, scene, dname) { DC_BOSS_KEY, (uint8_t)(scene), slotKey, dname " Boss Key", nullptr }
#define DMAP(slotKey, scene, dname, label) { DC_MAP, (uint8_t)(scene), slotKey, dname " Map", label }
#define DCOMP(slotKey, scene, dname) { DC_COMPASS, (uint8_t)(scene), slotKey, dname " Compass", nullptr }
#define DPAD { DC_PAD, 0, nullptr, "", nullptr }

// GetDungeonItemsVector(itemTrackerDungeonsWithMapsCompact): six dungeons a block, column-major, an
// ITEM_NONE blank under a shorter column, Thieves' Hideout patched into cell 35 (UpdateVectors).
const DungeonCell kDungeonWithMaps[] = {
    DSK("small_key_forest", SCENE_FOREST_TEMPLE, "Forest Temple", "FRST"),
    DSK("small_key_fire", SCENE_FIRE_TEMPLE, "Fire Temple", "FIRE"),
    DSK("small_key_water", SCENE_WATER_TEMPLE, "Water Temple", "WATR"),
    DSK("small_key_spirit", SCENE_SPIRIT_TEMPLE, "Spirit Temple", "SPRT"),
    DSK("small_key_shadow", SCENE_SHADOW_TEMPLE, "Shadow Temple", "SHDW"),
    DSK("small_key_botw", SCENE_BOTTOM_OF_THE_WELL, "Bottom of the Well", "BOTW"),
    DBK("boss_key_forest", SCENE_FOREST_TEMPLE, "Forest Temple"),
    DBK("boss_key_fire", SCENE_FIRE_TEMPLE, "Fire Temple"),
    DBK("boss_key_water", SCENE_WATER_TEMPLE, "Water Temple"),
    DBK("boss_key_spirit", SCENE_SPIRIT_TEMPLE, "Spirit Temple"),
    DBK("boss_key_shadow", SCENE_SHADOW_TEMPLE, "Shadow Temple"),
    DMAP("map_botw", SCENE_BOTTOM_OF_THE_WELL, "Bottom of the Well", nullptr),
    DMAP("map_forest", SCENE_FOREST_TEMPLE, "Forest Temple", nullptr),
    DMAP("map_fire", SCENE_FIRE_TEMPLE, "Fire Temple", nullptr),
    DMAP("map_water", SCENE_WATER_TEMPLE, "Water Temple", nullptr),
    DMAP("map_spirit", SCENE_SPIRIT_TEMPLE, "Spirit Temple", nullptr),
    DMAP("map_shadow", SCENE_SHADOW_TEMPLE, "Shadow Temple", nullptr),
    DCOMP("compass_botw", SCENE_BOTTOM_OF_THE_WELL, "Bottom of the Well"),
    DCOMP("compass_forest", SCENE_FOREST_TEMPLE, "Forest Temple"),
    DCOMP("compass_fire", SCENE_FIRE_TEMPLE, "Fire Temple"),
    DCOMP("compass_water", SCENE_WATER_TEMPLE, "Water Temple"),
    DCOMP("compass_spirit", SCENE_SPIRIT_TEMPLE, "Spirit Temple"),
    DCOMP("compass_shadow", SCENE_SHADOW_TEMPLE, "Shadow Temple"),
    DPAD,
    DMAP("map_deku", SCENE_DEKU_TREE, "Deku Tree", "DEKU"),
    DMAP("map_dodongo", SCENE_DODONGOS_CAVERN, "Dodongo's Cavern", "DCVN"),
    DMAP("map_jabu", SCENE_JABU_JABU, "Jabu-Jabu's Belly", "JABU"),
    DMAP("map_ice", SCENE_ICE_CAVERN, "Ice Cavern", "ICE"),
    DSK("small_key_ganon", SCENE_INSIDE_GANONS_CASTLE, "Ganon's Castle", "GANON"),
    DSK("small_key_gtg", SCENE_GERUDO_TRAINING_GROUND, "Gerudo Training Ground", "GTG"),
    DCOMP("compass_deku", SCENE_DEKU_TREE, "Deku Tree"),
    DCOMP("compass_dodongo", SCENE_DODONGOS_CAVERN, "Dodongo's Cavern"),
    DCOMP("compass_jabu", SCENE_JABU_JABU, "Jabu-Jabu's Belly"),
    DCOMP("compass_ice", SCENE_ICE_CAVERN, "Ice Cavern"),
    DBK("boss_key_ganon", SCENE_GANONS_TOWER, "Ganon's Castle"),
    DSK("small_key_hideout", SCENE_THIEVES_HIDEOUT, "Thieves' Hideout", "HIDE"),
};

// GetDungeonItemsVector(itemTrackerDungeonsCompact): the keys alone, the last three dungeons a block of three.
const DungeonCell kDungeonKeysOnly[] = {
    DSK("small_key_forest", SCENE_FOREST_TEMPLE, "Forest Temple", "FRST"),
    DSK("small_key_fire", SCENE_FIRE_TEMPLE, "Fire Temple", "FIRE"),
    DSK("small_key_water", SCENE_WATER_TEMPLE, "Water Temple", "WATR"),
    DSK("small_key_spirit", SCENE_SPIRIT_TEMPLE, "Spirit Temple", "SPRT"),
    DSK("small_key_shadow", SCENE_SHADOW_TEMPLE, "Shadow Temple", "SHDW"),
    DSK("small_key_ganon", SCENE_INSIDE_GANONS_CASTLE, "Ganon's Castle", "GANON"),
    DBK("boss_key_forest", SCENE_FOREST_TEMPLE, "Forest Temple"),
    DBK("boss_key_fire", SCENE_FIRE_TEMPLE, "Fire Temple"),
    DBK("boss_key_water", SCENE_WATER_TEMPLE, "Water Temple"),
    DBK("boss_key_spirit", SCENE_SPIRIT_TEMPLE, "Spirit Temple"),
    DBK("boss_key_shadow", SCENE_SHADOW_TEMPLE, "Shadow Temple"),
    DBK("boss_key_ganon", SCENE_GANONS_TOWER, "Ganon's Castle"),
    DSK("small_key_botw", SCENE_BOTTOM_OF_THE_WELL, "Bottom of the Well", "BOTW"),
    DSK("small_key_gtg", SCENE_GERUDO_TRAINING_GROUND, "Gerudo Training Ground", "GTG"),
    DSK("small_key_hideout", SCENE_THIEVES_HIDEOUT, "Thieves' Hideout", "HIDE"),
};

#undef DSK
#undef DBK
#undef DMAP
#undef DCOMP
#undef DPAD

constexpr int kDungeonWithMapsCount = (int)(sizeof(kDungeonWithMaps) / sizeof(kDungeonWithMaps[0]));
constexpr int kDungeonKeysOnlyCount = (int)(sizeof(kDungeonKeysOnly) / sizeof(kDungeonKeysOnly[0]));
static_assert(kDungeonWithMapsCount == 36 && kDungeonKeysOnlyCount == 15, "SoH's two compact dungeon tables");
constexpr int kFixedRowCount = kRowCount + kDungeonWithMapsCount + kDungeonKeysOnlyCount;

/** A dungeon's small keys as plain numbers (macros.h *_SMALL_KEY_MAX), vanilla or Master Quest. */
int DungeonKeyMax(uint8_t scene, bool mq) {
    switch (scene) {
        case SCENE_FOREST_TEMPLE:
            return mq ? 6 : 5;
        case SCENE_FIRE_TEMPLE:
            return mq ? 5 : 8;
        case SCENE_WATER_TEMPLE:
            return mq ? 2 : 6;
        case SCENE_SPIRIT_TEMPLE:
            return mq ? 7 : 5;
        case SCENE_SHADOW_TEMPLE:
            return mq ? 6 : 5;
        case SCENE_BOTTOM_OF_THE_WELL:
            return mq ? 2 : 3;
        case SCENE_GERUDO_TRAINING_GROUND:
            return mq ? 3 : 9;
        case SCENE_THIEVES_HIDEOUT:
            return 4; // GERUDO_FORTRESS_SMALL_KEY_MAX
        case SCENE_INSIDE_GANONS_CASTLE:
            return mq ? 3 : 2;
        default:
            return 0;
    }
}

/**
 * GetItemCurrentAndMax's small-key max over `src`: a randomizer file follows its
 * seed (each *_SMALL_KEY_MAX asks ResourceMgr_IsSceneMasterQuest, whose
 * randomizer leg is the seed's per-dungeon choice; Thieves' Hideout follows
 * RSK_GERUDO_FORTRESS: Normal 4, Fast 1, Free 0), any other file its quest.
 */
int OoTItemKeyMax(const SaveContext* src, uint8_t scene) {
    if (!IS_RANDO) {
        return DungeonKeyMax(scene, gSaveContext.ship.quest.id == QUEST_MASTER);
    }
    const OoTItemSeed seed = OoTItemSeedRead();
    if (scene == SCENE_THIEVES_HIDEOUT) {
        switch (seed.Option(RSK_GERUDO_FORTRESS)) {
            case RO_GF_CARPENTERS_NORMAL:
                return DungeonKeyMax(scene, false);
            case RO_GF_CARPENTERS_FAST:
                return 1;
            default: // RO_GF_CARPENTERS_FREE (SoH answers 0 for any other value too)
                return 0;
        }
    }
    return DungeonKeyMax(scene, seed.DungeonMQ(scene));
}

/** DrawDungeonItem over `src`: a small key is lit while its count is not -1 (SoH: dungeonKeys >= 0), drawn as
 *  collected (ship.stats.dungeonKeys) of the dungeon's keys with the keys in hand (GetItemCurrentAndMax);
 *  the boss key, map and compass are bit (item - ITEM_KEY_BOSS) of dungeonItems. */
void DeriveDungeonCell(const SaveContext* src, const DungeonCell& cell, const char* group, ComboItemRow* out) {
    out->group = group;
    out->slotKey = cell.slotKey;
    out->name = cell.name;
    out->label = cell.label;
    switch (cell.kind) {
        case DC_PAD:
            out->pad = true;
            return;
        case DC_SMALL_KEY: {
            const s8 held = gSaveContext.inventory.dungeonKeys[cell.scene];
            out->iconKey = "ITEM_KEY_SMALL";
            out->iconKeyFaded = "ITEM_KEY_SMALL_Faded";
            out->have = held >= 0;
            out->countKind = (uint8_t)COMBO_ITEM_COUNT_KIND_KEYS;
            out->held = held < 0 ? 0 : (int)held;
            out->count = (int)gSaveContext.ship.stats.dungeonKeys[cell.scene];
            out->max = OoTItemKeyMax(src, cell.scene);
            return;
        }
        case DC_BOSS_KEY:
        case DC_MAP:
        case DC_COMPASS: {
            const int item = cell.kind == DC_BOSS_KEY ? ITEM_KEY_BOSS
                             : cell.kind == DC_MAP    ? ITEM_DUNGEON_MAP
                                                      : ITEM_COMPASS;
            out->iconKey = cell.kind == DC_BOSS_KEY ? "ITEM_KEY_BOSS"
                           : cell.kind == DC_MAP    ? "ITEM_DUNGEON_MAP"
                                                    : "ITEM_COMPASS";
            out->iconKeyFaded = cell.kind == DC_BOSS_KEY ? "ITEM_KEY_BOSS_Faded"
                                : cell.kind == DC_MAP    ? "ITEM_DUNGEON_MAP_Faded"
                                                         : "ITEM_COMPASS_Faded";
            out->have = (gSaveContext.inventory.dungeonItems[cell.scene] & (1u << (item - ITEM_KEY_BOSS))) != 0;
            return;
        }
    }
}

// ---- the groups a randomizer file shows by its frozen options (#1142) --------------

struct FlagRow {
    const char* slotKey;
    const char* name;
    const char* label; // SoH's short name over the icon, or NULL
    RandomizerInf flag;
    const char* key;
    const char* faded;
    uint32_t family; // the shared-family slot this row is OoT's copy of (foreign_items.h), or 0
};

#define FLAG_ROW(slotKey, name, label, flag, icon) { slotKey, name, label, flag, #icon, #icon "_Faded", 0u }

const FlagRow kGregRows[] = { FLAG_ROW("greg", "Greg the Green Rupee", nullptr, RAND_INF_GREG_FOUND,
                                       ITEM_RUPEE_GREEN) };
const FlagRow kFishingPoleRows[] = { FLAG_ROW("fishing_pole", "Fishing Pole", nullptr, RAND_INF_FISHING_POLE_FOUND,
                                              ITEM_FISHING_POLE) };
// beanSoulItems, drawn with the Magic Bean (ITEM_TRACKER_ITEM_CUSTOM) and itemTrackerBeanShortNames.
const FlagRow kBeanSoulRows[] = {
    FLAG_ROW("bean_soul_dmc", "Death Mountain Crater Bean Soul", "DMC", RAND_INF_DEATH_MOUNTAIN_CRATER_BEAN_SOUL,
             ITEM_BEAN),
    FLAG_ROW("bean_soul_dmt", "Death Mountain Trail Bean Soul", "DMT", RAND_INF_DEATH_MOUNTAIN_TRAIL_BEAN_SOUL,
             ITEM_BEAN),
    FLAG_ROW("bean_soul_dc", "Desert Colossus Bean Soul", "DC", RAND_INF_DESERT_COLOSSUS_BEAN_SOUL, ITEM_BEAN),
    FLAG_ROW("bean_soul_gv", "Gerudo Valley Bean Soul", "GV", RAND_INF_GERUDO_VALLEY_BEAN_SOUL, ITEM_BEAN),
    FLAG_ROW("bean_soul_gy", "Graveyard Bean Soul", "GY", RAND_INF_GRAVEYARD_BEAN_SOUL, ITEM_BEAN),
    FLAG_ROW("bean_soul_kf", "Kokiri Forest Bean Soul", "KF", RAND_INF_KOKIRI_FOREST_BEAN_SOUL, ITEM_BEAN),
    FLAG_ROW("bean_soul_lh", "Lake Hylia Bean Soul", "LA", RAND_INF_LAKE_HYLIA_BEAN_SOUL, ITEM_BEAN),
    FLAG_ROW("bean_soul_lwb", "Lost Woods Bridge Bean Soul", "LWB", RAND_INF_LOST_WOODS_BRIDGE_BEAN_SOUL, ITEM_BEAN),
    FLAG_ROW("bean_soul_lwt", "Lost Woods Theatre Bean Soul", "LWT", RAND_INF_LOST_WOODS_BEAN_SOUL, ITEM_BEAN),
    FLAG_ROW("bean_soul_zr", "Zora's River Bean Soul", "ZR", RAND_INF_ZORAS_RIVER_BEAN_SOUL, ITEM_BEAN),
};
// bossSoulItems and itemTrackerBossShortNames; Ganon's is held unless the file shuffles it (OoTItemFlagHeld).
const FlagRow kBossSoulRows[] = {
    FLAG_ROW("boss_soul_gohma", "Gohma's Soul", "GOHMA", RAND_INF_GOHMA_SOUL, RG_GOHMA_SOUL),
    FLAG_ROW("boss_soul_kd", "King Dodongo's Soul", "KD", RAND_INF_KING_DODONGO_SOUL, RG_KING_DODONGO_SOUL),
    FLAG_ROW("boss_soul_bari", "Barinade's Soul", "BARI", RAND_INF_BARINADE_SOUL, RG_BARINADE_SOUL),
    FLAG_ROW("boss_soul_pg", "Phantom Ganon's Soul", "PG", RAND_INF_PHANTOM_GANON_SOUL, RG_PHANTOM_GANON_SOUL),
    FLAG_ROW("boss_soul_volv", "Volvagia's Soul", "VOLV", RAND_INF_VOLVAGIA_SOUL, RG_VOLVAGIA_SOUL),
    FLAG_ROW("boss_soul_morph", "Morpha's Soul", "MORPH", RAND_INF_MORPHA_SOUL, RG_MORPHA_SOUL),
    FLAG_ROW("boss_soul_bongo", "Bongo Bongo's Soul", "BONGO", RAND_INF_BONGO_BONGO_SOUL, RG_BONGO_BONGO_SOUL),
    FLAG_ROW("boss_soul_twin", "Twinrova's Soul", "TWIN", RAND_INF_TWINROVA_SOUL, RG_TWINROVA_SOUL),
    FLAG_ROW("boss_soul_ganon", "Ganon's Soul", "GANON", RAND_INF_GANON_SOUL, RG_GANON_SOUL),
};
// jabbernutItems (jabbernutMapping: the Deku Nut) and itemTrackerJabberNutShortNames.
const FlagRow kJabberNutRows[] = {
    FLAG_ROW("jabber_deku", "Deku Jabber Nut", "DEKU", RAND_INF_CAN_SPEAK_DEKU, RG_SPEAK_DEKU),
    FLAG_ROW("jabber_gerudo", "Gerudo Jabber Nut", "GERUDO", RAND_INF_CAN_SPEAK_GERUDO, RG_SPEAK_GERUDO),
    FLAG_ROW("jabber_goron", "Goron Jabber Nut", "GORON", RAND_INF_CAN_SPEAK_GORON, RG_SPEAK_GORON),
    FLAG_ROW("jabber_hylian", "Hylian Jabber Nut", "HYLIAN", RAND_INF_CAN_SPEAK_HYLIAN, RG_SPEAK_HYLIAN),
    FLAG_ROW("jabber_kokiri", "Kokiri Jabber Nut", "KOKIRI", RAND_INF_CAN_SPEAK_KOKIRI, RG_SPEAK_KOKIRI),
    FLAG_ROW("jabber_zora", "Zora Jabber Nut", "ZORA", RAND_INF_CAN_SPEAK_ZORA, RG_SPEAK_ZORA),
};
// ocarinaButtonItems, drawn with the Ocarina of Time, and itemTrackerOcarinaButtonShortNames. Each is OoT's
// copy of a Shared Ocarina Buttons family slot (#1067), as MM's rows are.
const FlagRow kOcarinaButtonRows[] = {
    { "ocarina_btn_a", "Ocarina A Button", "A", RAND_INF_HAS_OCARINA_A, "ITEM_OCARINA_TIME", "ITEM_OCARINA_TIME_Faded",
      RSBS_XITEM_SHARED_OCARINA_BUTTONS },
    { "ocarina_btn_c_up", "Ocarina C Up Button", "C-U", RAND_INF_HAS_OCARINA_C_UP, "ITEM_OCARINA_TIME",
      "ITEM_OCARINA_TIME_Faded", RSBS_XFAMILY_SLOT_OCARINA_C_UP },
    { "ocarina_btn_c_down", "Ocarina C Down Button", "C-D", RAND_INF_HAS_OCARINA_C_DOWN, "ITEM_OCARINA_TIME",
      "ITEM_OCARINA_TIME_Faded", RSBS_XFAMILY_SLOT_OCARINA_C_DOWN },
    { "ocarina_btn_c_left", "Ocarina C Left Button", "C-L", RAND_INF_HAS_OCARINA_C_LEFT, "ITEM_OCARINA_TIME",
      "ITEM_OCARINA_TIME_Faded", RSBS_XFAMILY_SLOT_OCARINA_C_LEFT },
    { "ocarina_btn_c_right", "Ocarina C Right Button", "C-R", RAND_INF_HAS_OCARINA_C_RIGHT, "ITEM_OCARINA_TIME",
      "ITEM_OCARINA_TIME_Faded", RSBS_XFAMILY_SLOT_OCARINA_C_RIGHT },
};
// overworldKeyItems, drawn with the small key, and itemTrackerOverworldKeyShortNames.
const FlagRow kOverworldKeyRows[] = {
    FLAG_ROW("ow_key_guard", "Guard House Key", "GUARD", RAND_INF_GUARD_HOUSE_KEY_OBTAINED, ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_mkbaz", "Market Bazaar Key", "MKBAZ", RAND_INF_MARKET_BAZAAR_KEY_OBTAINED, ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_mkpot", "Market Potion Shop Key", "MKPOT", RAND_INF_MARKET_POTION_SHOP_KEY_OBTAINED,
             ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_mask", "Mask Shop Key", "MASK", RAND_INF_MASK_SHOP_KEY_OBTAINED, ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_mksho", "Market Shooting Gallery Key", "MKSHO", RAND_INF_MARKET_SHOOTING_GALLERY_KEY_OBTAINED,
             ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_bowl", "Bombchu Bowling Key", "BOWL", RAND_INF_BOMBCHU_BOWLING_KEY_OBTAINED, ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_treasu", "Treasure Chest Game Building Key", "TREASU",
             RAND_INF_TREASURE_CHEST_GAME_BUILDING_KEY_OBTAINED, ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_chusho", "Bombchu Shop Key", "CHUSHO", RAND_INF_BOMBCHU_SHOP_KEY_OBTAINED, ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_rich", "Richards House Key", "RICH", RAND_INF_RICHARDS_HOUSE_KEY_OBTAINED, ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_alley", "Alley House Key", "ALLEY", RAND_INF_ALLEY_HOUSE_KEY_OBTAINED, ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_kakbaz", "Kak Bazaar Key", "KAKBAZ", RAND_INF_KAK_BAZAAR_KEY_OBTAINED, ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_kakpo", "Kak Potion Shop Key", "KAKPO", RAND_INF_KAK_POTION_SHOP_KEY_OBTAINED, ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_boss", "Boss House Key", "BOSS", RAND_INF_BOSS_HOUSE_KEY_OBTAINED, ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_granny", "Granny's Potion Shop Key", "GRANNY", RAND_INF_GRANNYS_POTION_SHOP_KEY_OBTAINED,
             ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_skull", "Skulltula House Key", "SKULL", RAND_INF_SKULLTULA_HOUSE_KEY_OBTAINED, ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_impas", "Impa's House Key", "IMPAS", RAND_INF_IMPAS_HOUSE_KEY_OBTAINED, ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_wind", "Windmill Key", "WIND", RAND_INF_WINDMILL_KEY_OBTAINED, ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_kaksho", "Kak Shooting Gallery Key", "KAKSHO", RAND_INF_KAK_SHOOTING_GALLERY_KEY_OBTAINED,
             ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_dampes", "Dampe's Hut Key", "DAMPES", RAND_INF_DAMPES_HUT_KEY_OBTAINED, ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_talons", "Talon's House Key", "TALONS", RAND_INF_TALONS_HOUSE_KEY_OBTAINED, ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_stable", "Stables Key", "STABLE", RAND_INF_STABLES_KEY_OBTAINED, ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_tower", "Back Tower Key", "TOWER", RAND_INF_BACK_TOWER_KEY_OBTAINED, ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_lab", "Hylia Lab Key", "LAB", RAND_INF_HYLIA_LAB_KEY_OBTAINED, ITEM_KEY_SMALL),
    FLAG_ROW("ow_key_fish", "Fishing Hole Key", "FISH", RAND_INF_FISHING_HOLE_KEY_OBTAINED, ITEM_KEY_SMALL),
};

#undef FLAG_ROW

/** One gated group: its rows, shown while `option` is on (RSK_NONE: in any randomizer file). */
struct GatedGroup {
    const char* group;
    const FlagRow* rows;
    int count;
    RandomizerSettingKey option;
};

#define GATED(group, rows, option) { group, rows, (int)(sizeof(rows) / sizeof(rows[0])), option }

// SoH's main-window order (UpdateVectors): Greg, then the fishing pole, the bean souls, the boss souls, the
// jabber nuts, the ocarina buttons and the overworld keys. The strings are the contract with the overlay's
// DisplayType keys (ComboItemTrackerWindow.cpp kComboItemTrackerSohOptionalGroups): spell them exactly.
const GatedGroup kGatedGroups[] = {
    GATED("Greg", kGregRows, RSK_NONE),
    GATED("Fishing Pole", kFishingPoleRows, RSK_SHUFFLE_FISHING_POLE),
    GATED("Bean Souls", kBeanSoulRows, RSK_SHUFFLE_BEAN_SOULS),
    GATED("Boss Souls", kBossSoulRows, RSK_SHUFFLE_BOSS_SOULS),
    GATED("Jabber Nuts", kJabberNutRows, RSK_SHUFFLE_SPEAK),
    GATED("Ocarina Buttons", kOcarinaButtonRows, RSK_SHUFFLE_OCARINA_BUTTONS),
    GATED("Overworld Keys", kOverworldKeyRows, RSK_LOCK_OVERWORLD_DOORS),
};

#undef GATED

/** Does the buffer's file show gated group `g`? A randomizer file whose frozen option is on. */
bool OoTItemGroupShown(const SaveContext* src, const OoTItemSeed& seed, const GatedGroup& g) {
    return IS_RANDO && (g.option == RSK_NONE || seed.Option(g.option) != 0);
}

/** DrawItem's held test for a flag row: its randomizer-inf flag, except Ganon's soul, which SoH draws held
 *  unless the file shuffles it (RO_BOSS_SOULS_ON_PLUS_GANON). */
bool OoTItemFlagHeld(const SaveContext* src, const OoTItemSeed& seed, const FlagRow& row) {
    if (row.flag == RAND_INF_GANON_SOUL && seed.Option(RSK_SHUFFLE_BOSS_SOULS) != RO_BOSS_SOULS_ON_PLUS_GANON) {
        return true;
    }
    return RandInfOf(src, row.flag);
}

/** The gated rows this buffer shows. */
int OoTItemGatedRows(const SaveContext* src) {
    if (!IS_RANDO) {
        return 0; // a vanilla file shows none: no seed to read
    }
    const OoTItemSeed seed = OoTItemSeedRead();
    int n = 0;
    for (const GatedGroup& g : kGatedGroups) {
        n += OoTItemGroupShown(src, seed, g) ? g.count : 0;
    }
    return n;
}

/** Gated row `index` (0-based over the groups shown), or false past the last. */
bool OoTItemDeriveGated(const SaveContext* src, int index, ComboItemRow* out) {
    if (!IS_RANDO) {
        return false;
    }
    const OoTItemSeed seed = OoTItemSeedRead();
    for (const GatedGroup& g : kGatedGroups) {
        if (!OoTItemGroupShown(src, seed, g)) {
            continue;
        }
        if (index < g.count) {
            const FlagRow& row = g.rows[index];
            out->group = g.group;
            out->slotKey = row.slotKey;
            out->name = row.name;
            out->label = row.label;
            out->iconKey = row.key;
            out->iconKeyFaded = row.faded;
            out->have = OoTItemFlagHeld(src, seed, row);
            out->sharedFamily = row.family;
            return true;
        }
        index -= g.count;
    }
    return false;
}

/** SoH pads each optional section onto a fresh line of its main window (UpdateVectors); the dungeon tables
 *  follow the songs. */
bool OoTItemGroupOwnLine(const char* group) {
    if (group == nullptr) {
        return false;
    }
    for (const GatedGroup& g : kGatedGroups) {
        if (std::strcmp(g.group, group) == 0) {
            return true;
        }
    }
    return false;
}

// ---- the vtable ----------------------------------------------------------------

int OoTItemCount(void) {
    return kFixedRowCount;
}

int OoTItemCountFor(const void* buf) {
    if (!Aligned(buf)) {
        return kFixedRowCount;
    }
    return kFixedRowCount + OoTItemGatedRows(static_cast<const SaveContext*>(buf));
}

/** A created OoT file is in `buf`: the test the .redsave slot panel registers
 *  (OoT_SaveBlobIsStarted, SaveManager.cpp), never the 'ZELDAZ' newf bytes,
 *  which Ship of Harkinian does not write (#873). */
bool OoTItemHasSave(const void* buf) {
    return Aligned(buf) && OoT_SaveBlobIsStarted(static_cast<const uint8_t*>(buf), sizeof(SaveContext)) != 0;
}

/** The shared family (foreign_items.h) an inventory row's item is OoT's own copy
 *  of, or 0: the view draws an armed family once, in the Shared group (#1065). */
uint32_t OoTItemSharedFamily(const RowDef& def) {
    if (def.kind == RK_INV && def.item == (uint8_t)ITEM_LENS) {
        return RSBS_XITEM_SHARED_LENS;
    }
    if (def.kind == RK_QUEST && def.data == (uint32_t)QUEST_SONG_EPONA) {
        return RSBS_XITEM_SHARED_SONG_EPONA; // #1066
    }
    if (def.kind == RK_QUEST && def.data == (uint32_t)QUEST_SONG_STORMS) {
        return RSBS_XITEM_SHARED_SONG_STORMS; // #1066
    }
    if (def.kind == RK_QUEST && def.data == (uint32_t)QUEST_SONG_TIME) {
        return RSBS_XITEM_SHARED_SONG_TIME; // #1069
    }
    if (def.kind == RK_INV && def.item == (uint8_t)ITEM_ARROW_FIRE) {
        return RSBS_XITEM_SHARED_ARROW_FIRE; // #968
    }
    if (def.kind == RK_INV && def.item == (uint8_t)ITEM_ARROW_ICE) {
        return RSBS_XITEM_SHARED_ARROW_ICE; // #968
    }
    if (def.kind == RK_INV && def.item == (uint8_t)ITEM_ARROW_LIGHT) {
        return RSBS_XITEM_SHARED_ARROW_LIGHT; // #968
    }
    return 0u;
}

bool OoTItemRowAt(const void* buf, int index, ComboItemRow* out) {
    if (!Aligned(buf) || out == nullptr || index < 0) {
        return false;
    }
    const SaveContext* src = static_cast<const SaveContext*>(buf);
    const uint8_t freshness = out->freshness; // the view's field; left alone
    std::memset(out, 0, sizeof(*out));
    out->freshness = freshness;
    if (index < kRowCount) {
        if (!Derive(src, kRows[index], out)) {
            return false;
        }
        out->sharedFamily = OoTItemSharedFamily(kRows[index]);
        return true;
    }
    index -= kRowCount;
    if (index < kDungeonWithMapsCount) {
        DeriveDungeonCell(src, kDungeonWithMaps[index], "Dungeon Items", out);
        return true;
    }
    index -= kDungeonWithMapsCount;
    if (index < kDungeonKeysOnlyCount) {
        DeriveDungeonCell(src, kDungeonKeysOnly[index], "Dungeon Keys", out);
        return true;
    }
    return OoTItemDeriveGated(src, index - kDungeonKeysOnlyCount, out);
}

constexpr int kDungeonCatalogCount = kDungeonWithMapsCount - 1; // 35 non-pad cells in kDungeonWithMaps
constexpr int kGatedCatalogCount =
    (int)(sizeof(kGregRows) / sizeof(kGregRows[0])) + (int)(sizeof(kFishingPoleRows) / sizeof(kFishingPoleRows[0])) +
    (int)(sizeof(kBeanSoulRows) / sizeof(kBeanSoulRows[0])) + (int)(sizeof(kBossSoulRows) / sizeof(kBossSoulRows[0])) +
    (int)(sizeof(kJabberNutRows) / sizeof(kJabberNutRows[0])) +
    (int)(sizeof(kOcarinaButtonRows) / sizeof(kOcarinaButtonRows[0])) +
    (int)(sizeof(kOverworldKeyRows) / sizeof(kOverworldKeyRows[0]));
constexpr int kCatalogRowCount = kRowCount + kDungeonCatalogCount + kGatedCatalogCount;

int OoTItemCatalogCount(void) {
    return kCatalogRowCount;
}

const SaveContext* OoTEmptyCatalogSave(void) {
    static SaveContext sEmptySave{};
    static bool sInitialized = false;
    if (!sInitialized) {
        std::memset(sEmptySave.inventory.items, ITEM_NONE, sizeof(sEmptySave.inventory.items));
        std::memset(sEmptySave.inventory.dungeonKeys, 0xFF, sizeof(sEmptySave.inventory.dungeonKeys));
        sInitialized = true;
    }
    return &sEmptySave;
}

bool OoTItemCatalogRowAt(const void* buf, int catalogIndex, ComboItemRow* out) {
    if (out == nullptr || catalogIndex < 0 || catalogIndex >= kCatalogRowCount) {
        return false;
    }
    const bool hasSave = Aligned(buf) && OoTItemHasSave(buf);
    const SaveContext* src = hasSave ? static_cast<const SaveContext*>(buf) : OoTEmptyCatalogSave();
    const uint8_t freshness = out->freshness;
    std::memset(out, 0, sizeof(*out));
    out->freshness = freshness;

    if (catalogIndex < kRowCount) {
        if (!Derive(src, kRows[catalogIndex], out)) {
            return false;
        }
        out->sharedFamily = OoTItemSharedFamily(kRows[catalogIndex]);
        if (!hasSave) {
            out->have = false;
            out->count = 0;
        }
        return true;
    }
    catalogIndex -= kRowCount;

    if (catalogIndex < kDungeonCatalogCount) {
        // Cell 23 is the single DC_PAD in kDungeonWithMaps.
        const int cellIndex = catalogIndex < 23 ? catalogIndex : catalogIndex + 1;
        DeriveDungeonCell(src, kDungeonWithMaps[cellIndex], "Dungeon Items", out);
        if (!hasSave) {
            out->have = false;
            out->count = 0;
            out->held = 0;
        }
        return true;
    }
    catalogIndex -= kDungeonCatalogCount;

    const OoTItemSeed seed = hasSave ? OoTItemSeedRead() : OoTItemSeed{ nullptr };
    for (const GatedGroup& g : kGatedGroups) {
        if (catalogIndex < g.count) {
            const FlagRow& row = g.rows[catalogIndex];
            out->group = g.group;
            out->slotKey = row.slotKey;
            out->name = row.name;
            out->label = row.label;
            out->iconKey = row.key;
            out->iconKeyFaded = row.faded;
            out->sharedFamily = row.family;
            if (!hasSave) {
                out->have = false;
            } else if (OoTItemGroupShown(src, seed, g)) {
                out->have = OoTItemFlagHeld(src, seed, row);
            } else {
                // When a save is loaded and the shuffle option for a gated group
                // (Boss Souls, Ocarina Buttons, Bean Souls, Jabber Nuts, Fishing
                // Pole, Overworld Keys) is off, the item is unshuffled and held by
                // default (matching 2Ship's !IsItemObtainable behavior).
                out->have = g.option != RSK_NONE;
            }
            return true;
        }
        catalogIndex -= g.count;
    }
    return false;
}

} // namespace

#undef gSaveContext
// ============================================================================
// Below: the real global again (nothing here reads a save).
// ============================================================================

namespace {

/**
 * OoT's own icon for a shared-pool row (#458 U3): the native tracker's texture
 * names (ImGuiUtils.cpp itemMapping, gregMapping's green rupee and
 * customItemsMapping's triforce piece), loaded under these names at OoT's boot.
 * Double defense has no icon in OoT's tracker; it takes the heart container,
 * the icon MM's own Rando table draws for it (GetIconTexturePath). The wallet
 * prints its capacity, as SoH's DrawItemCount does in its default mode
 * (GetItemCurrentAndMax: CUR_CAPACITY(UPG_WALLET)), and is drawn held at every
 * tier, as DrawItem draws it outside wallet shuffle.
 */
void OoTItemSharedIcon(uint8_t kind, uint16_t tier, ComboItemRow* row) {
    const char* key = nullptr;
    const char* faded = nullptr;
#define OOT_SHARED_ICON(name) \
    key = name;               \
    faded = name "_Faded"
    switch (kind) {
        case RSBS_SHARED_RES_RUPEES:
            OOT_SHARED_ICON("ITEM_RUPEE_GREEN");
            break;
        case RSBS_SHARED_RES_WALLET_TIER:
            if (tier >= 2) {
                OOT_SHARED_ICON("ITEM_WALLET_GIANT");
            } else {
                OOT_SHARED_ICON("ITEM_WALLET_ADULT");
            }
            row->iconNumber = (int)CAPACITY(UPG_WALLET, tier > 3 ? 3 : tier);
            break;
        case RSBS_SHARED_RES_HEALTH_QUARTERS:
            if (tier == COMBO_ITEM_SHARED_ICON_HEART_PIECE) {
                OOT_SHARED_ICON("ITEM_HEART_PIECE"); // the pooled pieces toward the next heart (#1159)
            } else {
                OOT_SHARED_ICON("ITEM_HEART_CONTAINER");
            }
            break;
        case RSBS_SHARED_RES_DOUBLE_DEFENSE: // OoT's tracker has no icon for it: MM's Rando shows a heart container
            OOT_SHARED_ICON("ITEM_HEART_CONTAINER");
            break;
        case RSBS_SHARED_RES_MAGIC_LEVEL:
            if (tier >= 2) {
                OOT_SHARED_ICON("ITEM_MAGIC_LARGE");
            } else {
                OOT_SHARED_ICON("ITEM_MAGIC_SMALL");
            }
            break;
        case RSBS_SHARED_RES_ARROW_COUNT:
            // The quiver tiers as the player knows them (#967): the Bow, then
            // the Big and Biggest Quivers.
            if (tier >= 3) {
                OOT_SHARED_ICON("ITEM_QUIVER_50");
            } else if (tier == 2) {
                OOT_SHARED_ICON("ITEM_QUIVER_40");
            } else {
                OOT_SHARED_ICON("ITEM_BOW");
            }
            break;
        case RSBS_SHARED_RES_BOMB_COUNT:
            if (tier >= 3) {
                OOT_SHARED_ICON("ITEM_BOMB_BAG_40");
            } else if (tier == 2) {
                OOT_SHARED_ICON("ITEM_BOMB_BAG_30");
            } else {
                OOT_SHARED_ICON("ITEM_BOMB_BAG_20");
            }
            break;
        case RSBS_SHARED_RES_BOMBCHU_COUNT:
            OOT_SHARED_ICON("ITEM_BOMBCHU");
            break;
        case RSBS_SHARED_RES_STICK_COUNT:
            OOT_SHARED_ICON("ITEM_STICK");
            break;
        case RSBS_SHARED_RES_NUT_COUNT:
            OOT_SHARED_ICON("ITEM_NUT");
            break;
        case RSBS_SHARED_RES_HOOKSHOT_TIER:
            if (tier >= 2) {
                OOT_SHARED_ICON("ITEM_LONGSHOT");
            } else {
                OOT_SHARED_ICON("ITEM_HOOKSHOT");
            }
            break;
        case RSBS_SHARED_RES_OCARINA_TIER:
            if (tier >= 2) {
                OOT_SHARED_ICON("ITEM_OCARINA_TIME");
            } else {
                OOT_SHARED_ICON("ITEM_OCARINA_FAIRY");
            }
            break;
        case RSBS_SHARED_RES_KOKIRI_SWORD:
            OOT_SHARED_ICON("ITEM_SWORD_KOKIRI");
            break;
        case RSBS_SHARED_RES_TRIFORCE_PIECES:
            OOT_SHARED_ICON("RG_TRIFORCE_PIECE");
            break;
        case RSBS_SHARED_RES_BOTTLE_COUNT: // #970
            OOT_SHARED_ICON("ITEM_BOTTLE");
            break;
        default:
            break;
    }
#undef OOT_SHARED_ICON
    row->iconKey = key;
    row->iconKeyFaded = faded;
    row->iconAspect = 0.0f;
    row->fadedAlpha = 0.0f; // SoH's faded textures carry their own alpha
    row->iconTint = 0;
}

// The live view reads the HANDED buffer (the view's liveSave, which production points at the process
// global and the ROM-free test points at an authored one), so the accessor macros are redirected again.
#define gSaveContext (*src)

// Heart pieces occupy the TOP NIBBLE of questItems (the pool's canonical health quantity,
// OoT_ReadHealthQuarters in GameExports_SingleExe.cpp).
constexpr uint32_t kOoTHeartPieceShift = 28u;
constexpr uint32_t kOoTHeartPieceMask = 0xF0000000u;

/**
 * The Shared column's LIVE view (#971): the value of shared kind `kind` in the save
 * `src`, over the harvest's own arithmetic (OoT_HarvestSharedResources in
 * GameExports_SingleExe.cpp: the wallet tier, the health quarters, the double
 * defense flag, the magic level from its acquired flags, the ammo slots and the
 * hookshot slot), but READ ONLY: no settle of the rupee accumulator, no pool or
 * watermark write, nothing stored in `src`. Gated like the harvest, on a played
 * file (Combo_SaveIsLiveFile over the buffer's own game mode), so the title
 * screen's attract save and file select read nothing. False for a kind this view
 * has no reading of.
 */
bool OoTItemSharedLive(const void* srcv, uint8_t kind, uint16_t* out) {
    if (out == nullptr || !Aligned(srcv)) {
        return false;
    }
    const SaveContext* src = static_cast<const SaveContext*>(srcv);
    if (!Combo_SaveIsLiveFile(GAME_OOT, (int32_t)gSaveContext.gameMode)) {
        return false;
    }
    const auto ammoOf = [src](uint8_t item) -> uint16_t {
        const s8 held = AMMO(item);
        return held < 0 ? 0u : (uint16_t)held;
    };
    switch (kind) {
        case RSBS_SHARED_RES_RUPEES:
            // The HUD's count: a pending rupeeAccumulator (settled by the harvest) lags a few frames.
            *out = gSaveContext.rupees < 0 ? 0u : (uint16_t)gSaveContext.rupees;
            return true;
        case RSBS_SHARED_RES_WALLET_TIER:
            *out = (uint16_t)CUR_UPG_VALUE(UPG_WALLET);
            return true;
        case RSBS_SHARED_RES_HEALTH_QUARTERS: {
            const uint16_t pieces =
                (uint16_t)((gSaveContext.inventory.questItems & kOoTHeartPieceMask) >> kOoTHeartPieceShift);
            const uint16_t capacity = gSaveContext.healthCapacity < 0 ? 0u : (uint16_t)gSaveContext.healthCapacity;
            *out = Combo_MakeHealthQuarters(capacity, pieces);
            return true;
        }
        case RSBS_SHARED_RES_DOUBLE_DEFENSE:
            *out = gSaveContext.isDoubleDefenseAcquired ? 1u : 0u;
            return true;
        case RSBS_SHARED_RES_MAGIC_LEVEL:
            *out = !gSaveContext.isMagicAcquired ? 0u : (gSaveContext.isDoubleMagicAcquired ? 2u : 1u);
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
            *out = ammoOf(ITEM_STICK);
            return true;
        case RSBS_SHARED_RES_NUT_COUNT:
            *out = ammoOf(ITEM_NUT);
            return true;
        case RSBS_SHARED_RES_HOOKSHOT_TIER: {
            const u8 held = InvContentOf(src, ITEM_HOOKSHOT);
            *out = (held == ITEM_LONGSHOT) ? 2u : (held == ITEM_HOOKSHOT ? 1u : 0u);
            return true;
        }
        default:
            return false;
    }
}

#undef gSaveContext

} // namespace

extern "C" void OoT_ItemAdapter_Register(void) {
    // SoH's main window grid (kComboItemSohGrid's metrics): the 66 rows flow six
    // a line, 11 lines, exactly DrawItemsInRows(mainWindowItems, 6), and each of
    // SoH's optional sections starts a line of its own (#1142).
    static const ComboItemGridStyle kOoTItemGrid = {
        kComboItemSohGrid.cellPx,       kComboItemSohGrid.gapPx,
        kComboItemSohGrid.columns,      kComboItemSohGrid.groupLines,
        kComboItemSohGrid.groupColumns, kComboItemSohGrid.countStyle,
        kComboItemSohGrid.lineGapPx,    OoTItemGroupOwnLine,
    };
    static const ComboItemOps kOps = {
        OoTItemCount,      OoTItemRowAt,    OoTItemHasSave,    OoTItemLiveSave,     OoTItemPaused,       &kOoTItemGrid,
        OoTItemSharedIcon, OoTItemCountFor, OoTItemSharedLive, OoTItemCatalogCount, OoTItemCatalogRowAt,
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
 * world: the Hookshot and the Song of Time only. Variant 2, the "capacity
 * tier" world (#967): the Fairy Bow (tier-1 quiver, 12 arrows), bombs (tier-2
 * bag, 7), Deku Sticks (tier 1, 5), Deku Nuts (tier 3, 9) and the Kokiri Sword.
 * Returns 1, or 0 when `size` cannot hold a SaveContext or `buf` is misaligned.
 */
extern "C" int OoT_ItemAdapter_TestAuthorSave(void* buf, size_t size, int variant) {
    if (buf == nullptr || size < sizeof(SaveContext) || !Aligned(buf)) {
        return 0;
    }
    std::memset(buf, 0, size);
    SaveContext* s = static_cast<SaveContext*>(buf);
    // Started the way a created file is (#873): the new-file path's three
    // hearts, and no 'ZELDAZ' newf bytes, which Ship of Harkinian never writes.
    s->healthCapacity = 0x30;
    s->health = 0x30;
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
    } else if (variant == 2) {
        // The capacity-tier world (#967): the Fairy Bow with a tier-1 quiver
        // holding 12 arrows, bombs with a tier-2 bag holding 7, 5 Deku Sticks
        // in a tier-1 bag, 9 Deku Nuts in a tier-3 bag, and the Kokiri Sword.
        s->inventory.items[SLOT_BOW] = ITEM_BOW;
        s->inventory.ammo[SLOT_BOW] = 12;
        s->inventory.upgrades |= (1u << OoT_gUpgradeShifts[UPG_QUIVER]);
        s->inventory.items[SLOT_BOMB] = ITEM_BOMB;
        s->inventory.ammo[SLOT_BOMB] = 7;
        s->inventory.upgrades |= (2u << OoT_gUpgradeShifts[UPG_BOMB_BAG]);
        s->inventory.items[SLOT_STICK] = ITEM_STICK;
        s->inventory.ammo[SLOT_STICK] = 5;
        s->inventory.upgrades |= (1u << OoT_gUpgradeShifts[UPG_STICKS]);
        s->inventory.items[SLOT_NUT] = ITEM_NUT;
        s->inventory.ammo[SLOT_NUT] = 9;
        s->inventory.upgrades |= (3u << OoT_gUpgradeShifts[UPG_NUTS]);
        s->inventory.equipment |= (1 << 0); // Kokiri Sword
    } else {
        s->inventory.items[SLOT_HOOKSHOT] = ITEM_HOOKSHOT;
        s->inventory.questItems |= (1u << QUEST_SONG_TIME);
    }
    return 1;
}

/**
 * The tracker world of #1142/#1158 (the OoT groups SoH's main window shows
 * beyond its default sections): written into `buf` (zeroed first) as a started
 * save, a randomizer file when `rando` is 1, a vanilla one otherwise. Small keys
 * per dungeon start at -1 (none), as a new file's do (SaveManager InitFileNormal).
 *   - Forest Temple: 2 keys in hand of 3 collected, its boss key and map;
 *   - Fire Temple: 0 in hand of 1 collected (a key used);
 *   - Thieves' Hideout: 1 in hand of 2 collected;
 *   - the Deku Tree's compass, Ganon's Tower's boss key;
 *   - the Longshot in the hookshot slot (the slot guard's in-range read);
 *   - the randomizer-inf flags of Gohma's and Twinrova's souls, the Kokiri Forest
 *     bean soul, the Zora jabber nut, the A and C-Left ocarina buttons, the Guard
 *     House and Fishing Hole keys, the fishing pole and Greg (not Ganon's soul).
 * Returns 1, or 0 when `size` cannot hold a SaveContext or `buf` is misaligned.
 */
extern "C" int OoT_ItemAdapter_TestAuthorTrackerWorld(void* buf, size_t size, int rando) {
    if (buf == nullptr || size < sizeof(SaveContext) || !Aligned(buf)) {
        return 0;
    }
    std::memset(buf, 0, size);
    SaveContext* s = static_cast<SaveContext*>(buf);
    s->healthCapacity = 0x30;
    s->health = 0x30;
    std::memset(s->inventory.items, ITEM_NONE, sizeof(s->inventory.items));
    std::memset(s->inventory.dungeonKeys, 0xFF, sizeof(s->inventory.dungeonKeys));
    s->ship.quest.id = rando ? QUEST_RANDOMIZER : QUEST_NORMAL;
    s->inventory.items[SLOT_HOOKSHOT] = ITEM_LONGSHOT;
    // Bit (item - ITEM_KEY_BOSS) of dungeonItems: the boss key, the compass, the map (DrawDungeonItem).
    const auto bitOf = [](int item) { return (uint8_t)(1u << (item - ITEM_KEY_BOSS)); };
    s->inventory.dungeonItems[SCENE_FOREST_TEMPLE] = bitOf(ITEM_KEY_BOSS) | bitOf(ITEM_DUNGEON_MAP);
    s->inventory.dungeonItems[SCENE_DEKU_TREE] = bitOf(ITEM_COMPASS);
    s->inventory.dungeonItems[SCENE_GANONS_TOWER] = bitOf(ITEM_KEY_BOSS);
    s->inventory.dungeonKeys[SCENE_FOREST_TEMPLE] = 2;
    s->ship.stats.dungeonKeys[SCENE_FOREST_TEMPLE] = 3;
    s->inventory.dungeonKeys[SCENE_FIRE_TEMPLE] = 0;
    s->ship.stats.dungeonKeys[SCENE_FIRE_TEMPLE] = 1;
    s->inventory.dungeonKeys[SCENE_THIEVES_HIDEOUT] = 1;
    s->ship.stats.dungeonKeys[SCENE_THIEVES_HIDEOUT] = 2;
    for (RandomizerInf flag :
         { RAND_INF_GOHMA_SOUL, RAND_INF_TWINROVA_SOUL, RAND_INF_KOKIRI_FOREST_BEAN_SOUL, RAND_INF_CAN_SPEAK_ZORA,
           RAND_INF_HAS_OCARINA_A, RAND_INF_HAS_OCARINA_C_LEFT, RAND_INF_GUARD_HOUSE_KEY_OBTAINED,
           RAND_INF_FISHING_HOLE_KEY_OBTAINED, RAND_INF_FISHING_POLE_FOUND, RAND_INF_GREG_FOUND }) {
        s->ship.randomizerInf[flag >> 4] |= (u16)(1u << (flag & 0xF));
    }
    return 1;
}

/**
 * The frozen randomizer options the OoT tracker groups are gated by, overridden
 * for the ROM-free lock (#1142): OoT's options live in the loaded seed's context,
 * never in the save buffer. `enabled` 0 puts the production read back. `bits`:
 * 0 RSK_SHUFFLE_BEAN_SOULS, 1 RSK_SHUFFLE_BOSS_SOULS on, 2 RSK_SHUFFLE_BOSS_SOULS
 * on plus Ganon (with bit 1), 3 RSK_SHUFFLE_SPEAK, 4 RSK_SHUFFLE_OCARINA_BUTTONS,
 * 5 RSK_LOCK_OVERWORLD_DOORS, 6 RSK_SHUFFLE_FISHING_POLE, 7-8 RSK_GERUDO_FORTRESS
 * (RO_GF_CARPENTERS_*), and from bit 16 the seed's Master Quest dungeons, bit
 * (16 + scene) per dungeon scene (the small-key maxes, #1263). Every read of the
 * seed goes through it while `enabled`, so the context is never consulted then.
 */
extern "C" void OoT_ItemAdapter_TestOverrideOptions(int enabled, unsigned bits) {
    sOoTItemTestOptionsOn = enabled != 0;
    sOoTItemTestOptions = bits;
}

/** The adapter's read of item `item`'s inventory slot over `buf`, or -1 when the
 *  adapter has no guarded read (SoH 7a7f942b8: an item id past gItemSlots names
 *  no slot, and INV_CONTENT would index past the table). */
extern "C" int OoT_ItemAdapter_TestInvContent(const void* buf, int item) {
    if (!Aligned(buf)) {
        return -1;
    }
    return (int)InvContentOf(static_cast<const SaveContext*>(buf), item);
}

/** OoT's gItemSlots length: an item id at or past it has no inventory slot. */
extern "C" int OoT_ItemAdapter_TestItemSlotCount(void) {
    return (int)ARRAY_COUNT(OoT_gItemSlots);
}

/**
 * Write into `buf` (zeroed first) a save Ship of Harkinian's own init path
 * authors, over a zeroed gSaveContext so nothing but that path's writes (and
 * the two fields its caller sets first) is in it (#873). The live gSaveContext
 * is put back afterwards. `variant`:
 *   0  the player's new file: Save_InitFile(false), which OoT_Sram_InitNewSave
 *      runs, after the file select's fileNum (file 1) and its game mode;
 *   1  the title screen's attract save: Opening_SetupTitleScreen's fileNum
 *      0xFF and GAMEMODE_TITLE_SCREEN, then Save_InitFile(true)
 *      (OoT_Sram_InitDebugSave);
 *   2  the same debug save in a played session (fileNum 0xFF, GAMEMODE_NORMAL),
 *      as the integration drives play it.
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

extern "C" int OoT_ItemAdapter_TestAuthorNewFile(void* buf, size_t size, int variant, int* outNewfMarked) {
    if (buf == nullptr || size < sizeof(SaveContext) || !Aligned(buf) || variant < 0 || variant > 2 ||
        SaveManager::Instance == nullptr || Ship::Context::GetInstance() == nullptr ||
        Ship::Context::GetInstance()->GetResourceManager() == nullptr) {
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
    if (variant == 0) {
        gSaveContext.fileNum = 0;
        gSaveContext.gameMode = GAMEMODE_FILE_SELECT;
    } else {
        gSaveContext.fileNum = 0xFF;
        gSaveContext.gameMode = (variant == 1) ? GAMEMODE_TITLE_SCREEN : GAMEMODE_NORMAL;
    }
    Save_InitFile(variant == 0 ? 0 : 1);
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
