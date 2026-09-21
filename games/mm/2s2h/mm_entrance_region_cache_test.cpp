/**
 * @file mm_entrance_region_cache_test.cpp
 * ROM-free, display-free probe and lock for #659: the entrance -> region cache
 * inside Rando::Logic::GetRegionIdFromEntrance must not freeze a map built from
 * a half-registered region graph.
 * CTest label "redship", row MMEntranceRegionCache in
 * CMake/SingleExecutable.cmake, dispatch "mm-entrance-region-cache" in
 * src/common/test_runner.cpp.
 *
 * WHAT #659 IS. Rando::Logic::Regions is populated by eighteen independent
 * RegisterShipInitFunc registrars (Logic/Regions/*.cpp plus the virtual RR_MAX
 * root at the bottom of Logic.cpp), all run by one S2H::ShipInit::InitAll()
 * pass whose order is static-initializer registration order, i.e. link order.
 * The cache's old guard was `if (entranceToRegionId.empty())`, which is
 * self-correcting for a lookup made while Regions is COMPLETELY empty and
 * permanently wrong for one made while it is PARTIALLY populated: that build
 * produces a non-empty map, the guard reads false forever after, and every
 * entrance owned by a not-yet-run registrar resolves to RR_MAX for the rest of
 * the process. CrawlReachableRegions seeds from
 * GetRegionIdFromEntrance(save->entrance), so the observable is not a crash but
 * a world that quietly looks less reachable than it is.
 *
 * THE TWO THINGS THIS ROW DOES, which are different claims:
 *
 *  (1) THE PROBE. #659's first ask is "prove no caller can reach
 *      GetRegionIdFromEntrance before InitAll completes". No amount of static
 *      reading settles that — link order is not in the source — so this row
 *      asks the running binary instead: drive the production bring-up
 *      (MM_Rando_Init -> S2H::ShipInit::InitAll -> Rando::Init) and read
 *      EntranceRegionCacheBuiltAtRegionCount(). Still (size_t)-1 afterwards
 *      means nothing in the entire pass looked an entrance up. A cache built
 *      from FEWER regions than the finished graph holds means one did, while the
 *      graph was half-built — and that is a failure even though the cache now
 *      self-corrects, because the caller that asked still received RR_MAX and no
 *      cache fix can repair an answer already returned.
 *
 *  (2) THE BOUNDARY LOCK. The fix itself: a lookup against a partial graph must
 *      not poison later lookups. Simulated the only way a unit row can — by
 *      reducing Regions to one real region, looking up an entrance that region
 *      does not own, restoring the full graph and looking the same entrance up
 *      again. Under the old emptiness guard the second lookup returns RR_MAX;
 *      under the count-keyed guard it returns the owning region.
 *
 *      NON-VACUITY IS EXPLICITLY ASSERTED. The partial map has to be NON-empty
 *      for the lock to mean anything: a partial build that happens to produce an
 *      empty map was already self-correcting under the old guard, so a "partial"
 *      subset contributing no entrances at all (the RR_MAX root, whose every
 *      exit is ONE_WAY_EXIT, is exactly such a subset) would make this row pass
 *      against the bug. The kept region is therefore required to resolve its own
 *      entrance before the restore is attempted.
 *
 * WHY THE VICTIM IS DISCOVERED AT RUNTIME rather than pinned. A hard-coded
 * (entrance, region) pair would rot the first time a region file is re-cut, and
 * it would be testing this file's copy of the graph rather than the graph. Both
 * regions and both entrances are read out of the live Regions map, so the row
 * follows the real data.
 *
 * Returns 0 on pass, a distinct nonzero step code on failure.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstddef>
#include <cstdio>
#include <map>

#include <ship/Context.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/Logic/Logic.h"

extern "C" {
// The production bring-up under test. Declared, never defined here.
void MM_Rando_Init(void);
}

namespace {

#define ERC_ASSERT(cond, code, msg)                                                     \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            printf("[TEST] FAIL(%d): %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            return (code);                                                              \
        }                                                                               \
    } while (0)

constexpr size_t kNeverBuilt = (size_t)-1;

// The first two-way exit a region declares, as the cache would key it: the
// RETURN entrance, which is what GetRegionIdFromEntrance maps back to a region.
// ONE_WAY_EXIT rows contribute nothing to the map and must be skipped here for
// the same reason the cache skips them.
bool FirstReturnEntranceOf(const Rando::Logic::RandoRegion& region, s32* outEntrance) {
    for (const auto& [_, regionExit] : region.exits) {
        if (regionExit.returnEntrance == ONE_WAY_EXIT) {
            continue;
        }
        *outEntrance = regionExit.returnEntrance;
        return true;
    }
    return false;
}

} // namespace

extern "C" int MM_EntranceRegionCache_RunHeadless(void) {
    printf("[TEST] mm-entrance-region-cache: no lookup runs against a half-registered region graph, and a partial "
           "build cannot freeze (#659)\n");

    auto ctx = Ship::Context::GetInstance();
    ERC_ASSERT(ctx != nullptr, 1, "Ship::Context singleton missing — run the shared bring-up first");
    ERC_ASSERT(ctx->GetConsoleVariables() != nullptr, 1,
               "ConsoleVariables missing — the ShipInit registrars MM_Rando_Init drives need them");

    // ------------------------------------------------------------------
    // (1) THE PROBE
    // ------------------------------------------------------------------
    // Freshness decides whether the probe can say anything. In this row's own
    // CTest process it is always fresh; inside `--test all` another row
    // (mm-registrar-coverage) has already run the irreversible MM_Rando_Init,
    // and the probe is reported as not run rather than passed vacuously.
    const bool fresh = Rando::Logic::Regions.empty() &&
                       Rando::Logic::EntranceRegionCacheBuiltAtRegionCount() == kNeverBuilt &&
                       Rando::Logic::EntranceRegionCacheRebuildCount() == 0;

    MM_Rando_Init();

    const size_t regionCount = Rando::Logic::Regions.size();
    ERC_ASSERT(regionCount > 1, 2,
               "the Logic/Regions graph did not populate — registrar TUs elided or InitAll did not run, and every "
               "assertion below would be vacuous");
    printf("[TEST] mm-entrance-region-cache: %zu regions registered\n", regionCount);

    if (fresh) {
        const size_t builtAt = Rando::Logic::EntranceRegionCacheBuiltAtRegionCount();
        const int rebuilds = Rando::Logic::EntranceRegionCacheRebuildCount();
        ERC_ASSERT(builtAt == kNeverBuilt || builtAt == regionCount, 3,
                   "the entrance->region cache was built from FEWER regions than the finished graph holds — a "
                   "production path looked an entrance up mid-registration and was answered RR_MAX (#659)");
        ERC_ASSERT(rebuilds <= 1, 3,
                   "the entrance->region cache was rebuilt more than once during bring-up — a lookup ran against a "
                   "partial graph (#659)");
        printf("[TEST] mm-entrance-region-cache: probe: cache state after bring-up is builtAt=%s rebuilds=%d — no "
               "lookup ran before region registration finished\n",
               builtAt == kNeverBuilt ? "never" : "complete-graph", rebuilds);
    } else {
        printf("[TEST] mm-entrance-region-cache: probe NOT RUN — MM_Rando_Init already ran in this process "
               "(`--test all`); the dedicated CTest row is the authoritative one\n");
    }

    // ------------------------------------------------------------------
    // (2) THE BOUNDARY LOCK
    // ------------------------------------------------------------------
    // Pick two distinct regions that each contribute at least one entrance to
    // the cache: one to KEEP in the simulated partial graph, one whose entrance
    // is the VICTIM the partial graph cannot resolve.
    RandoRegionId keepId = RR_MAX;
    RandoRegionId victimOwner = RR_MAX;
    s32 keepEntrance = 0;
    s32 victimEntrance = 0;
    for (const auto& [regionId, region] : Rando::Logic::Regions) {
        s32 entrance = 0;
        if (!FirstReturnEntranceOf(region, &entrance)) {
            continue;
        }
        if (keepId == RR_MAX) {
            keepId = regionId;
            keepEntrance = entrance;
            continue;
        }
        if (entrance != keepEntrance) {
            victimOwner = regionId;
            victimEntrance = entrance;
            break;
        }
    }
    ERC_ASSERT(keepId != RR_MAX && victimOwner != RR_MAX, 4,
               "could not find two regions with two-way exits — the graph shape this lock reads has changed");

    // Both resolve correctly against the COMPLETE graph. This also builds the
    // cache at the full region count, which is the state the freeze would have
    // to survive.
    ERC_ASSERT(Rando::Logic::GetRegionIdFromEntrance(keepEntrance) == keepId, 4,
               "the kept region's own entrance does not resolve to it against the complete graph");
    ERC_ASSERT(Rando::Logic::GetRegionIdFromEntrance(victimEntrance) == victimOwner, 4,
               "the victim entrance does not resolve to its owner against the complete graph");

    // THE PARTIAL GRAPH. A copy, restored below; RandoRegion holds std::function
    // members and is copyable, so this is a value snapshot rather than a view.
    const std::map<RandoRegionId, Rando::Logic::RandoRegion> completeGraph = Rando::Logic::Regions;
    Rando::Logic::Regions.clear();
    Rando::Logic::Regions[keepId] = completeGraph.at(keepId);

    // Non-vacuity, in this order on purpose: the partial map must be NON-EMPTY
    // (the kept region resolves) AND must not already answer the victim. A
    // partial map that is empty was self-correcting even under the old guard.
    ERC_ASSERT(Rando::Logic::GetRegionIdFromEntrance(keepEntrance) == keepId, 5,
               "the partial graph produced an EMPTY entrance map — this lock would pass against the bug it exists "
               "to catch (#659)");
    ERC_ASSERT(Rando::Logic::GetRegionIdFromEntrance(victimEntrance) == RR_MAX, 5,
               "the kept region also owns the victim entrance — pick a different pair, this lock proves nothing");

    // ...and the restore. THIS is the assertion that goes red on the old
    // emptiness guard: the cache built above is non-empty, so that guard never
    // rebuilds and the victim stays RR_MAX for the life of the process.
    Rando::Logic::Regions = completeGraph;
    ERC_ASSERT(Rando::Logic::GetRegionIdFromEntrance(victimEntrance) == victimOwner, 6,
               "a lookup made against a PARTIAL region graph froze the cache — every entrance owned by a "
               "not-yet-registered region resolves to RR_MAX forever (#659)");
    ERC_ASSERT(Rando::Logic::EntranceRegionCacheBuiltAtRegionCount() == regionCount, 6,
               "the cache did not record the region count it was rebuilt from");

    // ------------------------------------------------------------------
    // (3) THE EXPLICIT INVALIDATION HOOK
    // ------------------------------------------------------------------
    // The count cannot see a mutation that rewires exits without changing how
    // many regions exist (entrance randomization is the live candidate), so such
    // a writer calls InvalidateEntranceRegionCache(). Locked here so the hook
    // cannot quietly become a no-op before its first caller arrives.
    Rando::Logic::InvalidateEntranceRegionCache();
    ERC_ASSERT(Rando::Logic::EntranceRegionCacheBuiltAtRegionCount() == kNeverBuilt, 7,
               "InvalidateEntranceRegionCache did not drop the cache");
    ERC_ASSERT(Rando::Logic::GetRegionIdFromEntrance(victimEntrance) == victimOwner, 7,
               "the cache did not rebuild after an explicit invalidation");

    printf("[TEST] mm-entrance-region-cache: PASS\n");
    return 0;
}

#else

#include <cstdio>

/**
 * Outside the single exe MM runs its own boot and this row's subject — the
 * shared ShipInit pass over eighteen region registrars — does not exist in the
 * form #659 describes. test_runner.cpp declares the entry point
 * unconditionally, so report pass rather than failing to link.
 */
extern "C" int MM_EntranceRegionCache_RunHeadless(void) {
    printf("[TEST] mm-entrance-region-cache: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}

#endif
