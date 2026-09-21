#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include "Logic.h"

#include <cstdio>
#include <cstring>
#include <memory>

namespace Rando {

namespace Logic {

std::map<RandoRegionId, RandoRegion> Regions = {};

// Thread-local storage for current region time during check evaluation
thread_local uint64_t gCurrentRegionTime = 0;

// ============================================================================
// THE ENTRANCE -> REGION CACHE (#659)
// ============================================================================
//
// WHAT WAS WRONG WITH `if (entranceToRegionId.empty())`. Regions is populated by
// EIGHTEEN independent RegisterShipInitFunc registrars (the seventeen files
// under Logic/Regions/ plus the virtual RR_MAX root at the bottom of this file),
// all of them run by one S2H::ShipInit::InitAll() pass over a map whose
// iteration order is the order the static initializers happened to register in —
// i.e. link order, which nothing in the source controls.
//
// An `empty()` guard is self-correcting for a call made while Regions is
// COMPLETELY unpopulated: the cache stays empty and the next call rebuilds. The
// hazard is the PARTIAL case. A call made after some registrars have run and
// before the rest builds a NON-empty map from whatever existed at that instant,
// the guard then reads false for the rest of the process, and every entrance
// owned by a not-yet-run registrar resolves to RR_MAX forever. That is silent in
// the worst place: CrawlReachableRegions seeds reachableRegions from
// GetRegionIdFromEntrance(save->entrance), and an unknown entrance there
// collapses to the root-only seed, so a world simply looks less reachable than
// it is — no crash, no log, a wrong fill.
//
// THE FIX IS TO KEY THE CACHE ON REGISTRATION PROGRESS, NOT ON ITS OWN
// EMPTINESS. Registrars only ever ADD regions, so Regions.size() is monotonic
// across registration and constant once it finishes; rebuilding whenever the
// count differs from the count the cache was built at makes a partial build
// self-correcting in exactly the way an empty one already was, at the cost of
// one size_t compare per call (the crawl calls this once per exit per worklist
// pop, so an O(regions) content stamp here would be O(regions * exits) per
// crawl — measurably worse for no additional coverage of the #659 hazard).
//
// WHAT THE COUNT CANNOT SEE, and the hook that covers it:
// InvalidateEntranceRegionCache(). A mutation that REWIRES an existing region's
// exits without changing how many regions exist — entrance randomization is the
// live candidate — leaves the count identical, so any such writer must invalidate
// explicitly. Nothing does today, which is why this is a hook rather than a call:
// stating the contract at the one place that can honour it is what keeps the next
// entrance-shuffle lane from having to rediscover #659.
static std::map<s32, RandoRegionId> sEntranceToRegionId;
// The region count sEntranceToRegionId was built from. SIZE_MAX is "never
// built", which is distinct from "built from zero regions" — without that
// distinction a first call made before any registrar has run would look like a
// valid empty build.
static size_t sEntranceToRegionIdBuiltAt = (size_t)-1;

// Rebuild counter. Observable rather than internal because "was this cache ever
// built before registration finished?" is the question #659 asks and no static
// analysis of link order can answer it.
static int sEntranceToRegionIdRebuilds = 0;

void InvalidateEntranceRegionCache() {
    sEntranceToRegionId.clear();
    sEntranceToRegionIdBuiltAt = (size_t)-1;
}

size_t EntranceRegionCacheBuiltAtRegionCount() {
    return sEntranceToRegionIdBuiltAt;
}

int EntranceRegionCacheRebuildCount() {
    return sEntranceToRegionIdRebuilds;
}

RandoRegionId GetRegionIdFromEntrance(s32 entrance) {
    if (sEntranceToRegionIdBuiltAt != Regions.size()) {
        // A rebuild AFTER a previous build means a call landed mid-registration
        // (or a mutator added regions without invalidating). Harmless now — this
        // is the self-correction — but it is exactly the #659 symptom, and it
        // used to be permanent, so it is named rather than swallowed.
        if (sEntranceToRegionIdBuiltAt != (size_t)-1) {
            fprintf(stderr,
                    "[MM] entrance->region cache rebuilt: %zu regions now, built from %zu - a lookup ran before "
                    "region registration finished (#659)\n",
                    Regions.size(), sEntranceToRegionIdBuiltAt);
        }
        sEntranceToRegionId.clear();
        for (auto& [randoRegionId, randoRegion] : Regions) {
            for (auto& [_, regionExit] : randoRegion.exits) {
                if (regionExit.returnEntrance == ONE_WAY_EXIT) {
                    continue;
                }
                sEntranceToRegionId[regionExit.returnEntrance] = randoRegionId;
            }
            for (auto& entrance : randoRegion.oneWayEntrances) {
                sEntranceToRegionId[entrance] = randoRegionId;
            }
        }
        sEntranceToRegionIdBuiltAt = Regions.size();
        sEntranceToRegionIdRebuilds++;
    }

    if (sEntranceToRegionId.contains(entrance)) {
        return sEntranceToRegionId[entrance];
    }

    return RR_MAX;
}

// Helper: Convert runtime game time to TimeSlice enum for dynamic time checking
TimeSlice TimeSliceFromGameTime(s32 day, u16 time) {
    // Handle edge cases: day 0 or invalid inputs
    if (day < 1 || day > 3) {
        return TIME_DAY1_AM_06_00; // Default fallback
    }

    // Convert to time slice based on day/time ranges
    // This is approximate - exact mapping would need game time constants
    bool isNight = (time >= GAME_TIME_NIGHT_START || time < GAME_TIME_DAY_START);
    int halfDayOffset = (day - 1) * 2 + (isNight ? 1 : 0);

    // Map to approximate time slice within the half-day
    if (halfDayOffset >= 6)
        return TIME_NIGHT3_AM_05_00;

    const auto& range = HALF_DAY_TIME_RANGES[halfDayOffset];
    return static_cast<TimeSlice>(range.startSlice);
}

// Helper: Returns the initial time state for logic solving (start at Day 1, 6:00 AM)
RegionTimeState InitialTimeState() {
    return { .timeSlices = (TIME_BIT_ONE << TIME_DAY1_AM_06_00), .canStayOverTime = false };
}

// Shared initialization function for region time states
std::unordered_map<RandoRegionId, RegionTimeState> InitializeRegionTimeStates(RandoRegionId startRegion) {
    std::unordered_map<RandoRegionId, RegionTimeState> states;

    // Start with appropriate time based on Clock Shuffle
    if (SettingClocks()) {
        // Clock Shuffle: start with owned time slices only
        states[startRegion] = { .timeSlices = TimeLogic::GetOwnedTimeSlices(), .canStayOverTime = false };
    } else {
        // No Clock Shuffle: start at Day 1 6am
        states[startRegion] = InitialTimeState();
    }

    return states;
}

// Helper to ensure region time state exists
void EnsureRegionTimeState(std::unordered_map<RandoRegionId, RegionTimeState>& regionTimeStates,
                           RandoRegionId regionId) {
    if (regionTimeStates.find(regionId) == regionTimeStates.end()) {
        auto& region = Regions[regionId];
        regionTimeStates[regionId] = { .timeSlices = TimeLogic::GetOwnedTimeSlices(),
                                       .canStayOverTime = region.canStayOverTime };
    }
}

// Time expansion during region traversal with stay restrictions
// Time expansion semantics: if canStayOverTime, sequentially test each future time slice
// Stop permanently if any timeStayRestrictions check fails
//
// #585 — THE JOIN. This used to be FIRST-VISIT-WINS in two ways that compound.
// A region was explored only when `reachableRegions.count(target) == 0`, so
// (a) its edge condition was never re-evaluated once anything had reached it,
// and (b) its `regionTimeStates` entry was OVERWRITTEN with whichever time set
// happened to arrive first and never widened again. Reaching a region early
// under a narrow set of time slices therefore PINNED it to that set for the rest
// of the fill, and everything behind it inherited the under-approximation. The
// fill was measurably less capable than the reachability crawl sitting beside it
// in this same file (CrawlReachableRegions, ADR 0010 increment 1.3), which does
// the join correctly — and the observable cost was avoidable ladder attempts, in
// a fill whose dominant failure mode is a wall-clock abort.
//
// The fix adopts the crawl's discipline verbatim (ADR 0010 D2.3's join rule):
// a target already in the set is not skipped — its time slices are UNIONED with
// the arriving set, and if that union GREW, the target is re-explored. Growth is
// monotone and bounded by the 45-slice word, so the worklist drains.
//
// Two shape changes come with it, both forced rather than stylistic:
//   - ITERATIVE, not recursive. With re-exploration the recursion depth is no
//     longer bounded by the region count but by the number of growth events
//     (regions x slices), which is a stack depth nobody should have to reason
//     about. The worklist is the same traversal without the frame.
//   - Edge conditions are evaluated on EVERY visit, as the crawl does, because
//     "already reachable" no longer means "nothing more to learn here". The
//     conditions are pure predicates over the save, so this costs evaluations
//     and changes no state.
//
// RR_MAX is never re-entered, mirroring the crawl: it is the virtual root, and
// GetRegionIdFromEntrance also resolves an unknown exit to it, so propagating
// into it would splice arbitrary time sets into every save-warp destination.
void FindReachableRegions(RandoRegionId currentRegion, std::set<RandoRegionId>& reachableRegions,
                          std::unordered_map<RandoRegionId, RegionTimeState>& regionTimeStates) {
    std::vector<RandoRegionId> worklist;
    worklist.push_back(currentRegion);

    while (!worklist.empty()) {
        const RandoRegionId regionId = worklist.back();
        worklist.pop_back();

        // Ensure this region has time state
        EnsureRegionTimeState(regionTimeStates, regionId);

        auto& sourceRegion = Regions[regionId];
        auto& sourceTimeState = regionTimeStates[regionId];

        // Expand time if player can wait in this region
        uint64_t currentTime = sourceTimeState.timeSlices;
        if (sourceTimeState.canStayOverTime) {
            currentTime = TimeLogic::ExpandTimeForward(currentTime, sourceRegion);
            sourceTimeState.timeSlices = currentTime;
        }

        // Set global time for check evaluation
        gCurrentRegionTime = currentTime;

        auto propagate = [&](RandoRegionId targetId) {
            if (targetId == RR_MAX) {
                return; // never re-enter the virtual root (see the header note)
            }
            if (reachableRegions.insert(targetId).second) {
                regionTimeStates[targetId] = { .timeSlices = currentTime,
                                               .canStayOverTime = Regions[targetId].canStayOverTime };
                worklist.push_back(targetId);
                return;
            }
            auto stateIt = regionTimeStates.find(targetId);
            if (stateIt == regionTimeStates.end()) {
                // Seeded but not yet visited; EnsureRegionTimeState establishes
                // its state when the worklist reaches it.
                return;
            }
            const uint64_t merged = stateIt->second.timeSlices | currentTime;
            if (merged != stateIt->second.timeSlices) {
                stateIt->second.timeSlices = merged; // THE JOIN
                worklist.push_back(targetId);
            }
        };

        // Explore connections
        for (auto& [connectedRegionId, condition] : sourceRegion.connections) {
            if (condition.first()) {
                propagate(connectedRegionId);
            }
        }

        // Explore exits
        for (auto& [exitId, regionExit] : sourceRegion.exits) {
            if (regionExit.condition()) {
                propagate(GetRegionIdFromEntrance(exitId));
            }
        }
    }
}

// ============================================================================
// Factored reachability crawl + reachable-check closure (ADR 0010 increment
// 1.3; #500 work item 2). Contracts in Logic.h; the discipline notes that are
// implementation-shaped live here.
// ============================================================================

ReachabilityCrawl CrawlReachableRegions(s32 startEntrance) {
    ReachabilityCrawl crawl;
    // The check tracker's historical seeding, kept exactly: the virtual root
    // (starting items + save-warp exits) plus wherever the save's entrance
    // resolves. An unknown entrance resolves to RR_MAX, which collapses to the
    // root-only seed.
    crawl.reachableRegions = { RR_MAX, GetRegionIdFromEntrance(startEntrance) };
    crawl.regionTimeStates = InitializeRegionTimeStates(RR_MAX);

    // Fresh event evaluation (the tracker's discipline): stale RANDO_EVENTS
    // from a previous evaluation must not satisfy this one.
    for (int i = 0; i < RE_MAX; i++) {
        RANDO_EVENTS[i] = 0;
    }

    // Event registrations already fired, keyed by (region, index in the
    // region's event vector) — NOT by RandoEvent id. RANDO_EVENTS is a COUNT,
    // and several events are deliberately registered many times so the count
    // means something: RE_ACCESS_ZORA_EGG appears 7 times (4 in Pirates'
    // Fortress, 3 at Pinnacle Rock) because
    // RC_GREAT_BAY_COAST_NEW_WAVE_BOSSA_NOVA reads
    // `RANDO_EVENTS[RE_ACCESS_ZORA_EGG] >= 7` (Regions/West.cpp:215), and
    // RE_ACCESS_SPRING_WATER / RE_ACCESS_PIRATE_PICTURE / RE_ACCESS_BUGS are
    // the same shape. The check tracker's inline loop guarded on
    // `!RANDO_EVENTS[event.first]`, which dedupes by ID and so caps every one
    // of those counters at 1 — permanently unsatisfying every `>= n` gate and
    // everything behind it. That was cosmetic while the crawl only tinted
    // tracker rows; it is not cosmetic now that a foreign-host decision is
    // computed from it, so the shared crawl adopts the glitchless fill's
    // per-REGISTRATION accounting (GlitchlessLogic.cpp's eventsInLogic keys on
    // the registration, not the id). A (region, index) key rather than the
    // fill's pointer key so nothing here can depend on an address ordering.
    std::set<std::pair<RandoRegionId, size_t>> firedEventRegistrations;

    bool changed = true;
    while (changed) {
        changed = false;

        // Region/time propagation with a real JOIN (ADR 0010 D2.3): reaching
        // an already-explored region with NEW time slices unions them and
        // re-explores, where FindReachableRegions' first-visit guard keeps
        // whichever time set arrived first and never looks again.
        const std::set<RandoRegionId> frontier = crawl.reachableRegions;
        for (RandoRegionId regionId : frontier) {
            EnsureRegionTimeState(crawl.regionTimeStates, regionId);
            auto& sourceRegion = Regions[regionId];
            auto& sourceState = crawl.regionTimeStates[regionId];

            if (sourceState.canStayOverTime) {
                const uint64_t expanded = TimeLogic::ExpandTimeForward(sourceState.timeSlices, sourceRegion);
                if (expanded != sourceState.timeSlices) {
                    sourceState.timeSlices = expanded;
                    changed = true;
                }
            }
            const uint64_t currentTime = sourceState.timeSlices;
            gCurrentRegionTime = currentTime;

            auto propagate = [&](RandoRegionId targetId) {
                if (targetId == RR_MAX) {
                    // Never re-enter the root: it is the seed, and an exit
                    // whose entrance resolves nowhere also lands here.
                    return;
                }
                if (crawl.reachableRegions.insert(targetId).second) {
                    crawl.regionTimeStates[targetId] = { .timeSlices = currentTime,
                                                         .canStayOverTime = Regions[targetId].canStayOverTime };
                    changed = true;
                    return;
                }
                auto stateIt = crawl.regionTimeStates.find(targetId);
                if (stateIt == crawl.regionTimeStates.end()) {
                    // A seeded region not yet visited this round; its state is
                    // established by EnsureRegionTimeState when the frontier
                    // loop reaches it.
                    return;
                }
                const uint64_t merged = stateIt->second.timeSlices | currentTime;
                if (merged != stateIt->second.timeSlices) {
                    stateIt->second.timeSlices = merged; // the join
                    changed = true;
                }
            };

            for (auto& [connectedRegionId, condition] : sourceRegion.connections) {
                if (condition.first()) {
                    propagate(connectedRegionId);
                }
            }
            for (auto& [exitId, regionExit] : sourceRegion.exits) {
                if (regionExit.condition()) {
                    propagate(GetRegionIdFromEntrance(exitId));
                }
            }
        }

        // Event pass: fire newly satisfiable event REGISTRATIONS under each
        // region's joined time state; anything fired re-runs the crawl. See
        // firedEventRegistrations above for why the guard is per-registration
        // and not per-event-id.
        for (RandoRegionId regionId : crawl.reachableRegions) {
            auto& randoRegion = Regions[regionId];
            SetCurrentRegionTime(crawl.regionTimeStates, regionId);
            for (size_t eventIndex = 0; eventIndex < randoRegion.events.size(); eventIndex++) {
                auto& event = randoRegion.events[eventIndex];
                if (firedEventRegistrations.contains({ regionId, eventIndex })) {
                    continue;
                }
                if (event.second()) {
                    RANDO_EVENTS[event.first]++;
                    firedEventRegistrations.insert({ regionId, eventIndex });
                    changed = true;
                }
            }
        }
    }

    return crawl;
}

std::set<RandoCheckId> EvaluateReachableChecks(const ReachabilityCrawl& crawl) {
    std::set<RandoCheckId> satisfiable;
    for (RandoRegionId regionId : crawl.reachableRegions) {
        auto& randoRegion = Regions.at(regionId);
        SetCurrentRegionTime(crawl.regionTimeStates, regionId);
        for (auto& [randoCheckId, checkLogic] : randoRegion.checks) {
            // A check can live in more than one region (enemy drops); one
            // satisfiable placement is enough.
            if (!satisfiable.contains(randoCheckId) && checkLogic.first()) {
                satisfiable.insert(randoCheckId);
            }
        }
    }
    return satisfiable;
}

std::set<RandoCheckId> ComputeReachableCheckSet() {
    // The GlitchlessLogic memcpy swap discipline: the closure simulates a
    // playthrough by GIVING items into the live save, so the whole run is
    // bracketed by a byte snapshot/restore. Heap-allocated — the port's
    // SaveContext is large, and unlike the fill this can be called from
    // arbitrary harness stack depths.
    auto copiedSaveContext = std::make_unique<SaveContext>();
    memcpy(copiedSaveContext.get(), &gSaveContext, sizeof(SaveContext));
    // GiveItem's triforce-completion branch queues a transition into the
    // game-events queue — state OUTSIDE the save snapshot. Record the depth
    // and truncate back to it so a simulated completion cannot leak a queued
    // transition into gameplay.
    const size_t gameEventsDepth = MM_GameEvents_Queue().size();

    std::set<RandoCheckId> obtainable;
    bool collectedAny = true;
    while (collectedAny) {
        collectedAny = false;
        const ReachabilityCrawl crawl = CrawlReachableRegions(gSaveContext.save.entrance);
        for (RandoCheckId randoCheckId : EvaluateReachableChecks(crawl)) {
            if (obtainable.contains(randoCheckId)) {
                continue;
            }
            obtainable.insert(randoCheckId);
            // Simulate the collect: a shuffled check yields its fill-assigned
            // item, an unshuffled check its vanilla item — the same sourcing
            // the glitchless fill credits to logic (GlitchlessLogic.cpp's
            // isShuffled branch). Through the REAL give path, so progressive
            // resolution and flag side effects match gameplay; GiveItem and
            // ConvertItem consume no Ship_Random (CurrentJunkItem, the one
            // junk randomizer, is a pickup-time presentation concern that
            // never runs here).
            const RandoSaveCheck& randoSaveCheck = RANDO_SAVE_CHECKS[randoCheckId];
            const RandoItemId placedItem = randoSaveCheck.shuffled
                                               ? randoSaveCheck.randoItemId
                                               : Rando::StaticData::Checks[randoCheckId].randoItemId;
            if (placedItem != RI_UNKNOWN && placedItem != RI_NONE) {
                GiveItem(ConvertItem(placedItem));
            }
            collectedAny = true;
        }
    }

    memcpy(&gSaveContext, copiedSaveContext.get(), sizeof(SaveContext));
    if (MM_GameEvents_Queue().size() > gameEventsDepth) {
        MM_GameEvents_Queue().resize(gameEventsDepth);
    }
    return obtainable;
}

// clang-format off
static RegisterShipInitFunc initFunc([]() {
    Regions[RR_MAX] = RandoRegion{ .sceneId = SCENE_MAX,
        .checks = {
            CHECK(RC_STARTING_ITEM_DEKU_MASK, true),
            CHECK(RC_STARTING_ITEM_SONG_OF_HEALING, true),
        },
        .exits = { //     TO                                         FROM
            EXIT(ENTRANCE(SOUTH_CLOCK_TOWN, 0),                      ONE_WAY_EXIT, true), // Save warp
            EXIT(ENTRANCE(SOUTH_CLOCK_TOWN, 9),                      ONE_WAY_EXIT, CAN_PLAY_SONG(SOARING) && CAN_OWL_WARP(OWL_WARP_CLOCK_TOWN)),
            EXIT(ENTRANCE(SOUTHERN_SWAMP_POISONED, 10),              ONE_WAY_EXIT, CAN_PLAY_SONG(SOARING) && CAN_OWL_WARP(OWL_WARP_SOUTHERN_SWAMP)),
            EXIT(ENTRANCE(WOODFALL, 4),                              ONE_WAY_EXIT, CAN_PLAY_SONG(SOARING) && CAN_OWL_WARP(OWL_WARP_WOODFALL)),
            EXIT(ENTRANCE(MILK_ROAD, 4),                             ONE_WAY_EXIT, CAN_PLAY_SONG(SOARING) && CAN_OWL_WARP(OWL_WARP_MILK_ROAD)),
            EXIT(ENTRANCE(MOUNTAIN_VILLAGE_WINTER, 8),               ONE_WAY_EXIT, CAN_PLAY_SONG(SOARING) && CAN_OWL_WARP(OWL_WARP_MOUNTAIN_VILLAGE)),
            EXIT(ENTRANCE(SNOWHEAD, 3),                              ONE_WAY_EXIT, CAN_PLAY_SONG(SOARING) && CAN_OWL_WARP(OWL_WARP_SNOWHEAD)),
            EXIT(ENTRANCE(GREAT_BAY_COAST, 11),                      ONE_WAY_EXIT, CAN_PLAY_SONG(SOARING) && CAN_OWL_WARP(OWL_WARP_GREAT_BAY_COAST)),
            EXIT(ENTRANCE(ZORA_CAPE, 6),                             ONE_WAY_EXIT, CAN_PLAY_SONG(SOARING) && CAN_OWL_WARP(OWL_WARP_ZORA_CAPE)),
            EXIT(ENTRANCE(IKANA_CANYON, 4),                          ONE_WAY_EXIT, CAN_PLAY_SONG(SOARING) && CAN_OWL_WARP(OWL_WARP_IKANA_CANYON)),
            EXIT(ENTRANCE(STONE_TOWER, 3),                           ONE_WAY_EXIT, CAN_PLAY_SONG(SOARING) && CAN_OWL_WARP(OWL_WARP_STONE_TOWER)),
        },
    };
}, {});
// clang-format on

} // namespace Logic

} // namespace Rando
