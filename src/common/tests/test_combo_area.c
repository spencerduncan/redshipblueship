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
 * block above Test_MmHintAreaText.
 *
 * Linkage note: #included into test_runner.cpp at file scope (compiled as C++).
 */

#include "../combo_area.h"
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
