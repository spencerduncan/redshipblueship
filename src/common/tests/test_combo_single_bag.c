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
 *   F. THE GOAL RUNS THROUGH MM's FIXED CONTENTS (#737). On the shipped profile
 *      the boss remains are not shuffled: their checks sit outside GeneratePools'
 *      check pool and keep their vanilla items, which the player collects in play
 *      and MM's own fill credits. Over THIS proven world's tables, the exit round
 *      under beat-both reads goalMM=1 with MM's engine granting reached fixed
 *      contents (A7), and goalMM=0 — beat-both 0 — with that grant switched off
 *      (MM_ComboLogic_TestSetFixedGrants, test only): the red half, observed in
 *      every run rather than once on an older binary. Switched off, goalOoT falls
 *      to 0 as well (observed, printed, not asserted): OoT items the fill hosted in
 *      MM sit behind MM checks whose reach needs fixed MM contents, so without the
 *      grant the pair is unprovable from both ends. Then, still switched off,
 *      assuming the fixed remains contents restores goalMM=1 (goalOoT is
 *      printed), which names the remains as the fixed content MM's own goal runs
 *      through. The switch is put back before anything is asserted. (Lane G1;
 *      runs between B and C.)
 *      AMENDED (#697 fix pass, PR #763): the assumption is the fixed remains PLUS
 *      every fixed content MM classes RENEWABLE (ammo, refills, rupees: the items
 *      a pot or shop hands out again). The Twinmold tightening moved this pinned
 *      world so that a Deku Stick, which in this world sits only on fixed pots and a
 *      shop row, became load-bearing for Majora; seven single-content probes
 *      named it and nothing else. The claim the arm pins is therefore "no fixed
 *      PROGRESSION content other than the remains is load-bearing", and a fixed
 *      renewable is regainable in play by construction. The remains-only round is
 *      still run and printed, and the red half is untouched.
 *
 * PR #743 REVIEW LEGS, each with its red half observed when it was written:
 *
 *   A2. OoT's foreign-host rule, swept over every OoT location by category: every
 *       plain shop shelf (#800 pass 1), every Business Scrub, every merchant and
 *       the treasure chest game's reward (#800 pass 2) accepted, the reward
 *       collected by an ItemGetInf flag the room never resets; the chest game's
 *       room chests (RCTYPE_CHEST_GAME), a shop-ish NAME that is neither a shelf,
 *       a merchant nor the reward, and every other non-chest rejected; and
 *       some chests accepted. Leg A asks the same predicate the fill used, so it
 *       could not see the predicate itself loosen. Swept after leg A, asserted at
 *       the end of the row (after leg G).
 *   C2. THE ITEM-CLASS GATE: OoT's frozen itemClass without PROGRESSION makes
 *       every OoT row HOME_ONLY — zero OoT items in Termina — while MM's rows
 *       still cross. Red with SingleBagOriginMayCross's class conjunct deleted.
 *   D2. PAIRED-WORLD HINTS: the #441 checks over the hints OoT's remainder wrote
 *       for THIS world, plus every crossing host hintable, counted major by the
 *       "Named Item" filter exactly when MM grades its item major, and, where a
 *       hint names one, named by the MM item it yields, never its cover (#575 H3b), and an
 *       OoT item placed in MM hinted by its MM area (Rando_ValidatePairedWorldHints).
 *       #575 H5a: its P7 recomputes the foolish rule over OoT's own areas against the
 *       barren flags and the Foolish hints drawn; and here, every placement the
 *       coordinator made on an MM host reads IMPORTANT through MM's read-time
 *       answer (which sees only the save and the store, never these tables), so
 *       an MM stone can never call the area of a bag row foolish.
 *   D2b. A SOLO WORLD AFTER A PAIRED CREATION NAMES NO CROSSED ITEM (#575 H3b): the
 *       crossing store outlives its creation, so OoT's native general pass is run
 *       after D3 with the paired world's crossings still in the store, and its
 *       hints name none of them (Rando_ValidateSoloWorldHintsAfterPairing).
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
 *   G.  AN MM ITEM ON AN OoT SHOP SHELF (#800 pass 1): a paired creation on a
 *       fixed seed with OoT's shopsanity (four emptied shelves per shop) and MM's
 *       shop shuffle on places at least one MM item on an OoT RCTYPE_SHOP shelf;
 *       the crossing store answers the shelf's check with it; the drain's
 *       recording core records exactly one MM crossing for it; and once the check
 *       is collected the shelf reads sold out and records nothing more. Red with
 *       the shelf refused by OoT's foreign-host predicate: zero shelf crossings.
 *       #800 pass 2: the same creation also has OoT's scrub shuffle on (All), and
 *       the same assertions hold for an MM item on an OoT Business Scrub. Red
 *       with the scrub refused by the predicate: zero scrub crossings.
 *       #800 pass 2, merchants: the same creation also has OoT's merchant
 *       shuffle on (All), the same assertions hold for an MM item on a merchant
 *       (the bean salesman, Medigoron, Granny's Shop, the carpet salesman), and
 *       each of the four, given an MM item (the crossing store re-hydrated with
 *       one on every merchant), sells its randomized item until its check is
 *       collected and not after, read from the merchant's own gate (the bean
 *       salesman has none: his two real sale hooks are driven, and "sells" is
 *       his sale charging his price and setting his flag; "not after" is no
 *       charge, no flag, and his eligibility hook letting a broke "Yes" reach
 *       the sold-out answer). All four rows print before the verdict. Red with
 *       merchants refused by the predicate: zero merchant crossings; red with
 *       the gates and the bean salesman's sale-hook branch unchanged: every
 *       collected merchant still sells; red with only his eligibility term
 *       unchanged: his row reads 2.
 *       #800 pass 2, the treasure chest game: its reward, given an MM item that
 *       OoT draws from MM's descriptor (the store re-hydrated as for the
 *       merchants), is the check the game's own resolvers name for the final
 *       chest, for the ItemGetInf flag its opening sets and for the Lens of Truth
 *       display above it, and none of them names it once the prize is won (the
 *       room resets its chests every play); the flag-set hook queues it for the
 *       drain, which records exactly one MM crossing and nothing once it is
 *       collected; and the Lens display's REAL draw emits exactly MM's model.
 *       Red with the reward refused by the predicate: the A2 sweep; red with the
 *       display's draw unchanged: none of MM's model lists (0 emitted; the
 *       reader counts only path-named "__OTR__" lists, and the junk cover's
 *       draw emits none of them).
 *
 * #564 V2 LEGS: A STAGED GENERATION IS CONSUMED EXACTLY ONCE, AT FILE CREATION.
 *
 *   V2a. Leg E's creation is staged the way the file select stages one (Generate
 *        marks the context generated: OoT_Creation_TestStageGeneration). After
 *        the real author order (OoT_Creation_AuthorRandoFile) creates file 1, no
 *        generation is staged (Randomizer_IsSeedGenerated, which the file select's
 *        "Start Randomizer" and OoT_Sram_InitSave's isRandoFile both read, is 0),
 *        nothing waits at a general pass, and the randomizer menu's line under
 *        "Start Randomizer" (OoT_FileSelect_NoSeedText, drawn where SoH draws
 *        "No randomizer seed loaded") says the seed was used for file 1. That is
 *        where a second file from the same generation is turned away: before any
 *        creation work, not by the second paired fill's refusal (leg D3).
 *   V2b. The file select's own per-frame update (FileChoose_UpdateRandomizer, as
 *        its main menu runs it, with FileChoose_Init's "nothing auto-loaded yet")
 *        does not reload the spoiler that creation just wrote: no parse, no toast,
 *        and the line still names file 1. Without that, every return to the file
 *        select after a creation would refuse the paired spoiler with a toast.
 *   #936. A LATER LAUNCH of the same file select: V2b's consumption record is
 *        process-local, so a fresh process has none, nothing staged and nothing
 *        loaded, and gGeneral.SpoilerLog still names the paired spoiler. The
 *        per-frame update must not parse it (a general-pass mark ParseSpoiler
 *        would clear survives) and must queue no toast; the same file DROPPED
 *        is an explicit load and is still refused with the paired-spoiler toast.
 *        Under DontGenerateSpoiler the skipped spoiler is still deleted, as SoH's
 *        file select deletes any spoiler it finds then (no parse, no toast).
 *   V2c. A new generation forgets the consumed one: after leg G's Generate the
 *        line is SoH's own again.
 *   V2d. A FAILED creation consumes its staged generation too (the event has
 *        already retracted the world's identity, so nothing could create from it):
 *        nothing staged, nothing waiting at a general pass, SoH's own line (the
 *        "Not created:" toast is the failure's surface). At the end of the row.
 *
 * #564 V23 LEGS: ONE SPOILER ARTIFACT, AND NO PAIR OVERWRITES ANOTHER'S.
 *
 *   V23a. File 1's creation (leg E) leaves MM's spoiler selection as it found it
 *        (gRando.SpoilerFile empty, gRando.SpoilerFileIndex 0). MM's paired
 *        generation used to write its own RSBSPAIR<masterSeed>.json and select it
 *        (RefreshOptions found it in the folder, so the index was nonzero), and the
 *        join deleted the file afterwards: a transient second artifact, and a
 *        selection a later solo MM file would LOAD from.
 *   V23b. A second pair with the same OoT seed and OoT settings but one MM option
 *        changed has a different identity (mmProfileDigest) and the SAME OoT
 *        spoiler path, because OoT names its spoiler by its own hash icons alone.
 *        Its real creation must not rewrite the first pair's spoiler: the bytes at
 *        that path are unchanged, and the creation still succeeds (a spoiler is a
 *        report, so its refusal never fails a creation).
 *   V23c. In the same creation, gGeneral.SpoilerLog names a solo OoT document (no
 *        "combo" section: a dropped file, an older build's spoiler) that this
 *        creation did not write. OoT's writer refuses the aliased path, so the join
 *        must not run against whatever the CVar names: that document is unchanged,
 *        and the CVar does not name it afterwards (the Randomizer menu's "Spoiler
 *        File:" line would show it as this world's).
 *   V23d. The join itself (MM_Rando_AugmentSpoilerWithPairedHalf), handed the
 *        first pair's spoiler while the second pair is resident, refuses (-5) and
 *        leaves it byte-identical: the defence in depth behind V23c.
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
 * RSBS_CSB_FOOLISH=N (not set by CTest) is epic #575 H4's MEASUREMENT of the
 * pair-level foolish refinement, before anything wires it in: per creation (the
 * pinned seed RSBSSINGLEBAG1, then RSBSSAMPLE0..N-1), on the profile
 * RSBS_COMBO_PROFILE names (shipped | plentiful | armed, test_combo_logic_measure.c),
 * a probe runs right after Combo_Logic_RunFill succeeds with both engines live
 * (Combo_SingleBag_TestSetPostFillProbe) and times, over the coordinator's own
 * placements ("bag hosts"):
 *   (a) THE REQUIRED SET, OoTMM `analysis.ts` makeRequiredLocs: one round per bag
 *       host with that host forbidden (ABI 6); required iff the GOAL falls;
 *   (b) OoTMM's MONTE-CARLO ZIG-ZAG, `analysis-foolish.ts` (down / up passes over
 *       a growing forbidden set, repeated until three passes find nothing new),
 *       over the bag hosts (a) did not call required, with its own RNG seeded
 *       from the frozen identity (Combo_SingleBag_FoolishSeed).
 * It prints per creation and a summary against the #582 per-attempt budget, and
 * asserts nothing. RSBS_CSB_FOOLISH_ROUND_CAP bounds (b) per creation (default
 * 4000 rounds) so a runaway zig-zag is reported as capped, not waited on.
 *
 * WHY THE `rando` TIER. Everything here needs a real OoT generation and MM's
 * real region graph; a ROM-free run has neither, and every count would be zero.
 *
 * Linkage note: `#include`d into test_runner.cpp at FILE SCOPE (compiled as C++).
 */

#include "../combo_area.h"
#include "../combo_logic.h"
#include "../combo_single_bag.h"
#include "../context.h"
#include "../crossing_store.h"
#include "../foreign_items.h"
#include "../foreign_model.h"
#include "../notification_bridge.h"
#include "../pairing_refusal_toast.h"
#include "../shared_items.h"
#include "../shared_resources.h"
#include "../test_runner.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

extern "C" {
int Rando_HeadlessSeedTest(const char* seedStr);
int OoT_ComboLogic_GeneralPassDeferred(void);
int OoT_ComboLogic_TestCountIceTraps(void);
int OoT_ComboLogic_TestEmptyHostCount(void);
int OoT_Creation_FinishPairedHalf(int writeSpoiler);
int MM_Rando_HeadlessPairedHalf(void);
int MM_ComboLogic_FixedHarvestCount(void);
int MM_ComboLogic_TestSetFixedGrants(int enabled);
int MM_ComboLogic_TestFixedContents(int remainsOnly, uint16_t* outItems, uint16_t* outChecks, int cap);
int MM_ComboLogic_ClassifyItem(uint16_t id, ComboItemClassRow* out);
int MM_Rando_Foreign_TestIsForeignHostClass(uint16_t randoCheckId);
int MM_Rando_PairedGenLastAttempts(void);
int MM_Rando_PairedGenLastExhausted(void);
int OoT_ComboLogic_TestSweepForeignHostRule(int* outCounts);
int OoT_ComboLogic_TestIsShopShelf(uint16_t hostCheck);
int OoT_ComboLogic_TestIsScrub(uint16_t hostCheck);
int OoT_ComboLogic_TestIsMerchant(uint16_t hostCheck);
int OoT_Rando_Foreign_TestMerchantSells(uint16_t rc);
int OoT_ComboLogic_TestMerchantChecks(uint16_t* out, int cap);
int OoT_Rando_Foreign_TestChestGamePrizeCheck(int which);
int OoT_Rando_Foreign_TestChestGameRewardQueued(void);
int OoT_ForeignModel_TestChestGamePrizeDraw(const ComboModel* want);
int OoT_Rando_Foreign_RecordPickup(uint16_t rc);
int OoT_Rando_Foreign_TestSetObtained(uint16_t rc, int obtained);
int OoT_Rando_Foreign_HostsForeign(uint16_t rc);
int OoT_Rando_Foreign_HostCollected(uint16_t rc);
int Rando_ValidatePairedWorldHints(void);
int Rando_ValidateSoloWorldHintsAfterPairing(void);
int OoT_ComboLogic_TestSetNativeGeneralPass(int native);
int OoT_Creation_AuthorRandoFile(int slot);
void Randomizer_TestResetStartingGiveLog(void);
int Randomizer_TestCreationGiveHost(int i);
int Randomizer_TestHostEmpty(int rc);
int Randomizer_TestStartingGiveMatchesPlacement(int rc);
int OoT_ComboLogic_ExportPool(int source, uint16_t* outItems, uint16_t* outHosts, uint16_t* outFlags, int cap);
int MM_ComboLogic_TestShuffledItems(uint16_t* outItems, uint16_t* outChecks, int cap);
int MM_ComboLogic_TestShuffledItemsInShadow(uint16_t* outItems, int cap);
int MM_HintFoolish_TestHostStanding(uint16_t check); // #575 H5a (HintFoolishSingleExe.cpp)
void Randomizer_TestClearOoTSave(void);
const char* OoT_Creation_TestLastSequence(void);
uint32_t OoT_Creation_TestLastUnprovedHalves(void);
// #564 V2 (legs V2a-V2d).
void OoT_Creation_TestStageGeneration(void);
void OoT_Creation_ForgetConsumedGeneration(void); // #936
uint8_t Randomizer_IsSeedGenerated(void);
uint8_t Randomizer_IsSpoilerLoaded(void);
void OoT_ComboLogic_SetGeneralPassDeferred(int deferred);
const char* OoT_FileSelect_NoSeedText(uint8_t language);
// #564 V23 (legs V23a-V23b): the OoT spoiler's name is its hash icons (OTRGlobals.cpp).
uint8_t GetSeedIconIndex(uint8_t index);
const char* SohFileSelect_GetSettingText(uint8_t optionIndex, uint8_t language);
uint32_t SpoilerFileExists(const char* spoilerFileName);
void FileChoose_UpdateRandomizer(void);
extern bool fileSelectSpoilerFileLoaded;
}

// SoH's index of "No randomizer seed loaded" (RSM_NO_RANDOMIZER_GENERATED) and
// OoT's LANGUAGE_ENG, for the V2 legs' comparisons.
static const uint8_t kCsbNoSeedLine = 4;
static const uint8_t kCsbEnglish = 0;

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

/** Where OoT's writer puts the current generation's spoiler: "Randomizer/" plus the
 *  five hash icons, two digits each, joined by '-' (SpoilerLog_Write,
 *  3drando/spoiler_log.cpp), under the app directory. */
std::string CsbOoTSpoilerPath() {
    std::string name = "Randomizer/";
    for (uint8_t i = 0; i < 5; i++) {
        char part[8];
        snprintf(part, sizeof(part), i == 0 ? "%02u" : "-%02u", (unsigned)GetSeedIconIndex(i));
        name += part;
    }
    name += ".json";
    return Ship::Context::GetPathRelativeToAppDirectory(name);
}

/** The whole file, or "" when it cannot be opened. */
std::string CsbReadWholeFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return std::string();
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
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

/** One round over the current tables under `goal`, with `assumed` held. */
int CsbRoundNow(uint8_t goal, const std::vector<ComboLogicBagItem>& assumed, ComboLogicRoundResult* out) {
    ComboLogicRoundRequest req;
    memset(&req, 0, sizeof(req));
    req.assumed = assumed.empty() ? nullptr : assumed.data();
    req.assumedCount = (int)assumed.size();
    req.goal = goal;
    memset(out, 0, sizeof(*out));
    return Combo_Logic_RunRound(&req, out);
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

/** Both games' shop shuffles, which leg G (#800) generates under (restored after). */
const char* const kCsbShopCVars[][2] = {
    { "gRandoSettings.Shopsanity", "1" },         // RO_SHOPSANITY_SPECIFIC_COUNT
    { "gRandoSettings.ShopsanityCount", "4" },    // four emptied shelves per OoT shop
    { "gRando.Options.RO_SHUFFLE_SHOPS", "1" },   // MM's shop stock enters the bag
    { "gRandoSettings.ShuffleScrubs", "2" },      // RO_SCRUBS_ALL: every Business Scrub is a host (#800 pass 2)
    { "gRandoSettings.ShuffleMerchants", "3" },   // RO_SHUFFLE_MERCHANTS_ALL: all four merchants are hosts (#800 pass 2)
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

// ============================================================================
// RSBS_CSB_FOOLISH=N — epic #575 H4's measurement (see the file header)
// ============================================================================

struct CsbFoolishResult {
    bool ran = false;
    int baselineGoal = -1;
    int hosts = 0;
    int hostsBy[2][2] = {}; // [host is MM][item is MM]
    int required = 0;
    int requiredBy[2][2] = {};
    int roundErrors = 0;
    int roundsA = 0;
    uint32_t msA = 0;
    int candidates = 0;
    int roundsB = 0;
    uint32_t msB = 0;
    int passes = 0;
    int conditional = 0;
    int useless = 0;
    bool capped = false;
    uint32_t digestBefore = 0;
    uint32_t digestAfter = 0;
};

struct CsbFoolishCtx {
    uint8_t goal = 0;
    int roundCap = 4000;
    uint32_t rng = 0;
    int rounds = 0;
    std::vector<ComboLogicHostRef> hosts;
    std::vector<char> condReq; // OoTMM's conditionallyRequiredLocations, by host index
    CsbFoolishResult r;
};

/** One beat-the-frozen-GOAL round with `forbidden` (host-index flags) withheld.
 *  1 = the GOAL holds, 0 = it does not; an engine error counts as "does not" and
 *  is reported. */
int CsbFoolishGoal(CsbFoolishCtx* c, const std::vector<char>& forbidden) {
    std::vector<ComboLogicHostRef> refs;
    for (size_t i = 0; i < forbidden.size(); i++) {
        if (forbidden[i]) {
            refs.push_back(c->hosts[i]);
        }
    }
    ComboLogicRoundRequest req;
    memset(&req, 0, sizeof(req));
    req.goal = c->goal;
    req.forbidden = refs.empty() ? nullptr : refs.data();
    req.forbiddenCount = (int)refs.size();
    ComboLogicRoundResult res;
    memset(&res, 0, sizeof(res));
    c->rounds++;
    if (Combo_Logic_RunRound(&req, &res) != RSBS_COMBO_LOGIC_OK) {
        c->r.roundErrors++;
        return 0;
    }
    return res.goalExpression == 1 ? 1 : 0;
}

uint32_t CsbFoolishRngBelow(CsbFoolishCtx* c, uint32_t bound) {
    uint32_t x = c->rng != 0u ? c->rng : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    c->rng = x;
    return bound == 0u ? 0u : x % bound;
}

bool CsbFoolishCapped(CsbFoolishCtx* c) {
    if (c->rounds - c->r.roundsA >= c->roundCap) {
        c->r.capped = true;
    }
    return c->r.capped;
}

struct CsbZz {
    std::vector<char> allowed;
    std::vector<char> forbidden;
};

/** OoTMM monteCarloZigZagDown: forbid the allowed hosts one by one in a random
 *  order, keeping each forbidden unless the GOAL falls; a host whose removal
 *  breaks the GOAL is "sometimes required". Re-forbids the last NEW such host. */
bool CsbZigZagDown(CsbFoolishCtx* c, const CsbZz& zz, CsbZz* out) {
    std::vector<int> locs;
    for (size_t i = 0; i < zz.allowed.size(); i++) {
        if (zz.allowed[i]) {
            locs.push_back((int)i);
        }
    }
    CsbZz s = zz;
    int lastBanished = -1;
    while (!locs.empty()) {
        if (CsbFoolishCapped(c)) {
            return false;
        }
        const size_t k = CsbFoolishRngBelow(c, (uint32_t)locs.size());
        const int loc = locs[k];
        locs.erase(locs.begin() + (long)k);
        s.allowed[loc] = 0;
        s.forbidden[loc] = 1;
        if (!CsbFoolishGoal(c, s.forbidden)) {
            if (!c->condReq[loc]) {
                c->condReq[loc] = 1;
                lastBanished = loc;
            }
            s.allowed[loc] = 1;
            s.forbidden[loc] = 0;
        }
    }
    if (lastBanished < 0) {
        return false;
    }
    s.forbidden[lastBanished] = 1;
    s.allowed[lastBanished] = 0;
    *out = s;
    return true;
}

/** OoTMM monteCarloZigZagUp: re-allow the forbidden hosts one by one in a random
 *  order; a host whose return restores the GOAL is "sometimes required" and is
 *  forbidden again. Re-allows the last NEW such host. */
bool CsbZigZagUp(CsbFoolishCtx* c, const CsbZz& zz, CsbZz* out) {
    std::vector<int> locs;
    for (size_t i = 0; i < zz.forbidden.size(); i++) {
        if (zz.forbidden[i]) {
            locs.push_back((int)i);
        }
    }
    CsbZz s = zz;
    int lastAdded = -1;
    while (!locs.empty()) {
        if (CsbFoolishCapped(c)) {
            return false;
        }
        const size_t k = CsbFoolishRngBelow(c, (uint32_t)locs.size());
        const int loc = locs[k];
        locs.erase(locs.begin() + (long)k);
        s.forbidden[loc] = 0;
        s.allowed[loc] = 1;
        if (CsbFoolishGoal(c, s.forbidden)) {
            if (!c->condReq[loc]) {
                c->condReq[loc] = 1;
                lastAdded = loc;
            }
            s.allowed[loc] = 0;
            s.forbidden[loc] = 1;
        }
    }
    if (lastAdded < 0) {
        return false;
    }
    s.allowed[lastAdded] = 1;
    s.forbidden[lastAdded] = 0;
    *out = s;
    return true;
}

/** OoTMM monteCarloZigZag: alternate down and up generations until one yields no
 *  step. True when a down step found a new sometimes-required host. */
bool CsbZigZag(CsbFoolishCtx* c, const std::vector<char>& candidates) {
    CsbZz zz0;
    zz0.allowed = candidates;
    zz0.forbidden.assign(candidates.size(), 0);
    std::vector<CsbZz> stack(1, zz0);
    bool result = false;
    for (;;) {
        std::vector<CsbZz> down;
        down.swap(stack);
        for (const CsbZz& zz : down) {
            CsbZz step;
            while (CsbZigZagDown(c, zz, &step)) {
                result = true;
                stack.push_back(step);
            }
        }
        if (stack.empty() || c->r.capped) {
            break;
        }
        std::vector<CsbZz> up;
        up.swap(stack);
        for (const CsbZz& zz : up) {
            CsbZz step;
            while (CsbZigZagUp(c, zz, &step)) {
                stack.push_back(step);
            }
        }
        if (stack.empty() || c->r.capped) {
            break;
        }
    }
    return result;
}

/** THE PROBE: runs inside Combo_SingleBag_Run, right after the fill succeeded. */
void CsbFoolishProbe(void* ctxRaw) {
    CsbFoolishCtx* c = (CsbFoolishCtx*)ctxRaw;
    CsbFoolishResult& r = c->r;
    r.ran = true;
    c->goal = gComboCtx.comboSettings.goal;
    c->rng = Combo_SingleBag_FoolishSeed();
    c->rounds = 0;
    r.digestBefore = Combo_Logic_PlacementDigest();

    c->hosts.clear();
    const GameId games[2] = { GAME_OOT, GAME_MM };
    for (int g = 0; g < 2; g++) {
        ComboLogicPlacement p;
        for (int i = 0; i < Combo_Logic_PlacementCount(games[g]); i++) {
            if (Combo_Logic_PlacementAt(games[g], i, &p)) {
                ComboLogicHostRef h;
                memset(&h, 0, sizeof(h));
                h.hostGame = (uint8_t)games[g];
                h.hostCheck = p.hostCheck;
                c->hosts.push_back(h);
                r.hostsBy[g][p.item.originGame == (uint8_t)GAME_MM ? 1 : 0]++;
            }
        }
    }
    const int n = (int)c->hosts.size();
    r.hosts = n;
    std::vector<char> none((size_t)n, 0);
    r.baselineGoal = CsbFoolishGoal(c, none);

    // (a) the required set: one round per bag host, that host forbidden.
    std::vector<char> required((size_t)n, 0);
    const uint32_t tA = Combo_GenBudget_NowMs();
    for (int i = 0; i < n; i++) {
        std::vector<char> one((size_t)n, 0);
        one[(size_t)i] = 1;
        if (!CsbFoolishGoal(c, one)) {
            required[(size_t)i] = 1;
            r.required++;
            ComboLogicPlacement p;
            if (Combo_Logic_GetPlacement((GameId)c->hosts[(size_t)i].hostGame, c->hosts[(size_t)i].hostCheck, &p)) {
                r.requiredBy[c->hosts[(size_t)i].hostGame == (uint8_t)GAME_MM ? 1 : 0]
                            [p.item.originGame == (uint8_t)GAME_MM ? 1 : 0]++;
            }
        }
    }
    r.msA = Combo_GenBudget_NowMs() - tA;
    r.roundsA = c->rounds;

    // (b) OoTMM's zig-zag over the bag hosts (a) did not call required.
    std::vector<char> candidates((size_t)n, 0);
    for (int i = 0; i < n; i++) {
        candidates[(size_t)i] = required[(size_t)i] ? 0 : 1;
        r.candidates += candidates[(size_t)i];
    }
    c->condReq.assign((size_t)n, 0);
    const uint32_t tB = Combo_GenBudget_NowMs();
    int failures = 0;
    while (r.candidates > 0) {
        r.passes++;
        if (CsbZigZag(c, candidates)) {
            failures = 0;
        } else {
            failures++;
        }
        if (failures >= 3 || r.capped) {
            break;
        }
    }
    r.msB = Combo_GenBudget_NowMs() - tB;
    r.roundsB = c->rounds - r.roundsA;
    for (int i = 0; i < n; i++) {
        if (candidates[(size_t)i]) {
            r.conditional += c->condReq[(size_t)i] ? 1 : 0;
            r.useless += c->condReq[(size_t)i] ? 0 : 1;
        }
    }
    r.digestAfter = Combo_Logic_PlacementDigest();
}

/** RSBS_CSB_FOOLISH=N: the pinned seed and N sample seeds, measured. */
TestResult CsbFoolish(int n) {
    const char* profile = ClmProfileName(std::getenv("RSBS_COMBO_PROFILE"));
    const uint32_t budget = Combo_GenBudget_FillBudgetMs(0);
    int cap = 4000;
    if (const char* capEnv = std::getenv("RSBS_CSB_FOOLISH_ROUND_CAP")) {
        cap = atoi(capEnv) > 0 ? atoi(capEnv) : cap;
    }
    printf("[TEST] combo-single-bag FOOLISH: epic #575 H4's measurement, profile \"%s\", the pinned seed and %d "
           "sample seed(s), per-attempt budget %ums (host scale %u%%), zig-zag round cap %d\n",
           profile, n, budget, Combo_GenBudget_HostScalePercent(), cap);
    ClmProfileApply(profile);

    int measured = 0;
    int failed = 0;
    int over = 0;
    uint64_t sumA = 0, sumB = 0, sumHalf = 0, sumTotal = 0;
    uint32_t worstA = 0, worstB = 0, worstTotal = 0;
    int sumRoundsA = 0, sumRoundsB = 0, worstRoundsB = 0, cappedCount = 0, errorSeeds = 0;
    for (int i = -1; i < n; i++) {
        const std::string seed = i < 0 ? std::string("RSBSSINGLEBAG1") : "RSBSSAMPLE" + std::to_string(i);
        CsbFoolishCtx ctx;
        ctx.roundCap = cap;
        const uint32_t tGen = Combo_GenBudget_NowMs();
        if (Rando_HeadlessSeedTest(seed.c_str()) != 0) {
            printf("[FOOLISH] %s: OoT generation failed\n", seed.c_str());
            failed++;
            continue;
        }
        Combo_SingleBag_TestSetPostFillProbe(CsbFoolishProbe, &ctx);
        const uint32_t t0 = Combo_GenBudget_NowMs();
        const int rc = MM_Rando_HeadlessPairedHalf();
        const uint32_t halfMs = Combo_GenBudget_NowMs() - t0;
        Combo_SingleBag_TestSetPostFillProbe(nullptr, nullptr);
        const ComboSingleBagReport bag = *Combo_SingleBag_LastReport();
        const CsbFoolishResult& r = ctx.r;
        if (rc != 0 || !r.ran) {
            printf("[FOOLISH] %s: rc=%d status=%s probe=%d (no proven world to measure)\n", seed.c_str(), rc,
                   Combo_Logic_StatusName(bag.status), r.ran ? 1 : 0);
            failed++;
            Combo_SingleBag_Forget();
            Combo_Crossings_Clear();
            continue;
        }
        const uint32_t analysisMs = r.msA + r.msB;
        const uint32_t halfWithout = halfMs > analysisMs ? halfMs - analysisMs : 0u;
        const uint32_t tTail = Combo_GenBudget_NowMs();
        (void)OoT_Creation_FinishPairedHalf(0);
        const uint32_t tEnd = Combo_GenBudget_NowMs();
        const uint32_t total = tEnd - tGen;
        printf("[FOOLISH] %s: goal %u; bag hosts %d (OoT hosts: %d OoT + %d MM items; MM hosts: %d OoT + %d MM items); "
               "baseline GOAL=%d; digest %08X -> %08X%s\n",
               seed.c_str(), (unsigned)ctx.goal, r.hosts, r.hostsBy[0][0], r.hostsBy[0][1], r.hostsBy[1][0],
               r.hostsBy[1][1], r.baselineGoal, r.digestBefore, r.digestAfter,
               r.digestBefore == r.digestAfter ? "" : " (MOVED)");
        printf("[FOOLISH] %s: (a) required %d of %d (OoT hosts: %d OoT + %d MM items; MM hosts: %d OoT + %d MM items) "
               "in %d rounds, %ums (%.1f ms/round)\n",
               seed.c_str(), r.required, r.hosts, r.requiredBy[0][0], r.requiredBy[0][1], r.requiredBy[1][0],
               r.requiredBy[1][1], r.roundsA, r.msA, r.roundsA > 0 ? (double)r.msA / r.roundsA : 0.0);
        printf("[FOOLISH] %s: (b) zig-zag over %d candidates: %d pass(es), %d sometimes-required, %d useless, %d "
               "rounds, %ums (%.1f ms/round)%s; %d round error(s)\n",
               seed.c_str(), r.candidates, r.passes, r.conditional, r.useless, r.roundsB, r.msB,
               r.roundsB > 0 ? (double)r.msB / r.roundsB : 0.0, r.capped ? " CAPPED" : "", r.roundErrors);
        printf("[FOOLISH] %s: fill rounds %d, batches %d; MM half %ums = %ums creation + %ums analysis; end to end "
               "%ums (OoT Generate %ums, OoT tail %ums); creation + analysis = %.2fx the %ums budget\n",
               seed.c_str(), bag.fill.rounds, bag.fill.attempts, halfMs, halfWithout, analysisMs, total, t0 - tGen,
               tEnd - tTail, budget > 0 ? (double)(halfMs) / budget : 0.0, budget);
        measured++;
        over += (budget > 0 && halfMs > budget) ? 1 : 0;
        sumA += r.msA;
        sumB += r.msB;
        sumHalf += halfWithout;
        sumTotal += halfMs;
        worstA = r.msA > worstA ? r.msA : worstA;
        worstB = r.msB > worstB ? r.msB : worstB;
        worstTotal = halfMs > worstTotal ? halfMs : worstTotal;
        sumRoundsA += r.roundsA;
        sumRoundsB += r.roundsB;
        worstRoundsB = r.roundsB > worstRoundsB ? r.roundsB : worstRoundsB;
        cappedCount += r.capped ? 1 : 0;
        errorSeeds += r.roundErrors > 0 ? 1 : 0;
        Combo_SingleBag_Forget();
        Combo_Crossings_Clear();
    }
    ClmProfileRestore(profile);
    if (measured > 0) {
        printf("[FOOLISH] summary (%s): %d measured, %d failed; (a) mean %ums worst %ums, %.1f rounds; (b) mean %ums "
               "worst %ums, %.1f rounds (worst %d), %d capped; MM half without analysis mean %ums; with (a)+(b) mean "
               "%ums worst %ums; %d of %d over the %ums per-attempt budget; %d seed(s) with round errors\n",
               profile, measured, failed, (unsigned)(sumA / measured), worstA, (double)sumRoundsA / measured,
               (unsigned)(sumB / measured), worstB, (double)sumRoundsB / measured, worstRoundsB, cappedCount,
               (unsigned)(sumHalf / measured), (unsigned)(sumTotal / measured), worstTotal, over, measured, budget,
               errorSeeds);
    } else {
        printf("[FOOLISH] summary (%s): nothing measured, %d failed\n", profile, failed);
    }
    return TEST_PASS;
}

} // namespace

TestResult ComboSingleBag_Run(void) {
    if (const char* foolish = std::getenv("RSBS_CSB_FOOLISH")) {
        const int n = atoi(foolish);
        if (n >= 0 && foolish[0] != '\0') {
            return CsbFoolish(n);
        }
    }
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
    // A2. OoT's foreign-host rule, swept by category (PR #743 review; #800
    //     pass 1 admits the plain shop shelves). Swept here, ASSERTED at the
    //     end of the row, after leg G: a red predicate then shows both its
    //     sweep verdict and leg G's shop-crossing count in one run.
    // ------------------------------------------------------------------
    int hostRuleCounts[8];
    const int hostRuleViolations = OoT_ComboLogic_TestSweepForeignHostRule(hostRuleCounts);
    printf("[TEST] combo-single-bag: OoT host rule: %d shop shelves, %d Business Scrubs, %d merchants, %d chest-game "
           "reward (each must be accepted); chest-game room chests %d, shop-ish non-shelf non-merchant non-reward "
           "name %d, non-chest non-shelf non-scrub non-merchant %d rows (each must be rejected); %d accepted; %d "
           "violation(s)\n",
           hostRuleCounts[0], hostRuleCounts[1], hostRuleCounts[2], hostRuleCounts[7], hostRuleCounts[3],
           hostRuleCounts[4], hostRuleCounts[5], hostRuleCounts[6], hostRuleViolations);

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
    // F. #737: the GOAL runs through MM's fixed contents; both halves observed.
    // ------------------------------------------------------------------
    {
        const int fixedTotal = MM_ComboLogic_TestFixedContents(0, nullptr, nullptr, 0);
        const int remainsTotal = MM_ComboLogic_TestFixedContents(1, nullptr, nullptr, 0);
        std::vector<uint16_t> remainsItems((size_t)(remainsTotal > 0 ? remainsTotal : 0));
        std::vector<uint16_t> remainsChecks(remainsItems.size());
        if (remainsTotal > 0) {
            MM_ComboLogic_TestFixedContents(1, remainsItems.data(), remainsChecks.data(), remainsTotal);
        }
        std::vector<ComboLogicBagItem> remains;
        for (const uint16_t id : remainsItems) {
            ComboLogicBagItem row;
            memset(&row, 0, sizeof(row));
            row.item.originGame = (uint8_t)GAME_MM;
            row.item.id = id;
            remains.push_back(row);
        }
        // The remains plus every fixed RENEWABLE content (MM's own class), one row per
        // distinct item id: what a player regains in play from a pot or a shop.
        std::vector<ComboLogicBagItem> remainsAndRenewables = remains;
        int renewableIds = 0;
        {
            const int fixedAll = MM_ComboLogic_TestFixedContents(0, nullptr, nullptr, 0);
            std::vector<uint16_t> fixedItems((size_t)(fixedAll > 0 ? fixedAll : 0));
            if (fixedAll > 0) {
                MM_ComboLogic_TestFixedContents(0, fixedItems.data(), nullptr, fixedAll);
            }
            std::vector<uint16_t> seen;
            for (const uint16_t id : fixedItems) {
                ComboItemClassRow cls;
                if (MM_ComboLogic_ClassifyItem(id, &cls) != 1 || cls.fillClass != RSBS_FILL_CLASS_RENEWABLE) {
                    continue;
                }
                bool dup = false;
                for (const uint16_t other : seen) {
                    dup = dup || other == id;
                }
                if (dup) {
                    continue;
                }
                seen.push_back(id);
                ComboLogicBagItem row;
                memset(&row, 0, sizeof(row));
                row.item.originGame = (uint8_t)GAME_MM;
                row.item.id = id;
                remainsAndRenewables.push_back(row);
                renewableIds++;
            }
        }
        const std::vector<ComboLogicBagItem> none;
        const int grantsBefore = MM_ComboLogic_FixedHarvestCount();
        ComboLogicRoundResult on;
        const int onRc = CsbRoundNow(RSBS_COMBO_GOAL_BEAT_BOTH, none, &on);
        const int grantsOn = MM_ComboLogic_FixedHarvestCount() - grantsBefore;
        // The red half. Switched back on before any assertion can return.
        const int previous = MM_ComboLogic_TestSetFixedGrants(0);
        ComboLogicRoundResult off;
        const int offRcF = CsbRoundNow(RSBS_COMBO_GOAL_BEAT_BOTH, none, &off);
        const int grantsOff = MM_ComboLogic_FixedHarvestCount() - grantsBefore - grantsOn;
        ComboLogicRoundResult withRemains;
        const int remainsRc = CsbRoundNow(RSBS_COMBO_GOAL_BEAT_BOTH, remains, &withRemains);
        ComboLogicRoundResult withRenewables;
        const int renewablesRc = CsbRoundNow(RSBS_COMBO_GOAL_BEAT_BOTH, remainsAndRenewables, &withRenewables);
        MM_ComboLogic_TestSetFixedGrants(previous);
        printf("[TEST] combo-single-bag: F (#737): %d fixed MM contents outside the host pool, %d of them boss "
               "remains; beat-both exit round with the fixed grant: goalOoT=%d goalMM=%d GOAL=%d (%d fixed "
               "contents granted); without it: goalOoT=%d goalMM=%d GOAL=%d (%d granted); without it but the %d "
               "fixed remains assumed: goalOoT=%d goalMM=%d GOAL=%d\n",
               fixedTotal, remainsTotal, on.goalOoT, on.goalMM, on.goalExpression, grantsOn, off.goalOoT, off.goalMM,
               off.goalExpression, grantsOff, (int)remains.size(), withRemains.goalOoT, withRemains.goalMM,
               withRemains.goalExpression);
        printf("[TEST] combo-single-bag: F (#737): without it but the fixed remains and %d distinct fixed renewable "
               "ids assumed: goalOoT=%d goalMM=%d GOAL=%d\n",
               renewableIds, withRenewables.goalOoT, withRenewables.goalMM, withRenewables.goalExpression);
        CSB_ASSERT(previous == 1, "MM's fixed-content grant was already switched off before leg F");
        CSB_ASSERT(remainsTotal > 0, "the shipped profile leaves no boss remains fixed, so leg F proves nothing about "
                                     "the case #737 found");
        CSB_ASSERT(onRc == RSBS_COMBO_LOGIC_OK && on.goalMM == 1 && on.goalExpression == 1 && grantsOn > 0,
                   "with MM's fixed contents granted, the proven world's exit round does not prove beat-both");
        CSB_ASSERT(offRcF == RSBS_COMBO_LOGIC_OK && grantsOff == 0,
                   "the switched-off round still granted fixed contents");
        CSB_ASSERT(off.goalMM == 0 && off.goalExpression == 0,
                   "the GOAL is provable without MM's fixed contents, so this world does not run through them and "
                   "the grant is unlocked (#737's red half not observed)");
        CSB_ASSERT(remainsRc == RSBS_COMBO_LOGIC_OK,
                   "the round with the fixed boss remains assumed did not run");
        CSB_ASSERT(renewablesRc == RSBS_COMBO_LOGIC_OK && withRenewables.goalMM == 1,
                   "assuming the fixed boss remains and every fixed renewable does not restore MM's goal: a fixed "
                   "PROGRESSION content other than the remains is load-bearing for Majora");
        CSB_ASSERT(CsbGoalNow(goal) == 1, "leg F did not leave the world proving the GOAL");
    }

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
        CSB_ASSERT(hintRc == 0, "a paired world's hints leave a crossing host unhintable or name its cover, point at "
                                "no OoT location without naming Termina, or fail a #441 check (see [rando-hints])");

        // #575 H5a, MM's half: MM recomputes "a placement the coordinator made"
        // from the item-class owner and the crossing store at read time. Over
        // THIS fill, every coordinator row on an MM host must read IMPORTANT.
        const int mmPlaced = Combo_Logic_PlacementCount(GAME_MM);
        int mmUnimportant = 0;
        for (int i = 0; i < mmPlaced; i++) {
            ComboLogicPlacement p;
            if (!Combo_Logic_PlacementAt(GAME_MM, i, &p)) {
                continue;
            }
            if ((MM_HintFoolish_TestHostStanding(p.hostCheck) & COMBO_HOST_IMPORTANT) == 0) {
                if (mmUnimportant < 5) {
                    printf("[TEST] combo-single-bag: FAIL coordinator row on MM check %u (origin %u, id %u) reads "
                           "unimportant\n",
                           (unsigned)p.hostCheck, (unsigned)p.item.originGame, (unsigned)p.item.id);
                }
                mmUnimportant++;
            }
        }
        ComboFoolishAreas mmFoolish;
        const int mmFoolishAreas = Combo_Area_ComputeFoolish((uint8_t)GAME_MM, &mmFoolish);
        printf("[TEST] combo-single-bag: #575 H5a: %d coordinator rows on MM hosts, %d read unimportant; MM's stones "
               "would call %d MM area(s) foolish in this world\n",
               mmPlaced, mmUnimportant, mmFoolishAreas);
        CSB_ASSERT(mmPlaced > 0, "the fill placed nothing on an MM host, so the MM half of H5a proves nothing");
        CSB_ASSERT(mmUnimportant == 0, "a placement the coordinator made on an MM host reads unimportant to MM's "
                                       "foolish rule, so an MM stone could call its area foolish");
        CSB_ASSERT(mmFoolishAreas >= 0, "MM registered no foolish adapter");
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

    // ------------------------------------------------------------------
    // D2b. (#575 H3b) A solo world generated after the paired creation, with
    //      the paired world's crossings still in the store, names none of them.
    // ------------------------------------------------------------------
    {
        const int storedHosts = Combo_Crossings_Count(GAME_OOT);
        const int priorNative = OoT_ComboLogic_TestSetNativeGeneralPass(1);
        const int soloGen = Rando_HeadlessSeedTest("RSBSH3BSOLOWORLD");
        (void)OoT_ComboLogic_TestSetNativeGeneralPass(priorNative);
        const int soloRc = soloGen == 0 ? Rando_ValidateSoloWorldHintsAfterPairing() : -1;
        printf("[TEST] combo-single-bag: solo world after the paired creation (%d OoT-hosted crossings still stored): "
               "generation rc=%d, hint check rc=%d\n",
               storedHosts, soloGen, soloRc);
        CSB_ASSERT(storedHosts > 0, "the store holds no OoT-hosted crossing, so the solo-world check proves nothing");
        CSB_ASSERT(soloGen == 0, "the solo world after the paired creation did not generate");
        CSB_ASSERT(soloRc == 0, "a solo world generated after a paired creation names a crossed item, reads the "
                                "paired world's crossings in its hint pass, or fails a #441 check (see [rando-hints])");
    }
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
        // V23a's starting point: MM has no spoiler selected, and MM's "Generate
        // Spoiler File" is at its shipped default (on). MM_Rando_HeadlessPairedHalf,
        // which the legs above call, switches it off for its own digest; with it
        // off MM's generation writes no spoiler of any kind, and V23a would pass
        // whatever MM's paired generation does with one. V23b's starting point:
        // nothing an earlier run left sits at this pair's OoT spoiler path, so
        // file 1 writes it.
        const int savedGenerateSpoiler = CVarGetInteger("gRando.GenerateSpoiler", -1);
        struct GenerateSpoilerRestore {
            int saved;
            ~GenerateSpoilerRestore() {
                if (saved < 0) {
                    CVarClear("gRando.GenerateSpoiler");
                } else {
                    CVarSetInteger("gRando.GenerateSpoiler", saved);
                }
            }
        } restoreGenerateSpoiler{ savedGenerateSpoiler };
        CVarSetInteger("gRando.GenerateSpoiler", 1);
        CVarSetString("gRando.SpoilerFile", "");
        CVarSetInteger("gRando.SpoilerFileIndex", 0);
        const std::string v23Path = CsbOoTSpoilerPath();
        std::remove(v23Path.c_str());
        // Staged as the file select's Generate stages it (leg V2a reads it back).
        OoT_Creation_TestStageGeneration();
        CSB_ASSERT(Randomizer_IsSeedGenerated() != 0, "the staged generation does not read as staged");
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

        // V2a. #564 V2: file 1's creation consumed its staged generation, and the
        // file select says so where a second file would start.
        {
            const int generated = Randomizer_IsSeedGenerated();
            const int spoilerLoaded = Randomizer_IsSpoilerLoaded();
            const int waiting = OoT_ComboLogic_GeneralPassDeferred();
            const char* line = OoT_FileSelect_NoSeedText(kCsbEnglish);
            printf("[TEST] combo-single-bag: V2a after file 1's creation: generation staged=%d, spoiler loaded=%d, "
                   "general pass waiting=%d; the line under Start Randomizer reads \"%s\"\n",
                   generated, spoilerLoaded, waiting, line);
            CSB_ASSERT(generated == 0, "file 1's creation left its generation staged: the file select still offers "
                                       "Start Randomizer, and a second file from the same generation runs the "
                                       "creation work before anything refuses it (#564 V2)");
            CSB_ASSERT(spoilerLoaded == 0 && waiting == 0,
                       "file 1's creation left a spoiler loaded or a general pass waiting for another creation");
            CSB_ASSERT(std::strstr(line, "file 1") != nullptr,
                       "the line under Start Randomizer does not say the seed was used for file 1");
        }

        // V2b. The file select's per-frame update does not reload the spoiler
        // this creation wrote (FileChoose_Init leaves nothing auto-loaded yet).
        {
            std::string spoilerPath = CVarGetString("gGeneral.SpoilerLog", "");
            // SpoilerLog_Write puts the file under the app directory and names it
            // "./Randomizer/<name>" in the CVar. The two agree in a portable build,
            // where the app directory is the working directory. This harness points
            // SHIP_HOME at a sandbox (CreateHarnessStyleContext), which libultraship
            // honours on Linux and macOS only, so there the relative name misses the
            // file: point the CVar at where the writer put it, as a player's would.
            if (!SpoilerFileExists(spoilerPath.c_str()) && spoilerPath.rfind("./", 0) == 0) {
                const std::string written = Ship::Context::GetPathRelativeToAppDirectory(spoilerPath.substr(2));
                if (SpoilerFileExists(written.c_str())) {
                    spoilerPath = written;
                    CVarSetString("gGeneral.SpoilerLog", spoilerPath.c_str());
                }
            }
            const uint32_t exists = SpoilerFileExists(spoilerPath.c_str());
            fileSelectSpoilerFileLoaded = false;
            OoT_Notification_ClearForTest();
            FileChoose_UpdateRandomizer();
            ComboNotification toast;
            std::memset(&toast, 0, sizeof(toast));
            const int queued = OoT_Notification_PeekLastForTest(&toast);
            const bool parsed = fileSelectSpoilerFileLoaded;
            OoT_Notification_ClearForTest();
            const char* line = OoT_FileSelect_NoSeedText(kCsbEnglish);
            printf("[TEST] combo-single-bag: V2b the file select's update over the written spoiler '%s' (found %u): "
                   "auto-loaded=%d, toast queued=%d ('%s %s'), staged=%d, line \"%s\"\n",
                   spoilerPath.c_str(), (unsigned)exists, parsed ? 1 : 0, queued,
                   queued && toast.prefix != nullptr ? toast.prefix : "",
                   queued && toast.message != nullptr ? toast.message : "", Randomizer_IsSeedGenerated(), line);
            CSB_ASSERT(exists != 0, "the creation wrote no spoiler the file select can find, so V2b proves nothing");
            CSB_ASSERT(!parsed && queued == 0,
                       "the file select reloaded the spoiler the creation just wrote (and refused it with a toast) "
                       "because the creation consumed its generation");
            CSB_ASSERT(Randomizer_IsSeedGenerated() == 0 && Randomizer_IsSpoilerLoaded() == 0 &&
                           std::strstr(line, "file 1") != nullptr,
                       "the file select's update undid the consumption or lost its line");

            // #936. A LATER LAUNCH: the consumption record V2b leans on is
            // process-local, so a fresh process reaches the file select with nothing
            // staged, nothing loaded, no record, and gGeneral.SpoilerLog still naming
            // the paired spoiler. The auto-load must leave it alone (no parse, no
            // toast); a DROP of the same file is the player asking, and keeps its toast.
            {
                const std::string pairedPath = CVarGetString("gGeneral.SpoilerLog", "");
                OoT_Creation_ForgetConsumedGeneration(); // a new process starts with no record
                fileSelectSpoilerFileLoaded = false;     // FileChoose_Init
                CVarSetInteger("gGeneral.RandomizerNewFileDropped", 0);
                CVarSetString("gGeneral.RandomizerDroppedFile", "");
                // Context::ParseSpoiler's first act clears the general-pass mark, so a
                // mark that survives the update proves nothing parsed the spoiler.
                OoT_ComboLogic_SetGeneralPassDeferred(1);
                OoT_Notification_ClearForTest();
                FileChoose_UpdateRandomizer();
                ComboNotification freshToast;
                std::memset(&freshToast, 0, sizeof(freshToast));
                const int freshQueued = OoT_Notification_PeekLastForTest(&freshToast);
                const int freshParsed = OoT_ComboLogic_GeneralPassDeferred() == 0 ? 1 : 0;
                OoT_ComboLogic_SetGeneralPassDeferred(0);
                OoT_Notification_ClearForTest();
                printf("[TEST] combo-single-bag: #936 a fresh launch's file select over the paired spoiler '%s' (found "
                       "%u): parsed=%d, toast queued=%d ('%s %s'), spoiler loaded=%d, SpoilerLog \"%s\"\n",
                       pairedPath.c_str(), (unsigned)SpoilerFileExists(pairedPath.c_str()), freshParsed, freshQueued,
                       freshQueued && freshToast.prefix != nullptr ? freshToast.prefix : "",
                       freshQueued && freshToast.message != nullptr ? freshToast.message : "",
                       Randomizer_IsSpoilerLoaded(), CVarGetString("gGeneral.SpoilerLog", ""));
                CSB_ASSERT(SpoilerFileExists(pairedPath.c_str()) != 0,
                           "the paired spoiler is not on disk, so the fresh-launch leg proves nothing");
                CSB_ASSERT(freshQueued == 0,
                           "a fresh launch's file select auto-loaded the paired spoiler and refused it with "
                           "a toast the player did nothing to cause (#936)");
                CSB_ASSERT(freshParsed == 0,
                           "a fresh launch's file select parsed the paired spoiler on its own (#936)");
                CSB_ASSERT(Randomizer_IsSpoilerLoaded() == 0 && Randomizer_IsSeedGenerated() == 0,
                           "the fresh launch's file select loaded or staged a world from the paired spoiler");

                // The explicit load: the same file dropped on the window.
                CVarSetString("gGeneral.RandomizerDroppedFile", pairedPath.c_str());
                CVarSetInteger("gGeneral.RandomizerNewFileDropped", 1);
                fileSelectSpoilerFileLoaded = false;
                OoT_Notification_ClearForTest();
                FileChoose_UpdateRandomizer();
                std::memset(&freshToast, 0, sizeof(freshToast));
                const int dropQueued = OoT_Notification_PeekLastForTest(&freshToast);
                char expected[128];
                Combo_PairingRefusalToastMessage(RSBS_PAIRING_REFUSAL_OOT_SPOILER, nullptr, expected, sizeof(expected));
                const char* expectedPrefix = Combo_PairingRefusalToastPrefix(RSBS_PAIRING_REFUSAL_OOT_SPOILER);
                const bool refusalCopy = dropQueued == 1 && freshToast.prefix != nullptr &&
                                         freshToast.message != nullptr &&
                                         std::strcmp(freshToast.prefix, expectedPrefix) == 0 &&
                                         std::strcmp(freshToast.message, expected) == 0;
                printf("[TEST] combo-single-bag: #936 the same spoiler dropped: toast queued=%d ('%s %s'), refusal "
                       "copy=%d, spoiler loaded=%d\n",
                       dropQueued, dropQueued && freshToast.prefix != nullptr ? freshToast.prefix : "",
                       dropQueued && freshToast.message != nullptr ? freshToast.message : "", refusalCopy ? 1 : 0,
                       Randomizer_IsSpoilerLoaded());
                OoT_Notification_ClearForTest();
                CSB_ASSERT(refusalCopy, "a dropped paired spoiler was not refused with the paired-spoiler toast: the "
                                        "explicit load must still say why it did not load");
                CSB_ASSERT(Randomizer_IsSpoilerLoaded() == 0, "a dropped paired spoiler loaded as a solo OoT world");

                // Under DontGenerateSpoiler the player turned spoilers off, and SoH's
                // file select deletes the spoiler it finds. The silent skip must
                // still delete it (no parse, no toast), or the paired spoiler stays
                // readable for good. A copy is used so the real one survives for
                // the legs after this one, beside it: the app directory is not the
                // cwd everywhere (SHIP_HOME on Linux CI), and the CVar names the
                // spoiler by the path the creation gave it.
                const std::string noSpoilerPath =
                    pairedPath.substr(0, pairedPath.find_last_of("/\\") + 1) + "rsbs-936-dont-generate-spoiler.json";
                const std::string& noSpoilerCVar = noSpoilerPath;
                const std::string pairedDoc = CsbReadWholeFile(pairedPath);
                {
                    std::ofstream copy(noSpoilerPath, std::ios::binary | std::ios::trunc);
                    copy << pairedDoc;
                }
                const unsigned copyBefore = (unsigned)SpoilerFileExists(noSpoilerCVar.c_str());
                CVarSetString("gGeneral.SpoilerLog", noSpoilerCVar.c_str());
                CVarSetInteger("gRandoSettings.DontGenerateSpoiler", 1);
                OoT_Creation_ForgetConsumedGeneration();
                fileSelectSpoilerFileLoaded = false;
                CVarSetInteger("gGeneral.RandomizerNewFileDropped", 0);
                CVarSetString("gGeneral.RandomizerDroppedFile", "");
                OoT_ComboLogic_SetGeneralPassDeferred(1);
                OoT_Notification_ClearForTest();
                FileChoose_UpdateRandomizer();
                std::memset(&freshToast, 0, sizeof(freshToast));
                const int noSpoilerQueued = OoT_Notification_PeekLastForTest(&freshToast);
                const int noSpoilerParsed = OoT_ComboLogic_GeneralPassDeferred() == 0 ? 1 : 0;
                const unsigned copyAfter = (unsigned)SpoilerFileExists(noSpoilerCVar.c_str());
                OoT_ComboLogic_SetGeneralPassDeferred(0);
                OoT_Notification_ClearForTest();
                CVarClear("gRandoSettings.DontGenerateSpoiler");
                CVarSetString("gGeneral.SpoilerLog", pairedPath.c_str());
                std::remove(noSpoilerPath.c_str());
                printf("[TEST] combo-single-bag: #936 DontGenerateSpoiler, a fresh launch over a copy of the paired "
                       "spoiler (%zu bytes, found %u): parsed=%d, toast queued=%d, still on disk=%u\n",
                       pairedDoc.size(), copyBefore, noSpoilerParsed, noSpoilerQueued, copyAfter);
                CSB_ASSERT(!pairedDoc.empty() && copyBefore != 0,
                           "could not copy the paired spoiler, so the DontGenerateSpoiler leg proves nothing");
                CSB_ASSERT(noSpoilerQueued == 0 && noSpoilerParsed == 0,
                           "under DontGenerateSpoiler the file select parsed or toasted the paired spoiler (#936)");
                CSB_ASSERT(copyAfter == 0, "under DontGenerateSpoiler the file select left the paired spoiler on disk: "
                                           "the player turned spoilers off and it stays readable for good");
            }
        }

        // V23a. #564 V23: MM's paired generation minted no spoiler of its own and
        // selected none.
        {
            const std::string mmSelected = CVarGetString("gRando.SpoilerFile", "");
            const int mmIndex = CVarGetInteger("gRando.SpoilerFileIndex", 0);
            printf("[TEST] combo-single-bag: V23a after file 1's creation MM's spoiler selection reads "
                   "gRando.SpoilerFile='%s', gRando.SpoilerFileIndex=%d\n",
                   mmSelected.c_str(), mmIndex);
            fflush(stdout);
            CSB_ASSERT(mmSelected.empty() && mmIndex == 0,
                       "MM's paired generation wrote its own spoiler file and selected it in gRando.SpoilerFile: a "
                       "second artifact for one world, and a selection a later solo MM file loads from (#564 V23)");
        }

        // V23b. A second pair whose OoT spoiler path is the first pair's does not
        // rewrite the first pair's spoiler.
        {
            const std::string before = CsbReadWholeFile(v23Path);
            const uint32_t firstDigest = gComboCtx.mmProfileDigest;
            const uint32_t firstSeed = gComboCtx.sharedRandoSeed;
            CSB_ASSERT(before.find("\"combo\"") != std::string::npos,
                       "file 1's creation left no joined spoiler at its OoT spoiler path, so V23b proves nothing");

            const char* const kV23Option = "gRando.Options.RO_HINTS_SPIDER_HOUSES";
            const int savedOption = CVarGetInteger(kV23Option, -1);
            struct OptionRestore {
                const char* name;
                int saved;
                ~OptionRestore() {
                    if (saved < 0) {
                        CVarClear(name);
                    } else {
                        CVarSetInteger(name, saved);
                    }
                }
            } restoreOption{ kV23Option, savedOption };
            CVarSetInteger(kV23Option, savedOption == 1 ? 0 : 1);

            CSB_ASSERT(Rando_HeadlessSeedTest("RSBSSINGLEBAGGIVES") == 0,
                       "the second pair's generation (one MM option changed) failed");
            const std::string secondPath = CsbOoTSpoilerPath();
            const uint32_t secondDigest = gComboCtx.mmProfileDigest;
            printf("[TEST] combo-single-bag: V23b first pair masterSeed=0x%08X mmProfileDigest=0x%08X, second pair "
                   "masterSeed=0x%08X mmProfileDigest=0x%08X; OoT spoiler paths '%s' and '%s'\n",
                   (unsigned)firstSeed, (unsigned)firstDigest, (unsigned)gComboCtx.sharedRandoSeed,
                   (unsigned)secondDigest, v23Path.c_str(), secondPath.c_str());
            CSB_ASSERT(secondPath == v23Path && gComboCtx.sharedRandoSeed == firstSeed,
                       "the second pair's OoT spoiler is not named like the first pair's, so V23b proves nothing");
            CSB_ASSERT(secondDigest != firstDigest,
                       "the changed MM option did not change the pair's identity, so V23b proves nothing");

            // V23c's starting point: SpoilerLog names a solo OoT document this
            // creation will not write (the creation event joins into whatever the
            // CVar names).
            const char* const kV23cName = "Randomizer/rsbs-v23c-solo-oot-spoiler.json";
            const std::string v23cPath = Ship::Context::GetPathRelativeToAppDirectory(kV23cName);
            const std::string v23cCVar = std::string("./") + kV23cName;
            {
                std::ofstream solo(v23cPath, std::ios::binary | std::ios::trunc);
                solo << "{\n    \"_note\": \"a solo OoT spoiler with no combo section (ComboSingleBag V23c)\",\n"
                        "    \"seed\": \"V23C\"\n}\n";
            }
            const std::string soloBefore = CsbReadWholeFile(v23cPath);
            CSB_ASSERT(!soloBefore.empty(), "could not write V23c's solo OoT document, so V23c proves nothing");
            CVarSetString("gGeneral.SpoilerLog", v23cCVar.c_str());

            Randomizer_TestClearOoTSave();
            Context_ClearFrozenState(GAME_MM);
            OoT_Creation_TestStageGeneration();
            const int created = OoT_Creation_AuthorRandoFile(1);
            const std::string after = CsbReadWholeFile(v23Path);
            const std::string soloAfter = CsbReadWholeFile(v23cPath);
            const std::string spoilerLogAfter = CVarGetString("gGeneral.SpoilerLog", "");
            std::remove(v23cPath.c_str());
            printf("[TEST] combo-single-bag: V23c the solo OoT document SpoilerLog named is %s (%zu bytes before, %zu "
                   "after); SpoilerLog now reads '%s'\n",
                   soloAfter == soloBefore ? "unchanged" : "REWRITTEN", soloBefore.size(), soloAfter.size(),
                   spoilerLogAfter.c_str());
            fflush(stdout);
            printf("[TEST] combo-single-bag: V23b the second pair's creation: created=%d; the first pair's spoiler is "
                   "%s (%zu bytes before, %zu after)\n",
                   created, after == before ? "unchanged" : "REWRITTEN", before.size(), after.size());
            CSB_ASSERT(created == 1, "the second pair's creation failed: a spoiler is a report, and refusing to "
                                     "write one must not fail a creation");
            CSB_ASSERT(after == before,
                       "the second pair's creation rewrote the first pair's spoiler: two pairs that share an OoT seed "
                       "and OoT settings share one spoiler path, and the later creation replaced the earlier pair's "
                       "artifact (#564 V23)");
            CSB_ASSERT(soloAfter == soloBefore,
                       "the second pair's creation joined its half into the solo OoT document SpoilerLog named, a file "
                       "this creation did not write: OoT's writer refused the aliased path and left SpoilerLog as it "
                       "was (#564 V23)");
            CSB_ASSERT(spoilerLogAfter != v23cCVar,
                       "SpoilerLog still names a document this creation did not write, so the Randomizer menu shows "
                       "it as this world's spoiler (#564 V23)");

            // V23d. The join's own identity check, for any caller handed another
            // pair's document regardless (the creation event no longer is): the
            // second pair, still resident, joining into the first pair's spoiler
            // is refused and leaves it as it is.
            const int joinRc = MM_Rando_AugmentSpoilerWithPairedHalf(v23Path.c_str());
            const std::string afterJoin = CsbReadWholeFile(v23Path);
            printf("[TEST] combo-single-bag: V23d the second pair's join into the first pair's spoiler returned %d; "
                   "the first pair's spoiler is %s (%zu bytes before, %zu after)\n",
                   joinRc, afterJoin == before ? "unchanged" : "REWRITTEN", before.size(), afterJoin.size());
            fflush(stdout);
            CSB_ASSERT(joinRc == -5 && afterJoin == before,
                       "the join rewrote another pair's spoiler: a document whose combo.identity names another pair "
                       "is that pair's (#564 V23)");
        }
    }
    Combo_SingleBag_Forget();
    Combo_Crossings_Clear();

    // ------------------------------------------------------------------
    // G. #800 pass 1: an MM item on an OoT shop shelf, from a real paired
    //    creation with both games' shop shuffles on (a fixed seed).
    // ------------------------------------------------------------------
    {
        std::vector<std::string> savedCVars;
        for (const auto& cv : kCsbShopCVars) {
            savedCVars.push_back(std::to_string(CVarGetInteger(cv[0], -1)));
            CVarSetInteger(cv[0], atoi(cv[1]));
        }
        struct CVarRestore {
            const std::vector<std::string>* saved;
            ~CVarRestore() {
                for (size_t i = 0; i < saved->size(); i++) {
                    const int v = atoi((*saved)[i].c_str());
                    if (v < 0) {
                        CVarClear(kCsbShopCVars[i][0]);
                    } else {
                        CVarSetInteger(kCsbShopCVars[i][0], v);
                    }
                }
            }
        } restoreCVars{ &savedCVars };

        CSB_ASSERT(Rando_HeadlessSeedTest("RSBSSINGLEBAGSHOPS") == 0,
                   "the paired generation with both shop shuffles on failed");
        // V2c. A new generation forgets the one file 1 consumed.
        {
            const char* line = OoT_FileSelect_NoSeedText(kCsbEnglish);
            printf("[TEST] combo-single-bag: V2c after a new generation the line under Start Randomizer reads \"%s\"\n",
                   line);
            CSB_ASSERT(std::strcmp(line, SohFileSelect_GetSettingText(kCsbNoSeedLine, kCsbEnglish)) == 0,
                       "a new generation still reports the previous one as used for file 1");
        }
        CSB_ASSERT(MM_Rando_HeadlessPairedHalf() == 0, "MM's creation-time half failed with both shop shuffles on");
        const ComboSingleBagReport shopBag = *Combo_SingleBag_LastReport();
        CSB_ASSERT(shopBag.status == RSBS_COMBO_LOGIC_OK && shopBag.fill.goalProven,
                   "the single-bag fill with both shop shuffles on did not prove the GOAL");
        const CsbTables shopTables = CsbCopyTables();
        int shelfCrossings = 0;
        int scrubCrossings = 0;
        int merchantCrossings = 0;
        const ComboLogicPlacement* firstShelf = nullptr;
        const ComboLogicPlacement* firstScrub = nullptr;
        const ComboLogicPlacement* firstMerchant = nullptr;
        for (const ComboLogicPlacement& p : shopTables.oot) {
            if (p.item.originGame == (uint8_t)GAME_MM && OoT_ComboLogic_TestIsShopShelf(p.hostCheck) == 1) {
                shelfCrossings++;
                firstShelf = firstShelf == nullptr ? &p : firstShelf;
            }
            if (p.item.originGame == (uint8_t)GAME_MM && OoT_ComboLogic_TestIsScrub(p.hostCheck) == 1) {
                scrubCrossings++;
                firstScrub = firstScrub == nullptr ? &p : firstScrub;
            }
            if (p.item.originGame == (uint8_t)GAME_MM && OoT_ComboLogic_TestIsMerchant(p.hostCheck) == 1) {
                merchantCrossings++;
                firstMerchant = firstMerchant == nullptr ? &p : firstMerchant;
            }
        }
        printf("[TEST] combo-single-bag: G (#800): both shop shuffles, scrub shuffle and merchant shuffle on: %d "
               "crossings into MM, %d into OoT, %d of them on OoT shop shelves, %d on OoT Business Scrubs, %d on OoT "
               "merchants\n",
               shopBag.crossingsIntoMM, shopBag.crossingsIntoOoT, shelfCrossings, scrubCrossings, merchantCrossings);
        CSB_ASSERT(shelfCrossings > 0, "no MM item landed on an OoT shop shelf with shopsanity on (#800 pass 1)");
        CSB_ASSERT(scrubCrossings > 0,
                   "no MM item landed on an OoT Business Scrub with scrub shuffle on (#800 pass 2)");
        CSB_ASSERT(merchantCrossings > 0,
                   "no MM item landed on an OoT merchant with merchant shuffle on (#800 pass 2)");

        // The store the drain reads, then the drain's own recording core, on each
        // REAL host check: the placement is found by the check the host's
        // RandomizerInf flag queues (the shelf's purchase, the scrub's or the
        // merchant's sale), and the purchase records one MM crossing.
        CSB_ASSERT(OoT_Creation_FinishPairedHalf(0) >= 0, "OoT's remainder failed with both shop shuffles on");
        const ComboLogicPlacement* const hosts[3] = { firstShelf, firstScrub, firstMerchant };
        const char* const kinds[3] = { "shelf", "scrub", "merchant" };
        for (int h = 0; h < 3; h++) {
            const uint16_t host = hosts[h]->hostCheck;
            const SharedItem hostItem = hosts[h]->item;
            const char* itemName = Combo_DescribeItemName(hostItem);
            const char* hostName = Combo_DescribeCheckName((uint8_t)GAME_OOT, host);
            printf("[TEST] combo-single-bag: G (#800): %s: MM %s (id %u) on OoT %s (check %u)\n", kinds[h],
                   itemName != nullptr ? itemName : "(unnamed)", (unsigned)hostItem.id,
                   hostName != nullptr ? hostName : "(unnamed)", (unsigned)host);
            const SharedItem* stored = Combo_GetForeignPlacementForOoTCheck(host);
            CSB_ASSERT(stored != nullptr && stored->originGame == hostItem.originGame && stored->id == hostItem.id,
                       "the crossing store does not answer the host's check with its MM item");
            const int before = Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true);
            CSB_ASSERT(OoT_Rando_Foreign_HostsForeign(host) == 1,
                       "the host's draw and textbox do not see its MM item");
            CSB_ASSERT(OoT_Rando_Foreign_HostCollected(host) == 0,
                       "the host reads sold out before anything was bought");
            CSB_ASSERT(OoT_Rando_Foreign_RecordPickup(host) == 1, "buying from the host recorded no MM crossing");
            CSB_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true) == before + 1,
                       "buying from the host did not record exactly one MM crossing");
            // The drain then marks the check collected; from then on the host is
            // sold out even when its RandomizerInf flag was lost with an unsaved
            // reload.
            CSB_ASSERT(OoT_Rando_Foreign_TestSetObtained(host, 1) == 1, "the host's check could not be collected");
            CSB_ASSERT(OoT_Rando_Foreign_HostCollected(host) == 1,
                       "a host whose check is collected does not read sold out, so it would sell it again");
            CSB_ASSERT(OoT_Rando_Foreign_RecordPickup(host) == 0, "a collected host recorded a second MM crossing");
            OoT_Rando_Foreign_TestSetObtained(host, 0);
        }

        // The sold-out rule (the shelf's and the scrub's): a merchant whose MM
        // item's check is collected stops selling its randomized item even with
        // its RandomizerInf flag unset. Its own gate, the one its offer and its
        // sale consult, falls back to the merchant's vanilla behavior instead of
        // charging again for a crossing the drain will not record twice. All four
        // merchants, not only the ones this seed gave an MM item: the crossing
        // store is re-hydrated with this world's crossings plus the leg's MM item
        // on every merchant that does not already host one. Each must sell before
        // its check is collected (the bridge meets Granny's own trade-quest
        // precondition for the call), or the "after" proves nothing.
        std::vector<ComboCrossing> ootRows;
        std::vector<ComboCrossing> mmRows;
        ComboCrossing merchantRow{};
        for (int i = 0; i < Combo_Crossings_Count(GAME_OOT); i++) {
            ComboCrossing row;
            CSB_ASSERT(Combo_Crossings_At(GAME_OOT, i, &row), "an OoT-hosted crossing could not be read back");
            ootRows.push_back(row);
            if (row.hostCheck == firstMerchant->hostCheck) {
                merchantRow = row;
            }
        }
        for (int i = 0; i < Combo_Crossings_Count(GAME_MM); i++) {
            ComboCrossing row;
            CSB_ASSERT(Combo_Crossings_At(GAME_MM, i, &row), "an MM-hosted crossing could not be read back");
            mmRows.push_back(row);
        }
        CSB_ASSERT(merchantRow.hostCheck == firstMerchant->hostCheck,
                   "the crossing store does not hold the merchant's crossing");
        uint16_t merchantChecks[8];
        const int merchantCount = OoT_ComboLogic_TestMerchantChecks(merchantChecks, 8);
        CSB_ASSERT(merchantCount == 4, "OoT's static table does not hold the four traced merchants");
        for (int m = 0; m < merchantCount; m++) {
            if (Combo_Crossings_Lookup(GAME_OOT, merchantChecks[m]) == nullptr) {
                ComboCrossing extra = merchantRow;
                extra.hostCheck = merchantChecks[m];
                ootRows.push_back(extra);
            }
        }
        // The treasure chest game's reward (#800 pass 2), named by the check the
        // room's final chest resolves to, gets an MM item of this world whose
        // model OoT draws from MM's descriptor, whatever this seed put there.
        const int chestGameReward = OoT_Rando_Foreign_TestChestGamePrizeCheck(1);
        CSB_ASSERT(chestGameReward > 0, "the treasure chest game's final chest resolves to no OoT check");
        int rewardCrossings = 0;
        for (const ComboLogicPlacement& p : shopTables.oot) {
            rewardCrossings += (p.item.originGame == (uint8_t)GAME_MM && p.hostCheck == chestGameReward) ? 1 : 0;
        }
        ComboCrossing rewardRow{};
        ComboModelAnswer rewardModel{};
        for (const ComboCrossing& row : ootRows) {
            if (row.hostCheck != (uint16_t)chestGameReward &&
                Combo_GetForeignItemModel((uint8_t)GAME_OOT, row.item, &rewardModel) ==
                    (uint8_t)COMBO_MODEL_ANSWER_DESCRIPTOR) {
                rewardRow = row;
                break;
            }
        }
        CSB_ASSERT(rewardRow.hostCheck != 0, "no MM item of this world is drawn in OoT from MM's descriptor");
        rewardRow.hostCheck = (uint16_t)chestGameReward;
        ootRows.erase(std::remove_if(ootRows.begin(), ootRows.end(),
                                     [&](const ComboCrossing& row) { return row.hostCheck == rewardRow.hostCheck; }),
                      ootRows.end());
        ootRows.push_back(rewardRow);
        Combo_Crossings_Clear();
        CSB_ASSERT(Combo_Crossings_Replace(ootRows.data(), (int)ootRows.size(), mmRows.data(), (int)mmRows.size()) >= 0,
                   "the crossing store refused this world's crossings plus an MM item on every merchant and on the "
                   "chest game's reward");
        int merchantsNotSellingBefore = 0;
        int merchantsNotSoldOutAfter = 0;
        for (int m = 0; m < merchantCount; m++) {
            const uint16_t host = merchantChecks[m];
            const int before = OoT_Rando_Foreign_TestMerchantSells(host);
            CSB_ASSERT(OoT_Rando_Foreign_TestSetObtained(host, 1) == 1, "the merchant's check could not be collected");
            const int after = OoT_Rando_Foreign_TestMerchantSells(host);
            OoT_Rando_Foreign_TestSetObtained(host, 0);
            const char* hostName = Combo_DescribeCheckName((uint8_t)GAME_OOT, host);
            printf("[TEST] combo-single-bag: G (#800): merchant %s (check %u) hosting an MM item sells its randomized "
                   "item: %d before its check is collected, %d after\n",
                   hostName != nullptr ? hostName : "(unnamed)", (unsigned)host, before, after);
            merchantsNotSellingBefore += before != 1 ? 1 : 0;
            merchantsNotSoldOutAfter += after != 0 ? 1 : 0;
        }
        CSB_ASSERT(merchantsNotSellingBefore == 0, "a merchant hosting an MM item does not sell it before its check is "
                                                   "collected, so the sold-out check proves nothing");
        CSB_ASSERT(merchantsNotSoldOutAfter == 0, "a merchant whose MM item's check is collected still sells it (or, "
                                                  "the bean salesman, refuses it as 'not enough rupees'), so it would "
                                                  "charge again for nothing");

        // The treasure chest game's reward (#800 pass 2). The game's own
        // resolvers name it for every path of the prize: the ItemGetInf flag the
        // final chest's opening sets (the flag the drain is queued by), the final
        // chest, and the Lens of Truth display above it; once the prize is won
        // the room spawns its final chest and display with other params, which
        // name no check, so the room's per-play reset of its chests never gives
        // the reward twice. The flag-set hook queues the reward for the drain,
        // whose recording core records one MM crossing and nothing once the
        // check is collected. The display's REAL draw shows the MM model.
        {
            const char* rewardName = Combo_DescribeCheckName((uint8_t)GAME_OOT, (uint16_t)chestGameReward);
            const char* rewardItem = Combo_DescribeItemName(rewardRow.item);
            const int prizeFlag = OoT_Rando_Foreign_TestChestGamePrizeCheck(0);
            const int prizeChestWon = OoT_Rando_Foreign_TestChestGamePrizeCheck(2);
            const int prizeDisplay = OoT_Rando_Foreign_TestChestGamePrizeCheck(3);
            const int prizeDisplayWon = OoT_Rando_Foreign_TestChestGamePrizeCheck(4);
            printf("[TEST] combo-single-bag: G (#800): chest game: %d MM item(s) placed on its reward by this seed; "
                   "MM %s (id %u) on OoT %s (check %d); the reward's flag names check %d, the final chest %d before "
                   "the prize is won and %d after, its Lens display %d before and %d after\n",
                   rewardCrossings, rewardItem != nullptr ? rewardItem : "(unnamed)", (unsigned)rewardRow.item.id,
                   rewardName != nullptr ? rewardName : "(unnamed)", chestGameReward, prizeFlag, chestGameReward,
                   prizeChestWon, prizeDisplay, prizeDisplayWon);
            const int drawVerdict = OoT_ForeignModel_TestChestGamePrizeDraw(&rewardModel.model);
            CSB_ASSERT(prizeFlag == chestGameReward && prizeDisplay == chestGameReward,
                       "the chest game's reward flag or its Lens display names another check than its final chest");
            CSB_ASSERT(prizeChestWon == 0 && prizeDisplayWon == 0,
                       "once the chest game's prize is won its final chest or Lens display still names a check, so the "
                       "room's per-play reset could give the reward again");
            CSB_ASSERT(OoT_Rando_Foreign_HostsForeign((uint16_t)chestGameReward) == 1,
                       "the chest game's reward does not see its MM item");
            CSB_ASSERT(OoT_Rando_Foreign_TestChestGameRewardQueued() == chestGameReward,
                       "opening the chest game's final chest does not queue the reward's check for the drain");
            const int before = Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true);
            CSB_ASSERT(OoT_Rando_Foreign_RecordPickup((uint16_t)chestGameReward) == 1,
                       "opening the chest game's final chest recorded no MM crossing");
            CSB_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/true) == before + 1,
                       "opening the chest game's final chest did not record exactly one MM crossing");
            CSB_ASSERT(OoT_Rando_Foreign_TestSetObtained((uint16_t)chestGameReward, 1) == 1,
                       "the chest game's reward could not be collected");
            const int queuedAfter = OoT_Rando_Foreign_TestChestGameRewardQueued();
            const int recordedAfter = OoT_Rando_Foreign_RecordPickup((uint16_t)chestGameReward);
            OoT_Rando_Foreign_TestSetObtained((uint16_t)chestGameReward, 0);
            CSB_ASSERT(queuedAfter == 0 && recordedAfter == 0,
                       "a collected chest-game reward queued or recorded a second MM crossing");
            CSB_ASSERT(drawVerdict == 0, "the chest game's Lens display does not draw the MM item its reward holds "
                                         "(see the chest-game prize lines above)");
        }
    }
    Combo_SingleBag_Forget();
    Combo_Crossings_Clear();

    // A2's verdict (swept above, before the worlds of legs B-G).
    CSB_ASSERT(hostRuleViolations == 0,
               "OoT's foreign-host predicate rejects a plain shop shelf, a Business Scrub, a merchant or the chest "
               "game's reward, or accepts a chest-game room chest or other non-chest location");
    CSB_ASSERT(hostRuleCounts[0] > 0 && hostRuleCounts[1] > 0 && hostRuleCounts[2] > 0 && hostRuleCounts[3] > 0 &&
                   hostRuleCounts[4] > 0 && hostRuleCounts[5] > 0 && hostRuleCounts[7] > 0,
               "a category of the host-rule sweep is empty, so it proves nothing about that category");
    CSB_ASSERT(hostRuleCounts[6] > hostRuleCounts[0] + hostRuleCounts[1] + hostRuleCounts[2] + hostRuleCounts[7],
               "OoT's predicate accepts no treasure chest at all");

    // V2d. A FAILED creation consumes its staged generation too. The world's
    // identity is gone (as after a failed MM half, whose retraction clears it),
    // so the event refuses; a world still waits at its general pass.
    {
        OoT_Creation_TestStageGeneration();
        OoT_ComboLogic_SetGeneralPassDeferred(1);
        const bool priorSourceIsRando = gComboCtx.sourceIsRando;
        gComboCtx.sourceIsRando = false;
        const int created = OoT_Creation_AuthorRandoFile(1);
        gComboCtx.sourceIsRando = priorSourceIsRando;
        RsbsSave_ResetSlotSessionState();
        OoT_Notification_ClearForTest();
        const int generated = Randomizer_IsSeedGenerated();
        const int waiting = OoT_ComboLogic_GeneralPassDeferred();
        const char* line = OoT_FileSelect_NoSeedText(kCsbEnglish);
        printf("[TEST] combo-single-bag: V2d a failed creation of file 2: created=%d, generation staged=%d, general "
               "pass waiting=%d, line \"%s\"\n",
               created, generated, waiting, line);
        OoT_ComboLogic_SetGeneralPassDeferred(0);
        CSB_ASSERT(created == 0, "the creation with no pairing identity was not refused, so V2d proves nothing");
        CSB_ASSERT(generated == 0 && waiting == 0,
                   "a failed creation left its generation staged or its world waiting at a general pass, so the file "
                   "select offers Start Randomizer for a world whose identity was retracted (#564 V2)");
        CSB_ASSERT(std::strcmp(line, SohFileSelect_GetSettingText(kCsbNoSeedLine, kCsbEnglish)) == 0,
                   "a failed creation's line names a file as if the seed had created one");
    }

    printf("[TEST] PASS: combo-single-bag\n");
    return TEST_PASS;
}
