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
 * PR #743 REVIEW LEGS, each with its red half observed when it was written:
 *
 *   A2. OoT's foreign-host rule, swept over every OoT location by category (shop,
 *       scrub, merchant, chest game, a shop-ish NAME, non-EN_BOX): all rejected,
 *       and some chests accepted. Leg A asks the same predicate the fill used, so
 *       it could not see the predicate itself loosen.
 *   C2. THE ITEM-CLASS GATE: OoT's frozen itemClass without PROGRESSION makes
 *       every OoT row HOME_ONLY — zero OoT items in Termina — while MM's rows
 *       still cross. Red with SingleBagOriginMayCross's class conjunct deleted.
 *   D2. PAIRED-WORLD HINTS: the #441 checks over the hints OoT's remainder wrote
 *       for THIS world, plus no crossing host hintable or hinted, and an OoT item
 *       placed in MM hinted as Termina (Rando_ValidatePairedWorldHints).
 *   D3. A REFUSAL IS NOT A RUNG: a second creation over an OoT world that already
 *       finished its general pass is refused by the fill (BAD_REQUEST) and fails
 *       on its FIRST ladder attempt, not after ten, and not as "exhausted".
 *   E.  CREATION-TIME GIVES READ THE PLACED WORLD: under Link's Pocket =
 *       Anything, Skip Child Zelda with songs Anywhere, and an adult start with
 *       the Master Sword shuffled, the production author order
 *       (OoT_Creation_AuthorRandoFile: the event, THEN Randomizer_InitSaveFile)
 *       gives the item placed at each of those hosts, at least one of which was a
 *       general-pass host still empty at Generate.
 *
 * RSBS_CSB_SAMPLE=N (not set by CTest) turns the row into a MEASUREMENT: N paired
 * creations of consecutive seeds under the shipped per-attempt budget, one line
 * each (ladder attempts, batch attempts, rounds, wall time, status) and a
 * distribution summary. It asserts nothing and exists so the PR's timing claim
 * describes a sample rather than one seed.
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
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
int Rando_HeadlessSeedTest(const char* seedStr);
int OoT_ComboLogic_GeneralPassDeferred(void);
int OoT_ComboLogic_TestCountIceTraps(void);
int OoT_ComboLogic_TestEmptyHostCount(void);
int OoT_Creation_FinishPairedHalf(int writeSpoiler);
int MM_Rando_HeadlessPairedHalf(void);
int MM_ComboLogic_FixedHarvestCount(void);
int MM_Rando_Foreign_TestIsForeignHostClass(uint16_t randoCheckId);
int MM_Rando_PairedGenLastAttempts(void);
int MM_Rando_PairedGenLastExhausted(void);
int OoT_ComboLogic_TestSweepForeignHostRule(int* outCounts);
int Rando_ValidatePairedWorldHints(void);
int OoT_Creation_AuthorRandoFile(int slot);
void Randomizer_TestResetStartingGiveLog(void);
int Randomizer_TestCreationGiveHost(int i);
int Randomizer_TestHostEmpty(int rc);
int Randomizer_TestStartingGiveMatchesPlacement(int rc);
void Randomizer_TestClearOoTSave(void);
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

/** The Rando settings leg E generates under, as CVars (restored after). */
const char* const kCsbGiveCVars[][2] = {
    { "gRandoSettings.LinksPocket", "2" },        // RO_LINKS_POCKET_ANYTHING
    { "gRandoSettings.SkipChildZelda", "1" },     // skip: Impa's song is given at creation
    { "gRandoSettings.ShuffleSongs", "3" },       // RO_SONG_SHUFFLE_ANYWHERE: Impa is a general-pass host
    { "gRandoSettings.StartingAge", "1" },        // RO_AGE_ADULT
    { "gRandoSettings.DoorOfTime", "2" },         // open, so an adult start is legal
    { "gRandoSettings.ShuffleMasterSword", "1" }, // the ToT pedestal is a general-pass host
};

/** RSBS_CSB_SAMPLE=N: N paired creations under the shipped budget, measured. */
TestResult CsbSample(int n) {
    printf("[TEST] combo-single-bag SAMPLE: %d paired creations under the shipped per-attempt budget %ums "
           "(host scale %u%%)\n",
           n, Combo_GenBudget_FillBudgetMs(0), Combo_GenBudget_HostScalePercent());
    int ok = 0;
    int timeouts = 0;
    int other = 0;
    int batchHist[RSBS_COMBO_LOGIC_FILL_RETRIES + 2] = { 0 };
    uint32_t worstMs = 0;
    uint64_t sumMs = 0;
    int maxSide = 0;
    for (int i = 0; i < n; i++) {
        const std::string seed = "RSBSSAMPLE" + std::to_string(i);
        if (Rando_HeadlessSeedTest(seed.c_str()) != 0) {
            printf("[SAMPLE] %s: OoT generation failed\n", seed.c_str());
            other++;
            continue;
        }
        const uint32_t t0 = Combo_GenBudget_NowMs();
        const int rc = MM_Rando_HeadlessPairedHalf();
        const uint32_t ms = Combo_GenBudget_NowMs() - t0;
        const ComboSingleBagReport bag = *Combo_SingleBag_LastReport();
        printf("[SAMPLE] %s: rc=%d status=%s ladder=%d batches=%d rounds=%d fill=%ums half=%ums crossings MM<-OoT %d "
               "OoT<-MM %d\n",
               seed.c_str(), rc, Combo_Logic_StatusName(bag.status), MM_Rando_PairedGenLastAttempts(),
               bag.fill.attempts, bag.fill.rounds, bag.wallMs, ms, bag.crossingsIntoMM, bag.crossingsIntoOoT);
        if (rc == 0) {
            ok++;
            const int b = bag.fill.attempts < 0 ? 0
                          : bag.fill.attempts > RSBS_COMBO_LOGIC_FILL_RETRIES + 1 ? RSBS_COMBO_LOGIC_FILL_RETRIES + 1
                                                                                  : bag.fill.attempts;
            batchHist[b]++;
            worstMs = ms > worstMs ? ms : worstMs;
            sumMs += ms;
            maxSide = bag.crossingsIntoMM > maxSide ? bag.crossingsIntoMM : maxSide;
            maxSide = bag.crossingsIntoOoT > maxSide ? bag.crossingsIntoOoT : maxSide;
            (void)OoT_Creation_FinishPairedHalf(0);
        } else if (bag.status == RSBS_COMBO_LOGIC_ERR_ABORTED) {
            timeouts++;
        } else {
            other++;
        }
        Combo_SingleBag_Forget();
        Combo_Crossings_Clear();
    }
    printf("[SAMPLE] summary: %d/%d created, %d per-attempt budget stops (GenerationTimeout), %d other failures; "
           "MM half mean %ums, worst %ums; largest per-side crossing count %d\n",
           ok, n, timeouts, other, ok > 0 ? (unsigned)(sumMs / (uint64_t)ok) : 0u, worstMs, maxSide);
    for (int b = 0; b <= RSBS_COMBO_LOGIC_FILL_RETRIES + 1; b++) {
        if (batchHist[b] > 0) {
            printf("[SAMPLE] batch attempts %d: %d creation(s)\n", b, batchHist[b]);
        }
    }
    return TEST_PASS;
}

} // namespace

TestResult ComboSingleBag_Run(void) {
    if (const char* sample = std::getenv("RSBS_CSB_SAMPLE")) {
        const int n = atoi(sample);
        if (n > 0) {
            return CsbSample(n);
        }
    }
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
    CSB_ASSERT(bag.crossingsIntoMM <= (int)RSBS_CROSSINGS_PER_SIDE_MAX &&
                   bag.crossingsIntoOoT <= (int)RSBS_CROSSINGS_PER_SIDE_MAX,
               "a side hosts more crossings than the shared-item array can deliver before a redemption");
    CSB_ASSERT(MM_ComboLogic_FixedHarvestCount() > 0,
               "MM's rounds granted no fixed check content outside the host pool (#737)");

    const CsbTables full = CsbCopyTables();
    const ComboLogicEngine* ootEngine = Combo_Logic_GetEngine(GAME_OOT);
    const ComboLogicEngine* mmEngine = Combo_Logic_GetEngine(GAME_MM);
    CSB_ASSERT(ootEngine != nullptr && mmEngine != nullptr && ootEngine->hostAcceptsForeign != nullptr &&
                   mmEngine->hostAcceptsForeign != nullptr,
               "an engine answers no foreign-host question (ABI 5)");
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
    // A2. OoT's foreign-host rule, swept by category (PR #743 review).
    // ------------------------------------------------------------------
    {
        int counts[7];
        const int violations = OoT_ComboLogic_TestSweepForeignHostRule(counts);
        printf("[TEST] combo-single-bag: OoT host rule: shop %d, scrub %d, merchant %d, chest game %d, shop-ish name "
               "%d, non-EN_BOX %d rows all rejected; %d accepted; %d violation(s)\n",
               counts[0], counts[1], counts[2], counts[3], counts[4], counts[5], counts[6], violations);
        CSB_ASSERT(violations == 0, "OoT's foreign-host predicate accepts a shop, scrub, merchant, chest-game or "
                                    "non-chest location");
        CSB_ASSERT(counts[0] > 0 && counts[1] > 0 && counts[2] > 0 && counts[3] > 0 && counts[4] > 0 && counts[5] > 0,
                   "a category of the host-rule sweep is empty, so it proves nothing about that category");
        CSB_ASSERT(counts[6] > 0, "OoT's predicate accepts no location at all");
    }

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

    // ------------------------------------------------------------------
    // C2. The item-class gate: OoT's frozen class set without PROGRESSION.
    // ------------------------------------------------------------------
    {
        const uint16_t savedClassOoT = gComboCtx.comboSettings.itemClassOoT;
        Combo_Logic_ResetPlacements();
        gComboCtx.comboSettings.itemClassOoT = (uint16_t)(savedClassOoT & ~(uint16_t)RSBS_ITEMCLASS_PROGRESSION);
        const int classRc = MM_Rando_HeadlessPairedHalf();
        const ComboSingleBagReport classBag = *Combo_SingleBag_LastReport();
        gComboCtx.comboSettings.itemClassOoT = savedClassOoT;
        const int ootBagRows = classBag.compose.perGame[GAME_OOT].rows[RSBS_COMBO_COMPOSE_REQUIRED] +
                               classBag.compose.perGame[GAME_OOT].rows[RSBS_COMBO_COMPOSE_SURPLUS] +
                               classBag.compose.perGame[GAME_OOT].rows[RSBS_COMBO_COMPOSE_CONFINED];
        printf("[TEST] combo-single-bag: OoT class without PROGRESSION: status %s, %d home-only of %d (OoT rows %d), "
               "%d OoT items in MM, %d MM items in OoT\n",
               Combo_Logic_StatusName(classBag.status), classBag.homeOnlyRows, classBag.bagCount, ootBagRows,
               classBag.crossingsIntoMM, classBag.crossingsIntoOoT);
        CSB_ASSERT(classRc == 0 && classBag.status == RSBS_COMBO_LOGIC_OK && classBag.fill.goalProven,
                   "a paired world with OoT's PROGRESSION class off could not be proved");
        CSB_ASSERT(ootBagRows > 0 && classBag.homeOnlyRows >= ootBagRows,
                   "OoT's rows were not all HOME_ONLY under a frozen class set without PROGRESSION");
        CSB_ASSERT(classBag.crossingsIntoMM == 0,
                   "an OoT item crossed into Termina although OoT's frozen class set has no PROGRESSION bit");
        CSB_ASSERT(classBag.crossingsIntoOoT > 0,
                   "MM's items stopped crossing too, so the gate is not per-origin (or the leg proves nothing)");
    }

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

    // ------------------------------------------------------------------
    // D2. The hints OoT's remainder just wrote, for THIS paired world.
    // ------------------------------------------------------------------
    {
        const int hintRc = Rando_ValidatePairedWorldHints();
        printf("[TEST] combo-single-bag: paired-world hint validity rc=%d\n", hintRc);
        CSB_ASSERT(hintRc == 0, "a paired world's hints name a crossing host, leave one hintable, point at no OoT "
                                "location without naming Termina, or fail a #441 check (see [rando-hints])");
    }
    Combo_SingleBag_Forget();

    // ------------------------------------------------------------------
    // D3. A refusal is not a rung: OoT's world has finished its general pass,
    //     so a second creation over it is refused by the fill, on attempt 1.
    // ------------------------------------------------------------------
    {
        const int againRc = MM_Rando_HeadlessPairedHalf();
        const ComboSingleBagReport again = *Combo_SingleBag_LastReport();
        printf("[TEST] combo-single-bag: second creation over a finished OoT world: rc=%d status %s, %d ladder "
               "attempt(s), exhausted=%d\n",
               againRc, Combo_Logic_StatusName(again.status), MM_Rando_PairedGenLastAttempts(),
               MM_Rando_PairedGenLastExhausted());
        CSB_ASSERT(againRc != 0 && again.status == RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                   "a creation over an OoT world that already finished its general pass was not refused");
        CSB_ASSERT(MM_Rando_PairedGenLastAttempts() == 1 && MM_Rando_PairedGenLastExhausted() == 0,
                   "the fill's refusal climbed the attempt ladder as if it were a world dead end");
    }
    Combo_SingleBag_Forget();
    Combo_Crossings_Clear();

    // ------------------------------------------------------------------
    // E. Creation-time gives read the PLACED world (the production order).
    // ------------------------------------------------------------------
    {
        std::vector<std::string> savedCVars;
        for (const auto& cv : kCsbGiveCVars) {
            savedCVars.push_back(std::to_string(CVarGetInteger(cv[0], -1)));
            CVarSetInteger(cv[0], atoi(cv[1]));
        }
        struct CVarRestore {
            const std::vector<std::string>* saved;
            ~CVarRestore() {
                for (size_t i = 0; i < saved->size(); i++) {
                    const int v = atoi((*saved)[i].c_str());
                    if (v < 0) {
                        CVarClear(kCsbGiveCVars[i][0]);
                    } else {
                        CVarSetInteger(kCsbGiveCVars[i][0], v);
                    }
                }
            }
        } restoreCVars{ &savedCVars };

        CSB_ASSERT(Rando_HeadlessSeedTest("RSBSSINGLEBAGGIVES") == 0,
                   "the paired generation under the creation-give settings failed");
        CSB_ASSERT(OoT_ComboLogic_GeneralPassDeferred() != 0, "the creation-give world did not stop at its general pass");
        bool emptyAtGenerate[3] = { false, false, false };
        int generalPassHosts = 0;
        for (int i = 0; i < 3; i++) {
            emptyAtGenerate[i] = Randomizer_TestHostEmpty(Randomizer_TestCreationGiveHost(i)) != 0;
            generalPassHosts += emptyAtGenerate[i] ? 1 : 0;
        }
        printf("[TEST] combo-single-bag: creation-give hosts empty at Generate: pocket=%d impa=%d master-sword=%d\n",
               emptyAtGenerate[0] ? 1 : 0, emptyAtGenerate[1] ? 1 : 0, emptyAtGenerate[2] ? 1 : 0);
        CSB_ASSERT(generalPassHosts > 0, "none of the creation-give hosts is a general-pass host under these "
                                         "settings, so the order this leg locks is never exercised");

        Randomizer_TestClearOoTSave();
        Randomizer_TestResetStartingGiveLog();
        CSB_ASSERT(OoT_Creation_AuthorRandoFile(0) == 1, "the paired creation under the creation-give settings failed");
        for (int i = 0; i < 3; i++) {
            const int rc = Randomizer_TestCreationGiveHost(i);
            const int verdict = Randomizer_TestStartingGiveMatchesPlacement(rc);
            printf("[TEST] combo-single-bag: creation give at check %d (empty at Generate %d): verdict %d\n", rc,
                   emptyAtGenerate[i] ? 1 : 0, verdict);
            CSB_ASSERT(verdict != -2, "a creation-give host is still empty after the creation");
            CSB_ASSERT(verdict != -1, "Randomizer_InitSaveFile gave nothing for a creation-give host");
            CSB_ASSERT(verdict == 1, "a creation-time give handed out an item other than the one placed at its host "
                                     "(it read the host before the single bag placed it)");
        }
    }
    Combo_SingleBag_Forget();
    Combo_Crossings_Clear();

    printf("[TEST] PASS: combo-single-bag\n");
    return TEST_PASS;
}
