/**
 * @file mm_logic_gates_test.cpp
 * @brief #1168's Majora's Mask siblings: two logic gates 2Ship2Harkinian added
 *        after our import base, driven through MM's REAL reachability crawl.
 *        CTest row `MMLogicGates` (label `rando`), test name `mm-logic-gates`,
 *        registered in src/common/test_runner.cpp.
 *
 * WHY THE CRAWL AND NOT THE LAMBDA. Both defects come from how the crawl
 * propagates time: an exit is evaluated against the source region's WHOLE time
 * mask and, when it passes, the whole mask flows into the target. Milk Road's
 * exit to Romani Ranch is `AFTER(TIME_DAY3_AM_06_00) || boulder`, so a Milk Road
 * reachable at every time hands Romani Ranch every time slice — Night 1 included
 * — without the boulder ever breaking; the Milk Bar's door is open by day, so the
 * bar is handed the night windows too. A probe of the check lambda alone, under a
 * hand-picked time mask, would not show that; CrawlReachableRegions +
 * EvaluateReachableChecks (the tracker's and the combo engine's crawl) does.
 *
 *  R. Romani Ranch (2S2H 9b699dee1). From Milk Road with the Ocarina of Time,
 *     Epona's Song and the Bow but no way to break the boulder (no Goron Mask, no
 *     Powder Keg): the ranch and the barn ARE reached (by the Day 3 road, which is
 *     the point), and none of the six cows, Epona's Song or the alien drop is.
 *     With the Goron Mask and a Powder Keg all eight are.
 *  C. The Circus Leader's Mask (2S2H cc5d87143). From East Clock Town with the
 *     Ocarina of Time, the Deku, Goron and Zora Masks and every ocarina button,
 *     but no Romani's Mask: the Milk Bar IS reached and the Circus Leader's check
 *     is not. With Romani's Mask it is.
 *
 * Every "not reached" leg is paired with a "reached" leg on the same save plus
 * one thing, and each names a region or check that must be reached, so a probe
 * that reaches nothing fails instead of passing.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <set>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/Logic/Logic.h"

extern "C" {
#include "variables.h"
}

// games/mm/2s2h/GameExports_SingleExe.cpp (see mm_trick_bindings_test.cpp).
extern "C" void MM_Rando_InitCore(void);

namespace {

int sFailures = 0;

void GateFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[MM-LOGIC-GATES] FAIL: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    sFailures++;
}

/** Empty inventory (every slot ITEM_NONE, not zero: a zeroed slot reads as
 *  holding item 0x00, the Ocarina of Time), options zeroed (no clock shuffle),
 *  human form. */
void ResetSave() {
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    memset(gSaveContext.save.saveInfo.inventory.items, ITEM_NONE, sizeof(gSaveContext.save.saveInfo.inventory.items));
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
}

void Give(ItemId itemId) {
    INV_CONTENT(itemId) = itemId;
}

void GiveEveryOcarinaButton() {
    for (int i = RANDO_INF_OBTAINED_OCARINA_BUTTON_A; i <= RANDO_INF_OBTAINED_OCARINA_BUTTON_C_UP; i++) {
        Flags_SetRandoInf((RandoInf)i);
    }
}

struct Reach {
    Rando::Logic::ReachabilityCrawl crawl;
    std::set<RandoCheckId> checks;
};

Reach CrawlFrom(s32 entrance) {
    Reach reach;
    reach.crawl = Rando::Logic::CrawlReachableRegions(entrance);
    reach.checks = Rando::Logic::EvaluateReachableChecks(reach.crawl);
    return reach;
}

void RequireRegion(const char* leg, const Reach& reach, RandoRegionId region, const char* name) {
    if (!reach.crawl.reachableRegions.contains(region)) {
        GateFail("%s: %s is not reached, so this leg probes nothing", leg, name);
    }
}

void ExpectCheck(const char* leg, const Reach& reach, RandoCheckId check, const char* name, bool expected,
                 const char* why) {
    const bool got = reach.checks.contains(check);
    if (got != expected) {
        GateFail("%s: %s is %s: %s", leg, name, got ? "reachable" : "unreachable", why);
        return;
    }
    printf("[TEST]   ok: %s: %s %s\n", leg, name, expected ? "reachable" : "unreachable");
}

struct RanchCheck {
    RandoCheckId check;
    const char* name;
};

const RanchCheck kRanchChecks[] = {
    { RC_ROMANI_RANCH_BARN_COW_LEFT, "barn cow (left)" },
    { RC_ROMANI_RANCH_BARN_COW_MIDDLE, "barn cow (middle)" },
    { RC_ROMANI_RANCH_BARN_COW_RIGHT, "barn cow (right)" },
    { RC_ROMANI_RANCH_FIELD_COW_ENTRANCE, "field cow (entrance)" },
    { RC_ROMANI_RANCH_FIELD_COW_NEAR_HOUSE_BACK, "field cow (near house, back)" },
    { RC_ROMANI_RANCH_FIELD_COW_NEAR_HOUSE_FRONT, "field cow (near house, front)" },
    { RC_ROMANI_RANCH_EPONAS_SONG, "Epona's Song" },
    { RC_ENEMY_DROP_ALIEN, "alien drop" },
};

void ArmRanchSave() {
    ResetSave();
    Give(ITEM_OCARINA_OF_TIME);
    Give(ITEM_BOW);
    SET_QUEST_ITEM(QUEST_SONG_EPONA);
    GiveEveryOcarinaButton();
}

void ArmMilkBarSave() {
    ResetSave();
    Give(ITEM_OCARINA_OF_TIME);
    Give(ITEM_MASK_DEKU);
    Give(ITEM_MASK_GORON);
    Give(ITEM_MASK_ZORA);
    GiveEveryOcarinaButton();
}

} // namespace

extern "C" int MM_LogicGates_RunHeadless(void) {
    printf("[TEST] mm-logic-gates: Romani Ranch's cows, Epona's Song and the alien drop need the Milk Road boulder "
           "broken, and the Circus Leader needs Romani's Mask (#1168)\n");
    sFailures = 0;

    // Populate the region graph (file-scope ShipInit registrars; see
    // mm_trick_bindings_test.cpp for why CORE only). Idempotent.
    MM_Rando_InitCore();
    if (gRegEditor == NULL) {
        static RegEditor sProbeRegEditor = {};
        gRegEditor = &sProbeRegEditor;
    }

    // Heap, not stack: the port's SaveContext is tens of kilobytes.
    auto saved = std::make_unique<SaveContext>();
    memcpy(saved.get(), &gSaveContext, sizeof(SaveContext));
    const uint64_t savedTime = Rando::Logic::gCurrentRegionTime;

    // ---- R: Romani Ranch behind the Milk Road boulder (2S2H 9b699dee1) ------
    const s32 kMilkRoad = ENTRANCE(MILK_ROAD, 0);
    {
        ArmRanchSave();
        const Reach reach = CrawlFrom(kMilkRoad);
        const char* leg = "R1 no Goron Mask, no Powder Keg";
        RequireRegion(leg, reach, RR_MILK_ROAD, "Milk Road");
        RequireRegion(leg, reach, RR_ROMANI_RANCH, "Romani Ranch");
        RequireRegion(leg, reach, RR_RANCH_BARN, "the ranch barn");
        ExpectCheck(leg, reach, RC_ROMANI_RANCH_FIELD_LARGE_CRATE, "the field crate", true,
                    "the ranch's unconditional crate is not reached, so the crawl reached nothing");
        for (const RanchCheck& rc : kRanchChecks) {
            ExpectCheck(leg, reach, rc.check, rc.name, false,
                        "reached without breaking the boulder: the ranch is open only from Day 3, after the cows' "
                        "Night 1 window and Romani's Day 1 lesson");
        }
    }
    {
        ArmRanchSave();
        Give(ITEM_MASK_GORON);
        Give(ITEM_POWDER_KEG);
        const Reach reach = CrawlFrom(kMilkRoad);
        const char* leg = "R2 Goron Mask and Powder Keg";
        RequireRegion(leg, reach, RR_ROMANI_RANCH, "Romani Ranch");
        for (const RanchCheck& rc : kRanchChecks) {
            ExpectCheck(leg, reach, rc.check, rc.name, true,
                        "not reached with the boulder breakable on Day 1, so the gate refuses its own route");
        }
    }

    // ---- C: the Circus Leader's Mask needs Romani's Mask (2S2H cc5d87143) --
    const s32 kEastClockTown = ENTRANCE(EAST_CLOCK_TOWN, 11);
    {
        ArmMilkBarSave();
        const Reach reach = CrawlFrom(kEastClockTown);
        const char* leg = "C1 no Romani's Mask";
        RequireRegion(leg, reach, RR_CLOCK_TOWN_EAST, "East Clock Town");
        RequireRegion(leg, reach, RR_MILK_BAR, "the Milk Bar");
        ExpectCheck(leg, reach, RC_MILK_BAR_CIRCUS_LEADER_MASK, "the Circus Leader's Mask", false,
                    "reached without Romani's Mask: the bar's night show is behind the mask, its day door is not");
    }
    {
        ArmMilkBarSave();
        Give(ITEM_MASK_ROMANI);
        const Reach reach = CrawlFrom(kEastClockTown);
        ExpectCheck("C2 with Romani's Mask", reach, RC_MILK_BAR_CIRCUS_LEADER_MASK, "the Circus Leader's Mask", true,
                    "not reached with Romani's Mask, the ocarina, the three masks and every button");
    }

    memcpy(&gSaveContext, saved.get(), sizeof(SaveContext));
    Rando::Logic::gCurrentRegionTime = savedTime;

    if (sFailures == 0) {
        printf("[TEST] PASS: mm-logic-gates\n");
    } else {
        printf("[TEST] FAIL: mm-logic-gates: %d leg(s) failed\n", sFailures);
    }
    return sFailures;
}

#endif // RSBS_SINGLE_EXECUTABLE
