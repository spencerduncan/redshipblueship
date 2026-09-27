/**
 * @file ForeignTextboxIconOoT.cpp
 * @brief OoT's answer to "which icon does this RG_* show in another game's
 *        get-item textbox?" (#607, Tier 2b of #494).
 *
 * Registered into src/common/foreign_textbox_icon.h at static-init time, the
 * same shape as this directory's describer registrar. MM calls it (through
 * Combo_GetForeignItemTextboxIcon) when the player picks up an OoT item on an MM
 * check, and draws the answer inside MM's own blue get-item textbox.
 *
 * WHICH ICON. The icon OoT itself would show for the item: the ItemID row of
 * OoT_gItemIcons that the item's GetItemEntry names (the table OoT's pause menu
 * and its own get-item textbox draw from). Three things need more than that
 * lookup, and each rule below says which OoT convention it follows:
 *
 *  - PROGRESSIVE rows have no static GetItemEntry (Item::GetGIEntry resolves
 *    them against the live inventory, which belongs to a suspended game while
 *    MM runs). They show their FIRST tier (Progressive Hookshot -> Hookshot),
 *    the tier the rando item tracker also draws for an unowned progressive.
 *  - "Give" ItemIDs past the icon table (ITEM_BOMBCHUS_5, ITEM_STICK_UPGRADE_20,
 *    ITEM_HEART_PIECE_2, ...) fold to the inventory item they fill, as OoT's own
 *    item-get code does before it indexes the icon table.
 *  - MOD_RANDOMIZER rows carry an RG_* where the ItemID would be. Dungeon keys,
 *    maps and compasses show OoT's quest icons; boss souls, the Triforce piece
 *    and Roc's Feather show the soh.o2r textures SoH's own tracker uses
 *    (ImGuiUtils.cpp customItemsMapping); the jabber nuts show the Deku Nut, as
 *    SoH's jabbernutMapping does; bean souls show the Magic Bean; the ocarina
 *    buttons show the Fairy Ocarina; overworld keys and the skeleton key show the
 *    small key; Greg shows the rupee counter icon in SoH's Greg green. The
 *    action-shuffle abilities (Climb, Crawl, Open Chests) have no OoT icon and
 *    keep the icon-less textbox.
 *
 * WHAT IT NEVER DOES: read the save, the rando Context or any live state. The
 * answer is a pure function of static tables, so it is the same while OoT is
 * suspended, and it is headless-testable (ForeignTextboxIcon row).
 *
 * SHAPES. OoT's icons come in exactly the layouts foreign_textbox_icon.h names,
 * per the extraction XMLs (assets/xml/.../textures/icon_item_static.xml and
 * icon_item_24_static.xml): ItemIDs 0x00-0x59 are RGBA32 32x32, the twelve songs
 * are the IA8 16x24 note, and 0x66-0x79 are RGBA32 24x24 quest icons. The three
 * soh.o2r textures are RGBA32 32x32 (assets/custom/textures/*.rgba32.png).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cmath>
#include <cstdint>
#include <cstring>

#include "soh/OTRGlobals.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/SohGui/ImGuiUtils.h" // vanillaSongMapping: OoT's own song-note tints

#include "assets/soh_assets.h" // gBossSoulTex, gTriforcePieceTex, gRocsFeatherTex

#include "context.h"              // src/common — GameId
#include "foreign_textbox_icon.h" // src/common — the registry this file feeds

extern "C" {
#include <z64.h>
#include "variables.h" // OoT_gItemIcons
}

namespace {

// OoT's rupee counter icon (parameter_static, IA8 16x16). Spelled out rather than
// pulled from the parameter_static asset header, which defines ~100 statics this
// TU has no use for; the path is the archive's, not a copy of the texture.
constexpr const char* kOoTRupeeCounterIconTex = "__OTR__textures/parameter_static/gRupeeCounterIconTex";

/** A progressive row's first tier: the RG whose static entry names its icon. */
RandomizerGet FirstTier(RandomizerGet rg) {
    switch (rg) {
        case RG_PROGRESSIVE_STICK_UPGRADE:
            return RG_DEKU_STICK_CAPACITY_20;
        case RG_PROGRESSIVE_NUT_UPGRADE:
            return RG_DEKU_NUT_CAPACITY_30;
        case RG_PROGRESSIVE_BOMB_BAG:
            return RG_BOMB_BAG;
        case RG_PROGRESSIVE_BOW:
            return RG_FAIRY_BOW;
        case RG_PROGRESSIVE_SLINGSHOT:
            return RG_FAIRY_SLINGSHOT;
        case RG_PROGRESSIVE_OCARINA:
            return RG_FAIRY_OCARINA;
        case RG_PROGRESSIVE_HOOKSHOT:
            return RG_HOOKSHOT;
        case RG_PROGRESSIVE_STRENGTH:
            return RG_GORONS_BRACELET;
        case RG_PROGRESSIVE_WALLET:
            return RG_ADULT_WALLET;
        case RG_PROGRESSIVE_SCALE:
            return RG_SILVER_SCALE;
        case RG_PROGRESSIVE_MAGIC_METER:
            return RG_MAGIC_SINGLE;
        case RG_PROGRESSIVE_GORONSWORD:
            return RG_BIGGORON_SWORD;
        case RG_PROGRESSIVE_BOMBCHU_BAG:
            return RG_BOMBCHU_10;
        default:
            return rg;
    }
}

/** A "give" ItemID past the icon table, folded to the inventory item it fills;
 *  any other id unchanged. */
uint16_t FoldGiveItemId(uint16_t itemId) {
    switch (itemId) {
        case ITEM_HEART_PIECE_2:
            return ITEM_HEART_PIECE;
        case ITEM_SINGLE_MAGIC:
            return ITEM_MAGIC_SMALL;
        case ITEM_DOUBLE_MAGIC:
            return ITEM_MAGIC_LARGE;
        case ITEM_DOUBLE_DEFENSE:
            return ITEM_HEART_CONTAINER;
        case ITEM_MILK:
            return ITEM_MILK_BOTTLE;
        case ITEM_STICKS_5:
        case ITEM_STICKS_10:
        case ITEM_STICK_UPGRADE_20:
        case ITEM_STICK_UPGRADE_30:
            return ITEM_STICK;
        case ITEM_NUTS_5:
        case ITEM_NUTS_10:
        case ITEM_NUT_UPGRADE_30:
        case ITEM_NUT_UPGRADE_40:
            return ITEM_NUT;
        case ITEM_BOMBS_5:
        case ITEM_BOMBS_10:
        case ITEM_BOMBS_20:
        case ITEM_BOMBS_30:
            return ITEM_BOMB;
        case ITEM_ARROWS_SMALL:
        case ITEM_ARROWS_MEDIUM:
        case ITEM_ARROWS_LARGE:
            return ITEM_BOW;
        case ITEM_SEEDS_30:
            return ITEM_SEEDS;
        case ITEM_BOMBCHUS_5:
        case ITEM_BOMBCHUS_20:
            return ITEM_BOMBCHU;
        default:
            return itemId;
    }
}

/** The icon of an inventory ItemID, by OoT_gItemIcons row and the layout band it
 *  sits in. 0 for an id with no icon row. */
int IconForItemId(uint16_t rawItemId, ComboTextboxIcon* out) {
    const uint16_t itemId = FoldGiveItemId(rawItemId);
    ComboTextboxIcon icon = { nullptr, (uint8_t)COMBO_TEXTBOX_ICON_NONE, 255, 255, 255 };
    if (itemId <= ITEM_FISHING_POLE) {
        icon.shape = (uint8_t)COMBO_TEXTBOX_ICON_ITEM;
    } else if (itemId >= ITEM_SONG_MINUET && itemId <= ITEM_SONG_STORMS) {
        icon.shape = (uint8_t)COMBO_TEXTBOX_ICON_NOTE;
        // OoT's own tint for this song's note: the vanilla pause-menu colours
        // (warp songs coloured, the rest white). QuestItem and ItemID list the
        // twelve songs in the same order.
        const uint32_t quest = (uint32_t)QUEST_SONG_MINUET + (uint32_t)(itemId - ITEM_SONG_MINUET);
        for (const SongMapEntry& song : vanillaSongMapping) {
            if (song.id == quest) {
                icon.r = (uint8_t)std::lround(song.color.x * 255.0f);
                icon.g = (uint8_t)std::lround(song.color.y * 255.0f);
                icon.b = (uint8_t)std::lround(song.color.z * 255.0f);
                break;
            }
        }
    } else if (itemId >= ITEM_MEDALLION_FOREST && itemId <= ITEM_MAGIC_LARGE) {
        icon.shape = (uint8_t)COMBO_TEXTBOX_ICON_QUEST;
    } else {
        return 0;
    }
    icon.texture = static_cast<const char*>(OoT_gItemIcons[itemId]);
    if (icon.texture == nullptr || icon.texture[0] == '\0') {
        return 0;
    }
    *out = icon;
    return 1;
}

int IconForTexture(const char* texture, ComboTextboxIconShape shape, ComboTextboxIcon* out) {
    *out = { texture, (uint8_t)shape, 255, 255, 255 };
    return 1;
}

bool IsBossSoul(RandomizerGet rg) {
    return rg >= RG_GOHMA_SOUL && rg <= RG_GANON_SOUL;
}

bool IsBeanSoul(RandomizerGet rg) {
    return rg >= RG_DEATH_MOUNTAIN_CRATER_BEAN_SOUL && rg <= RG_ZORAS_RIVER_BEAN_SOUL;
}

bool IsOverworldKey(RandomizerGet rg) {
    return rg >= RG_GUARD_HOUSE_KEY && rg <= RG_FISHING_HOLE_KEY;
}

bool IsOcarinaButton(RandomizerGet rg) {
    return rg >= RG_OCARINA_A_BUTTON && rg <= RG_OCARINA_C_RIGHT_BUTTON;
}

bool IsJabberNut(RandomizerGet rg) {
    return rg >= RG_SPEAK_DEKU && rg <= RG_SPEAK_ZORA;
}

} // namespace

/**
 * THE SOURCE. C linkage so the test row can prove the registered pointer is this
 * function (a registrar the linker dropped would leave OoT unregistered).
 */
extern "C" int OoT_ComboTextboxIcon(uint16_t id, ComboTextboxIcon* out) {
    ComboTextboxIcon none = { nullptr, (uint8_t)COMBO_TEXTBOX_ICON_NONE, 0, 0, 0 };
    if (out != nullptr) {
        *out = none;
    }
    if (out == nullptr || id == (uint16_t)RG_NONE || id >= (uint16_t)RG_MAX) {
        return 0;
    }
    const RandomizerGet rg = FirstTier((RandomizerGet)id);

    // The randomizer's own items, whose GetItemEntry carries an RG_* instead of
    // an ItemID. Explicit first, so a table row can never be read as an ItemID.
    if (IsBossSoul(rg)) {
        return IconForTexture(gBossSoulTex, COMBO_TEXTBOX_ICON_ITEM, out);
    }
    switch (rg) {
        case RG_TRIFORCE_PIECE:
            return IconForTexture(gTriforcePieceTex, COMBO_TEXTBOX_ICON_ITEM, out);
        case RG_ROCS_FEATHER:
            return IconForTexture(gRocsFeatherTex, COMBO_TEXTBOX_ICON_ITEM, out);
        case RG_GREG_RUPEE:
            // SoH's gregMapping: the rupee counter icon in Greg green.
            *out = { kOoTRupeeCounterIconTex, (uint8_t)COMBO_TEXTBOX_ICON_RUPEE, 42, 169, 40 };
            return 1;
        case RG_MAGIC_SINGLE:
            return IconForItemId(ITEM_MAGIC_SMALL, out);
        case RG_MAGIC_DOUBLE:
        case RG_MAGIC_INF:
            return IconForItemId(ITEM_MAGIC_LARGE, out);
        case RG_DOUBLE_DEFENSE:
            return IconForItemId(ITEM_HEART_CONTAINER, out);
        case RG_FISHING_POLE:
            return IconForItemId(ITEM_FISHING_POLE, out);
        case RG_BRONZE_SCALE:
            return IconForItemId(ITEM_SCALE_SILVER, out);
        case RG_CHILD_WALLET:
            return IconForItemId(ITEM_WALLET_ADULT, out);
        case RG_TYCOON_WALLET:
        case RG_WALLET_INF:
            return IconForItemId(ITEM_WALLET_GIANT, out);
        case RG_POWER_BRACELET:
            return IconForItemId(ITEM_BRACELET, out);
        case RG_SKELETON_KEY:
            return IconForItemId(ITEM_KEY_SMALL, out);
        case RG_DEKU_STICK_BAG:
        case RG_STICK_UPGRADE_INF:
            return IconForItemId(ITEM_STICK, out);
        case RG_DEKU_NUT_BAG:
        case RG_NUT_UPGRADE_INF:
            return IconForItemId(ITEM_NUT, out);
        case RG_QUIVER_INF:
            return IconForItemId(ITEM_QUIVER_50, out);
        case RG_BOMB_BAG_INF:
            return IconForItemId(ITEM_BOMB_BAG_40, out);
        case RG_BULLET_BAG_INF:
            return IconForItemId(ITEM_BULLET_BAG_50, out);
        case RG_BOMBCHU_INF:
            return IconForItemId(ITEM_BOMBCHU, out);
        // MOD_RANDOMIZER rows whose entry carries an RG_* (or, for the Master
        // Sword, an ItemID the modIndex says not to trust): the item they give.
        case RG_MASTER_SWORD:
            return IconForItemId(ITEM_SWORD_MASTER, out);
        case RG_MAGIC_BEAN_PACK:
            return IconForItemId(ITEM_BEAN, out);
        case RG_BOTTLE_WITH_RED_POTION:
            return IconForItemId(ITEM_POTION_RED, out);
        case RG_BOTTLE_WITH_GREEN_POTION:
            return IconForItemId(ITEM_POTION_GREEN, out);
        case RG_BOTTLE_WITH_BLUE_POTION:
            return IconForItemId(ITEM_POTION_BLUE, out);
        case RG_BOTTLE_WITH_FAIRY:
            return IconForItemId(ITEM_FAIRY, out);
        case RG_BOTTLE_WITH_FISH:
            return IconForItemId(ITEM_FISH, out);
        case RG_BOTTLE_WITH_BLUE_FIRE:
            return IconForItemId(ITEM_BLUE_FIRE, out);
        case RG_BOTTLE_WITH_BUGS:
            return IconForItemId(ITEM_BUG, out);
        case RG_BOTTLE_WITH_POE:
            return IconForItemId(ITEM_POE, out);
        case RG_BOTTLE_WITH_BIG_POE:
            return IconForItemId(ITEM_BIG_POE, out);
        case RG_BOTTLE_WITH_MILK:
            return IconForItemId(ITEM_MILK_BOTTLE, out);
        case RG_KEATON_MASK:
            return IconForItemId(ITEM_MASK_KEATON, out);
        case RG_SKULL_MASK:
            return IconForItemId(ITEM_MASK_SKULL, out);
        case RG_SPOOKY_MASK:
            return IconForItemId(ITEM_MASK_SPOOKY, out);
        case RG_BUNNY_HOOD:
            return IconForItemId(ITEM_MASK_BUNNY, out);
        case RG_GORON_MASK:
            return IconForItemId(ITEM_MASK_GORON, out);
        case RG_ZORA_MASK:
            return IconForItemId(ITEM_MASK_ZORA, out);
        case RG_GERUDO_MASK:
            return IconForItemId(ITEM_MASK_GERUDO, out);
        case RG_MASK_OF_TRUTH:
            return IconForItemId(ITEM_MASK_TRUTH, out);
        // The action-shuffle abilities have no OoT icon at all (SoH's tracker
        // draws them over a blank button background): the icon-less textbox.
        case RG_CLIMB:
        case RG_CRAWL:
        case RG_OPEN_CHEST:
            return 0;
        default:
            break;
    }
    if (IsBeanSoul(rg)) {
        return IconForItemId(ITEM_BEAN, out);
    }
    if (IsOverworldKey(rg)) {
        return IconForItemId(ITEM_KEY_SMALL, out);
    }
    if (IsOcarinaButton(rg)) {
        return IconForItemId(ITEM_OCARINA_FAIRY, out);
    }
    if (IsJabberNut(rg)) {
        return IconForItemId(ITEM_NUT, out);
    }

    Rando::Item& item = Rando::StaticData::RetrieveItem(rg);
    switch (item.GetItemType()) {
        case ITEMTYPE_SMALLKEY:
        case ITEMTYPE_FORTRESS_SMALLKEY:
            return IconForItemId(ITEM_KEY_SMALL, out);
        case ITEMTYPE_BOSSKEY:
            return IconForItemId(ITEM_KEY_BOSS, out);
        case ITEMTYPE_MAP:
            return IconForItemId(ITEM_DUNGEON_MAP, out);
        case ITEMTYPE_COMPASS:
            return IconForItemId(ITEM_COMPASS, out);
        case ITEMTYPE_TOKEN:
            return IconForItemId(ITEM_SKULL_TOKEN, out);
        default:
            break;
    }

    const GetItemEntry* entry = item.GetStaticGIEntry();
    if (entry == nullptr || entry->modIndex != MOD_NONE) {
        return 0;
    }
    return IconForItemId(entry->itemId, out);
}

/** TEST BRIDGE: the id-space the ForeignTextboxIcon row walks (RG_MAX). */
extern "C" int OoT_ComboTextboxIcon_TestIdSpace(void) {
    return (int)RG_MAX;
}

namespace {
struct OoTTextboxIconRegistrar {
    OoTTextboxIconRegistrar() {
        Combo_RegisterTextboxIconSource((uint8_t)GAME_OOT, OoT_ComboTextboxIcon);
    }
};
const OoTTextboxIconRegistrar gOoTTextboxIconRegistrar;
} // namespace

#endif // RSBS_SINGLE_EXECUTABLE
