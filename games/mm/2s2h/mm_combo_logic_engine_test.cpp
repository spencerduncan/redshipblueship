/**
 * @file mm_combo_logic_engine_test.cpp
 * The lock on MM's combo-logic engine export (ADR 0010 increment 3, #645; lane
 * K2b). CTest row MMComboLogicEngine in CMake/SingleExecutable.cmake, dispatch
 * "mm-combo-logic-engine" in src/common/test_runner.cpp.
 *
 * WHAT IS UNDER TEST is games/mm/2s2h/Rando/ComboLogicEngineSingleExe.cpp — the
 * `ComboLogicEngine` vtable MM registers for `src/common/combo_logic.h`. The
 * coordinator itself is already locked over synthetic stub engines by lane K1's
 * src/common/tests/test_combo_logic.c, so this row deliberately does NOT re-test
 * the round loop. It drives MM'S OWN vtable, obtained through
 * `Combo_Logic_GetEngine(GAME_MM)`, against MM's REAL region graph and REAL save.
 *
 * WHY THE VTABLE IS FETCHED FROM THE REGISTRY RATHER THAN REFERENCED DIRECTLY.
 * Doing it this way makes leg 1 a registrar-elision lock at the same time: MM's
 * engine publishes itself from a file-scope static object in a TU nothing else
 * references, which is exactly the #516 / #678 shape where the linker dropped the
 * member and the registrar never ran. A direct reference to the vtable would pull
 * the TU in by force and the row could not fail that way.
 *
 * ORDER OF THE LEGS IS DELIBERATE: every save-touching leg runs inside an outer
 * byte snapshot, and the byte-exactness legs run BEFORE the rounds, so that a
 * round which leaks state cannot make the byte-exactness claim pass. Leg numbers
 * are also the FAILURE CODES, so they are never renumbered; 11 belongs to the
 * entry point's own preconditions, which is why RunLegs goes 10 -> 12.
 *
 * WHAT REVIEW CHANGED IN THIS ROW, because three of its claims were weaker than
 * their comments said:
 *   - leg 6 observed `crossingOpen` only TRUE and `goalReached` only FALSE, so a
 *     constant answer passed. LEG 12 observes the other polarity of both, by
 *     graph surgery under a scope guard, and also observes the shrink detector
 *     firing and the round REPORTING the smaller set.
 *   - leg 8's repeat-grant row compared reachability, which a removed dedup does
 *     not move (the second grant of most ids collapses to RI_JUNK). It now reads
 *     MM's two genuine counters directly, and measures what a raw double give
 *     does to them in the same bracket.
 *   - leg 7's third round was described as a residue lock, but the residue it
 *     named self-heals at the top of every crawl. Its rationale is restated and
 *     it now asserts the residue is genuinely present, and poisons it by hand.
 *   - LEG 13 is new: `expand` must harvest own-origin placements, and did not.
 *   - LEG 14 is new: the two always-shuffled shop checks must be placeable hosts.
 *   - leg 5 parked the whole region graph into a function-local and restored it
 *     after four early-returning assertions. It is a scope guard now.
 *
 * ============================================================================
 * WHICH GiveItem / ConvertItem BRANCHES THIS ROW DOES *NOT* EXERCISE
 * ============================================================================
 *
 * Audit §6.3 lists "whether any branch of GiveItem/ConvertItem reachable only
 * with foreign items present touches state outside gSaveContext" as
 * undetermined, and this row does not close it. It grants only branches whose
 * whole effect is a write into `gSaveContext`:
 *
 *     RI_SINGLE_MAGIC, RI_DOUBLE_DEFENSE, RI_GREAT_SPIN_ATTACK,
 *     RI_WOODFALL_STRAY_FAIRY, RI_WOODFALL_SMALL_KEY, RI_WOODFALL_BOSS_KEY,
 *     RI_ABILITY_SWIM, RI_OCARINA_BUTTON_A, RI_FROG_BLUE
 *
 * NOT exercised, and named so nobody reads this row as covering them:
 *   - THE DEFAULT BRANCH, `MM_Item_Give(MM_gPlayState, ...)`. That is where every
 *     mask, song, bottle, trade item and ordinary inventory item goes, and
 *     `MM_gPlayState` is NULL here. `Item_GiveImpl` carries only PARTIAL null
 *     guards; the three legs that still dereference `play` are enumerated in
 *     Rando/ForeignItemsSingleExe.cpp's header (skull-token count, and the
 *     bottle-content and trade-item icon loads). Two of those three are reachable
 *     only on a save that already holds a bottle or trade item in a C/D-equipped
 *     slot, which is why exercising "some items" would prove nothing about the
 *     rest.
 *   - THE TRIFORCE-COMPLETION BRANCH (RI_TRIFORCE_PIECE at the required count):
 *     it calls `GameInteractor_ExecuteOnGameCompletion()` and emplaces a
 *     `GIEventTransition`. The engine snapshots the game-events queue DEPTH for
 *     exactly this reason and leg 4 proves the depth comes back — but a hook
 *     dispatch is not undone by any restore, and this row does not fire it.
 *   - RI_TRAP, which calls `Rando::MiscBehavior::OfferTrapItem()`.
 *   - RI_GS_TOKEN_SWAMP / RI_GS_TOKEN_OCEAN, which call
 *     `Inventory_IncrementSkullTokenCount`.
 *   - Every `ConvertItem` progressive branch whose "already maxed" arm is a bare
 *     `assert(false)` (RI_PROGRESSIVE_BOMB_BAG, _BOW, _MAGIC, _WALLET,
 *     _LULLABY, _SWORD). Reaching one of those is a live abort in a debug build,
 *     and it is reachable through a repeated grant — which is one of the reasons
 *     the engine dedups (its header's note A2).
 */

#include "global.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <vector>

#if !defined(RSBS_SINGLE_EXECUTABLE)
/**
 * The engine TU is guarded by RSBS_SINGLE_EXECUTABLE (it is the cross-game
 * surface), and test_runner.cpp declares this dispatch unconditionally, so a
 * standalone 2ship build reports pass rather than failing to link.
 */
extern "C" int MM_ComboLogicEngine_RunHeadless(void) {
    printf("[TEST] mm-combo-logic-engine: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <ship/Context.h>

#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/Logic/Logic.h"

#include "combo_logic.h"
#include "context.h"
#include "mm_game_hooks.h"

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;

// The CORE half of MM's rando bring-up (GameExports_SingleExe.cpp): ShipInit
// registrars + Rando::Init, which is what populates Rando::Logic::Regions.
// Asset-free and once-guarded; calling it twice is a no-op.
void MM_Rando_InitCore(void);

// The engine's own diagnostics surface (ComboLogicEngineSingleExe.cpp).
void MM_ComboLogic_SetHostPool(const uint16_t* checks, int count);
int MM_ComboLogic_ShrinkObservations(void);
int MM_ComboLogic_HarvestCount(void);
void MM_ComboLogic_BracketCounters(int* outBeginRefusals, int* outEndCalls, int* outRedundantEnds);
void MM_ComboLogic_ResetCounters(void);
int MM_ComboLogic_SnapshotLive(void);
int MM_ComboLogic_HeldPlacementCount(void);
}

namespace {

// ============================================================================
// Scope guards — an early return out of a graph-surgery leg must not leave the
// process's region graph rewritten
// ============================================================================
//
// WHY THESE EXIST (review finding). Legs 5, 12 and 13 mutate
// `Rando::Logic::Regions`, which is process-global and shared with every other
// row in a `--test all` process. An earlier draft of leg 5 parked the whole graph
// into a function-local and restored it by hand AFTER four CE_ASSERTs — each of
// which returns from RunLegs(). Any one of them firing destroyed the parked copy
// and left `Regions` permanently EMPTY, so every later diagnostic in the process
// became vacuous and the reported failure pointed at the wrong thing. The guards
// below make the restore unconditional.

/** Parks the whole region graph, leaving `Regions` empty, and swaps it back on
 *  scope exit however the scope is left. */
struct ParkedGraphGuard {
    std::map<RandoRegionId, Rando::Logic::RandoRegion> parked;
    ParkedGraphGuard() {
        parked.swap(Rando::Logic::Regions);
    }
    ~ParkedGraphGuard() {
        Rando::Logic::Regions.swap(parked);
    }
    ParkedGraphGuard(const ParkedGraphGuard&) = delete;
    ParkedGraphGuard& operator=(const ParkedGraphGuard&) = delete;
};

/** Copies one region's `exits` map and puts it back on scope exit, so a leg may
 *  add or remove an authored edge without leaking the edit. It copies rather
 *  than moves, so the graph stays usable inside the scope; `Regions.size()` is
 *  unchanged either way, which matters because `GetRegionIdFromEntrance`'s cache
 *  is keyed on that size (#659). */
struct RegionExitsGuard {
    RandoRegionId regionId;
    std::map<s32, Rando::Logic::RandoRegionExit> saved;
    explicit RegionExitsGuard(RandoRegionId id) : regionId(id), saved(Rando::Logic::Regions[id].exits) {
    }
    ~RegionExitsGuard() {
        Rando::Logic::Regions[regionId].exits = saved;
    }
    RegionExitsGuard(const RegionExitsGuard&) = delete;
    RegionExitsGuard& operator=(const RegionExitsGuard&) = delete;
};

/** Restores the whole live save (and the game-events queue depth, and
 *  `gCurrentRegionTime`) on scope exit. Used by the legs that mutate the save
 *  OUTSIDE a round bracket, because by A1 the live save is a round's starting
 *  state and such a mutation is not undone by `restore`. */
struct SaveGuard {
    std::unique_ptr<SaveContext> saved = std::make_unique<SaveContext>();
    size_t depth = 0;
    uint64_t regionTime = 0;
    SaveGuard() {
        memcpy(saved.get(), &gSaveContext, sizeof(SaveContext));
        depth = MM_GameEvents_Queue().size();
        regionTime = Rando::Logic::gCurrentRegionTime;
    }
    ~SaveGuard() {
        memcpy(&gSaveContext, saved.get(), sizeof(SaveContext));
        if (MM_GameEvents_Queue().size() > depth) {
            MM_GameEvents_Queue().resize(depth);
        }
        Rando::Logic::gCurrentRegionTime = regionTime;
    }
    SaveGuard(const SaveGuard&) = delete;
    SaveGuard& operator=(const SaveGuard&) = delete;
};

#define CE_ASSERT(cond, code, msg)                                                      \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            printf("[TEST] FAIL(%d): %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            return (code);                                                              \
        }                                                                               \
    } while (0)

const ComboLogicEngine* gEngine = nullptr;

/** One complete round over the engine, run exactly in the order
 *  combo_logic.h's CALL ORDER section prescribes, and reporting the facts a
 *  round makes available. */
struct RoundObservation {
    int beganOk = 0;
    int iterations = 0;
    int crossingOpen = 0;
    int goalReached = 0;
    int reachedHosts = 0;
    int allHosts = 0;
    // The first few reached host ids, so "identical queries agree" is a claim
    // about the ANSWER and not only about its cardinality.
    std::vector<uint16_t> reachedSample;
};

RoundObservation RunRound(const std::vector<uint16_t>& assume, int sampleCap = 32) {
    RoundObservation obs;

    // snapshot BEFORE beginQuery, as the contract requires: the restore has to
    // undo whatever beginQuery did to the live save too.
    if (!gEngine->snapshot(gEngine->self)) {
        return obs;
    }
    obs.beganOk = gEngine->beginQuery(gEngine->self);
    if (!obs.beganOk) {
        gEngine->restore(gEngine->self);
        gEngine->endQuery(gEngine->self);
        return obs;
    }

    for (uint16_t id : assume) {
        gEngine->assumeOwnItem(gEngine->self, id);
    }

    // The inner alternation, bounded the way the coordinator bounds it.
    for (int i = 0; i < RSBS_COMBO_LOGIC_MAX_ROUND_ITERATIONS; ++i) {
        obs.iterations++;
        if (!gEngine->expand(gEngine->self)) {
            break;
        }
    }

    // Read before teardown — checkReached and the enumerators are only valid
    // inside the bracket.
    obs.crossingOpen = gEngine->crossingOpen(gEngine->self) ? 1 : 0;
    obs.goalReached = gEngine->goalReached(gEngine->self) ? 1 : 0;
    obs.reachedHosts = gEngine->reachedEmptyHosts(gEngine->self, nullptr, 0);
    obs.allHosts = gEngine->allEmptyHosts(gEngine->self, nullptr, 0);

    obs.reachedSample.assign((size_t)sampleCap, 0);
    const int written = gEngine->reachedEmptyHosts(gEngine->self, obs.reachedSample.data(), sampleCap);
    (void)written;
    if (obs.reachedHosts < sampleCap) {
        obs.reachedSample.resize((size_t)(obs.reachedHosts < 0 ? 0 : obs.reachedHosts));
    }

    gEngine->restore(gEngine->self);
    gEngine->endQuery(gEngine->self);
    return obs;
}

// Save-only GiveItem branches. See the file header for the list of branches this
// deliberately does NOT reach.
const std::vector<uint16_t> kSaveOnlyGrants = {
    (uint16_t)RI_SINGLE_MAGIC,      (uint16_t)RI_DOUBLE_DEFENSE,       (uint16_t)RI_GREAT_SPIN_ATTACK,
    (uint16_t)RI_ABILITY_SWIM,      (uint16_t)RI_WOODFALL_STRAY_FAIRY, (uint16_t)RI_WOODFALL_SMALL_KEY,
    (uint16_t)RI_WOODFALL_BOSS_KEY, (uint16_t)RI_OCARINA_BUTTON_A,     (uint16_t)RI_FROG_BLUE,
};

/** The WHOLE reached-host list of one round (RunRound only samples 32), so a leg
 *  can pick hosts that are reached and hosts that are not. */
std::vector<uint16_t> ReachedHostList() {
    std::vector<uint16_t> out;
    if (!gEngine->snapshot(gEngine->self)) {
        return out;
    }
    if (!gEngine->beginQuery(gEngine->self)) {
        gEngine->restore(gEngine->self);
        gEngine->endQuery(gEngine->self);
        return out;
    }
    for (int i = 0; i < RSBS_COMBO_LOGIC_MAX_ROUND_ITERATIONS; ++i) {
        if (!gEngine->expand(gEngine->self)) {
            break;
        }
    }
    const int total = gEngine->reachedEmptyHosts(gEngine->self, nullptr, 0);
    if (total > 0) {
        out.assign((size_t)total, 0);
        gEngine->reachedEmptyHosts(gEngine->self, out.data(), total);
    }
    gEngine->restore(gEngine->self);
    gEngine->endQuery(gEngine->self);
    return out;
}

/** Every host the engine offers, in its own ascending order. */
std::vector<uint16_t> AllHostList() {
    std::vector<uint16_t> out;
    const int total = gEngine->allEmptyHosts(gEngine->self, nullptr, 0);
    if (total > 0) {
        out.assign((size_t)total, 0);
        gEngine->allEmptyHosts(gEngine->self, out.data(), total);
    }
    return out;
}

/** Drops the engine's placements on scope exit, so a failed assertion in a
 *  placement leg cannot leave the coordinator's writes in MM's check table. */
struct PlacementsGuard {
    ~PlacementsGuard() {
        gEngine->clearPlacements(gEngine->self);
    }
};

int RunLegs() {
    // ---- Leg 1: the engine is REGISTERED, and its vtable has no holes --------
    // This is also the registrar-elision lock: MM's engine publishes itself from
    // a file-scope static in a TU nothing else references (#516 / #678 shape).
    gEngine = Combo_Logic_GetEngine(GAME_MM);
    CE_ASSERT(gEngine != nullptr, 1,
              "no engine is registered for GAME_MM - ComboLogicEngineSingleExe.cpp's file-scope registrar never ran, "
              "which is the 2ship archive elision class (#516/#678), or registration was refused");
    CE_ASSERT(gEngine->abiVersion == RSBS_COMBO_LOGIC_ENGINE_ABI, 1,
              "MM's engine registered a different RSBS_COMBO_LOGIC_ENGINE_ABI than this build's header");
    CE_ASSERT(gEngine->beginQuery != nullptr && gEngine->assumeOwnItem != nullptr && gEngine->expand != nullptr &&
                  gEngine->crossingOpen != nullptr && gEngine->checkReached != nullptr &&
                  gEngine->reachedEmptyHosts != nullptr && gEngine->allEmptyHosts != nullptr &&
                  gEngine->goalReached != nullptr && gEngine->place != nullptr && gEngine->clearPlacements != nullptr &&
                  gEngine->endQuery != nullptr,
              1, "MM's engine has a NULL required function pointer - registration should have refused it");
    CE_ASSERT(gEngine->snapshot != nullptr && gEngine->restore != nullptr, 1,
              "MM's engine must supply BOTH snapshot and restore: every MM query writes the live save, so the round "
              "has nothing to bracket it with (audit §4.4)");

    // ---- Leg 2: the region graph is populated -------------------------------
    // Without this every later leg answers about an empty world and passes
    // vacuously.
    MM_Rando_InitCore();
    CE_ASSERT(!Rando::Logic::Regions.empty(), 2,
              "Rando::Logic::Regions is empty after MM_Rando_InitCore - the registrars did not run and every "
              "reachability leg below would be vacuous");

    MM_ComboLogic_ResetCounters();
    gEngine->clearPlacements(gEngine->self);
    MM_ComboLogic_SetHostPool(nullptr, 0); // the default: the graph's check set

    // ---- Leg 3: the host enumerators, order, truncation, and the ID SPACE ----
    //
    // The contract makes both enumerators return the TOTAL even when they wrote
    // at most `cap`, precisely so a caller can tell truncation from exhaustion.
    // MM is the engine where that matters: its graph names far more checks than
    // RSBS_COMBO_LOGIC_PLACEMENT_CAP, so a coordinator scratch buffer sized by
    // the PLACEMENT cap instead of by the ID SPACE silently loses hosts.
    const int allTotal = gEngine->allEmptyHosts(gEngine->self, nullptr, 0);
    printf("[TEST] mm-combo-logic-engine: allEmptyHosts total=%d, RSBS_COMBO_LOGIC_PLACEMENT_CAP=%d\n", allTotal,
           (int)RSBS_COMBO_LOGIC_PLACEMENT_CAP);
    CE_ASSERT(allTotal > 0, 3,
              "allEmptyHosts reports no hosts at all over the populated region graph - the host universe is empty");
    CE_ASSERT(allTotal > (int)RSBS_COMBO_LOGIC_PLACEMENT_CAP, 3,
              "MM's host id space no longer exceeds RSBS_COMBO_LOGIC_PLACEMENT_CAP. That is not a failure of the "
              "engine - it means the coordinator's host scratch buffer may now be sized by the placement cap "
              "without losing hosts, and this leg must be re-stated rather than deleted (the printed totals above "
              "are the numbers to re-state it with)");
    {
        // Full write into a buffer the size of the whole id space: ascending and
        // duplicate-free (ORDER IS A CONTRACT, and it must be a function of the
        // engine's table alone).
        std::vector<uint16_t> full((size_t)allTotal + 8, 0);
        const int wrote = gEngine->allEmptyHosts(gEngine->self, full.data(), (int)full.size());
        CE_ASSERT(wrote == allTotal, 3, "allEmptyHosts returned a different total when given a large buffer");
        for (int i = 1; i < allTotal; ++i) {
            CE_ASSERT(full[(size_t)i] > full[(size_t)i - 1], 3,
                      "allEmptyHosts is not strictly ascending - the contract requires a STABLE order that is a "
                      "function of the engine's own table, and duplicates would let one host be drawn twice");
        }

        // A3's CORRECTION, locked here: the two shop checks GeneratePools
        // shuffles in EVERY configuration must be in the host universe.
        // GeneratePools.cpp:126-131 skips RCTYPE_SHOP only when
        // RO_SHUFFLE_SHOPS == RO_GENERIC_NO *and* the row is neither of these two
        // ("We always want shuffle ..."), so an unconditional shop exclusion in
        // this engine is a settings-conditional NARROWING dressed as a fact —
        // and because `place` shares the predicate, it also made the two
        // always-shuffled rows un-placeable (RSBS_COMBO_LOGIC_ERR_ENGINE_REFUSED)
        // and silently dropped them from MM_ComboLogic_SetHostPool.
        {
            const uint16_t* begin = full.data();
            const uint16_t* end = full.data() + allTotal;
            CE_ASSERT(std::find(begin, end, (uint16_t)RC_CURIOSITY_SHOP_SPECIAL_ITEM) != end, 3,
                      "RC_CURIOSITY_SHOP_SPECIAL_ITEM is not in the host universe - GeneratePools shuffles it in "
                      "every settings configuration, so excluding it is a narrowing this engine may not make (it "
                      "also makes `place` refuse a host MM's real pool contains)");
            CE_ASSERT(std::find(begin, end, (uint16_t)RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM) != end, 3,
                      "RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM is not in the host universe - same fact");
        }

        // Truncation: return is still the TOTAL, and the prefix is the same ids.
        // NOT named `small`: Windows' rpcndr.h defines `small` as `char`.
        std::vector<uint16_t> truncBuf(4, 0xFFFF);
        const int truncated = gEngine->allEmptyHosts(gEngine->self, truncBuf.data(), 4);
        CE_ASSERT(truncated == allTotal, 3,
                  "allEmptyHosts returned the WRITTEN count instead of the TOTAL when truncated - truncation and "
                  "exhaustion become indistinguishable, and the coordinator refuses a world it should have filled");
        for (int i = 0; i < 4; ++i) {
            CE_ASSERT(truncBuf[(size_t)i] == full[(size_t)i], 3,
                      "the truncated write is not the prefix of the full write - the order is not stable");
        }
    }

    // ---- Leg 4: snapshot/restore is byte-exact over the WHOLE struct --------
    // and over the two pieces of state OUTSIDE the save that a query writes: the
    // game-events queue depth, and Rando::Logic::gCurrentRegionTime.
    {
        auto reference = std::make_unique<SaveContext>();
        memcpy(reference.get(), &gSaveContext, sizeof(SaveContext));
        const size_t referenceDepth = MM_GameEvents_Queue().size();
        const uint64_t referenceRegionTime = Rando::Logic::gCurrentRegionTime;

        CE_ASSERT(MM_ComboLogic_SnapshotLive() == 0, 4, "a snapshot was already live before leg 4 took one");
        CE_ASSERT(gEngine->snapshot(gEngine->self) != 0, 4, "snapshot refused");
        CE_ASSERT(MM_ComboLogic_SnapshotLive() == 1, 4, "snapshot succeeded but does not report itself live");

        // Mutate in several places at once, deliberately spread across the
        // struct: the claim is byte-exactness over the whole thing, not over the
        // fields somebody remembered.
        gSaveContext.save.saveInfo.playerData.isMagicAcquired = !gSaveContext.save.saveInfo.playerData.isMagicAcquired;
        gSaveContext.save.entrance = 0x1234;
        gSaveContext.save.playerForm = PLAYER_FORM_DEKU;
        gSaveContext.save.saveInfo.inventory.strayFairies[0] = 7;
        gSaveContext.save.shipSaveInfo.rando.randoInf[0] ^= 0x5A5A;
        RANDO_SAVE_CHECKS[1].randoItemId = RI_JUNK;
        RANDO_SAVE_CHECKS[1].shuffled = true;
        RANDO_EVENTS[0] = 9;
        // And push something onto the queue the way a triforce-completion give
        // would, which no memcpy of the save can undo.
        MM_GameEvents_Queue().emplace_back(GIEventTransition{ .entrance = 0,
                                                              .cutsceneIndex = 0,
                                                              .transitionTrigger = TRANS_TRIGGER_START,
                                                              .transitionType = TRANS_TYPE_FADE_BLACK });
        // And move gCurrentRegionTime, the thread_local uint64_t OUTSIDE the save
        // that FindReachableRegions and SetCurrentRegionTime both assign
        // (Logic.cpp:17, Logic.h:105/121-124). No memcmp of the save can see it,
        // which is exactly why the bracket has to carry it: the contract says the
        // pair "captures everything this engine's queries will mutate", and
        // mm_trick_bindings_test.cpp already treats this word as bracket state.
        Rando::Logic::gCurrentRegionTime = ~referenceRegionTime;
        CE_ASSERT(memcmp(reference.get(), &gSaveContext, sizeof(SaveContext)) != 0, 4,
                  "the mutation leg did not actually change gSaveContext - leg 4 would pass vacuously");
        CE_ASSERT(MM_GameEvents_Queue().size() == referenceDepth + 1, 4,
                  "the queue push did not take - the queue-depth half of leg 4 would pass vacuously");
        CE_ASSERT(Rando::Logic::gCurrentRegionTime != referenceRegionTime, 4,
                  "the gCurrentRegionTime mutation did not take - that half of leg 4 would pass vacuously");

        gEngine->restore(gEngine->self);
        CE_ASSERT(Rando::Logic::gCurrentRegionTime == referenceRegionTime, 4,
                  "restore did not put Rando::Logic::gCurrentRegionTime back - it is state OUTSIDE gSaveContext that "
                  "every crawl writes, so the whole-struct memcmp below cannot see it leaking out of the bracket");
        CE_ASSERT(memcmp(reference.get(), &gSaveContext, sizeof(SaveContext)) == 0, 4,
                  "restore did not put gSaveContext back BYTE FOR BYTE over the whole struct");
        CE_ASSERT(MM_GameEvents_Queue().size() == referenceDepth, 4,
                  "restore did not truncate MM_GameEvents_Queue back to the snapshotted depth - a simulated "
                  "completion can leak a queued transition into gameplay (Logic.cpp's closure guards this same way)");
        CE_ASSERT(MM_ComboLogic_SnapshotLive() == 0, 4, "the snapshot is still live after a restore");

        // A SECOND restore must be a no-op, not a re-application of a stale
        // snapshot over newer state. The coordinator's teardown may reach restore
        // on a bracket that never opened, so this has to hold.
        gSaveContext.save.entrance = 0x4321;
        gEngine->restore(gEngine->self);
        CE_ASSERT(gSaveContext.save.entrance == 0x4321, 4,
                  "a second restore re-applied the stale snapshot and reverted state the round never owned");
        gSaveContext.save.entrance = reference->save.entrance;
        CE_ASSERT(memcmp(reference.get(), &gSaveContext, sizeof(SaveContext)) == 0, 4,
                  "leg 4 did not leave the save as it found it");
    }

    // ---- Leg 5: the failed bracket - beginQuery refuses, endQuery is safe ----
    //
    // THE CONCRETE HAZARD, and why this leg exists: the coordinator snapshots a
    // side BEFORE calling its beginQuery, so a refusal leaves a live snapshot and
    // a teardown that must still run. endQuery therefore has to be safe with no
    // round open, and safe twice.
    {
        auto reference = std::make_unique<SaveContext>();
        memcpy(reference.get(), &gSaveContext, sizeof(SaveContext));

        int beginRefusalsBefore = 0, endCallsBefore = 0, redundantBefore = 0;
        MM_ComboLogic_BracketCounters(&beginRefusalsBefore, &endCallsBefore, &redundantBefore);

        // Take the graph away, exactly as an ordering mistake would. UNDER A
        // SCOPE GUARD: every CE_ASSERT below returns out of RunLegs(), and an
        // earlier draft's hand-written swap-back never ran on those paths, so one
        // failed assertion left Rando::Logic::Regions permanently empty and every
        // later leg's diagnosis meaningless.
        {
            ParkedGraphGuard parkGuard;
            CE_ASSERT(Rando::Logic::Regions.empty(), 5, "the graph could not be parked - leg 5 would be vacuous");

            CE_ASSERT(gEngine->snapshot(gEngine->self) != 0, 5, "snapshot refused with no graph (it must not care)");
            const int began = gEngine->beginQuery(gEngine->self);
            if (began != 0) {
                // Do not leave a live snapshot behind on the failure path either.
                gEngine->restore(gEngine->self);
                gEngine->endQuery(gEngine->self);
            }
            CE_ASSERT(began == 0, 5,
                      "beginQuery ACCEPTED with an empty region graph - every later answer would be 'nothing is "
                      "reachable', which is indistinguishable from a world in which nothing is");

            // The coordinator's teardown, on a bracket that never opened.
            gEngine->restore(gEngine->self);
            gEngine->endQuery(gEngine->self);
            gEngine->endQuery(gEngine->self); // and twice
        }
        CE_ASSERT(!Rando::Logic::Regions.empty(), 5, "the graph was not restored after leg 5");

        int beginRefusalsAfter = 0, endCallsAfter = 0, redundantAfter = 0;
        MM_ComboLogic_BracketCounters(&beginRefusalsAfter, &endCallsAfter, &redundantAfter);
        CE_ASSERT(beginRefusalsAfter == beginRefusalsBefore + 1, 5, "beginQuery's refusal was not recorded");
        CE_ASSERT(endCallsAfter == endCallsBefore + 2, 5, "both endQuery calls were not seen");
        CE_ASSERT(redundantAfter == redundantBefore + 2, 5,
                  "endQuery did not treat a teardown with no round open as the no-op it must be");
        CE_ASSERT(MM_ComboLogic_SnapshotLive() == 0, 5, "the failed bracket left a live snapshot behind");
        CE_ASSERT(memcmp(reference.get(), &gSaveContext, sizeof(SaveContext)) == 0, 5,
                  "the failed bracket changed gSaveContext");

        // And the engine still works afterwards: a refusal must not latch.
        const RoundObservation after = RunRound({});
        CE_ASSERT(after.beganOk != 0, 5, "beginQuery still refuses after the graph came back - the refusal latched");
    }

    // ---- Leg 6: a real round over the real graph, from a bare start ---------
    //
    // WHAT THIS LEG DOES *NOT* PROVE (corrected in review). An earlier draft
    // claimed a bug making every answer constant would fail here. It would not:
    // this leg observes `crossingOpen` only in its TRUE state and `goalReached`
    // only in its FALSE state, so a `crossingOpen` hardwired to 1 and a
    // `goalReached` hardwired to 0 both pass — and the second is the specific bug
    // the code is exposed to, because `GoalReached` synthesizes a
    // ReachabilityCrawl out of the round's own accumulated regions, and
    // populating that from the wrong set yields a constant false. LEG 12 is the
    // leg that observes the other polarity of both, and it is where that claim
    // now lives. What this leg locks is the bare answers themselves:
    //   - the crossing IS open from the arrival with nothing assumed. Audit §4.5:
    //     RR_CLOCK_TOWN_SOUTH -> RR_CLOCK_TOWER_INTERIOR is EXIT(..., true) and
    //     the arrival entrance resolves to RR_CLOCK_TOWN_SOUTH, so the MM->OoT
    //     crossing is unconditional from MM's root.
    //   - the GOAL is NOT reached from a bare start. The Moon needs Oath plus the
    //     remains/masks counts, and the lair needs them again (Regions/Moon.cpp).
    RoundObservation bare;
    {
        auto reference = std::make_unique<SaveContext>();
        memcpy(reference.get(), &gSaveContext, sizeof(SaveContext));
        const size_t referenceDepth = MM_GameEvents_Queue().size();

        bare = RunRound({});
        CE_ASSERT(bare.beganOk != 0, 6, "beginQuery refused on a populated graph");
        printf("[TEST] mm-combo-logic-engine: bare round: iterations=%d crossingOpen=%d goal=%d reachedHosts=%d "
               "allHosts=%d\n",
               bare.iterations, bare.crossingOpen, bare.goalReached, bare.reachedHosts, bare.allHosts);
        CE_ASSERT(bare.crossingOpen == 1, 6,
                  "crossingOpen is FALSE from the MM arrival with nothing assumed - RR_CLOCK_TOWER_INTERIOR is "
                  "reachable from RR_CLOCK_TOWN_SOUTH by an unconditional exit (audit §4.5), so either the "
                  "observable names the wrong region or the crawl is not reaching the arrival region at all");
        CE_ASSERT(bare.goalReached == 0, 6,
                  "goalReached is TRUE from a bare start - MM_GOAL needs the lair AND a Majora-capable kit, so this "
                  "answer is not reading the graph or the save");
        CE_ASSERT(bare.reachedHosts > 0, 6, "no reached hosts at all from the arrival - the crawl reached nothing");
        CE_ASSERT(bare.reachedHosts <= bare.allHosts, 6,
                  "reachedEmptyHosts reports MORE hosts than allEmptyHosts - the contract makes the latter a "
                  "SUPERSET (the same list without the reachability filter)");
        CE_ASSERT(bare.iterations >= 2, 6,
                  "expand converged in a single call, so it never once answered 'nothing new' after a change - the "
                  "coordinator's fixpoint loop would have nothing to converge on");

        // checkReached distinguishes: a reached host reads reached, and the round
        // is over so nothing does.
        CE_ASSERT(MM_ComboLogic_SnapshotLive() == 0, 6, "the round left a live snapshot");
        CE_ASSERT(memcmp(reference.get(), &gSaveContext, sizeof(SaveContext)) == 0, 6,
                  "a round with NOTHING assumed changed gSaveContext - the crawl zeroes and re-fires RANDO_EVENTS, "
                  "so this is the bracket failing");
        CE_ASSERT(MM_GameEvents_Queue().size() == referenceDepth, 6, "a bare round changed the game-events queue");
    }

    // ---- Leg 7: IDENTICAL QUERIES AGREE, including over event residue -------
    //
    // RESTATED IN REVIEW. The earlier text said the third round's hazard was that
    // "CrawlReachableRegions ZEROES and re-fires RANDO_EVENTS in the live save, so
    // an unbracketed crawl between two rounds is exactly the state leak that would
    // make the second round answer a different question". That is backwards as a
    // hazard: the zeroing is the DEFENCE, not the leak. Logic.cpp:281-285 zeroes
    // every RANDO_EVENTS[i] at the top of every crawl ("stale RANDO_EVENTS from a
    // previous evaluation must not satisfy this one"), so whatever an unbracketed
    // crawl leaves behind is wiped by the next round's first crawl, and the
    // sub-leg as written would have passed with snapshot/restore replaced by
    // no-ops.
    //
    // WHAT IT LOCKS NOW. Event residue in the live save does not change the
    // answer, stated as a falsifiable claim rather than an assumed hazard: the
    // residue is asserted to be genuinely PRESENT before the third round runs (an
    // unrelated crawl leaves a populated RANDO_EVENTS, and the leg additionally
    // poisons the array by hand), and the third round must still agree with
    // `bare`. This is the regression an upstream pull that drops the zeroing loop
    // would trip, which is a real way for it to go red. The real bracket lock for
    // a round that MUTATES state is leg 8's post-grant whole-struct memcmp, and
    // leg 4's is the one for the bracket itself.
    {
        SaveGuard leg7Guard; // the hand poison below is an UNBRACKETED save write
        const RoundObservation again = RunRound({});
        CE_ASSERT(again.crossingOpen == bare.crossingOpen && again.goalReached == bare.goalReached &&
                      again.reachedHosts == bare.reachedHosts && again.allHosts == bare.allHosts &&
                      again.iterations == bare.iterations,
                  7, "two back-to-back identical rounds gave different answers");
        CE_ASSERT(again.reachedSample == bare.reachedSample, 7,
                  "two back-to-back identical rounds reported the same NUMBER of hosts but different host IDS");

        // An unrelated crawl, outside any bracket, the way the check tracker
        // would run one. It leaves a populated RANDO_EVENTS in the LIVE save.
        const Rando::Logic::ReachabilityCrawl unrelated =
            Rando::Logic::CrawlReachableRegions(gSaveContext.save.entrance);
        (void)Rando::Logic::EvaluateReachableChecks(unrelated);

        // NON-VACUITY, and then some. Prove the residue is really there, and then
        // poison the array outright so the third round faces event counts no crawl
        // would ever have produced. If the zeroing loop at Logic.cpp:281-285 ever
        // goes away, this is what goes red.
        bool residuePresent = false;
        for (int i = 0; i < RE_MAX; i++) {
            if (RANDO_EVENTS[i] != 0) {
                residuePresent = true;
                break;
            }
        }
        CE_ASSERT(residuePresent, 7,
                  "an unbracketed crawl left RANDO_EVENTS entirely zero - there is no event residue for the third "
                  "round to be immune to, so this sub-leg would pass vacuously");
        for (int i = 0; i < RE_MAX; i++) {
            RANDO_EVENTS[i] = 0x7F;
        }

        const RoundObservation third = RunRound({});
        CE_ASSERT(third.crossingOpen == bare.crossingOpen && third.goalReached == bare.goalReached &&
                      third.reachedHosts == bare.reachedHosts && third.allHosts == bare.allHosts &&
                      third.iterations == bare.iterations,
                  7,
                  "a round run after an unrelated crawl gave a different answer than the same round run before it - "
                  "the engine inherits residue from whatever ran between rounds");
        CE_ASSERT(third.reachedSample == bare.reachedSample, 7,
                  "the host IDS changed after an unrelated crawl ran between two identical rounds");
    }

    // ---- Leg 8: MONOTONE UNDER ASSUME, and the give path stays in the save ---
    {
        auto reference = std::make_unique<SaveContext>();
        memcpy(reference.get(), &gSaveContext, sizeof(SaveContext));
        const size_t referenceDepth = MM_GameEvents_Queue().size();
        const int shrinksBefore = MM_ComboLogic_ShrinkObservations();

        // Granted one at a time, cumulatively, so a single item that LOWERED
        // reachability is attributable rather than averaged away.
        std::vector<uint16_t> granted;
        int prevReached = bare.reachedHosts;
        int prevCrossing = bare.crossingOpen;
        for (uint16_t id : kSaveOnlyGrants) {
            granted.push_back(id);
            const RoundObservation obs = RunRound(granted);
            CE_ASSERT(obs.beganOk != 0, 8, "beginQuery refused inside the monotonicity leg");
            CE_ASSERT(obs.reachedHosts >= prevReached, 8,
                      "granting one more item REDUCED the reached-host count - reachability is not monotone under "
                      "assume, and assumed fill is unsound without it (ADR 0010 §2.3)");
            CE_ASSERT(obs.crossingOpen >= prevCrossing, 8, "granting one more item CLOSED the crossing");
            prevReached = obs.reachedHosts;
            prevCrossing = obs.crossingOpen;
        }
        printf("[TEST] mm-combo-logic-engine: reachedHosts %d (bare) -> %d after %zu save-only grants\n",
               bare.reachedHosts, prevReached, kSaveOnlyGrants.size());
        // NON-VACUITY. `>=` alone is satisfied by an engine whose assume does
        // nothing at all, which is precisely the failure a monotonicity lock is
        // most likely to hide. Measured on this tree: 424 reached hosts bare,
        // 489 after the nine grants. Asserting STRICT growth over the set is what
        // makes every `>=` above mean something; if a future settings change makes
        // these nine items inert, re-state this with grants that are not, rather
        // than deleting it.
        CE_ASSERT(prevReached > bare.reachedHosts, 8,
                  "granting nine real MM items changed the reached-host count not at all - either assumeOwnItem is "
                  "not reaching the give path, or the crawl is not reading the save it writes, and every monotone "
                  "assertion above is then vacuous");

        // A repeated grant of the same id must change nothing (the engine's note
        // A2: MM's counter gives are not idempotent, so the engine dedups).
        const RoundObservation once = RunRound(granted);
        std::vector<uint16_t> doubled = granted;
        for (uint16_t id : kSaveOnlyGrants) {
            doubled.push_back(id);
        }
        const RoundObservation repeated = RunRound(doubled);
        CE_ASSERT(repeated.reachedHosts == once.reachedHosts && repeated.crossingOpen == once.crossingOpen &&
                      repeated.goalReached == once.goalReached && repeated.reachedSample == once.reachedSample,
                  8,
                  "assuming the same ids twice in one round changed the answer - MM's stray-fairy, small-key, "
                  "skull-token and triforce gives are COUNTERS, so a double grant over-states the world");

        // THE DEDUP, READ OFF THE COUNTERS THEMSELVES (added in review).
        //
        // The reachability comparison above is NOT a lock on A2's dedup. With the
        // dedup removed, most of the second grants collapse to RI_JUNK anyway —
        // ConvertItem's `!IsItemObtainable` arm returns RI_JUNK
        // (ConvertItem.cpp:646-671) and IsItemObtainable is false for an
        // already-held item — so only the two genuine COUNTERS in the list would
        // move save state at all, and neither of them is a logic term whose loss
        // shows up in `reachedHosts`. So this sub-leg reads the counters directly,
        // INSIDE the bracket where the grants land, and it measures the red half
        // in the same breath: the same two ids given twice through MM's OWN give
        // path (bypassing the engine) must advance the counters by TWO, which is
        // what would happen to every round if the dedup went away.
        {
            const int fairyIdx = DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE;
            CE_ASSERT(gEngine->snapshot(gEngine->self) != 0, 8, "snapshot refused in the dedup sub-leg");
            CE_ASSERT(gEngine->beginQuery(gEngine->self) != 0, 8, "beginQuery refused in the dedup sub-leg");

            // Zero both counters first, INSIDE the bracket, so the arithmetic is
            // exact regardless of what this row inherited. A negative key count is
            // GiveItem's "no keys yet" sentinel and takes its `= 1` branch rather
            // than its `++`, which would make a relative assertion wrong
            // (GiveItem.cpp:55-63).
            gSaveContext.save.saveInfo.inventory.strayFairies[fairyIdx] = 0;
            DUNGEON_KEY_COUNT(fairyIdx) = 0;

            // Through the ENGINE, twice each.
            gEngine->assumeOwnItem(gEngine->self, (uint16_t)RI_WOODFALL_STRAY_FAIRY);
            gEngine->assumeOwnItem(gEngine->self, (uint16_t)RI_WOODFALL_STRAY_FAIRY);
            gEngine->assumeOwnItem(gEngine->self, (uint16_t)RI_WOODFALL_SMALL_KEY);
            gEngine->assumeOwnItem(gEngine->self, (uint16_t)RI_WOODFALL_SMALL_KEY);

            const int fairyAfterEngine = (int)gSaveContext.save.saveInfo.inventory.strayFairies[fairyIdx];
            const int keyAfterEngine = (int)DUNGEON_KEY_COUNT(fairyIdx);
            printf("[TEST] mm-combo-logic-engine: dedup: two engine grants each -> woodfall fairies=%d keys=%d\n",
                   fairyAfterEngine, keyAfterEngine);
            CE_ASSERT(fairyAfterEngine == 1, 8,
                      "assumeOwnItem granted RI_WOODFALL_STRAY_FAIRY TWICE in one round - GiveItem.cpp:17 is a `++`, "
                      "so the round now proves reachability with a fairy the player does not have (A2's dedup is "
                      "gone)");
            CE_ASSERT(keyAfterEngine == 1, 8,
                      "assumeOwnItem granted RI_WOODFALL_SMALL_KEY TWICE in one round - GiveItem.cpp:55-63 is a `++`, "
                      "so the round holds a key the player does not have");

            // THE RED HALF, MEASURED IN THE SAME BREATH: MM's own give path really
            // does count, so the 1s above are the dedup doing work rather than an
            // item that happens to be inert.
            Rando::GiveItem(Rando::ConvertItem(RI_WOODFALL_STRAY_FAIRY));
            Rando::GiveItem(Rando::ConvertItem(RI_WOODFALL_STRAY_FAIRY));
            Rando::GiveItem(Rando::ConvertItem(RI_WOODFALL_SMALL_KEY));
            Rando::GiveItem(Rando::ConvertItem(RI_WOODFALL_SMALL_KEY));
            const int fairyAfterRaw = (int)gSaveContext.save.saveInfo.inventory.strayFairies[fairyIdx];
            const int keyAfterRaw = (int)DUNGEON_KEY_COUNT(fairyIdx);
            printf("[TEST] mm-combo-logic-engine: dedup: two RAW gives each on top -> woodfall fairies=%d keys=%d "
                   "(this is the number a round would carry with the dedup removed)\n",
                   fairyAfterRaw, keyAfterRaw);
            CE_ASSERT(fairyAfterRaw == 3, 8,
                      "two raw GiveItem(ConvertItem(RI_WOODFALL_STRAY_FAIRY)) calls did NOT advance the stray-fairy "
                      "counter by two - the item has become idempotent, so the dedup assertion above no longer "
                      "distinguishes anything and A2 must be re-stated");
            CE_ASSERT(keyAfterRaw == 3, 8,
                      "two raw GiveItem(ConvertItem(RI_WOODFALL_SMALL_KEY)) calls did NOT advance the Woodfall key "
                      "count by two - same conclusion");

            gEngine->restore(gEngine->self);
            gEngine->endQuery(gEngine->self);
        }

        CE_ASSERT(MM_ComboLogic_ShrinkObservations() == shrinksBefore, 8,
                  "a recompute inside a round came back SMALLER than the round's accumulation: MM's reachability is "
                  "not monotone under the granted items (see MM_ComboLogic_ShrinkObservations)");

        // THE GIVE PATH TOUCHED NOTHING OUTSIDE THE SNAPSHOT, for these items.
        // The file header names the branches this does NOT cover.
        CE_ASSERT(memcmp(reference.get(), &gSaveContext, sizeof(SaveContext)) == 0, 8,
                  "a round that GAVE items left gSaveContext changed - either a give reached state the snapshot "
                  "does not cover, or the restore is incomplete");
        CE_ASSERT(MM_GameEvents_Queue().size() == referenceDepth, 8,
                  "a round that gave items changed the game-events queue depth");
    }

    // ---- Leg 9: place / clearPlacements ------------------------------------
    {
        auto reference = std::make_unique<SaveContext>();
        memcpy(reference.get(), &gSaveContext, sizeof(SaveContext));

        // A known-shape host pool, so the leg's arithmetic is exact. Drawn from
        // the real enumeration so every id is a host the engine accepts.
        std::vector<uint16_t> universe((size_t)allTotal, 0);
        gEngine->allEmptyHosts(gEngine->self, universe.data(), allTotal);
        CE_ASSERT(universe.size() >= 3, 9, "fewer than three hosts exist - leg 9 cannot author its shapes");
        const uint16_t hostA = universe[0];
        const uint16_t hostB = universe[1];
        const uint16_t hostC = universe[2];

        const RandoItemId priorA = RANDO_SAVE_CHECKS[hostA].randoItemId;
        const bool priorShuffledA = RANDO_SAVE_CHECKS[hostA].shuffled;
        // Read through the same accessor the assertion below uses, so the leg
        // cannot depend on SLOT()'s table mapping the way a hand-written slot
        // index would.
        const u8 bunnyBefore = INV_CONTENT(ITEM_MASK_BUNNY);

        SharedItem mmItem;
        mmItem.originGame = (uint8_t)GAME_MM;
        mmItem.flags = 0;
        mmItem.id = (uint16_t)RI_MASK_BUNNY;
        SharedItem ootItem;
        ootItem.originGame = (uint8_t)GAME_OOT;
        ootItem.flags = 0;
        ootItem.id = 0x1234; // an OoT id this TU deliberately cannot interpret

        CE_ASSERT(MM_ComboLogic_HeldPlacementCount() == 0, 9, "the engine already held placements");
        CE_ASSERT(gEngine->place(gEngine->self, hostA, mmItem) != 0, 9, "place refused an MM-origin item on a host");
        CE_ASSERT(RANDO_SAVE_CHECKS[hostA].randoItemId == (RandoItemId)mmItem.id, 9,
                  "an MM-origin placement did not write the check's randoItemId (GlitchlessLogic.cpp's own write)");
        CE_ASSERT(RANDO_SAVE_CHECKS[hostA].shuffled, 9, "an MM-origin placement did not set the shuffled bit");

        // IT MUST NOT GRANT. Placing is not holding. Compared against what the
        // slot held BEFORE the place, so the leg does not depend on the mask
        // being absent from whatever save this row inherited.
        CE_ASSERT(INV_CONTENT(ITEM_MASK_BUNNY) == bunnyBefore, 9,
                  "place GRANTED the item it placed - every placement would be instantly available and the fill's "
                  "proof vacuous");

        // JUNK COVER: a foreign id must never enter MM's tables (ADR 0002).
        CE_ASSERT(gEngine->place(gEngine->self, hostB, ootItem) != 0, 9, "place refused an OoT-origin item");
        CE_ASSERT(RANDO_SAVE_CHECKS[hostB].randoItemId == RI_JUNK, 9,
                  "an OoT-origin placement did not leave the legal junk-class MM filler in MM's own table - a raw "
                  "foreign id in RANDO_SAVE_CHECKS is the #356 bug class");
        CE_ASSERT(RANDO_SAVE_CHECKS[hostB].randoItemId != (RandoItemId)ootItem.id, 9,
                  "the OoT id was written straight into MM's check table");

        // IDEMPOTENT, and a DIFFERENT item on the same host is refused.
        CE_ASSERT(gEngine->place(gEngine->self, hostA, mmItem) != 0, 9,
                  "re-placing the same item on the same host was refused - the coordinator re-applies its whole "
                  "table after every restore, so place must be idempotent");
        CE_ASSERT(MM_ComboLogic_HeldPlacementCount() == 2, 9, "an idempotent re-place was counted as a new placement");
        CE_ASSERT(gEngine->place(gEngine->self, hostA, ootItem) == 0, 9,
                  "place accepted a DIFFERENT item on an already-held host");

        // An occupied host leaves the enumeration.
        const int allAfter = gEngine->allEmptyHosts(gEngine->self, nullptr, 0);
        CE_ASSERT(allAfter == allTotal - 2, 9,
                  "allEmptyHosts did not drop exactly the two hosts the coordinator placed on");

        // The placements survive a snapshot/restore ROUND only because the
        // coordinator re-applies them; the engine's job is that the re-apply
        // works. Prove the restore really does undo the writes first.
        CE_ASSERT(gEngine->snapshot(gEngine->self) != 0, 9, "snapshot refused in leg 9");
        RANDO_SAVE_CHECKS[hostC].randoItemId = RI_JUNK;
        gEngine->restore(gEngine->self);
        CE_ASSERT(RANDO_SAVE_CHECKS[hostA].randoItemId == (RandoItemId)mmItem.id, 9,
                  "the restore reverted a placement made BEFORE the snapshot - the snapshot was taken at the wrong "
                  "time or the placement did not land in the save");
        CE_ASSERT(gEngine->place(gEngine->self, hostA, mmItem) != 0, 9, "the post-restore re-apply was refused");

        // clearPlacements puts back exactly what it found.
        gEngine->clearPlacements(gEngine->self);
        CE_ASSERT(MM_ComboLogic_HeldPlacementCount() == 0, 9, "clearPlacements left placements behind");
        CE_ASSERT(RANDO_SAVE_CHECKS[hostA].randoItemId == priorA && RANDO_SAVE_CHECKS[hostA].shuffled == priorShuffledA,
                  9, "clearPlacements did not restore the host's prior item and shuffled bit");
        CE_ASSERT(gEngine->allEmptyHosts(gEngine->self, nullptr, 0) == allTotal, 9,
                  "the hosts did not return to the enumeration after clearPlacements");
        CE_ASSERT(memcmp(reference.get(), &gSaveContext, sizeof(SaveContext)) == 0, 9,
                  "leg 9 did not leave gSaveContext as it found it after clearPlacements");
    }

    // ---- Leg 10: the explicit host-pool seam (the engine's note A3) ---------
    {
        std::vector<uint16_t> universe((size_t)allTotal, 0);
        gEngine->allEmptyHosts(gEngine->self, universe.data(), allTotal);
        // Handed in DESCENDING, to prove the engine re-sorts: host order is a
        // contract and must be a function of the engine's table, not the caller's.
        const std::vector<uint16_t> pool = { universe[2], universe[1], universe[0] };
        MM_ComboLogic_SetHostPool(pool.data(), (int)pool.size());
        std::vector<uint16_t> out(3, 0);
        const int total = gEngine->allEmptyHosts(gEngine->self, out.data(), 3);
        CE_ASSERT(total == 3, 10, "the explicit host pool did not become the host universe");
        CE_ASSERT(out[0] == universe[0] && out[1] == universe[1] && out[2] == universe[2], 10,
                  "the explicit host pool was enumerated in the CALLER's order - the contract makes host order a "
                  "function of the engine's own table");
        MM_ComboLogic_SetHostPool(nullptr, 0);
        CE_ASSERT(gEngine->allEmptyHosts(gEngine->self, nullptr, 0) == allTotal, 10,
                  "clearing the explicit host pool did not restore the graph's check set");
    }

    // ---- Leg 12: THE OTHER POLARITY of crossingOpen and goalReached, and the
    //      shrink detector, all actually observed ---------------------------
    //
    // ADDED IN REVIEW, because legs 6-8 observe `crossingOpen` only TRUE and
    // `goalReached` only FALSE. A `crossingOpen` hardwired to 1 and a
    // `goalReached` hardwired to 0 pass every one of them — including leg 7's
    // self-consistency comparisons and leg 8's `obs.crossingOpen >= prevCrossing`,
    // which any constant satisfies. `goalReached` is the one that matters most,
    // because it synthesizes a ReachabilityCrawl out of the round's own
    // accumulated regions (ComboLogicEngineSingleExe.cpp's GoalReached), and
    // forgetting to populate `crawl.reachableRegions`, or populating it from the
    // wrong set, yields a constant false that nothing else in this row would
    // notice.
    //
    // Both halves need GRAPH SURGERY, because both observables are unconditional
    // facts about MM's authored graph from the arrival: the crossing edge is
    // `EXIT(..., true)` and the lair sits behind a full playthrough. The surgery
    // is under RegionExitsGuard, which restores the region's exits however the
    // scope is left, and the save writes are under SaveGuard, because by A1 an
    // unbracketed save write IS a round's starting state and `restore` will not
    // undo it.
    {
        SaveGuard leg12Save;

        // ---- 12a: crossingOpen goes FALSE when its one authored edge is gone.
        // RR_CLOCK_TOWER_INTERIOR has exactly one edge in: RR_CLOCK_TOWN_SOUTH's
        // EXIT(ENTRANCE(CLOCK_TOWER_INTERIOR, 1), ENTRANCE(SOUTH_CLOCK_TOWN, 0),
        // true) at Regions/Central.cpp:230. Nothing else in the graph names that
        // entrance and RR_MAX's save-warp/owl exits do not include it, so removing
        // it must close the crossing and nothing else.
        {
            RegionExitsGuard southGuard(RR_CLOCK_TOWN_SOUTH);
            const size_t erased =
                Rando::Logic::Regions[RR_CLOCK_TOWN_SOUTH].exits.erase(ENTRANCE(CLOCK_TOWER_INTERIOR, 1));
            CE_ASSERT(erased == 1, 12,
                      "RR_CLOCK_TOWN_SOUTH no longer carries the EXIT to ENTRANCE(CLOCK_TOWER_INTERIOR, 1) - the "
                      "MM->OoT crossing edge moved, so this leg cannot close it and leg 6's crossing claim needs "
                      "re-stating against wherever it now lives (audit §4.5)");

            const RoundObservation closed = RunRound({});
            CE_ASSERT(closed.beganOk != 0, 12, "beginQuery refused with the crossing edge removed");
            CE_ASSERT(closed.crossingOpen == 0, 12,
                      "crossingOpen is still TRUE with the ONLY authored edge into RR_CLOCK_TOWER_INTERIOR removed - "
                      "the observable is not reading the round's reachable region set, so leg 6's TRUE answer proves "
                      "nothing");
            CE_ASSERT(closed.reachedHosts > 0, 12,
                      "the round reached no hosts at all with one town edge removed - the surgery broke more than the "
                      "crossing and 12a is measuring the wrong thing");
        }
        // And it comes back, so 12a did not permanently rewrite the graph.
        {
            const RoundObservation reopened = RunRound({});
            CE_ASSERT(reopened.crossingOpen == 1, 12,
                      "the crossing did not reopen after 12a's guard put RR_CLOCK_TOWN_SOUTH's exits back");
        }

        // ---- 12b: goalReached goes TRUE on both conjuncts, and only then.
        // MM_GOAL is `RR_MOON_MAJORAS_LAIR reachable AND CanDefeatMajora()`
        // (Logic.h:966-968). The lair's own reachability stays exactly where MM
        // authored it, so the leg does not touch Moon.cpp's gate: it gives the
        // ARRIVAL region a synthetic one-way exit to ENTRANCE(MAJORAS_LAIR, 0),
        // which `GetRegionIdFromEntrance` already resolves to the lair (it is the
        // lair's registered oneWayEntrance), and whose condition the leg controls
        // through the save. Regions.size() is unchanged, so the #659
        // entrance->region cache is not disturbed.
        {
            RegionExitsGuard southGuard(RR_CLOCK_TOWN_SOUTH);
            Rando::Logic::Regions[RR_CLOCK_TOWN_SOUTH].exits[ENTRANCE(MAJORAS_LAIR, 0)] =
                Rando::Logic::RandoRegionExit{ ONE_WAY_EXIT, [] { return HAS_MAGIC; },
                                               std::string("synthetic (mm-combo-logic-engine leg 12)") };
            // A SECOND synthetic edge on the same gate, to RR_MOON_ZORA_TRIAL,
            // whose RC_MOON_TRIAL_ZORA_PIECE_OF_HEART is CHECK(..., true) and is
            // not one of the unconditionally excluded rows. 12c needs it: the
            // lair's own two pots are SCENE_LAST_BS and therefore NOT hosts, so a
            // shrink that loses only the lair moves no HOST count — and the host
            // count (with the crossing flag) is the only observable
            // combo_logic.c's monotone detector watches. Measured on this tree
            // without this edge: the lair left the reported set, the goal fell to
            // 0, and reachedHosts stayed at 444, i.e. ERR_NON_MONOTONE could not
            // have fired. With it, the same flip also loses a real host.
            Rando::Logic::Regions[RR_CLOCK_TOWN_SOUTH].exits[ENTRANCE(MOON_ZORA_TRIAL, 0)] =
                Rando::Logic::RandoRegionExit{ ONE_WAY_EXIT, [] { return HAS_MAGIC; },
                                               std::string("synthetic (mm-combo-logic-engine leg 12)") };
            CE_ASSERT(Rando::Logic::GetRegionIdFromEntrance(ENTRANCE(MAJORAS_LAIR, 0)) == RR_MOON_MAJORAS_LAIR, 12,
                      "ENTRANCE(MAJORAS_LAIR, 0) no longer resolves to RR_MOON_MAJORAS_LAIR - 12b's synthetic edge "
                      "would lead somewhere else and prove nothing");
            CE_ASSERT(Rando::Logic::GetRegionIdFromEntrance(ENTRANCE(MOON_ZORA_TRIAL, 0)) == RR_MOON_ZORA_TRIAL, 12,
                      "ENTRANCE(MOON_ZORA_TRIAL, 0) no longer resolves to RR_MOON_ZORA_TRIAL - 12c's host-count "
                      "shrink would have nothing to lose");

            // The DEFEAT conjunct, as mm_majora_goal_test.cpp establishes it: a
            // Kokiri sword in human form is enough. Asserted directly first, so a
            // FALSE goal below cannot be blamed on a kit that never satisfied it.
            gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
            SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_KOKIRI);
            CE_ASSERT(Rando::Logic::CanDefeatMajora(), 12,
                      "CanDefeatMajora() is false with a Kokiri sword in human form - mm_majora_goal_test.cpp says it "
                      "is true, so 12b's kit no longer satisfies the goal's second conjunct");

            // LAIR REACHABLE + DEFEATABLE -> the goal is TRUE. This is the
            // observation leg 6 cannot make.
            gSaveContext.save.saveInfo.playerData.isMagicAcquired = true;
            const RoundObservation reachable = RunRound({});
            CE_ASSERT(reachable.beganOk != 0, 12, "beginQuery refused in 12b");
            printf("[TEST] mm-combo-logic-engine: leg 12b: lair reachable + Majora-capable -> goal=%d "
                   "(reachedHosts=%d)\n",
                   reachable.goalReached, reachable.reachedHosts);
            CE_ASSERT(reachable.goalReached == 1, 12,
                      "goalReached is FALSE with RR_MOON_MAJORAS_LAIR reachable AND CanDefeatMajora() true - "
                      "GoalReached is not handing MmGoalMajoraDefeated the round's real region set, so every FALSE it "
                      "has ever reported is uninformative");

            // SECOND CONJUNCT ALONE IS NOT ENOUGH: lair reachable, no weapon.
            SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_NONE);
            const bool defeatableWithoutKit = Rando::Logic::CanDefeatMajora();
            if (!defeatableWithoutKit) {
                const RoundObservation noKit = RunRound({});
                CE_ASSERT(noKit.goalReached == 0, 12,
                          "goalReached is TRUE with the lair reachable but Majora not defeatable - the second "
                          "conjunct is not being evaluated");
            }
            SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_KOKIRI);

            // ---- 12c: THE SHRINK DETECTOR, OBSERVED RED. A5's correction says a
            // recompute that comes back smaller is REPORTED, not unioned away, so
            // the coordinator's own monotone detector can see it. Inside one
            // bracket: open the synthetic edge, expand, then close it and expand
            // again. The engine must count the shrink AND report the smaller
            // answer (the goal goes back to FALSE). With the union restored, the
            // count still rises but the goal stays TRUE — which is precisely the
            // silent over-approximation.
            MM_ComboLogic_ResetCounters();
            CE_ASSERT(gEngine->snapshot(gEngine->self) != 0, 12, "snapshot refused in 12c");
            CE_ASSERT(gEngine->beginQuery(gEngine->self) != 0, 12, "beginQuery refused in 12c");
            gSaveContext.save.saveInfo.playerData.isMagicAcquired = true;
            (void)gEngine->expand(gEngine->self);
            const int goalWide = gEngine->goalReached(gEngine->self) ? 1 : 0;
            const int hostsWide = gEngine->reachedEmptyHosts(gEngine->self, nullptr, 0);
            CE_ASSERT(goalWide == 1, 12, "12c's wide state did not reach the goal - the sub-leg has nothing to shrink");

            gSaveContext.save.saveInfo.playerData.isMagicAcquired = false;
            (void)gEngine->expand(gEngine->self);
            const int goalNarrow = gEngine->goalReached(gEngine->self) ? 1 : 0;
            const int hostsNarrow = gEngine->reachedEmptyHosts(gEngine->self, nullptr, 0);
            const int shrinks = MM_ComboLogic_ShrinkObservations();
            gEngine->restore(gEngine->self);
            gEngine->endQuery(gEngine->self);

            printf("[TEST] mm-combo-logic-engine: leg 12c: wide goal=%d hosts=%d -> narrow goal=%d hosts=%d, "
                   "shrinkObservations=%d\n",
                   goalWide, hostsWide, goalNarrow, hostsNarrow, shrinks);
            CE_ASSERT(shrinks >= 1, 12,
                      "a recompute that lost regions and checks was not counted as a shrink - the detector behind "
                      "MM_ComboLogic_ShrinkObservations does not fire, so leg 8's `== shrinksBefore` proves nothing");
            CE_ASSERT(goalNarrow == 0, 12,
                      "the round still reports the GOAL as reached after a recompute in which the lair became "
                      "unreachable - the engine is unioning the shrunk answer away, which is the silent "
                      "over-approximation A5 forbids (the coordinator's own monotone detector would stay green while "
                      "the fill proves reachability that does not hold)");
            CE_ASSERT(hostsNarrow < hostsWide, 12,
                      "the reported reached-host count did not fall across a shrink - that count is the observable "
                      "combo_logic.c's monotone detector watches, so ERR_NON_MONOTONE could never fire");
        }
        MM_ComboLogic_ResetCounters();
    }

    // ---- Leg 13: expand HARVESTS the coordinator's own-origin placements -----
    //
    // ADDED IN REVIEW. combo_logic.h:441-445 and combo_logic.c:437-440 both put
    // this job on `expand`: `place` may not grant, the exchange deliberately skips
    // own-origin items, and the reason given is that harvesting from a reached
    // check in its OWN game is the engine's own `expand` — which is what OoT's
    // ReachabilitySearch -> AddCheckToLogic -> ApplyPlacedItemEffect does. MM's
    // `Expand` did not, so a round could only ever prove MM's goal from the
    // starting inventory and MM's candidate set SHRANK as the fill placed.
    //
    // THE MEASUREMENT, with both numbers taken in this row so neither half is
    // argued: placing the nine save-only items on nine REACHED hosts must give
    // the same reachability as ASSUMING them (less the nine hosts that are now
    // occupied), while placing them on nine UNREACHED hosts must give the bare
    // answer. An engine that never harvests produces the bare answer in both.
    {
        PlacementsGuard placementsGuard;
        auto leg13Reference = std::make_unique<SaveContext>();
        memcpy(leg13Reference.get(), &gSaveContext, sizeof(SaveContext));
        CE_ASSERT(MM_ComboLogic_HeldPlacementCount() == 0, 13, "leg 13 started with placements already held");

        const RoundObservation baseBare = RunRound({});
        const RoundObservation assumedNine = RunRound(kSaveOnlyGrants);
        CE_ASSERT(baseBare.beganOk != 0 && assumedNine.beganOk != 0, 13, "beginQuery refused in leg 13's baselines");
        CE_ASSERT(assumedNine.reachedHosts > baseBare.reachedHosts, 13,
                  "assuming the nine save-only items did not widen reachability here, so leg 13 has no gap to "
                  "attribute to the harvest");

        const std::vector<uint16_t> reachedHosts = ReachedHostList();
        const std::vector<uint16_t> allHosts = AllHostList();
        CE_ASSERT(reachedHosts.size() >= kSaveOnlyGrants.size(), 13, "fewer reached hosts than items to place");
        std::vector<uint16_t> unreachedHosts;
        {
            const std::set<uint16_t> reachedSet(reachedHosts.begin(), reachedHosts.end());
            for (uint16_t h : allHosts) {
                if (reachedSet.find(h) == reachedSet.end()) {
                    unreachedHosts.push_back(h);
                }
            }
        }
        CE_ASSERT(unreachedHosts.size() >= kSaveOnlyGrants.size(), 13,
                  "fewer than nine UNREACHED hosts exist - leg 13 cannot measure the no-harvest number");

        const int nine = (int)kSaveOnlyGrants.size();

        // ---- 13a: on REACHED hosts, the harvest must reproduce the assume.
        {
            const int harvestsBefore = MM_ComboLogic_HarvestCount();
            for (int i = 0; i < nine; ++i) {
                SharedItem it;
                it.originGame = (uint8_t)GAME_MM;
                it.flags = 0;
                it.id = kSaveOnlyGrants[(size_t)i];
                CE_ASSERT(gEngine->place(gEngine->self, reachedHosts[(size_t)i], it) != 0, 13,
                          "place refused an MM-origin item on a host the engine had itself offered as reached");
            }
            const RoundObservation placedReached = RunRound({});
            const int harvested = MM_ComboLogic_HarvestCount() - harvestsBefore;
            printf("[TEST] mm-combo-logic-engine: leg 13a: bare=%d assumed9=%d placedOnReached=%d (expect %d) "
                   "harvested=%d\n",
                   baseBare.reachedHosts, assumedNine.reachedHosts, placedReached.reachedHosts,
                   assumedNine.reachedHosts - nine, harvested);
            CE_ASSERT(placedReached.beganOk != 0, 13, "beginQuery refused in 13a");
            CE_ASSERT(harvested >= nine, 13,
                      "expand harvested fewer than the nine MM-origin items the coordinator placed on REACHED hosts - "
                      "the harvest the K1 contract assigns to `expand` is not running");
            CE_ASSERT(placedReached.reachedHosts == assumedNine.reachedHosts - nine, 13,
                      "a round with the nine items PLACED on reached hosts did not reach what the same nine items "
                      "ASSUMED reach (less the nine now-occupied hosts) - the own-origin harvest is missing or "
                      "partial, so MM's half of the goal is unprovable in every fill and its candidate set shrinks "
                      "as the fill places");
            gEngine->clearPlacements(gEngine->self);
        }

        // ---- 13b: on UNREACHED hosts, nothing is harvested. THIS IS THE RED HALF
        // of 13a, measured rather than argued: it is the number 13a would have
        // produced with no harvest at all.
        {
            const int harvestsBefore = MM_ComboLogic_HarvestCount();
            for (int i = 0; i < nine; ++i) {
                SharedItem it;
                it.originGame = (uint8_t)GAME_MM;
                it.flags = 0;
                it.id = kSaveOnlyGrants[(size_t)i];
                CE_ASSERT(gEngine->place(gEngine->self, unreachedHosts[(size_t)i], it) != 0, 13,
                          "place refused an MM-origin item on an unreached host");
            }
            const RoundObservation placedUnreached = RunRound({});
            const int harvested = MM_ComboLogic_HarvestCount() - harvestsBefore;
            printf("[TEST] mm-combo-logic-engine: leg 13b: placedOnUnreached=%d (bare=%d) harvested=%d\n",
                   placedUnreached.reachedHosts, baseBare.reachedHosts, harvested);
            CE_ASSERT(harvested == 0, 13,
                      "expand harvested an item from a host it never reached - the harvest is not gated on "
                      "reachability, so the fill would credit items the player cannot get to");
            CE_ASSERT(placedUnreached.reachedHosts == baseBare.reachedHosts, 13,
                      "placing nine items on UNREACHED hosts changed reachability - either the harvest ignored "
                      "reachedness, or `place` itself is moving the crawl");
            gEngine->clearPlacements(gEngine->self);
        }

        // ---- 13c: a FOREIGN placement is NOT harvested here. Crossing exchange
        // is the coordinator's job (combo_logic.c's ComboLogicExchangeFrom), and
        // the host physically holds RI_JUNK, so there is nothing for MM to credit.
        {
            const int harvestsBefore = MM_ComboLogic_HarvestCount();
            SharedItem foreign;
            foreign.originGame = (uint8_t)GAME_OOT;
            foreign.flags = 0;
            foreign.id = 0x1234;
            CE_ASSERT(gEngine->place(gEngine->self, reachedHosts[0], foreign) != 0, 13,
                      "place refused an OoT-origin item on a reached host");
            const RoundObservation placedForeign = RunRound({});
            CE_ASSERT(MM_ComboLogic_HarvestCount() == harvestsBefore, 13,
                      "expand harvested a FOREIGN placement - MM cannot interpret an OoT id, and the crossing "
                      "exchange is the coordinator's call, not the engine's");
            CE_ASSERT(placedForeign.reachedHosts == baseBare.reachedHosts - 1, 13,
                      "a single foreign placement on a reached host did not simply remove that host from the reached "
                      "enumeration");
            gEngine->clearPlacements(gEngine->self);
        }

        CE_ASSERT(MM_ComboLogic_HeldPlacementCount() == 0, 13, "leg 13 left placements behind");
        CE_ASSERT(memcmp(leg13Reference.get(), &gSaveContext, sizeof(SaveContext)) == 0, 13,
                  "leg 13 did not leave gSaveContext as it found it - a harvest escaped the round bracket, or "
                  "clearPlacements did not put a host's prior contents back");
    }

    // ---- Leg 14: the two always-shuffled SHOP checks are real hosts ---------
    //
    // A3's correction has teeth beyond the enumeration asserted in leg 3:
    // `IsUnconditionallyExcluded` also gates `place`, so while shops were treated
    // as unconditional, `place` returned 0 — i.e.
    // RSBS_COMBO_LOGIC_ERR_ENGINE_REFUSED (combo_logic.h:141-145) — on two checks
    // that are in MM's real `checkPool` in every settings configuration, and
    // `MM_ComboLogic_SetHostPool` dropped them silently, so increment 4's seam
    // could not hand MM's pool in intact.
    {
        PlacementsGuard placementsGuard;
        const uint16_t shopHosts[2] = { (uint16_t)RC_CURIOSITY_SHOP_SPECIAL_ITEM,
                                        (uint16_t)RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM };
        auto reference = std::make_unique<SaveContext>();
        memcpy(reference.get(), &gSaveContext, sizeof(SaveContext));

        SharedItem mmItem;
        mmItem.originGame = (uint8_t)GAME_MM;
        mmItem.flags = 0;
        mmItem.id = (uint16_t)RI_MASK_BUNNY;
        for (uint16_t host : shopHosts) {
            CE_ASSERT(gEngine->place(gEngine->self, host, mmItem) != 0, 14,
                      "place refused an always-shuffled SHOP check as a host - GeneratePools puts both of these in "
                      "checkPool under every setting, so a fill that hands MM's real pool to the coordinator would "
                      "die with RSBS_COMBO_LOGIC_ERR_ENGINE_REFUSED");
        }

        // And the pool seam keeps them, instead of dropping them on the floor.
        MM_ComboLogic_SetHostPool(shopHosts, 2);
        const int poolTotal = gEngine->allEmptyHosts(gEngine->self, nullptr, 0);
        MM_ComboLogic_SetHostPool(nullptr, 0);
        gEngine->clearPlacements(gEngine->self);
        // Both are HELD at the point the pool was read, so the enumeration is
        // empty; what matters is that SetHostPool did not silently discard them,
        // which the successful places above already show. Re-read with nothing
        // held.
        MM_ComboLogic_SetHostPool(shopHosts, 2);
        const int poolTotalFree = gEngine->allEmptyHosts(gEngine->self, nullptr, 0);
        MM_ComboLogic_SetHostPool(nullptr, 0);
        CE_ASSERT(poolTotal == 0, 14, "a host pool of two HELD checks should enumerate nothing");
        CE_ASSERT(poolTotalFree == 2, 14,
                  "MM_ComboLogic_SetHostPool silently dropped the two always-shuffled shop checks - the increment-4 "
                  "seam cannot hand MM's real pool in intact");
        CE_ASSERT(memcmp(reference.get(), &gSaveContext, sizeof(SaveContext)) == 0, 14,
                  "leg 14 did not leave gSaveContext as it found it");
    }

    return 0;
}

} // namespace

extern "C" int MM_ComboLogicEngine_RunHeadless(void) {
    printf("[TEST] mm-combo-logic-engine: MM's ComboLogicEngine answers from the real region graph, brackets the "
           "live save byte-exactly, is monotone under assume and agrees with itself (#645, ADR 0010 inc. 3)\n");

    auto ctx = Ship::Context::GetInstance();
    CE_ASSERT(ctx != nullptr, 11, "Ship::Context singleton missing - run the shared bring-up first");
    CE_ASSERT(ctx->GetConsoleVariables() != nullptr, 11,
              "ConsoleVariables missing - MM_Rando_InitCore's registrars read them");

    // Outer bracket: --test all shares one process and every leg writes
    // gSaveContext. Heap, not a static: the port's SaveContext is large.
    auto saved = std::make_unique<SaveContext>();
    memcpy(saved.get(), &gSaveContext, sizeof(SaveContext));
    const size_t savedDepth = MM_GameEvents_Queue().size();

    int rc = RunLegs();

    if (gEngine != nullptr) {
        gEngine->clearPlacements(gEngine->self);
    }
    MM_ComboLogic_SetHostPool(nullptr, 0);
    memcpy(&gSaveContext, saved.get(), sizeof(SaveContext));
    if (MM_GameEvents_Queue().size() > savedDepth) {
        MM_GameEvents_Queue().resize(savedDepth);
    }

    if (rc == 0) {
        printf("[TEST] mm-combo-logic-engine: PASS (registered engine, whole-struct + queue-depth bracket, "
               "monotone under assume, identical rounds agree before and after an unrelated crawl, junk cover "
               "keeps OoT ids out of MM's tables)\n");
    }
    return rc;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
