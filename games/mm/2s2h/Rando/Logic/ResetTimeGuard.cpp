// MM's reset-time guard (ADR 0010 answer O2, #645). The contract and the OoTMM
// lineage are in ResetTimeGuard.h.
#include "ResetTimeGuard.h"

#include <iterator>

#ifdef RSBS_SINGLE_EXECUTABLE
// src/common: MM_Rando_FrozenMoonCrash and RSBS_MM_MOON_CRASH_* (the frozen "Moon
// Crash Behavior", with the Shared Song of Time rule's forced New Cycle folded
// in; defined in Rando/Foreign.cpp).
#include "combo_mm_options_view.h"
#endif

namespace Rando {

namespace Logic {

bool MmCanResetTime() {
#ifdef RSBS_SINGLE_EXECUTABLE
    return MM_Rando_FrozenMoonCrash() == RSBS_MM_MOON_CRASH_CYCLE || CAN_PLAY_SONG(TIME);
#else
    return true; // upstream 2Ship: no frozen moon crash, and no reset term (see the header)
#endif
}

bool MmCanResetTimeOnMoon() {
#ifdef RSBS_SINGLE_EXECUTABLE
    return CAN_PLAY_SONG(TIME);
#else
    return true;
#endif
}

bool IsResetTimeNookRegion(RandoRegionId regionId) {
    switch (regionId) {
        case RR_MAX: // the root: the starting checks and the save warp back to the arrival
        case RR_CLOCK_TOWN_SOUTH:
        case RR_CLOCK_TOWER_INTERIOR: // the MM->OoT crossing
        case RR_CLOCK_TOWER_ROOF:     // the region only; its checks are not nook checks
            return true;
        default:
            return false;
    }
}

bool IsResetTimeNookCheck(RandoCheckId checkId) {
    switch (checkId) {
        case RC_STARTING_ITEM_DEKU_MASK:
        case RC_STARTING_ITEM_SONG_OF_HEALING:
        case RC_CLOCK_TOWN_SOUTH_PLATFORM_PIECE_OF_HEART:
        case RC_CLOCK_TOWN_SOUTH_OWL_STATUE:
            return true;
        default:
            return false;
    }
}

namespace {

/** The regions OoTMM enters only through an exit gated on can_reset_time_on_moon. */
bool IsMoonTrialRegion(RandoRegionId regionId) {
    switch (regionId) {
        case RR_MOON_DEKU_TRIAL:
        case RR_MOON_GORON_TRIAL:
        case RR_MOON_ZORA_TRIAL:
        case RR_MOON_LINK_TRIAL:
            return true;
        default:
            return false;
    }
}

} // namespace

bool ApplyResetTimeGuard(ReachabilityCrawl& crawl, std::set<RandoCheckId>& checks) {
    const bool canReset = MmCanResetTime();
    if (canReset && MmCanResetTimeOnMoon()) {
        return false;
    }

    // Regions first: the nook when time cannot be reset at all, everything but the
    // Moon trials when it can be reset everywhere but on the Moon.
    bool regionsPruned = false;
    for (auto it = crawl.reachableRegions.begin(); it != crawl.reachableRegions.end();) {
        const bool keep = canReset ? !IsMoonTrialRegion(*it) : IsResetTimeNookRegion(*it);
        if (keep) {
            ++it;
            continue;
        }
        it = crawl.reachableRegions.erase(it);
        regionsPruned = true;
    }
    // Their time states with them, so the crawl stays one consistent answer (the
    // engine hands it to MmGoalMajoraDefeated, and SetCurrentRegionTime's `.at()`
    // expects every reached region to have a state).
    for (auto it = crawl.regionTimeStates.begin(); it != crawl.regionTimeStates.end();) {
        if (crawl.reachableRegions.find(it->first) == crawl.reachableRegions.end()) {
            it = crawl.regionTimeStates.erase(it);
        } else {
            ++it;
        }
    }

    // Then the checks, re-evaluated over the kept regions: one check can sit in a
    // pruned region and a kept one (the enemy drops in the Link trial), so
    // removing a pruned region's check ids would be wrong.
    if (regionsPruned) {
        checks = EvaluateReachableChecks(crawl);
    }
    const size_t before = checks.size();
    for (auto it = checks.begin(); it != checks.end();) {
        const bool keep = canReset ? *it != RC_MOON_FIERCE_DEITY_MASK : IsResetTimeNookCheck(*it);
        it = keep ? std::next(it) : checks.erase(it);
    }
    return regionsPruned || checks.size() != before;
}

} // namespace Logic

} // namespace Rando
