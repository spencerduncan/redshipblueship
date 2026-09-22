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
 * round which leaks state cannot make the byte-exactness claim pass.
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
void MM_ComboLogic_BracketCounters(int* outBeginRefusals, int* outEndCalls, int* outRedundantEnds);
void MM_ComboLogic_ResetCounters(void);
int MM_ComboLogic_SnapshotLive(void);
int MM_ComboLogic_HeldPlacementCount(void);
}

namespace {

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
    // and over the game-events queue depth, which is state OUTSIDE the save.
    {
        auto reference = std::make_unique<SaveContext>();
        memcpy(reference.get(), &gSaveContext, sizeof(SaveContext));
        const size_t referenceDepth = MM_GameEvents_Queue().size();

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
        CE_ASSERT(memcmp(reference.get(), &gSaveContext, sizeof(SaveContext)) != 0, 4,
                  "the mutation leg did not actually change gSaveContext - leg 4 would pass vacuously");
        CE_ASSERT(MM_GameEvents_Queue().size() == referenceDepth + 1, 4,
                  "the queue push did not take - the queue-depth half of leg 4 would pass vacuously");

        gEngine->restore(gEngine->self);
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

        // Take the graph away, exactly as an ordering mistake would.
        std::map<RandoRegionId, Rando::Logic::RandoRegion> parked;
        parked.swap(Rando::Logic::Regions);
        CE_ASSERT(Rando::Logic::Regions.empty(), 5, "the graph could not be parked - leg 5 would be vacuous");

        CE_ASSERT(gEngine->snapshot(gEngine->self) != 0, 5, "snapshot refused with no graph (it must not care)");
        const int began = gEngine->beginQuery(gEngine->self);
        CE_ASSERT(began == 0, 5,
                  "beginQuery ACCEPTED with an empty region graph - every later answer would be 'nothing is "
                  "reachable', which is indistinguishable from a world in which nothing is");

        // The coordinator's teardown, on a bracket that never opened.
        gEngine->restore(gEngine->self);
        gEngine->endQuery(gEngine->self);
        gEngine->endQuery(gEngine->self); // and twice

        Rando::Logic::Regions.swap(parked);
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

    // ---- Leg 6: a real round, and the two facts that make it non-vacuous ----
    //
    // Both are concrete claims about MM's authored graph rather than about this
    // engine's plumbing, so a bug that made every answer constant fails here:
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

    // ---- Leg 7: IDENTICAL QUERIES AGREE, including after an unrelated crawl --
    //
    // The OoT engine's version of this leg is the residue bug's direct lock; MM's
    // hazard is the same shape from the other side. CrawlReachableRegions ZEROES
    // and re-fires RANDO_EVENTS in the live save, so an unbracketed crawl between
    // two rounds is exactly the state leak that would make the second round
    // answer a different question.
    {
        const RoundObservation again = RunRound({});
        CE_ASSERT(again.crossingOpen == bare.crossingOpen && again.goalReached == bare.goalReached &&
                      again.reachedHosts == bare.reachedHosts && again.allHosts == bare.allHosts &&
                      again.iterations == bare.iterations,
                  7, "two back-to-back identical rounds gave different answers");
        CE_ASSERT(again.reachedSample == bare.reachedSample, 7,
                  "two back-to-back identical rounds reported the same NUMBER of hosts but different host IDS");

        // An unrelated crawl, outside any bracket, the way the check tracker
        // would run one.
        const Rando::Logic::ReachabilityCrawl unrelated =
            Rando::Logic::CrawlReachableRegions(gSaveContext.save.entrance);
        (void)Rando::Logic::EvaluateReachableChecks(unrelated);

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
