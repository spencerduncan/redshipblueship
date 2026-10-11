/**
 * @file test_combo_area.c
 * @brief ComboAreaVocabulary: the shared area vocabulary (epic #575, child H1).
 *
 * ROM-free, redship tier. Drives the real vocabulary (src/common/combo_area.c),
 * both games' registered adapters through Combo_Area_OfCheck, MM's real check
 * table and OoT's real hint text table (HintTable_Init, the first step of
 * InitStaticData, runs without a ROM), and two source scans:
 *
 *  1. Names are unique within each half, every name reads back through
 *     Combo_Area_FromName to the same area (the two specials included), and no
 *     OoT area's name equals an MM area's name.
 *  2. Every MM check except RC_UNKNOWN maps to an MM area through MM's adapter;
 *     the only checks with no area are the enemy drops, all of them; and every
 *     MM area holds at least one check.
 *  3. Every RandomizerArea from RA_KOKIRI_FOREST to RA_GANONS_CASTLE maps to a
 *     distinct OoT area whose name is OoT's own English clear text for it, and
 *     RA_NONE / RA_TERMINA map to no OoT area.
 *  4. MM's dead StaticData::RandoStaticRegion decoy is gone, and combo_area.{h,c}
 *     include no game header.
 *
 * Also MmHintAreaText (child H2): MM's own hints speak that vocabulary. See the
 * block above Test_MmHintAreaText. And MmHintCrossedArea (child H3c): MM's hints
 * name the OoT area of an MM item that crossed into Hyrule. See the block above
 * Test_MmHintCrossedArea.
 *
 * Linkage note: #included into test_runner.cpp at file scope (compiled as C++).
 */

#include "../combo_area.h"
#include "../combo_tracker_view.h" // OoT_TrackerAdapter_Register: OoT's check status (H3c)
#include "../crossing_store.h"     // Combo_Crossings_Replace: an MM item hosted in OoT (H3c)
#include "../game.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

extern "C" {
int MM_HintArea_TestCheckRow(uint16_t id, int* outIsEnemyDrop);
int MM_HintArea_TestCheckIdMax(void);
int OoT_HintArea_TestInitStaticData(void);
void OoT_HintArea_TestAreaRange(int* outFirst, int* outLast);
void OoT_HintArea_TestArea(int ra, uint8_t* outArea, char* outEnglish, int cap);
void OoT_HintArea_TestSpecials(uint8_t* outNone, uint8_t* outPocket, uint8_t* outTermina);
// #575 H2: MM's gossip stone (games/mm/2s2h/Rando/ActorBehavior/EnGs.cpp's bridge block)
int MM_Rando_Hints_TestGossipCandidate(uint16_t randoCheckId, int excludeObtained);
int MM_Rando_Hints_TestGossipText(uint16_t randoCheckId, int purchased, char* out, int cap);
int MM_Rando_Hints_TestSceneName(uint16_t randoCheckId, char* out, int cap);
// #575 H2: the helper every MM hint fills its location with (HintAreaSingleExe.cpp)
int MM_HintArea_TestLocationText(uint16_t randoCheckId, const char* tmpl, const char* key, char* out, int cap);
void MM_Rando_Hints_TestSetCheck(uint16_t randoCheckId, uint16_t randoItemId, int shuffled, int obtained,
                                 uint16_t* priorItemId, int* priorShuffled, int* priorObtained);
// #575 H3c: the stone's candidates of either host game, and the static hints' item helper
int MM_Rando_Hints_TestGossipCandidateAt(int excludeObtained, int index, uint8_t* outGame, uint16_t* outCheck);
int MM_Rando_Hints_TestGossipHostText(uint8_t hostGame, uint16_t check, int purchased, char* out, int cap);
int MM_HintArea_TestItemLocationText(uint16_t randoItemId, const char* tmpl, const char* key, char* out, int cap);
// #575 H3c: an OoT world on the heap whose checks have areas (TrackerAdapterSingleExe.cpp, HintAreaOoT.cpp)
int OoT_TrackerAdapter_TestAuthorWorld(uint32_t seed, uint16_t outIds[3]);
void OoT_TrackerAdapter_TestReleaseWorld(void);
int OoT_TrackerAdapter_TestSetCollected(uint16_t rc, int collected);
int OoT_HintArea_TestSetArea(uint16_t rc, int ra);
}

#define CA_ASSERT(cond)                                                                                                \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            return TEST_FAIL;                                                                                          \
        }                                                                                                              \
    } while (0)

namespace {

bool CaReadSource(const std::string& rel, std::string* out) {
#ifdef RSBS_SOURCE_DIR
    std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + rel, std::ios::binary);
    if (!in.good()) {
        return false;
    }
    out->assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return true;
#else
    (void)rel;
    (void)out;
    return false;
#endif
}

/** Every #include target in `text` that is not on the allowed list, or "". */
std::string CaForeignInclude(const std::string& text, const std::set<std::string>& allowed) {
    size_t pos = 0;
    while ((pos = text.find("#include", pos)) != std::string::npos) {
        const size_t open = text.find_first_of("<\"", pos);
        const size_t close = open == std::string::npos ? open : text.find_first_of(">\"", open + 1);
        if (open == std::string::npos || close == std::string::npos) {
            return "(unparsable #include)";
        }
        const std::string target = text.substr(open + 1, close - open - 1);
        if (allowed.count(target) == 0) {
            return target;
        }
        pos = close;
    }
    return "";
}

} // namespace

TestResult Test_ComboAreaVocabulary(void) {
    printf("[TEST] combo-area-vocabulary: one shared area vocabulary, both adapters, MM's table (#575 H1)\n");

    // ---- 1. Names: unique per half, FromName inverts Name, no cross-half collision.
    const uint8_t games[2] = { (uint8_t)GAME_OOT, (uint8_t)GAME_MM };
    CA_ASSERT(Combo_Area_Count((uint8_t)GAME_OOT) == 35);
    CA_ASSERT(Combo_Area_Count((uint8_t)GAME_MM) == 46);
    CA_ASSERT(Combo_Area_Count((uint8_t)GAME_NONE) == 0);
    std::set<std::string> namesByGame[2];
    for (int g = 0; g < 2; g++) {
        const int count = Combo_Area_Count(games[g]);
        for (int i = 0; i <= count + 1; i++) {
            ComboArea a = { games[g], (uint8_t)(i == count + 1 ? COMBO_AREA_POCKET : i) };
            const char* name = Combo_Area_Name(a);
            CA_ASSERT(name != nullptr && name[0] != '\0');
            if (namesByGame[g].count(name) != 0) {
                printf("[TEST] FAIL: game %d area %d repeats the name \"%s\"\n", games[g], a.area, name);
                return TEST_FAIL;
            }
            namesByGame[g].insert(name);
            ComboArea back = { 0, 0 };
            CA_ASSERT(Combo_Area_FromName(games[g], name, &back));
            if (back.game != a.game || back.area != a.area) {
                printf("[TEST] FAIL: \"%s\" reads back as {%d,%d}, not {%d,%d}\n", name, back.game, back.area,
                       a.game, a.area);
                return TEST_FAIL;
            }
            CA_ASSERT(Combo_Area_IsValid(a));
        }
        const ComboArea past = { games[g], (uint8_t)(count + 1) };
        CA_ASSERT(!Combo_Area_IsValid(past));
        CA_ASSERT(Combo_Area_Name(past) == nullptr);
    }
    for (int i = 1; i <= Combo_Area_Count((uint8_t)GAME_OOT); i++) {
        const ComboArea oot = { (uint8_t)GAME_OOT, (uint8_t)i };
        for (int j = 1; j <= Combo_Area_Count((uint8_t)GAME_MM); j++) {
            const ComboArea mm = { (uint8_t)GAME_MM, (uint8_t)j };
            if (strcmp(Combo_Area_Name(oot), Combo_Area_Name(mm)) == 0) {
                printf("[TEST] FAIL: OoT area %d and MM area %d share the name \"%s\"\n", i, j, Combo_Area_Name(oot));
                return TEST_FAIL;
            }
        }
    }
    ComboArea untouched = { 7, 7 };
    CA_ASSERT(!Combo_Area_FromName((uint8_t)GAME_MM, "Kokiri Forest", &untouched));
    CA_ASSERT(untouched.game == 7 && untouched.area == 7);
    CA_ASSERT(Combo_Area_Name(ComboArea{ (uint8_t)GAME_NONE, 1 }) == nullptr);
    printf("[TEST]   names: %zu OoT and %zu MM names (specials included) are unique, invert, and never collide\n",
           namesByGame[0].size(), namesByGame[1].size());

    // ---- 2. MM's table through MM's registered adapter.
    CA_ASSERT(Combo_Area_HasAdapter((uint8_t)GAME_MM));
    const int checkIdMax = MM_HintArea_TestCheckIdMax();
    CA_ASSERT(checkIdMax > 1);
    std::vector<int> checksPerArea((size_t)Combo_Area_Count((uint8_t)GAME_MM) + 1, 0);
    int rows = 0;
    int enemyDrops = 0;
    int enemyDropsWithArea = 0;
    int rowsWithoutArea = 0;
    for (int id = 0; id < checkIdMax; id++) {
        int isEnemyDrop = 0;
        const ComboArea a = Combo_Area_OfCheck((uint8_t)GAME_MM, (uint16_t)id);
        CA_ASSERT(a.game == (uint8_t)GAME_MM);
        if (MM_HintArea_TestCheckRow((uint16_t)id, &isEnemyDrop) == 0) {
            CA_ASSERT(a.area == COMBO_AREA_NONE); // RC_UNKNOWN and non-rows have no area
            continue;
        }
        rows++;
        enemyDrops += isEnemyDrop;
        if (a.area == COMBO_AREA_NONE) {
            if (!isEnemyDrop) {
                rowsWithoutArea++;
                if (rowsWithoutArea <= 5) {
                    printf("[TEST]   MM check %d has no area and is not an enemy drop\n", id);
                }
            }
            continue;
        }
        if (isEnemyDrop) {
            enemyDropsWithArea++;
        }
        CA_ASSERT(a.area != COMBO_AREA_POCKET && (int)a.area <= Combo_Area_Count((uint8_t)GAME_MM));
        checksPerArea[a.area]++;
    }
    printf("[TEST]   MM: %d check rows, %d enemy drops; %d other rows without an area, %d enemy drops with one\n", rows,
           enemyDrops, rowsWithoutArea, enemyDropsWithArea);
    CA_ASSERT(rows > 0);
    CA_ASSERT(enemyDrops == 55);
    CA_ASSERT(rowsWithoutArea == 0);
    CA_ASSERT(enemyDropsWithArea == 0);
    int emptyAreas = 0;
    for (int i = 1; i <= Combo_Area_Count((uint8_t)GAME_MM); i++) {
        if (checksPerArea[i] == 0) {
            const ComboArea a = { (uint8_t)GAME_MM, (uint8_t)i };
            printf("[TEST]   MM area %d (%s) holds no check\n", i, Combo_Area_Name(a));
            emptyAreas++;
        }
    }
    CA_ASSERT(emptyAreas == 0);
    printf("[TEST]   MM: every area holds a check\n");

    // ---- 3. OoT: RandomizerArea -> the OoT half, named in OoT's own English text.
    CA_ASSERT(Combo_Area_HasAdapter((uint8_t)GAME_OOT));
    CA_ASSERT(OoT_HintArea_TestInitStaticData() == 0);
    int first = 0;
    int last = 0;
    OoT_HintArea_TestAreaRange(&first, &last);
    CA_ASSERT(last - first + 1 == Combo_Area_Count((uint8_t)GAME_OOT));
    std::set<int> ootAreasSeen;
    for (int ra = first; ra <= last; ra++) {
        uint8_t area = 0;
        char english[128] = { 0 };
        OoT_HintArea_TestArea(ra, &area, english, (int)sizeof(english));
        const ComboArea a = { (uint8_t)GAME_OOT, area };
        CA_ASSERT(area != COMBO_AREA_NONE && area != COMBO_AREA_POCKET && Combo_Area_IsValid(a));
        CA_ASSERT(ootAreasSeen.insert(area).second);
        if (english[0] == '\0' || strcmp(english, Combo_Area_Name(a)) != 0) {
            printf("[TEST] FAIL: RandomizerArea %d is OoT area %d \"%s\", but OoT's own text reads \"%s\"\n", ra, area,
                   Combo_Area_Name(a), english);
            return TEST_FAIL;
        }
    }
    uint8_t raNone = 0xEE;
    uint8_t raPocket = 0xEE;
    uint8_t raTermina = 0xEE;
    OoT_HintArea_TestSpecials(&raNone, &raPocket, &raTermina);
    CA_ASSERT(raNone == COMBO_AREA_NONE);
    CA_ASSERT(raPocket == COMBO_AREA_POCKET);
    CA_ASSERT(raTermina == COMBO_AREA_NONE);
    printf("[TEST]   OoT: %zu RandomizerAreas map to distinct OoT areas named in OoT's own English text\n",
           ootAreasSeen.size());

    // ---- 4. Source scans.
    std::string staticData;
    CA_ASSERT(CaReadSource("games/mm/2s2h/Rando/StaticData/StaticData.h", &staticData));
    if (staticData.find("RandoStaticRegion") != std::string::npos) {
        printf("[TEST] FAIL: games/mm/2s2h/Rando/StaticData/StaticData.h still declares RandoStaticRegion\n");
        return TEST_FAIL;
    }
    const std::set<std::string> allowedH = { "stdbool.h", "stdint.h" };
    const std::set<std::string> allowedC = { "combo_area.h", "game.h", "stddef.h", "string.h" };
    std::string header;
    std::string source;
    CA_ASSERT(CaReadSource("src/common/combo_area.h", &header));
    CA_ASSERT(CaReadSource("src/common/combo_area.c", &source));
    const std::string badH = CaForeignInclude(header, allowedH);
    const std::string badC = CaForeignInclude(source, allowedC);
    if (!badH.empty() || !badC.empty()) {
        printf("[TEST] FAIL: combo_area includes \"%s\"%s\"%s\" beyond the game-free list\n", badH.c_str(),
               badH.empty() || badC.empty() ? "" : " and ", badC.c_str());
        return TEST_FAIL;
    }
    printf("[TEST]   sources: RandoStaticRegion is gone; combo_area.{h,c} include no game header\n");

    printf("[TEST] PASS: combo-area-vocabulary\n");
    return TEST_PASS;
}

// ============================================================================
// #575 H2: MM's own hints name the shared area, never the scene.
//
// MM's gossip stones said where a check is by its SCENE's display name, so all
// 415 grotto checks (SCENE_KAKUSIANA) read "Lone Peak Shrine & Grottos" wherever
// the grotto is, and the 55 enemy drops (SCENE_SPOT00, "Cutscene Scene") could
// be hinted at all. The stone now names the check's area in the shared
// vocabulary (H1's MM table) with OoTMM's preposition, and hints only checks
// that have an area. Drives the stone's REAL text (GossipHintText, the function
// both text hooks call, EnGs.cpp) and its REAL candidate filter over MM's real
// check table. Section 4 drives only the shared helper with sample templates:
// the five static hint call sites (DmStk, EnTalk, EnZow, EnKgy x2) are not
// driven, so their templates are not locked here.
// ============================================================================
TestResult Test_MmHintAreaText(void) {
    printf("[TEST] mm-hint-area-text: MM's own hints name the shared area, not the scene (#575 H2)\n");
    static const char kGrottoScene[] = "Lone Peak Shrine & Grottos";
    const int idMax = MM_HintArea_TestCheckIdMax();
    char text[512];
    char scene[128];

    // ---- 1. A grotto check's text names its overworld area, never the grotto scene.
    int grottoRows = 0;
    uint16_t firstGrotto = 0;
    for (int id = 1; id < idMax; id++) {
        int isEnemyDrop = 0;
        if (!MM_HintArea_TestCheckRow((uint16_t)id, &isEnemyDrop)) {
            continue;
        }
        CA_ASSERT(MM_Rando_Hints_TestSceneName((uint16_t)id, scene, (int)sizeof(scene)) > 0);
        if (strcmp(scene, kGrottoScene) != 0) {
            continue;
        }
        const ComboArea area = Combo_Area_OfCheck((uint8_t)GAME_MM, (uint16_t)id);
        const char* areaName = Combo_Area_Name(area);
        CA_ASSERT(area.area != COMBO_AREA_NONE && areaName != nullptr);
        for (int purchased = 0; purchased < 2; purchased++) {
            CA_ASSERT(MM_Rando_Hints_TestGossipText((uint16_t)id, purchased, text, (int)sizeof(text)) > 0);
            if (firstGrotto == 0) {
                printf("[TEST]   grotto check %d (scene \"%s\", area \"%s\"): stone says \"%s\"\n", id, scene, areaName,
                       text);
            }
            if (strstr(text, kGrottoScene) != nullptr || strstr(text, areaName) == nullptr) {
                printf("[TEST] FAIL: grotto check %d (area \"%s\") says \"%s\"\n", id, areaName, text);
                return TEST_FAIL;
            }
        }
        if (firstGrotto == 0) {
            firstGrotto = (uint16_t)id;
        }
        grottoRows++;
    }
    CA_ASSERT(grottoRows == 415);
    printf("[TEST]   %d grotto checks name their overworld area, none \"%s\"\n", grottoRows, kGrottoScene);

    // ---- 2. Every check with an area is named by it, with OoTMM's preposition.
    int areaRows = 0;
    uint16_t firstEnemyDrop = 0;
    for (int id = 1; id < idMax; id++) {
        int isEnemyDrop = 0;
        if (!MM_HintArea_TestCheckRow((uint16_t)id, &isEnemyDrop)) {
            continue;
        }
        if (isEnemyDrop) {
            if (firstEnemyDrop == 0) {
                firstEnemyDrop = (uint16_t)id;
            }
            continue;
        }
        const ComboArea area = Combo_Area_OfCheck((uint8_t)GAME_MM, (uint16_t)id);
        CA_ASSERT(area.area != COMBO_AREA_NONE);
        const char* prepos = Combo_Area_Prepos(area);
        const std::string said = std::string("is hidden ") + (prepos != nullptr ? std::string(prepos) + " " : "") +
                                 "%y" + Combo_Area_Name(area) + "%w.";
        for (int purchased = 0; purchased < 2; purchased++) {
            CA_ASSERT(MM_Rando_Hints_TestGossipText((uint16_t)id, purchased, text, (int)sizeof(text)) > 0);
            if (strstr(text, said.c_str()) == nullptr) {
                printf("[TEST] FAIL: check %d says \"%s\", not \"... %s\"\n", id, text, said.c_str());
                return TEST_FAIL;
            }
        }
        areaRows++;
    }
    printf("[TEST]   %d checks with an area are named by it, with its preposition\n", areaRows);

    // ---- 3. An enemy drop (no area) is never a stone's candidate, even holding a major item.
    CA_ASSERT(firstEnemyDrop != 0 && firstGrotto != 0);
    SharedItem mmLens;
    CA_ASSERT(TestNamedItem((uint8_t)GAME_MM, "Lens of Truth", &mmLens));
    const uint16_t rows[2] = { firstEnemyDrop, firstGrotto };
    uint16_t priorItem[2];
    int priorShuffled[2];
    int priorObtained[2];
    for (int i = 0; i < 2; i++) {
        MM_Rando_Hints_TestSetCheck(rows[i], mmLens.id, 1, 0, &priorItem[i], &priorShuffled[i], &priorObtained[i]);
    }
    const int dropFree = MM_Rando_Hints_TestGossipCandidate(firstEnemyDrop, 0);
    const int dropPurchased = MM_Rando_Hints_TestGossipCandidate(firstEnemyDrop, 1);
    const int grottoFree = MM_Rando_Hints_TestGossipCandidate(firstGrotto, 0);
    const int grottoPurchased = MM_Rando_Hints_TestGossipCandidate(firstGrotto, 1);
    for (int i = 0; i < 2; i++) {
        MM_Rando_Hints_TestSetCheck(rows[i], priorItem[i], priorShuffled[i], priorObtained[i], NULL, NULL, NULL);
    }
    printf("[TEST]   the Lens of Truth on enemy drop %u: candidate=%d purchasable=%d; on grotto check %u: "
           "candidate=%d purchasable=%d\n",
           (unsigned)firstEnemyDrop, dropFree, dropPurchased, (unsigned)firstGrotto, grottoFree, grottoPurchased);
    CA_ASSERT(grottoFree == 1 && grottoPurchased == 1); // control: the same item on a check with an area
    CA_ASSERT(dropFree == 0 && dropPurchased == 0);

    // ---- 4. The helper the static hints call (ReplaceLocation), on sample templates
    // of their two forms. This does not drive DmStk/EnTalk/EnZow/EnKgy themselves.
    char said[256];
    const ComboArea grottoArea = Combo_Area_OfCheck((uint8_t)GAME_MM, firstGrotto);
    CA_ASSERT(Combo_Area_Prepos(grottoArea) != nullptr);
    // a template that names the preposition (gold dust, remains, Hookshot)
    CA_ASSERT(MM_HintArea_TestLocationText(firstGrotto, "can be found {{prepos}} %r{location}%w?", "{location}", said,
                                           (int)sizeof(said)) > 0);
    CA_ASSERT(std::string("can be found ") + Combo_Area_Prepos(grottoArea) + " %r" + Combo_Area_Name(grottoArea) +
                  "%w?" ==
              said);
    // a template that does not (the Oath: "coming from"): the name alone
    CA_ASSERT(MM_HintArea_TestLocationText(firstGrotto, "coming from %y{{location}}%w.", "{{location}}", said,
                                           (int)sizeof(said)) > 0);
    CA_ASSERT(std::string("coming from %y") + Combo_Area_Name(grottoArea) + "%w." == said);
    // no area: an enemy drop, and RC_UNKNOWN (0, what FindItemPlacement returns
    // for an item no MM check holds): "nowhere", OoTMM's word, with no preposition
    const uint16_t noArea[2] = { firstEnemyDrop, 0 };
    for (int i = 0; i < 2; i++) {
        CA_ASSERT(MM_HintArea_TestLocationText(noArea[i], "can be found {{prepos}} %r{location}%w?", "{location}",
                                               said, (int)sizeof(said)) > 0);
        printf("[TEST]   check %u (no area): \"%s\"\n", (unsigned)noArea[i], said);
        CA_ASSERT(strcmp(said, "can be found %rnowhere%w?") == 0);
    }

    printf("[TEST] PASS: mm-hint-area-text\n");
    return TEST_PASS;
}

// ============================================================================
// #575 H3c: MM names the OoT area for an MM item hosted in OoT.
//
// Under the single bag an MM item can sit on an OoT check (the crossing store's
// OoT-hosted rows), where no MM check holds it. MM's static hints found the item
// with Rando::FindItemPlacement, got RC_UNKNOWN and said "nowhere" (before H2,
// "Unknown"), and MM's gossip stones drew only from MM's own checks, so such an
// item was never hinted by a stone at all. Now the static hints name the OoT
// host's area (Combo_Area_OfCheck(GAME_OOT, host)) and the stones draw
// OoT-hosted MM items too, after MM's own checks, in the store's insertion order.
//
// Authors a crossing set through Combo_Crossings_Replace onto a heap OoT world
// whose hosts carry areas (OoT's real area adapter answers for them), then drives:
//  1. ReplaceItemLocation, the helper all five static hint call sites fill their
//     location with (a source scan holds them to it): the OoT area is named with
//     the preposition MM's voice gives it, never "Unknown" or "nowhere"; an item
//     an MM check holds is still named by that check's MM area (control).
//  2. The stone's real candidate list (GossipHintCandidates) and real text
//     (GossipHintText): every OoT host is a candidate after MM's checks, in
//     insertion order; the purchasable path skips a host OoT has collected; the
//     text names the MM item and the OoT area.
// Failures are counted, not returned early, so one run reports every leg.
//
// Needs the display-free shared bring-up (OoT's check status names a found
// check's item in the language its CVar holds), so the entry point is this
// plain function, which the Test_MmHintCrossedArea wrapper in test_runner.cpp
// calls after CreateHarnessStyleContext, as Test_ComboTrackerView does.
// ============================================================================
namespace {

int sH3cFailures = 0;

void H3cCheck(bool ok, const char* what, const char* said) {
    if (!ok) {
        sH3cFailures++;
        printf("[TEST] FAIL: %s: \"%s\"\n", what, said != nullptr ? said : "");
    }
}

bool H3cSaysNoPlace(const char* text) {
    return strstr(text, "Unknown") == nullptr && strstr(text, "nowhere") == nullptr;
}

/** The RandomizerArea whose OoT-half area is `area`, or -1. */
int H3cOoTRandomizerArea(uint8_t area) {
    int first = 0;
    int last = 0;
    OoT_HintArea_TestAreaRange(&first, &last);
    for (int ra = first; ra <= last; ra++) {
        uint8_t mapped = 0;
        char english[128] = { 0 };
        OoT_HintArea_TestArea(ra, &mapped, english, (int)sizeof(english));
        if (mapped == area) {
            return ra;
        }
    }
    return -1;
}

} // namespace

TestResult MmHintCrossedArea_RunHeadless(void) {
    printf("[TEST] mm-hint-crossed-area: MM names the OoT area for an MM item hosted in OoT (#575 H3c)\n");
    sH3cFailures = 0;
    const int idMax = MM_HintArea_TestCheckIdMax();
    char text[512];
    char scene[128];

    // ---- Sources: every static hint fills its location through ReplaceItemLocation.
    static const char* const kStaticHintSources[] = {
        "games/mm/2s2h/Rando/ActorBehavior/DmStk.cpp",  // the Oath
        "games/mm/2s2h/Rando/ActorBehavior/EnTalk.cpp", // boss remains
        "games/mm/2s2h/Rando/ActorBehavior/EnZow.cpp",  // the Hookshot
        "games/mm/2s2h/Rando/ActorBehavior/EnKgy.cpp",  // gold dust (two texts)
    };
    int helperCalls = 0;
    for (const char* rel : kStaticHintSources) {
        std::string src;
        CA_ASSERT(CaReadSource(rel, &src));
        H3cCheck(src.find("FindItemPlacement") == std::string::npos,
                 "a static hint finds its item itself instead of through ReplaceItemLocation", rel);
        for (size_t pos = 0; (pos = src.find("HintArea::ReplaceItemLocation(", pos)) != std::string::npos; pos++) {
            helperCalls++;
        }
    }
    printf("[TEST]   sources: %d static hint call sites fill their location through ReplaceItemLocation\n",
           helperCalls);
    CA_ASSERT(helperCalls == 5);

    // ---- The world: four MM items, a grotto check of MM's, three OoT hosts with areas.
    SharedItem hookshot;
    SharedItem oath;
    SharedItem odolwa;
    SharedItem goldDust;
    CA_ASSERT(TestNamedItem((uint8_t)GAME_MM, "Hookshot", &hookshot));
    CA_ASSERT(TestNamedItem((uint8_t)GAME_MM, "Oath to Order", &oath));
    CA_ASSERT(TestNamedItem((uint8_t)GAME_MM, "Odolwa's Remains", &odolwa));
    CA_ASSERT(TestNamedItem((uint8_t)GAME_MM, "Bottle With Gold Dust", &goldDust));
    uint16_t grotto = 0;
    for (int id = 1; id < idMax && grotto == 0; id++) {
        int isEnemyDrop = 0;
        if (MM_HintArea_TestCheckRow((uint16_t)id, &isEnemyDrop) &&
            MM_Rando_Hints_TestSceneName((uint16_t)id, scene, (int)sizeof(scene)) > 0 &&
            strcmp(scene, "Lone Peak Shrine & Grottos") == 0) {
            grotto = (uint16_t)id;
        }
    }
    CA_ASSERT(grotto != 0);
    const ComboArea grottoArea = Combo_Area_OfCheck((uint8_t)GAME_MM, grotto);
    CA_ASSERT(grottoArea.area != COMBO_AREA_NONE && Combo_Area_Prepos(grottoArea) != nullptr);

    CA_ASSERT(OoT_HintArea_TestInitStaticData() == 0);
    const int raTrail = H3cOoTRandomizerArea(COMBO_AREA_OOT_DEATH_MOUNTAIN_TRAIL);
    const int raGanon = H3cOoTRandomizerArea(COMBO_AREA_OOT_GANONS_CASTLE);
    const int raForest = H3cOoTRandomizerArea(COMBO_AREA_OOT_KOKIRI_FOREST);
    CA_ASSERT(raTrail >= 0 && raGanon >= 0 && raForest >= 0);

    ComboContext_Init();
    Combo_Crossings_Clear();
    OoT_TrackerAdapter_Register(); // the production registration (Combo_TrackerWindow_Init makes it)
    // MM's hints are read while MM is played: OoT's heap is then its suspended
    // world, read as of the last switch (combo_tracker_view.h).
    const GameId priorGame = Context_GetCurrentGame();
    Context_SetCurrentGame(GAME_MM);
    // [collected, unchecked, unchecked] OoT checks on a heap Rando::Context.
    uint16_t ootIds[3] = { 0, 0, 0 };
    CA_ASSERT(OoT_TrackerAdapter_TestAuthorWorld(0x0A11CE03u, ootIds) == 3);
    const int setAreas = OoT_HintArea_TestSetArea(ootIds[0], raForest) + OoT_HintArea_TestSetArea(ootIds[1], raTrail) +
                         OoT_HintArea_TestSetArea(ootIds[2], raGanon);
    const ComboArea forestArea = Combo_Area_OfCheck((uint8_t)GAME_OOT, ootIds[0]);
    const ComboArea trailArea = Combo_Area_OfCheck((uint8_t)GAME_OOT, ootIds[1]);
    const ComboArea ganonArea = Combo_Area_OfCheck((uint8_t)GAME_OOT, ootIds[2]);
    printf("[TEST]   OoT hosts %u/%u/%u are in \"%s\" / \"%s\" / \"%s\"\n", (unsigned)ootIds[0], (unsigned)ootIds[1],
           (unsigned)ootIds[2], Combo_Area_Name(forestArea), Combo_Area_Name(trailArea), Combo_Area_Name(ganonArea));
    // The control on the fixture: OoT's own adapter answers the authored areas.
    bool worldOk = setAreas == 0 && forestArea.area == COMBO_AREA_OOT_KOKIRI_FOREST &&
                   trailArea.area == COMBO_AREA_OOT_DEATH_MOUNTAIN_TRAIL && ganonArea.area == COMBO_AREA_OOT_GANONS_CASTLE;

    // The OoT-hosted rows, deliberately not in ascending host order: the stone
    // must keep the store's insertion order, never re-sort it.
    ComboCrossing hosted[3];
    memset(hosted, 0, sizeof(hosted));
    const uint16_t hostOrder[3] = { ootIds[2], ootIds[1], ootIds[0] };
    const SharedItem itemOrder[3] = { oath, hookshot, odolwa };
    for (int i = 0; i < 3; i++) {
        hosted[i].hostCheck = hostOrder[i];
        hosted[i].itemClass = (uint16_t)RSBS_ITEMCLASS_PROGRESSION;
        hosted[i].item = itemOrder[i];
    }
    worldOk = worldOk && Combo_Crossings_Replace(hosted, 3, NULL, 0) == 3;
    // MM's own check holds the gold dust: the control that a placed item is
    // still named by its MM check, and the one MM candidate of the stone.
    uint16_t priorItem = 0;
    int priorShuffled = 0;
    int priorObtained = 0;
    MM_Rando_Hints_TestSetCheck(grotto, goldDust.id, 1, 0, &priorItem, &priorShuffled, &priorObtained);

    if (worldOk) {
        // ---- 1. The static hints: the item helper, on the templates' two forms.
        struct StaticCase {
            const char* what;
            uint16_t item;
            const char* tmpl;
            const char* key;
            std::string expected;
        };
        const StaticCase statics[] = {
            { "the Hookshot hint", hookshot.id,
              "I overheard those Pirates talk about treasure {{prepos}} %y{{location}}%w that can help you reach the "
              "unreachable!",
              "{{location}}", std::string("treasure on %y") + Combo_Area_Name(trailArea) + "%w that" },
            { "the Oath hint", oath.id, "I can hear the Giants Melody coming from %y{{location}}%w.", "{{location}}",
              "coming from %yinside Ganon's Castle%w." },
            { "the remains hint", odolwa.id, "Last seen {{prepos}} %y{{location}}%w.", "{{location}}",
              std::string("Last seen in %y") + Combo_Area_Name(forestArea) + "%w." },
            { "the gold dust hint (on MM's own check)", goldDust.id, "Gold dust can be found {{prepos}} %p{location}%w.",
              "{location}",
              std::string("found ") + Combo_Area_Prepos(grottoArea) + " %p" + Combo_Area_Name(grottoArea) + "%w." },
        };
        for (const StaticCase& st : statics) {
            const int len = MM_HintArea_TestItemLocationText(st.item, st.tmpl, st.key, text, (int)sizeof(text));
            printf("[TEST]   %s says \"%s\"\n", st.what, len > 0 ? text : "");
            H3cCheck(len > 0 && strstr(text, st.expected.c_str()) != nullptr && H3cSaysNoPlace(text), st.what,
                     len > 0 ? text : "");
        }

        // ---- 2. The stones: candidates of both host games, in a stable order.
        for (int purchased = 0; purchased < 2; purchased++) {
            uint8_t game = 0;
            uint16_t check = 0;
            const int count = MM_Rando_Hints_TestGossipCandidateAt(purchased, -1, &game, &check);
            std::vector<uint16_t> ootSeen;
            bool mmAfterOoT = false;
            bool grottoSeen = false;
            for (int i = 0; i < count; i++) {
                H3cCheck(MM_Rando_Hints_TestGossipCandidateAt(purchased, i, &game, &check) == count,
                         "the candidate list changed between two reads", "");
                if (game == (uint8_t)GAME_OOT) {
                    ootSeen.push_back(check);
                } else {
                    mmAfterOoT = mmAfterOoT || !ootSeen.empty();
                    grottoSeen = grottoSeen || (game == (uint8_t)GAME_MM && check == grotto);
                }
            }
            printf("[TEST]   %s path: %d candidates, %zu of them OoT hosts:", purchased ? "purchasable" : "free", count,
                   ootSeen.size());
            for (uint16_t h : ootSeen) {
                printf(" %u", (unsigned)h);
            }
            printf("\n");
            // The free stone draws every OoT host; the purchasable path skips the
            // host OoT has already collected (ootIds[0]), as it skips a collected
            // MM check.
            const std::vector<uint16_t> expected = purchased ? std::vector<uint16_t>{ ootIds[2], ootIds[1] }
                                                             : std::vector<uint16_t>{ ootIds[2], ootIds[1], ootIds[0] };
            H3cCheck(ootSeen == expected, "the OoT hosts are not the stone's candidates, in insertion order",
                     purchased ? "purchasable" : "free");
            H3cCheck(grottoSeen && !mmAfterOoT, "MM's own checks are not all drawn first",
                     purchased ? "purchasable" : "free");
        }
        // The purchasable skip follows OoT's own check status, not the host's identity.
        H3cCheck(OoT_TrackerAdapter_TestSetCollected(ootIds[0], 0) == 1, "OoT's authored host was not collected", "");
        {
            uint8_t game = 0;
            uint16_t check = 0;
            const int count = MM_Rando_Hints_TestGossipCandidateAt(1, -1, &game, &check);
            bool hostBack = false;
            for (int i = 0; i < count; i++) {
                MM_Rando_Hints_TestGossipCandidateAt(1, i, &game, &check);
                hostBack = hostBack || (game == (uint8_t)GAME_OOT && check == ootIds[0]);
            }
            H3cCheck(hostBack, "an uncollected OoT host is not a purchasable candidate", "host uncollected");
        }

        // The text: the MM item (MM's own, so no game marker) and the OoT area.
        struct StoneCase {
            uint16_t host;
            SharedItem item;
            std::string place;
        };
        const StoneCase stones[] = {
            { ootIds[1], hookshot, std::string("is hidden on %y") + Combo_Area_Name(trailArea) + "%w." },
            { ootIds[2], oath, "is hidden %yinside Ganon's Castle%w." },
            { ootIds[0], odolwa, std::string("is hidden in %y") + Combo_Area_Name(forestArea) + "%w." },
        };
        for (const StoneCase& st : stones) {
            const char* itemName = Combo_GetForeignItemName(st.item);
            for (int purchased = 0; purchased < 2; purchased++) {
                const int len =
                    MM_Rando_Hints_TestGossipHostText((uint8_t)GAME_OOT, st.host, purchased, text, (int)sizeof(text));
                if (purchased == 0) {
                    printf("[TEST]   a stone on OoT host %u says \"%s\"\n", (unsigned)st.host, len > 0 ? text : "");
                }
                H3cCheck(len > 0 && itemName != nullptr && strstr(text, itemName) != nullptr &&
                             strstr(text, st.place.c_str()) != nullptr && strstr(text, "(OoT)") == nullptr &&
                             strstr(text, "(MM)") == nullptr && H3cSaysNoPlace(text),
                         "a stone does not name the MM item and its OoT area", len > 0 ? text : "");
            }
        }
    } else {
        printf("[TEST] FAIL: the fixture did not build (OoT areas %d/%d/%d, crossings %d)\n", forestArea.area,
               trailArea.area, ganonArea.area, Combo_Crossings_Count(GAME_OOT));
        sH3cFailures++;
    }

    // ---- Restore: MM's row, the store, OoT's heap world, the combo context.
    MM_Rando_Hints_TestSetCheck(grotto, priorItem, priorShuffled, priorObtained, NULL, NULL, NULL);
    Combo_Crossings_Clear();
    OoT_TrackerAdapter_TestReleaseWorld();
    ComboContext_Init();
    Context_SetCurrentGame(priorGame);

    if (sH3cFailures != 0) {
        printf("[TEST] FAIL: mm-hint-crossed-area: %d failure(s)\n", sH3cFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: mm-hint-crossed-area\n");
    return TEST_PASS;
}
