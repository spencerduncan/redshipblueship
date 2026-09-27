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
 *   D4. ZERO DEAD HEART PICKUPS over the PRODUCTION creation (PR #744's B7
 *       shape, lane K11b): every max-health row the finished paired world holds
 *       — OoT's final world, MM's shuffled checks, and each crossing's real item
 *       from the crossing store — awarded through the REAL shared carrier from
 *       the frozen starting bar ends the bar at exactly 320 with zero clamped
 *       pickups, and the world holds exactly the heart rows the coordinator
 *       placed (none came back through a per-game pass). Red with the trimmed
 *       rows handed back to the per-game passes as themselves.
 *   E.  CREATION-TIME GIVES READ THE PLACED WORLD: under Link's Pocket =
 *       Anything, Skip Child Zelda with songs Anywhere, and an adult start with
 *       the Master Sword shuffled, the production author order
 *       (OoT_Creation_AuthorRandoFile: the event, THEN Randomizer_InitSaveFile)
 *       gives the item placed at each of those hosts, at least one of which was a
 *       general-pass host still empty at Generate.
 *   E2. THE EVENT'S STEP ORDER (#680's, restored on the second review): the same
 *       real event records "M0S0J0A1" — MM's half authored, the crossings stored
 *       and OoT's remainder placed, the spoiler joined, each with NO armed MM
 *       shadow, and only then the arm. Red with the arm back inside MM's half:
 *       "M1S1J1A1".
 *   E3. ZERO DEAD HEART PICKUPS OVER THE REAL EVENT (leg D4's walk, second
 *       review): the world that same event finished — OoT's final world, MM's
 *       shuffled checks read from the ARMED MM shadow, the crossing store — ends
 *       the shared bar at exactly 320 with zero clamped pickups. D4 walks the
 *       headless harness's creation (the same fill and the same two per-game
 *       passes, without the event's bracket, spoiler and arm) and can also compare
 *       against the coordinator's tables, which the event drops on commit.
 *
 * RSBS_CSB_SAMPLE=N (not set by CTest) turns the row into a MEASUREMENT: N paired
 * creations of consecutive seeds under the shipped per-attempt budget, one line
 * each (ladder attempts, batch attempts, rounds, wall time, status) and a
 * distribution summary. It asserts nothing and exists so the PR's timing claim
 * describes a sample rather than one seed. Each line times the HEADLESS paired
 * creation (OoT's Generate, MM's creation-time half with the fill, OoT's tail with
 * the spoiler off) and walks the finished world's hearts as leg D4 does.
 *
 * RSBS_CSB_SAMPLE_EVENT=N (not set by CTest) measures the REAL creation instead:
 * per seed, OoT's Generate, then OoT_Creation_AuthorRandoFile — the creation event
 * (MM's half, OoT's tail with the spoiler written and joined, the arm) and
 * Randomizer_InitSaveFile — timed end to end, with leg E3's heart walk over the
 * armed world. Save_SaveFile (the slot write) and the overlay's presentation are
 * the only parts of a file-select creation it leaves out.
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
#include "../shared_resources.h"
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
int OoT_ComboLogic_ExportPool(int source, uint16_t* outItems, uint16_t* outHosts, uint16_t* outFlags, int cap);
int MM_ComboLogic_TestShuffledItems(uint16_t* outItems, uint16_t* outChecks, int cap);
int MM_ComboLogic_TestShuffledItemsInShadow(uint16_t* outItems, int cap);
void Randomizer_TestClearOoTSave(void);
const char* OoT_Creation_TestLastSequence(void);
uint32_t OoT_Creation_TestLastUnprovedHalves(void);
}

// THE PLAY-SIDE CHECK (lane K13): award every heart row of a bag, interleaved by
// origin, through the REAL shared carrier. Defined in test_shared_quantity_policy.c
// (included after this file).
int SqpDeadHeartPickups(const ComboLogicBagItem* bag, int count, uint16_t startHealth, int* outPickups,
                        int* outFinal);

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

/** Leg D4's walk over the FINISHED paired world (after OoT's remainder, before
 *  Combo_SingleBag_Forget: the coordinator's tables are still read). */
struct CsbHeartWalk {
    int ootHosted = -1;     // OoT final-world rows read (ExportPool source 1)
    int mmShuffled = -1;    // MM shuffled checks read
    int heartRows = 0;      // max-health rows in the finished world, crossings included
    int placedByBag = 0;    // max-health rows the coordinator placed
    int pickups = 0;        // pickups the carrier walk made
    int dead = -1;          // pickups the 20-heart clamp swallowed
    int finalValue = 0;     // the bar at the end of the walk
    uint16_t startHealth = 0;
};

void CsbAddHeart(std::vector<ComboLogicBagItem>& out, uint8_t origin, uint16_t id) {
    SharedItem item;
    memset(&item, 0, sizeof(item));
    item.originGame = origin;
    item.id = id;
    if (Combo_ItemClassSharedKind(item) != RSBS_SHARED_RES_HEALTH_QUARTERS) {
        return;
    }
    ComboLogicBagItem row;
    memset(&row, 0, sizeof(row));
    row.item = item;
    out.push_back(row);
}

/**
 * @param afterEvent false: the headless harness's world (MM's bytes live, the
 *        coordinator's tables still held, so placedByBag is counted); true: a world
 *        a REAL creation event finished (OoT's bytes live, MM's world only in the
 *        armed shadow, the tables dropped on commit: placedByBag stays -1).
 */
CsbHeartWalk CsbWalkFinishedWorldHearts(const ComboSingleBagReport& bag, bool afterEvent = false) {
    CsbHeartWalk w;
    std::vector<ComboLogicBagItem> hearts;
    std::vector<uint16_t> items(8192, 0);
    w.ootHosted = OoT_ComboLogic_ExportPool(1, items.data(), nullptr, nullptr, (int)items.size());
    for (int i = 0; i < w.ootHosted && i < (int)items.size(); i++) {
        CsbAddHeart(hearts, (uint8_t)GAME_OOT, items[(size_t)i]);
    }
    w.mmShuffled = afterEvent ? MM_ComboLogic_TestShuffledItemsInShadow(items.data(), (int)items.size())
                              : MM_ComboLogic_TestShuffledItems(items.data(), nullptr, (int)items.size());
    for (int i = 0; i < w.mmShuffled && i < (int)items.size(); i++) {
        CsbAddHeart(hearts, (uint8_t)GAME_MM, items[(size_t)i]);
    }
    // A crossing host physically holds its junk cover; the item is the store's.
    for (const GameId host : { GAME_OOT, GAME_MM }) {
        for (int i = 0; i < Combo_Crossings_Count(host); i++) {
            ComboCrossing row;
            if (Combo_Crossings_At(host, i, &row)) {
                CsbAddHeart(hearts, row.item.originGame, row.item.id);
            }
        }
    }
    std::vector<ComboLogicBagItem> placed;
    ComboLogicPlacement p;
    for (const GameId host : { GAME_OOT, GAME_MM }) {
        for (int i = 0; !afterEvent && i < Combo_Logic_PlacementCount(host); i++) {
            if (Combo_Logic_PlacementAt(host, i, &p)) {
                CsbAddHeart(placed, p.item.originGame, p.item.id);
            }
        }
    }
    w.heartRows = (int)hearts.size();
    w.placedByBag = afterEvent ? -1 : (int)placed.size();
    // The shared bar starts at the larger frozen starting health (shared_items.h).
    w.startHealth = bag.startingHealthOoT > bag.startingHealthMM ? bag.startingHealthOoT : bag.startingHealthMM;
    w.dead = SqpDeadHeartPickups(hearts.empty() ? nullptr : hearts.data(), (int)hearts.size(), w.startHealth,
                                 &w.pickups, &w.finalValue);
    return w;
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
        const uint32_t tGen = Combo_GenBudget_NowMs();
        if (Rando_HeadlessSeedTest(seed.c_str()) != 0) {
            printf("[SAMPLE] %s: OoT generation failed\n", seed.c_str());
            other++;
            continue;
        }
        const uint32_t t0 = Combo_GenBudget_NowMs();
        const int rc = MM_Rando_HeadlessPairedHalf();
        const uint32_t ms = Combo_GenBudget_NowMs() - t0;
        const ComboSingleBagReport bag = *Combo_SingleBag_LastReport();
        printf("[SAMPLE] %s: rc=%d status=%s ladder=%d batches=%d (roll-backs %d) rounds=%d bag=%d rows (trimmed "
               "OoT %d MM %d) fill=%ums half=%ums crossings MM<-OoT %d OoT<-MM %d\n",
               seed.c_str(), rc, Combo_Logic_StatusName(bag.status), MM_Rando_PairedGenLastAttempts(),
               bag.fill.attempts, bag.fill.attempts > 0 ? bag.fill.attempts - 1 : 0, bag.fill.rounds, bag.bagCount,
               bag.trimmedOoT, bag.trimmedMM, bag.wallMs, ms, bag.crossingsIntoMM, bag.crossingsIntoOoT);
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
            const uint32_t tTail = Combo_GenBudget_NowMs();
            (void)OoT_Creation_FinishPairedHalf(0);
            const uint32_t tEnd = Combo_GenBudget_NowMs();
            const CsbHeartWalk hw = CsbWalkFinishedWorldHearts(bag);
            printf("[SAMPLE] %s: end to end %ums = OoT Generate %ums + MM half %ums + OoT tail %ums; hearts in the "
                   "world %d (bag placed %d), %d pickups from 0x%X, %d dead, bar ends 0x%X\n",
                   seed.c_str(), tEnd - tGen, t0 - tGen, ms, tEnd - tTail, hw.heartRows, hw.placedByBag, hw.pickups,
                   (unsigned)hw.startHealth, hw.dead, (unsigned)hw.finalValue);
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

/** RSBS_CSB_SAMPLE_EVENT=N: N REAL paired creations (the event + InitSaveFile). */
TestResult CsbSampleEvent(int n) {
    printf("[TEST] combo-single-bag EVENT SAMPLE: %d real paired creations (Generate, then the creation event with "
           "the spoiler written and joined, the arm, and Randomizer_InitSaveFile) under the shipped per-attempt "
           "budget %ums (host scale %u%%)\n",
           n, Combo_GenBudget_FillBudgetMs(0), Combo_GenBudget_HostScalePercent());
    int ok = 0;
    int timeouts = 0;
    int other = 0;
    int deadTotal = 0;
    uint32_t worstMs = 0;
    uint32_t bestMs = 0xFFFFFFFFu;
    uint64_t sumMs = 0;
    int batchHist[RSBS_COMBO_LOGIC_FILL_RETRIES + 2] = { 0 };
    for (int i = 0; i < n; i++) {
        const std::string seed = "RSBSSAMPLE" + std::to_string(i);
        Combo_Crossings_Clear();
        Context_ClearFrozenState(GAME_MM);
        const uint32_t tGen = Combo_GenBudget_NowMs();
        if (Rando_HeadlessSeedTest(seed.c_str()) != 0) {
            printf("[EVENT-SAMPLE] %s: OoT generation failed\n", seed.c_str());
            other++;
            continue;
        }
        Randomizer_TestClearOoTSave();
        const uint32_t t0 = Combo_GenBudget_NowMs();
        const int created = OoT_Creation_AuthorRandoFile(0);
        const uint32_t tEnd = Combo_GenBudget_NowMs();
        const ComboSingleBagReport bag = *Combo_SingleBag_LastReport();
        const uint32_t total = tEnd - tGen;
        printf("[EVENT-SAMPLE] %s: created=%d status=%s ladder=%d batches=%d rounds=%d bag=%d rows fill=%ums; end to "
               "end %ums = OoT Generate %ums + creation event and InitSaveFile %ums; steps %s; crossings MM<-OoT %d "
               "OoT<-MM %d\n",
               seed.c_str(), created, Combo_Logic_StatusName(bag.status), MM_Rando_PairedGenLastAttempts(),
               bag.fill.attempts, bag.fill.rounds, bag.bagCount, bag.wallMs, total, t0 - tGen, tEnd - t0,
               OoT_Creation_TestLastSequence(), bag.crossingsIntoMM, bag.crossingsIntoOoT);
        if (created == 1) {
            ok++;
            const CsbHeartWalk hw = CsbWalkFinishedWorldHearts(bag, true);
            deadTotal += hw.dead > 0 ? hw.dead : 0;
            printf("[EVENT-SAMPLE] %s: hearts in the armed world %d, %d pickups from 0x%X, %d dead, bar ends 0x%X\n",
                   seed.c_str(), hw.heartRows, hw.pickups, (unsigned)hw.startHealth, hw.dead, (unsigned)hw.finalValue);
            worstMs = total > worstMs ? total : worstMs;
            bestMs = total < bestMs ? total : bestMs;
            sumMs += total;
            const int b = bag.fill.attempts < 0 ? 0
                          : bag.fill.attempts > RSBS_COMBO_LOGIC_FILL_RETRIES + 1 ? RSBS_COMBO_LOGIC_FILL_RETRIES + 1
                                                                                  : bag.fill.attempts;
            batchHist[b]++;
        } else if (bag.status == RSBS_COMBO_LOGIC_ERR_ABORTED) {
            timeouts++;
        } else {
            other++;
        }
    }
    printf("[EVENT-SAMPLE] summary: %d/%d created, %d per-attempt budget stops (GenerationTimeout), %d other "
           "failures; end to end mean %ums, best %ums, worst %ums; %d dead heart pickups in total\n",
           ok, n, timeouts, other, ok > 0 ? (unsigned)(sumMs / (uint64_t)ok) : 0u, ok > 0 ? bestMs : 0u, worstMs,
           deadTotal);
    for (int b = 0; b <= RSBS_COMBO_LOGIC_FILL_RETRIES + 1; b++) {
        if (batchHist[b] > 0) {
            printf("[EVENT-SAMPLE] batch attempts %d: %d creation(s)\n", b, batchHist[b]);
        }
    }
    Combo_Crossings_Clear();
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
    if (const char* sample = std::getenv("RSBS_CSB_SAMPLE_EVENT")) {
        const int n = atoi(sample);
        if (n > 0) {
            return CsbSampleEvent(n);
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

    // ------------------------------------------------------------------
    // D4. Zero dead heart pickups over THIS creation's placements.
    // ------------------------------------------------------------------
    {
        const CsbHeartWalk hw = CsbWalkFinishedWorldHearts(finalBag);
        printf("[TEST] combo-single-bag: trimmed OoT %d + MM %d rows to filler; the finished world holds %d heart "
               "rows (%d OoT-hosted rows and %d MM shuffled checks read, the coordinator placed %d); %d pickups from "
               "0x%X: %d dead, the bar ends at %d\n",
               finalBag.trimmedOoT, finalBag.trimmedMM, hw.heartRows, hw.ootHosted, hw.mmShuffled, hw.placedByBag,
               hw.pickups, (unsigned)hw.startHealth, hw.dead, hw.finalValue);
        CSB_ASSERT(finalBag.trimmedOoT + finalBag.trimmedMM > 0,
                   "the shipped profile's creation trimmed nothing, so this leg would prove nothing");
        CSB_ASSERT(hw.ootHosted > 0 && hw.mmShuffled > 0, "the finished world could not be read");
        CSB_ASSERT(finalBag.startingHealthOoT != 0u && finalBag.startingHealthMM != 0u,
                   "the creation did not publish both frozen starting healths to the trim");
        CSB_ASSERT(hw.heartRows == hw.placedByBag,
                   "the finished world holds heart rows the coordinator did not place: a trimmed row came back "
                   "through a per-game pass as itself");
        CSB_ASSERT(hw.pickups > 0 && hw.dead == 0 && hw.finalValue == (int)RSBS_SHARED_RES_MAX_HEALTH_QUARTERS,
                   "the creation's hearts do not end the shared bar at exactly 320 with zero dead pickups");
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
        // No armed MM shadow going in, so E2 reads only what THIS event armed.
        Context_ClearFrozenState(GAME_MM);
        CSB_ASSERT(OoT_Creation_AuthorRandoFile(0) == 1, "the paired creation under the creation-give settings failed");

        // E2. The event's step order.
        const char* steps = OoT_Creation_TestLastSequence();
        printf("[TEST] combo-single-bag: the creation event's steps: %s (expected M0S0J0A1)\n", steps);
        CSB_ASSERT(std::strcmp(steps, "M0S0J0A1") == 0,
                   "the creation event did not run MM's half, the crossings and OoT's remainder, and the spoiler join "
                   "with no armed MM shadow and THEN arm it (#680's order)");
        CSB_ASSERT(Context_HasFrozenState(GAME_MM) != 0, "the finished creation left no armed MM shadow");

        // E2b. ADR 0010 section 1.2's creation warning is computed by the real
        // event from its own fill, and under the frozen goal of this world (the
        // shipped beat-both, both halves proved) it names nothing. A creation
        // that never asked leaves the sentinel, which is red here too.
        {
            const ComboSingleBagReport warnBag = *Combo_SingleBag_LastReport();
            const uint32_t warned = OoT_Creation_TestLastUnprovedHalves();
            printf("[TEST] combo-single-bag: the creation warning under GOAL %u: halves without proof 0x%X (proof "
                   "halves OoT %d MM %d)\n",
                   (unsigned)gComboCtx.comboSettings.goal, (unsigned)warned, warnBag.fill.goalOoT, warnBag.fill.goalMM);
            CSB_ASSERT(warned == Combo_Logic_UnprovedHalves(&warnBag.fill),
                       "the creation event did not compute its warning from its own fill");
            CSB_ASSERT(gComboCtx.comboSettings.goal != (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH ||
                           (warned == 0u && warnBag.fill.goalOoT == 1 && warnBag.fill.goalMM == 1),
                       "a beat-both creation must prove both halves and warn about neither");
        }

        // E3. Zero dead heart pickups over the world this real event finished.
        {
            const ComboSingleBagReport eventBag = *Combo_SingleBag_LastReport();
            const CsbHeartWalk hw = CsbWalkFinishedWorldHearts(eventBag, true);
            printf("[TEST] combo-single-bag: the real event's world: %d heart rows (%d OoT-hosted rows and %d MM "
                   "shuffled checks read from the armed shadow, %d crossings); %d pickups from 0x%X: %d dead, the bar "
                   "ends at %d\n",
                   hw.heartRows, hw.ootHosted, hw.mmShuffled,
                   Combo_Crossings_Count(GAME_OOT) + Combo_Crossings_Count(GAME_MM), hw.pickups,
                   (unsigned)hw.startHealth, hw.dead, hw.finalValue);
            CSB_ASSERT(eventBag.trimmedOoT + eventBag.trimmedMM > 0,
                       "the real event's creation trimmed nothing, so E3 would prove nothing");
            CSB_ASSERT(hw.ootHosted > 0 && hw.mmShuffled > 0, "the real event's finished world could not be read");
            CSB_ASSERT(hw.pickups > 0 && hw.dead == 0 && hw.finalValue == (int)RSBS_SHARED_RES_MAX_HEALTH_QUARTERS,
                       "the real event's hearts do not end the shared bar at exactly 320 with zero dead pickups");
        }
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
