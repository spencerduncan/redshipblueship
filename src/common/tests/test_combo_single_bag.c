/**
 * @file test_combo_single_bag.c
 * @brief The single-bag fill AT THE CREATION EVENT, over both real engines, and
 *        ADR 0010 D5's pair-level locks paired with removal (#645, lane K11).
 *
 * ============================================================================
 * WHAT THIS ROW LOCKS
 * ============================================================================
 *
 *   A. THE SEAM. A real OoT generation of a paired world stops at its general
 *      pass (the restricted passes placed, the general hosts empty, the general
 *      items still in the pool), and MM's creation-time half places the union
 *      bag over both games through the production path (OnFileCreate's paired
 *      branch -> Combo_SingleBag_Run): the GOAL proven, crossings in BOTH
 *      directions, every crossing on a host whose own give path can deliver a
 *      foreign item, no trap and no HOME_ONLY row on a foreign host, and MM's
 *      fixed check contents granted to the rounds (#737).
 *
 *   B. D5 — PAIR-LEVEL BEATABILITY, PAIRED WITH REMOVAL. "Generate a paired
 *      world with an OoT item hosted only in MM; assert the goal provable;
 *      assert removing the MM host flips it unprovable. Both halves required —
 *      without removal the lock is theatre." For EACH direction: the proven
 *      world's final round reads GOAL=1; then, for a crossing whose item has no
 *      other copy anywhere in the coordinator's tables, the tables are rebuilt
 *      WITHOUT that one row (Combo_Logic_HydrateTables, no engine call) and the
 *      same round reads GOAL=0. The witness is found by trying the single-copy
 *      crossings in the coordinator's order; one per direction must exist. Both
 *      halves are OBSERVED on the real engines, and the full tables are put back
 *      after every probe.
 *
 *   C. THE DIRECTION GATE (ADR 0011 decision 2.3 under one bag). The same
 *      creation under a frozen direction of OFF proves the GOAL with ZERO
 *      crossings (every bag row HOME_ONLY), and back under BOTH it reproduces
 *      leg A's coordinator digest in-process.
 *
 *   D. OoT's REMAINDER. OoT_Creation_FinishPairedHalf stores exactly the
 *      coordinator's crossings, leaves no OoT host empty and every OoT ice trap
 *      on an OoT host.
 *
 * WHY THE `rando` TIER. Everything here needs a real OoT generation and MM's
 * real region graph; a ROM-free run has neither, and every count would be zero.
 *
 * Linkage note: `#include`d into test_runner.cpp at FILE SCOPE (compiled as C++).
 */

#include "../combo_logic.h"
#include "../combo_single_bag.h"
#include "../context.h"
#include "../crossing_store.h"
#include "../foreign_items.h"
#include "../shared_items.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <vector>

extern "C" {
int Rando_HeadlessSeedTest(const char* seedStr);
int OoT_ComboLogic_GeneralPassDeferred(void);
int OoT_ComboLogic_TestCountIceTraps(void);
int OoT_ComboLogic_TestEmptyHostCount(void);
int OoT_Creation_FinishPairedHalf(int writeSpoiler);
int MM_Rando_HeadlessPairedHalf(void);
int MM_ComboLogic_FixedHarvestCount(void);
int MM_Rando_Foreign_TestIsForeignHostClass(uint16_t randoCheckId);
}

namespace {

#define CSB_ASSERT(cond, msg)                                                                                          \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("[TEST] FAIL (combo-single-bag): %s  [%s:%d]\n", msg, __FILE__, __LINE__);                          \
            return TEST_FAIL;                                                                                          \
        }                                                                                                              \
    } while (0)

/** Both coordinator tables, copied out. */
struct CsbTables {
    std::vector<ComboLogicPlacement> oot;
    std::vector<ComboLogicPlacement> mm;
};

CsbTables CsbCopyTables() {
    CsbTables t;
    ComboLogicPlacement p;
    for (int i = 0; i < Combo_Logic_PlacementCount(GAME_OOT); i++) {
        if (Combo_Logic_PlacementAt(GAME_OOT, i, &p)) {
            t.oot.push_back(p);
        }
    }
    for (int i = 0; i < Combo_Logic_PlacementCount(GAME_MM); i++) {
        if (Combo_Logic_PlacementAt(GAME_MM, i, &p)) {
            t.mm.push_back(p);
        }
    }
    return t;
}

bool CsbHydrate(const CsbTables& t) {
    return Combo_Logic_HydrateTables(t.oot.empty() ? nullptr : t.oot.data(), (int)t.oot.size(),
                                     t.mm.empty() ? nullptr : t.mm.data(), (int)t.mm.size());
}

/** The GOAL over the current tables with nothing assumed: the fill's exit round. */
int CsbGoalNow(uint8_t goal) {
    ComboLogicRoundRequest req;
    memset(&req, 0, sizeof(req));
    req.goal = goal;
    ComboLogicRoundResult res;
    if (Combo_Logic_RunRound(&req, &res) != RSBS_COMBO_LOGIC_OK) {
        return -1;
    }
    return res.goalExpression;
}

int CsbCopiesOf(const CsbTables& t, SharedItem item) {
    int n = 0;
    for (const ComboLogicPlacement& p : t.oot) {
        n += (p.item.originGame == item.originGame && p.item.id == item.id) ? 1 : 0;
    }
    for (const ComboLogicPlacement& p : t.mm) {
        n += (p.item.originGame == item.originGame && p.item.id == item.id) ? 1 : 0;
    }
    return n;
}

/**
 * D5, one direction: find a crossing hosted in `hostGame` whose item has one copy
 * in the whole bag, and show the GOAL provable with it and unprovable without it.
 * @return the witness's index in its host table, or -1 when no single-copy
 *         crossing of this direction is load-bearing.
 */
int CsbFindRemovalWitness(const CsbTables& full, GameId hostGame, uint8_t goal, int* outTried) {
    const std::vector<ComboLogicPlacement>& hosted = (hostGame == GAME_OOT) ? full.oot : full.mm;
    *outTried = 0;
    for (size_t i = 0; i < hosted.size(); i++) {
        const ComboLogicPlacement& row = hosted[i];
        if (row.item.originGame == (uint8_t)hostGame || CsbCopiesOf(full, row.item) != 1) {
            continue;
        }
        (*outTried)++;
        CsbTables without = full;
        std::vector<ComboLogicPlacement>& side = (hostGame == GAME_OOT) ? without.oot : without.mm;
        side.erase(side.begin() + (std::ptrdiff_t)i);
        if (!CsbHydrate(without)) {
            return -1;
        }
        const int withoutGoal = CsbGoalNow(goal);
        if (!CsbHydrate(full)) {
            return -1;
        }
        if (withoutGoal == 0) {
            return (int)i;
        }
    }
    return -1;
}

} // namespace

TestResult ComboSingleBag_Run(void) {
    printf("[TEST] combo-single-bag: the single-bag fill at the creation event over both real engines, and ADR "
           "0010 D5's pair-level locks paired with removal (#645, lane K11)\n");

    // ------------------------------------------------------------------
    // A. The seam.
    // ------------------------------------------------------------------
    CSB_ASSERT(Rando_HeadlessSeedTest("RSBSSINGLEBAG1") == 0, "the paired OoT generation failed");
    CSB_ASSERT(Combo_ForeignPairingActive() && Combo_ComboSettingsFrozen(), "the generation froze no paired identity");
    CSB_ASSERT(OoT_ComboLogic_GeneralPassDeferred() != 0,
               "a paired generation ran OoT's general pass itself — the seam in Fill() is gone");
    const int ootEmptyAtGenerate = OoT_ComboLogic_TestEmptyHostCount();
    printf("[TEST] combo-single-bag: OoT stopped at its general pass with %d empty hosts\n", ootEmptyAtGenerate);
    CSB_ASSERT(ootEmptyAtGenerate > 50, "OoT's general-pass hosts are not empty at Generate");
    CSB_ASSERT(Combo_CountForeignPlacementsOoT() == 0, "the retired reverse overlay pass still placed MM items");
    const uint8_t goal = gComboCtx.comboSettings.goal;

    CSB_ASSERT(MM_Rando_HeadlessPairedHalf() == 0, "MM's creation-time half did not produce a rando world");
    const ComboSingleBagReport bag = *Combo_SingleBag_LastReport();
    printf("[TEST] combo-single-bag: bag %d rows (%d home-only), %d placed, %d crossings into MM, %d into OoT, %d "
           "rounds, %d batch attempt(s), %ums\n",
           bag.bagCount, bag.homeOnlyRows, bag.fill.placed, bag.crossingsIntoMM, bag.crossingsIntoOoT,
           bag.fill.rounds, bag.fill.attempts, bag.wallMs);
    CSB_ASSERT(bag.status == RSBS_COMBO_LOGIC_OK && bag.fill.goalProven, "the single-bag fill did not prove the GOAL");
    CSB_ASSERT(bag.crossingsIntoMM > 0 && bag.crossingsIntoOoT > 0,
               "the shipped direction BOTH crossed nothing in one direction");
    CSB_ASSERT(MM_ComboLogic_FixedHarvestCount() > 0,
               "MM's rounds granted no fixed check content outside the host pool (#737)");

    const CsbTables full = CsbCopyTables();
    const ComboLogicEngine* ootEngine = Combo_Logic_GetEngine(GAME_OOT);
    const ComboLogicEngine* mmEngine = Combo_Logic_GetEngine(GAME_MM);
    CSB_ASSERT(ootEngine != nullptr && mmEngine != nullptr && ootEngine->hostAcceptsForeign != nullptr &&
                   mmEngine->hostAcceptsForeign != nullptr,
               "an engine answers no foreign-host question (ABI 4)");
    for (const ComboLogicPlacement& p : full.oot) {
        CSB_ASSERT(Combo_ItemClassOf(p.item) != RSBS_FILL_CLASS_TRAP, "a trap row was placed by the coordinator");
        if (p.item.originGame == (uint8_t)GAME_MM) {
            CSB_ASSERT(ootEngine->hostAcceptsForeign(ootEngine->self, p.hostCheck) != 0,
                       "an MM item landed on an OoT host whose give path cannot deliver it");
        }
    }
    for (const ComboLogicPlacement& p : full.mm) {
        CSB_ASSERT(Combo_ItemClassOf(p.item) != RSBS_FILL_CLASS_TRAP, "a trap row was placed by the coordinator");
        if (p.item.originGame == (uint8_t)GAME_OOT) {
            CSB_ASSERT(MM_Rando_Foreign_TestIsForeignHostClass(p.hostCheck) != 0,
                       "an OoT item landed on an MM host whose give path cannot deliver it");
        }
    }
    const uint32_t digestBoth = Combo_Logic_PlacementDigest();

    // ------------------------------------------------------------------
    // B. D5, both directions, both halves observed.
    // ------------------------------------------------------------------
    CSB_ASSERT(CsbGoalNow(goal) == 1, "the proven world's exit round does not read GOAL=1");
    {
        int tried = 0;
        const int w = CsbFindRemovalWitness(full, GAME_MM, goal, &tried);
        printf("[TEST] combo-single-bag: D5 (OoT item hosted only in MM): %d single-copy crossings tried, witness %d\n",
               tried, w);
        CSB_ASSERT(w >= 0, "no OoT item hosted only in MM is load-bearing: removing any one leaves the GOAL "
                           "provable (or the probe could not run)");
        const ComboLogicPlacement& row = full.mm[(size_t)w];
        const char* item = Combo_DescribeItemName(row.item);
        const char* host = Combo_DescribeCheckName((uint8_t)GAME_MM, row.hostCheck);
        printf("[TEST] combo-single-bag: D5 witness: OoT %s (id %u) on MM %s (check %u) — provable with it, "
               "unprovable without it\n",
               item != nullptr ? item : "(unnamed)", (unsigned)row.item.id, host != nullptr ? host : "(unnamed)",
               (unsigned)row.hostCheck);
    }
    {
        int tried = 0;
        const int w = CsbFindRemovalWitness(full, GAME_OOT, goal, &tried);
        printf("[TEST] combo-single-bag: D5 (MM item hosted only in OoT): %d single-copy crossings tried, witness %d\n",
               tried, w);
        CSB_ASSERT(w >= 0, "no MM item hosted only in OoT is load-bearing: removing any one leaves the GOAL "
                           "provable (or the probe could not run)");
        const ComboLogicPlacement& row = full.oot[(size_t)w];
        const char* item = Combo_DescribeItemName(row.item);
        const char* host = Combo_DescribeCheckName((uint8_t)GAME_OOT, row.hostCheck);
        printf("[TEST] combo-single-bag: D5 witness: MM %s (id %u) on OoT %s (check %u) — provable with it, "
               "unprovable without it\n",
               item != nullptr ? item : "(unnamed)", (unsigned)row.item.id, host != nullptr ? host : "(unnamed)",
               (unsigned)row.hostCheck);
    }
    CSB_ASSERT(Combo_Logic_PlacementDigest() == digestBoth, "the D5 probes did not put the tables back");
    CSB_ASSERT(CsbGoalNow(goal) == 1, "the restored world no longer proves the GOAL");

    // ------------------------------------------------------------------
    // C. The direction gate under one bag, then BOTH reproduced.
    // ------------------------------------------------------------------
    const uint8_t savedDirection = gComboCtx.comboSettings.direction;
    Combo_Logic_ResetPlacements(); // roll both engines back to the general-pass state
    gComboCtx.comboSettings.direction = (uint8_t)RSBS_COMBO_DIR_OFF;
    const int offRc = MM_Rando_HeadlessPairedHalf();
    const ComboSingleBagReport offBag = *Combo_SingleBag_LastReport();
    gComboCtx.comboSettings.direction = savedDirection;
    printf("[TEST] combo-single-bag: direction OFF: status %s, %d home-only of %d, %d + %d crossings\n",
           Combo_Logic_StatusName(offBag.status), offBag.homeOnlyRows, offBag.bagCount, offBag.crossingsIntoMM,
           offBag.crossingsIntoOoT);
    CSB_ASSERT(offRc == 0 && offBag.status == RSBS_COMBO_LOGIC_OK && offBag.fill.goalProven,
               "a paired world with direction OFF could not be proved");
    CSB_ASSERT(offBag.homeOnlyRows == offBag.bagCount && offBag.crossingsIntoMM == 0 && offBag.crossingsIntoOoT == 0,
               "direction OFF still crossed an item");

    Combo_Logic_ResetPlacements();
    CSB_ASSERT(MM_Rando_HeadlessPairedHalf() == 0, "the BOTH re-run failed");
    CSB_ASSERT(Combo_Logic_PlacementDigest() == digestBoth,
               "the same frozen identity placed a different world on its second in-process creation");

    // ------------------------------------------------------------------
    // D. OoT's remainder.
    // ------------------------------------------------------------------
    const ComboSingleBagReport finalBag = *Combo_SingleBag_LastReport();
    const int crossings = OoT_Creation_FinishPairedHalf(0);
    printf("[TEST] combo-single-bag: OoT's remainder: %d crossings stored, %d OoT hosts empty after, %d ice traps "
           "in OoT (the bag left %d)\n",
           crossings, OoT_ComboLogic_TestEmptyHostCount(), OoT_ComboLogic_TestCountIceTraps(),
           finalBag.compose.perGame[GAME_OOT].rows[RSBS_COMBO_COMPOSE_TRAP]);
    CSB_ASSERT(crossings == finalBag.crossingsIntoMM + finalBag.crossingsIntoOoT,
               "the store did not capture exactly the coordinator's crossings");
    CSB_ASSERT(Combo_Crossings_Count(GAME_MM) == finalBag.crossingsIntoMM &&
                   Combo_Crossings_Count(GAME_OOT) == finalBag.crossingsIntoOoT,
               "the store's per-host counts disagree with the coordinator's");
    CSB_ASSERT(OoT_ComboLogic_TestEmptyHostCount() == 0, "OoT's remainder left a host empty");
    CSB_ASSERT(OoT_ComboLogic_GeneralPassDeferred() == 0, "OoT's world still reads as deferred after its remainder");
    CSB_ASSERT(OoT_ComboLogic_TestCountIceTraps() >= finalBag.compose.perGame[GAME_OOT].rows[RSBS_COMBO_COMPOSE_TRAP],
               "an OoT ice trap was not placed on an OoT host");
    Combo_SingleBag_Forget();
    Combo_Crossings_Clear();

    printf("[TEST] PASS: combo-single-bag\n");
    return TEST_PASS;
}
