/**
 * ComboLogicMonotonicitySingleExe.cpp — the MM half of the monotonicity
 * grow-check's test surface (ADR 0010 answer O6; the `combo-logic-monotonicity`
 * row, #645). The OoT twin is
 * soh/Enhancements/randomizer/ComboLogicMonotonicityOoT.cpp.
 *
 * The grow-check (src/common/tests/test_combo_logic_monotonicity.c) drives MM's
 * engine through the coordinator's own vtable and reads the round's region set
 * through the one read-only accessor the engine TU exports for it
 * (`MM_ComboLogic_TestRoundRegions`). Three things it cannot do from src/common,
 * because src/common names no MM enumerator (ADR 0002), live here:
 *
 *   1. FORCE A TRICKS-ON SET. MM's tricks are FROZEN per-file bits
 *      (`randoSaveTricks[MMRT_*]`, read by `MM_TRICK`, never the CVar). MM's round
 *      starts from the live save as snapshotted (combo_logic.h, `beginQuery`), so
 *      the tricks-on run sets those bits in the live save before the round and
 *      puts back exactly the prior bytes after. Reserved keys stay inert however
 *      they are set (`MM_TRICK` forces them off), which mm-trick-table locks.
 *   2. PLANT ONE NEGATED EDGE for the red half. At runtime, on ONE connection or
 *      exit of the live `Rando::Logic::Regions` map, restored from a saved copy of
 *      the `std::function` after. No region file is edited and no negation is left
 *      in either graph's source.
 *   3. SAY WHICH ITEMS THE NEGATION CAN KEY ON (a mask the starting save does not
 *      already hold, so `HAS_ITEM` flips on the grant).
 *
 * NOTHING HERE RUNS IN A PRODUCTION PATH, consumes RNG, or touches a placement.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <vector>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/StaticData/StaticData.h"
#include "Rando/Logic/Logic.h"

extern "C" {
#include "variables.h"
}

namespace {

// ============================================================================
// The trick set
// ============================================================================

u8 sSavedTricks[MMRT_MAX];
bool sTricksForced = false;

// ============================================================================
// The planted negation (red half only)
// ============================================================================

/** The inventory item the planted condition negates. */
ItemId sNegItem = ITEM_NONE;

struct ArmedMmEdge {
    bool live = false;
    bool isExit = false;
    RandoRegionId source = RR_MAX;
    RandoRegionId target = RR_MAX;
    RandoRegionId connectionKey = RR_MAX; // when !isExit
    s32 exitKey = 0;                      // when isExit
    std::function<bool()> saved;
};
ArmedMmEdge sArmed;

/** The MM arrival region (ENTRANCE(SOUTH_CLOCK_TOWN, 0)), a crawl seed and so
 *  never a candidate — a seed is reachable whatever its inbound edges say. */
RandoRegionId ArrivalRegion() {
    return Rando::Logic::GetRegionIdFromEntrance(ENTRANCE(SOUTH_CLOCK_TOWN, 0));
}

} // namespace

/**
 * Force EVERY MM trick key's frozen bit on (`on` != 0) or put back exactly the
 * prior bytes (`on` == 0). Returns how many keys the forcing call turned on that
 * were off (anti-vacuity), 0 for a restore.
 */
extern "C" int MM_ComboMono_ForceAllTricks(int on) {
    u8* tricks = gSaveContext.save.shipSaveInfo.rando.randoSaveTricks;
    if (on != 0) {
        if (!sTricksForced) {
            memcpy(sSavedTricks, tricks, sizeof(sSavedTricks));
        }
        int turnedOn = 0;
        for (int i = 0; i < (int)MMRT_MAX; ++i) {
            if (tricks[i] == 0) {
                ++turnedOn;
            }
            tricks[i] = 1;
        }
        sTricksForced = true;
        return turnedOn;
    }
    if (sTricksForced) {
        memcpy(tricks, sSavedTricks, sizeof(sSavedTricks));
        sTricksForced = false;
    }
    return 0;
}

/** 1 iff `riId` gives a MASK (an inventory-slot item, so `HAS_ITEM` is a sound
 *  read of it) that the LIVE save does not already hold — the planted negation
 *  keyed on it is then true at the start of a round and flips on the grant. */
extern "C" int MM_ComboMono_NegatableItem(uint16_t riId) {
    const auto it = Rando::StaticData::Items.find((RandoItemId)riId);
    if (riId >= (uint16_t)RI_MAX || it == Rando::StaticData::Items.end()) {
        return 0;
    }
    const ItemId item = it->second.itemId;
    if (item < ITEM_MASK_DEKU || item > ITEM_MASK_GIANT) {
        return 0;
    }
    return HAS_ITEM(item) ? 0 : 1;
}

/**
 * RED HALF: plant ONE negated edge. Picks, deterministically (lowest region id),
 * a region T that is neither the virtual root nor the arrival seed, has EXACTLY
 * ONE inbound edge across every region's connections and exits, and whose source
 * is in `startRegions` — preferring a T that owns at least one check — and
 * replaces that one edge's condition with "the player does NOT hold the mask
 * `negRi` gives". The grant of `negRi` then makes T and its checks unreachable,
 * and a grow-check that is not vacuous must go red at exactly that grant.
 *
 * @return T, or -1 already armed / `negRi` not negatable, -2 no candidate.
 */
extern "C" int MM_ComboMono_ArmRegionNegation(const uint16_t* startRegions, int count, uint16_t negRi,
                                              uint16_t* outSource) {
    if (sArmed.live || MM_ComboMono_NegatableItem(negRi) != 1) {
        return -1;
    }
    std::set<int> start;
    for (int i = 0; i < count; ++i) {
        start.insert((int)startRegions[i]);
    }
    struct Edge {
        RandoRegionId source;
        bool isExit;
        RandoRegionId connectionKey;
        s32 exitKey;
    };
    std::map<int, int> inbound;
    std::map<int, Edge> soleEdge;
    for (auto& [sourceId, region] : Rando::Logic::Regions) {
        for (auto& [connectedId, condition] : region.connections) {
            (void)condition;
            inbound[(int)connectedId]++;
            soleEdge[(int)connectedId] = Edge{ sourceId, false, connectedId, 0 };
        }
        for (auto& [exitId, regionExit] : region.exits) {
            (void)regionExit;
            const RandoRegionId target = Rando::Logic::GetRegionIdFromEntrance(exitId);
            inbound[(int)target]++;
            soleEdge[(int)target] = Edge{ sourceId, true, RR_MAX, exitId };
        }
    }
    const int arrival = (int)ArrivalRegion();
    int chosen = -1;
    for (int pass = 0; pass < 2 && chosen < 0; ++pass) {
        for (const auto& [target, n] : inbound) { // std::map: ascending region id
            if (n != 1 || target == (int)RR_MAX || target == arrival) {
                continue;
            }
            const Edge& e = soleEdge[target];
            if (start.count((int)e.source) == 0 || (int)e.source == target) {
                continue;
            }
            const auto regionIt = Rando::Logic::Regions.find((RandoRegionId)target);
            if (regionIt == Rando::Logic::Regions.end()) {
                continue;
            }
            if (pass == 0 && regionIt->second.checks.empty()) {
                continue; // first pass: only a T with a check, so the CHECK set goes red too
            }
            chosen = target;
            break;
        }
    }
    if (chosen < 0) {
        return -2;
    }
    const Edge e = soleEdge[chosen];
    sNegItem = Rando::StaticData::Items.at((RandoItemId)negRi).itemId;
    // THE ONE NEGATED EDGE: "passable only while the player does NOT hold the
    // mask" — the shape ADR 0010 §2.3 bans, installed at runtime, test-only.
    std::function<bool()> negated = [] { return !HAS_ITEM(sNegItem); };
    Rando::Logic::RandoRegion& source = Rando::Logic::Regions[e.source];
    if (e.isExit) {
        sArmed.saved = source.exits[e.exitKey].condition;
        source.exits[e.exitKey].condition = negated;
    } else {
        sArmed.saved = source.connections[e.connectionKey].first;
        source.connections[e.connectionKey].first = negated;
    }
    sArmed.live = true;
    sArmed.isExit = e.isExit;
    sArmed.source = e.source;
    sArmed.target = (RandoRegionId)chosen;
    sArmed.connectionKey = e.connectionKey;
    sArmed.exitKey = e.exitKey;
    if (outSource != nullptr) {
        *outSource = (uint16_t)e.source;
    }
    printf("[MM ComboMono] planted ONE negated %s: region %d -> %d now requires NOT holding item 0x%02X (RI %u)\n",
           e.isExit ? "exit" : "connection", (int)e.source, chosen, (unsigned)sNegItem, (unsigned)negRi);
    return chosen;
}

/** Put the planted edge's original `std::function` back. 1 if disarmed, 0 if
 *  nothing was armed. */
extern "C" int MM_ComboMono_Disarm(void) {
    if (!sArmed.live) {
        return 0;
    }
    Rando::Logic::RandoRegion& source = Rando::Logic::Regions[sArmed.source];
    if (sArmed.isExit) {
        source.exits[sArmed.exitKey].condition = sArmed.saved;
    } else {
        source.connections[sArmed.connectionKey].first = sArmed.saved;
    }
    sArmed = ArmedMmEdge();
    sNegItem = ITEM_NONE;
    return 1;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
