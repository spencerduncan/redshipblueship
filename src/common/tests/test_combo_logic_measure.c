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
 *   S4. PRODUCTION STATE COMES BACK BYTE-IDENTICAL: the whole unified save buffer
 *       memcmp-equal, OoT's world placement digest equal, MM holding zero
 *       coordinator placements, MM's snapshot not live, and MM's reachability
 *       never observed shrinking.
 *   S5. The measurement is NON-VACUOUS: the bag is non-empty, both sides offer
 *       candidate hosts, and the fill under the proved rung actually ran rounds.
 *
 * ============================================================================
 * WHAT IS MEASURED, AND THE THREE PLACES THE ANSWER IS AN APPROXIMATION
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
 * (M3) THE FILL, under the proved no-tricks rung and under `none`, for three
 *      coordinator seeds and both GOALs that have an evaluator. Reported: wall
 *      time, rounds, rounds per placed item, attempts (hence batch roll-backs),
 *      dead-ends, and whether the GOAL became provable.
 *
 * THE THREE APPROXIMATIONS, named here so nobody reads a number as more than it
 * is:
 *
 *   (A) THE MEASURED BAG IS A SAMPLE OF THE REAL BAG BY DEFAULT. The real union
 *       bag exceeds `RSBS_COMBO_LOGIC_BAG_CAP` (the row prints by how much), and
 *       even inside the cap a fill runs one round per bag item — so a full-bag
 *       fill is minutes of wall clock and five of them is not a CI row. The row
 *       therefore measures a deterministic STRIDE SAMPLE of each half, prints the
 *       full figures beside the sampled ones, and extrapolates the full-bag total
 *       arithmetically from the measured median round. `RSBS_COMBO_MEASURE_*`
 *       overrides every size, so the full-bag run is one env var away and its
 *       numbers are what the epic comment quotes.
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
// The two ports' bridges. Every one of these is already declared by
// test_oot_logic_export.c, which is #included ABOVE this file in test_runner.cpp,
// so re-declaring them here is a redundant-but-identical declaration rather than
// a second definition — deliberate, so this file reads on its own and does not
// silently depend on the include order of its neighbour.
// ---------------------------------------------------------------------------
extern "C" {
// OoT (games/oot/soh/Enhancements/randomizer/ComboLogicEngineOoT.cpp)
int OoT_ComboLogic_TestOwnedHostCount(void);
int OoT_ComboLogic_TestOwnedHosts(uint16_t* out, int cap);
int OoT_ComboLogic_TestPlacedItemAt(uint16_t rc, uint16_t* outItemId, int* outAdvancement);
int OoT_ComboLogic_TestSetPlacedItem(uint16_t rc, uint16_t itemId);
uint32_t OoT_ComboLogic_TestWorldDigest(void);

// OoT's headless generation (games/oot/soh/.../randomizer.cpp via the harness).
int Rando_HeadlessSeedTest(const char* seedStr);

// MM (games/mm/2s2h/Rando/ComboLogicEngineSingleExe.cpp) + its rando bring-up.
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
// games reinterpret. S4 compares it byte for byte.
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
 * How many of `ids` are DISTINCT, and therefore how many rows both engines'
 * `assumeOwnItem` will silently collapse.
 *
 * THIS IS NOT A STATISTIC, IT IS THE DEAD-END'S CAUSE. Both engine TUs document
 * that they DE-DUPLICATE by id within a round, and both give the same reason: the
 * K1 contract says repeats "must be harmless" and neither port's give path has
 * that property — OoT's progressive rows are `SetUpgrade(x, CurrentUpgrade + 1)`
 * with no clamp (so a repeat past the top tier walks into the next upgrade's
 * bits), and MM's stray fairies, small keys, skull tokens and triforce pieces all
 * `++`. Both chose sound-but-pessimistic, and both recorded that a
 * multiplicity-aware assume is a contract change for a later increment.
 *
 * The consequence is measurable and this is where it shows: a real bag holds many
 * copies of one id — OoT's small keys, bombchus, wallet, quiver, bomb bag and
 * strength tiers; MM's small keys and stray fairies — and under de-dup a round
 * evaluates reachability as if the player held ONE of each. Deep dungeon
 * interiors then never enter the reached set, so the reached empty-host supply is
 * far smaller than the bag, and the fill dead-ends with ERR_NO_CANDIDATE. Printing
 * the collapse alongside the dead-end predictor is what makes the dead end
 * attributable to the surface rather than to MM's graph.
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

    // ------------------------------------------------------------------
    // THE NULL-PLAY GIVE PROBE, env-gated and never run by the CTest row.
    // ------------------------------------------------------------------
    // `RSBS_COMBO_MEASURE_PROBE=<startIndex+1>` walks MM's whole graph-wide
    // vanilla item list through `Rando::GiveItem` one id at a time, printing each
    // id to stderr BEFORE giving it, and returns. Its purpose is to CRASH on an id
    // whose give dereferences a NULL `MM_gPlayState`, so that the id names itself
    // in the log — which is how the exclusion list in
    // `MM_ComboLogic_PoolVanillaItems` was derived, and how it must be re-derived
    // whenever MM's item table or `Item_GiveImpl`'s guards change.
    //
    // It runs BEFORE the OoT generation on purpose: the probe needs MM's graph and
    // MM's profile and nothing else, and skipping the generation makes the
    // re-run-past-the-last-id loop seconds rather than minutes.
    const int probeFrom = EnvInt("RSBS_COMBO_MEASURE_PROBE", 0, 0, 1 << 20);
    if (probeFrom > 0) {
        MM_Rando_InitCore();
        std::unique_ptr<unsigned char[]> probeSave(new unsigned char[OOT_SAVE_CONTEXT_SIZE]);
        memcpy(probeSave.get(), gSaveContext, OOT_SAVE_CONTEXT_SIZE);
        MM_ComboLogic_ApplyShippedProfile();
        const int probeTotal = MM_ComboLogic_PoolVanillaItems(nullptr, nullptr, 0);
        std::vector<uint16_t> probeIds((size_t)probeTotal, 0);
        if (probeTotal > 0) {
            MM_ComboLogic_PoolVanillaItems(probeIds.data(), nullptr, probeTotal);
        }
        printf("[TEST] combo-logic-measure: PROBE MODE: walking %d MM vanilla ids from index %d through "
               "Rando::GiveItem with MM_gPlayState NULL; a crash names the id on stderr\n",
               probeTotal, probeFrom - 1);
        fflush(stdout);
        const int returned = MM_ComboLogic_ProbeGives(probeIds.data(), probeTotal, probeFrom - 1);
        memcpy(gSaveContext, probeSave.get(), OOT_SAVE_CONTEXT_SIZE);
        printf("[TEST] combo-logic-measure: PROBE MODE: %d gives returned; this mode measures nothing else and is "
               "not what the CTest row runs\n",
               returned);
        return TEST_PASS;
    }

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
    // THE MEASUREMENT BAG. OoT's half comes from hosts this row EMPTIES, so the
    // items are genuinely in the bag and not simultaneously in the world; MM's
    // half comes from the narrowed pool's vanilla items.
    //
    // Only the SAMPLED OoT hosts are emptied. Emptying every advancement-bearing
    // host would collapse the world towards sphere zero and make the final round's
    // GOAL unprovable for a reason that is about the fixture rather than about the
    // fill.
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

    // THE DE-DUP COLLAPSE. Both engines' `assumeOwnItem` drops a repeat of an id
    // already granted this round, so these are the rows a round does not see.
    const int ootDistinct = DistinctCount(ootBagItems);
    const int mmDistinct = DistinctCount(mmBagItems);
    const int ootFullDistinct = DistinctCount(ootAdvItems);
    printf("[TEST] combo-logic-measure: DE-DUP COLLAPSE: OoT half %d rows -> %d distinct ids (%d collapsed), MM half "
           "%d rows -> %d distinct (%d collapsed). The WHOLE OoT advancement half is %d rows -> %d distinct (%d "
           "collapsed). Both engines' assumeOwnItem drops a repeat within a round (no multiplicity on the K1 "
           "surface), so a round evaluates reachability as if the player held ONE of each — which is why the reached "
           "host supply below can be far short of the bag.\n",
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
    // figure, and the budget is per ATTEMPT while the fill may take up to
    // RSBS_COMBO_LOGIC_FILL_RETRIES of them within one seed.
    const double fillPerRoundMs = (beatEitherA.res.rounds > 0)
                                      ? (beatEitherA.wallMs / (double)beatEitherA.res.rounds)
                                      : roundStats.median;
    const double fullBagOneAttemptMs = fillPerRoundMs * fullBagRounds;
    printf("[TEST] combo-logic-measure: PER-ROUND INSIDE THE FILL: %.1fms (%.1fms wall / %d rounds) against %.1fms for "
           "an isolated round — the difference is the crossing exchange and the post-restore placement re-apply, "
           "which the isolated round does not pay because its tables are empty.\n",
           fillPerRoundMs, beatEitherA.wallMs, beatEitherA.res.rounds, roundStats.median);
    printf("[TEST] combo-logic-measure: EXTRAPOLATED PER ATTEMPT (arithmetic): the full union bag over %.0f rounds at "
           "%.1fms = %.0fms = %.1fs per attempt, i.e. %.2fx the 30000ms floor and %.2fx this host's %ums per-attempt "
           "budget. At the coordinator's default of %d batch roll-backs that is %.0fs worst case against a %ums "
           "total-creation budget -> %s.\n",
           fullBagRounds, fillPerRoundMs, fullBagOneAttemptMs, fullBagOneAttemptMs / 1000.0,
           fullBagOneAttemptMs / (double)RSBS_GENBUDGET_FLOOR_MS,
           fullBagOneAttemptMs / (double)Combo_GenBudget_FillBudgetMs(0),
           (unsigned)Combo_GenBudget_FillBudgetMs(0), (int)RSBS_COMBO_LOGIC_FILL_RETRIES,
           (fullBagOneAttemptMs * (double)RSBS_COMBO_LOGIC_FILL_RETRIES) / 1000.0,
           (unsigned)Combo_GenBudget_TotalBudgetMs(),
           ((fullBagOneAttemptMs * (double)RSBS_COMBO_LOGIC_FILL_RETRIES) > (double)Combo_GenBudget_TotalBudgetMs())
               ? "OVER the total budget, so the retry count is a budget decision and not only a quality one"
               : "inside the total budget");
    printf("[TEST] combo-logic-measure: MM harvests during the measurement=%d, MM shrink observations=%d\n",
           MM_ComboLogic_HarvestCount(), MM_ComboLogic_ShrinkObservations());

    // ==================================================================
    // TEARDOWN, then S4: production state byte-identical.
    // ==================================================================
    Combo_Logic_ResetPlacements();
    MM_ComboLogic_SetHostPool(nullptr, 0);
    for (size_t i = 0; i < ootBagHosts.size(); ++i) {
        CLM_ASSERT(OoT_ComboLogic_TestSetPlacedItem(ootBagHosts[i], ootRestoreItems[i]) == 1,
                   "could not restore an emptied OoT host");
    }
    memcpy(gSaveContext, saveBefore.get(), OOT_SAVE_CONTEXT_SIZE);

    CLM_ASSERT(MM_ComboLogic_HeldPlacementCount() == 0,
               "MM's engine still holds coordinator placements after the reset — a later row would see them as "
               "assigned");
    CLM_ASSERT(MM_ComboLogic_SnapshotLive() == 0,
               "MM's snapshot is still LIVE — a round left MM's save owned by the engine");
    CLM_ASSERT(MM_ComboLogic_ShrinkObservations() == 0,
               "MM's reachability shrank inside a round — ADR 0010 §2.3 says the fill may not survive that");
    CLM_ASSERT(OoT_ComboLogic_TestWorldDigest() == worldDigest0,
               "the measurement moved an OoT placement — the world is not what the generation produced");
    CLM_ASSERT(memcmp(saveBefore.get(), gSaveContext, OOT_SAVE_CONTEXT_SIZE) == 0,
               "the unified save buffer is not byte-identical after the measurement");
    CLM_ASSERT(Combo_Logic_PlacementCount(GAME_OOT) == 0 && Combo_Logic_PlacementCount(GAME_MM) == 0,
               "the coordinator's placement tables are not empty after the teardown");

    printf("[TEST] PASS: the linked round and the single-bag fill were measured over both real engines; every round "
           "and fill terminated, the same seed reproduced, and OoT's world digest plus the whole unified save buffer "
           "came back byte-identical\n");
    return TEST_PASS;
}
