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
 * (M1) THE COMPOSED BAG (lane K9), against the coordinator's own caps. Audit §4.6
 *      says the union bag is "the last general pass" — OoT's
 *      `remainingAdvancementItems` (`fill.cpp:1405-1407`) plus MM's whole shuffled
 *      pool — and since lane K9 the bag is COMPOSED from both games' real pool
 *      exports by Combo_Logic_ComposeBag under THE BAG COMPOSITION RULE
 *      (combo_logic.h): progression copies REQUIRED, plentiful copies SURPLUS,
 *      restricted-pass families CONFINED to their own game, filler and traps
 *      counted and never admitted.
 *
 *      OoT's pool is read back from the GENERATED WORLD
 *      (`OoT_ComboLogic_ExportPool` source 1): every owned host's fill item,
 *      skipping fixed placements (`IsHidden()`), so the restricted passes'
 *      families are told apart by the frozen confinement word rather than lumped
 *      in as the pre-K9 "every advancement-bearing host" superset did.
 *
 *      MM's pool is `GeneratePools`' REAL pool under the resolved profile
 *      (`MM_ComboLogic_TestGeneratePool`, over a heap copy of the save's rando
 *      info), and MM's host pool is GeneratePools' own check pool.
 *
 *      THE PROFILE is RSBS_COMBO_MEASURE_PROFILE: "shipped" (the CI default),
 *      "plentiful" or "maximal" — see ClmProfileApply.
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
 * (M2b) ONE ROUND WITH MM'S WHOLE VANILLA POOL ASSUMED (added with the
 *      multiplicity ruling, combo_logic.h ABI 3): whether MM's half is provable
 *      at all from the pool the bag's MM half is sampled from. It is what
 *      localises a `beat-both` failure to the SAMPLE (approximation A) rather
 *      than to the fill: on 2026-09-26 it read goalMM=1 while every capped
 *      measurement bag read goalMM=0 before a single item was placed.
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
 *   (A) THE MEASURED BAG IS A SAMPLE OF THE COMPOSED BAG BY DEFAULT. A fill runs
 *       one round per bag item, so eight full-bag fills are minutes of wall clock
 *       and not a CI row. The row
 *       therefore measures a deterministic STRIDE SAMPLE of each half, prints the
 *       full figures beside the sampled ones, and extrapolates the full bag
 *       arithmetically — twice, once from the isolated median round and once from
 *       the in-fill per-round figure, because the two differ by a factor of two
 *       and only the second is what a budget should be compared against.
 *       `RSBS_COMBO_MEASURE_*` overrides every size, so a larger run is one env
 *       var away; the epic comment quotes the 32-item and 512-item runs.
 *
 *   (B) MM'S HOST POOL IS GeneratePools' CHECK POOL (lane K9), handed to MM's
 *       engine through `MM_ComboLogic_SetHostPool` — the increment-4 seam doing the
 *       job it was built for. `RSBS_COMBO_MEASURE_MM_HOSTS` narrows it further by
 *       stride sample for a smaller, faster configuration. The row prints both
 *       caps against the measured figures either way.
 *
 *   (C) ONE HOST, THREE PROFILES, TRICKS OFF. The numbers are this workstation's,
 *       under the profile RSBS_COMBO_MEASURE_PROFILE names, with MM never booted
 *       into play. Other hosts and tricks-on are NOT measured; the row prints the
 *       #582 host-scale percent so a reader can at least locate this machine
 *       relative to the reference.
 *
 *   (D) OoT'S FREE HOSTS ARE ONLY ITS BAG ROWS' HOSTS. The row empties the sampled
 *       OoT bag rows' hosts, so those items are in the bag and not in the world;
 *       every other OoT host keeps what the per-game fill put there — including the
 *       filler hosts a production creation would hand the coordinator free. So the
 *       OoT host supply is SMALLER than production's, which is pessimistic for a
 *       convergence question and makes surplus DROPS appear that production would
 *       not see (the plentiful profile drops some for exactly this reason). MM empties
 *       nothing and needs to: its engine harvests only coordinator placements, never
 *       a host's vanilla contents — which is also why a check OUTSIDE MM's check pool
 *       gives the round nothing (M2d measures what that costs MM's goal).
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
#include "../shared_items.h"
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

// Lane K9's pool exports (first declarations): OoT's pool rows and confinement
// word (ComboLogicEngineOoT.cpp), MM's real GeneratePools pool with its plentiful
// marks and check pool, and the two #733 bridges (ComboLogicEngineSingleExe.cpp).
int OoT_ComboLogic_ExportPool(int source, uint16_t* outItems, uint16_t* outHosts, uint16_t* outFlags, int cap);
uint32_t OoT_ComboLogic_ConfinementArmed(void);
int OoT_ComboLogic_PlentifulAddedCount(void);
int MM_ComboLogic_TestGeneratePool(uint16_t* outItems, uint16_t* outFlags, int itemCap, uint16_t* outChecks,
                                   int checkCap, int* outCheckTotal);
int MM_ComboLogic_TestHeartGatedChecks(uint16_t* out, int cap);
int MM_ComboLogic_TestHealthCapacity(void);
// The review-pass bridges: B5's two MM fixture ids and its give-into-save
// (ComboLogicEngineSingleExe.cpp, first declarations), and OoT's counter-row id
// bridge, whose kind 1 is the gold skulltula token (a duplicate of
// test_combo_logic_multiplicity.c's declaration).
int MM_ComboLogic_TestMaxHpFixtureIds(uint16_t* out, int cap);
int MM_ComboLogic_TestGiveIntoSave(const uint16_t* ids, int count);
int OoT_ComboLogic_TestCounterItemId(int kind);

// libultraship's C CVar bridge, for the plentiful profile's MM options.
void CVarSetInteger(const char* name, int32_t value);
void CVarClear(const char* name);
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
// THE PROFILES and THE COMPOSED BAG (lane K9) — shared by combo-logic-measure and
// combo-logic-bag-composition
// ============================================================================
//
// THREE PROFILES, chosen by name so a row can print what it ran:
//   "shipped"   — both games' shipped defaults (OoT's base ice traps are on by
//                 default; MM's traps and both plentiful settings are off).
//   "plentiful" — OoT RSK_ITEM_POOL = Plentiful plus five additional ice traps;
//                 MM RO_PLENTIFUL_ITEMS and RO_SHUFFLE_TRAPS on (RO_TRAP_AMOUNT
//                 stays at its default). The profile that exercises the SURPLUS
//                 rule and both games' TRAP rows over the real engines.
//   "armed"     — "plentiful" plus a SUBSET of OoT's confinement families at
//                 their ANYWHERE value (small keys, songs, Ganon's boss key) and
//                 every token shuffled. The CTest-run lock on
//                 OoT_ComboLogic_ConfinementArmed's exact word and on the
//                 tokensanity plentiful record (combo-logic-bag-composition B6);
//                 a subset, so a setting mapped to the wrong bit shows as a set
//                 bit that should be clear.
//   "maximal"   — "plentiful" plus OoT's key, boss-key, Ganon's-key, gerudo-key,
//                 map/compass, song and reward families at their ANYWHERE value
//                 (so they join the general pass instead of being confined) and
//                 every token shuffled; every MM LOCATION category shuffled
//                 (remains, tokens, owls, frogs, cows, pots, crates, barrels,
//                 grass, freestanding, snowballs, enemy drops, shops, Tingle
//                 shops). The largest bag MEASURED, which the bag cap (#727) is
//                 sized against — NOT every setting that grows the bag: MM's soul,
//                 ocarina-button, swim, clock and triforce families and OoT's
//                 boss/bean souls, ocarina buttons and triforce pieces are left
//                 out (combo_logic.h, RSBS_COMBO_LOGIC_BAG_CAP, says why and
//                 estimates them). Measurement only, no CTest row.
// OoT's half is applied through RSBS_DIAG_CVARS, the harness's own non-default
// settings door (Rando_HeadlessSeedTest applies it after creating the options);
// MM's through its authoring CVars, which MM_ComboLogic_ApplyShippedProfile
// resolves into the save. Both are undone by ClmProfileRestore.

bool ClmProfileIsMaximal(const char* profile) {
    return profile != nullptr && strcmp(profile, "maximal") == 0;
}

bool ClmProfileIsArmed(const char* profile) {
    return profile != nullptr && strcmp(profile, "armed") == 0;
}

/** True for every profile that turns plentiful on. */
bool ClmProfileIsPlentiful(const char* profile) {
    return profile != nullptr &&
           (strcmp(profile, "plentiful") == 0 || ClmProfileIsMaximal(profile) || ClmProfileIsArmed(profile));
}

/** The canonical profile name for an environment value (NULL/unknown -> shipped). */
const char* ClmProfileName(const char* env) {
    if (ClmProfileIsMaximal(env)) {
        return "maximal";
    }
    if (ClmProfileIsArmed(env)) {
        return "armed";
    }
    return ClmProfileIsPlentiful(env) ? "plentiful" : "shipped";
}

/** The "armed" profile's OoT confinement word: exactly the four families it sets
 *  to their ANYWHERE value (small keys, songs, Ganon's boss key) or shuffles at
 *  all (tokens). Every other confinement bit must stay CLEAR. */
constexpr uint32_t kClmArmedProfileOoTWord = RSBS_FILL_ARM_SMALL_KEYS_ROAM | RSBS_FILL_ARM_SONGS_ROAM |
                                             RSBS_FILL_ARM_TOKENS_ROAM | RSBS_FILL_ARM_GANON_BOSS_KEY_ROAM;

void ClmSetEnv(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value != nullptr ? value : "");
#else
    if (value != nullptr && value[0] != '\0') {
        setenv(name, value, 1);
    } else {
        unsetenv(name);
    }
#endif
}

const char* const kClmMmPlentifulCvars[] = { "gRando.Options.RO_PLENTIFUL_ITEMS", "gRando.Options.RO_SHUFFLE_TRAPS" };
const char* const kClmMmMaximalCvars[] = {
    "gRando.Options.RO_SHUFFLE_BOSS_REMAINS",   "gRando.Options.RO_SHUFFLE_GOLD_SKULLTULAS",
    "gRando.Options.RO_SHUFFLE_OWL_STATUES",    "gRando.Options.RO_SHUFFLE_FROGS",
    "gRando.Options.RO_SHUFFLE_COWS",           "gRando.Options.RO_SHUFFLE_POT_DROPS",
    "gRando.Options.RO_SHUFFLE_CRATE_DROPS",    "gRando.Options.RO_SHUFFLE_BARREL_DROPS",
    "gRando.Options.RO_SHUFFLE_GRASS_DROPS",    "gRando.Options.RO_SHUFFLE_FREESTANDING_ITEMS",
    "gRando.Options.RO_SHUFFLE_SNOWBALL_DROPS", "gRando.Options.RO_SHUFFLE_ENEMY_DROPS",
    "gRando.Options.RO_SHUFFLE_SHOPS",          "gRando.Options.RO_SHUFFLE_TINGLE_SHOPS",
};
// OoT's "maximal" CVars: plentiful + five extra ice traps, every confinement
// family at its ANYWHERE index (RandoOptionDungeonItemLocation 5, songs 3, rewards
// 4, gerudo keys 3, Ganon's boss key 5) and all tokens shuffled (3).
const char* const kClmOoTMaximalCvars[] = {
    "gRandoSettings.ItemPool",          "gRandoSettings.AdditionalIceTraps", "gRandoSettings.Keysanity",
    "gRandoSettings.BossKeysanity",     "gRandoSettings.StartingMapsCompasses", "gRandoSettings.ShuffleSongs",
    "gRandoSettings.ShuffleTokens",     "gRandoSettings.ShuffleDungeonReward", "gRandoSettings.GerudoKeys",
    "gRandoSettings.ShuffleGanonBossKey",
};

/** Apply `profile` before the OoT generation and MM's profile resolution. */
void ClmProfileApply(const char* profile) {
    if (!ClmProfileIsPlentiful(profile)) {
        return;
    }
    if (ClmProfileIsArmed(profile)) {
        // Keysanity / songs / Ganon's boss key at ANYWHERE (5 / 3 / 5), every
        // token shuffled (3): the indices the maximal profile's armed word
        // 0x033F0000 already showed map to those values.
        ClmSetEnv("RSBS_DIAG_CVARS",
                  "gRandoSettings.ItemPool=0,gRandoSettings.AdditionalIceTraps=5,gRandoSettings.Keysanity=5,"
                  "gRandoSettings.ShuffleSongs=3,gRandoSettings.ShuffleTokens=3,gRandoSettings.ShuffleGanonBossKey=5");
    } else if (ClmProfileIsMaximal(profile)) {
        ClmSetEnv("RSBS_DIAG_CVARS",
                  "gRandoSettings.ItemPool=0,gRandoSettings.AdditionalIceTraps=5,gRandoSettings.Keysanity=5,"
                  "gRandoSettings.BossKeysanity=5,gRandoSettings.StartingMapsCompasses=5,gRandoSettings.ShuffleSongs=3,"
                  "gRandoSettings.ShuffleTokens=3,gRandoSettings.ShuffleDungeonReward=4,gRandoSettings.GerudoKeys=3,"
                  "gRandoSettings.ShuffleGanonBossKey=5");
        for (const char* cvar : kClmMmMaximalCvars) {
            CVarSetInteger(cvar, 1);
        }
    } else {
        ClmSetEnv("RSBS_DIAG_CVARS", "gRandoSettings.ItemPool=0,gRandoSettings.AdditionalIceTraps=5");
    }
    for (const char* cvar : kClmMmPlentifulCvars) {
        CVarSetInteger(cvar, 1);
    }
}

void ClmProfileRestore(const char* profile) {
    if (!ClmProfileIsPlentiful(profile)) {
        return;
    }
    ClmSetEnv("RSBS_DIAG_CVARS", nullptr);
    for (const char* cvar : kClmOoTMaximalCvars) {
        CVarClear(cvar);
    }
    for (const char* cvar : kClmMmPlentifulCvars) {
        CVarClear(cvar);
    }
    for (const char* cvar : kClmMmMaximalCvars) {
        CVarClear(cvar);
    }
}

/** Both games' real pools, composed by Combo_Logic_ComposeBag. */
struct ClmComposed {
    std::vector<ComboLogicPoolRow> rows; // OoT rows first (pool order), then MM's
    std::vector<uint16_t> ootHost;       // per row: the OoT host a world-read-back row came from; 0 for MM
    int ootRows = 0;
    int mmRows = 0;
    std::vector<uint16_t> mmChecks; // MM's check pool: the hosts MM's creation would shuffle over
    uint32_t armedOoT = 0;
    uint32_t armedMM = 0;
    std::vector<ComboLogicBagItem> bag;
    std::vector<int> bagPoolIndex; // bag row -> index into `rows`
    ComboLogicComposeResult res;
    int status = RSBS_COMBO_LOGIC_ERR_BAD_REQUEST;
};

/**
 * Export both pools and compose the bag. OoT's pool is read back from the
 * GENERATED WORLD (OoT_ComboLogic_ExportPool source 1: a finished fill has drained
 * `itemPool`); MM's is GeneratePools' real pool under the resolved profile. Call
 * after the OoT generation, MM_Rando_InitCore and MM_ComboLogic_ApplyShippedProfile.
 * Consumes MM's Ship_Random (GeneratePools' prices and plentiful half) — see
 * MM_ComboLogic_TestGeneratePool.
 */
bool ClmCompose(ClmComposed& c) {
    const int ootTotal = OoT_ComboLogic_ExportPool(1, nullptr, nullptr, nullptr, 0);
    if (ootTotal <= 0) {
        printf("[TEST] compose: OoT's export returned nothing (%d)\n", ootTotal);
        return false;
    }
    std::vector<uint16_t> ootItems((size_t)ootTotal), ootHosts((size_t)ootTotal), ootFlags((size_t)ootTotal);
    OoT_ComboLogic_ExportPool(1, ootItems.data(), ootHosts.data(), ootFlags.data(), ootTotal);
    // MM's pool in ONE call, into buffers sized up front. GeneratePools consumes
    // Ship_Random (and under RO_PLENTIFUL_ITEMS draws WHICH lesser rows to
    // duplicate), so a count-then-fill pair would be two DIFFERENT pools — the
    // first "maximal" measurement read its count from one and its rows from
    // another and showed two phantom RI_UNKNOWN rows where the pools differed.
    const int kMmItemCap = 8192;
    std::vector<uint16_t> mmItems((size_t)kMmItemCap), mmFlags((size_t)kMmItemCap);
    c.mmChecks.assign((size_t)RSBS_COMBO_LOGIC_HOST_CAP, 0);
    int mmCheckTotal = 0;
    const int mmTotal = MM_ComboLogic_TestGeneratePool(mmItems.data(), mmFlags.data(), kMmItemCap, c.mmChecks.data(),
                                                       RSBS_COMBO_LOGIC_HOST_CAP, &mmCheckTotal);
    if (mmTotal <= 0 || mmTotal > kMmItemCap || mmCheckTotal <= 0 || mmCheckTotal > RSBS_COMBO_LOGIC_HOST_CAP) {
        printf("[TEST] compose: MM's export returned %d rows / %d checks (caps %d / %d)\n", mmTotal, mmCheckTotal,
               kMmItemCap, (int)RSBS_COMBO_LOGIC_HOST_CAP);
        return false;
    }
    mmItems.resize((size_t)mmTotal);
    mmFlags.resize((size_t)mmTotal);
    c.mmChecks.resize((size_t)mmCheckTotal);
    c.rows.clear();
    c.ootHost.clear();
    for (int i = 0; i < ootTotal; ++i) {
        ComboLogicPoolRow row;
        memset(&row, 0, sizeof(row));
        row.item.originGame = (uint8_t)GAME_OOT;
        row.item.id = ootItems[(size_t)i];
        row.poolFlags = ootFlags[(size_t)i];
        c.rows.push_back(row);
        c.ootHost.push_back(ootHosts[(size_t)i]);
    }
    for (int i = 0; i < mmTotal; ++i) {
        ComboLogicPoolRow row;
        memset(&row, 0, sizeof(row));
        row.item.originGame = (uint8_t)GAME_MM;
        row.item.id = mmItems[(size_t)i];
        row.poolFlags = mmFlags[(size_t)i];
        c.rows.push_back(row);
        c.ootHost.push_back(0);
    }
    c.ootRows = ootTotal;
    c.mmRows = mmTotal;
    c.armedOoT = OoT_ComboLogic_ConfinementArmed() | Combo_ItemClassArmedFromFrozen((uint8_t)GAME_OOT);
    c.armedMM = Combo_ItemClassArmedFromFrozen((uint8_t)GAME_MM);

    ComboLogicComposeRequest req;
    memset(&req, 0, sizeof(req));
    req.rows = c.rows.data();
    req.rowCount = (int)c.rows.size();
    req.armedOoT = c.armedOoT;
    req.armedMM = c.armedMM;
    c.bag.assign((size_t)RSBS_COMBO_LOGIC_BAG_CAP, ComboLogicBagItem());
    c.bagPoolIndex.assign((size_t)RSBS_COMBO_LOGIC_BAG_CAP, -1);
    c.status = Combo_Logic_ComposeBag(&req, c.bag.data(), RSBS_COMBO_LOGIC_BAG_CAP, c.bagPoolIndex.data(), &c.res);
    // The output buffers are meaningful only on OK (combo_logic.h, REFUSALS): a
    // refused compose leaves NO bag, so nothing downstream can fill a prefix.
    const int kept = (c.status != RSBS_COMBO_LOGIC_OK) ? 0 : c.res.bagCount;
    c.bag.resize((size_t)kept);
    c.bagPoolIndex.resize((size_t)kept);
    return true;
}

/** Print the composition, one line per game, in the rule table's own words. */
void ClmPrintComposition(const char* tag, const char* profile, const ClmComposed& c) {
    printf("[TEST] %s: === THE COMPOSED BAG (profile \"%s\") ===\n", tag, profile);
    printf("[TEST] %s: pool rows OoT=%d (read back from the generated world; %d copies plentiful added) MM=%d "
           "(GeneratePools), check pool MM=%d; armed OoT=0x%08X MM=0x%08X\n",
           tag, c.ootRows, OoT_ComboLogic_PlentifulAddedCount(), c.mmRows, (int)c.mmChecks.size(),
           (unsigned)c.armedOoT, (unsigned)c.armedMM);
    const GameId games[2] = { GAME_OOT, GAME_MM };
    for (const GameId g : games) {
        const ComboLogicComposeCounts& k = c.res.perGame[(int)g];
        printf("[TEST] %s: %s required=%d surplus=%d confined=%d | renewable=%d junk=%d trap=%d unclassified=%d | "
               "plentiful-marked=%d\n",
               tag, Game_ToString(g), k.rows[RSBS_COMBO_COMPOSE_REQUIRED], k.rows[RSBS_COMBO_COMPOSE_SURPLUS],
               k.rows[RSBS_COMBO_COMPOSE_CONFINED], k.rows[RSBS_COMBO_COMPOSE_RENEWABLE],
               k.rows[RSBS_COMBO_COMPOSE_JUNK], k.rows[RSBS_COMBO_COMPOSE_TRAP],
               k.rows[RSBS_COMBO_COMPOSE_UNCLASSIFIED], k.plentiful);
    }
    printf("[TEST] %s: composed bag=%d rows (status %s) against RSBS_COMBO_LOGIC_BAG_CAP=%d and "
           "RSBS_COMBO_LOGIC_MEASURED_WORST_BAG=%d; the union of both whole pools would be %d rows\n",
           tag, c.res.bagCount, Combo_Logic_StatusName(c.status), (int)RSBS_COMBO_LOGIC_BAG_CAP,
           (int)RSBS_COMBO_LOGIC_MEASURED_WORST_BAG, c.ootRows + c.mmRows);
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
    // THE PROFILE (lane K9): "shipped" by default, "plentiful" through
    // RSBS_COMBO_MEASURE_PROFILE — see ClmProfileApply. Applied before the OoT
    // generation and MM's profile resolution, undone after the teardown.
    const char* profile = ClmProfileName(getenv("RSBS_COMBO_MEASURE_PROFILE"));
    printf("[TEST] combo-logic-measure: profile \"%s\" (RSBS_COMBO_MEASURE_PROFILE=plentiful|maximal selects the "
           "others)\n",
           profile);
    ClmProfileApply(profile);

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
    // M1: THE COMPOSED BAG (lane K9), against the coordinator's caps.
    // ==================================================================
    // The bag is no longer "every OoT advancement-bearing host plus MM's whole
    // vanilla pool" (the 2489-row union #727 measured). It is Combo_Logic_ComposeBag
    // over both games' real pool exports under THE BAG COMPOSITION RULE
    // (combo_logic.h): every progression copy a REQUIRED row, every plentiful copy
    // a SURPLUS row, restricted-pass items CONFINED to their own game, filler and
    // traps counted and never admitted. OoT's pool is read back from the generated
    // world (OoT_ComboLogic_ExportPool source 1: fixed placements and the restricted
    // passes' families are told apart, so this is no longer a superset of the
    // general pass beyond Link's-pocket-style one-off passes); MM's is GeneratePools'
    // real pool under the resolved profile.
    ClmComposed composed;
    CLM_ASSERT(ClmCompose(composed), "a pool export returned nothing");
    ClmPrintComposition("combo-logic-measure", profile, composed);
    CLM_ASSERT(composed.status == RSBS_COMBO_LOGIC_OK, "the composed bag was refused (see the counts above)");
    CLM_ASSERT(composed.res.perGame[GAME_MM].rows[RSBS_COMBO_COMPOSE_CONFINED] == 0,
               "an MM progression row is CONFINED: MM has no restricted pass to place it, so the pool and the frozen "
               "profile disagree");
    const int fullUnionBag = composed.res.bagCount;

    // MM's HOST POOL is GeneratePools' own check pool — the hosts MM's creation
    // shuffles over — through MM_ComboLogic_SetHostPool (the increment-4 seam), and
    // RSBS_COMBO_MEASURE_MM_HOSTS can narrow it further. Restored in the teardown.
    const int mmGraphHosts = mm->allEmptyHosts(mm->self, nullptr, 0);
    std::vector<uint16_t> mmHostPool = composed.mmChecks;
    if (mmHostTarget > 0) {
        mmHostPool = StrideSample(mmHostPool, mmHostTarget);
    }
    MM_ComboLogic_SetHostPool(mmHostPool.data(), (int)mmHostPool.size());
    const int mmPooledHosts = mm->allEmptyHosts(mm->self, nullptr, 0);
    const int ootOwned = OoT_ComboLogic_TestOwnedHostCount();
    printf("[TEST] combo-logic-measure: hosts: MM graph=%d, MM check pool=%d, MM offered after SetHostPool=%d%s; OoT "
           "owned=%d; RSBS_COMBO_LOGIC_HOST_CAP=%d, RSBS_COMBO_LOGIC_PLACEMENT_CAP=%d\n",
           mmGraphHosts, (int)composed.mmChecks.size(), mmPooledHosts,
           (mmHostTarget > 0) ? " (narrowed by RSBS_COMBO_MEASURE_MM_HOSTS)" : "", ootOwned,
           (int)RSBS_COMBO_LOGIC_HOST_CAP, (int)RSBS_COMBO_LOGIC_PLACEMENT_CAP);
    CLM_ASSERT(mmPooledHosts > 0, "the MM host pool is empty");
    CLM_ASSERT(mmPooledHosts <= RSBS_COMBO_LOGIC_HOST_CAP,
               "MM offers more hosts in one enumeration than RSBS_COMBO_LOGIC_HOST_CAP allows, so every fill below "
               "would refuse with ERR_CAPACITY on its first bag item");

    // ------------------------------------------------------------------
    // THE MEASUREMENT BAG: a deterministic stride sample of each composed half
    // (approximation A; RSBS_COMBO_MEASURE_OOT_BAG / _MM_BAG open it to the whole
    // composed bag). An OoT row in the sample has its host EMPTIED, so the item is
    // in the bag and not also in the world; an OoT bag row left out of the sample
    // stays natively placed and OoT's own search harvests it. MM rows empty
    // nothing: MM's engine harvests only coordinator placements, never a host's
    // vanilla contents, so an MM bag row is in the world only once it is placed.
    // ------------------------------------------------------------------
    std::vector<uint16_t> ootComposedIdx;
    std::vector<uint16_t> mmComposedIdx;
    std::vector<uint16_t> ootAdvItems; // every OoT row of the composed bag (the multiplicity baseline)
    for (int i = 0; i < (int)composed.bag.size(); ++i) {
        if (composed.bag[(size_t)i].item.originGame == (uint8_t)GAME_OOT) {
            ootComposedIdx.push_back((uint16_t)i);
            ootAdvItems.push_back(composed.bag[(size_t)i].item.id);
        } else {
            mmComposedIdx.push_back((uint16_t)i);
        }
    }
    const int ootFullBagHalf = (int)ootComposedIdx.size();
    CLM_ASSERT(ootFullBagHalf > 0, "the composed bag holds no OoT row");
    CLM_ASSERT(!mmComposedIdx.empty(), "the composed bag holds no MM row");
    const std::vector<uint16_t> ootSel = StrideSample(ootComposedIdx, ootBagTarget);
    const std::vector<uint16_t> mmSel = StrideSample(mmComposedIdx, mmBagTarget);

    std::vector<ComboLogicBagItem> bag;
    std::vector<uint16_t> ootBagHosts;
    std::vector<uint16_t> ootRestoreItems;
    std::vector<uint16_t> ootBagItems;
    std::vector<uint16_t> mmBagItems;
    int surplusInBag = 0;
    for (const uint16_t idx : ootSel) {
        const ComboLogicBagItem& b = composed.bag[idx];
        const uint16_t host = composed.ootHost[(size_t)composed.bagPoolIndex[idx]];
        uint16_t item = 0;
        int advancement = 0;
        CLM_ASSERT(OoT_ComboLogic_TestPlacedItemAt(host, &item, &advancement) == 1 && item == b.item.id,
                   "an OoT bag row's host no longer holds the row's item");
        ootBagHosts.push_back(host);
        ootRestoreItems.push_back(item);
        CLM_ASSERT(OoT_ComboLogic_TestSetPlacedItem(host, 0) == 1, "could not empty an OoT host to build the bag");
        bag.push_back(b);
        ootBagItems.push_back(b.item.id);
        surplusInBag += (b.bagFlags & RSBS_COMBO_BAG_SURPLUS) ? 1 : 0;
    }
    for (const uint16_t idx : mmSel) {
        bag.push_back(composed.bag[idx]);
        mmBagItems.push_back(composed.bag[idx].item.id);
        surplusInBag += (composed.bag[idx].bagFlags & RSBS_COMBO_BAG_SURPLUS) ? 1 : 0;
    }
    const int bagCount = (int)bag.size();
    CLM_ASSERT(bagCount > 0, "the measurement bag is empty");
    CLM_ASSERT(bagCount <= RSBS_COMBO_LOGIC_BAG_CAP, "the measurement bag exceeds the coordinator's bag cap");
    printf("[TEST] combo-logic-measure: measurement bag=%d (OoT %d of %d, MM %d of %d; %d SURPLUS rows) — a "
           "deterministic stride sample of the composed bag; see approximation (A)\n",
           bagCount, (int)ootBagItems.size(), ootFullBagHalf, (int)mmBagItems.size(), (int)mmComposedIdx.size(),
           surplusInBag);

    // THE REPEATS. Under ABI 2 both engines dropped these rows within a round;
    // under ABI 3 every one is a counted copy (see DistinctCount).
    const int ootDistinct = DistinctCount(ootBagItems);
    const int mmDistinct = DistinctCount(mmBagItems);
    const int ootFullDistinct = DistinctCount(ootAdvItems);
    printf("[TEST] combo-logic-measure: MULTIPLICITY: OoT half %d rows = %d distinct ids + %d repeated copies, MM half "
           "%d rows = %d distinct + %d repeated. The WHOLE OoT advancement half is %d rows = %d distinct + %d "
           "repeated. Under ABI 3 every repeated copy is COUNTED by the round (one assumeOwnItem per copy; both "
           "engines clamp progressives at the top tier and counters at their maxima) — under ABI 2 all of them were "
           "dropped, which is what made the proof fail on 2026-09-22.\n",
           (int)ootBagItems.size(), ootDistinct, (int)ootBagItems.size() - ootDistinct, (int)mmBagItems.size(),
           mmDistinct, (int)mmBagItems.size() - mmDistinct, ootFullBagHalf, ootFullDistinct,
           ootFullBagHalf - ootFullDistinct);
    // WHICH COMPOSITIONS CAN SAY ANYTHING ABOUT MULTIPLICITY (review of PR #728).
    // An OoT row the bag does not carry stays NATIVELY PLACED in OoT's world, and
    // OoT's own search harvests it; so with a sampled OoT half, OoT's goal is
    // carried by the native fill and the round reads goalOoT=1 before a single
    // bag item is placed. Only a bag carrying the WHOLE OoT advancement half puts
    // every repeated OoT copy through `assumeOwnItem`, which is what the ABI-2
    // de-dup broke — so only that composition is evidence about the fix.
    if ((int)ootBagItems.size() < ootFullBagHalf) {
        printf("[TEST] combo-logic-measure: NOT DIAGNOSTIC FOR MULTIPLICITY: the OoT half carries %d of %d OoT "
               "advancement rows; the other %d stay natively placed and are harvested by OoT's own search, so OoT's "
               "goal here is carried by the native fill. Set RSBS_COMBO_MEASURE_OOT_BAG>=%d for a composition that "
               "puts every OoT copy through assumeOwnItem.\n",
               (int)ootBagItems.size(), ootFullBagHalf, ootFullBagHalf - (int)ootBagItems.size(), ootFullBagHalf);
    }

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

    // ------------------------------------------------------------------
    // M2b / M2c. CAN MM'S HALF BE PROVED FROM MM'S COMPOSED PROGRESSION? (K9)
    // ------------------------------------------------------------------
    // The round above assumes the MEASUREMENT bag, whose MM half may be a stride
    // SAMPLE (approximation A). If its goalMM is 0, that alone cannot say whether
    // the fill failed or the sample simply does not carry what Majora needs. So:
    //   M2b assumes the measurement bag's OoT half plus EVERY MM REQUIRED row of
    //       the composed bag — the rows the proof is allowed to rely on;
    //   M2c adds every MM FILLER row of the pool on top (renewable and junk; never
    //       a trap, whose give leaves the save).
    // M2b=1 says MM's composed progression carries Majora. M2b=0 with M2c=1 says a
    // FILLER-class item is load-bearing for MM's goal — the #733 class, which the
    // composition must not leave open. Printed, not asserted: it measures the
    // fixture. RunRound has no bag cap.
    {
        std::vector<ComboLogicBagItem> required;
        std::vector<ComboLogicBagItem> withFiller;
        for (const uint16_t id : ootBagItems) {
            ComboLogicBagItem row;
            memset(&row, 0, sizeof(row));
            row.item.originGame = (uint8_t)GAME_OOT;
            row.item.id = id;
            required.push_back(row);
        }
        for (const ComboLogicBagItem& b : composed.bag) {
            if (b.item.originGame == (uint8_t)GAME_MM && (b.bagFlags & RSBS_COMBO_BAG_SURPLUS) == 0u) {
                ComboLogicBagItem row = b;
                row.bagFlags = 0u;
                required.push_back(row);
            }
        }
        withFiller = required;
        int fillerRows = 0;
        for (const ComboLogicPoolRow& r : composed.rows) {
            if (r.item.originGame != (uint8_t)GAME_MM) {
                continue;
            }
            const int d = Combo_Logic_ComposeDisposition(r.item, r.poolFlags, composed.armedMM);
            if (d == RSBS_COMBO_COMPOSE_RENEWABLE || d == RSBS_COMBO_COMPOSE_JUNK) {
                ComboLogicBagItem row;
                memset(&row, 0, sizeof(row));
                row.item = r.item;
                withFiller.push_back(row);
                fillerRows++;
            }
        }
        // M2d: + the VANILLA item of every MM graph check that is NOT in the check
        // pool — the fixed contents MM's own creation leaves in place (a category
        // whose shuffle is off keeps its vanilla item) and that MM's engine never
        // harvests (it grants only coordinator placements). M2c=0 with M2d=1 says
        // MM's goal needs those fixed contents: a harvest the engine does not do.
        std::vector<ComboLogicBagItem> withFixed = withFiller;
        int fixedRows = 0;
        int fixedProgression = 0;
        {
            MM_ComboLogic_SetHostPool(nullptr, 0); // the whole graph, for this one read
            const int graphTotal = MM_ComboLogic_PoolVanillaItems(nullptr, nullptr, 0);
            std::vector<uint16_t> gItems((size_t)(graphTotal > 0 ? graphTotal : 0));
            std::vector<uint16_t> gHosts((size_t)(graphTotal > 0 ? graphTotal : 0));
            if (graphTotal > 0) {
                MM_ComboLogic_PoolVanillaItems(gItems.data(), gHosts.data(), graphTotal);
            }
            MM_ComboLogic_SetHostPool(mmHostPool.data(), (int)mmHostPool.size());
            std::vector<bool> pooled(1u << 16, false);
            for (const uint16_t check : composed.mmChecks) {
                pooled[check] = true;
            }
            for (int i = 0; i < graphTotal; ++i) {
                if (pooled[gHosts[(size_t)i]]) {
                    continue;
                }
                ComboLogicBagItem row;
                memset(&row, 0, sizeof(row));
                row.item.originGame = (uint8_t)GAME_MM;
                row.item.id = gItems[(size_t)i];
                withFixed.push_back(row);
                fixedRows++;
                fixedProgression += (Combo_ItemClassOf(row.item) == RSBS_FILL_CLASS_PROGRESSION) ? 1 : 0;
            }
        }
        const std::vector<ComboLogicBagItem>* sets[3] = { &required, &withFiller, &withFixed };
        const char* names[3] = { "M2b MM REQUIRED rows", "M2c + MM filler rows", "M2d + MM fixed vanilla contents" };
        printf("[TEST] combo-logic-measure: M2d adds %d fixed vanilla contents of MM graph checks outside the check pool "
               "(%d of them PROGRESSION-class)\n",
               fixedRows, fixedProgression);
        for (int s = 0; s < 3; ++s) {
            ComboLogicRoundRequest req;
            memset(&req, 0, sizeof(req));
            req.assumed = sets[s]->data();
            req.assumedCount = (int)sets[s]->size();
            req.goal = RSBS_COMBO_GOAL_BEAT_BOTH;
            ComboLogicRoundResult res;
            const double t0 = NowMs();
            const int status = Combo_Logic_RunRound(&req, &res);
            const double ms = NowMs() - t0;
            CLM_ASSERT(status == RSBS_COMBO_LOGIC_OK, "an M2b/M2c round did not succeed");
            printf("[TEST] combo-logic-measure: %s assumed (%d rows; %d filler rows): %.1fms goalOoT=%d goalMM=%d "
                   "beat-both=%d candidatesMM=%d — against goalMM=%d for the measurement bag\n",
                   names[s], req.assumedCount, (s >= 1) ? fillerRows : 0, ms, res.goalOoT, res.goalMM,
                   res.goalExpression, res.candidatesMM, firstRound.goalMM);
        }
    }

    // ==================================================================
    // M3: THE FILL — convergence, per rung, per GOAL, per seed.
    // ==================================================================
    printf("[TEST] combo-logic-measure: === M3 THE SINGLE-BAG ASSUMED FILL ===\n");
    const uint32_t kSeedA = 0xC0FFEE01u;
    const uint32_t kSeedB = 0x5EED0002u;
    const uint32_t kSeedC = 0xA11CE003u;

    // (1) the proved no-tricks rung, default GOAL. `beat-both` is the shipped
    //     default (O11) and it is measured FIRST, on all three coordinator seeds
    //     (lane K9: one seed could not tell a provable bag from a lucky order):
    //     MM's half has to prove Majora defeated from South Clock Town, and whether
    //     it can under the measured bag is exactly the thing being measured rather
    //     than assumed.
    const FillMeasurement beatBothA =
        RunTimedFill("beatable/beat-both A", bag.data(), bagCount, RSBS_COMBO_GOAL_BEAT_BOTH,
                     RSBS_COMBO_RUNG_BEATABLE, kSeedA, fillAttempts);
    const FillMeasurement beatBothB =
        RunTimedFill("beatable/beat-both B", bag.data(), bagCount, RSBS_COMBO_GOAL_BEAT_BOTH,
                     RSBS_COMBO_RUNG_BEATABLE, kSeedB, fillAttempts);
    const FillMeasurement beatBothC =
        RunTimedFill("beatable/beat-both C", bag.data(), bagCount, RSBS_COMBO_GOAL_BEAT_BOTH,
                     RSBS_COMBO_RUNG_BEATABLE, kSeedC, fillAttempts);

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
    // THE SUMMARY THE EPIC QUOTES (lane K9): per GOAL, how many of the three seeds
    // proved, the roll-backs they needed, and the wall time against #582's floor
    // and ceiling, and rounds per placed item.
    //
    // WHICH WALL IS THE BUDGET'S, corrected on review of PR #738. The first version
    // of this summary divided each fill's wall by its attempts and compared that
    // PER-BATCH figure against the floor. But every batch roll-back happens inside
    // ONE Combo_Logic_RunFill, which is ONE ladder attempt (M4 above; combo_logic.h
    // `maxAttempts`), and the #582 per-attempt budget governs that whole call. So
    // the number compared against the floor and ceiling is the RunFill WALL. The
    // per-batch figure is still printed, labelled as what it is, because it is what
    // extrapolates: this row caps roll-backs at `fillAttempts` (default 3) to stay a
    // CI-sized run, while production allows RSBS_COMBO_LOGIC_FILL_RETRIES, and a
    // seed whose GOAL cannot be proved grinds through ALL of them before the ladder
    // sees it. That extrapolated figure is printed for every goal with an unproved
    // seed, as arithmetic, not as a measurement.
    {
        const FillMeasurement* goals[2][3] = { { &beatBothA, &beatBothB, &beatBothC },
                                               { &beatEitherA, &beatEitherB, &beatEitherC } };
        const char* goalNames[2] = { "beat-both", "beat-either" };
        for (int g = 0; g < 2; ++g) {
            int proved = 0;
            int rollbacks = 0;
            double worstRunFillMs = 0.0;
            double worstBatchMs = 0.0;
            double roundsPerPlaced = 0.0;
            for (int s = 0; s < 3; ++s) {
                const FillMeasurement& f = *goals[g][s];
                proved += (f.status == RSBS_COMBO_LOGIC_OK && f.res.goalProven) ? 1 : 0;
                rollbacks += (f.res.attempts > 0) ? (f.res.attempts - 1) : 0;
                worstRunFillMs = (f.wallMs > worstRunFillMs) ? f.wallMs : worstRunFillMs;
                const double perBatch = (f.res.attempts > 0) ? (f.wallMs / (double)f.res.attempts) : f.wallMs;
                worstBatchMs = (perBatch > worstBatchMs) ? perBatch : worstBatchMs;
                roundsPerPlaced += (f.res.placed > 0) ? ((double)f.res.rounds / (double)f.res.placed) : 0.0;
            }
            printf("[TEST] combo-logic-measure: SUMMARY %-11s profile=%s bag=%d proved %d/3 seeds, batch roll-backs "
                   "%d (cap %d per RunFill here), worst RunFill wall (= one ladder attempt) %.1fms = %.2fx the %ums "
                   "floor / %.2fx the %ums ceiling; worst per-batch %.1fms; mean rounds/placed %.2f\n",
                   goalNames[g], profile, bagCount, proved, rollbacks, fillAttempts, worstRunFillMs,
                   worstRunFillMs / (double)RSBS_GENBUDGET_FLOOR_MS, (unsigned)RSBS_GENBUDGET_FLOOR_MS,
                   worstRunFillMs / (double)RSBS_GENBUDGET_CEILING_MS, (unsigned)RSBS_GENBUDGET_CEILING_MS,
                   worstBatchMs, roundsPerPlaced / 3.0);
            if (proved < 3) {
                const double unprovableMs = worstBatchMs * (double)RSBS_COMBO_LOGIC_FILL_RETRIES;
                printf("[TEST] combo-logic-measure: SUMMARY %-11s profile=%s EXTRAPOLATED (arithmetic, NOT measured): "
                       "an unprovable seed at production's %d batch roll-backs costs %d x %.1fms = %.1fs in ONE "
                       "ladder attempt = %.2fx the %ums floor / %.2fx the %ums ceiling\n",
                       goalNames[g], profile, (int)RSBS_COMBO_LOGIC_FILL_RETRIES, (int)RSBS_COMBO_LOGIC_FILL_RETRIES,
                       worstBatchMs, unprovableMs / 1000.0, unprovableMs / (double)RSBS_GENBUDGET_FLOOR_MS,
                       (unsigned)RSBS_GENBUDGET_FLOOR_MS, unprovableMs / (double)RSBS_GENBUDGET_CEILING_MS,
                       (unsigned)RSBS_GENBUDGET_CEILING_MS);
            }
        }
    }

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
    ClmProfileRestore(profile);

    printf("[TEST] PASS: the linked round and the single-bag fill were measured over both real engines; every round "
           "and fill terminated, the same seed reproduced, OoT's world digest came back equal, and the whole unified "
           "save buffer came back byte-identical to the state MM's shipped profile left it in (checked BEFORE the "
           "outer restore, which is construction rather than a claim)\n");
    return TEST_PASS;
}

// ============================================================================
// combo-logic-bag-composition — THE BAG COMPOSITION RULE OVER BOTH REAL POOLS
// (#645 increment 3, lane K9; #731, #733). A LOCK, unlike the row above.
// ============================================================================
//
// The rule table itself is locked ROM-free over synthetic sources (the
// combo-logic-bag-model row, leg C). This row locks what only the REAL exports
// can show, on the profile named by RSBS_COMBO_PROFILE ("shipped" by default,
// "plentiful" for its second CTest row):
//
//  B1 THE RULE HOLDS OVER THE REAL POOLS. The compose succeeds; every bag row is
//     PROGRESSION under the O8 owner and armed for its origin; no pool row is
//     unclassified; every pool row is accounted for by exactly one disposition;
//     OoT CONFINES something (its restricted passes are real on both profiles)
//     and MM confines nothing (it has no restricted pass); OoT's ice traps are
//     counted TRAP rows and none is in the bag.
//  B2 SURPLUS FOLLOWS THE PROFILE. Shipped: no PLENTIFUL mark and no SURPLUS row
//     in either game. Plentiful: both exports mark copies, both games contribute
//     SURPLUS rows, MM counts its trap rows, and every marked progression copy is
//     SURPLUS (never REQUIRED).
//  B3 MM'S HEARTS ARE REQUIRED ROWS, AND THE PROOF READS THEM (#733). Every MM
//     pool row feeding the heart-quarters kind is in the bag as REQUIRED; and over
//     MM's REAL engine, a round assuming every MM REQUIRED row reaches both
//     CHECK_MAX_HP(4) checks, while the same round WITHOUT the heart rows reaches
//     neither — the red half, so the classification is load-bearing and not
//     decorative. (MM starts at three hearts; the bridge prints the capacity.)
//  B4 SURPLUS OVER THE REAL ENGINES (plentiful profiles only — the behaviour K4
//     left unverified). A small fill of 12 MM REQUIRED rows plus 8 MM SURPLUS
//     rows under `beatable` / `beat-either`, over MM's whole check pool (283
//     hosts for 20 rows, so nothing may be dropped), places every required row,
//     then places EVERY surplus row after the proof (surplusDropped == 0), and
//     places nothing that is not a bag row.
//  B5 #733 AT FILL LEVEL: A WORLD WHOSE HOSTS ARE ONLY REACHABLE THROUGH HEARTS
//     PROVES OR REFUSES CORRECTLY. The live MM save is given every MM REQUIRED
//     row EXCEPT the hearts (so it holds everything the two CHECK_MAX_HP(4)
//     checks need but health — 3 hearts), and MM's host pool is narrowed to
//     those two checks plus ONE host X that the save reaches unaided; OoT offers
//     no host at all (asserted). Three rows, three hosts, so two rows MUST land
//     behind CHECK_MAX_HP(4). WITH a heart container as one of the rows the fill
//     proves, the container lands on X (it cannot sit behind itself) and the two
//     other rows land on the heart-gated checks; WITHOUT it (a logic-neutral row
//     in its place) the heart-gated checks are never reached and the fill REFUSES
//     with ERR_NO_CANDIDATE on every roll-back. The refusal is the red half.
//  B6 THE OoT CONFINEMENT WORD AND THE TOKENSANITY RECORD ("armed" profile
//     only). OoT_ComboLogic_ConfinementArmed() returns EXACTLY the four bits the
//     profile arms (a setting mapped to the wrong bit, or the function returning
//     0, fails), and the export marks EXACTLY ten gold-skulltula-token rows
//     PLENTIFUL — item_pool.cpp's tokensanity "+10" under plentiful, recorded by
//     its guarded seam (deleting that record gives 0). The red half of both was
//     observed with a deliberately broken build (PR #738's review round).
//
// It puts back what it perturbs: the MM host pool, the coordinator tables, and
// the whole unified save buffer (compared against the post-profile baseline
// BEFORE the outer restore, the measurement row's S4 discipline).

#define CLB_ASSERT(cond, msg)                                                    \
    do {                                                                         \
        if (!(cond)) {                                                           \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__);       \
            return TEST_FAIL;                                                    \
        }                                                                        \
    } while (0)

namespace {

/** One linked round assuming `assumed`; writes whether each of `checks` was
 *  reached on MM's side, read inside the round through the engine itself. */
struct ClbRoundProbe {
    const uint16_t* checks = nullptr;
    int count = 0;
    std::vector<int> reached; // sized to `count` by ClbRoundWithProbe
};

// The probe needs `checkReached` INSIDE the round (it is only valid before
// teardown), so it wraps MM's registered vtable for one round: the wrapper's
// goalReached reads the probe checks through the real engine, then answers with
// the real engine's own goal. Nothing else changes.
ComboLogicEngine sClbWrapped;
const ComboLogicEngine* sClbReal = nullptr;
ClbRoundProbe* sClbProbe = nullptr;

int ClbProbeGoal(void* self) {
    if (sClbProbe != nullptr) {
        for (int i = 0; i < sClbProbe->count; ++i) {
            sClbProbe->reached[(size_t)i] = sClbReal->checkReached(self, sClbProbe->checks[i]);
        }
    }
    return sClbReal->goalReached(self);
}

int ClbRoundWithProbe(const std::vector<ComboLogicBagItem>& assumed, ClbRoundProbe* probe) {
    sClbReal = Combo_Logic_GetEngine(GAME_MM);
    if (sClbReal == nullptr) {
        return RSBS_COMBO_LOGIC_ERR_NO_ENGINE;
    }
    sClbWrapped = *sClbReal;
    sClbWrapped.goalReached = ClbProbeGoal;
    probe->reached.assign((size_t)(probe->count > 0 ? probe->count : 0), 0);
    sClbProbe = probe;
    Combo_Logic_RegisterEngine(GAME_MM, &sClbWrapped);
    ComboLogicRoundRequest req;
    memset(&req, 0, sizeof(req));
    req.assumed = assumed.empty() ? nullptr : assumed.data();
    req.assumedCount = (int)assumed.size();
    req.goal = RSBS_COMBO_GOAL_BEAT_EITHER;
    ComboLogicRoundResult res;
    const int status = Combo_Logic_RunRound(&req, &res);
    Combo_Logic_RegisterEngine(GAME_MM, sClbReal);
    sClbProbe = nullptr;
    return status;
}

} // namespace

TestResult ComboLogicBagComposition_Run(void) {
    const char* profile = ClmProfileName(getenv("RSBS_COMBO_PROFILE"));
    const bool plentiful = ClmProfileIsPlentiful(profile);
    printf("[TEST] combo-logic-bag-composition: THE BAG COMPOSITION RULE over both real pools, profile \"%s\" "
           "(#645 lane K9; #731, #733)\n",
           profile);

    const ComboLogicEngine* oot = Combo_Logic_GetEngine(GAME_OOT);
    const ComboLogicEngine* mm = Combo_Logic_GetEngine(GAME_MM);
    CLB_ASSERT(oot != nullptr && mm != nullptr, "both real engines are registered");

    ClmProfileApply(profile);
    CLB_ASSERT(Rando_HeadlessSeedTest("RSBSCOMBOBAGCOMP1") == 0, "headless OoT seed generation failed");
    MM_Rando_InitCore();

    std::unique_ptr<unsigned char[]> saveBefore(new unsigned char[OOT_SAVE_CONTEXT_SIZE]);
    memcpy(saveBefore.get(), gSaveContext, OOT_SAVE_CONTEXT_SIZE);
    const uint32_t worldDigest0 = OoT_ComboLogic_TestWorldDigest();
    MM_ComboLogic_ResetCounters();
    (void)MM_ComboLogic_ApplyShippedProfile();
    std::unique_ptr<unsigned char[]> saveAfterProfile(new unsigned char[OOT_SAVE_CONTEXT_SIZE]);
    memcpy(saveAfterProfile.get(), gSaveContext, OOT_SAVE_CONTEXT_SIZE);

    // ---- B1: the rule over the real pools ----------------------------------
    ClmComposed c;
    CLB_ASSERT(ClmCompose(c), "a pool export returned nothing");
    ClmPrintComposition("combo-logic-bag-composition", profile, c);
    CLB_ASSERT(c.status == RSBS_COMBO_LOGIC_OK, "B1: the composed bag is accepted");
    const ComboLogicComposeCounts& ko = c.res.perGame[GAME_OOT];
    const ComboLogicComposeCounts& km = c.res.perGame[GAME_MM];
    int sumO = 0;
    int sumM = 0;
    for (int d = 0; d < RSBS_COMBO_COMPOSE_COUNT; ++d) {
        sumO += ko.rows[d];
        sumM += km.rows[d];
    }
    CLB_ASSERT(sumO == c.ootRows && sumM == c.mmRows, "B1: every pool row has exactly one disposition");
    CLB_ASSERT(ko.rows[RSBS_COMBO_COMPOSE_UNCLASSIFIED] == 0 && km.rows[RSBS_COMBO_COMPOSE_UNCLASSIFIED] == 0,
               "B1: no real pool row is unclassified");
    CLB_ASSERT(ko.rows[RSBS_COMBO_COMPOSE_REQUIRED] > 0 && km.rows[RSBS_COMBO_COMPOSE_REQUIRED] > 0,
               "B1: both games contribute REQUIRED rows (anti-vacuity)");
    CLB_ASSERT(ClmProfileIsMaximal(profile) || ko.rows[RSBS_COMBO_COMPOSE_CONFINED] > 0,
               "B1: OoT's restricted passes confine something on this profile (own-dungeon keys, song locations, "
               "end-of-dungeon rewards) — the P14 exclusion is live, not vacuous");
    CLB_ASSERT(km.rows[RSBS_COMBO_COMPOSE_CONFINED] == 0, "B1: MM confines nothing (it has no restricted pass)");
    CLB_ASSERT(ko.rows[RSBS_COMBO_COMPOSE_TRAP] > 0, "B1: OoT's base ice traps are counted as TRAP rows");
    for (size_t i = 0; i < c.bag.size(); ++i) {
        const ComboLogicBagItem& b = c.bag[i];
        const uint32_t armed = (b.item.originGame == (uint8_t)GAME_OOT) ? c.armedOoT : c.armedMM;
        CLB_ASSERT(Combo_ItemClassOf(b.item) == RSBS_FILL_CLASS_PROGRESSION,
                   "B1: every bag row is PROGRESSION — no trap, junk or renewable row is ever admitted");
        CLB_ASSERT((Combo_ItemClassArmedBy(b.item) & ~armed) == 0u, "B1: every bag row is armed for its origin");
        const ComboLogicPoolRow& src = c.rows[(size_t)c.bagPoolIndex[i]];
        CLB_ASSERT(src.item.originGame == b.item.originGame && src.item.id == b.item.id,
                   "B1: each bag row maps back to its own pool row");
        CLB_ASSERT(((b.bagFlags & RSBS_COMBO_BAG_SURPLUS) != 0u) == ((src.poolFlags & RSBS_COMBO_POOL_PLENTIFUL) != 0u),
                   "B1: a bag row is SURPLUS exactly when its pool row is a plentiful copy");
    }

    // ---- B2: surplus follows the profile -----------------------------------
    if (!plentiful) {
        CLB_ASSERT(ko.plentiful == 0 && km.plentiful == 0 && ko.rows[RSBS_COMBO_COMPOSE_SURPLUS] == 0 &&
                       km.rows[RSBS_COMBO_COMPOSE_SURPLUS] == 0,
                   "B2: the shipped profile marks no plentiful copy and composes no SURPLUS row");
    } else {
        CLB_ASSERT(ko.plentiful > 0 && km.plentiful > 0, "B2: both exports mark plentiful copies");
        CLB_ASSERT(ko.rows[RSBS_COMBO_COMPOSE_SURPLUS] > 0 && km.rows[RSBS_COMBO_COMPOSE_SURPLUS] > 0,
                   "B2: both games contribute SURPLUS rows under plentiful");
        CLB_ASSERT(km.rows[RSBS_COMBO_COMPOSE_TRAP] > 0, "B2: MM's shuffled traps are counted as TRAP rows");
    }

    // ---- B3: MM's hearts are REQUIRED rows, and the proof reads them (#733) --
    std::vector<ComboLogicBagItem> mmRequired;
    std::vector<ComboLogicBagItem> mmRequiredNoHearts;
    std::vector<ComboLogicBagItem> mmSurplus;
    int heartRows = 0;
    for (const ComboLogicBagItem& b : c.bag) {
        if (b.item.originGame != (uint8_t)GAME_MM) {
            continue;
        }
        if ((b.bagFlags & RSBS_COMBO_BAG_SURPLUS) != 0u) {
            mmSurplus.push_back(b);
            continue;
        }
        mmRequired.push_back(b);
        if (Combo_ItemClassSharedKind(b.item) == RSBS_SHARED_RES_HEALTH_QUARTERS) {
            heartRows++;
        } else {
            mmRequiredNoHearts.push_back(b);
        }
    }
    for (const ComboLogicPoolRow& r : c.rows) {
        if (r.item.originGame == (uint8_t)GAME_MM && Combo_ItemClassSharedKind(r.item) == RSBS_SHARED_RES_HEALTH_QUARTERS) {
            const int d = Combo_Logic_ComposeDisposition(r.item, r.poolFlags, c.armedMM);
            CLB_ASSERT(d == RSBS_COMBO_COMPOSE_REQUIRED || d == RSBS_COMBO_COMPOSE_SURPLUS,
                       "B3: every MM heart-quarter pool row is in the bag (#733)");
        }
    }
    CLB_ASSERT(heartRows > 0, "B3: MM's pool carries heart rows and they are REQUIRED bag rows");

    uint16_t gated[4] = { 0, 0, 0, 0 };
    const int gatedCount = MM_ComboLogic_TestHeartGatedChecks(gated, 4);
    CLB_ASSERT(gatedCount == 2, "B3: the two CHECK_MAX_HP(4) checks are named");
    printf("[TEST] combo-logic-bag-composition: MM starts with healthCapacity=0x%X (16 per heart); %d MM heart rows "
           "of %d MM REQUIRED rows\n",
           (unsigned)MM_ComboLogic_TestHealthCapacity(), heartRows, (int)mmRequired.size());
    MM_ComboLogic_SetHostPool(c.mmChecks.data(), (int)c.mmChecks.size());
    ClbRoundProbe with;
    with.checks = gated;
    with.count = gatedCount;
    CLB_ASSERT(ClbRoundWithProbe(mmRequired, &with) == RSBS_COMBO_LOGIC_OK, "B3: the round with the hearts ran");
    ClbRoundProbe without;
    without.checks = gated;
    without.count = gatedCount;
    CLB_ASSERT(ClbRoundWithProbe(mmRequiredNoHearts, &without) == RSBS_COMBO_LOGIC_OK,
               "B3: the round without the hearts ran");
    printf("[TEST] combo-logic-bag-composition: B3 CHECK_MAX_HP(4) checks reached WITH the %d heart rows: %d %d; "
           "WITHOUT them: %d %d\n",
           heartRows, with.reached[0], with.reached[1], without.reached[0], without.reached[1]);
    CLB_ASSERT(with.reached[0] == 1 && with.reached[1] == 1,
               "B3: with every MM REQUIRED row assumed, both heart-gated checks are reached — the bag carries what "
               "CHECK_MAX_HP reads");
    CLB_ASSERT(without.reached[0] == 0 && without.reached[1] == 0,
               "B3 RED HALF: without the heart rows neither check is reached — health is load-bearing, so filing "
               "MM's hearts as filler would have let the proof rely on items placed with no logic");

    // ---- B4: surplus over the real engines (plentiful only) -----------------
    if (plentiful) {
        std::vector<uint16_t> reqIdx;
        std::vector<uint16_t> surIdx;
        for (size_t i = 0; i < mmRequired.size(); ++i) {
            reqIdx.push_back((uint16_t)i);
        }
        for (size_t i = 0; i < mmSurplus.size(); ++i) {
            surIdx.push_back((uint16_t)i);
        }
        std::vector<ComboLogicBagItem> b4Bag;
        for (const uint16_t i : StrideSample(reqIdx, 12)) {
            b4Bag.push_back(mmRequired[i]);
        }
        const int requiredRows = (int)b4Bag.size();
        for (const uint16_t i : StrideSample(surIdx, 8)) {
            b4Bag.push_back(mmSurplus[i]);
        }
        const int surplusRows = (int)b4Bag.size() - requiredRows;
        ComboLogicFillRequest req;
        memset(&req, 0, sizeof(req));
        req.bag = b4Bag.data();
        req.bagCount = (int)b4Bag.size();
        req.goal = RSBS_COMBO_GOAL_BEAT_EITHER;
        req.logicRung = RSBS_COMBO_RUNG_BEATABLE;
        req.seed = 0xB4C0FFEEu;
        req.maxAttempts = 3;
        ComboLogicFillResult res;
        const int status = Combo_Logic_RunFill(&req, &res);
        printf("[TEST] combo-logic-bag-composition: B4 fill of %d MM required + %d MM surplus rows: status=%s "
               "requiredPlaced=%d surplusPlaced=%d surplusDropped=%d goalProven=%d leftover OoT=%d MM=%d\n",
               requiredRows, surplusRows, Combo_Logic_StatusName(status), res.requiredPlaced, res.surplusPlaced,
               res.surplusDropped, res.goalProven ? 1 : 0, res.leftoverHostsOoT, res.leftoverHostsMM);
        CLB_ASSERT(surplusRows > 0, "B4: the plentiful bag has surplus rows to place");
        CLB_ASSERT(status == RSBS_COMBO_LOGIC_OK && res.goalProven, "B4: the fill proves beat-either");
        CLB_ASSERT(res.requiredPlaced == requiredRows, "B4: every REQUIRED row is placed");
        CLB_ASSERT(res.surplusPlaced + res.surplusDropped == surplusRows,
                   "B4: every SURPLUS row is placed after the proof or dropped, none lost");
        CLB_ASSERT(res.surplusDropped == 0 && res.surplusPlaced == surplusRows,
                   "B4: with MM's whole check pool for 20 rows, EVERY surplus row is placed and none is dropped");
        CLB_ASSERT(res.placed == res.requiredPlaced + res.surplusPlaced, "B4: the coordinator placed bag rows only");
        Combo_Logic_ResetPlacements();
    }

    // S4 for B3/B4, BEFORE B5 deliberately changes the live save: the rounds and
    // the fill put it back themselves.
    Combo_Logic_ResetPlacements();
    CLB_ASSERT(memcmp(saveAfterProfile.get(), gSaveContext, OOT_SAVE_CONTEXT_SIZE) == 0,
               "B3/B4's rounds and fill returned the unified save buffer to the post-profile state");

    // ---- B5: #733 at FILL level ---------------------------------------------
    {
        uint16_t fx[2] = { 0, 0 };
        CLB_ASSERT(MM_ComboLogic_TestMaxHpFixtureIds(fx, 2) == 2, "B5: the fixture ids are named");
        const uint16_t heartId = fx[0];
        const uint16_t neutralId = fx[1];
        std::vector<uint16_t> support;
        for (const ComboLogicBagItem& b : mmRequiredNoHearts) {
            support.push_back(b.item.id);
        }
        const int given = MM_ComboLogic_TestGiveIntoSave(support.data(), (int)support.size());
        const int health = MM_ComboLogic_TestHealthCapacity();
        printf("[TEST] combo-logic-bag-composition: B5 gave %d of %d non-heart MM REQUIRED rows into the save; "
               "healthCapacity=0x%X\n",
               given, (int)support.size(), (unsigned)health);
        CLB_ASSERT(given > 0, "B5: the support rows were given");
        CLB_ASSERT(health < 4 * 16, "B5 PREMISE: without the heart rows the save is below CHECK_MAX_HP(4)");
        std::unique_ptr<unsigned char[]> saveWithSupport(new unsigned char[OOT_SAVE_CONTEXT_SIZE]);
        memcpy(saveWithSupport.get(), gSaveContext, OOT_SAVE_CONTEXT_SIZE);

        // X: the first check of MM's check pool, other than the two gated ones,
        // that the support save reaches with NOTHING assumed.
        MM_ComboLogic_SetHostPool(c.mmChecks.data(), (int)c.mmChecks.size());
        ClbRoundProbe base;
        base.checks = c.mmChecks.data();
        base.count = (int)c.mmChecks.size();
        const std::vector<ComboLogicBagItem> none;
        CLB_ASSERT(ClbRoundWithProbe(none, &base) == RSBS_COMBO_LOGIC_OK, "B5: the unaided round ran");
        uint16_t hostX = 0;
        for (size_t i = 0; i < c.mmChecks.size(); ++i) {
            const uint16_t chk = c.mmChecks[i];
            if (base.reached[i] == 1 && chk != gated[0] && chk != gated[1]) {
                hostX = chk;
                break;
            }
        }
        ClbRoundProbe gatedBase;
        gatedBase.checks = gated;
        gatedBase.count = 2;
        CLB_ASSERT(ClbRoundWithProbe(none, &gatedBase) == RSBS_COMBO_LOGIC_OK, "B5: the gated probe ran");
        CLB_ASSERT(hostX != 0, "B5: the support save reaches at least one other MM check unaided");
        CLB_ASSERT(gatedBase.reached[0] == 0 && gatedBase.reached[1] == 0,
                   "B5 PREMISE: the support save alone reaches neither CHECK_MAX_HP(4) check");

        const uint16_t hosts[3] = { gated[0], gated[1], hostX };
        MM_ComboLogic_SetHostPool(hosts, 3);
        const ComboLogicEngine* ootEngine = Combo_Logic_GetEngine(GAME_OOT);
        CLB_ASSERT(ootEngine->allEmptyHosts(ootEngine->self, nullptr, 0) == 0,
                   "B5 PREMISE: OoT offers no host, so MM's three hosts are the whole supply");

        ComboLogicBagItem withHeart[3];
        ComboLogicBagItem withoutHeart[3];
        memset(withHeart, 0, sizeof(withHeart));
        memset(withoutHeart, 0, sizeof(withoutHeart));
        for (int i = 0; i < 3; ++i) {
            withHeart[i].item.originGame = (uint8_t)GAME_MM;
            withHeart[i].item.id = (i == 0) ? heartId : neutralId;
            withoutHeart[i].item.originGame = (uint8_t)GAME_MM;
            withoutHeart[i].item.id = neutralId;
        }
        ComboLogicFillRequest req;
        memset(&req, 0, sizeof(req));
        req.bagCount = 3;
        req.goal = RSBS_COMBO_GOAL_BEAT_EITHER;
        req.logicRung = RSBS_COMBO_RUNG_BEATABLE;
        req.seed = 0xB5C0FFEEu;
        req.maxAttempts = RSBS_COMBO_LOGIC_FILL_RETRIES;

        req.bag = withHeart;
        ComboLogicFillResult resWith;
        const int stWith = Combo_Logic_RunFill(&req, &resWith);
        ComboLogicPlacement onX;
        memset(&onX, 0, sizeof(onX));
        const bool xHeld = Combo_Logic_GetPlacement(GAME_MM, hostX, &onX);
        int gatedHeld = 0;
        for (int i = 0; i < 2; ++i) {
            ComboLogicPlacement p;
            if (Combo_Logic_GetPlacement(GAME_MM, gated[i], &p)) {
                gatedHeld++;
            }
        }
        printf("[TEST] combo-logic-bag-composition: B5 WITH the heart row: status=%s attempts=%d goalProven=%d "
               "placed MM=%d OoT=%d, X holds id %u (heart id %u), heart-gated hosts filled %d/2\n",
               Combo_Logic_StatusName(stWith), resWith.attempts, resWith.goalProven ? 1 : 0,
               Combo_Logic_PlacementCount(GAME_MM), Combo_Logic_PlacementCount(GAME_OOT),
               xHeld ? (unsigned)onX.item.id : 0u, (unsigned)heartId, gatedHeld);
        CLB_ASSERT(stWith == RSBS_COMBO_LOGIC_OK && resWith.goalProven,
                   "B5: with the heart row the fill proves — two rows behind CHECK_MAX_HP(4) are placed with logic");
        CLB_ASSERT(Combo_Logic_PlacementCount(GAME_MM) == 3 && Combo_Logic_PlacementCount(GAME_OOT) == 0 &&
                       gatedHeld == 2,
                   "B5: all three rows are on MM's three hosts, two of them heart-gated");
        CLB_ASSERT(xHeld && onX.item.id == heartId,
                   "B5: the heart container sits on the unaided host X — it cannot be placed behind itself");
        Combo_Logic_ResetPlacements();

        req.bag = withoutHeart;
        ComboLogicFillResult resWithout;
        const int stWithout = Combo_Logic_RunFill(&req, &resWithout);
        printf("[TEST] combo-logic-bag-composition: B5 WITHOUT the heart row: status=%s attempts=%d goalProven=%d "
               "requiredPlaced=%d\n",
               Combo_Logic_StatusName(stWithout), resWithout.attempts, resWithout.goalProven ? 1 : 0,
               resWithout.requiredPlaced);
        CLB_ASSERT(stWithout == RSBS_COMBO_LOGIC_ERR_NO_CANDIDATE && !resWithout.goalProven &&
                       resWithout.attempts == RSBS_COMBO_LOGIC_FILL_RETRIES,
                   "B5 RED HALF: without the heart row the heart-gated hosts are never reached and the fill REFUSES "
                   "(no candidate host, every roll-back) instead of placing a row where health forbids it");
        Combo_Logic_ResetPlacements();
        MM_ComboLogic_SetHostPool(nullptr, 0);
        CLB_ASSERT(memcmp(saveWithSupport.get(), gSaveContext, OOT_SAVE_CONTEXT_SIZE) == 0,
                   "B5's rounds and fills returned the save to the support state");
        // Construction, not a claim: B5 changed the live save on purpose.
        memcpy(gSaveContext, saveAfterProfile.get(), OOT_SAVE_CONTEXT_SIZE);
    }

    // ---- B6: the OoT confinement word and the tokensanity record ("armed") ---
    if (ClmProfileIsArmed(profile)) {
        const uint32_t word = OoT_ComboLogic_ConfinementArmed();
        const int tokenId = OoT_ComboLogic_TestCounterItemId(1);
        int tokenRows = 0;
        int tokenPlentiful = 0;
        for (int i = 0; i < c.ootRows; ++i) {
            const ComboLogicPoolRow& r = c.rows[(size_t)i];
            if ((int)r.item.id == tokenId) {
                tokenRows++;
                tokenPlentiful += ((r.poolFlags & RSBS_COMBO_POOL_PLENTIFUL) != 0u) ? 1 : 0;
            }
        }
        // Both printed BEFORE either is asserted, so one broken build shows both
        // halves' red at once.
        printf("[TEST] combo-logic-bag-composition: B6 OoT confinement word 0x%08X (expected 0x%08X); gold skulltula "
               "token rows %d, of them PLENTIFUL %d (expected 10: tokensanity all, no starting tokens, so the "
               "record's min(added, 10) is 10)\n",
               (unsigned)word, (unsigned)kClmArmedProfileOoTWord, tokenRows, tokenPlentiful);
        CLB_ASSERT(word == kClmArmedProfileOoTWord,
                   "B6: OoT_ComboLogic_ConfinementArmed returns exactly the families this profile arms — no bit "
                   "missing, none extra");
        CLB_ASSERT((c.armedOoT & kClmArmedProfileOoTWord) == kClmArmedProfileOoTWord,
                   "B6: the compose request carried that word for OoT-origin rows");
        CLB_ASSERT(tokenPlentiful == 10,
                   "B6: the export marks exactly the ten tokens item_pool.cpp's tokensanity plentiful extra added");
    }

    // ---- teardown, S4-style ------------------------------------------------
    Combo_Logic_ResetPlacements();
    MM_ComboLogic_SetHostPool(nullptr, 0);
    CLB_ASSERT(MM_ComboLogic_HeldPlacementCount() == 0 && MM_ComboLogic_SnapshotLive() == 0,
               "MM's engine holds no placement and no live snapshot");
    CLB_ASSERT(OoT_ComboLogic_TestWorldDigest() == worldDigest0, "the row moved no OoT placement");
    CLB_ASSERT(memcmp(saveAfterProfile.get(), gSaveContext, OOT_SAVE_CONTEXT_SIZE) == 0,
               "the rounds and the fill returned the unified save buffer to the post-profile state");
    memcpy(gSaveContext, saveBefore.get(), OOT_SAVE_CONTEXT_SIZE);
    ClmProfileRestore(profile);
    printf("[TEST] PASS: combo-logic-bag-composition (%s)\n", profile);
    return TEST_PASS;
}
