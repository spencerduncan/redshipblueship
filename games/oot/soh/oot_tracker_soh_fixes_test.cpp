/**
 * @file oot_tracker_soh_fixes_test.cpp
 * ROM-free lock for #1420 (upstream gap SFX-26: Ship of Harkinian's tracker
 * fixes, which reach the unified Check Tracker through its OoT pane). Row
 * OoTTrackerSohFixes (CTest label "redship", dispatch "oot-tracker-soh-fixes").
 * Display-free: the shared Ship::Context bring-up for the CVar store, a heap
 * Rando::Context and OoT's region graph (Rando_InitRegionGraphForTest), no
 * archive and no generation. The legs that need a generated world run in the
 * rando-tier row TrackerOoTPanelList (oot_tracker_reachability_test.cpp,
 * OoT_TrackerSohFixes_RunPaired).
 *
 * 1  Locations exclusion (SoH #6447): with Ganon's Boss Key at Vanilla, the
 *    Ganon's Tower boss key chest is offered for exclusion under Triforce Hunt,
 *    whether OoT's own row turns the hunt on or the [Both Games] Triforce Hunt
 *    goal does. With neither it is not (the control: the chest holds the key).
 * 2  Where Available Checks starts (SoH #6165, #6237, #7300): a saved entrance
 *    index names Link's region, an index the graph does not know steps back to
 *    the scene's known entrance, an index outside the table names Root, and a
 *    graph rebuilt without its entrance stamps (what a generation without
 *    entrance shuffle leaves) names Root rather than a freed exit's region. The
 *    link age and night flag name the age and time.
 * 3  OoT's item pane during the end credits (SoH #7298): OoT's live source
 *    answers under GAMEMODE_END_CREDITS for a slot's file (0-2), and not for the
 *    debug save, the title screen or file select.
 * 4  Entrance tracker data (SoH #7041, #6725): the two Gerudo Fortress grottoes'
 *    Thieves' Hideout doors carry the right names both ways; the owl drops are
 *    filed under Death Mountain Trail and Lake Hylia, so the first group is
 *    "Spawns/Warp Songs"; and every row sits in its area's block.
 * 5  The pause screen's list (SoH #6184): Crawl, Climb, Grab, Open Chests, Swim,
 *    the ten bean souls and Roc's Feather appear, in SoH's order, exactly when
 *    their shuffles are on.
 *
 * Every check runs and reports; the entry point returns the failure count. Every
 * key, option and save field a leg changes is put back.
 */

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <libultraship/bridge.h>

#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/entrance.h"
#include "soh/Enhancements/randomizer/randomizer_check_objects.h"
#include "soh/Enhancements/randomizer/randomizer_entrance_tracker.h"

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>

#include "cvar_shared_keys.h" // src/common: RSBS_CVAR_COMBO_RANDO_GOAL
#include "foreign_items.h"    // src/common: RSBS_COMBO_GOAL_TRIFORCE_HUNT

extern "C" {
// entrance.cpp (#661): OoT's region graph over a heap Rando::Context, entrances stamped.
int Rando_InitRegionGraphForTest(void);
// randomizer_check_tracker.cpp (#1420): where Available Checks starts.
int OoT_CheckTracker_TestStartingRegion(int32_t savedEntrance);
int OoT_CheckTracker_TestStartingAgeTime(int32_t linkAge, int32_t nightFlag);
// ItemAdapterSingleExe.cpp (#1420): OoT's live item source under a game mode and file number.
int OoT_ItemAdapter_TestLiveSaveUnder(int which, int fileNum);
// randomizer_entrance_tracker.cpp (#1420): a group's heading, and the first row out of its area's block.
const char* OoT_EntranceTracker_TestGroupName(int group);
int OoT_EntranceTracker_TestFirstRowOutOfAreaOrder(void);
// kaleido.cpp (#1420): the pause-screen list's lines over the live seed context.
int OoT_Kaleido_TestEntryLines(char* out, int cap);
}

namespace {

int sFailures = 0;

#define OTSF_CHECK(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            sFailures++;                                                  \
        }                                                                 \
    } while (0)

// ---- 1. the Locations exclusion list --------------------------------------------

const char* const kTriforceHuntCVar = CVAR_RANDOMIZER_SETTING("TriforceHunt");
const char* const kGanonBossKeyCVar = CVAR_RANDOMIZER_SETTING("ShuffleGanonBossKey");

/** Whether the exclusion list offers the tower chest with Ganon's Boss Key at
 *  Vanilla, OoT's hunt row at `hunt` and the [Both Games] goal at `goal`
 *  (a negative value leaves the key unset). */
bool OtsfTowerChestOffered(int hunt, int goal) {
    CVarSetInteger(kGanonBossKeyCVar, RO_GANON_BOSS_KEY_VANILLA);
    if (hunt >= 0) {
        CVarSetInteger(kTriforceHuntCVar, hunt);
    } else {
        CVarClear(kTriforceHuntCVar);
    }
    if (goal >= 0) {
        CVarSetInteger(RSBS_CVAR_COMBO_RANDO_GOAL, goal);
    } else {
        CVarClear(RSBS_CVAR_COMBO_RANDO_GOAL);
    }
    RandomizerCheckObjects::UpdateImGuiVisibility();
    return Rando::Context::GetInstance()->GetItemLocation(RC_GANONS_TOWER_BOSS_KEY_CHEST)->IsVisible();
}

void OtsfLegExclusion() {
    const bool off = OtsfTowerChestOffered(-1, -1);
    const bool ownWin = OtsfTowerChestOffered(RO_TRIFORCE_HUNT_WIN, -1);
    const bool ownGbk = OtsfTowerChestOffered(RO_TRIFORCE_HUNT_GBK, -1);
    const bool comboGoal = OtsfTowerChestOffered(RO_TRIFORCE_HUNT_OFF, (int)RSBS_COMBO_GOAL_TRIFORCE_HUNT);
    printf("[TEST] 1 (exclusion list, Ganon's Boss Key Vanilla): tower chest offered with no hunt %d, OoT's hunt Win "
           "%d, OoT's hunt Ganon's Boss Key %d, [Both Games] Triforce Hunt goal %d\n",
           off ? 1 : 0, ownWin ? 1 : 0, ownGbk ? 1 : 0, comboGoal ? 1 : 0);
    OTSF_CHECK(!off, "1: with no Triforce Hunt the exclusion list offers the Ganon's Tower boss key chest, which holds "
                     "the boss key at Vanilla (the control)");
    OTSF_CHECK(ownWin, "1: OoT's Triforce Hunt (Win) leaves the boss key out of the pool, yet the exclusion list does "
                       "not offer the tower chest (SoH #6447)");
    OTSF_CHECK(ownGbk, "1: OoT's Triforce Hunt (Ganon's Boss Key) leaves the boss key out of the pool, yet the "
                       "exclusion list does not offer the tower chest");
    OTSF_CHECK(comboGoal, "1: the [Both Games] Triforce Hunt goal generates OoT's own hunt (#1389), yet the "
                          "exclusion list does not offer the tower chest");
    CVarClear(kTriforceHuntCVar);
    CVarClear(kGanonBossKeyCVar);
    CVarClear(RSBS_CVAR_COMBO_RANDO_GOAL);
    RandomizerCheckObjects::UpdateImGuiVisibility();
}

// ---- 2. where Available Checks starts --------------------------------------------

void OtsfLegStartingRegion() {
    const int dekuTree = OoT_CheckTracker_TestStartingRegion(ENTR_DEKU_TREE_ENTRANCE);
    const bool stepIsUnknown = Rando::EntranceShuffler::GetEntranceByIndex(ENTR_DEKU_TREE_0_1) == nullptr;
    const int dekuTreeStep = OoT_CheckTracker_TestStartingRegion(ENTR_DEKU_TREE_0_1);
    const int linksHouse = OoT_CheckTracker_TestStartingRegion(ENTR_LINKS_HOUSE_CHILD_SPAWN);
    const int below = OoT_CheckTracker_TestStartingRegion(-1);
    const int past = OoT_CheckTracker_TestStartingRegion(ENTR_MAX);
    printf("[TEST] 2 (starting region): Deku Tree entrance -> %d (want %d); its unknown spawn 1 (unknown %d) -> %d; "
           "child spawn -> %d (want %d); -1 -> %d, ENTR_MAX -> %d (want Root %d)\n",
           dekuTree, (int)RR_DEKU_TREE_ENTRYWAY, stepIsUnknown ? 1 : 0, dekuTreeStep, linksHouse,
           (int)RR_KF_LINKS_HOUSE, below, past, (int)RR_ROOT);
    OTSF_CHECK(dekuTree == (int)RR_DEKU_TREE_ENTRYWAY,
               "2: the Deku Tree's entrance index does not name the Deku Tree entryway");
    OTSF_CHECK(stepIsUnknown, "2: the region graph knows ENTR_DEKU_TREE_0_1, so the step-back leg is vacuous");
    OTSF_CHECK(dekuTreeStep == (int)RR_DEKU_TREE_ENTRYWAY,
               "2: an index the graph does not know (ENTR_DEKU_TREE_0_1) does not step back to the scene's entrance");
    OTSF_CHECK(linksHouse == (int)RR_KF_LINKS_HOUSE, "2: the child spawn does not name Link's house");
    OTSF_CHECK(below == (int)RR_ROOT && past == (int)RR_ROOT, "2: an index outside the entrance table is not Root");

    // The graph a generation without entrance shuffle leaves: rebuilt, never stamped.
    RegionTable_Init();
    const int unstamped = OoT_CheckTracker_TestStartingRegion(ENTR_DEKU_TREE_ENTRANCE);
    Rando_InitRegionGraphForTest();
    const int restamped = OoT_CheckTracker_TestStartingRegion(ENTR_DEKU_TREE_ENTRANCE);
    printf("[TEST] 2 (rebuilt graph): Deku Tree entrance unstamped -> %d, stamped again -> %d\n", unstamped, restamped);
    OTSF_CHECK(unstamped == (int)RR_ROOT, "2: a rebuilt graph without entrance stamps still names a region (a stale "
                                          "entrance, not the live graph's)");
    OTSF_CHECK(restamped == (int)RR_DEKU_TREE_ENTRYWAY, "2: the stamped graph no longer names the Deku Tree entryway");

    const int childDay = OoT_CheckTracker_TestStartingAgeTime(LINK_AGE_CHILD, 0);
    const int childNight = OoT_CheckTracker_TestStartingAgeTime(LINK_AGE_CHILD, 1);
    const int adultDay = OoT_CheckTracker_TestStartingAgeTime(LINK_AGE_ADULT, 0);
    const int adultNight = OoT_CheckTracker_TestStartingAgeTime(LINK_AGE_ADULT, 1);
    printf("[TEST] 2 (age and time): child day %d, child night %d, adult day %d, adult night %d\n", childDay,
           childNight, adultDay, adultNight);
    OTSF_CHECK(childDay == (int)RAT_CHILD_DAY && childNight == (int)RAT_CHILD_NIGHT && adultDay == (int)RAT_ADULT_DAY &&
                   adultNight == (int)RAT_ADULT_NIGHT,
               "2: Link's age and the night flag do not name his age and time of day");
}

// ---- 3. OoT's item pane during the end credits -----------------------------------

void OtsfLegCredits() {
    // OoT_ItemAdapter_TestLiveSaveUnder's `which`: 0 normal, 1 title screen, 2 file select, 3 end credits.
    const int creditsFile1 = OoT_ItemAdapter_TestLiveSaveUnder(3, 0);
    const int creditsFile3 = OoT_ItemAdapter_TestLiveSaveUnder(3, 2);
    const int creditsDebug = OoT_ItemAdapter_TestLiveSaveUnder(3, 0xFF);
    const int creditsPast = OoT_ItemAdapter_TestLiveSaveUnder(3, 3);
    const int title = OoT_ItemAdapter_TestLiveSaveUnder(1, 0);
    const int fileSelect = OoT_ItemAdapter_TestLiveSaveUnder(2, 0);
    const int normal = OoT_ItemAdapter_TestLiveSaveUnder(0, 0);
    printf("[TEST] 3 (live item source): credits file 1 %d, file 3 %d, debug save %d, fileNum 3 %d; title %d, file "
           "select %d; normal with no play state %d (informational)\n",
           creditsFile1, creditsFile3, creditsDebug, creditsPast, title, fileSelect, normal);
    OTSF_CHECK(creditsFile1 == 1 && creditsFile3 == 1,
               "3: during OoT's end credits the item pane does not read the live save (SoH #7298)");
    OTSF_CHECK(creditsDebug == 0 && creditsPast == 0, "3: during the end credits a save that is not a slot's file is "
                                                      "read live");
    OTSF_CHECK(title == 0 && fileSelect == 0, "3: the title screen or file select is read live");
}

// ---- 4. entrance tracker data --------------------------------------------------

bool OtsfRow(int16_t index, const char* source, const char* destination, SpoilerEntranceGroup srcGroup,
             SpoilerEntranceGroup dstGroup) {
    const EntranceData* row = EntranceTracker::GetEntranceData(index);
    if (row == nullptr) {
        printf("[TEST]   entrance 0x%X: no row\n", (unsigned)(uint16_t)index);
        return false;
    }
    printf("[TEST]   entrance 0x%X: \"%s\" -> \"%s\", groups %d -> %d\n", (unsigned)(uint16_t)index,
           row->source.c_str(), row->destination.c_str(), (int)row->srcGroup, (int)row->dstGroup);
    return row->source == source && row->destination == destination && row->srcGroup == srcGroup &&
           row->dstGroup == dstGroup;
}

void OtsfLegEntranceData() {
    printf("[TEST] 4 (entrance tracker rows)\n");
    // SoH #7041: the fortress door by the grotto's north side opens on the 1 Torch Cell,
    // the one by its east side on the kitchen corridor.
    OTSF_CHECK(OtsfRow(ENTR_THIEVES_HIDEOUT_1, "GF Near Grotto North", "TH 1 Torch Cell",
                       ENTRANCE_GROUP_GERUDO_FORTRESS, ENTRANCE_GROUP_GERUDO_FORTRESS),
               "4: the hideout's 1 Torch Cell door is not \"GF Near Grotto North\" (SoH #7041)");
    OTSF_CHECK(OtsfRow(ENTR_THIEVES_HIDEOUT_2, "GF Near Grotto East", "TH Kitchen Corridor Lower",
                       ENTRANCE_GROUP_GERUDO_FORTRESS, ENTRANCE_GROUP_GERUDO_FORTRESS),
               "4: the hideout's lower kitchen corridor door is not \"GF Near Grotto East\"");
    OTSF_CHECK(OtsfRow(ENTR_GERUDOS_FORTRESS_2, "TH 1 Torch Cell", "GF Near Grotto North",
                       ENTRANCE_GROUP_GERUDO_FORTRESS, ENTRANCE_GROUP_GERUDO_FORTRESS),
               "4: the 1 Torch Cell's way out does not lead to \"GF Near Grotto North\"");
    OTSF_CHECK(OtsfRow(ENTR_GERUDOS_FORTRESS_3, "TH Kitchen Corridor Lower", "GF Near Grotto East",
                       ENTRANCE_GROUP_GERUDO_FORTRESS, ENTRANCE_GROUP_GERUDO_FORTRESS),
               "4: the lower kitchen corridor's way out does not lead to \"GF Near Grotto East\"");
    // SoH #6725: the owl drops are filed where the owl flies from and to.
    OTSF_CHECK(OtsfRow(ENTR_KAKARIKO_VILLAGE_OWL_DROP, "DMT Owl Flight", "Kakariko Village Owl Drop",
                       ENTRANCE_GROUP_DEATH_MOUNTAIN_TRAIL, ENTRANCE_GROUP_KAKARIKO),
               "4: the Death Mountain Trail owl is not filed under Death Mountain Trail and Kakariko (SoH #6725)");
    OTSF_CHECK(OtsfRow(ENTR_HYRULE_FIELD_OWL_DROP, "LH Owl Flight", "Hyrule Field Owl Drop", ENTRANCE_GROUP_LAKE_HYLIA,
                       ENTRANCE_GROUP_HYRULE_FIELD),
               "4: the Lake Hylia owl is not filed under Lake Hylia and Hyrule Field");
    const char* oneWay = OoT_EntranceTracker_TestGroupName(ENTRANCE_GROUP_ONE_WAY);
    const int outOfOrder = OoT_EntranceTracker_TestFirstRowOutOfAreaOrder();
    printf("[TEST] 4: first group \"%s\"; first row out of its area's block %d\n",
           oneWay != nullptr ? oneWay : "(none)", outOfOrder);
    OTSF_CHECK(oneWay != nullptr && strcmp(oneWay, "Spawns/Warp Songs") == 0,
               "4: the first group still promises owls, which are filed under their areas now");
    OTSF_CHECK(outOfOrder < 0, "4: an entrance row sits outside its area's block, so the by-area list files it under "
                               "the wrong heading");
}

// ---- 5. the pause screen's list ---------------------------------------------------

const RandomizerSettingKey kPauseKeys[] = {
    RSK_SHUFFLE_CRAWL, RSK_SHUFFLE_CLIMB,      RSK_SHUFFLE_GRAB, RSK_SHUFFLE_OPEN_CHEST,
    RSK_SHUFFLE_SWIM,  RSK_SHUFFLE_BEAN_SOULS, RSK_ROCS_FEATHER,
};

std::vector<std::string> OtsfPauseLines() {
    static char buf[8192];
    std::vector<std::string> lines;
    const int count = OoT_Kaleido_TestEntryLines(buf, (int)sizeof(buf));
    if (count < 0) {
        return lines;
    }
    std::string all(buf);
    size_t start = 0;
    for (int i = 0; i < count; i++) {
        const size_t end = all.find('\n', start);
        lines.push_back(all.substr(start, end == std::string::npos ? std::string::npos : end - start));
        start = end == std::string::npos ? all.size() : end + 1;
    }
    return lines;
}

void OtsfLegPauseList() {
    auto ctx = Rando::Context::GetInstance();
    uint8_t before[sizeof(kPauseKeys) / sizeof(kPauseKeys[0])];
    for (size_t i = 0; i < sizeof(kPauseKeys) / sizeof(kPauseKeys[0]); i++) {
        before[i] = ctx->GetOption(kPauseKeys[i]).Get();
    }
    // The speak entries sit between Grab and Open Chests (SoH's order); off, so the new rows are one block.
    const uint8_t speakBefore = ctx->GetOption(RSK_SHUFFLE_SPEAK).Get();
    ctx->GetOption(RSK_SHUFFLE_SPEAK).Set(0);

    for (const RandomizerSettingKey key : kPauseKeys) {
        ctx->GetOption(key).Set(0);
    }
    const std::vector<std::string> off = OtsfPauseLines();
    for (const RandomizerSettingKey key : kPauseKeys) {
        ctx->GetOption(key).Set(1);
    }
    const std::vector<std::string> on = OtsfPauseLines();

    const std::vector<std::string> expected = {
        "Crawl",
        "Climb",
        "Grab",
        "Open Chests",
        "Swim",
        "Death Mountain Crater",
        "Death Mountain Trail",
        "Desert Colossus",
        "Gerudo Valley",
        "Graveyard",
        "Kokiri Forest",
        "Lake Hylia",
        "Lost Woods Bridge",
        "Lost Woods",
        "Zora's River",
        "Roc's Feather",
    };
    bool tailMatches = on.size() >= expected.size();
    for (size_t i = 0; tailMatches && i < expected.size(); i++) {
        tailMatches = on[on.size() - expected.size() + i] == expected[i];
    }
    int offHits = 0;
    for (const std::string& line : off) {
        for (const char* name : { "Crawl", "Climb", "Grab", "Open Chests", "Swim", "Roc's Feather" }) {
            if (line == name) {
                offHits++;
            }
        }
    }
    printf("[TEST] 5 (pause screen list): %d lines with the seven shuffles off, %d on; the last %d on match SoH's "
           "order %d; new names listed with the shuffles off %d\n",
           (int)off.size(), (int)on.size(), (int)expected.size(), tailMatches ? 1 : 0, offHits);
    for (size_t i = on.size() > expected.size() ? on.size() - expected.size() : 0; i < on.size(); i++) {
        printf("[TEST]   on[%d] \"%s\"\n", (int)i, on[i].c_str());
    }
    OTSF_CHECK(!off.empty(),
               "5: the pause screen's list is empty with the shuffles off (Greg is always listed), so the "
               "list was not read");
    OTSF_CHECK(on.size() == off.size() + expected.size(),
               "5: turning the seven shuffles on does not add exactly their sixteen lines (SoH #6184)");
    OTSF_CHECK(tailMatches, "5: the new lines are not Crawl, Climb, Grab, Open Chests, Swim, the ten bean souls and "
                            "Roc's Feather, in SoH's order");
    OTSF_CHECK(offHits == 0, "5: a new line is listed while its shuffle is off");

    for (size_t i = 0; i < sizeof(kPauseKeys) / sizeof(kPauseKeys[0]); i++) {
        ctx->GetOption(kPauseKeys[i]).Set(before[i]);
    }
    ctx->GetOption(RSK_SHUFFLE_SPEAK).Set(speakBefore);
}

} // namespace

extern "C" int OoTTest_TrackerSohFixes(void) {
    printf("[TEST] oot-tracker-soh-fixes: Ship of Harkinian's tracker fixes in the unified tracker's OoT pane: the "
           "tower chest under Triforce Hunt, where Available Checks starts, the credits item pane, entrance names "
           "and the pause screen's list (#1420)\n");
    sFailures = 0;
    if (Rando_InitRegionGraphForTest() != 0 || Rando::Context::GetInstance() == nullptr) {
        printf("[TEST] FAIL: oot-tracker-soh-fixes: no Rando::Context and region graph\n");
        return 1;
    }
    if (Rando::StaticData::GetLocation(RC_GANONS_TOWER_BOSS_KEY_CHEST)->GetName().empty()) {
        Rando::StaticData::InitLocationTable();
    }
    if (Rando::StaticData::RetrieveItem(RG_ZORAS_RIVER_BEAN_SOUL).GetName().GetEnglish().empty()) {
        Rando::StaticData::InitItemTable();
    }

    OtsfLegExclusion();
    OtsfLegStartingRegion();
    OtsfLegCredits();
    OtsfLegEntranceData();
    OtsfLegPauseList();

    if (sFailures != 0) {
        printf("[TEST] FAIL: oot-tracker-soh-fixes: %d check(s) failed\n", sFailures);
    } else {
        printf("[TEST] PASS: oot-tracker-soh-fixes\n");
    }
    return sFailures;
}
