/**
 * ROM-free lock for "the combo owns MM's paired start" (#879, operator ruling
 * 2026-10-09, items 1-2). CTest row MMPairedStart, label "rando" (it runs a
 * real paired generation, so it needs the display bring-up the rando rows use;
 * no archive is read). Dispatch `mm-paired-start` in src/common/test_runner.cpp.
 *
 * WHAT #879 FOUND. A paired world's MM half started with 2Ship's default
 * starting-items list, { RI_PROGRESSIVE_SWORD, RI_SHIELD_HERO, RI_OCARINA,
 * RI_SONG_TIME } (Rando::GetStartingItemsFromConfig, StartingItems.cpp), or with
 * whatever a `CVars.gRando.StartingItems` block in the player's config named
 * instead: a source the combo's settings surface never shows. The ruling: at
 * creation the paired MM start is the combo's own frozen decision, the Ocarina
 * of Time and the Song of Time and nothing else; the Kokiri Sword and the Hero's
 * Shield leave the start and go into MM's pool, as OoTMM does; 2Ship's config
 * block is never read for a paired world.
 *
 * WHAT THIS ROW LOCKS, with a 2Ship config block present that names a DIFFERENT
 * list (the old default plus the Bunny Hood, so a leak names itself):
 *
 *   1. The identity. The creation stamp (MM_Rando_ComputeProfileStamp, the one
 *      computation creation and every arrival run) is the same with and without
 *      the block: a config change after creation changes nothing, and is never
 *      a reason to refuse an arrival.
 *   2. The start. A real paired creation (a headless OoT generation, then MM's
 *      creation-time half through the real OnSaveInit chain) persists exactly
 *      { RI_OCARINA, RI_SONG_TIME } into the MM half, and the half holds the
 *      Ocarina of Time and the Song of Time, no sword (B empty), no shield and
 *      no Bunny Hood.
 *   3. The pool. GeneratePools over the half's own persisted rando info (the
 *      creation's input, unchanged) holds the Kokiri Sword as a third
 *      progressive sword beside the two Smithy rows, and the Hero's Shield; the
 *      Skull Kid roof's Ocarina and Song of Time are not in it.
 *   4. The world. The single bag placed the sword and the shield: every pool
 *      copy sits on an MM check or, crossed, on an OoT host.
 *
 * Every leg reports and the row fails at the end, so a red run names every
 * contract it breaks rather than the first.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <ship/Context.h>
#include <ship/config/Config.h>
#include <nlohmann/json.hpp>
#include <libultraship/bridge/consolevariablebridge.h>

#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/Rando/StaticData/StaticData.h"

// src/common — the creation stamp, the coordinator's placement table.
#include "combo_mm_options_view.h"
#include "combo_logic.h"
#include "foreign_items.h"

extern "C" {
#include "functions.h"
#include "variables.h"
// 3drando/menu.cpp: the headless Generate button (freezes the pairing identity
// and the combo record, stops OoT's fill at its general pass).
int Rando_HeadlessSeedTest(const char* seedStr);
int OoT_ComboLogic_GeneralPassDeferred(void);
// mm_rando_gen_test.cpp: MM's creation-time half over that world, through the
// real OnSaveInit chain, without OoT's side of the tail.
int MM_Rando_HeadlessPairedHalf(void);
}

namespace {

constexpr const char* kStartSeed = "RSBSSINGLEBAG1"; // ComboSingleBag's pinned paired seed

int sFailures = 0;

// Ship::Config::SetBlock descends only through keys that already exist (a
// missing `CVars.gRando` would land the block at `CVars.StartingItems`, where
// 2Ship never looks), and it saves the config file. So the block is written at
// the deepest level that exists, and the key this row created is the one it
// erases afterwards.
std::string sCreatedKey;

void WriteStartingItemsBlock(const std::shared_ptr<Ship::Config>& config, const nlohmann::json& items) {
    const nlohmann::json nested = config->GetNestedJson();
    const bool hasCVars = nested.is_object() && nested.contains("CVars") && nested["CVars"].is_object();
    const bool hasRando = hasCVars && nested["CVars"].contains("gRando") && nested["CVars"]["gRando"].is_object();
    if (hasRando) {
        sCreatedKey = "CVars.gRando.StartingItems";
        config->SetBlock(sCreatedKey, items);
    } else if (hasCVars) {
        sCreatedKey = "CVars.gRando";
        config->SetBlock(sCreatedKey, nlohmann::json{ { "StartingItems", items } });
    } else {
        sCreatedKey = "CVars";
        config->SetBlock(sCreatedKey, nlohmann::json{ { "gRando", { { "StartingItems", items } } } });
    }
}

void EraseStartingItemsBlock(const std::shared_ptr<Ship::Config>& config) {
    if (!sCreatedKey.empty()) {
        config->EraseBlock(sCreatedKey);
        sCreatedKey.clear();
    }
}

void Check(bool ok, const char* what) {
    if (!ok) {
        sFailures++;
        printf("[TEST] FAIL: %s\n", what);
    }
}

std::string ItemNames(const std::vector<RandoItemId>& items) {
    std::string s;
    for (RandoItemId id : items) {
        if (!s.empty()) {
            s += ", ";
        }
        s += (id > RI_UNKNOWN && id < RI_MAX) ? Rando::StaticData::Items[id].spoilerName : "?";
    }
    return s.empty() ? std::string("(none)") : s;
}

int CountOf(const std::vector<RandoItemId>& items, RandoItemId id) {
    int n = 0;
    for (RandoItemId item : items) {
        n += item == id ? 1 : 0;
    }
    return n;
}

/** Copies of MM's @p id in the finished world: on an MM check, or crossed onto an
 *  OoT host by the single bag (the coordinator's table, still held). */
int PlacedInWorld(RandoItemId id, int* outOnOoT) {
    int onMM = 0;
    for (int rc = 0; rc < (int)RC_MAX; ++rc) {
        const RandoSaveCheck& check = RANDO_SAVE_CHECKS[rc];
        onMM += (check.shuffled && !check.skipped && check.randoItemId == id) ? 1 : 0;
    }
    int onOoT = 0;
    ComboLogicPlacement p;
    for (int i = 0; i < Combo_Logic_PlacementCount(GAME_OOT); ++i) {
        if (Combo_Logic_PlacementAt(GAME_OOT, i, &p) && p.item.originGame == (uint8_t)GAME_MM &&
            p.item.id == (uint16_t)id) {
            onOoT++;
        }
    }
    *outOnOoT = onOoT;
    return onMM + onOoT;
}

} // namespace

extern "C" int MM_PairedStart_RunHeadless(void) {
    printf("[TEST] mm-paired-start: the combo owns MM's paired start (Ocarina + Song of Time); the Kokiri Sword and "
           "the Hero's Shield are MM pool items; 2Ship's StartingItems block is never read (#879)\n");
    sFailures = 0;

    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConfig() == nullptr) {
        printf("[TEST] FAIL: no Ship::Context config: the 2Ship config block cannot be written, so every leg below "
               "would be vacuous\n");
        return 1;
    }
    auto config = ctx->GetConfig();

    // The block a player's config may carry: 2Ship's old default plus the Bunny
    // Hood, so any item of it reaching the paired world names the leak.
    const std::vector<RandoItemId> block = { RI_PROGRESSIVE_SWORD, RI_SHIELD_HERO, RI_OCARINA, RI_SONG_TIME,
                                             RI_MASK_BUNNY };
    auto blockJson = nlohmann::json::array();
    for (RandoItemId id : block) {
        blockJson.push_back(Rando::StaticData::Items[id].spoilerName);
    }

    // ------------------------------------------------------------------------
    // 1. The identity ignores the block.
    // ------------------------------------------------------------------------
    const uint32_t stampWithout = MM_Rando_ComputeProfileStamp();
    WriteStartingItemsBlock(config, blockJson);
    // Non-vacuity: 2Ship's own accessor reads the block this row wrote.
    const std::vector<RandoItemId> fromConfig = Rando::GetStartingItemsFromConfig();
    printf("[TEST] mm-paired-start: (1) 2Ship's config accessor now reads: %s\n", ItemNames(fromConfig).c_str());
    if (CountOf(fromConfig, RI_MASK_BUNNY) != 1) {
        printf("[TEST] FAIL: 2Ship's accessor did not read the block this row wrote; the legs would be vacuous\n");
        EraseStartingItemsBlock(config);
        return 1;
    }
    const uint32_t stampWith = MM_Rando_ComputeProfileStamp();
    printf("[TEST] mm-paired-start: (1) creation stamp without the block %08X, with it %08X (expected equal)\n",
           (unsigned)stampWithout, (unsigned)stampWith);
    Check(stampWith == stampWithout,
          "(1) the paired MM identity folds 2Ship's StartingItems config block: a config change after creation "
          "would refuse the world's arrival (#879)");

    // ------------------------------------------------------------------------
    // 2. A real paired creation with the block present.
    // ------------------------------------------------------------------------
    int rc = Rando_HeadlessSeedTest(kStartSeed);
    if (rc != 0 || !Combo_ForeignPairingActive() || !Combo_ComboSettingsFrozen() ||
        !OoT_ComboLogic_GeneralPassDeferred()) {
        printf("[TEST] FAIL: the paired OoT generation for %s did not leave a frozen paired world at its general "
               "pass (rc=%d)\n",
               kStartSeed, rc);
        EraseStartingItemsBlock(config);
        return 1;
    }
    rc = MM_Rando_HeadlessPairedHalf();
    if (rc != 0) {
        printf("[TEST] FAIL: MM's creation-time half did not produce a rando world (rc=%d)\n", rc);
        EraseStartingItemsBlock(config);
        return 1;
    }

    const std::vector<RandoItemId> persisted = Rando::GetStartingItemsFromSave(gSaveContext.save.shipSaveInfo.rando);
    printf("[TEST] mm-paired-start: (2) the MM half's frozen start list: %s (expected RI_OCARINA, RI_SONG_TIME)\n",
           ItemNames(persisted).c_str());
    Check(persisted.size() == 2 && CountOf(persisted, RI_OCARINA) == 1 && CountOf(persisted, RI_SONG_TIME) == 1,
          "(2) the paired MM start is not exactly the Ocarina of Time and the Song of Time (#879)");

    const bool ocarina = INV_CONTENT(ITEM_OCARINA_OF_TIME) == ITEM_OCARINA_OF_TIME;
    const bool songOfTime = CHECK_QUEST_ITEM(QUEST_SONG_TIME) != 0;
    const int sword = (int)GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    const int shield = (int)GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
    const int bButton = (int)BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B);
    const bool bunny = INV_CONTENT(ITEM_MASK_BUNNY) == ITEM_MASK_BUNNY;
    printf("[TEST] mm-paired-start: (2) the MM half holds: ocarina=%d songOfTime=%d sword=%d shield=%d B=0x%02X "
           "bunnyHood=%d (expected 1/1/%d/%d/0x%02X/0)\n",
           ocarina ? 1 : 0, songOfTime ? 1 : 0, sword, shield, bButton & 0xFF, bunny ? 1 : 0,
           (int)EQUIP_VALUE_SWORD_NONE, (int)EQUIP_VALUE_SHIELD_NONE, (int)ITEM_NONE & 0xFF);
    Check(ocarina, "(2) the paired MM half does not hold the Ocarina of Time");
    Check(songOfTime, "(2) the paired MM half does not hold the Song of Time");
    Check(sword == EQUIP_VALUE_SWORD_NONE && bButton == ITEM_NONE,
          "(2) the paired MM half starts with a sword: the Kokiri Sword is a pool item (#879)");
    Check(shield == EQUIP_VALUE_SHIELD_NONE,
          "(2) the paired MM half starts with a shield: the Hero's Shield is a pool item (#879)");
    Check(!bunny, "(2) 2Ship's StartingItems config block reached the paired world (the Bunny Hood was granted)");

    // ------------------------------------------------------------------------
    // 3. The pool the creation generated, from the half's own persisted input.
    // ------------------------------------------------------------------------
    auto info = std::make_unique<RandoSaveInfo>();
    memcpy(info.get(), &gSaveContext.save.shipSaveInfo.rando, sizeof(RandoSaveInfo));
    std::vector<RandoCheckId> checkPool;
    std::vector<RandoItemId> itemPool;
    Rando::Logic::GeneratePools(*info, checkPool, itemPool);
    const int swords = CountOf(itemPool, RI_PROGRESSIVE_SWORD);
    const int shields = CountOf(itemPool, RI_SHIELD_HERO);
    const int ocarinas = CountOf(itemPool, RI_OCARINA);
    const int songs = CountOf(itemPool, RI_SONG_TIME);
    printf("[TEST] mm-paired-start: (3) MM's pool: %zu items over %zu checks; progressive swords %d, Hero's Shield "
           "%d, Ocarina %d, Song of Time %d (expected 3/1/0/0)\n",
           itemPool.size(), checkPool.size(), swords, shields, ocarinas, songs);
    Check(swords == 3,
          "(3) MM's pool does not hold three progressive swords (the Kokiri Sword beside the two Smithy rows)");
    Check(shields == 1, "(3) MM's pool does not hold the Hero's Shield");
    Check(ocarinas == 0 && songs == 0,
          "(3) MM's pool still holds the Skull Kid roof's Ocarina or Song of Time, which the world starts with");

    // ------------------------------------------------------------------------
    // 4. The world holds every pool copy.
    // ------------------------------------------------------------------------
    int swordsOnOoT = 0;
    int shieldsOnOoT = 0;
    const int swordsPlaced = PlacedInWorld(RI_PROGRESSIVE_SWORD, &swordsOnOoT);
    const int shieldsPlaced = PlacedInWorld(RI_SHIELD_HERO, &shieldsOnOoT);
    printf("[TEST] mm-paired-start: (4) placed in the world: progressive swords %d (%d crossed onto OoT hosts), "
           "Hero's Shield %d (%d crossed) (expected %d/%d)\n",
           swordsPlaced, swordsOnOoT, shieldsPlaced, shieldsOnOoT, swords, shields);
    Check(swordsPlaced == swords, "(4) not every progressive sword of MM's pool was placed in the world");
    Check(shieldsPlaced == shields, "(4) the Hero's Shield of MM's pool was not placed in the world");

    // Leave the config file as this row found it: the block it created goes.
    EraseStartingItemsBlock(config);

    if (sFailures != 0) {
        printf("[TEST] FAIL: %d contract(s) broken\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: a paired MM half starts with exactly the Ocarina of Time and the Song of Time whatever "
           "2Ship's config says; the Kokiri Sword and the Hero's Shield are placed as MM pool items (#879)\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
