/**
 * @file test_combo_logic_measure.c
 * @brief Increment 3's first two MEASUREMENTS: what one linked round costs, and
 *        whether a single-bag assumed fill over MM's forward-authored graph
 *        converges — both over the REAL registered engines (#645, lane K3).
 *
 * ============================================================================
 * THIS ROW IS A MEASUREMENT HARNESS, NOT A LOCK ON A NUMBER
 * ============================================================================
 *
 * Epic #645 carries two unchecked work items, lifted verbatim from the
 * solver-inventory audit's §6.3 "what this audit could not determine statically":
 *
 *   - "The cost of one linked round (OoT search + MM snapshot/give/crawl) against
 *      the #582 ~30 s creation-budget floor — unmeasured; increment 3's first
 *      work item."
 *   - "Whether an assumed fill over MM's forward-authored graph converges on the
 *      shipped profile without retries — empirical; increment 3's second work
 *      item."
 *
 * Both are questions about `Combo_Logic_RunRound` and `Combo_Logic_RunFill`, and
 * neither can be answered by anything except running them over both real engines.
 * So this row runs them and PRINTS the answers.
 *
 * IT ASSERTS NO TIMING. Not one. PR #581 §2a's rule is that a wall clock must
 * never decide anything in this tree — an abort is a function of how fast the
 * machine is, not of the seed — and a row that failed when a shared build host got
 * busy would be deleted within a week and its measurement lost with it. What the
 * row DOES assert is sanity, and each assertion is a property a broken harness
 * would violate rather than a property of this host:
 *
 *   S1. Both real engines are registered (the elision class of #512/#516).
 *   S2. Every round and every fill TERMINATES and returns a named status. The
 *       watchdog bound is never reached.
 *   S3. The same coordinator seed over the same bag reproduces the same placement
 *       digest, and a different seed does not (the sensitivity control, without
 *       which "reproducible" is satisfied by a constant).
 *   S4. THE MEASUREMENT RETURNS THE LIVE SAVE TO WHERE THE PROFILE APPLY LEFT IT:
 *       the whole unified save buffer memcmp-equal against a baseline taken AFTER
 *       `MM_ComboLogic_ApplyShippedProfile` and compared BEFORE the teardown's
 *       restoring memcpy, plus OoT's world placement digest equal, MM holding zero
 *       coordinator placements, MM's snapshot not live, and MM's reachability never
 *       observed shrinking.
 *
 *       THE ORDER IS THE ASSERTION, and getting it wrong is how this row shipped a
 *       vacuous lock for four commits. The first version compared against the OUTER
 *       baseline — the state the row must leave behind — AFTER the memcpy that
 *       restores it, which compares a buffer with the buffer it was just copied
 *       from. It could not fail, and it was the most advertised assertion in the PR.
 *       The inner baseline is the one with teeth: the rounds' Snapshot/Restore
 *       brackets, MM's placement re-applies and OoT's beginQuery/endQuery pairs have
 *       to put the save back for real, and nothing copies it back for them. The
 *       outer restore still happens, after the comparison, and it is now documented
 *       as construction rather than asserted as a property — because that is what it
 *       is. A premise check beside the inner baseline asserts the profile apply
 *       CHANGED the buffer, without which the two baselines are equal and S4 is
 *       vacuous again by a different route.
 *   S5. The measurement is NON-VACUOUS: the bag is non-empty, both sides offer
 *       candidate hosts, and the fill under the proved rung actually ran rounds.
 *
 * ============================================================================
 * WHAT IS MEASURED, AND THE FOUR PLACES THE ANSWER IS AN APPROXIMATION
 * ============================================================================
 *
 * (M1) THE UNION BAG'S REAL SIZE, against the coordinator's own caps. Audit §4.6
 *      says the union bag is "the last general pass" — OoT's
 *      `remainingAdvancementItems` (`fill.cpp:1405-1407`) plus MM's whole shuffled
 *      pool. Both halves are measured over the real world:
 *
 *      OoT's half is read back from the GENERATED WORLD: every owned host whose
 *      placed item `IsAdvancement()`. That is a SUPERSET of the general pass,
 *      because the six restricted passes that run BEFORE it (dungeon rewards,
 *      own-dungeon items, restricted songs, Link's pocket) also place advancement
 *      items and their hosts are indistinguishable afterwards. Conservative for a
 *      cost measurement, and stated rather than papered over.
 *
 *      MM's half is `MM_ComboLogic_PoolVanillaItems`, which is `GeneratePools`'
 *      own walk without its RNG; that accessor's comment names its own three
 *      deviations.
 *
 * (M2) ONE LINKED ROUND, timed at the MOST EXPENSIVE assumed-set size the fill
 *      ever uses (the whole bag; the fill's first round assumes bag-1). min /
 *      median / max over a sample, plus the alternations each round needed.
 *
 *      THAT NUMBER IS NOT THE BUDGET'S NUMBER, and the row says so where it
 *      prints both. An isolated round runs with EMPTY placement tables, so it
 *      pays neither the crossing exchange nor the post-restore placement
 *      re-apply — and the re-apply is one `place` per placement per round for a
 *      restoring side, which MM is. The fill's own wall/rounds is therefore also
 *      reported, and it is roughly double.
 *
 * (M3) THE FILL, under the proved no-tricks rung and under `none`, for three
 *      coordinator seeds and both GOALs that have an evaluator. Reported: wall
 *      time, rounds, rounds per placed item, attempts (hence batch roll-backs),
 *      dead-ends, and whether the GOAL became provable.
 *
 * (M4) THE BUDGET ARITHMETIC: the full union bag's cost for ONE BATCH from M2's
 *      in-fill figure, against #582's floor and this host's calibrated per-attempt
 *      budget — and then times `RSBS_COMBO_LOGIC_FILL_RETRIES` against that SAME
 *      per-attempt budget, because all of those roll-backs happen inside one
 *      `Combo_Logic_RunFill` and therefore inside one ladder attempt
 *      (`combo_logic.h:766`). The total-creation budget is printed beside a LADDER-
 *      attempt count instead, since `gen_budget.h` samples it BETWEEN attempts and it
 *      cannot truncate a RunFill at all. The first version of this row compared the
 *      roll-back product against the total and reported "three to four times over",
 *      which was the wrong budget by the total multiplier and the wrong mechanism by
 *      a level of the ladder; the honest figure is against the per-attempt budget and
 *      it is worse, not better.
 *
 * THE FOUR APPROXIMATIONS, named here so nobody reads a number as more than it
 * is:
 *
 *   (A) THE MEASURED BAG IS A SAMPLE OF THE REAL BAG BY DEFAULT. The real union
 *       bag exceeds `RSBS_COMBO_LOGIC_BAG_CAP` (the row prints by how much), and
 *       even inside the cap a fill runs one round per bag item — so a full-bag
 *       fill is minutes of wall clock and five of them is not a CI row. The row
 *       therefore measures a deterministic STRIDE SAMPLE of each half, prints the
 *       full figures beside the sampled ones, and extrapolates the full bag
 *       arithmetically — twice, once from the isolated median round and once from
 *       the in-fill per-round figure, because the two differ by a factor of two
 *       and only the second is what a budget should be compared against.
 *       `RSBS_COMBO_MEASURE_*` overrides every size, so a larger run is one env
 *       var away; the epic comment quotes the 32-item and 512-item runs.
 *
 *   (B) MM'S HOST POOL CAN BE NARROWED, AND BY DEFAULT IS NOT. The first draft of
 *       this row narrowed it unconditionally, because `ComboLogicCollectFrom` sized
 *       its scratch by `RSBS_COMBO_LOGIC_PLACEMENT_CAP` (1024) and MM's graph offers
 *       ~2250 hosts, so an honest MM enumeration refused the first bag item of the
 *       first fill with `ERR_CAPACITY`. PR #717 fixed exactly that on main — the
 *       enumeration is now bounded by `RSBS_COMBO_LOGIC_HOST_CAP` (4096), sized by
 *       the two check id-spaces rather than by the bag — so the narrowing is no
 *       longer needed and the row measures MM's WHOLE graph host set by default.
 *       `RSBS_COMBO_MEASURE_MM_HOSTS` still narrows it through
 *       `MM_ComboLogic_SetHostPool` (the increment-4 seam, doing exactly the job
 *       `GeneratePools`' `checkPool` will do) for a smaller, faster configuration.
 *       The row prints both caps against both measured figures either way.
 *
 *   (C) ONE HOST, ONE PROFILE, TRICKS OFF. The numbers are this workstation's,
 *       under the shipped default profile with MM never booted into play. Other
 *       hosts, non-default profiles and tricks-on are NOT measured; the row prints
 *       the #582 host-scale percent so a reader can at least locate this machine
 *       relative to the reference.
 *
 *   (D) ONLY OoT'S HALF OF THE BAG IS REALLY IN THE BAG. The row empties the sampled
 *       OoT hosts, so those items are in the bag and not in the world. It empties NO
 *       MM host — MM's engine exposes no `TestSetPlacedItem` equivalent and adding
 *       one would be production surface for a measurement's benefit — so every MM
 *       bag row is simultaneously in the bag and still the vanilla item of its own
 *       host. MM's side of a round therefore computes reachability over a world that
 *       still contains the items the round is assuming, which inflates MM's reached
 *       host supply and, through it, M2's round cost and the DEAD-END PREDICTOR.
 *       Conservative for a COST question (more reached hosts is more work, not less)
 *       and OPTIMISTIC for a convergence question, which is the direction that
 *       matters when reading M3.
 *
 * ============================================================================
 * WHY THE `rando` TIER
 * ============================================================================
 *
 * The same reason `oot-logic-export` and `mm-combo-logic-engine` are there, and
 * it is a correctness reason rather than a convenience one: with no generation in
 * the process every OoT location is `RG_NONE`, `ctx->allLocations` is empty, and
 * MM's `Rando::Logic::Regions` is unpopulated — so the bag would be empty, every
 * count zero, and every sanity assertion above satisfied by 0 == 0. This row runs
 * a REAL headless OoT generation and MM's real rando bring-up first, and asserts
 * strict inequalities wherever a constant would otherwise pass (S5).
 *
 * Linkage note: `#include`d into test_runner.cpp at FILE SCOPE and therefore
 * compiled as C++, like every other file in this directory.
 */

#include "../combo_logic.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../game.h"
#include "../gen_budget.h"
#include "../test_runner.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

// ---------------------------------------------------------------------------
// The two ports' bridges, declared here so this file reads on its own and does not
// silently depend on the include order of its neighbours in test_runner.cpp.
//
// WHICH ARE DUPLICATES AND WHICH ARE FIRST DECLARATIONS, spelled out because an
// earlier version of this comment claimed "every one of these is already declared
// by test_oot_logic_export.c, so re-declaring them is redundant-but-identical" —
// and that was true of ten of the seventeen entries and false of the other seven,
// which makes the stated reason for the block ("it is all duplication, therefore
// harmless") not the reason at all. It IS harmless, but for the ordinary reason a
// C declaration is: each one matches the single definition in the named TU, and a
// mismatch is a link error rather than a silent second definition. The split, so a
// future edit can keep it honest:
//
//   DUPLICATES of test_oot_logic_export.c's own block (:112, :120-141, :148-153),
//   which is #included ABOVE this file: the five OoT_ComboLogic_Test* rows used
//   here, Rando_HeadlessSeedTest, MM_Rando_InitCore, MM_ComboLogic_SnapshotLive,
//   MM_ComboLogic_ShrinkObservations, and `extern char gSaveContext[]`.
//
//   FIRST DECLARATIONS in this directory: MM_ComboLogic_PoolVanillaItems,
//   MM_ComboLogic_ApplyShippedProfile and MM_ComboLogic_ProbeGives (created by the
//   same PR as this file), plus MM_ComboLogic_SetHostPool, MM_ComboLogic_HarvestCount,
//   MM_ComboLogic_HeldPlacementCount and MM_ComboLogic_ResetCounters — those four
//   exist, but their only other declarations are in MM's OWN test TU
//   (games/mm/2s2h/mm_combo_logic_engine_test.cpp:124-130), which is a different
//   translation unit and reaches this one through nothing at all.
// ---------------------------------------------------------------------------
extern "C" {
// OoT (games/oot/soh/Enhancements/randomizer/ComboLogicEngineOoT.cpp) — duplicates.
int OoT_ComboLogic_TestOwnedHostCount(void);
int OoT_ComboLogic_TestOwnedHosts(uint16_t* out, int cap);
int OoT_ComboLogic_TestPlacedItemAt(uint16_t rc, uint16_t* outItemId, int* outAdvancement);
int OoT_ComboLogic_TestSetPlacedItem(uint16_t rc, uint16_t itemId);
uint32_t OoT_ComboLogic_TestWorldDigest(void);

// OoT's headless generation (games/oot/soh/.../randomizer.cpp via the harness) —
// a duplicate.
int Rando_HeadlessSeedTest(const char* seedStr);

// MM (games/mm/2s2h/Rando/ComboLogicEngineSingleExe.cpp) + its rando bring-up.
// MM_Rando_InitCore, MM_ComboLogic_SnapshotLive and MM_ComboLogic_ShrinkObservations
// are duplicates; the other five are first declarations here.
void MM_Rando_InitCore(void);
void MM_ComboLogic_SetHostPool(const uint16_t* checks, int count);
int MM_ComboLogic_PoolVanillaItems(uint16_t* outItems, uint16_t* outHosts, int cap);
int MM_ComboLogic_ApplyShippedProfile(void);
int MM_ComboLogic_ProbeGives(const uint16_t* ids, int count, int startIndex);
int MM_ComboLogic_ShrinkObservations(void);
int MM_ComboLogic_HarvestCount(void);
int MM_ComboLogic_SnapshotLive(void);
int MM_ComboLogic_HeldPlacementCount(void);
void MM_ComboLogic_ResetCounters(void);

// The unified save buffer (src/common/unified_save.c): the one char array both
// games reinterpret. S4 compares it byte for byte. A duplicate of
// test_oot_logic_export.c:141.
extern char gSaveContext[];
}

namespace {

// ============================================================================
// Sizes, and the env overrides that make the full-bag run possible
// ============================================================================
//
// THE DEFAULTS ARE A CI BUDGET, NOT A MEASUREMENT CHOICE. They are set so the row
// finishes inside its CTest timeout on a busy shared build host: five fills of
// (bag + 1) rounds each, plus the round-timing sample. Every one is overridable,
// and the epic comment's headline numbers come from a run with the OoT and MM bag
// halves opened up to the whole real bag.

int EnvInt(const char* name, int fallback, int lo, int hi) {
    const char* raw = getenv(name);
    if (raw == nullptr || raw[0] == '\0') {
        return fallback;
    }
    const long parsed = strtol(raw, nullptr, 10);
    if (parsed < (long)lo) {
        return lo;
    }
    if (parsed > (long)hi) {
        return hi;
    }
    return (int)parsed;
}

// ============================================================================
// Small helpers
// ============================================================================

/** A deterministic STRIDE sample of `src` down to at most `target` entries,
 *  preserving order. Stride rather than a prefix because a prefix of an
 *  ascending-id list is one corner of the world — the low `RC_*`/`RI_*` ids are a
 *  handful of scenes — and a cost measurement over one corner is a measurement of
 *  that corner. Deterministic because nothing here may consume a random number:
 *  the whole point of the row is that two runs agree. */
std::vector<uint16_t> StrideSample(const std::vector<uint16_t>& src, int target) {
    std::vector<uint16_t> out;
    if (src.empty() || target <= 0) {
        return out;
    }
    if ((int)src.size() <= target) {
        return src;
    }
    const size_t stride = src.size() / (size_t)target;
    for (size_t i = 0; i < src.size() && (int)out.size() < target; i += stride) {
        out.push_back(src[i]);
    }
    return out;
}

/**
 * How many of `ids` are DISTINCT — i.e. how many copies are REPEATS of an id
 * already in the bag.
 *
 * THIS WAS THE DEAD-END'S CAUSE, and it is printed so the fix stays measured.
 * Under combo_logic.h ABI 2 both engines' `assumeOwnItem` de-duplicated by id
 * within a round (OoT because its progressive rows walk past the top tier, MM
 * because its counter gives `++`), so a round held ONE of each id: the 2026-09-22
 * measurement found OoT's whole advancement half to be 321 rows but only 105
 * distinct ids, and no world could be proved (#645). ABI 3 (the operator's
 * multiplicity ruling, 2026-09-26) makes every row a counted copy — OoT clamps
 * progressive copies at the top tier, MM clamps its counters at their maxima —
 * so the repeats this function counts are now copies the round SEES. The number
 * is kept beside the proof result as the attribution: a proof that still fails
 * with every copy counted fails for a different reason.
 */
int DistinctCount(const std::vector<uint16_t>& ids) {
    std::vector<bool> seen(1u << 16, false);
    int distinct = 0;
    for (const uint16_t id : ids) {
        if (!seen[id]) {
            seen[id] = true;
            ++distinct;
        }
    }
    return distinct;
}

/** Milliseconds, wall clock, as a double so a sub-millisecond round does not read
 *  as zero. `steady_clock` and not `system_clock`: nothing here is compared with a
 *  timestamp taken elsewhere, and a monotonic clock cannot be moved by NTP
 *  mid-measurement. */
double NowMs() {
    using namespace std::chrono;
    return (double)duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count() / 1000.0;
}

struct Stats {
    double min;
    double median;
    double max;
    double total;
};

Stats Summarize(std::vector<double> samples) {
    Stats s = { 0.0, 0.0, 0.0, 0.0 };
    if (samples.empty()) {
        return s;
    }
    // Insertion sort: the sample is single digits and pulling in <algorithm>'s
    // sort for it would be the larger dependency.
    for (size_t i = 1; i < samples.size(); ++i) {
        const double key = samples[i];
        size_t j = i;
        while (j > 0 && samples[j - 1] > key) {
            samples[j] = samples[j - 1];
            --j;
        }
        samples[j] = key;
    }
    s.min = samples.front();
    s.max = samples.back();
    s.median = samples[samples.size() / 2];
    for (const double v : samples) {
        s.total += v;
    }
    return s;
}

/** One fill, timed, with everything the epic asked for printed on one line. */
struct FillMeasurement {
    int status;
    double wallMs;
    ComboLogicFillResult res;
};

FillMeasurement RunTimedFill(const char* label, const ComboLogicBagItem* bag, int bagCount, uint8_t goal,
                             uint8_t rung, uint32_t seed, int maxAttempts) {
    FillMeasurement m;
    memset(&m, 0, sizeof(m));

    ComboLogicFillRequest req;
    memset(&req, 0, sizeof(req));
    req.bag = bag;
    req.bagCount = bagCount;
    req.goal = goal;
    req.logicRung = rung;
    req.seed = seed;
    req.maxAttempts = maxAttempts;

    const double t0 = NowMs();
    m.status = Combo_Logic_RunFill(&req, &m.res);
    m.wallMs = NowMs() - t0;

    // rounds per PLACED item, which is the shape audit §4.3 predicts ("the fill
    // will call the round O(#bag items) times") and the number that decides
    // whether the fill fits a budget. Reported as -1.0 when nothing was placed,
    // rather than dividing by zero and printing `inf`.
    const double roundsPerPlaced = (m.res.placed > 0) ? ((double)m.res.rounds / (double)m.res.placed) : -1.0;

    printf("[TEST] combo-logic-measure: FILL %-22s status=%-16s wall=%.1fms attempts=%d placed=%d rounds=%d "
           "rounds/placed=%.2f goalProven=%d proofSkipped=%d allHostsReached=%d digest=%08X\n",
           label, Combo_Logic_StatusName(m.status), m.wallMs, m.res.attempts, m.res.placed, m.res.rounds,
           roundsPerPlaced, m.res.goalProven ? 1 : 0, m.res.proofSkipped ? 1 : 0, m.res.allHostsReached ? 1 : 0,
           m.res.placementDigest);
    // THE BAG MODEL's accounting (combo_logic.h). The shipped profile has no
    // plentiful pool, so surplus is 0 here; leftover hosts are what each game's
    // own junk pass would fill.
    printf("[TEST] combo-logic-measure:      %-22s required placed=%d surplus placed=%d dropped=%d leftover hosts "
           "OoT=%d MM=%d\n",
           label, m.res.requiredPlaced, m.res.surplusPlaced, m.res.surplusDropped, m.res.leftoverHostsOoT,
           m.res.leftoverHostsMM);
    // Roll-backs and dead-ends, spelled out rather than left to be inferred from
    // `attempts`: a batch roll-back is what the coordinator does on a dead end
    // WITHIN one seed, and it is a different mechanism from the attempt ladder
    // that re-rolls the seed one level up (combo_logic.h says so; conflating the
    // two produced the silent-vanilla-revert class).
    printf("[TEST] combo-logic-measure:      %-22s batch roll-backs=%d  terminal dead-end=%s  goal-unprovable=%s\n",
           label, (m.res.attempts > 0) ? (m.res.attempts - 1) : 0,
           (m.status == RSBS_COMBO_LOGIC_ERR_NO_CANDIDATE) ? "yes" : "no",
           (m.status == RSBS_COMBO_LOGIC_ERR_GOAL_UNPROVABLE) ? "yes" : "no");
    return m;
}

#define CLM_ASSERT(cond, msg)                                                    \
    do {                                                                         \
        if (!(cond)) {                                                           \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__);       \
            return TEST_FAIL;                                                    \
        }                                                                        \
    } while (0)

} // namespace

// ============================================================================
// THE NULL-PLAY GIVE PROBE — ITS OWN DISPATCH, `combo-logic-give-probe`
// ============================================================================
//
// It walks MM's whole graph-wide vanilla item list through `Rando::GiveItem` one id
// at a time, printing each id to stderr BEFORE giving it, and returns. Its purpose
// is to CRASH on an id whose give dereferences a NULL `MM_gPlayState` or a NULL
// `gRegEditor`, so that the id names itself in the log — which is how the exclusion
// list in `MM_ComboLogic_PoolVanillaItems` and the `gRegEditor` stand-in in
// `MM_ComboLogic_ApplyShippedProfile` were both derived, and how they must be
// re-derived whenever MM's item table or `Item_GiveImpl`'s guards change.
//
// WHY IT IS ITS OWN DISPATCH AND NOT AN ENV MODE OF THE ROW BELOW. It was an env
// mode (`RSBS_COMBO_MEASURE_PROBE`) for one revision of this file, and the review of
// PR #722 caught what that costs: the row returned TEST_PASS from the probe branch,
// BEFORE the OoT generation, before M1/M2/M3 and before every sanity assert except
// S1. An environment variable therefore turned the ComboLogicMeasure CTest row into
// a green no-op that measured nothing — and CTest does not scrub the inherited
// environment, so a developer shell or a CI image that exported the variable got
// exactly that. This file's own "WHY THE rando TIER" paragraph exists to keep the
// row from passing vacuously; a mode that made it pass vacuously by accident was
// the same defect wearing the harness's own clothes. Two dispatch names cannot
// collide that way: `combo-logic-measure` always measures, and this one always
// probes.
//
// IT HAS NO CTEST ROW, deliberately: it is designed to abort the process, and a row
// whose intended outcome is an access violation is not a lock. It is also in the
// `--test all` skip list beside its sibling, for the same display/OTR reason.
//
// `RSBS_COMBO_PROBE_FROM=<n>` resumes at index n. Resuming is for walking PAST an
// id already known to fault and nothing else: the gives are CUMULATIVE into one
// save, so a resumed run grants none of the ids before n, and the claim it supports
// is correspondingly weaker. MM_ComboLogic_ProbeGives' own doc states that in full.
TestResult ComboLogicGiveProbe_Run(void) {
    printf("[TEST] combo-logic-give-probe: walking MM's giveable vanilla item list through Rando::GiveItem with "
           "MM_gPlayState NULL; a crash names the id on stderr. This dispatch MEASURES NOTHING — combo-logic-measure "
           "does that (#645, audit §6.3)\n");

    const int probeFrom = EnvInt("RSBS_COMBO_PROBE_FROM", 0, 0, 1 << 20);
    MM_Rando_InitCore();
    std::unique_ptr<unsigned char[]> probeSave(new unsigned char[OOT_SAVE_CONTEXT_SIZE]);
    memcpy(probeSave.get(), gSaveContext, OOT_SAVE_CONTEXT_SIZE);
    MM_ComboLogic_ApplyShippedProfile();
    const int probeTotal = MM_ComboLogic_PoolVanillaItems(nullptr, nullptr, 0);
    CLM_ASSERT(probeTotal > 0, "MM's graph yields no giveable vanilla item — there is nothing to probe");
    std::vector<uint16_t> probeIds((size_t)probeTotal, 0);
    MM_ComboLogic_PoolVanillaItems(probeIds.data(), nullptr, probeTotal);
    printf("[TEST] combo-logic-give-probe: %d ids in the list, starting at index %d%s\n", probeTotal, probeFrom,
           (probeFrom > 0) ? " (RESUMED: the ids before this index are NOT granted, so a fault that needs them is "
                             "unreachable by this run)"
                           : " (a full cumulative walk of the list)");
    fflush(stdout);

    const int returned = MM_ComboLogic_ProbeGives(probeIds.data(), probeTotal, probeFrom);
    memcpy(gSaveContext, probeSave.get(), OOT_SAVE_CONTEXT_SIZE);
    printf("[TEST] combo-logic-give-probe: %d of %d gives RETURNED (the process survived them). This is a statement "
           "about ONE order and ONE starting profile, over a list that excludes RI_TRAP and RI_TRIFORCE_PIECE a "
           "priori; it is not a statement that no order faults.\n",
           returned, probeTotal - probeFrom);
    printf("[TEST] PASS: the give probe walked %d ids without a fault\n", returned);
    return TEST_PASS;
}

// ============================================================================
// The row
// ============================================================================

TestResult ComboLogicMeasure_Run(void) {
    printf("[TEST] combo-logic-measure: the linked round's cost and the single-bag fill's convergence, over BOTH real "
           "engines (#645 increment 3 work items 1 and 2; audit §6.3)\n");

    // ------------------------------------------------------------------
    // S1: both engines registered.
    // ------------------------------------------------------------------
    const ComboLogicEngine* oot = Combo_Logic_GetEngine(GAME_OOT);
    const ComboLogicEngine* mm = Combo_Logic_GetEngine(GAME_MM);
    CLM_ASSERT(oot != nullptr, "no OoT engine is registered — the soh_rando registrar was elided or refused");
    CLM_ASSERT(mm != nullptr, "no MM engine is registered — the 2ship_rando registrar was elided or refused");
    CLM_ASSERT(oot->abiVersion == RSBS_COMBO_LOGIC_ENGINE_ABI && mm->abiVersion == RSBS_COMBO_LOGIC_ENGINE_ABI,
               "a registered engine carries the wrong ABI");

    const int ootBagTarget = EnvInt("RSBS_COMBO_MEASURE_OOT_BAG", 16, 1, RSBS_COMBO_LOGIC_BAG_CAP);
    const int mmBagTarget = EnvInt("RSBS_COMBO_MEASURE_MM_BAG", 16, 1, RSBS_COMBO_LOGIC_BAG_CAP);
    // 0 (the default) means DO NOT NARROW: measure MM's whole graph host set. See
    // approximation (B) — the narrowing existed only to dodge a cap PR #717 removed.
    const int mmHostTarget = EnvInt("RSBS_COMBO_MEASURE_MM_HOSTS", 0, 0, RSBS_COMBO_LOGIC_HOST_CAP);
    const int roundSamples = EnvInt("RSBS_COMBO_MEASURE_ROUNDS", 5, 1, 64);
    const int fillAttempts = EnvInt("RSBS_COMBO_MEASURE_ATTEMPTS", 3, 1, RSBS_COMBO_LOGIC_FILL_RETRIES);
    printf("[TEST] combo-logic-measure: configuration oot-bag<=%d mm-bag<=%d mm-hosts=%s round-samples=%d "
           "fill-attempts=%d (all four are RSBS_COMBO_MEASURE_* overridable; the defaults are a CI budget)\n",
           ootBagTarget, mmBagTarget, (mmHostTarget > 0) ? "narrowed (see below)" : "MM's whole graph", roundSamples,
           fillAttempts);

    // ------------------------------------------------------------------
    // This host, against #582's budget. Printed, never asserted.
    // ------------------------------------------------------------------
    printf("[TEST] combo-logic-measure: #582 budget on this host: floor=%ums ceiling=%ums hostScale=%u%% "
           "per-attempt=%ums total-creation=%ums\n",
           (unsigned)RSBS_GENBUDGET_FLOOR_MS, (unsigned)RSBS_GENBUDGET_CEILING_MS,
           (unsigned)Combo_GenBudget_HostScalePercent(), (unsigned)Combo_GenBudget_FillBudgetMs(0),
           (unsigned)Combo_GenBudget_TotalBudgetMs());

    // NO ENV-GATED EARLY RETURN LIVES HERE ANY MORE. The give probe used to be a
    // `RSBS_COMBO_MEASURE_PROBE` mode of this function that returned TEST_PASS
    // before the generation and before every assert except S1, so an exported
    // environment variable could turn this row green without measuring anything.
    // It is now its own dispatch (`ComboLogicGiveProbe_Run` above,
    // `redship --test combo-logic-give-probe`). This function has exactly one path
    // and it always measures.

    // ------------------------------------------------------------------
    // A REAL OoT generation, then MM's rando bring-up. Everything below is
    // vacuous without both.
    // ------------------------------------------------------------------
    const double genT0 = NowMs();
    const int rc = Rando_HeadlessSeedTest("RSBSCOMBOMEASURE1");
    const double genMs = NowMs() - genT0;
    CLM_ASSERT(rc == 0, "headless OoT seed generation failed");
    printf("[TEST] combo-logic-measure: OoT headless generation took %.1fms (the per-game half of a creation, for "
           "scale — the coordinator's cost is measured on top of this, not instead of it)\n",
           genMs);
    MM_Rando_InitCore();

    // ------------------------------------------------------------------
    // THE OUTER BRACKET. Everything below perturbs the world or the save, and S4
    // requires both back byte-identical. The save copy is heap, not a static: the
    // unified buffer is 136 KB and this runs at an arbitrary harness stack depth.
    // ------------------------------------------------------------------
    std::unique_ptr<unsigned char[]> saveBefore(new unsigned char[OOT_SAVE_CONTEXT_SIZE]);
    memcpy(saveBefore.get(), gSaveContext, OOT_SAVE_CONTEXT_SIZE);
    const uint32_t worldDigest0 = OoT_ComboLogic_TestWorldDigest();
    CLM_ASSERT(worldDigest0 != 0u, "OoT's world digest is zero — no fill result is visible to the engine");
    printf("[TEST] combo-logic-measure: OoT world placement digest after generation = %08X\n", worldDigest0);

    MM_ComboLogic_ResetCounters();
    const int mmLogicMode = MM_ComboLogic_ApplyShippedProfile();
    printf("[TEST] combo-logic-measure: MM profile resolved from the shipped CVars; RO_LOGIC=%d, starting items "
           "granted (the state the creation seam hands MM's engine — NOT the zeroed save mm-combo-logic-engine "
           "measures)\n",
           mmLogicMode);

    // ------------------------------------------------------------------
    // S4's INNER baseline, and the reason there are two.
    // ------------------------------------------------------------------
    // `saveBefore` above is the state this row must LEAVE BEHIND, and it is restored
    // by memcpy in the teardown. Comparing against it after that memcpy is a
    // tautology — it compares a buffer with the buffer it was just copied from — and
    // for one revision of this file that tautology was the row's most advertised
    // assertion. `saveAfterProfile` is the honest baseline: MM's save exactly as
    // `MM_ComboLogic_ApplyShippedProfile` left it, before any round or fill ran. The
    // MEASUREMENT must return the save to this, and nothing in the measurement
    // restores it from a copy — every round's Snapshot/Restore bracket has to do it
    // for real, MM's placement re-apply has to be undone with it, and OoT's
    // beginQuery/endQuery has to leave the live save alone. That assertion can go
    // red, and S4 below is now that one. The outer restore is stated as construction
    // rather than asserted as a property, because that is what it is.
    std::unique_ptr<unsigned char[]> saveAfterProfile(new unsigned char[OOT_SAVE_CONTEXT_SIZE]);
    memcpy(saveAfterProfile.get(), gSaveContext, OOT_SAVE_CONTEXT_SIZE);
    // A premise check on the two baselines, not a property of the measurement:
    // applying the shipped profile MUST move the save, or `saveAfterProfile` is the
    // same bytes as `saveBefore` and S4 is back to being a statement about nothing.
    CLM_ASSERT(memcmp(saveBefore.get(), saveAfterProfile.get(), OOT_SAVE_CONTEXT_SIZE) != 0,
               "applying MM's shipped profile changed no byte of the unified save buffer — either the profile did not "
               "apply or MM's save does not live in this buffer, and either way S4 below would be vacuous");

    // ==================================================================
    // M1: THE UNION BAG'S REAL SIZE, against the coordinator's caps.
    // ==================================================================

    // --- OoT's half, read back from the generated world.
    const int ootOwned = OoT_ComboLogic_TestOwnedHostCount();
    CLM_ASSERT(ootOwned > 0, "the OoT engine owns no host over a generated world");
    std::vector<uint16_t> ootOwnedHosts((size_t)ootOwned, 0);
    CLM_ASSERT(OoT_ComboLogic_TestOwnedHosts(ootOwnedHosts.data(), ootOwned) == ootOwned,
               "the OoT owned-host bridge disagreed with its own count");

    std::vector<uint16_t> ootAdvHosts;
    std::vector<uint16_t> ootAdvItems;
    for (const uint16_t host : ootOwnedHosts) {
        uint16_t item = 0;
        int advancement = 0;
        if (OoT_ComboLogic_TestPlacedItemAt(host, &item, &advancement) != 1) {
            continue;
        }
        if (advancement != 0) {
            ootAdvHosts.push_back(host);
            ootAdvItems.push_back(item);
        }
    }
    const int ootFullBagHalf = (int)ootAdvItems.size();
    CLM_ASSERT(ootFullBagHalf > 0, "the generated OoT world holds no advancement item — the bag would be empty");

    // --- MM's half. FIRST over the whole graph, which is the figure the epic
    //     needs; the pool narrowing (approximation B) comes after.
    const int mmGraphHosts = mm->allEmptyHosts(mm->self, nullptr, 0);
    const int mmGraphItems = MM_ComboLogic_PoolVanillaItems(nullptr, nullptr, 0);
    CLM_ASSERT(mmGraphHosts > 0, "MM's engine offers no host at all — the region graph did not come up");
    CLM_ASSERT(mmGraphItems > 0, "MM's graph yields no giveable vanilla item — the static check table did not come up");

    const int fullUnionBag = ootFullBagHalf + mmGraphItems;
    printf("[TEST] combo-logic-measure: === M1 THE REAL UNION BAG AND THE COORDINATOR'S CAPS ===\n");
    printf("[TEST] combo-logic-measure: OoT owned hosts=%d, of which advancement-bearing=%d (a SUPERSET of the last "
           "general pass: the six restricted passes place advancement items too and their hosts are "
           "indistinguishable post-fill)\n",
           ootOwned, ootFullBagHalf);
    printf("[TEST] combo-logic-measure: MM graph hosts=%d, giveable vanilla items over them=%d (GeneratePools' own "
           "walk without its RNG; settings-conditional narrowing NOT applied, so a superset)\n",
           mmGraphHosts, mmGraphItems);
    printf("[TEST] combo-logic-measure: union bag=%d against RSBS_COMBO_LOGIC_BAG_CAP=%d -> %s by %d\n", fullUnionBag,
           (int)RSBS_COMBO_LOGIC_BAG_CAP, (fullUnionBag > RSBS_COMBO_LOGIC_BAG_CAP) ? "OVER the cap" : "inside the cap",
           (fullUnionBag > RSBS_COMBO_LOGIC_BAG_CAP) ? (fullUnionBag - (int)RSBS_COMBO_LOGIC_BAG_CAP)
                                                     : ((int)RSBS_COMBO_LOGIC_BAG_CAP - fullUnionBag));
    // TWO DIFFERENT CAPS, and conflating them is a mistake this row made once. What
    // bounds ONE ENUMERATION is RSBS_COMBO_LOGIC_HOST_CAP (PR #717 introduced it
    // precisely because the placement cap was wrong for this job and an honest MM
    // enumeration refused the first bag item of the first fill). What bounds
    // PLACEMENTS ON ONE HOST GAME is RSBS_COMBO_LOGIC_PLACEMENT_CAP, and a fill can
    // only exceed it by placing that many items, which the bag cap already bounds.
    printf("[TEST] combo-logic-measure: MM hosts=%d against RSBS_COMBO_LOGIC_HOST_CAP=%d (what bounds ONE "
           "enumeration; ComboLogicCollectFrom refuses rather than truncates above it) -> %s\n",
           mmGraphHosts, (int)RSBS_COMBO_LOGIC_HOST_CAP,
           (mmGraphHosts > RSBS_COMBO_LOGIC_HOST_CAP) ? "OVER the cap" : "inside the cap");
    printf("[TEST] combo-logic-measure: OoT hosts=%d against the same host cap -> %s\n", ootOwned,
           (ootOwned > RSBS_COMBO_LOGIC_HOST_CAP) ? "OVER the cap" : "inside the cap");
    printf("[TEST] combo-logic-measure: RSBS_COMBO_LOGIC_PLACEMENT_CAP=%d bounds PLACEMENTS PER HOST GAME, not the "
           "enumeration: MM's %d hosts exceed it, but a fill cannot place on more than min(bag, cap) of them and the "
           "bag cap is %d — so it binds only if the bag ever outgrows it.\n",
           (int)RSBS_COMBO_LOGIC_PLACEMENT_CAP, mmGraphHosts, (int)RSBS_COMBO_LOGIC_BAG_CAP);

    // ------------------------------------------------------------------
    // Approximation B: narrow MM's host pool ONLY if asked. Restored to the graph
    // default in the teardown either way.
    // ------------------------------------------------------------------
    int mmPooledHosts = mmGraphHosts;
    if (mmHostTarget > 0) {
        std::vector<uint16_t> mmAllHosts((size_t)mmGraphHosts, 0);
        CLM_ASSERT(mm->allEmptyHosts(mm->self, mmAllHosts.data(), mmGraphHosts) == mmGraphHosts,
                   "MM's allEmptyHosts disagreed with its own count");
        const std::vector<uint16_t> mmPool = StrideSample(mmAllHosts, mmHostTarget);
        MM_ComboLogic_SetHostPool(mmPool.data(), (int)mmPool.size());
        mmPooledHosts = mm->allEmptyHosts(mm->self, nullptr, 0);
        printf("[TEST] combo-logic-measure: MM host pool NARROWED through MM_ComboLogic_SetHostPool to %d hosts "
               "(stride sample of %d; the increment-4 seam standing in for GeneratePools' checkPool)\n",
               mmPooledHosts, mmGraphHosts);
        CLM_ASSERT(mmPooledHosts > 0, "the narrowed MM pool is empty");
    } else {
        printf("[TEST] combo-logic-measure: MM host pool NOT narrowed: all %d graph hosts are offered, which is what "
               "PR #717's RSBS_COMBO_LOGIC_HOST_CAP made possible\n",
               mmGraphHosts);
    }
    CLM_ASSERT(mmPooledHosts <= RSBS_COMBO_LOGIC_HOST_CAP,
               "MM offers more hosts in one enumeration than RSBS_COMBO_LOGIC_HOST_CAP allows, so every fill below "
               "would refuse with ERR_CAPACITY on its first bag item");

    // ------------------------------------------------------------------
    // THE MEASUREMENT BAG, and the ASYMMETRY between its two halves.
    // ------------------------------------------------------------------
    // OoT's half comes from hosts this row EMPTIES, so those items are genuinely in
    // the bag and not simultaneously in the world. Only the SAMPLED OoT hosts are
    // emptied: emptying every advancement-bearing host would collapse the world
    // towards sphere zero and make the final round's GOAL unprovable for a reason
    // that is about the fixture rather than about the fill.
    //
    // MM'S HALF IS NOT SYMMETRIC WITH THAT, and approximation (D) in the file header
    // is this fact. It comes from `MM_ComboLogic_PoolVanillaItems` over the CURRENT
    // host universe — which by default (`RSBS_COMBO_MEASURE_MM_HOSTS=0`) is MM's
    // WHOLE graph and not a narrowed pool; an earlier version of this comment said
    // "the narrowed pool's vanilla items" and was left behind when the narrowing
    // became opt-in. Nothing here empties an MM host, because MM's engine has no
    // `TestSetPlacedItem` equivalent and adding one would be production surface for a
    // measurement's benefit. So every MM bag row is an item that is in the bag AND
    // still the vanilla item of its own host, which inflates the reachability a
    // round computes over MM's side — in exactly the quantity M2 is measuring. The
    // OoT half is clean; the MM half is not; the numbers are read accordingly.
    // ------------------------------------------------------------------
    std::vector<uint16_t> ootBagHostsAll = ootAdvHosts;
    const std::vector<uint16_t> ootBagHosts = StrideSample(ootBagHostsAll, ootBagTarget);
    std::vector<uint16_t> ootBagItems;
    std::vector<uint16_t> ootRestoreItems;
    for (const uint16_t host : ootBagHosts) {
        uint16_t item = 0;
        int advancement = 0;
        if (OoT_ComboLogic_TestPlacedItemAt(host, &item, &advancement) != 1 || advancement == 0) {
            continue;
        }
        ootBagItems.push_back(item);
        ootRestoreItems.push_back(item);
        CLM_ASSERT(OoT_ComboLogic_TestSetPlacedItem(host, 0) == 1, "could not empty an OoT host to build the bag");
    }
    CLM_ASSERT(ootBagItems.size() == ootBagHosts.size(), "an OoT bag host lost its item between two reads");

    std::vector<uint16_t> mmPooledItems((size_t)MM_ComboLogic_PoolVanillaItems(nullptr, nullptr, 0), 0);
    const int mmPooledItemTotal = (int)mmPooledItems.size();
    if (mmPooledItemTotal > 0) {
        MM_ComboLogic_PoolVanillaItems(mmPooledItems.data(), nullptr, mmPooledItemTotal);
    }
    const std::vector<uint16_t> mmBagItems = StrideSample(mmPooledItems, mmBagTarget);

    std::vector<ComboLogicBagItem> bag;
    for (const uint16_t id : ootBagItems) {
        ComboLogicBagItem row;
        memset(&row, 0, sizeof(row));
        row.item.originGame = (uint8_t)GAME_OOT;
        row.item.id = id;
        // itemClass 0 deliberately: combo_logic.h CARRIES the class and filters
        // nothing by it, and answer O8's single-owner classification table does
        // not exist yet. Passing a made-up class here would make the measurement
        // depend on a value nobody owns.
        bag.push_back(row);
    }
    for (const uint16_t id : mmBagItems) {
        ComboLogicBagItem row;
        memset(&row, 0, sizeof(row));
        row.item.originGame = (uint8_t)GAME_MM;
        row.item.id = id;
        bag.push_back(row);
    }
    const int bagCount = (int)bag.size();
    CLM_ASSERT(bagCount > 0, "the measurement bag is empty");
    CLM_ASSERT(bagCount <= RSBS_COMBO_LOGIC_BAG_CAP, "the measurement bag exceeds the coordinator's bag cap");
    printf("[TEST] combo-logic-measure: measurement bag=%d (OoT %d of %d, MM %d of %d) — a deterministic stride "
           "sample; see approximation (A)\n",
           bagCount, (int)ootBagItems.size(), ootFullBagHalf, (int)mmBagItems.size(), mmPooledItemTotal);

    // THE REPEATS. Under ABI 2 both engines dropped these rows within a round;
    // under ABI 3 every one is a counted copy (see DistinctCount).
    const int ootDistinct = DistinctCount(ootBagItems);
    const int mmDistinct = DistinctCount(mmBagItems);
    const int ootFullDistinct = DistinctCount(ootAdvItems);
    printf("[TEST] combo-logic-measure: MULTIPLICITY: OoT half %d rows = %d distinct ids + %d repeated copies, MM half "
           "%d rows = %d distinct + %d repeated. The WHOLE OoT advancement half is %d rows = %d distinct + %d "
           "repeated. Under ABI 3 every repeated copy is COUNTED by the round (one assumeOwnItem per copy; OoT clamps "
           "progressives at the top tier, MM clamps counters at their maxima) — under ABI 2 all of them were "
           "dropped, which is what made the proof fail on 2026-09-22.\n",
           (int)ootBagItems.size(), ootDistinct, (int)ootBagItems.size() - ootDistinct, (int)mmBagItems.size(),
           mmDistinct, (int)mmBagItems.size() - mmDistinct, ootFullBagHalf, ootFullDistinct,
           ootFullBagHalf - ootFullDistinct);

    // ==================================================================
    // M2: ONE LINKED ROUND, at the fill's most expensive assumed-set size.
    // ==================================================================
    printf("[TEST] combo-logic-measure: === M2 ONE LINKED ROUND (assumed set = the whole bag, %d items) ===\n",
           bagCount);
    Combo_Logic_ResetPlacements();

    std::vector<double> roundMs;
    std::vector<int> roundIterations;
    ComboLogicRoundResult firstRound;
    memset(&firstRound, 0, sizeof(firstRound));
    for (int i = 0; i < roundSamples; ++i) {
        ComboLogicRoundRequest req;
        memset(&req, 0, sizeof(req));
        req.assumed = bag.data();
        req.assumedCount = bagCount;
        req.goal = RSBS_COMBO_GOAL_BEAT_EITHER;

        ComboLogicRoundResult res;
        const double t0 = NowMs();
        const int status = Combo_Logic_RunRound(&req, &res);
        const double ms = NowMs() - t0;

        CLM_ASSERT(status == RSBS_COMBO_LOGIC_OK, "a linked round over both real engines did not succeed");
        // S2: the watchdog bound is a premise check, never a working limit.
        CLM_ASSERT(res.iterations > 0 && res.iterations < RSBS_COMBO_LOGIC_MAX_ROUND_ITERATIONS,
                   "a linked round did not converge inside the watchdog bound");
        roundMs.push_back(ms);
        roundIterations.push_back(res.iterations);
        if (i == 0) {
            firstRound = res;
        }
        printf("[TEST] combo-logic-measure: round %d: %.1fms alternations=%d crossingOoT=%d crossingMM=%d "
               "candidatesOoT=%d candidatesMM=%d exchanged=%d goalOoT=%d goalMM=%d goalExpr=%d\n",
               i + 1, ms, res.iterations, res.crossingOpenOoT, res.crossingOpenMM, res.candidatesOoT,
               res.candidatesMM, res.exchanged, res.goalOoT, res.goalMM, res.goalExpression);
    }

    const Stats roundStats = Summarize(roundMs);
    const Stats iterStats = Summarize(std::vector<double>(roundIterations.begin(), roundIterations.end()));
    printf("[TEST] combo-logic-measure: ROUND COST over %d samples: min=%.1fms median=%.1fms max=%.1fms; "
           "alternations min=%.0f median=%.0f max=%.0f\n",
           roundSamples, roundStats.min, roundStats.median, roundStats.max, iterStats.min, iterStats.median,
           iterStats.max);

    // THE DEAD-END PREDICTOR, printed because it is what a failed fill below
    // means. An assumed fill's assumed set SHRINKS as items are placed (the
    // assumed set is the bag MINUS what is already placed), so the reached-host
    // supply is at its LARGEST in the first round and falls monotonically from
    // there. The first round's supply is therefore an UPPER BOUND on how many
    // items the proved rung can ever place: a bag larger than this number
    // dead-ends with ERR_NO_CANDIDATE for a capacity reason, not a logic one, and
    // no re-shuffle within the seed can fix it.
    const int reachedSupply = firstRound.candidatesOoT + firstRound.candidatesMM;
    printf("[TEST] combo-logic-measure: DEAD-END PREDICTOR: first-round reached empty hosts = %d OoT + %d MM = %d, "
           "against a bag of %d -> %s. The assumed set shrinks as the fill places, so this supply only falls; a bag "
           "above it cannot be placed under the proved rung however often the batch is rolled back.\n",
           firstRound.candidatesOoT, firstRound.candidatesMM, reachedSupply, bagCount,
           (reachedSupply >= bagCount) ? "sufficient at the start" : "ALREADY SHORT at the start");

    // S5: non-vacuity. Both sides must actually be participating, or every
    // number above is a statement about one engine.
    CLM_ASSERT(firstRound.candidatesOoT > 0, "the round collected no OoT candidate host — OoT's half did not run");
    CLM_ASSERT(firstRound.candidatesMM > 0,
               "the round collected no MM candidate host — MM's half did not run, or the arrival gate held it shut, "
               "so nothing measured here is a PAIR-level cost");
    CLM_ASSERT(firstRound.crossingOpenOoT == 1,
               "OoT's crossing read CLOSED, so the arrival gate suppressed MM for the whole measurement");

    // ==================================================================
    // M3: THE FILL — convergence, per rung, per GOAL, per seed.
    // ==================================================================
    printf("[TEST] combo-logic-measure: === M3 THE SINGLE-BAG ASSUMED FILL ===\n");
    const uint32_t kSeedA = 0xC0FFEE01u;
    const uint32_t kSeedB = 0x5EED0002u;
    const uint32_t kSeedC = 0xA11CE003u;

    // (1) the proved no-tricks rung, default GOAL. `beat-both` is the shipped
    //     default (O11) and it is measured FIRST, with one attempt: MM's half has
    //     to prove Majora defeated from South Clock Town, and whether it can under
    //     a sampled bag is exactly the thing being measured rather than assumed.
    const FillMeasurement beatBoth =
        RunTimedFill("beatable/beat-both", bag.data(), bagCount, RSBS_COMBO_GOAL_BEAT_BOTH,
                     RSBS_COMBO_RUNG_BEATABLE, kSeedA, 1);

    // (2) the same rung under `beat-either`, which OoT's half alone can satisfy —
    //     the configuration in which the fill's CONVERGENCE (rather than MM's
    //     goal) is what the numbers are about.
    const FillMeasurement beatEitherA = RunTimedFill("beatable/beat-either A", bag.data(), bagCount,
                                                     RSBS_COMBO_GOAL_BEAT_EITHER, RSBS_COMBO_RUNG_BEATABLE,
                                                     kSeedA, fillAttempts);
    CLM_ASSERT(beatEitherA.res.rounds > 0, "the proved rung ran no round — the fill took the `none` path");
    CLM_ASSERT(beatEitherA.res.attempts > 0, "the proved rung was refused before any attempt ran");

    // (3) the `none` rung: no round at all, hosts from allEmptyHosts, nothing
    //     proved. The contrast is the measurement — it is the cost floor the
    //     operator's base mode pays.
    const FillMeasurement noneRung = RunTimedFill("none/beat-both", bag.data(), bagCount, RSBS_COMBO_GOAL_BEAT_BOTH,
                                                 RSBS_COMBO_RUNG_NONE, kSeedA, 1);
    CLM_ASSERT(noneRung.res.rounds == 0, "the `none` rung ran a round — it is specified to run none at all");
    if (noneRung.status == RSBS_COMBO_LOGIC_OK) {
        CLM_ASSERT(noneRung.res.proofSkipped && !noneRung.res.goalProven,
                   "the `none` rung reported a proof it is specified not to attempt");
    }

    // (4) variance: two more coordinator seeds, same bag, same rung.
    const FillMeasurement beatEitherB = RunTimedFill("beatable/beat-either B", bag.data(), bagCount,
                                                     RSBS_COMBO_GOAL_BEAT_EITHER, RSBS_COMBO_RUNG_BEATABLE,
                                                     kSeedB, fillAttempts);
    const FillMeasurement beatEitherC = RunTimedFill("beatable/beat-either C", bag.data(), bagCount,
                                                     RSBS_COMBO_GOAL_BEAT_EITHER, RSBS_COMBO_RUNG_BEATABLE,
                                                     kSeedC, fillAttempts);

    // (5) S3: same seed reproduces, different seed does not. Without the second
    //     half "reproducible" is satisfied by a constant digest.
    const FillMeasurement beatEitherARepeat = RunTimedFill("beatable/beat-either A'", bag.data(), bagCount,
                                                           RSBS_COMBO_GOAL_BEAT_EITHER, RSBS_COMBO_RUNG_BEATABLE,
                                                           kSeedA, fillAttempts);
    CLM_ASSERT(beatEitherARepeat.status == beatEitherA.status,
               "the same seed gave a different STATUS on a second fill — the fill is not a pure function of its "
               "inputs");
    CLM_ASSERT(beatEitherARepeat.res.placed == beatEitherA.res.placed &&
                   beatEitherARepeat.res.placementDigest == beatEitherA.res.placementDigest,
               "the same seed placed differently on a second fill over the same bag and the same engines");
    // The control is guarded on both fills having PLACED something, not on both
    // having succeeded: a fill that dead-ends still placed hundreds of items and
    // its digest is still a function of the seed, so requiring success here would
    // make the control skip in exactly the configurations where the fill is most
    // interesting.
    if (beatEitherA.res.placed > 0 && beatEitherB.res.placed > 0) {
        CLM_ASSERT(beatEitherB.res.placementDigest != beatEitherA.res.placementDigest,
                   "two DIFFERENT coordinator seeds placed identically — the seed does not reach the draw, so the "
                   "same-seed agreement above is a statement about a constant");
    }

    const Stats fillStats = Summarize({ beatEitherA.wallMs, beatEitherB.wallMs, beatEitherC.wallMs });
    printf("[TEST] combo-logic-measure: FILL VARIANCE across 3 coordinator seeds (beatable/beat-either): "
           "min=%.1fms median=%.1fms max=%.1fms\n",
           fillStats.min, fillStats.median, fillStats.max);

    // ==================================================================
    // THE #582 ARITHMETIC. Measurement, then extrapolation, labelled.
    // ==================================================================
    const double measuredFillMs = beatEitherA.wallMs;
    const double fullBagRounds = (double)fullUnionBag + 1.0; // one round per item, plus the final proof round
    const double extrapolatedMs = roundStats.median * fullBagRounds;
    printf("[TEST] combo-logic-measure: === THE #582 FLOOR ===\n");
    printf("[TEST] combo-logic-measure: MEASURED: a %d-item bag under the proved rung took %.1fms, which is %.3fx the "
           "30000ms floor and %.3fx this host's %ums per-attempt budget\n",
           bagCount, measuredFillMs, measuredFillMs / (double)RSBS_GENBUDGET_FLOOR_MS,
           measuredFillMs / (double)Combo_GenBudget_FillBudgetMs(0), (unsigned)Combo_GenBudget_FillBudgetMs(0));
    printf("[TEST] combo-logic-measure: EXTRAPOLATED (arithmetic, NOT measured): the full union bag of %d items needs "
           "%.0f rounds; at the measured median of %.1fms per round that is %.0fms = %.1fs = %.2fx the 30000ms floor "
           "and %.2fx this host's %ums total-creation budget\n",
           fullUnionBag, fullBagRounds, roundStats.median, extrapolatedMs, extrapolatedMs / 1000.0,
           extrapolatedMs / (double)RSBS_GENBUDGET_FLOOR_MS,
           extrapolatedMs / (double)Combo_GenBudget_TotalBudgetMs(), (unsigned)Combo_GenBudget_TotalBudgetMs());
    printf("[TEST] combo-logic-measure: the extrapolation is LINEAR in the bag and that is its weakness: a round's "
           "cost is dominated by two full closures whose size does not depend on the assumed-set size, so linear is "
           "the honest first order — but the assume loop itself is O(bag) per round, which makes the true curve "
           "super-linear. It is an underestimate, not an overestimate.\n");

    // THE ATTEMPT ARITHMETIC, which is the number #582 is actually about. The
    // isolated round above is measured with EMPTY placement tables, so it pays
    // neither the crossing exchange nor the post-restore placement re-apply — and
    // the re-apply is one `place` per placement per round for a restoring side,
    // which MM is. The fill's own wall/rounds is therefore the honest per-round
    // figure.
    //
    // WHICH BUDGET THE RETRY PRODUCT IS COMPARED AGAINST, because this line had it
    // wrong by a factor of two and by the wrong mechanism. RSBS_COMBO_LOGIC_FILL_RETRIES
    // is `maxAttempts`, and combo_logic.h:766 defines it as "Batch roll-backs within
    // this seed... Re-rolling the SEED is the attempt ladder's job, one level up". So
    // all ten roll-backs happen INSIDE ONE Combo_Logic_RunFill call, which is inside
    // ONE ladder attempt — and a ladder attempt is bounded by
    // Combo_GenBudget_FillBudgetMs(attempt), not by the total. The TOTAL budget
    // (gen_budget.h:49-56) is "2 x the per-attempt budget, checked BETWEEN attempts...
    // so it can never truncate an attempt that is about to succeed": it is
    // structurally incapable of bounding a single RunFill, however long that RunFill
    // grinds. The comparison below is therefore retries x per-attempt-cost against the
    // PER-ATTEMPT budget. The total is printed too, but beside a LADDER-attempt count
    // rather than beside the roll-back count, which is the only place it means
    // anything.
    const double fillPerRoundMs = (beatEitherA.res.rounds > 0)
                                      ? (beatEitherA.wallMs / (double)beatEitherA.res.rounds)
                                      : roundStats.median;
    const double fullBagOneAttemptMs = fillPerRoundMs * fullBagRounds;
    printf("[TEST] combo-logic-measure: PER-ROUND INSIDE THE FILL: %.1fms (%.1fms wall / %d rounds) against %.1fms for "
           "an isolated round. The gap is work the isolated round cannot pay because its tables are EMPTY — the "
           "post-restore placement re-apply (one `place` per placement per round, for a restoring side, which MM is) "
           "and the crossing exchange. Which of the two dominates is NOT measured here: the row counts exchanges only "
           "for the isolated rounds, where the answer is trivially zero.\n",
           fillPerRoundMs, beatEitherA.wallMs, beatEitherA.res.rounds, roundStats.median);
    const double fullBagAllRollbacksMs = fullBagOneAttemptMs * (double)RSBS_COMBO_LOGIC_FILL_RETRIES;
    printf("[TEST] combo-logic-measure: EXTRAPOLATED, ONE BATCH (arithmetic): the full union bag over %.0f rounds at "
           "%.1fms = %.0fms = %.1fs, i.e. %.2fx the 30000ms floor and %.2fx this host's %ums PER-ATTEMPT budget\n",
           fullBagRounds, fillPerRoundMs, fullBagOneAttemptMs, fullBagOneAttemptMs / 1000.0,
           fullBagOneAttemptMs / (double)RSBS_GENBUDGET_FLOOR_MS,
           fullBagOneAttemptMs / (double)Combo_GenBudget_FillBudgetMs(0),
           (unsigned)Combo_GenBudget_FillBudgetMs(0));
    printf("[TEST] combo-logic-measure: EXTRAPOLATED, WORST-CASE RunFill: the coordinator's default of %d BATCH "
           "ROLL-BACKS all happen inside ONE Combo_Logic_RunFill, i.e. inside ONE ladder attempt, so %.1fs is what has "
           "to fit this host's %ums PER-ATTEMPT budget: %.2fx it -> %s\n",
           (int)RSBS_COMBO_LOGIC_FILL_RETRIES, fullBagAllRollbacksMs / 1000.0,
           (unsigned)Combo_GenBudget_FillBudgetMs(0),
           fullBagAllRollbacksMs / (double)Combo_GenBudget_FillBudgetMs(0),
           (fullBagAllRollbacksMs > (double)Combo_GenBudget_FillBudgetMs(0))
               ? "OVER the per-attempt budget, so the roll-back count is a budget decision and not only a quality one"
               : "inside the per-attempt budget");
    printf("[TEST] combo-logic-measure: the %ums TOTAL-creation budget is %ux the per-attempt one and is sampled "
           "BETWEEN LADDER ATTEMPTS (gen_budget.h), so it bounds the number of SEED re-rolls and cannot truncate a "
           "RunFill: at %.1fs per batch it admits %.1f full-bag batches in total, which is fewer than the %d "
           "roll-backs one RunFill is allowed. Comparing the roll-back product against it (as this row did once) "
           "compares the wrong two numbers.\n",
           (unsigned)Combo_GenBudget_TotalBudgetMs(), (unsigned)RSBS_GENBUDGET_TOTAL_MULTIPLIER,
           fullBagOneAttemptMs / 1000.0, (double)Combo_GenBudget_TotalBudgetMs() / fullBagOneAttemptMs,
           (int)RSBS_COMBO_LOGIC_FILL_RETRIES);
    printf("[TEST] combo-logic-measure: MM harvests during the measurement=%d, MM shrink observations=%d\n",
           MM_ComboLogic_HarvestCount(), MM_ComboLogic_ShrinkObservations());

    // ==================================================================
    // S4 FIRST, THEN TEARDOWN. The order is the assertion.
    // ==================================================================
    // Every S4 check below runs BEFORE the restoring memcpy, against the inner
    // baseline taken after the profile apply. That ordering is the whole content of
    // the assertion: after the memcpy the comparison cannot fail, and it was made in
    // that order once.
    Combo_Logic_ResetPlacements();
    MM_ComboLogic_SetHostPool(nullptr, 0);
    for (size_t i = 0; i < ootBagHosts.size(); ++i) {
        CLM_ASSERT(OoT_ComboLogic_TestSetPlacedItem(ootBagHosts[i], ootRestoreItems[i]) == 1,
                   "could not restore an emptied OoT host");
    }

    CLM_ASSERT(MM_ComboLogic_HeldPlacementCount() == 0,
               "MM's engine still holds coordinator placements after the reset — a later row would see them as "
               "assigned");
    CLM_ASSERT(MM_ComboLogic_SnapshotLive() == 0,
               "MM's snapshot is still LIVE — a round left MM's save owned by the engine");
    CLM_ASSERT(MM_ComboLogic_ShrinkObservations() == 0,
               "MM's reachability shrank inside a round — ADR 0010 §2.3 says the fill may not survive that");
    CLM_ASSERT(OoT_ComboLogic_TestWorldDigest() == worldDigest0,
               "the measurement moved an OoT placement — the world is not what the generation produced");
    // THE ONE WITH TEETH: 41 rounds' worth of Snapshot/Restore brackets, MM
    // placement re-applies and OoT beginQuery/endQuery pairs, and the live save is
    // back to the byte the profile apply left it at. Nothing restored it from a copy.
    // THE RED HALF WAS OBSERVED, not argued. With MM's `Restore` memcpy
    // (ComboLogicEngineSingleExe.cpp:482) commented out — so each round's assumed
    // grants leak into the live save — this line fails and the row exits 1. On that
    // SAME binary, the form this row shipped with for four commits
    // (`memcmp(saveBefore, gSaveContext)` AFTER the outer restore) reported
    // memcmp == 0 and would have passed. Both were printed side by side in one run
    // before this assertion was restored; PR #722's body quotes the two lines.
    CLM_ASSERT(memcmp(saveAfterProfile.get(), gSaveContext, OOT_SAVE_CONTEXT_SIZE) == 0,
               "the measurement did not return the unified save buffer to the state MM's shipped profile left it in — "
               "a round or a fill leaked an assumed grant, a placement re-apply or a snapshot into the live save");
    CLM_ASSERT(Combo_Logic_PlacementCount(GAME_OOT) == 0 && Combo_Logic_PlacementCount(GAME_MM) == 0,
               "the coordinator's placement tables are not empty after the teardown");

    // THE OUTER RESTORE, stated as construction and not asserted. This puts the save
    // back to before the profile apply, for whichever row runs next in this process.
    // A memcmp after it would compare the buffer with its own source.
    memcpy(gSaveContext, saveBefore.get(), OOT_SAVE_CONTEXT_SIZE);

    printf("[TEST] PASS: the linked round and the single-bag fill were measured over both real engines; every round "
           "and fill terminated, the same seed reproduced, OoT's world digest came back equal, and the whole unified "
           "save buffer came back byte-identical to the state MM's shipped profile left it in (checked BEFORE the "
           "outer restore, which is construction rather than a claim)\n");
    return TEST_PASS;
}
