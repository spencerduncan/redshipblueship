/**
 * @file test_foolish_hints.c
 * @brief MmFoolishStoneHint: MM's free gossip stone speaks a deterministic
 *        foolish hint in MM's voice (epic #575, child H5a).
 *
 * ROM-free, redship tier. Authors a small MM world over MM's REAL check table
 * through the stone's own test bridges (EnGs.cpp) and the crossing store, then
 * reads what the free stone says for many stone seeds through
 * MM_Rando_Hints_TestFreeStoneText: the text the free stone's hook shows, for a
 * given item pick and stone seed.
 *
 * The world, five MM areas of the shared vocabulary (combo_area.h):
 *   A  every check shuffled, every one holding junk            -> foolish
 *   B  every check shuffled, junk but one: MM's Lens of Truth  -> not (an item MM's fill counts)
 *   C  every check shuffled, junk, one of them a CROSSING HOST -> not (a crossing is always important;
 *      the host physically holds MM's junk cover, the OoT item lives in the store)
 *   D  every check shuffled, junk but one: a Heart Piece       -> not (shared health: the single bag
 *      places it, so its reconciled class is PROGRESSION)
 *   E  junk everywhere, nothing shuffled                       -> not (no hintable check)
 * Every other check is unshuffled junk, so A is the ONLY foolish area.
 *
 * Asserted: the shared rule (Combo_Area_ComputeFoolish) calls exactly {A}
 * foolish; and over 512 stone seeds with the stone's item pick fixed on B's Lens:
 *   1. some stones speak the foolish variant, and not all of them;
 *   2. every foolish text is exactly "They say plundering %y<A>%w is a %pfoolish
 *      choice%w." -- only A is ever called foolish (never B, C, D or E);
 *   3. every other text is exactly the item hint the stone said before H5a;
 *   4. the same seed says the same thing twice (a pure function of the frozen
 *      world and the stone);
 *   5. about one stone in five speaks the foolish variant;
 * and, with E's checks shuffled too, the rule calls exactly {A, E} foolish and
 * the stones name both of them.
 *
 * Linkage note: #included into test_runner.cpp at file scope (compiled as C++).
 */

#include "../combo_area.h"
#include "../context.h"
#include "../crossing_store.h"
#include "../game.h"
#include "../shared_items.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

extern "C" {
// MM's gossip stone (games/mm/2s2h/Rando/ActorBehavior/EnGs.cpp's bridge block)
int MM_Rando_Hints_TestGossipText(uint16_t randoCheckId, int purchased, char* out, int cap);
int MM_Rando_Hints_TestFreeStoneText(uint16_t randoCheckId, uint32_t stoneSeed, char* out, int cap);
void MM_Rando_Hints_TestSetCheck(uint16_t randoCheckId, uint16_t randoItemId, int shuffled, int obtained,
                                 uint16_t* priorItemId, int* priorShuffled, int* priorObtained);
// MM's check table (HintAreaSingleExe.cpp, Foreign.cpp)
int MM_HintArea_TestCheckRow(uint16_t id, int* outIsEnemyDrop);
int MM_HintArea_TestCheckIdMax(void);
int MM_Rando_Foreign_TestIsForeignHostClass(uint16_t randoCheckId);
void MM_Rando_Foreign_TestItemSentinels(uint16_t* outJunk, uint16_t* outNone, uint16_t* outUnknown);
}

namespace {

struct FhPrior {
    uint16_t id;
    uint16_t item;
    int shuffled;
    int obtained;
};

/** "They say plundering %y<area>%w is a %pfoolish choice%w." for `area`, or "". */
std::string FhFoolishText(ComboArea area) {
    const char* name = Combo_Area_Name(area);
    return name != nullptr ? std::string("They say plundering %y") + name + "%w is a %pfoolish choice%w." : "";
}

/** The authored world's assertions; the caller restores every check after. */
bool FhRunAuthoredWorld(const ComboArea areas[5], uint16_t lensCheck) {
    char itemText[512];
    if (MM_Rando_Hints_TestGossipText(lensCheck, 0, itemText, (int)sizeof(itemText)) <= 0) {
        printf("[TEST] FAIL: the stone has no item hint for check %u\n", (unsigned)lensCheck);
        return false;
    }
    // The set the stone draws from, straight from the shared rule: A and only A.
    ComboFoolishAreas set;
    const int foolishAreas = Combo_Area_ComputeFoolish((uint8_t)GAME_MM, &set);
    printf("[TEST]   the shared rule calls %d MM area(s) foolish; A=%d B=%d C=%d D=%d E=%d\n", foolishAreas,
           (int)Combo_Area_IsFoolish(&set, areas[0]), (int)Combo_Area_IsFoolish(&set, areas[1]),
           (int)Combo_Area_IsFoolish(&set, areas[2]), (int)Combo_Area_IsFoolish(&set, areas[3]),
           (int)Combo_Area_IsFoolish(&set, areas[4]));
    if (foolishAreas != 1 || !Combo_Area_IsFoolish(&set, areas[0])) {
        printf("[TEST] FAIL: the shared rule's MM set is not exactly {A}\n");
        return false;
    }
    const std::string foolishA = FhFoolishText(areas[0]);
    int foolish = 0;
    int itemHints = 0;
    std::set<std::string> otherFoolish;
    std::string firstFoolish;
    const uint32_t kSeeds = 512;
    for (uint32_t seed = 0; seed < kSeeds; seed++) {
        char text[512];
        char again[512];
        if (MM_Rando_Hints_TestFreeStoneText(lensCheck, seed, text, (int)sizeof(text)) <= 0 ||
            MM_Rando_Hints_TestFreeStoneText(lensCheck, seed, again, (int)sizeof(again)) <= 0) {
            printf("[TEST] FAIL: stone seed %u said nothing\n", (unsigned)seed);
            return false;
        }
        if (strcmp(text, again) != 0) {
            printf("[TEST] FAIL: stone seed %u said \"%s\", then \"%s\"\n", (unsigned)seed, text, again);
            return false;
        }
        if (strstr(text, "foolish") != nullptr) {
            foolish++;
            if (firstFoolish.empty()) {
                firstFoolish = text;
            }
            if (foolishA != text) {
                otherFoolish.insert(text);
            }
        } else if (strcmp(text, itemText) == 0) {
            itemHints++;
        } else {
            printf("[TEST] FAIL: stone seed %u said \"%s\": neither the item hint nor a foolish hint\n",
                   (unsigned)seed, text);
            return false;
        }
    }
    printf("[TEST]   %u stone seeds: %d foolish (first: \"%s\"), %d the item hint \"%s\"\n", (unsigned)kSeeds,
           foolish, firstFoolish.c_str(), itemHints, itemText);
    bool ok = true;
    // About one stone in Rando::HintFoolish::kFreeStoneOneIn (5): 102 of 512 expected.
    if (foolish < 51 || foolish > 154) {
        printf("[TEST] FAIL: %d of %u stones spoke the foolish variant, far from one in five\n", foolish,
               (unsigned)kSeeds);
        ok = false;
    }
    for (const std::string& t : otherFoolish) {
        printf("[TEST] FAIL: a stone called somewhere other than \"%s\" foolish: \"%s\"\n", Combo_Area_Name(areas[0]),
               t.c_str());
        ok = false;
    }
    if (foolish == 0) {
        printf("[TEST] FAIL: no stone of %u ever names a foolish area, though \"%s\" holds nothing but junk\n",
               (unsigned)kSeeds, Combo_Area_Name(areas[0]));
        ok = false;
    }
    if (itemHints == 0) {
        printf("[TEST] FAIL: every stone of %u spoke the foolish variant; the item hint is gone\n", (unsigned)kSeeds);
        ok = false;
    }
    return ok;
}

/** With E's checks shuffled too, E is foolish beside A: the stones name both,
 *  and nothing else (the draw over several foolish areas reaches every one). */
bool FhRunTwoFoolishAreas(const ComboArea areas[5], uint16_t lensCheck) {
    ComboFoolishAreas set;
    const int foolishAreas = Combo_Area_ComputeFoolish((uint8_t)GAME_MM, &set);
    if (foolishAreas != 2 || !Combo_Area_IsFoolish(&set, areas[0]) || !Combo_Area_IsFoolish(&set, areas[4])) {
        printf("[TEST] FAIL: with E shuffled, the shared rule's MM set is not exactly {A, E} (%d areas)\n",
               foolishAreas);
        return false;
    }
    const std::string foolishA = FhFoolishText(areas[0]);
    const std::string foolishE = FhFoolishText(areas[4]);
    int namesA = 0;
    int namesE = 0;
    for (uint32_t seed = 0; seed < 512; seed++) {
        char text[512];
        if (MM_Rando_Hints_TestFreeStoneText(lensCheck, seed, text, (int)sizeof(text)) <= 0) {
            return false;
        }
        if (strstr(text, "foolish") == nullptr) {
            continue;
        }
        if (foolishA == text) {
            namesA++;
        } else if (foolishE == text) {
            namesE++;
        } else {
            printf("[TEST] FAIL: stone seed %u called somewhere else foolish: \"%s\"\n", (unsigned)seed, text);
            return false;
        }
    }
    printf("[TEST]   with E shuffled too: {A, E} foolish; 512 stone seeds name A %d times, E %d times\n", namesA,
           namesE);
    if (namesA == 0 || namesE == 0) {
        printf("[TEST] FAIL: one of two foolish areas is never named\n");
        return false;
    }
    return true;
}

} // namespace

TestResult Test_MmFoolishStoneHint(void) {
    printf("[TEST] mm-foolish-stone-hint: MM's free gossip stone names a foolish area in MM's voice (#575 H5a)\n");
    const int idMax = MM_HintArea_TestCheckIdMax();
    uint16_t riJunk = 0;
    MM_Rando_Foreign_TestItemSentinels(&riJunk, NULL, NULL);

    SharedItem mmLens;
    SharedItem mmHeart;
    SharedItem ootHammer;
    const bool haveItems = TestNamedItem((uint8_t)GAME_MM, "Lens of Truth", &mmLens) &&
                           TestNamedItem((uint8_t)GAME_MM, "Heart Piece", &mmHeart) &&
                           TestNamedItem((uint8_t)GAME_OOT, "Megaton Hammer", &ootHammer);
    if (!haveItems || riJunk == 0) {
        printf("[TEST] FAIL: the named items or MM's junk cover could not be found\n");
        return TEST_FAIL;
    }

    // Every real check, grouped by its MM area; the first crossing-host-class
    // check fixes area C.
    std::vector<std::vector<uint16_t>> byArea((size_t)Combo_Area_Count((uint8_t)GAME_MM) + 1);
    uint16_t hostCheck = 0;
    for (int id = 1; id < idMax; id++) {
        if (!MM_HintArea_TestCheckRow((uint16_t)id, NULL)) {
            continue;
        }
        const ComboArea a = Combo_Area_OfCheck((uint8_t)GAME_MM, (uint16_t)id);
        if (a.area != COMBO_AREA_NONE && a.area < byArea.size()) {
            byArea[a.area].push_back((uint16_t)id);
        }
        if (hostCheck == 0 && a.area != COMBO_AREA_NONE && MM_Rando_Foreign_TestIsForeignHostClass((uint16_t)id)) {
            hostCheck = (uint16_t)id;
        }
    }
    if (hostCheck == 0) {
        printf("[TEST] FAIL: no MM check with an area can host a crossing\n");
        return TEST_FAIL;
    }
    const uint8_t areaC = Combo_Area_OfCheck((uint8_t)GAME_MM, hostCheck).area;
    uint8_t picked[4] = { 0, 0, 0, 0 }; // A, B, D, E
    int pickedCount = 0;
    for (size_t a = 1; a < byArea.size() && pickedCount < 4; a++) {
        if (a != areaC && !byArea[a].empty()) {
            picked[pickedCount++] = (uint8_t)a;
        }
    }
    if (pickedCount < 4) {
        printf("[TEST] FAIL: MM's table has fewer than five areas with checks\n");
        return TEST_FAIL;
    }
    const ComboArea areas[5] = {
        { (uint8_t)GAME_MM, picked[0] }, // A
        { (uint8_t)GAME_MM, picked[1] }, // B
        { (uint8_t)GAME_MM, areaC },     // C
        { (uint8_t)GAME_MM, picked[2] }, // D
        { (uint8_t)GAME_MM, picked[3] }, // E
    };
    const uint16_t lensCheck = byArea[areas[1].area].front();
    const uint16_t heartCheck = byArea[areas[3].area].front();
    printf("[TEST]   A \"%s\" (%zu checks, junk), B \"%s\" (the Lens on %u), C \"%s\" (crossing host %u), D \"%s\" "
           "(a Heart Piece on %u), E \"%s\" (unshuffled)\n",
           Combo_Area_Name(areas[0]), byArea[areas[0].area].size(), Combo_Area_Name(areas[1]), (unsigned)lensCheck,
           Combo_Area_Name(areas[2]), (unsigned)hostCheck, Combo_Area_Name(areas[3]), (unsigned)heartCheck,
           Combo_Area_Name(areas[4]));

    // The Heart Piece is MM's own RITYPE_HEALTH, but health is one shared
    // quantity whose reconciled class is PROGRESSION, so the single bag places
    // it: area D is the "any coordinator placement" leg.
    const uint8_t heartClass = Combo_ItemClassOf(mmHeart);
    printf("[TEST]   MM's Heart Piece reconciles to class \"%s\"\n", Combo_ItemClassName(heartClass));
    if (heartClass != RSBS_FILL_CLASS_PROGRESSION) {
        printf("[TEST] FAIL: the Heart Piece is not PROGRESSION here, so leg D would prove nothing\n");
        return TEST_FAIL;
    }

    // ---- Author the world. Every check first: unshuffled junk (priors kept).
    std::vector<FhPrior> priors;
    ComboContext_Init();
    Combo_Crossings_Clear();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0xC0FFEE05u;
    gComboCtx.sharedRandoSettingsHash = 0x5EED5A5Bu;
    for (int id = 1; id < idMax; id++) {
        if (!MM_HintArea_TestCheckRow((uint16_t)id, NULL)) {
            continue;
        }
        FhPrior p = { (uint16_t)id, 0, 0, 0 };
        MM_Rando_Hints_TestSetCheck((uint16_t)id, riJunk, 0, 0, &p.item, &p.shuffled, &p.obtained);
        priors.push_back(p);
    }
    for (int i = 0; i < 4; i++) { // A, B, C, D shuffled; E not
        for (const uint16_t id : byArea[areas[i].area]) {
            MM_Rando_Hints_TestSetCheck(id, riJunk, 1, 0, NULL, NULL, NULL);
        }
    }
    MM_Rando_Hints_TestSetCheck(lensCheck, mmLens.id, 1, 0, NULL, NULL, NULL);
    MM_Rando_Hints_TestSetCheck(heartCheck, mmHeart.id, 1, 0, NULL, NULL, NULL);
    ComboCrossing crossing;
    memset(&crossing, 0, sizeof(crossing));
    crossing.hostCheck = hostCheck;
    crossing.itemClass = (uint16_t)RSBS_ITEMCLASS_PROGRESSION;
    crossing.item = ootHammer;
    bool ok = Combo_Crossings_Replace(NULL, 0, &crossing, 1) == 1;
    if (!ok) {
        printf("[TEST] FAIL: the crossing store refused the authored crossing\n");
    } else {
        ok = FhRunAuthoredWorld(areas, lensCheck);
    }
    if (ok) {
        for (const uint16_t id : byArea[areas[4].area]) {
            MM_Rando_Hints_TestSetCheck(id, riJunk, 1, 0, NULL, NULL, NULL);
        }
        ok = FhRunTwoFoolishAreas(areas, lensCheck);
    }

    for (const FhPrior& p : priors) {
        MM_Rando_Hints_TestSetCheck(p.id, p.item, p.shuffled, p.obtained, NULL, NULL, NULL);
    }
    Combo_Crossings_Clear();
    ComboContext_Init();
    if (!ok) {
        return TEST_FAIL;
    }
    printf("[TEST] PASS: mm-foolish-stone-hint\n");
    return TEST_PASS;
}
