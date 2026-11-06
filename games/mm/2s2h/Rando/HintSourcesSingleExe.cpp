/**
 * @file HintSourcesSingleExe.cpp
 * @brief Majora's Mask's hint sources for the combined Hint Tracker (#1475,
 *        lane H1): the fixed-location hints' texts, the resolver over a
 *        SaveContext image, the read hook, and the MM adapter of
 *        src/common's combo_hint_view.h. See Rando/HintSources.h.
 *
 * ONE IMPLEMENTATION. Each fixed-location hint's textbox hook (EnTalk, DmStk,
 * EnZow, EnKgy, EnSsh, EnElfgrp, EnGirlA) builds its message with FixedText
 * below over MM's live save, and the combined tracker's resolver calls the same
 * FixedText over whichever MM save it reads (the live one while MM is played,
 * the frozen MM half otherwise). The texts are upstream's, moved here verbatim
 * (each hook keeps upstream's own expression under its #else); the item-location
 * hints fill their location through Rando::HintArea::ReplaceItemLocation's
 * table-taking form, which the live form now calls too. One name differs on
 * purpose: MM's own junk padding in the Bomb Shop's line, which the textbox
 * suffixes with a junk item drawn from MM's global stream and the resolver does
 * not (HintSources.h, JunkName).
 *
 * WHERE A SOURCE IS LISTED (the pane groups by it). A gossip stone: its scene's
 * area (Rando::HintArea::SceneArea; the gossip grottos are Termina Field's). A
 * fixed hint: where its speaker is: the posters under South Clock Town (they hang
 * around Clock Town), the Skull Kid on the Clock Tower rooftop, the Zora in Zora
 * Hall, the smithy in Mountain Village, and a hint about one check (a spider
 * house's, a Great Fairy's, the Bomb Shop's) where that check is.
 *
 * ADR 0002: MM's ids are read only here and in EnGs.cpp; what crosses into
 * src/common is the game-tagged ComboHintResolution.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/ShipUtils.h"
#include "Rando/ActorBehavior/ActorBehavior.h" // MarkCheckSeen, ShopOfferedItemName
#include "Rando/HintArea.h"
#include "Rando/HintSources.h"
#include "Rando/Rando.h"
#include "Rando/StaticData/StaticData.h"

#include "combo_area.h"      // src/common: the shared vocabulary
#include "combo_hint_view.h" // src/common: the view model this TU feeds
#include "foreign_items.h"   // src/common: Combo_GetForeignPlacementForCheck
#include "game.h"            // src/common: GAME_MM / GAME_OOT

namespace {

/** A fixed-location source's facts. */
struct MmFixedSource {
    uint16_t source;
    const char* name;
    RandoItemId item;       // the item whose location it names, or RI_UNKNOWN
    RandoCheckId checks[2]; // the check(s) whose item it names, or RC_UNKNOWN
    RandoOptionId option;   // the option that turns it on, or RO_MAX for every randomizer world
    uint8_t area;           // where it is listed; COMBO_AREA_NONE = where its first check is
};

const MmFixedSource kMmFixedSources[] = {
    { RSBS_MM_HINT_SRC_REMAINS_ODOLWA,
      "Clock Town Poster (Odolwa)",
      RI_REMAINS_ODOLWA,
      { RC_UNKNOWN, RC_UNKNOWN },
      RO_HINTS_BOSS_REMAINS,
      COMBO_AREA_MM_CLOCK_TOWN_SOUTH },
    { RSBS_MM_HINT_SRC_REMAINS_GOHT,
      "Clock Town Poster (Goht)",
      RI_REMAINS_GOHT,
      { RC_UNKNOWN, RC_UNKNOWN },
      RO_HINTS_BOSS_REMAINS,
      COMBO_AREA_MM_CLOCK_TOWN_SOUTH },
    { RSBS_MM_HINT_SRC_REMAINS_GYORG,
      "Clock Town Poster (Gyorg)",
      RI_REMAINS_GYORG,
      { RC_UNKNOWN, RC_UNKNOWN },
      RO_HINTS_BOSS_REMAINS,
      COMBO_AREA_MM_CLOCK_TOWN_SOUTH },
    { RSBS_MM_HINT_SRC_REMAINS_TWINMOLD,
      "Clock Town Poster (Twinmold)",
      RI_REMAINS_TWINMOLD,
      { RC_UNKNOWN, RC_UNKNOWN },
      RO_HINTS_BOSS_REMAINS,
      COMBO_AREA_MM_CLOCK_TOWN_SOUTH },
    { RSBS_MM_HINT_SRC_OATH,
      "Skull Kid (Oath to Order)",
      RI_SONG_OATH,
      { RC_UNKNOWN, RC_UNKNOWN },
      RO_HINTS_OATH_TO_ORDER,
      COMBO_AREA_MM_CLOCK_TOWER_ROOFTOP },
    { RSBS_MM_HINT_SRC_HOOKSHOT,
      "Zora (Hookshot)",
      RI_HOOKSHOT,
      { RC_UNKNOWN, RC_UNKNOWN },
      RO_HINTS_HOOKSHOT,
      COMBO_AREA_MM_ZORA_HALL },
    { RSBS_MM_HINT_SRC_GOLD_DUST,
      "Smithy (Gold Dust)",
      RI_BOTTLE_GOLD_DUST,
      { RC_UNKNOWN, RC_UNKNOWN },
      RO_MAX,
      COMBO_AREA_MM_MOUNTAIN_VILLAGE },
    { RSBS_MM_HINT_SRC_GOLD_DUST_PRIZE,
      "Smithy (Gold Dust Prize)",
      RI_BOTTLE_GOLD_DUST,
      { RC_MOUNTAIN_VILLAGE_SMITHY_GILDED_SWORD, RC_UNKNOWN },
      RO_MAX,
      COMBO_AREA_MM_MOUNTAIN_VILLAGE },
    { RSBS_MM_HINT_SRC_SPIDER_HOUSE_SWAMP,
      "Swamp Spider House Man",
      RI_UNKNOWN,
      { RC_SWAMP_SPIDER_HOUSE_MASK_OF_TRUTH, RC_UNKNOWN },
      RO_HINTS_SPIDER_HOUSES,
      COMBO_AREA_NONE },
    { RSBS_MM_HINT_SRC_SPIDER_HOUSE_OCEAN,
      "Ocean Spider House Man",
      RI_UNKNOWN,
      { RC_OCEAN_SPIDER_HOUSE_WALLET, RC_UNKNOWN },
      RO_HINTS_SPIDER_HOUSES,
      COMBO_AREA_NONE },
    { RSBS_MM_HINT_SRC_FAIRY_CLOCK_TOWN,
      "Clock Town Great Fairy",
      RI_UNKNOWN,
      { RC_CLOCK_TOWN_GREAT_FAIRY, RC_CLOCK_TOWN_GREAT_FAIRY_ALT },
      RO_MAX,
      COMBO_AREA_NONE },
    { RSBS_MM_HINT_SRC_FAIRY_WOODFALL,
      "Woodfall Great Fairy",
      RI_UNKNOWN,
      { RC_WOODFALL_GREAT_FAIRY, RC_UNKNOWN },
      RO_MAX,
      COMBO_AREA_NONE },
    { RSBS_MM_HINT_SRC_FAIRY_SNOWHEAD,
      "Snowhead Great Fairy",
      RI_UNKNOWN,
      { RC_SNOWHEAD_GREAT_FAIRY, RC_UNKNOWN },
      RO_MAX,
      COMBO_AREA_NONE },
    { RSBS_MM_HINT_SRC_FAIRY_GREAT_BAY,
      "Great Bay Great Fairy",
      RI_UNKNOWN,
      { RC_GREAT_BAY_GREAT_FAIRY, RC_UNKNOWN },
      RO_MAX,
      COMBO_AREA_NONE },
    { RSBS_MM_HINT_SRC_FAIRY_IKANA,
      "Ikana Great Fairy",
      RI_UNKNOWN,
      { RC_IKANA_GREAT_FAIRY, RC_UNKNOWN },
      RO_MAX,
      COMBO_AREA_NONE },
    { RSBS_MM_HINT_SRC_BOMB_SHOP_STOCK,
      "Bomb Shop (Next Stock)",
      RI_UNKNOWN,
      { RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM, RC_UNKNOWN },
      RO_MAX,
      COMBO_AREA_NONE },
};

static_assert(sizeof(kMmFixedSources) / sizeof(kMmFixedSources[0]) ==
                  (size_t)(RSBS_MM_HINT_SRC_COUNT - RSBS_MM_HINT_SRC_REMAINS_ODOLWA),
              "every fixed MM hint source (combo_hint_view.h) has one row here, in id order");

const MmFixedSource* FixedSourceOf(uint16_t source) {
    if (source < RSBS_MM_HINT_SRC_REMAINS_ODOLWA || source >= RSBS_MM_HINT_SRC_COUNT) {
        return nullptr;
    }
    const MmFixedSource* row = &kMmFixedSources[source - RSBS_MM_HINT_SRC_REMAINS_ODOLWA];
    return row->source == source ? row : nullptr;
}

// ---- The texts' shared pieces ---------------------------------------------

/** The spider houses' and Great Fairies' item phrase: the stored item's article
 *  (and a space) for `articleKey`, its name for `itemKey` (upstream's expression). */
void ReplaceStoredItem(std::string* msg, const RandoSaveCheck* checks, RandoCheckId check, const char* articleKey,
                       const char* itemKey) {
    auto& randoStaticItem = Rando::StaticData::Items[checks[check].randoItemId];

    if (!Ship_IsCStringEmpty(randoStaticItem.article)) {
        CustomMessage::Replace(msg, articleKey, std::string(randoStaticItem.article) + " ");
    } else {
        CustomMessage::Replace(msg, articleKey, "");
    }

    CustomMessage::Replace(msg, itemKey, randoStaticItem.name);
}

/** The resolution's entry for an item-location hint: where `item` is, which its
 *  text names. */
void AddItemLocation(ComboHintResolution* out, const RandoSaveCheck* checks, RandoItemId item) {
    if (out == nullptr) {
        return;
    }
    ComboHintTarget t = {};
    uint8_t game = (uint8_t)GAME_NONE;
    uint16_t check = 0;
    ComboArea area = { (uint8_t)GAME_MM, (uint8_t)COMBO_AREA_NONE };
    if (Rando::HintArea::ItemLocation(checks, item, &game, &check, &area)) {
        t.checkGame = game;
        t.check = check;
    }
    t.itemGame = (uint8_t)GAME_MM;
    t.item = (uint16_t)item;
    t.area = area;
    (void)Combo_HintResolutionAddTarget(out, &t);
    if (area.area != COMBO_AREA_NONE) {
        (void)Combo_HintResolutionAddArea(out, area);
    }
}

/** The resolution's entry for a hint naming check `check`'s item. `crossed`: the
 *  text names a crossing host's crossed item (the Bomb Shop's line), not MM's cover. */
void AddCheckItem(ComboHintResolution* out, const RandoSaveCheck* checks, RandoCheckId check, bool crossed) {
    if (out == nullptr) {
        return;
    }
    ComboHintTarget t = {};
    t.checkGame = (uint8_t)GAME_MM;
    t.check = (uint16_t)check;
    const SharedItem* foreign = crossed ? Combo_GetForeignPlacementForCheck((uint16_t)check) : nullptr;
    if (foreign != nullptr) {
        t.itemGame = foreign->originGame;
        t.item = foreign->id;
    } else {
        t.itemGame = (uint8_t)GAME_MM;
        t.item = (uint16_t)checks[check].randoItemId;
    }
    t.area = Combo_Area_OfCheck((uint8_t)GAME_MM, (uint16_t)check);
    (void)Combo_HintResolutionAddTarget(out, &t);
}

/** GetItemName's article and name (StaticData/Items.cpp) without the
 *  " (<junk item>)" it appends to RI_JUNK: that suffix is Rando::CurrentJunkItem's
 *  pick, drawn from MM's global stream against MM's live save, so the resolver
 *  never asks for it (HintSources.h, THE ONE DELIBERATE DIFFERENCE). */
std::string PlainItemName(RandoItemId item) {
    auto& randoStaticItem = Rando::StaticData::Items[item];
    std::string name;
    if (!Ship_IsCStringEmpty(randoStaticItem.article)) {
        name += randoStaticItem.article;
        name += " ";
    }
    name += randoStaticItem.name;
    return name;
}

/** The Bomb Shop line's item phrase (text 0x648). Textbox: the hook's own
 *  expression, call for call: GetItemName runs first whatever the slot holds, so
 *  an RI_JUNK there (MM's own padding, or the cover of a slot whose OoT item
 *  crossed) draws its suffix as main drew it, even when ShopOfferedItemName then
 *  names the crossed item instead. Plain: GetItemName is never asked about
 *  RI_JUNK; ShopOfferedItemName names a crossed item first (the foreign name, so
 *  a cover's suffix was never shown), and MM's own padding is "Junk". */
std::string BombShopItemPhrase(const RandoSaveCheck* checks, Rando::HintSources::JunkName junkName) {
    const RandoCheckId rc = RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM;
    const RandoItemId item = checks[rc].randoItemId;
    const std::string nativeName = (junkName == Rando::HintSources::JunkName::Plain && item == RI_JUNK)
                                       ? PlainItemName(item)
                                       : Rando::StaticData::GetItemName(item);
    return Rando::ActorBehavior::ShopOfferedItemName(rc, nativeName, true);
}

/** A remains poster's boss line, padded as upstream pads it. */
const char* RemainsBoss(uint16_t source) {
    switch (source) {
        case RSBS_MM_HINT_SRC_REMAINS_ODOLWA:
            return " Odolwa";
        case RSBS_MM_HINT_SRC_REMAINS_GOHT:
            return "  Goht";
        case RSBS_MM_HINT_SRC_REMAINS_GYORG:
            return "  Gyorg";
        default:
            return "Twinmold";
    }
}

/** Upstream's message for fixed source `source` over `checks` (the hook's own
 *  text, moved here; each hook's #else keeps upstream's expression). `junkName`
 *  matters to the Bomb Shop's line only (BombShopItemPhrase). */
std::string BuildFixedText(const MmFixedSource& row, const RandoSaveCheck* checks,
                           Rando::HintSources::JunkName junkName) {
    std::string msg;
    switch (row.source) {
        case RSBS_MM_HINT_SRC_REMAINS_ODOLWA:
        case RSBS_MM_HINT_SRC_REMAINS_GOHT:
        case RSBS_MM_HINT_SRC_REMAINS_GYORG:
        case RSBS_MM_HINT_SRC_REMAINS_TWINMOLD:
            // EnTalk.cpp ApplyRemainsHint, pages 1-4.
            msg = "         %g{{boss}}%w:\n"
                  "Last seen " RANDO_HINT_PREPOS("in near") " %y{{location}}%w.";
            CustomMessage::Replace(&msg, "{{boss}}", RemainsBoss(row.source));
            Rando::HintArea::ReplaceItemLocation(checks, &msg, "{{location}}", row.item);
            break;
        case RSBS_MM_HINT_SRC_OATH:
            // DmStk.cpp ApplyOathHint, once the remains count is met.
            msg = "I can hear the Giants Melody coming from "
                  "%y{{location}}%w. But it's too late! They can't help you now!";
            Rando::HintArea::ReplaceItemLocation(checks, &msg, "{{location}}", RI_SONG_OATH);
            break;
        case RSBS_MM_HINT_SRC_HOOKSHOT:
            // EnZow.cpp ApplyHookshotHint.
            msg = "I overheard those Pirates talk about treasure " RANDO_HINT_PREPOS(
                "in") " %y{{location}}%w that can help you reach the unreachable!";
            Rando::HintArea::ReplaceItemLocation(checks, &msg, "{{location}}", RI_HOOKSHOT);
            break;
        case RSBS_MM_HINT_SRC_GOLD_DUST:
            // EnKgy.cpp, text 0xC49.
            msg = "Huh? You say that gold dust can be found " RANDO_HINT_PREPOS("at") " %r{location}%w?\x19";
            Rando::HintArea::ReplaceItemLocation(checks, &msg, "{location}", RI_BOTTLE_GOLD_DUST);
            break;
        case RSBS_MM_HINT_SRC_GOLD_DUST_PRIZE:
            // EnKgy.cpp, text 0xC4B.
            msg = "Gold dust can be found " RANDO_HINT_PREPOS("at") " %p{location}%w.\x10";
            msg += "Bring me that, and my %r{itemName}%w is all yours.\xE0";
            CustomMessage::Replace(
                &msg, "{itemName}",
                Rando::StaticData::Items[checks[RC_MOUNTAIN_VILLAGE_SMITHY_GILDED_SWORD].randoItemId].name);
            Rando::HintArea::ReplaceItemLocation(checks, &msg, "{location}", RI_BOTTLE_GOLD_DUST);
            break;
        case RSBS_MM_HINT_SRC_SPIDER_HOUSE_SWAMP:
            // EnSsh.cpp ApplySwampSpiderHouseHint.
            msg = "Make me...normal again...I'll give you %g{{article}}{{item}}%w...Please...help me...\xE0";
            ReplaceStoredItem(&msg, checks, RC_SWAMP_SPIDER_HOUSE_MASK_OF_TRUTH, "{{article}}", "{{item}}");
            break;
        case RSBS_MM_HINT_SRC_SPIDER_HOUSE_OCEAN:
            // EnSsh.cpp ApplyOceanSpiderHouseHint.
            msg = "Huh? How'd I get up here... Why do I have %g{{article}}{{item}}%w in my pocket...?\xE0";
            ReplaceStoredItem(&msg, checks, RC_OCEAN_SPIDER_HOUSE_WALLET, "{{article}}", "{{item}}");
            break;
        case RSBS_MM_HINT_SRC_FAIRY_CLOCK_TOWN:
            // EnElfgrp.cpp ApplyClockTownGreatFairyHint.
            msg = "%wPlease, find the Stray Fairy who's lost! We will reward you with %g{{article1}}{{item1}}%w and "
                  "maybe "
                  "even %g{{article2}}{{item2}}%w if you are worthy.";
            ReplaceStoredItem(&msg, checks, RC_CLOCK_TOWN_GREAT_FAIRY, "{{article1}}", "{{item1}}");
            ReplaceStoredItem(&msg, checks, RC_CLOCK_TOWN_GREAT_FAIRY_ALT, "{{article2}}", "{{item2}}");
            break;
        case RSBS_MM_HINT_SRC_FAIRY_WOODFALL:
        case RSBS_MM_HINT_SRC_FAIRY_SNOWHEAD:
        case RSBS_MM_HINT_SRC_FAIRY_GREAT_BAY:
        case RSBS_MM_HINT_SRC_FAIRY_IKANA:
            // EnElfgrp.cpp ApplyGreatFairyHint.
            msg = "%wPlease, find the Stray Fairies who match our color! We will reward you with "
                  "%g{{article}}{{item}}%w.";
            ReplaceStoredItem(&msg, checks, row.checks[0], "{{article}}", "{{item}}");
            break;
        case RSBS_MM_HINT_SRC_BOMB_SHOP_STOCK: {
            // EnGirlA.cpp, text 0x648.
            msg =
                "If nothing devastating happens to Mommy tonight, we should be able to sell %r{{itemName}}%w.\x19\xA8";
            const std::string itemName = BombShopItemPhrase(checks, junkName);
            CustomMessage::Replace(&msg, "{{itemName}}", itemName);
            break;
        }
        default:
            break;
    }
    return msg;
}

// ---- The source list -------------------------------------------------------

/** Every source's label, built once: a stone by its scene's name (numbered when
 *  the scene has several), a gossip grotto's stones by their table order, a fixed
 *  source by its row. Static storage the view hands out. */
const std::vector<std::string>& SourceNames() {
    static std::vector<std::string> sNames;
    if (!sNames.empty()) {
        return sNames;
    }
    std::vector<std::string> names((size_t)RSBS_MM_HINT_SRC_COUNT);
    const int stones = Rando::HintSources::StoneCount();
    for (int i = 0; i < stones && i < (int)RSBS_MM_HINT_SRC_STONE_COUNT; i++) {
        int16_t scene = 0;
        (void)Rando::HintSources::StoneAt(i, &scene, nullptr, nullptr);
        int ordinal = 0;
        int inScene = 0;
        for (int j = 0; j < stones; j++) {
            int16_t other = 0;
            (void)Rando::HintSources::StoneAt(j, &other, nullptr, nullptr);
            if (other == scene) {
                inScene++;
                if (j <= i) {
                    ordinal++;
                }
            }
        }
        std::string name;
        if (scene == (int16_t)SCENE_KAKUSIANA) {
            name = "Gossip Grotto Stone";
        } else {
            const char* sceneName = Ship_GetSceneName(scene);
            name = std::string(sceneName != nullptr ? sceneName : "") + " Gossip Stone";
        }
        if (inScene > 1) {
            name += " #" + std::to_string(ordinal);
        }
        names[(size_t)(RSBS_MM_HINT_SRC_STONE_FIRST + i)] = name;
    }
    for (const MmFixedSource& row : kMmFixedSources) {
        names[row.source] = row.name;
    }
    sNames = names;
    return sNames;
}

bool MMHintSourceInfo(uint16_t source, ComboHintSourceInfo* out) {
    if (out == nullptr || source >= RSBS_MM_HINT_SRC_COUNT) {
        return false;
    }
    std::memset(out, 0, sizeof(*out));
    out->source = source;
    out->name = SourceNames()[source].c_str();
    if (source < RSBS_MM_HINT_SRC_STONE_FIRST + RSBS_MM_HINT_SRC_STONE_COUNT) {
        int16_t scene = 0;
        if (!Rando::HintSources::StoneAt((int)(source - RSBS_MM_HINT_SRC_STONE_FIRST), &scene, nullptr, nullptr)) {
            return false;
        }
        out->kind = (uint8_t)COMBO_HINT_SOURCE_GOSSIP_STONE;
        out->area = Rando::HintArea::SceneArea(scene);
        return true;
    }
    const MmFixedSource* row = FixedSourceOf(source);
    if (row == nullptr) {
        return false;
    }
    out->kind = (uint8_t)COMBO_HINT_SOURCE_FIXED;
    out->area = (row->area != COMBO_AREA_NONE) ? ComboArea{ (uint8_t)GAME_MM, row->area }
                                               : Combo_Area_OfCheck((uint8_t)GAME_MM, (uint16_t)row->checks[0]);
    return true;
}

bool MMHintListed(const void* save, uint16_t source) {
    return Rando::HintSources::Listed(static_cast<const SaveContext*>(save), source);
}

bool MMHintResolve(const void* save, uint16_t source, ComboHintResolution* out) {
    return Rando::HintSources::Resolve(static_cast<const SaveContext*>(save), source, out);
}

} // namespace

// ============================================================================
// Rando/HintSources.h
// ============================================================================

std::string Rando::HintSources::FixedText(uint16_t source, const RandoSaveCheck* checks, ComboHintResolution* out,
                                          JunkName junkName) {
    Combo_HintResolutionClear(out);
    const MmFixedSource* row = FixedSourceOf(source);
    if (row == nullptr || checks == nullptr) {
        return "";
    }
    const std::string msg = BuildFixedText(*row, checks, junkName);
    if (out != nullptr) {
        out->type = (uint8_t)COMBO_HINT_TYPE_FIXED;
        if (row->item != RI_UNKNOWN) {
            AddItemLocation(out, checks, row->item);
        }
        for (const RandoCheckId check : row->checks) {
            if (check != RC_UNKNOWN) {
                AddCheckItem(out, checks, check, row->source == RSBS_MM_HINT_SRC_BOMB_SHOP_STOCK);
            }
        }
        (void)Combo_HintResolutionSetText(out, msg.c_str());
    }
    return msg;
}

RandoItemId Rando::HintSources::FixedItem(uint16_t source) {
    const MmFixedSource* row = FixedSourceOf(source);
    return row != nullptr ? row->item : RI_UNKNOWN;
}

uint16_t Rando::HintSources::GreatFairySource(RandoCheckId randoCheckId) {
    switch (randoCheckId) {
        case RC_CLOCK_TOWN_GREAT_FAIRY:
        case RC_CLOCK_TOWN_GREAT_FAIRY_ALT:
            return RSBS_MM_HINT_SRC_FAIRY_CLOCK_TOWN;
        case RC_WOODFALL_GREAT_FAIRY:
            return RSBS_MM_HINT_SRC_FAIRY_WOODFALL;
        case RC_SNOWHEAD_GREAT_FAIRY:
            return RSBS_MM_HINT_SRC_FAIRY_SNOWHEAD;
        case RC_GREAT_BAY_GREAT_FAIRY:
            return RSBS_MM_HINT_SRC_FAIRY_GREAT_BAY;
        case RC_IKANA_GREAT_FAIRY:
            return RSBS_MM_HINT_SRC_FAIRY_IKANA;
        default:
            return RSBS_MM_HINT_SRC_COUNT;
    }
}

bool Rando::HintSources::Listed(const SaveContext* save, uint16_t source) {
    if (save == nullptr || source >= RSBS_MM_HINT_SRC_COUNT || save->save.shipSaveInfo.saveType != SAVETYPE_RANDO) {
        return false;
    }
    const u32* options = save->save.shipSaveInfo.rando.randoSaveOptions;
    if (source < RSBS_MM_HINT_SRC_STONE_FIRST + RSBS_MM_HINT_SRC_STONE_COUNT) {
        // The free stone's text is shown only with gossip-stone hints on (EnGs.cpp's
        // FIRST_GS_MESSAGE hook); the purchased hint is no source.
        return options[RO_HINTS_GOSSIP_STONES] != 0;
    }
    const MmFixedSource* row = FixedSourceOf(source);
    if (row == nullptr) {
        return false;
    }
    return row->option == RO_MAX || options[row->option] != 0;
}

bool Rando::HintSources::Resolve(const SaveContext* save, uint16_t source, ComboHintResolution* out) {
    Combo_HintResolutionClear(out);
    if (!Listed(save, source)) {
        return false;
    }
    const RandoSaveCheck* checks = save->save.shipSaveInfo.rando.randoSaveChecks;
    if (source < RSBS_MM_HINT_SRC_STONE_FIRST + RSBS_MM_HINT_SRC_STONE_COUNT) {
        return ResolveStone(checks, save->save.shipSaveInfo.rando.finalSeed,
                            (int)(source - RSBS_MM_HINT_SRC_STONE_FIRST), out);
    }
    // Plain: the resolver names MM's own junk padding without the textbox's drawn
    // suffix, so it reads no stream and no live save (HintSources.h).
    ComboHintResolution resolved;
    if (FixedText(source, checks, &resolved, JunkName::Plain).empty()) {
        return false;
    }
    if (out != nullptr) {
        *out = resolved;
    }
    return true;
}

void Rando::HintSources::MarkRead(uint16_t source) {
    if (source >= RSBS_MM_HINT_SRC_COUNT) {
        return;
    }
    (void)Combo_HintMarkRead((uint8_t)GAME_MM, source);
    // MM-side Seen: every MM check the hint names is marked seen in MM's live
    // save, the byte MM's own check tracker and the combined Check Tracker's MM
    // pane read (MarkCheckSeen, as the Bomb Shop's line already did). A hint that
    // names an OoT host (an MM item that crossed) is left to lane H2.
    ComboHintResolution resolved;
    if (!Resolve(&gSaveContext, source, &resolved)) {
        return;
    }
    for (int i = 0; i < (int)resolved.targetCount; i++) {
        const ComboHintTarget& t = resolved.targets[i];
        if (t.checkGame == (uint8_t)GAME_MM && t.check > (uint16_t)RC_UNKNOWN && t.check < (uint16_t)RC_MAX) {
            Rando::ActorBehavior::MarkCheckSeen((RandoCheckId)t.check, RCSEEN_SEEN);
        }
    }
}

/**
 * The MM adapter of combo_hint_view.h. Called from MM_TrackerAdapter_Register
 * (TrackerAdapterSingleExe.cpp), so every bring-up of the combo tracker's MM
 * adapter (Combo_TrackerWindow_Init, which main.cpp calls) registers it, never a
 * file-scope registrar: the source names read Rando::StaticData and the scene
 * table, whose population must not race static init.
 */
extern "C" void MM_HintAdapter_Register(void) {
    ComboMMHintDesc desc = {};
    desc.sourceCount = (uint16_t)RSBS_MM_HINT_SRC_COUNT;
    desc.sourceInfo = MMHintSourceInfo;
    desc.listed = MMHintListed;
    desc.resolve = MMHintResolve;
    Combo_Hint_RegisterMM(&desc);
}

// ============================================================================
// TEST BRIDGES (src/common/tests/test_mm_hint_sources.c)
// ============================================================================

/** sizeof(SaveContext): the image size the bridges below copy. */
extern "C" int MM_HintSources_TestSaveSize(void) {
    return (int)sizeof(SaveContext);
}

/** Copy MM's live save into `out` (MM_HintSources_TestSaveSize() bytes). */
extern "C" void MM_HintSources_TestCopyLive(void* out) {
    if (out != nullptr) {
        std::memcpy(out, &gSaveContext, sizeof(SaveContext));
    }
}

/** Overwrite MM's live save with the image `in`. */
extern "C" void MM_HintSources_TestSetLive(const void* in) {
    if (in != nullptr) {
        std::memcpy(&gSaveContext, in, sizeof(SaveContext));
    }
}

/** Author a marked randomizer save in `image` (NULL: MM's live save): zeroed,
 *  the 'ZELDA3' new-file marker, SAVETYPE_RANDO, `finalSeed`, and every hint
 *  option at `hintsOn` (purchasable hints off). */
extern "C" void MM_HintSources_TestAuthorSave(void* image, uint32_t finalSeed, int hintsOn) {
    SaveContext* save = image != nullptr ? static_cast<SaveContext*>(image) : &gSaveContext;
    std::memset(save, 0, sizeof(SaveContext));
    static const char kNewf[6] = { 'Z', 'E', 'L', 'D', 'A', '3' };
    std::memcpy(save->save.saveInfo.playerData.newf, kNewf, sizeof(kNewf));
    save->save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    save->save.shipSaveInfo.rando.finalSeed = finalSeed;
    const u32 on = hintsOn != 0 ? 1u : 0u;
    save->save.shipSaveInfo.rando.randoSaveOptions[RO_HINTS_GOSSIP_STONES] = on;
    save->save.shipSaveInfo.rando.randoSaveOptions[RO_HINTS_BOSS_REMAINS] = on;
    save->save.shipSaveInfo.rando.randoSaveOptions[RO_HINTS_OATH_TO_ORDER] = on;
    save->save.shipSaveInfo.rando.randoSaveOptions[RO_HINTS_HOOKSHOT] = on;
    save->save.shipSaveInfo.rando.randoSaveOptions[RO_HINTS_SPIDER_HOUSES] = on;
    save->save.shipSaveInfo.rando.randoSaveOptions[RO_HINTS_PURCHASEABLE] = 0;
}

/** Resolve `source` over the image `save` (NULL: MM's live save). 1, or 0 when it says nothing. */
extern "C" int MM_HintSources_TestResolve(const void* save, uint16_t source, ComboHintResolution* out) {
    const SaveContext* image = save != nullptr ? static_cast<const SaveContext*>(save) : &gSaveContext;
    return Rando::HintSources::Resolve(image, source, out) ? 1 : 0;
}

/** The read hook for `source`, as its textbox hook runs it. */
extern "C" void MM_HintSources_TestMarkRead(uint16_t source) {
    Rando::HintSources::MarkRead(source);
}

// CurrentJunkItem's cache made cold (ConvertItem.cpp's test bridge).
extern "C" void MM_Rando_TestColdJunkCache(void);

/** 1 when the read hook for `source` (with `mark` nonzero) or the resolver over
 *  the image `save` (NULL: the live save; with `mark` zero) leaves MM's global
 *  Ship_Random stream where it was: put at `streamSeed`, it draws the same next
 *  value with or without the call in between. The stone's flavour line draws from
 *  that stream right after its text, so neither may move it. Rando::CurrentJunkItem's
 *  cache is made cold first, as after more than 20 frames or with MM not running:
 *  a warm cache answers without a draw, so a call that named MM's junk padding
 *  through GetItemName would otherwise pass. */
extern "C" int MM_HintSources_TestKeepsStream(int mark, const void* save, uint16_t source, uint64_t streamSeed) {
    MM_Rando_TestColdJunkCache();
    Ship_Random_Seed(streamSeed);
    const s32 untouched = Ship_Random(0, 0x40000000);
    Ship_Random_Seed(streamSeed);
    if (mark != 0) {
        Rando::HintSources::MarkRead(source);
    } else {
        ComboHintResolution resolved;
        const SaveContext* image = save != nullptr ? static_cast<const SaveContext*>(save) : &gSaveContext;
        (void)Rando::HintSources::Resolve(image, source, &resolved);
    }
    return Ship_Random(0, 0x40000000) == untouched ? 1 : 0;
}

/** The text fixed source `source`'s textbox hook shows over MM's live save:
 *  FixedText with JunkName::Textbox, as the Bomb Shop hook calls it (every other
 *  hook's text is the same with either). Draws as the hook draws. Returns the full
 *  length, or -1 for a source that is not fixed. */
extern "C" int MM_HintSources_TestTextboxText(uint16_t source, char* out, int cap) {
    if (out == nullptr || cap <= 0 || FixedSourceOf(source) == nullptr) {
        return -1;
    }
    const std::string msg =
        Rando::HintSources::FixedText(source, RANDO_SAVE_CHECKS, nullptr, Rando::HintSources::JunkName::Textbox);
    snprintf(out, (size_t)cap, "%s", msg.c_str());
    return (int)msg.size();
}

/** The Bomb Shop line's check (RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM) and
 *  MM's junk padding item (RI_JUNK), for the rows that put the one in the other. */
extern "C" void MM_HintSources_TestBombShopIds(uint16_t* outCheck, uint16_t* outJunk) {
    if (outCheck != nullptr) {
        *outCheck = (uint16_t)RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM;
    }
    if (outJunk != nullptr) {
        *outJunk = (uint16_t)RI_JUNK;
    }
}

/** Check `check`'s seen byte in the image `save` (NULL: MM's live save); -1 out of range. */
extern "C" int MM_HintSources_TestSeen(const void* save, uint16_t check) {
    if (check >= (uint16_t)RC_MAX) {
        return -1;
    }
    const SaveContext* image = save != nullptr ? static_cast<const SaveContext*>(save) : &gSaveContext;
    return (int)image->save.shipSaveInfo.rando.randoSaveChecks[check].seen;
}

/** Set check `check`'s seen byte in MM's live save, returning the prior value (-1 out of range). */
extern "C" int MM_HintSources_TestSetSeen(uint16_t check, int seen) {
    if (check >= (uint16_t)RC_MAX) {
        return -1;
    }
    RandoSaveCheck& row = RANDO_SAVE_CHECKS[check];
    const int prior = (int)row.seen;
    row.seen = (u8)seen;
    return prior;
}

/**
 * MM's global stream against the caller-held one (MM_Ship_RandomOnStream), from
 * seed `seed` over [0, max): the global draw Ship_Random_Seed(seed);
 * Ship_Random(0, max) and the draw after it, and the same two from a local
 * stream started at `seed` (the second through Ship_Random_Seed(stream), as the
 * talk-time path hands it back). Also `*outUntouched` = 1 when a local draw left
 * the global stream where it was.
 */
extern "C" void MM_HintSources_TestStreamReplica(uint64_t seed, int32_t max, int32_t* outGlobal, int32_t* outGlobalNext,
                                                 int32_t* outLocal, int32_t* outLocalNext, int* outUntouched) {
    Ship_Random_Seed(seed);
    *outGlobal = Ship_Random(0, max);
    *outGlobalNext = Ship_Random(0, 0x40000000);

    Ship_Random_Seed(seed ^ 0x5A5A5A5A5A5A5A5Aull);
    const s32 untouched = Ship_Random(0, 0x40000000);
    Ship_Random_Seed(seed ^ 0x5A5A5A5A5A5A5A5Aull);
    u64 stream = seed;
    *outLocal = MM_Ship_RandomOnStream(&stream, 0, max);
    *outUntouched = Ship_Random(0, 0x40000000) == untouched ? 1 : 0;
    Ship_Random_Seed(stream);
    *outLocalNext = Ship_Random(0, 0x40000000);
}

/**
 * Fixed source `source`'s text the way main's hooks built it, before #1475 moved
 * it here: upstream's expressions over MM's LIVE save (RANDO_SAVE_CHECKS and the
 * live ReplaceItemLocation), copied from each hook's #else. The rows hold the
 * hook's text (FixedText, JunkName::Textbox) to it, so the move changed no byte,
 * and the resolver's to it but for the one junk suffix (HintSources.h). Draws as
 * the hook drew. Returns the full length, or -1 for a source that is not fixed.
 */
extern "C" int MM_HintSources_TestLegacyFixedText(uint16_t source, char* out, int cap) {
    if (out == nullptr || cap <= 0 || FixedSourceOf(source) == nullptr) {
        return -1;
    }
    std::string msg;
    switch (source) {
        case RSBS_MM_HINT_SRC_REMAINS_ODOLWA:
        case RSBS_MM_HINT_SRC_REMAINS_GOHT:
        case RSBS_MM_HINT_SRC_REMAINS_GYORG:
        case RSBS_MM_HINT_SRC_REMAINS_TWINMOLD:
            msg = "         %g{{boss}}%w:\n"
                  "Last seen " RANDO_HINT_PREPOS("in near") " %y{{location}}%w.";
            CustomMessage::Replace(&msg, "{{boss}}", RemainsBoss(source));
            Rando::HintArea::ReplaceItemLocation(&msg, "{{location}}", FixedSourceOf(source)->item);
            break;
        case RSBS_MM_HINT_SRC_OATH:
            msg = "I can hear the Giants Melody coming from "
                  "%y{{location}}%w. But it's too late! They can't help you now!";
            Rando::HintArea::ReplaceItemLocation(&msg, "{{location}}", RI_SONG_OATH);
            break;
        case RSBS_MM_HINT_SRC_HOOKSHOT:
            msg = "I overheard those Pirates talk about treasure " RANDO_HINT_PREPOS(
                "in") " %y{{location}}%w that can help you reach the unreachable!";
            Rando::HintArea::ReplaceItemLocation(&msg, "{{location}}", RI_HOOKSHOT);
            break;
        case RSBS_MM_HINT_SRC_GOLD_DUST:
            msg = "Huh? You say that gold dust can be found " RANDO_HINT_PREPOS("at") " %r{location}%w?\x19";
            Rando::HintArea::ReplaceItemLocation(&msg, "{location}", RI_BOTTLE_GOLD_DUST);
            break;
        case RSBS_MM_HINT_SRC_GOLD_DUST_PRIZE:
            msg = "Gold dust can be found " RANDO_HINT_PREPOS("at") " %p{location}%w.\x10";
            msg += "Bring me that, and my %r{itemName}%w is all yours.\xE0";
            CustomMessage::Replace(
                &msg, "{itemName}",
                Rando::StaticData::Items[RANDO_SAVE_CHECKS[RC_MOUNTAIN_VILLAGE_SMITHY_GILDED_SWORD].randoItemId].name);
            Rando::HintArea::ReplaceItemLocation(&msg, "{location}", RI_BOTTLE_GOLD_DUST);
            break;
        case RSBS_MM_HINT_SRC_SPIDER_HOUSE_SWAMP: {
            msg = "Make me...normal again...I'll give you %g{{article}}{{item}}%w...Please...help me...\xE0";
            auto& randoStaticItem =
                Rando::StaticData::Items[RANDO_SAVE_CHECKS[RC_SWAMP_SPIDER_HOUSE_MASK_OF_TRUTH].randoItemId];
            CustomMessage::Replace(&msg, "{{article}}",
                                   !Ship_IsCStringEmpty(randoStaticItem.article)
                                       ? std::string(randoStaticItem.article) + " "
                                       : std::string(""));
            CustomMessage::Replace(&msg, "{{item}}", randoStaticItem.name);
            break;
        }
        case RSBS_MM_HINT_SRC_SPIDER_HOUSE_OCEAN: {
            msg = "Huh? How'd I get up here... Why do I have %g{{article}}{{item}}%w in my pocket...?\xE0";
            auto& randoStaticItem =
                Rando::StaticData::Items[RANDO_SAVE_CHECKS[RC_OCEAN_SPIDER_HOUSE_WALLET].randoItemId];
            CustomMessage::Replace(&msg, "{{article}}",
                                   !Ship_IsCStringEmpty(randoStaticItem.article)
                                       ? std::string(randoStaticItem.article) + " "
                                       : std::string(""));
            CustomMessage::Replace(&msg, "{{item}}", randoStaticItem.name);
            break;
        }
        case RSBS_MM_HINT_SRC_FAIRY_CLOCK_TOWN: {
            msg = "%wPlease, find the Stray Fairy who's lost! We will reward you with %g{{article1}}{{item1}}%w and "
                  "maybe "
                  "even %g{{article2}}{{item2}}%w if you are worthy.";
            auto& item1 = Rando::StaticData::Items[RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_GREAT_FAIRY].randoItemId];
            CustomMessage::Replace(&msg, "{{article1}}",
                                   !Ship_IsCStringEmpty(item1.article) ? std::string(item1.article) + " "
                                                                       : std::string(""));
            CustomMessage::Replace(&msg, "{{item1}}", item1.name);
            auto& item2 = Rando::StaticData::Items[RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_GREAT_FAIRY_ALT].randoItemId];
            CustomMessage::Replace(&msg, "{{article2}}",
                                   !Ship_IsCStringEmpty(item2.article) ? std::string(item2.article) + " "
                                                                       : std::string(""));
            CustomMessage::Replace(&msg, "{{item2}}", item2.name);
            break;
        }
        case RSBS_MM_HINT_SRC_FAIRY_WOODFALL:
        case RSBS_MM_HINT_SRC_FAIRY_SNOWHEAD:
        case RSBS_MM_HINT_SRC_FAIRY_GREAT_BAY:
        case RSBS_MM_HINT_SRC_FAIRY_IKANA: {
            msg = "%wPlease, find the Stray Fairies who match our color! We will reward you with "
                  "%g{{article}}{{item}}%w.";
            auto& item = Rando::StaticData::Items[RANDO_SAVE_CHECKS[FixedSourceOf(source)->checks[0]].randoItemId];
            CustomMessage::Replace(&msg, "{{article}}",
                                   !Ship_IsCStringEmpty(item.article) ? std::string(item.article) + " "
                                                                      : std::string(""));
            CustomMessage::Replace(&msg, "{{item}}", item.name);
            break;
        }
        case RSBS_MM_HINT_SRC_BOMB_SHOP_STOCK: {
            auto randoSaveCheck = RANDO_SAVE_CHECKS[RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM];
            msg =
                "If nothing devastating happens to Mommy tonight, we should be able to sell %r{{itemName}}%w.\x19\xA8";
            const std::string itemName = Rando::ActorBehavior::ShopOfferedItemName(
                RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM, Rando::StaticData::GetItemName(randoSaveCheck.randoItemId),
                true);
            CustomMessage::Replace(&msg, "{{itemName}}", itemName);
            break;
        }
        default:
            return -1;
    }
    snprintf(out, (size_t)cap, "%s", msg.c_str());
    return (int)msg.size();
}

#endif /* RSBS_SINGLE_EXECUTABLE */
