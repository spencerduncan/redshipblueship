#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include "Logic.h"

#include <cstring>
#include <memory>

namespace Rando {

namespace Logic {

std::map<RandoRegionId, RandoRegion> Regions = {};

// Thread-local storage for current region time during check evaluation
thread_local uint64_t gCurrentRegionTime = 0;

RandoRegionId GetRegionIdFromEntrance(s32 entrance) {
    static std::map<s32, RandoRegionId> entranceToRegionId;
    if (entranceToRegionId.empty()) {
        for (auto& [randoRegionId, randoRegion] : Regions) {
            for (auto& [_, regionExit] : randoRegion.exits) {
                if (regionExit.returnEntrance == ONE_WAY_EXIT) {
                    continue;
                }
                entranceToRegionId[regionExit.returnEntrance] = randoRegionId;
            }
            for (auto& entrance : randoRegion.oneWayEntrances) {
                entranceToRegionId[entrance] = randoRegionId;
            }
        }
    }

    if (entranceToRegionId.contains(entrance)) {
        return entranceToRegionId[entrance];
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
void FindReachableRegions(RandoRegionId currentRegion, std::set<RandoRegionId>& reachableRegions,
                          std::unordered_map<RandoRegionId, RegionTimeState>& regionTimeStates) {
    // Ensure current region has time state
    EnsureRegionTimeState(regionTimeStates, currentRegion);

    auto& sourceRegion = Regions[currentRegion];
    auto& sourceTimeState = regionTimeStates[currentRegion];

    // Expand time if player can wait in this region
    uint64_t currentTime = sourceTimeState.timeSlices;
    if (sourceTimeState.canStayOverTime) {
        currentTime = TimeLogic::ExpandTimeForward(currentTime, sourceRegion);
        sourceTimeState.timeSlices = currentTime;
    }

    // Set global time for check evaluation
    gCurrentRegionTime = currentTime;

    // Explore connections
    for (auto& [connectedRegionId, condition] : sourceRegion.connections) {
        if (reachableRegions.count(connectedRegionId) == 0 && condition.first()) {
            reachableRegions.insert(connectedRegionId);

            auto& targetRegion = Regions[connectedRegionId];
            regionTimeStates[connectedRegionId] = { .timeSlices = currentTime,
                                                    .canStayOverTime = targetRegion.canStayOverTime };

            FindReachableRegions(connectedRegionId, reachableRegions, regionTimeStates);
        }
    }

    // Explore exits
    for (auto& [exitId, regionExit] : sourceRegion.exits) {
        RandoRegionId connectedRegionId = GetRegionIdFromEntrance(exitId);
        if (reachableRegions.count(connectedRegionId) == 0 && regionExit.condition()) {
            reachableRegions.insert(connectedRegionId);

            auto& targetRegion = Regions[connectedRegionId];
            regionTimeStates[connectedRegionId] = { .timeSlices = currentTime,
                                                    .canStayOverTime = targetRegion.canStayOverTime };

            FindReachableRegions(connectedRegionId, reachableRegions, regionTimeStates);
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
