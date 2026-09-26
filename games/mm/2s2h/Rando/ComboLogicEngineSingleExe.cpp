/**
 * ComboLogicEngineSingleExe.cpp — MM's implementation of the combo-logic
 * coordinator's engine surface (ADR 0010 increment 3, #645; lane K2b).
 *
 * The MM twin of the OoT export TU beside
 * soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp. It is one of the MM
 * translation units where `RI_*` / `RC_*` / `RR_*` may be in scope for
 * cross-game purposes, because everything leaves it as a game-neutral scalar —
 * an origin-tagged `SharedItem`, a HOST CHECK ID in MM's own id-space, or a
 * boolean (ADR 0002, the #356 bug class). `src/common/combo_logic.h` names not
 * one MM enumerator and this file registers itself through that header's
 * vtable, so the coordinator never learns that an MM id means anything.
 *
 * Lives in 2s2h/Rando/, glob-collected into `2ship_rando`, which links
 * WHOLE_ARCHIVE (games/mm/CMakeLists.txt) — so this TU and its file-scope
 * registrar survive the link with no CMake edit. That is the property the
 * `2ship_enh` elision class (#516 / #678) is about, and it is asserted at
 * runtime by the MMComboLogicEngine ctest row: its first leg reads
 * Combo_Logic_GetEngine(GAME_MM) and fails if the registrar never ran.
 *
 * ============================================================================
 * NOTHING HERE RUNS IN A PRODUCTION PATH
 * ============================================================================
 *
 * Registration publishes a vtable. It calls nothing, and no shipping caller
 * invokes Combo_Logic_RunRound or Combo_Logic_RunFill at this commit — the
 * coordinator's own header says so, and its "deliberately absent" list names the
 * creation seam as a later increment. So every function below runs only when a
 * test drives it. In particular no generated world can move, because no world
 * generation path reaches any of this code. The PR states the comparison that
 * was actually run rather than resting on that argument.
 *
 * ============================================================================
 * MM'S ROUND IS A SAVE MUTATION, WHICH IS WHY snapshot/restore IS NOT OPTIONAL
 * ============================================================================
 *
 * MM has no detached simulated save. Its world state IS `gSaveContext`: every
 * guard macro in Logic.h dereferences it, `CrawlReachableRegions` ZEROES and
 * re-fires `RANDO_EVENTS` inside it, and "assume the player has item X" can only
 * be expressed by GIVING X into it (audit §2.1, §2.3, §4.2). So this engine
 * supplies the OPTIONAL snapshot/restore pair, and the coordinator brackets the
 * whole round with it (audit §4.4 / the header's amendment 1):
 *
 *   - `snapshot` memcpy's the whole `SaveContext` to the HEAP — not a static —
 *     because the port's struct is large and this can be called from arbitrary
 *     harness stack depths, exactly as `Logic.cpp`'s closure argues;
 *   - plus the `MM_GameEvents_Queue()` DEPTH, which is state outside the save.
 *     `GiveItem`'s triforce-completion branch emplaces a transition into that
 *     queue (GiveItem.cpp), and a simulated completion must not leak a queued
 *     warp into gameplay;
 *   - plus `Rando::Logic::gCurrentRegionTime` (ADDED IN REVIEW). It is a
 *     `thread_local uint64_t` declared at Logic.h:105 and defined at
 *     Logic.cpp:17 — state OUTSIDE the save that both halves of a crawl write:
 *     `FindReachableRegions` assigns it per region, and
 *     `EvaluateReachableChecks` re-assigns it through `SetCurrentRegionTime`
 *     (Logic.h:121-124). So every `expand` leaves it holding whichever region
 *     the crawl finished on, and the lock's whole-struct memcmp cannot see that.
 *     The precedent for bracketing it is already in this tree
 *     (mm_trick_bindings_test.cpp:573 and :803 save and restore it around their
 *     work). It is one word, and the contract says this pair "captures
 *     everything this engine's queries will mutate" — so it is in the pair
 *     rather than named as an exception;
 *   - `restore` memcpy's back, truncates the queue to that depth, and puts
 *     `gCurrentRegionTime` back.
 *
 * Both are idempotent and both are safe out of order, because the coordinator's
 * teardown is allowed to run after a bracket that never opened (see `EndQuery`).
 *
 * ============================================================================
 * WHERE THE K1 CONTRACT WAS AMBIGUOUS OR DOES NOT FIT MM — stated, not silently
 * reinterpreted
 * ============================================================================
 *
 * (A1) `beginQuery` is specified as "reset the simulated inventory to the
 *      WORLD'S STARTING INVENTORY and detach". MM HAS NO SUCH PRIMITIVE. There
 *      is no `Logic::Reset(true)` analogue and no detached save to reset; the
 *      only inventory MM has is the live one. This engine therefore treats THE
 *      SNAPSHOTTED LIVE SAVE as the round's starting state and resets only the
 *      per-round accumulators it owns (the reached sets and the granted-id set).
 *      That is correct for the seam increment 4 will use — at file creation the
 *      live save holds exactly the world's starting inventory, granted by
 *      `Rando::GrantStartingItems` — and it is WRONG if a caller ever opens a
 *      round on a mid-game save, where the round would silently assume every
 *      item the player already collected. Nothing can do that today (no caller
 *      exists), and the fix belongs in the contract: `beginQuery` needs to say
 *      whose job the starting state is. It is not this engine's, because MM
 *      cannot re-derive it.
 *      RESOLVED IN THE CONTRACT (ABI 3): combo_logic.h's `beginQuery` entry now
 *      assigns it — for an engine with no detached save, THE CALLER of
 *      RunRound/RunFill puts the live save in the file-creation starting state
 *      first (`MM_Sram_InitNewSave`, the frozen profile, `GrantStartingItems`),
 *      and a round opened on a mid-game save is a caller defect.
 *
 * (A2) MULTIPLICITY — RESOLVED BY THE CONTRACT (ABI 3, operator ruling
 *      2026-09-26). The ABI-2 contract said `assumeOwnItem` repeats "must be
 *      harmless" and, separately, that the same id could arrive from seeding and
 *      from an exchange; MM's counter gives (stray fairies, small keys, skull
 *      tokens, triforce pieces all `++` in GiveItem.cpp) made the two readings
 *      disagree, and this engine chose to DEDUP BY ID — sound, but a round then
 *      held one Woodfall key however many the bag carried, and no real world
 *      could be proved (#645, 2026-09-22).
 *      The contract now says exactly what a repeat is: ONE CALL IS ONE COPY, and
 *      the coordinator never delivers a copy twice (seeding covers the unplaced
 *      rows, the exchange covers each placement once per round). So this engine
 *      COUNTS EVERY CALL, and makes each item shape exact:
 *        - PROGRESSIVES self-clamp in MM's own give path: `ConvertItem` returns
 *          RI_JUNK once `IsItemObtainable` says the top tier is held
 *          (ConvertItem.cpp's RI_PROGRESSIVE_* arms), so a surplus copy is inert.
 *        - SET ITEMS are idempotent (`IsItemObtainable` -> RI_JUNK once held).
 *        - COUNTERS are the one shape MM does NOT bound — `IsItemObtainable`
 *          answers true for them unconditionally — so this engine clamps them
 *          at their MAXIMUM before giving (MmGiveOneCopy): small keys, stray
 *          fairies and skull tokens at the number of copies MM's own static check
 *          table holds as vanilla items (derived, never hard-coded; the lock
 *          prints them), triforce pieces at the seed's `RO_TRIFORCE_PIECES_MAX`.
 *          No MM logic term asks for more than the world contains, so the clamp
 *          never lowers an answer; it keeps a plentiful surplus from claiming a
 *          count the world cannot hold, or overflowing the counter's storage.
 *
 * (A3) `allEmptyHosts` is specified as "the engine's own shuffled-check table
 *      minus the hosts it already holds ... the same shape as asking a pool
 *      what is still in it". MM HAS NO SUCH TABLE OUTSIDE ITS OWN FILL.
 *      `GeneratePools` builds `checkPool` as a local vector and never persists
 *      it; before the fill runs, every pooled check already carries its VANILLA
 *      `randoItemId` (GeneratePools.cpp: "Initialize the check with it's
 *      vanilla item") and `shuffled` is still false — so neither field is an
 *      occupancy bit this engine could read. Worse, `GeneratePools` rolls shop
 *      and Tingle prices from `Ship_Random`, and the contract forbids a query
 *      from consuming any RNG the world generation consumes, so the engine may
 *      NOT call it to find out.
 *      The answer taken here: the host universe is MM'S REGION GRAPH'S OWN
 *      CHECK SET (every check id that appears in some `Rando::Logic::Regions`
 *      entry), minus the rows `GeneratePools` excludes UNCONDITIONALLY, and
 *      "already assigned" means "this coordinator placed here", which is the
 *      only occupancy this engine can honestly claim. Over-reporting is
 *      explicitly free (the coordinator filters against its own tables and is
 *      the sole occupancy authority), and the SETTINGS-CONDITIONAL narrowing —
 *      skulltulas off, owls off, pots off, shops off, user excludes — is
 *      deliberately NOT applied, because it belongs to whoever runs
 *      `GeneratePools` once before the coordinator. `MM_ComboLogic_SetHostPool`
 *      below is that seam, and it is also what lets a lock author a small pool
 *      with a known answer.
 *
 *      CORRECTED IN REVIEW — shops and Tingle shops are NOT unconditional.
 *      An earlier draft of this TU listed `RCTYPE_SHOP` and `RCTYPE_TINGLE_SHOP`
 *      among the rows "GeneratePools excludes UNCONDITIONALLY". That was false,
 *      and it made this paragraph's own claim false with it:
 *      `GeneratePools.cpp:116-118` skips Tingle shops only when
 *      `randoSaveOptions[RO_SHUFFLE_TINGLE_SHOPS] == RO_GENERIC_NO`, and
 *      `:126-131` skips shops only when `RO_SHUFFLE_SHOPS == RO_GENERIC_NO`
 *      AND the row is neither `RC_CURIOSITY_SHOP_SPECIAL_ITEM` nor
 *      `RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM` ("We always want shuffle"
 *      — those two are in MM's real `checkPool` in EVERY settings
 *      configuration). Excluding them here was therefore a settings-conditional
 *      NARROWING applied unconditionally, and it had two teeth: `place` shares
 *      this predicate, so it returned 0 — i.e.
 *      RSBS_COMBO_LOGIC_ERR_ENGINE_REFUSED — on the two always-shuffled shop
 *      checks, and `MM_ComboLogic_SetHostPool` silently dropped them, so the
 *      increment-4 seam could not hand MM's real pool in intact. Both rows are
 *      gone; shops stay in the universe and the pool seam decides.
 *      ONE CAVEAT that belongs to the coordinator rather than here: a FOREIGN
 *      cover on a shop host is a separate question, and MM already has a
 *      predicate for it — `Rando::IsEligibleHost` / `IsAllowedHostClass`
 *      (Foreign.cpp:460-467) rejects shop classes because "the shop give/price
 *      flow and spoiler shape differ from the ordinary eligible->CheckQueue path
 *      THE GENERIC FOREIGN PRESENTATION targets". That reason IS
 *      foreign-specific (an earlier draft of this header claimed it was not),
 *      so it cannot justify removing shops from the universe for MM-origin
 *      placement too. What the contract is missing is a PER-ORIGIN host
 *      predicate; until it has one, increment 4 must consult
 *      `Rando::IsEligibleHost` before choosing a shop host for a foreign item.
 *      That is stated in the PR as a contract gap, not worked around here.
 *
 * (A4) The host universe is LARGER THAN THE COORDINATOR'S SCRATCH BUFFER, and
 *      that is a fact about MM rather than a choice here: MM's graph names on
 *      the order of a thousand-plus checks against
 *      RSBS_COMBO_LOGIC_PLACEMENT_CAP = 1024. Both enumerators therefore honour
 *      the contract's truncation rule exactly — write at most `cap`, RETURN THE
 *      TOTAL — so a caller can tell truncation from exhaustion. The lock asserts
 *      the whole-graph total against that cap and prints both numbers, so a
 *      coordinator scratch buffer sized by the placement cap rather than by the
 *      id space is caught here rather than in a fill months later.
 *
 * (A5) `expand` must answer "did anything become reachable that was not
 *      reachable at the previous expand of this round". MM's crawl is a FULL
 *      RECOMPUTE from the arrival entrance; it does not carry an incremental
 *      frontier. This engine therefore recomputes and DIFFS against what the
 *      round last reported.
 *
 *      CORRECTED IN REVIEW — the round reports the LATEST RECOMPUTE, not a
 *      union. An earlier draft accumulated a union across the round's expands
 *      and, on a recompute that came back smaller, kept the union anyway while
 *      counting and logging the shrink. That was the silent over-approximation
 *      this note claimed to have avoided, and it was worse than trusting MM:
 *      the union keeps the candidate COUNT flat, which is exactly the observable
 *      the coordinator's own monotone detector watches
 *      (combo_logic.c:583-598), so a genuine non-monotonicity in MM's dialect
 *      would have left `RSBS_COMBO_LOGIC_ERR_NON_MONOTONE` unfired — a state
 *      combo_logic.h:112-116 says "is refused, never worked around" — while the
 *      engine went on reporting regions and checks as reachable that its own
 *      latest recompute says are not. Only the extern "C" counter would have
 *      known, and nothing outside this row reads it.
 *      Now: `sRound.regions` / `sRound.checks` / `sRound.regionTimeStates` are
 *      ASSIGNED from each recompute. A shrink is still counted and logged, and
 *      it now also SHOWS: the reported reached-host count falls, the crossing
 *      flag may fall, and the coordinator refuses the round. The unsound
 *      direction (claiming reachability that does not hold) is gone; the cost is
 *      that an MM-side non-monotonicity fails a fill instead of being papered
 *      over, which is what ADR 0010 §2.3 asks for.
 *      A side benefit: `regionTimeStates` and `regions` now always come from ONE
 *      crawl, so the crawl `goalReached` hands to `MmGoalMajoraDefeated` can no
 *      longer contain a region id absent from its time-state map — which
 *      `SetCurrentRegionTime`'s `.at()` (Logic.h:121-124) would throw on for any
 *      future goal term that consults time states.
 *
 * (A6) `expand` MUST HARVEST OWN-ORIGIN PLACEMENTS, and an earlier draft of this
 *      TU did not (added in review). The contract assigns that job to `expand`
 *      in two places: combo_logic.h:441-445 says `place` must not grant because
 *      "the coordinator decides when an item counts as held (`assumeOwnItem`,
 *      and the harvest of own-origin items that `expand` does when their host
 *      becomes reached)", and combo_logic.c:437-440 says own-origin placements
 *      are skipped by the exchange precisely because "harvesting an item from a
 *      reached check in its OWN game is the engine's own `expand` (that is what
 *      OoT's `ReachabilitySearch` already does)". OoT's side genuinely does it:
 *      ReachabilitySearch -> ProcessRegion -> AddCheckToLogic ->
 *      ApplyOrStoreItem -> `loc->ApplyPlacedItemEffect()`.
 *      Without it the round is not a reachability evaluation under the partial
 *      placement P at all: MM's candidate set would SHRINK as the fill places
 *      rather than hold steady, and the final round (assumed set empty) could
 *      only ever prove MM's goal from the starting inventory — so `goalMM` would
 *      be ~always 0, i.e. ERR_GOAL_UNPROVABLE under beat-both and MM's half
 *      silently unproved under beat-either.
 *      MM already had the primitive and it was unused: `ComputeReachableCheckSet`
 *      (Logic.cpp:408-456) is exactly crawl-then-`GiveItem(ConvertItem(placed))`
 *      to closure, with the same heap memcpy and queue-depth bracket this engine
 *      copied. `Expand` now runs that closure, restricted to hosts THIS
 *      COORDINATOR placed on (`sHeld`, `item.originGame == GAME_MM`) — MM's own
 *      vanilla and fill-assigned items are deliberately NOT harvested, because
 *      the coordinator's bag is the authority on what the pair's world holds and
 *      crediting MM's untouched vanilla contents would prove a world nobody
 *      authored. The harvest is PER PLACED HOST, once per round (the contract's
 *      `expand` rule): two hosts holding the same id are two copies and both are
 *      granted, through the same counter clamp as A2.
 *
 * ============================================================================
 * WHAT IS NOT PROVED HERE — read this before trusting the give path
 * ============================================================================
 *
 * Audit §6.3 lists as undetermined "whether any branch of
 * `GiveItem`/`ConvertItem` reachable only with foreign items present touches
 * state outside `gSaveContext`". It still is. Three branches of `GiveItem` are
 * known to reach outside the save, all of them read out of the source rather
 * than measured here:
 *   - the triforce-completion branch calls
 *     `GameInteractor_ExecuteOnGameCompletion()` and emplaces a
 *     `GIEventTransition` (the queue depth is snapshotted for exactly this
 *     reason, but the hook dispatch is NOT undone by any restore);
 *   - `RI_TRAP` calls `Rando::MiscBehavior::OfferTrapItem()`;
 *   - the default branch calls `MM_Item_Give(MM_gPlayState, ...)`, and
 *     `MM_gPlayState` is NULL outside gameplay, where `Item_GiveImpl` carries
 *     only PARTIAL null guards (the three unguarded legs are enumerated in
 *     ForeignItemsSingleExe.cpp's header).
 * The lock exercises save-only branches and names the ones it did not; the PR
 * repeats that list. Nothing here narrows the pool to a safe set, because this
 * engine does not own the pool — the bag does, and the bag is increment 4.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/Foreign.h" // Rando::Foreign::ResolvePairedProfile — measurement bridge only
#include "Rando/StaticData/StaticData.h"
#include "Rando/Logic/Logic.h"

extern "C" {
#include "variables.h"
#include "functions.h"
// `gRegEditor` / RegEditor: MM's give paths write REG slots through it and it is
// NULL until a real boot's Regs_Init allocates it. Needed only by
// MM_ComboLogic_ApplyShippedProfile's stand-in guard at the bottom of this file.
#include "regs.h"
// The MM-prefixed save initialiser, declared rather than reached for through an
// umbrella header — the same declaration mm_rando_gen_test.cpp makes, for the
// same reason. Used ONLY by MM_ComboLogic_ApplyShippedProfile at the bottom of
// this file; no vtable call touches it.
void MM_Sram_InitNewSave(void);
}

#include "mm_game_hooks.h" // MM_GameEvents_Queue()

// src/common. Included OUTSIDE any extern "C" block: these headers manage their
// own linkage and pull in <stdbool.h>/<stdint.h> (matching Foreign.cpp and
// ForeignItemsSingleExe.cpp).
#include "combo_logic.h"
#include "context.h"

namespace {

// ============================================================================
// The MM arrival entrance the round crawls from
// ============================================================================
//
// ENTRANCE(SOUTH_CLOCK_TOWN, 0) — 0xD800 — is the combo's MM arrival
// (src/common/entrance.h's OoT->MM link, audit §4.5) and also MM's own save-warp
// destination (the RR_MAX root's first exit, Logic.cpp). Pinned here rather than
// read from `gSaveContext.save.entrance` on purpose: a round is a question about
// the WORLD, and taking the arrival from the live save would make the answer
// depend on wherever the player happened to be standing when a later consumer
// opened the round. `GetRegionIdFromEntrance` resolves it to RR_CLOCK_TOWN_SOUTH.
constexpr s32 kMmArrivalEntrance = ENTRANCE(SOUTH_CLOCK_TOWN, 0);

// ============================================================================
// Round state — all of it owned by this engine, none of it in the save
// ============================================================================

struct RoundState {
    bool inRound = false;
    // The round's accumulated reachability. A union across the round's expands
    // (A5): MM's crawl is a full recompute, so "new since the previous expand"
    // is a diff rather than a frontier.
    std::set<RandoRegionId> regions;
    std::set<RandoCheckId> checks;
    // The joined time states of the most recent recompute, kept so `goalReached`
    // can hand a real ReachabilityCrawl to MM's own MmGoalMajoraDefeated instead
    // of restating its condition.
    std::unordered_map<RandoRegionId, Rando::Logic::RegionTimeState> regionTimeStates;
    // Hosts whose own-origin placement `expand` has already harvested this round
    // (A6): ONE grant per placed host per round, however many passes run.
    std::set<uint16_t> harvestedHosts;
    // Copies granted this round, by either path. APPEND-ONLY: nothing but
    // `beginQuery` resets it, and nothing takes a grant back out of the save
    // before the round's `restore` — this engine's half of the contract's "no
    // copy ever leaves a round".
    int copiesGranted = 0;
    int expands = 0;
};

RoundState sRound;

// The snapshot bracket. Heap, as Logic.cpp's closure argues.
std::unique_ptr<SaveContext> sSnapshot;
size_t sSnapshotQueueDepth = 0;
// Rando::Logic::gCurrentRegionTime — thread_local, outside the save, written by
// both halves of every crawl. See the bracket note in the file header.
uint64_t sSnapshotRegionTime = 0;
bool sSnapshotLive = false;

// Diagnostics the lock reads. Observable rather than internal because "did MM's
// reachability ever shrink inside a round?" (A5) and "was teardown reached with
// no round open?" are the questions this engine's contract turns on, and neither
// is answerable from outside the process.
int sShrinkObservations = 0;
int sBeginQueryRefusals = 0;
int sEndQueryCalls = 0;
int sRedundantEndQueries = 0;
// How many own-origin placed items `expand` has harvested (A6). Observable so
// the lock can assert the harvest actually fired rather than inferring it from a
// reachability number that could move for another reason.
int sHarvests = 0;

// ============================================================================
// The host universe (A3)
// ============================================================================

/** The graph's own check set, cached. Keyed on `Regions.size()` for the same
 *  #659 reason `GetRegionIdFromEntrance`'s cache is: eighteen independent
 *  ShipInit registrars populate `Regions` in link order, so a set built before
 *  they all ran must not be frozen for the life of the process. SIZE_MAX is
 *  "never built", which is distinct from "built from zero regions". */
std::vector<uint16_t> sGraphHosts;
size_t sGraphHostsBuiltAt = (size_t)-1;

/** The explicit pool, when a caller has supplied one (A3's seam). `sHostPoolSet
 *  == false` means "use the graph's check set". */
std::vector<uint16_t> sHostPool;
bool sHostPoolSet = false;

/** Rows `GeneratePools` skips with NO settings involved, so excluding them here
 *  states the same fact rather than a narrower one:
 *   - `RC_UNKNOWN`, an out-of-range id, or a row absent from the static table is
 *     not a check;
 *   - `SCENE_LAST_BS` is Majora's arena, whose two pots GeneratePools declines
 *     outright ("We may never shuffle these 2 pots"). They are also exactly the
 *     two checks mm_majora_goal_test.cpp pins as the lair's whole fill-visible
 *     surface, so offering them as hosts would contradict a live lock.
 *
 *  THAT IS THE WHOLE LIST. Shops and Tingle shops were here and are not
 *  `GeneratePools`-unconditional — see A3's correction in the file header for
 *  what that cost (`place` refusing the two always-shuffled shop checks, and
 *  `MM_ComboLogic_SetHostPool` silently dropping them). Anything settings-shaped
 *  belongs to the pool seam, not to this predicate, which also gates `place`. */
bool IsUnconditionallyExcluded(RandoCheckId randoCheckId) {
    if (randoCheckId == RC_UNKNOWN || (uint32_t)randoCheckId >= (uint32_t)RC_MAX) {
        return true;
    }
    const auto it = Rando::StaticData::Checks.find(randoCheckId);
    if (it == Rando::StaticData::Checks.end() || it->second.randoCheckId == RC_UNKNOWN) {
        return true;
    }
    if (it->second.sceneId == SCENE_LAST_BS) {
        return true;
    }
    return false;
}

const std::vector<uint16_t>& HostUniverse() {
    if (sHostPoolSet) {
        return sHostPool;
    }
    if (sGraphHostsBuiltAt != Rando::Logic::Regions.size()) {
        // std::set, hence ascending check id — which IS the stable order the
        // contract requires of both enumerators, and it is a function of the
        // graph alone rather than of anything a round did.
        std::set<uint16_t> unique;
        for (const auto& regionEntry : Rando::Logic::Regions) {
            for (const auto& checkEntry : regionEntry.second.checks) {
                if (!IsUnconditionallyExcluded(checkEntry.first)) {
                    unique.insert((uint16_t)checkEntry.first);
                }
            }
        }
        sGraphHosts.assign(unique.begin(), unique.end());
        sGraphHostsBuiltAt = Rando::Logic::Regions.size();
    }
    return sGraphHosts;
}

// ============================================================================
// Placements this coordinator gave us
// ============================================================================

/** One coordinator placement, plus what the save held before it, so
 *  `clearPlacements` puts back exactly what it found and nothing else. */
struct HeldPlacement {
    SharedItem item;
    RandoItemId priorItemId;
    bool priorShuffled;
    bool priorSkipped;
};

// std::map: ascending host order, so clearPlacements' restore order and the
// engine's occupancy view are both a function of the host ids alone.
std::map<uint16_t, HeldPlacement> sHeld;

bool SameItem(SharedItem a, SharedItem b) {
    return a.originGame == b.originGame && a.id == b.id;
}

// ============================================================================
// Item validation
// ============================================================================

/** Is `riId` a real, giveable MM item id? RI_UNKNOWN is enumerator 0 (a
 *  zero-initialised slot) and RI_NONE is "literally nothing"; both are declared
 *  RITYPE_JUNK, so a type-based test accepts them — the #488 sentinel trap. An
 *  id at or past RI_MAX would index Rando::StaticData::Items out of range. */
bool IsGiveableItemId(uint16_t riId) {
    return riId != (uint16_t)RI_UNKNOWN && riId != (uint16_t)RI_NONE && riId < (uint16_t)RI_MAX &&
           Rando::StaticData::Items.find((RandoItemId)riId) != Rando::StaticData::Items.end();
}

// ============================================================================
// Counter maxima (A2) — the one item shape MM's give path does not bound
// ============================================================================

/** Which counter an id advances. */
enum class MmCounterKind { None, SmallKey, StrayFairy, SkullToken, Triforce };

struct MmCounterInfo {
    MmCounterKind kind = MmCounterKind::None;
    int index = 0; // dungeon scene index (keys, fairies) or scene id (tokens)
};

MmCounterInfo MmCounterFor(uint16_t riId) {
    MmCounterInfo c;
    switch ((RandoItemId)riId) {
        case RI_WOODFALL_SMALL_KEY:
            c = { MmCounterKind::SmallKey, DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE };
            break;
        case RI_SNOWHEAD_SMALL_KEY:
            c = { MmCounterKind::SmallKey, DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE };
            break;
        case RI_GREAT_BAY_SMALL_KEY:
            c = { MmCounterKind::SmallKey, DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE };
            break;
        case RI_STONE_TOWER_SMALL_KEY:
            c = { MmCounterKind::SmallKey, DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE };
            break;
        // RI_CLOCK_TOWN_STRAY_FAIRY is a flag (SET_WEEKEVENTREG), not a counter.
        case RI_WOODFALL_STRAY_FAIRY:
            c = { MmCounterKind::StrayFairy, DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE };
            break;
        case RI_SNOWHEAD_STRAY_FAIRY:
            c = { MmCounterKind::StrayFairy, DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE };
            break;
        case RI_GREAT_BAY_STRAY_FAIRY:
            c = { MmCounterKind::StrayFairy, DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE };
            break;
        case RI_STONE_TOWER_STRAY_FAIRY:
            c = { MmCounterKind::StrayFairy, DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE };
            break;
        case RI_GS_TOKEN_SWAMP:
            c = { MmCounterKind::SkullToken, SCENE_KINSTA1 };
            break;
        case RI_GS_TOKEN_OCEAN:
            c = { MmCounterKind::SkullToken, SCENE_KINDAN2 };
            break;
        case RI_TRIFORCE_PIECE:
        case RI_TRIFORCE_PIECE_PREVIOUS:
            c = { MmCounterKind::Triforce, 0 };
            break;
        default:
            break;
    }
    return c;
}

/** The counter's value in the live save — what MM's own logic reads for it
 *  (KEY_COUNT reads `foundDungeonKeys`, Logic.h). */
int MmCounterValue(const MmCounterInfo& c) {
    switch (c.kind) {
        case MmCounterKind::SmallKey:
            return (int)gSaveContext.save.shipSaveInfo.rando.foundDungeonKeys[c.index];
        case MmCounterKind::StrayFairy:
            return (int)gSaveContext.save.saveInfo.inventory.strayFairies[c.index];
        case MmCounterKind::SkullToken:
            return (int)Inventory_GetSkullTokenCount((s16)c.index);
        case MmCounterKind::Triforce:
            return (int)gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces;
        default:
            return 0;
    }
}

/**
 * The counter's MAXIMUM. Keys, fairies and tokens: the number of copies of that
 * id MM's own static check table holds as VANILLA items — MM's own statement of
 * how many exist, derived rather than hard-coded, and cached (the table is
 * static). Triforce pieces: the seed's `RO_TRIFORCE_PIECES_MAX`, because the
 * piece count is a setting, not a property of the vanilla world.
 */
int MmCounterMax(uint16_t riId, const MmCounterInfo& c) {
    if (c.kind == MmCounterKind::None) {
        return -1;
    }
    if (c.kind == MmCounterKind::Triforce) {
        return (int)RANDO_SAVE_OPTIONS[RO_TRIFORCE_PIECES_MAX];
    }
    static std::map<uint16_t, int> sVanillaCopies;
    const auto hit = sVanillaCopies.find(riId);
    if (hit != sVanillaCopies.end()) {
        return hit->second;
    }
    int copies = 0;
    for (const auto& entry : Rando::StaticData::Checks) {
        if (entry.second.randoCheckId != RC_UNKNOWN && (uint16_t)entry.second.randoItemId == riId) {
            copies++;
        }
    }
    sVanillaCopies[riId] = copies;
    return copies;
}

int sCounterClamps = 0; // copies a clamp absorbed; the lock reads it

/**
 * GIVE ONE COPY. The single give path both `assumeOwnItem` and the A6 harvest
 * use, so the two sources of a copy cannot drift apart. A counter already at
 * its maximum absorbs the copy (counted in sCounterClamps); everything else goes
 * through MM's own `GiveItem(ConvertItem(...))`, whose progressive and set-item
 * arms bound themselves.
 */
void MmGiveOneCopy(uint16_t riId) {
    const MmCounterInfo c = MmCounterFor(riId);
    if (c.kind != MmCounterKind::None) {
        const int max = MmCounterMax(riId, c);
        if (max >= 0 && MmCounterValue(c) >= max) {
            sCounterClamps++;
            sRound.copiesGranted++;
            return;
        }
    }
    Rando::GiveItem(Rando::ConvertItem((RandoItemId)riId));
    sRound.copiesGranted++;
}

// ============================================================================
// The vtable
// ============================================================================

int Snapshot(void* self) {
    (void)self;
    if (sSnapshot == nullptr) {
        sSnapshot = std::make_unique<SaveContext>();
    }
    if (sSnapshot == nullptr) {
        fprintf(stderr, "[MM ComboLogic] snapshot: allocation failed\n");
        return 0;
    }
    memcpy(sSnapshot.get(), &gSaveContext, sizeof(SaveContext));
    sSnapshotQueueDepth = MM_GameEvents_Queue().size();
    sSnapshotRegionTime = Rando::Logic::gCurrentRegionTime;
    sSnapshotLive = true;
    return 1;
}

void Restore(void* self) {
    (void)self;
    if (!sSnapshotLive || sSnapshot == nullptr) {
        // A restore with no live snapshot is a no-op rather than a revert to
        // whatever the buffer last held. The coordinator's teardown runs in
        // reverse over BOTH sides and may run after a bracket that never opened,
        // so this has to be safe to reach twice and safe to reach never having
        // snapped.
        return;
    }
    memcpy(&gSaveContext, sSnapshot.get(), sizeof(SaveContext));
    if (MM_GameEvents_Queue().size() > sSnapshotQueueDepth) {
        MM_GameEvents_Queue().resize(sSnapshotQueueDepth);
    }
    Rando::Logic::gCurrentRegionTime = sSnapshotRegionTime;
    // The buffer stays allocated (it is reused every round) but stops being
    // LIVE, so a second restore cannot re-apply a stale save over newer state.
    sSnapshotLive = false;
}

int BeginQuery(void* self) {
    (void)self;
    // The one genuine refusal: with no region graph every later answer would be
    // "nothing is reachable", which is indistinguishable from a world in which
    // nothing is reachable. Refusing turns a bring-up ordering mistake into
    // RSBS_COMBO_LOGIC_ERR_ENGINE_REFUSED instead of a silently empty world.
    if (Rando::Logic::Regions.empty()) {
        fprintf(stderr, "[MM ComboLogic] beginQuery refused: Rando::Logic::Regions is empty (the registrars have not "
                        "run — MM_Rando_InitCore first)\n");
        sBeginQueryRefusals++;
        return 0;
    }
    // A1: MM has no starting-inventory reset. The round's starting state is the
    // save as snapshotted; all this engine resets is what it owns.
    sRound.inRound = true;
    sRound.regions.clear();
    sRound.checks.clear();
    sRound.regionTimeStates.clear();
    sRound.harvestedHosts.clear();
    sRound.copiesGranted = 0;
    sRound.expands = 0;
    return 1;
}

void AssumeOwnItem(void* self, uint16_t ownItemId) {
    (void)self;
    if (!IsGiveableItemId(ownItemId)) {
        // Not an error the contract lets us report (the call returns void), so it
        // is logged and dropped. A sentinel here means the bag carried one, which
        // is the caller's bug.
        fprintf(stderr, "[MM ComboLogic] assumeOwnItem: id %u is not a giveable RandoItemId — ignored\n",
                (unsigned)ownItemId);
        return;
    }
    // A2: ONE CALL IS ONE COPY. No dedup — the coordinator never delivers a
    // copy twice — and counters stop at their maximum inside MmGiveOneCopy.
    MmGiveOneCopy(ownItemId);
}

/**
 * One complete reachability evaluation under the coordinator's partial
 * placement (A5 + A6).
 *
 * TERMINATION. The outer loop runs another pass only when the harvest granted at
 * least one copy from a host it had never harvested before this round, and every
 * harvest inserts that host into `sRound.harvestedHosts`, which is bounded by
 * the placements this engine holds. So the number of passes is bounded by the
 * number of MM-origin placements, plus one. This is the same closure
 * `ComputeReachableCheckSet` runs (Logic.cpp:408-456) and terminates for the
 * same reason.
 */
int Expand(void* self) {
    (void)self;
    int changed = 0;
    bool grantedThisPass = true;

    while (grantedThisPass) {
        grantedThisPass = false;

        const Rando::Logic::ReachabilityCrawl crawl = Rando::Logic::CrawlReachableRegions(kMmArrivalEntrance);
        const std::set<RandoCheckId> reached = Rando::Logic::EvaluateReachableChecks(crawl);

        // --- did MM's answer SHRINK relative to what this round last reported?
        bool shrank = false;
        for (RandoRegionId regionId : sRound.regions) {
            if (crawl.reachableRegions.find(regionId) == crawl.reachableRegions.end()) {
                shrank = true;
                break;
            }
        }
        if (!shrank) {
            for (RandoCheckId randoCheckId : sRound.checks) {
                if (reached.find(randoCheckId) == reached.end()) {
                    shrank = true;
                    break;
                }
            }
        }
        if (shrank) {
            // A5: counted, logged, AND REPORTED. The assignment below is what
            // makes this visible to the coordinator's own monotone detector
            // (combo_logic.c:583-598) instead of being hidden behind a union
            // that keeps the candidate count flat.
            sShrinkObservations++;
            fprintf(stderr, "[MM ComboLogic] expand: a recompute came back SMALLER than this round's previous answer "
                            "— MM reachability is not monotone under the items granted (ADR 0010 §2.3). The smaller "
                            "set is what this round now reports.\n");
            changed = 1;
        }

        // --- did it GROW?
        if (!changed) {
            for (RandoRegionId regionId : crawl.reachableRegions) {
                if (sRound.regions.find(regionId) == sRound.regions.end()) {
                    changed = 1;
                    break;
                }
            }
        }
        if (!changed) {
            for (RandoCheckId randoCheckId : reached) {
                if (sRound.checks.find(randoCheckId) == sRound.checks.end()) {
                    changed = 1;
                    break;
                }
            }
        }

        // --- the round reports THIS recompute. Not a union (A5).
        sRound.regions = crawl.reachableRegions;
        sRound.checks = reached;
        sRound.regionTimeStates = crawl.regionTimeStates;

        // --- A6: harvest the coordinator's OWN-ORIGIN placements whose host is
        // now reached — ONCE PER HOST PER ROUND, so two hosts holding the same id
        // grant two copies and no host grants twice. MM's own vanilla and
        // fill-assigned check contents are NOT harvested: the coordinator's bag is
        // the authority on what this pair's world holds.
        for (const auto& entry : sHeld) {
            if (entry.second.item.originGame != (uint8_t)GAME_MM) {
                continue;
            }
            if (sRound.checks.find((RandoCheckId)entry.first) == sRound.checks.end()) {
                continue;
            }
            if (!IsGiveableItemId(entry.second.item.id)) {
                continue;
            }
            if (!sRound.harvestedHosts.insert(entry.first).second) {
                continue;
            }
            MmGiveOneCopy(entry.second.item.id);
            sHarvests++;
            grantedThisPass = true;
            changed = 1;
        }
    }

    sRound.expands++;
    return changed;
}

int CrossingOpen(void* self) {
    (void)self;
    // Audit §4.5: the MM->OoT crossing is an exit on RR_CLOCK_TOWER_INTERIOR, so
    // the observable is that region's reachability. A region FACT — no region
    // file changes for increment 3 (audit amendment 3). Pure read.
    return sRound.regions.find(RR_CLOCK_TOWER_INTERIOR) != sRound.regions.end() ? 1 : 0;
}

int CheckReached(void* self, uint16_t hostCheck) {
    (void)self;
    return sRound.checks.find((RandoCheckId)hostCheck) != sRound.checks.end() ? 1 : 0;
}

/** Shared body of the two enumerators. `reachedOnly` applies the reachability
 *  filter; both honour the contract's truncation rule (write at most `cap`,
 *  return the TOTAL) so a caller can distinguish truncation from exhaustion. */
int EnumerateHosts(uint16_t* out, int cap, bool reachedOnly) {
    int total = 0;
    for (uint16_t hostCheck : HostUniverse()) {
        if (sHeld.find(hostCheck) != sHeld.end()) {
            continue; // this coordinator already placed here (A3's occupancy)
        }
        if (reachedOnly && sRound.checks.find((RandoCheckId)hostCheck) == sRound.checks.end()) {
            continue;
        }
        if (out != nullptr && total < cap) {
            out[total] = hostCheck;
        }
        total++;
    }
    return total;
}

int ReachedEmptyHosts(void* self, uint16_t* out, int cap) {
    (void)self;
    return EnumerateHosts(out, cap, true);
}

int AllEmptyHosts(void* self, uint16_t* out, int cap) {
    (void)self;
    // Legal OUTSIDE a round: it consults the graph's check set and this engine's
    // own placement record, never the round's reached set or the save's
    // inventory.
    return EnumerateHosts(out, cap, false);
}

int GoalReached(void* self) {
    (void)self;
    // MM's own MM_GOAL, not a restatement of it: PR #690 authored
    // MmGoalMajoraDefeated as "RR_MOON_MAJORAS_LAIR reachable AND
    // CanDefeatMajora()" (ADR 0010 D1's two conjuncts), and lair reachability
    // stays exactly where MM authored it. The crawl handed over carries this
    // round's ACCUMULATED regions, which is the set the round's other answers
    // are about.
    Rando::Logic::ReachabilityCrawl crawl;
    crawl.reachableRegions = sRound.regions;
    crawl.regionTimeStates = sRound.regionTimeStates;
    return Rando::Logic::MmGoalMajoraDefeated(crawl) ? 1 : 0;
}

int Place(void* self, uint16_t hostCheck, SharedItem item) {
    (void)self;
    if (item.originGame != (uint8_t)GAME_OOT && item.originGame != (uint8_t)GAME_MM) {
        fprintf(stderr, "[MM ComboLogic] place: item carries no origin\n");
        return 0;
    }
    if (IsUnconditionallyExcluded((RandoCheckId)hostCheck)) {
        fprintf(stderr, "[MM ComboLogic] place: check %u is not a legal MM host\n", (unsigned)hostCheck);
        return 0;
    }

    // IDEMPOTENT for the same (host, item). The coordinator re-applies its whole
    // table after every restore, so this is a hard requirement rather than a
    // nicety — see the header's amendment 1.
    const auto existing = sHeld.find(hostCheck);
    if (existing != sHeld.end()) {
        if (!SameItem(existing->second.item, item)) {
            fprintf(stderr, "[MM ComboLogic] place: check %u already holds a different item (origin %u id %u)\n",
                    (unsigned)hostCheck, (unsigned)existing->second.item.originGame,
                    (unsigned)existing->second.item.id);
            return 0;
        }
        // Re-apply the save write: a restore has probably just undone it. The
        // PRIOR values recorded on the first place stay untouched, so a later
        // clearPlacements still puts back what the world originally held.
        RandoSaveCheck& reapply = RANDO_SAVE_CHECKS[hostCheck];
        reapply.randoItemId = (item.originGame == (uint8_t)GAME_MM) ? (RandoItemId)item.id : RI_JUNK;
        reapply.shuffled = true;
        return 1;
    }

    RandoSaveCheck& randoSaveCheck = RANDO_SAVE_CHECKS[hostCheck];
    HeldPlacement held;
    held.item = item;
    held.priorItemId = randoSaveCheck.randoItemId;
    held.priorShuffled = randoSaveCheck.shuffled;
    held.priorSkipped = randoSaveCheck.skipped;

    if (item.originGame == (uint8_t)GAME_MM) {
        if (!IsGiveableItemId(item.id)) {
            fprintf(stderr, "[MM ComboLogic] place: MM-origin id %u is not a real item\n", (unsigned)item.id);
            return 0;
        }
        // The fill's own write, verbatim (GlitchlessLogic.cpp): the check's
        // assigned item plus the shuffled bit. It does NOT grant — placing is not
        // holding, and the coordinator decides when an item counts as held.
        randoSaveCheck.randoItemId = (RandoItemId)item.id;
    } else {
        // JUNK COVER. A raw RG_* must never enter MM's tables (ADR 0002), so the
        // host physically holds the legal junk-class MM filler and the foreign
        // identity stays in the coordinator's table — which is also what the
        // check degrades to if that table is ever absent (Foreign.cpp's
        // documented degradation). RI_JUNK is the legal filler GeneratePools
        // itself injects; RI_NONE and RI_UNKNOWN are RITYPE_JUNK but are not
        // items, and are never used here.
        randoSaveCheck.randoItemId = RI_JUNK;
    }
    randoSaveCheck.shuffled = true;
    sHeld.emplace(hostCheck, held);
    return 1;
}

void ClearPlacements(void* self) {
    (void)self;
    // Ascending host order (std::map), restoring exactly the three fields
    // `place` overwrote. Nothing else is touched: not the graph, not a check this
    // coordinator never placed on, not MM's own restricted placements.
    for (const auto& entry : sHeld) {
        RandoSaveCheck& randoSaveCheck = RANDO_SAVE_CHECKS[entry.first];
        randoSaveCheck.randoItemId = entry.second.priorItemId;
        randoSaveCheck.shuffled = entry.second.priorShuffled;
        randoSaveCheck.skipped = entry.second.priorSkipped;
    }
    sHeld.clear();
}

void EndQuery(void* self) {
    (void)self;
    sEndQueryCalls++;
    if (!sRound.inRound) {
        // SAFE AFTER A FAILED beginQuery AND SAFE TWICE. The coordinator's
        // teardown is allowed to reach here on a bracket that never opened (a
        // snapshot taken, then beginQuery refused), and an engine that "restored
        // its coupling" unconditionally would undo state it never set. MM has no
        // live coupling to re-point — that obligation is OoT's
        // `Logic::mSaveContext` (audit §1.8 / §4.4) — so there is genuinely
        // nothing to do, and saying so is the whole content of this branch.
        sRedundantEndQueries++;
        return;
    }
    sRound.inRound = false;
    sRound.regions.clear();
    sRound.checks.clear();
    sRound.regionTimeStates.clear();
    sRound.harvestedHosts.clear();
    sRound.copiesGranted = 0;
}

const ComboLogicEngine kMmEngine = {
    /* abiVersion        */ RSBS_COMBO_LOGIC_ENGINE_ABI,
    /* self              */ nullptr,
    /* beginQuery        */ BeginQuery,
    /* assumeOwnItem     */ AssumeOwnItem,
    /* expand            */ Expand,
    /* crossingOpen      */ CrossingOpen,
    /* checkReached      */ CheckReached,
    /* reachedEmptyHosts */ ReachedEmptyHosts,
    /* allEmptyHosts     */ AllEmptyHosts,
    /* goalReached       */ GoalReached,
    /* place             */ Place,
    /* clearPlacements   */ ClearPlacements,
    /* endQuery          */ EndQuery,
    /* snapshot          */ Snapshot,
    /* restore           */ Restore,
};

/**
 * File-scope registrar, the same shape as kForeignPoolMMV1's in
 * ForeignItemsSingleExe.cpp. Registration STORES A POINTER and calls nothing, so
 * running it at static-initialisation time is safe in both directions: the
 * coordinator's registry is a zero-initialised static array with no dynamic
 * initialiser of its own, and none of the functions above runs until somebody
 * drives the vtable. It deliberately does NOT go through RegisterShipInitFunc —
 * this engine must be published whether or not MM's rando bring-up has run, so
 * that a caller who forgets the bring-up gets BeginQuery's loud refusal instead
 * of RSBS_COMBO_LOGIC_ERR_NO_ENGINE, which would read as "MM was not built in".
 */
struct ComboLogicEngineRegistrar {
    ComboLogicEngineRegistrar() {
        Combo_Logic_RegisterEngine(GAME_MM, &kMmEngine);
    }
};
const ComboLogicEngineRegistrar gComboLogicEngineRegistrar;

} // namespace

// ============================================================================
// The seam + diagnostics surface (extern "C", game-neutral scalars only)
// ============================================================================

/**
 * Supply the HOST POOL explicitly (A3). `checks` is an array of MM check ids in
 * the caller's own order; the engine re-sorts ascending and drops rows it would
 * never host on, because the contract makes host order a function of the
 * engine's table rather than of the caller. `checks == NULL` or `count <= 0`
 * restores the default (the region graph's whole check set).
 *
 * THIS IS THE INCREMENT-4 SEAM, and it exists because `GeneratePools` — the one
 * place that knows MM's real pool — rolls prices from `Ship_Random` and so may
 * never be called from inside a query. Whoever runs `GeneratePools` once before
 * the coordinator hands its `checkPool` in through here.
 */
extern "C" void MM_ComboLogic_SetHostPool(const uint16_t* checks, int count) {
    if (checks == nullptr || count <= 0) {
        sHostPool.clear();
        sHostPoolSet = false;
        return;
    }
    std::set<uint16_t> unique;
    for (int i = 0; i < count; ++i) {
        if (!IsUnconditionallyExcluded((RandoCheckId)checks[i])) {
            unique.insert(checks[i]);
        }
    }
    sHostPool.assign(unique.begin(), unique.end());
    sHostPoolSet = true;
}

/** How many times a recompute inside a round came back smaller than the round's
 *  accumulation (A5). Zero is the claim; anything else is a monotonicity
 *  violation in MM's own dialect, which ADR 0010 §2.3 says the fill may not
 *  survive. */
extern "C" int MM_ComboLogic_ShrinkObservations(void) {
    return sShrinkObservations;
}

/** How many own-origin placed items `expand` has harvested since the last
 *  counter reset (A6). Zero across a round in which the coordinator placed an
 *  MM-origin item on a REACHED host means the harvest is not running, which
 *  makes MM's half of the goal unprovable in every fill. */
extern "C" int MM_ComboLogic_HarvestCount(void) {
    return sHarvests;
}

/** `beginQuery` refusals, `endQuery` calls, and how many of those landed with no
 *  round open (the failed-bracket teardown path). Any pointer may be NULL. */
extern "C" void MM_ComboLogic_BracketCounters(int* outBeginRefusals, int* outEndCalls, int* outRedundantEnds) {
    if (outBeginRefusals != nullptr) {
        *outBeginRefusals = sBeginQueryRefusals;
    }
    if (outEndCalls != nullptr) {
        *outEndCalls = sEndQueryCalls;
    }
    if (outRedundantEnds != nullptr) {
        *outRedundantEnds = sRedundantEndQueries;
    }
}

/** Reset every counter above, for a lock that wants to measure one sequence. */
extern "C" void MM_ComboLogic_ResetCounters(void) {
    sShrinkObservations = 0;
    sBeginQueryRefusals = 0;
    sEndQueryCalls = 0;
    sRedundantEndQueries = 0;
    sHarvests = 0;
    sCounterClamps = 0;
}

/** Is a snapshot currently LIVE (taken and not yet restored)? The lock asserts
 *  the bracket is balanced; a stuck-live snapshot would mean a round left MM's
 *  save owned by this engine. */
extern "C" int MM_ComboLogic_SnapshotLive(void) {
    return sSnapshotLive ? 1 : 0;
}

/** How many placements this engine currently holds for the coordinator. */
extern "C" int MM_ComboLogic_HeldPlacementCount(void) {
    return (int)sHeld.size();
}

/**
 * READ-ONLY, TEST ONLY: this round's REGION SET — the regions of the latest
 * recompute (A5) in ascending id order, and each one's joined time slices.
 *
 * Why it exists (ADR 0010 answer O6, the `combo-logic-monotonicity` grow-check):
 * the vtable reports CHECKS (`checkReached`) but not regions, and O6's grow-check
 * asserts that neither the reached check set NOR the reached region set ever
 * shrinks as items are granted. A region can open, or gain time slices, without
 * any new check becoming reachable, so the check set alone would let a region-side
 * regression through. `sRound` is file-static, so this is the only way to read it;
 * it writes nothing and is called by nothing but that row.
 *
 * At most `cap` entries are written to each non-NULL array; the TOTAL is
 * returned (the enumerators' truncation contract). Zero outside a round.
 */
extern "C" int MM_ComboLogic_TestRoundRegions(uint16_t* outRegions, uint64_t* outTimeSlices, int cap) {
    int total = 0;
    for (const RandoRegionId regionId : sRound.regions) {
        if (total < cap) {
            if (outRegions != nullptr) {
                outRegions[total] = (uint16_t)regionId;
            }
            if (outTimeSlices != nullptr) {
                const auto it = sRound.regionTimeStates.find(regionId);
                outTimeSlices[total] = (it != sRound.regionTimeStates.end()) ? it->second.timeSlices : 0;
            }
        }
        total++;
    }
    return total;
}

// ============================================================================
// THE MEASUREMENT BRIDGES (#645 increment 3's first two work items, lane K3)
// ============================================================================
//
// Two read-only-ish accessors the measurement row in src/common cannot write for
// itself, because it has no MM enum in scope by design (ADR 0002). Neither is
// called by production and neither is called by the vtable above.
//
// ONE OF THEM CAN TOUCH `Ship_Random`, AND THE CLAIM IS QUALIFIED RATHER THAN
// ABSOLUTE. `MM_ComboLogic_PoolVanillaItems` consumes none — that is why it reads
// `Rando::StaticData::Checks` instead of calling `GeneratePools`, and the
// difference is listed as deviation (3) in its own doc. But
// `MM_ComboLogic_ApplyShippedProfile` calls `Rando::GrantStartingItems()`, and
// StartingItems.cpp:56-59 has one branch that both RE-SEEDS and draws the global
// stream: with `RO_CLOCK_SHUFFLE` == `RO_GENERIC_YES` and
// `RO_CLOCK_SHUFFLE_PROGRESSIVE` == `RO_CLOCK_SHUFFLE_RANDOM` it runs
// `Ship_Random_Seed(rando.finalSeed)` and then `Ship_Random(0, 5)` to pick a
// starting time item. So: on the SHIPPED DEFAULT profile, which is the only profile
// the measurement row runs, clock shuffle is off and that branch does not fire, and
// the bridge consumes nothing. Under a clock-shuffle profile it does, and it also
// leaves the stream reseeded to `finalSeed` rather than where it found it. The
// coordinator contract's "a query must consume none" (combo_logic.h's seed
// paragraph) is a property of the VTABLE, which this bridge is not part of; a future
// caller that needs the absolute property must save and restore the stream around
// `GrantStartingItems` — and `Ship_Random`'s state is TU-local statics in
// ShipUtils.cpp with no getter today, so that is a new accessor, not a local fix.
//
// WHY THEY ARE HERE AND NOT IN THE MEASUREMENT ROW'S OWN TU: the row needs (a)
// MM's half of the union bag and (b) MM's rounds to run against MM's SHIPPED
// PROFILE rather than a zeroed save, and both facts are spelled `RandoItemId`,
// `Rando::StaticData::Checks` and `RANDO_SAVE_OPTIONS`.

/**
 * MM'S HALF OF THE UNION BAG: the VANILLA item of every host in the CURRENT host
 * universe, which is the graph's whole check set or — once
 * `MM_ComboLogic_SetHostPool` has been called — that pool.
 *
 * WHY THE VANILLA ITEMS ARE THE POOL, and where this is an OVER-estimate.
 * `GeneratePools` builds MM's item pool by walking exactly the same region-graph
 * check set and pushing `randoStaticCheck.randoItemId` — the check's vanilla item
 * — once per pooled check (GeneratePools.cpp: "Initialize the check with it's
 * vanilla item", then `itemPool.push_back(randoStaticCheck.randoItemId)`). So this
 * IS that multiset, with three differences, all of them named rather than hidden:
 *
 *  (1) SETTINGS-CONDITIONAL NARROWING IS NOT APPLIED, for the same reason A3
 *      gives for the host universe: skulltulas-off, owls-off, cows-off, frogs-off,
 *      shops-off and the user's exclude list all drop rows from MM's real pool,
 *      and applying them here would duplicate `GeneratePools`' settings reading in
 *      a second place. The answer is therefore a SUPERSET of MM's real default
 *      pool, which for a COST measurement errs in the conservative direction.
 *  (2) THE NO-VANILLA-LOCATION ADDITIONS ARE ABSENT: `RI_PROGRESSIVE_SWORD`,
 *      `RI_SHIELD_HERO`, boss/enemy souls, clock items, `RI_ABILITY_SWIM`,
 *      ocarina buttons, triforce pieces and traps have no vanilla check, so no
 *      host names them. On the shipped default profile all but the first two are
 *      off anyway.
 *  (3) THE STARTING-ITEM REMOVAL AND THE PLENTIFUL DUPLICATION ARE ABSENT. Both
 *      are `GeneratePools` steps after the walk, and the second draws from
 *      `Ship_Random` — which is precisely why this accessor may not call
 *      `GeneratePools` and reads the static table instead.
 *
 * TWO IDS ARE EXCLUDED, and the reason is state outside the save rather than
 * tidiness: `RI_TRAP` reaches
 * `Rando::MiscBehavior::OfferTrapItem()` and `RI_TRIFORCE_PIECE` at the required
 * count dispatches `GameInteractor_ExecuteOnGameCompletion()` and emplaces a
 * `GIEventTransition` — a hook dispatch NO snapshot/restore undoes (the file
 * header's "what is not proved here" list). Neither is a vanilla check's item on
 * any profile, so excluding them changes no realistic answer; they are excluded so
 * that a future static-table edit cannot quietly put one in a measurement bag.
 * Everything else `IsGiveableItemId` accepts is admitted, including the ordinary
 * `MM_Item_Give(MM_gPlayState, ...)` default branch — which is the same path MM's
 * OWN fill grants its whole pool through (`GlitchlessLogic.cpp:221`), headlessly,
 * in the green `mm-rando-gen` and `mm-paired-attempt` rows.
 *
 * Same truncation contract as the two enumerators: at most `cap` entries written,
 * the TOTAL always returned. `outItems` and `outHosts` may each be NULL.
 */
extern "C" int MM_ComboLogic_PoolVanillaItems(uint16_t* outItems, uint16_t* outHosts, int cap) {
    int total = 0;
    for (uint16_t hostCheck : HostUniverse()) {
        const auto it = Rando::StaticData::Checks.find((RandoCheckId)hostCheck);
        if (it == Rando::StaticData::Checks.end()) {
            continue;
        }
        const uint16_t vanilla = (uint16_t)it->second.randoItemId;
        if (!IsGiveableItemId(vanilla)) {
            continue;
        }
        if (vanilla == (uint16_t)RI_TRAP || vanilla == (uint16_t)RI_TRIFORCE_PIECE) {
            continue; // see the block above: both reach outside the save
        }
        if (total < cap) {
            if (outItems != nullptr) {
                outItems[total] = vanilla;
            }
            if (outHosts != nullptr) {
                outHosts[total] = hostCheck;
            }
        }
        total++;
    }
    return total;
}

/**
 * THE NULL-PLAY GIVE PROBE, and the reason it exists rather than an argument.
 *
 * Audit §6.3 lists as undetermined "whether any branch of `GiveItem`/`ConvertItem`
 * reachable only with foreign items present touches state outside
 * `gSaveContext`". The measurement row hit that question the hard way: granting a
 * stride sample of MM's graph-wide vanilla items headlessly, with `MM_gPlayState`
 * NULL, produced an ACCESS VIOLATION (write near address 0) for some samples and
 * not others. That is the hazard `ForeignItemsSingleExe.cpp`'s header enumerates
 * from source — `Item_GiveImpl`'s skull-token count and its two icon-load pairs —
 * reaching a running process for the first time.
 *
 * So this walks a caller-supplied id list, printing `index` and `id` to STDERR
 * (unbuffered, and flushed) BEFORE each give. A crash therefore names the id that
 * caused it in the log, and `startIndex` lets the next run resume past it, so a
 * few runs enumerate the whole unsafe set. That set is then excluded by NAME in
 * `MM_ComboLogic_PoolVanillaItems` above, each with the branch it dies in.
 *
 * WHAT ONE RUN CAN AND CANNOT ESTABLISH, stated because a RESUMED run is not a walk
 * of the whole list. The gives are CUMULATIVE into one save: the loop takes ONE
 * snapshot, gives every id from `startIndex` onwards into it, and restores once at
 * the end. A run with `startIndex > 0` therefore gives NONE of the ids before it,
 * and any leg that faults only with prior state present is unreachable by that run —
 * which is exactly the class `MM_ComboLogic_ApplyShippedProfile`'s doc names below
 * (the two `Item_GiveImpl` legs that need a bottle or trade item already in an
 * EQUIPPED slot). What a single run FROM INDEX 0 supports is "no id in this list
 * faulted when given cumulatively, in this order, from this starting profile" — not
 * "no order of these ids faults", and not a statement about ids the list does not
 * contain (`MM_ComboLogic_PoolVanillaItems` excludes `RI_TRAP` and
 * `RI_TRIFORCE_PIECE` a priori, by reasoning rather than by measurement). Run it
 * from 0 whenever the answer is meant to be about the whole list; resume only to
 * walk PAST an id already known to fault.
 *
 * IT IS NOT CALLED BY ANY CTEST ROW, and as of the review of PR #722 it is not
 * reachable from the measurement row either: it has its own dispatch,
 * `redship --test combo-logic-give-probe`. That separation is deliberate. While it
 * was an env mode of `combo-logic-measure`, an exported `RSBS_COMBO_MEASURE_PROBE`
 * in a shell or a CI image turned the measuring row into a green no-op — the
 * silent-green class this tree keeps catching — and CTest does not scrub an
 * inherited environment. It is kept rather than deleted after use because the set it
 * derived will need re-deriving the next time MM's item table or `Item_GiveImpl`'s
 * guards change, and a probe nobody can re-run is a claim nobody can re-check.
 *
 * @return the number of gives that RETURNED. A crash means the process died and
 *         nothing is returned at all; that is the signal.
 */
extern "C" int MM_ComboLogic_ProbeGives(const uint16_t* ids, int count, int startIndex) {
    if (ids == nullptr || count <= 0) {
        return 0;
    }
    if (!Snapshot(nullptr)) {
        fprintf(stderr, "[MM ComboLogic] probe: snapshot failed; refusing to give into the live save\n");
        return 0;
    }
    int given = 0;
    for (int i = (startIndex > 0) ? startIndex : 0; i < count; ++i) {
        if (!IsGiveableItemId(ids[i])) {
            continue;
        }
        fprintf(stderr, "[MM ComboLogic] probe: index=%d id=%u\n", i, (unsigned)ids[i]);
        fflush(stderr);
        Rando::GiveItem(Rando::ConvertItem((RandoItemId)ids[i]));
        given++;
    }
    fprintf(stderr, "[MM ComboLogic] probe: %d gives returned without a fault\n", given);
    fflush(stderr);
    Restore(nullptr);
    return given;
}

/**
 * Put MM's save into the state THE CREATION SEAM puts it in, so a measured round
 * is a question about MM's SHIPPED PROFILE rather than about a zeroed struct.
 *
 * WHY THE MEASUREMENT NEEDS THIS. The engine's own row (`mm-combo-logic-engine`)
 * runs against whatever `gSaveContext` holds, which in a standalone ctest process
 * is static-init zeros: every `randoSaveOptions` entry 0, every trick bit 0. That
 * is a legal configuration and it is fine for a CONTRACT lock, but it is not the
 * profile a player generates under, and MM's per-round cost is a function of how
 * many regions and checks the crawl walks — which the options and the trick set
 * decide. Measuring the zeroed profile and reporting it as the shipped one would
 * be the same class of claim this tree keeps catching.
 *
 * WHAT IT DOES, in the order the creation seam does it:
 *   1. `MM_Sram_InitNewSave()` — a fresh MM save (the same call
 *      `mm_rando_gen_test.cpp` makes headlessly), so no bottle and no trade item
 *      is held, let alone C- or D-equipped. That matters: the two
 *      `Item_GiveImpl` legs that dereference a NULL `MM_gPlayState`
 *      (ForeignItemsSingleExe.cpp's enumerated legs 2 and 3) are reachable ONLY
 *      from a save that already holds one in an equipped slot.
 *   2. `saveType = SAVETYPE_RANDO`, so every `IS_RANDO`-gated registrar and
 *      condition reads this as a rando world.
 *   3. `Rando::Foreign::ResolvePairedProfile(false)` — resolves the authoring CVars into
 *      `RANDO_SAVE_OPTIONS` and `randoSaveTricks`, which is exactly what freezes
 *      the profile a fill runs under. SOLO (`paired == false`) deliberately: that
 *      path returns before touching `gComboCtx.mmProfileDigest`, so a measurement
 *      cannot stamp or trip a cross-game identity.
 *   4. `Rando::GrantStartingItems()` — A1's "at file creation the live save holds
 *      exactly the world's starting inventory". Without it the round's starting
 *      state is an empty save, which under-states reachability everywhere.
 *
 * AND ONE BOOT-TIME DEPENDENCY IT STANDS IN FOR, which the probe ABOVE found the
 * hard way rather than from source. `gRegEditor` (games/mm/src/code/z_debug.c) is a
 * plain NULL pointer until `Regs_Init()` allocates it out of MM's system arena
 * during a real `MM_Game_Init`, and MM's give paths write REG slots through it:
 * `Rando::GiveItem(RI_TINGLE_MAP_*)` ends in
 * `Inventory_SetWorldMapCloudVisibility`, whose last statement is
 * `R_MINIMAP_DISABLED = false` — i.e. `gRegEditor->data[14 * 96 + 95]`, a WRITE at
 * offset 0xb52 off a null pointer. Six ids fault there:
 * RI_TINGLE_MAP_CLOCK_TOWN, _GREAT_BAY, _ROMANI_RANCH, _SNOWHEAD, _STONE_TOWER,
 * _WOODFALL — found by resuming `MM_ComboLogic_ProbeGives` past each fault in turn,
 * which is how the stand-in below came to exist.
 *
 * WHAT THAT IS AND IS NOT EVIDENCE OF. With the stand-in installed, ONE cumulative
 * run of the probe from index 0 over the whole giveable-vanilla list returns without
 * a fault: `combo-logic-give-probe` reported 2168 of 2168 on the workstation this
 * increment was measured on, and it prints both numbers so the claim is re-checkable
 * rather than quoted. That is the strongest statement this method supports, and
 * it is still narrower than "no give reaches outside `gSaveContext`": the run fixes
 * ONE order and ONE starting profile, `RI_TRAP` and `RI_TRIFORCE_PIECE` are excluded
 * from the list a priori (see `MM_ComboLogic_PoolVanillaItems` above, which names
 * them as the two known outside-the-save cases), and order-dependent legs other than
 * the equipped-slot pair are not enumerated at all. The probe's own doc states the
 * cumulative-order limit in full.
 *
 * THAT IS NOT A NEW DEFECT AND IT IS NOT IN `Item_GiveImpl`. It is the same
 * boot-time dependency the PRODUCTION creation seam already stands in for
 * (`GameExports_SingleExe.cpp`, the `if (gRegEditor == NULL)` static right after
 * `MM_Rando_InitCore()`) and that `mm_rando_gen_test.cpp` stands in for with the
 * same static and a comment naming the same 0xb52 — so a paired creation with
 * "Starting Maps and Compasses" on is already safe. What the probe establishes is
 * that the hazard list in `ForeignItemsSingleExe.cpp`'s header is INCOMPLETE as a
 * guide: all three legs it enumerates are inside `Item_GiveImpl`, and this one
 * never reaches `MM_Item_Give` at all — it is in `GiveItem`'s own switch. Any
 * future caller that drives MM's give path outside a boot needs this guard too,
 * which is why it is here rather than in the measurement row: it belongs with
 * "the state the creation seam puts MM in".
 *
 * IT WRITES `gSaveContext`, and the caller MUST bracket it. The measurement row
 * copies the whole unified buffer before calling and restores it afterwards, and
 * then re-asserts OoT's world digest — the same outer bracket
 * `MM_ComboLogicEngine_RunHeadless` uses.
 *
 * @return `RANDO_SAVE_OPTIONS[RO_LOGIC]` as resolved, so the row can PRINT which
 *         logic mode it measured instead of asserting one it did not read.
 */
extern "C" int MM_ComboLogic_ApplyShippedProfile(void) {
    // The REG storage a real boot's Regs_Init would have allocated. Same static,
    // same guard and the same reason as the production creation seam's — see the
    // boot-time-dependency block above for the six ids that fault without it and
    // for why that list is not the one ForeignItemsSingleExe.cpp enumerates.
    if (gRegEditor == NULL) {
        static RegEditor sMeasurementRegEditor = {};
        gRegEditor = &sMeasurementRegEditor;
    }
    MM_Sram_InitNewSave();
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    Rando::Foreign::ResolvePairedProfile(false);
    Rando::GrantStartingItems();
    return (int)RANDO_SAVE_OPTIONS[RO_LOGIC];
}

// ============================================================================
// MULTIPLICITY BRIDGES (rando tier; src/common/tests/test_combo_logic_multiplicity.c)
// ============================================================================
//
// src/common cannot name an RI_* (ADR 0002), so the counter rows are exposed as
// small integer KINDS: 0 Woodfall small key, 1 Stone Tower small key, 2 Woodfall
// stray fairy, 3 swamp skull token.

namespace {
const RandoItemId kMmComboCounterKinds[] = {
    RI_WOODFALL_SMALL_KEY,
    RI_STONE_TOWER_SMALL_KEY,
    RI_WOODFALL_STRAY_FAIRY,
    RI_GS_TOKEN_SWAMP,
};
constexpr int kMmComboCounterKindCount = (int)(sizeof(kMmComboCounterKinds) / sizeof(kMmComboCounterKinds[0]));
} // namespace

extern "C" int MM_ComboLogic_TestCounterKindCount(void) {
    return kMmComboCounterKindCount;
}

/** The RI_* a kind names, for `assumeOwnItem`; -1 out of range. */
extern "C" int MM_ComboLogic_TestCounterItemId(int kind) {
    if (kind < 0 || kind >= kMmComboCounterKindCount) {
        return -1;
    }
    return (int)kMmComboCounterKinds[kind];
}

/** The kind's current count in the live save (inside a round: the round's). */
extern "C" int MM_ComboLogic_TestCounterValue(int kind) {
    if (kind < 0 || kind >= kMmComboCounterKindCount) {
        return -1;
    }
    return MmCounterValue(MmCounterFor((uint16_t)kMmComboCounterKinds[kind]));
}

/** The kind's maximum — the engine's own rule, not a copy of it. */
extern "C" int MM_ComboLogic_TestCounterMax(int kind) {
    if (kind < 0 || kind >= kMmComboCounterKindCount) {
        return -1;
    }
    const uint16_t id = (uint16_t)kMmComboCounterKinds[kind];
    return MmCounterMax(id, MmCounterFor(id));
}

/** Zero a kind's counter in the live save, so a lock's arithmetic is exact. Call
 *  it only INSIDE a snapshot bracket. Keys are zeroed in both of GiveItem's
 *  fields, so its `< 0` "no keys yet" sentinel branch is not taken. */
extern "C" void MM_ComboLogic_TestZeroCounter(int kind) {
    if (kind < 0 || kind >= kMmComboCounterKindCount) {
        return;
    }
    const MmCounterInfo c = MmCounterFor((uint16_t)kMmComboCounterKinds[kind]);
    switch (c.kind) {
        case MmCounterKind::SmallKey:
            gSaveContext.save.shipSaveInfo.rando.foundDungeonKeys[c.index] = 0;
            DUNGEON_KEY_COUNT(c.index) = 0;
            break;
        case MmCounterKind::StrayFairy:
            gSaveContext.save.saveInfo.inventory.strayFairies[c.index] = 0;
            break;
        case MmCounterKind::SkullToken:
            if (c.index == SCENE_KINSTA1) {
                gSaveContext.save.saveInfo.skullTokenCount &= 0x0000FFFF;
            } else {
                gSaveContext.save.saveInfo.skullTokenCount &= 0xFFFF0000;
            }
            break;
        default:
            break;
    }
}

/** Copies a counter clamp has absorbed since the last MM_ComboLogic_ResetCounters. */
extern "C" int MM_ComboLogic_CounterClamps(void) {
    return sCounterClamps;
}

/** Copies granted in the CURRENT round, by either path (0 outside a round). */
extern "C" int MM_ComboLogic_RoundCopiesGranted(void) {
    return sRound.copiesGranted;
}

// ============================================================================
// THE FILL-CLASS SOURCE (ADR 0010 answer O8; #645 increment 3, lane K5)
// ============================================================================
//
// MM's rows for the single-owner classification table in src/common/
// shared_items.{h,c} — the twin of the OoT source in ComboLogicEngineOoT.cpp.
// This TU is the SOURCE, never the owner: the owner walks it once and every
// consumer reads the owner's stored row. It lives here because this is already
// the MM TU that may name `RI_*` for the coordinator (ADR 0002) and it links
// WHOLE_ARCHIVE, so the registrar survives the link as the engine's does.
//
// Deliberately NOT inside the engine's vtable or the pool bridges K4's
// multiplicity work edits: the bag will read the owner table, not this function.

#include "shared_items.h" // src/common — the owner this source registers with

/**
 * Classify one MM item id (ComboItemClassifyFn). The precedence is the owner's
 * (shared_items.h): TRAP, then PROGRESSION by MM's OWN fill predicate, then
 * RENEWABLE, then JUNK.
 *
 * MM's fill predicate is GlitchlessLogic.cpp's non-junk test — `randoItemType`
 * is neither RITYPE_JUNK nor RITYPE_HEALTH — taken verbatim. It is generous
 * (maps, compasses, owl statues, Tingle maps and the gold-dust refill are all
 * RITYPE_LESSER and therefore non-junk to MM's fill), and it calls RI_TRAP
 * non-junk too, which is exactly why TRAP is decided first.
 *
 * RENEWABLE: the rest of RITYPE_JUNK — ammo, rupees, refills, recovery hearts,
 * magic jars — except RI_JUNK itself, the cover item a skipped or foreign-hosting
 * check holds, which is JUNK. RITYPE_HEALTH (heart pieces, heart containers,
 * double defense) is JUNK: not regainable, and not progression to MM's fill.
 *
 * NOT A FILL ITEM (returns 0): ids past RI_MAX or with no Items row, the two
 * sentinels RI_UNKNOWN and RI_NONE, and RI_TRIFORCE_PIECE_PREVIOUS, which
 * Items.cpp says "only exists to aid in the drawing of unique models" — CheckQueue
 * swaps it in for display, and no pool ever holds it.
 *
 * ARMING: the four criterion-3 give-capability families (souls, ocarina buttons,
 * swim, clock items) carry their RSBS_GIVECAP_* bit, and triforce pieces are a
 * per-world goal quantity. No confinement family: MM's fill places its whole pool
 * in one pass, so no MM setting holds an item in a restricted pass (small keys
 * included — which is the asymmetry with OoT's keysanity the owner's predicate
 * exists to express).
 */
extern "C" int MM_ComboLogic_ClassifyItem(uint16_t id, ComboItemClassRow* out) {
    ComboItemClassRow row = { RSBS_FILL_CLASS_NONE, 0u };
    if (out != nullptr) {
        *out = row;
    }
    if (Rando::StaticData::Items.empty()) {
        return -1; // a static std::map in another TU: not constructed yet
    }
    if (id >= (uint16_t)RI_MAX) {
        return 0;
    }
    const RandoItemId ri = (RandoItemId)id;
    const auto it = Rando::StaticData::Items.find(ri);
    if (it == Rando::StaticData::Items.end() || ri == RI_UNKNOWN || ri == RI_NONE || ri == RI_TRIFORCE_PIECE_PREVIOUS) {
        return 0;
    }
    const RandoItemType type = it->second.randoItemType;

    if (ri == RI_TRAP) {
        row.fillClass = RSBS_FILL_CLASS_TRAP;
    } else if (type != RITYPE_JUNK && type != RITYPE_HEALTH) {
        row.fillClass = RSBS_FILL_CLASS_PROGRESSION;
    } else if (type == RITYPE_JUNK && ri != RI_JUNK) {
        row.fillClass = RSBS_FILL_CLASS_RENEWABLE;
    } else {
        row.fillClass = RSBS_FILL_CLASS_JUNK;
    }

    // The same contiguous ranges GeneratePools.cpp walks to add these families.
    if ((ri >= RI_SOUL_BOSS_GOHT && ri <= RI_SOUL_BOSS_TWINMOLD) ||
        (ri >= RI_SOUL_ENEMY_ALIEN && ri <= RI_SOUL_ENEMY_WOLFOS)) {
        row.armedBy = RSBS_FILL_ARM_SOULS;
    } else if (ri >= RI_OCARINA_BUTTON_A && ri <= RI_OCARINA_BUTTON_C_UP) {
        row.armedBy = RSBS_FILL_ARM_OCARINA_BUTTONS;
    } else if (ri == RI_ABILITY_SWIM) {
        row.armedBy = RSBS_FILL_ARM_SWIM;
    } else if (ri >= RI_TIME_DAY_1 && ri <= RI_TIME_PROGRESSIVE) {
        row.armedBy = RSBS_FILL_ARM_CLOCKS;
    } else if (ri == RI_TRIFORCE_PIECE) {
        row.armedBy = RSBS_FILL_ARM_WORLD_EVENT;
    }
    if (out != nullptr) {
        *out = row;
    }
    return 1;
}

namespace {
const ComboItemClassSource kMMItemClassSource = {
    /* abiVersion */ RSBS_ITEM_CLASS_SOURCE_ABI,
    /* idSpace    */ (uint16_t)RI_MAX,
    /* classify   */ MM_ComboLogic_ClassifyItem,
};

struct MMItemClassRegistrar {
    MMItemClassRegistrar() {
        Combo_RegisterItemClassSource(GAME_MM, &kMMItemClassSource);
    }
};
const MMItemClassRegistrar gMMItemClassRegistrar;
} // namespace

/** TEST BRIDGE (redship tier): MM's own fill predicate for one id, read directly
 *  off Rando::StaticData::Items and NOT through the classifier, so the lock can
 *  check the classifier's precedence against it. 1 non-junk (the fill's
 *  "may unlock something"), 0 junk or health, -1 no Items row. */
extern "C" int MM_ComboLogic_TestFillAdvancement(uint16_t id) {
    const auto it = Rando::StaticData::Items.find((RandoItemId)id);
    if (id >= (uint16_t)RI_MAX || it == Rando::StaticData::Items.end()) {
        return -1;
    }
    const RandoItemType type = it->second.randoItemType;
    return (type != RITYPE_JUNK && type != RITYPE_HEALTH) ? 1 : 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
