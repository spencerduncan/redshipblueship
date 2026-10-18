/**
 * @file oot_mask_quest_paired_test.cpp
 * A paired world's Mask Quest and Mask Shop Hint (#973), run by the rando row
 * `rando-mask-quest-paired`.
 *
 * The Happy Mask Shop door is the OoT<->MM crossing (entrance.cpp pins it out of
 * every shuffle pool), so the shop's room never loads. SoH's Mask Quest
 * "Vanilla" credits borrowing the masks there and "Completed" credits them once
 * the shop opens, and the Mask Shop Hint is the sign in that room. A paired
 * creation therefore generates Mask Quest as "Shuffle" and the hint off
 * (3drando/playthrough.cpp), as OoTMM does: its Mask Shop is only the crossing
 * and it shuffles all eight OoT masks.
 *
 * The row runs a real headless paired generation for each authored mode a player
 * can arrive with ("Vanilla", the option's default, and "Completed", the shipped
 * presets' value and the operator's 2026-10-17 playtest seed), with the Mask
 * Shop Hint authored on, and checks:
 *   M1 the generated settings: Mask Quest "Shuffle", Mask Shop Hint off;
 *   M2 the pool: each of the eight OoT masks is in the world, in the general-pass
 *      pool the single bag takes or already placed;
 *   M3 the logic, over the REGISTERED OoT engine, every placed item withheld and
 *      everything else in the world assumed: the Deku Theater is reached, yet
 *      neither of its checks is until its own mask is assumed; the Skull Mask
 *      opens only its check, and the Mask of Truth then opens the other;
 *   M4 the menu: each row shows the generated value (it is the row's default),
 *      a stored value (a preset's) is cleared back to it, and the callbacks never
 *      store one, which would take a config off the shipped defaults the
 *      integration rows require.
 * Every check runs and reports, and the row fails if any did, so one red run
 * shows every leg a change breaks.
 *
 * RED HALVES, run before landing: without playthrough.cpp's #973 block, M1 and
 * M2 fail under both modes and M3 fails over the "Completed" world (both checks
 * are reached before any mask is assumed); without the two settings.cpp
 * callbacks, or with Mask Quest's default left at "Vanilla", M4 fails.
 */

#include <cstdio>
#include <vector>

#include <libultraship/bridge.h>

#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/settings.h"

#include "combo_logic.h"      // Combo_Logic_GetEngine, the engine vtable
#include "combo_single_bag.h" // OoT_ComboLogic_ExportPool

// games/oot/soh/Enhancements/randomizer/3drando/menu.cpp: the headless generation
// bridge the RandoGen rows use.
extern "C" int Rando_HeadlessSeedTest(const char* seedStr);

namespace {

int sFailures = 0;

/** A check that reports and counts, so every leg runs. */
#define MQP_CHECK(cond, msg)                                              \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            sFailures++;                                                  \
        }                                                                 \
    } while (0)

/** A precondition: without it the legs after it would prove nothing. */
#define MQP_REQUIRE(cond, msg)                                            \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            sFailures++;                                                  \
            return;                                                       \
        }                                                                 \
    } while (0)

const char* const kMaskQuestCVar = CVAR_RANDOMIZER_SETTING("CompleteMaskQuest");
const char* const kMaskShopHintCVar = CVAR_RANDOMIZER_SETTING("MaskShopHint");

const RandomizerGet kMasks[8] = {
    RG_KEATON_MASK, RG_SKULL_MASK, RG_SPOOKY_MASK, RG_BUNNY_HOOD,
    RG_GORON_MASK,  RG_ZORA_MASK,  RG_GERUDO_MASK, RG_MASK_OF_TRUTH,
};

bool IsRealItem(RandomizerGet rg) {
    return rg > RG_NONE && rg < RG_MAX && Rando::StaticData::RetrieveItem(rg).GetRandomizerGet() == rg;
}

/** The general-pass pool the single bag takes (export source 0). */
std::vector<uint16_t> PoolRows() {
    const int total = OoT_ComboLogic_ExportPool(0, nullptr, nullptr, nullptr, 0);
    std::vector<uint16_t> rows(total > 0 ? (size_t)total : 0);
    if (total > 0) {
        OoT_ComboLogic_ExportPool(0, rows.data(), nullptr, nullptr, total);
    }
    return rows;
}

/** Copies of `rg` in the world: in the pool, or at a location already. */
int CopiesInWorld(RandomizerGet rg, const std::vector<uint16_t>& pool) {
    int copies = 0;
    for (const uint16_t row : pool) {
        copies += (row == (uint16_t)rg) ? 1 : 0;
    }
    auto ctx = Rando::Context::GetInstance();
    for (const RandomizerCheck rc : ctx->allLocations) {
        copies += (ctx->GetItemLocation(rc)->GetPlacedRandomizerGet() == rg) ? 1 : 0;
    }
    return copies;
}

void ExpandToFixpoint(const ComboLogicEngine* e) {
    for (int i = 0; i < 16 && e->expand(e->self) != 0; i++) {}
}

/** M1 and M2 over one generation authored with `authoredMode`. Returns false
 *  when there is no world to check the later legs against. */
bool GenerateAndCheck(uint8_t authoredMode, const char* modeName) {
    CVarSetInteger(kMaskQuestCVar, authoredMode);
    CVarSetInteger(kMaskShopHintCVar, 1);
    const int genRc = Rando_HeadlessSeedTest("RSBSMASKQUEST973");
    CVarClear(kMaskQuestCVar);
    CVarClear(kMaskShopHintCVar);
    auto ctx = Rando::Context::GetInstance();
    if (genRc != 0 || ctx == nullptr) {
        printf("[TEST] FAIL: headless paired generation (authored %s) failed, rc=%d (%s:%d)\n", modeName, genRc,
               __FILE__, __LINE__);
        sFailures++;
        return false;
    }

    // ---- M1: the generated settings ----------------------------------------
    const int mode = (int)ctx->GetOption(RSK_MASK_QUEST).Get();
    const int hint = (int)ctx->GetOption(RSK_MASK_SHOP_HINT).Get();
    printf("[TEST] M1 (authored %s, Mask Shop Hint on): generated Mask Quest %d, Mask Shop Hint %d\n", modeName, mode,
           hint);
    MQP_CHECK(mode == RO_MASK_QUEST_SHUFFLE,
              "M1: a paired world kept a Mask Quest that needs the Happy Mask Shop, whose door leads to Termina");
    MQP_CHECK(hint == RO_GENERIC_OFF,
              "M1: a paired world kept the Mask Shop Hint, whose sign is inside the Happy Mask Shop");

    // ---- M2: the masks are in the world ------------------------------------
    const std::vector<uint16_t> pool = PoolRows();
    if (pool.empty()) {
        printf("[TEST] FAIL: M2: the general-pass pool is empty, so the pool leg would be vacuous (%s:%d)\n", __FILE__,
               __LINE__);
        sFailures++;
        return true;
    }
    int missing = 0;
    for (const RandomizerGet mask : kMasks) {
        if (CopiesInWorld(mask, pool) < 1) {
            printf("[TEST] FAIL: M2 (authored %s): %s is in neither the pool (%d rows) nor at any location (%s:%d)\n",
                   modeName, Rando::StaticData::RetrieveItem(mask).GetName().GetEnglish().c_str(), (int)pool.size(),
                   __FILE__, __LINE__);
            missing++;
        }
    }
    sFailures += missing;
    if (missing == 0) {
        printf("[TEST] M2 (authored %s): all eight OoT masks are in the world (pool %d rows)\n", modeName,
               (int)pool.size());
    }
    return true;
}

/** M3 over the world the last generation left. */
void CheckTheaterNeedsTheMasks() {
    auto ctx = Rando::Context::GetInstance();
    const ComboLogicEngine* e = Combo_Logic_GetEngine(GAME_OOT);
    MQP_REQUIRE(e != nullptr, "M3: no OoT combo-logic engine is registered");

    // Everything in the world but the two theater masks: the pool rows and every
    // placed item. Placed items are also withheld, so the round's inventory is
    // exactly this set plus the starting inventory, whatever the fill put where.
    std::vector<uint16_t> assumed = PoolRows();
    std::vector<RandomizerCheck> placedHosts;
    for (const RandomizerCheck rc : ctx->allLocations) {
        const RandomizerGet placed = ctx->GetItemLocation(rc)->GetPlacedRandomizerGet();
        if (placed == RG_NONE) {
            continue;
        }
        placedHosts.push_back(rc);
        assumed.push_back((uint16_t)placed);
    }
    if (e->beginQuery(e->self) == 0) {
        // combo_logic.h: endQuery is still called when beginQuery refuses.
        e->endQuery(e->self);
        printf("[TEST] FAIL: M3: the OoT engine refused to open a round (%s:%d)\n", __FILE__, __LINE__);
        sFailures++;
        return;
    }
    for (const RandomizerCheck rc : placedHosts) {
        e->withholdHost(e->self, (uint16_t)rc);
    }
    int assumedCount = 0;
    for (const uint16_t id : assumed) {
        if (id == (uint16_t)RG_SKULL_MASK || id == (uint16_t)RG_MASK_OF_TRUTH || !IsRealItem((RandomizerGet)id)) {
            continue;
        }
        e->assumeOwnItem(e->self, id);
        assumedCount++;
    }
    ExpandToFixpoint(e);
    const bool theater = RegionTable(RR_DEKU_THEATER)->Child();
    const bool skullBefore = e->checkReached(e->self, (uint16_t)RC_DEKU_THEATER_SKULL_MASK) != 0;
    const bool truthBefore = e->checkReached(e->self, (uint16_t)RC_DEKU_THEATER_MASK_OF_TRUTH) != 0;
    printf("[TEST] M3: %d items assumed, %d hosts withheld: Deku Theater reached %d, Skull Mask check %d, Mask of "
           "Truth check %d\n",
           assumedCount, (int)placedHosts.size(), theater ? 1 : 0, skullBefore ? 1 : 0, truthBefore ? 1 : 0);

    e->assumeOwnItem(e->self, (uint16_t)RG_SKULL_MASK);
    ExpandToFixpoint(e);
    const bool skullWithSkull = e->checkReached(e->self, (uint16_t)RC_DEKU_THEATER_SKULL_MASK) != 0;
    const bool truthWithSkull = e->checkReached(e->self, (uint16_t)RC_DEKU_THEATER_MASK_OF_TRUTH) != 0;
    printf("[TEST] M3: + Skull Mask: Skull Mask check %d, Mask of Truth check %d\n", skullWithSkull ? 1 : 0,
           truthWithSkull ? 1 : 0);

    e->assumeOwnItem(e->self, (uint16_t)RG_MASK_OF_TRUTH);
    ExpandToFixpoint(e);
    const bool truthWithBoth = e->checkReached(e->self, (uint16_t)RC_DEKU_THEATER_MASK_OF_TRUTH) != 0;
    printf("[TEST] M3: + Mask of Truth: Mask of Truth check %d\n", truthWithBoth ? 1 : 0);
    e->endQuery(e->self);

    MQP_REQUIRE(theater, "M3: the Deku Theater itself is not reached with everything but its masks, so the mask "
                         "legs would prove nothing");
    MQP_CHECK(!skullBefore, "M3: the Skull Mask check is reached before the Skull Mask is assumed - the logic credits "
                            "a mask the Happy Mask Shop would lend, and its room never loads");
    MQP_CHECK(!truthBefore, "M3: the Mask of Truth check is reached before the Mask of Truth is assumed - the logic "
                            "credits a mask the Happy Mask Shop would lend, and its room never loads");
    MQP_CHECK(skullWithSkull, "M3: the Skull Mask did not open the Skull Mask check");
    MQP_CHECK(!truthWithSkull, "M3: the Skull Mask opened the Mask of Truth check");
    MQP_CHECK(truthWithBoth, "M3: the Mask of Truth did not open the Mask of Truth check");
}

/** M4: the menu rows show the generated value, and the callbacks never store a
 *  setting (a stored one would take a config off the shipped defaults, which the
 *  integration rows refuse). */
void CheckMenuRows() {
    auto settings = Rando::Settings::GetInstance();
    Rando::Option& maskQuest = settings->GetOption(RSK_MASK_QUEST);
    Rando::Option& maskShopHint = settings->GetOption(RSK_MASK_SHOP_HINT);

    // A preset's (or an old config's) stored values are cleared to the defaults.
    CVarSetInteger(kMaskQuestCVar, RO_MASK_QUEST_COMPLETED);
    CVarSetInteger(kMaskShopHintCVar, 1);
    maskQuest.RunCallback();
    maskShopHint.RunCallback();
    const bool storedMode = CVarExists(kMaskQuestCVar);
    const bool storedHint = CVarExists(kMaskShopHintCVar);
    const int shownMode = maskQuest.GetOptionIndex();
    const int shownHint = maskShopHint.GetOptionIndex();
    CVarClear(kMaskQuestCVar);
    CVarClear(kMaskShopHintCVar);
    printf("[TEST] M4: stored Completed / hint on -> stored afterwards %d / %d, the rows show Mask Quest %d, Mask Shop "
           "Hint %d\n",
           storedMode ? 1 : 0, storedHint ? 1 : 0, shownMode, shownHint);
    MQP_CHECK(shownMode == RO_MASK_QUEST_SHUFFLE, "M4: the Mask Quest row does not show \"Shuffle\"");
    MQP_CHECK(shownHint == RO_GENERIC_OFF, "M4: the Mask Shop Hint row does not show off");
    MQP_CHECK(!storedMode && !storedHint, "M4: a stored Mask Quest / Mask Shop Hint value was not cleared");

    // Nothing stored: the callbacks store nothing, and the rows still show the
    // generated values (they are the defaults).
    maskQuest.RunCallback();
    maskShopHint.RunCallback();
    const bool createdMode = CVarExists(kMaskQuestCVar);
    const bool createdHint = CVarExists(kMaskShopHintCVar);
    printf("[TEST] M4: nothing stored -> stored afterwards %d / %d, the rows show Mask Quest %d, Mask Shop Hint %d\n",
           createdMode ? 1 : 0, createdHint ? 1 : 0, (int)maskQuest.GetOptionIndex(),
           (int)maskShopHint.GetOptionIndex());
    MQP_CHECK(!createdMode && !createdHint, "M4: a menu callback stored a randomizer setting the player never set");
    MQP_CHECK(maskQuest.GetOptionIndex() == RO_MASK_QUEST_SHUFFLE && maskShopHint.GetOptionIndex() == RO_GENERIC_OFF,
              "M4: with nothing stored the rows do not show \"Shuffle\" / off");
    CVarClear(kMaskQuestCVar);
    CVarClear(kMaskShopHintCVar);
}

} // namespace

extern "C" int OoT_MaskQuestPaired_RunGenerated(void) {
    printf("[TEST] rando-mask-quest-paired (OoT): a paired world shuffles the Happy Mask Shop's masks and drops its "
           "sign hint, because the shop's door leads to Termina (#973)\n");
    sFailures = 0;
    GenerateAndCheck(RO_MASK_QUEST_VANILLA, "Vanilla");
    if (GenerateAndCheck(RO_MASK_QUEST_COMPLETED, "Completed")) {
        CheckTheaterNeedsTheMasks();
    }
    CheckMenuRows();
    if (sFailures != 0) {
        printf("[TEST] FAIL: rando-mask-quest-paired (OoT): %d check(s) failed\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: rando-mask-quest-paired (OoT)\n");
    return 0;
}
