/**
 * ComboLogicMonotonicityOoT.cpp — the OoT half of the monotonicity grow-check's
 * test surface (ADR 0010 answer O6; the `combo-logic-monotonicity` row, #645).
 *
 * ============================================================================
 * WHAT THIS IS
 * ============================================================================
 *
 * ADR 0010 §2.3 makes the combo fill sound only while reachability is MONOTONE
 * in the player's items, and answer O6 enforces that three ways at once: a
 * review rule, a static probe over the condition sources
 * (.github/scripts/check-monotonicity-negations.py), and a CI GROW-CHECK that
 * grants items one copy at a time through the real engines and asserts the
 * reached check and region sets never shrink. The grow-check lives in
 * src/common/tests/test_combo_logic_monotonicity.c and drives the OoT engine
 * through the coordinator's own vtable (`Combo_Logic_GetEngine(GAME_OOT)`).
 * What it cannot do from src/common — because src/common includes no game
 * header (ADR 0002) — is the four things below, so they live here:
 *
 *   1. READ THE REGION SET. The vtable reports checks (`checkReached`) but not
 *      regions, and a region can open for an age/time without any new check
 *      becoming reachable. `OoT_ComboMono_RegionBits` exposes the four
 *      age/time bits of every region, read-only, exactly as the engine's own
 *      `expand` counts them.
 *   2. FORCE A TRICKS-ON SET. OoT's tricks are frozen `Rando::Context` options;
 *      the row measures the operator with every trick on as well as with every
 *      trick off. `OoT_ComboMono_ForceAllTricks` saves and restores the exact
 *      prior values.
 *   3. PLANT ONE NEGATED EDGE, for the row's red half. Nothing here edits a
 *      region file: the negation is installed at runtime on ONE `Entrance` of the
 *      live `areaTable` and the whole `Entrance` object is copy-restored after.
 *      No negation is left in either graph's source (the static probe would
 *      refuse one).
 *   4. SAY WHICH ITEMS THE NEGATION CAN KEY ON (an item whose effect sets a
 *      `LogicVal`, which the planted condition reads back).
 *
 * NOTHING HERE RUNS IN A PRODUCTION PATH. No shipping caller references any of
 * these symbols; they exist for one CTest row. Nothing here consumes RNG or
 * touches a placement, so no generated world can move.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>

#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/randomizer/entrance.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/SeedContext.h"

namespace {

// ============================================================================
// The planted negation (red half only)
// ============================================================================

/** The logic value the planted condition negates. LOGIC_NONE while disarmed. */
LogicVal sNegLogicVal = LOGIC_NONE;

/**
 * THE ONE NEGATED EDGE: "passable only while the player does NOT hold X". This
 * is precisely the shape ADR 0010 §2.3 bans — a `!` over a player-state term —
 * and it is here, in a test-only TU, as a runtime condition and never as source
 * in a region file. Capture-less on purpose: OoT's `ConditionFn` is a plain
 * function pointer, so the item it keys on is a file-static.
 */
bool OoTComboMonoNegatedCondition() {
    return !logic->Get(sNegLogicVal);
}

struct ArmedEdge {
    bool live = false;
    Rando::Entrance* entrance = nullptr; // into the live areaTable (a std::list: stable address)
    std::vector<Rando::Entrance> saved;  // 0 or 1 element: the whole object as it was
    RandomizerRegion parent = RR_NONE;
    RandomizerRegion target = RR_NONE;
};
ArmedEdge sArmed;

// ============================================================================
// The trick set
// ============================================================================

std::vector<uint8_t> sSavedTricks;
bool sTricksForced = false;

bool OoTComboMonoReady() {
    return Rando::Context::GetInstance() != nullptr && logic != nullptr;
}

} // namespace

/**
 * The four age/time access bits of every region, as `expand` left them:
 * bit 0 childDay, bit 1 childNight, bit 2 adultDay, bit 3 adultNight.
 * READ-ONLY. Writes at most `cap` entries (index = RandomizerRegion) and
 * returns the id-space size (RR_MAX); -1 when OoT's randomizer is not up.
 */
extern "C" int OoT_ComboMono_RegionBits(uint8_t* out, int cap) {
    if (!OoTComboMonoReady()) {
        return -1;
    }
    for (int i = 0; i < (int)RR_MAX; ++i) {
        uint8_t bits = 0;
        if (i > (int)RR_NONE) {
            const Region* region = RegionTable((RandomizerRegion)i);
            if (region != nullptr) {
                bits = (uint8_t)((region->childDay ? 1u : 0u) | (region->childNight ? 2u : 0u) |
                                 (region->adultDay ? 4u : 0u) | (region->adultNight ? 8u : 0u));
            }
        }
        if (out != nullptr && i < cap) {
            out[i] = bits;
        }
    }
    return (int)RR_MAX;
}

/**
 * Force EVERY trick on (`on` != 0) or put back exactly what was there (`on` == 0).
 * The first forcing call saves the prior value of every key; a second forcing call
 * before a restore does not overwrite that save. Returns how many keys the forcing
 * call turned ON that were off (the lock's anti-vacuity: a "tricks-on" run that
 * changed nothing would be the tricks-off run twice), 0 for a restore, -1 when
 * OoT's randomizer is not up.
 */
extern "C" int OoT_ComboMono_ForceAllTricks(int on) {
    if (!OoTComboMonoReady()) {
        return -1;
    }
    auto ctx = Rando::Context::GetInstance();
    if (on != 0) {
        if (!sTricksForced) {
            sSavedTricks.assign((size_t)RT_MAX, 0);
            for (int rt = 0; rt < (int)RT_MAX; ++rt) {
                sSavedTricks[(size_t)rt] = ctx->GetTrickOption((RandomizerTrick)rt).Get();
            }
        }
        int turnedOn = 0;
        for (int rt = 0; rt < (int)RT_MAX; ++rt) {
            Rando::OptionValue& opt = ctx->GetTrickOption((RandomizerTrick)rt);
            if (opt.Get() != (uint8_t)RO_GENERIC_ON) {
                ++turnedOn;
            }
            opt.Set((uint8_t)RO_GENERIC_ON);
        }
        sTricksForced = true;
        return turnedOn;
    }
    if (sTricksForced) {
        for (int rt = 0; rt < (int)RT_MAX; ++rt) {
            ctx->GetTrickOption((RandomizerTrick)rt).Set(sSavedTricks[(size_t)rt]);
        }
        sTricksForced = false;
    }
    return 0;
}

/** 1 iff `itemId` is a real OoT item whose effect sets a `LogicVal` — i.e. the
 *  planted negation can key on it and will flip when a copy is granted. */
extern "C" int OoT_ComboMono_NegatableItem(uint16_t itemId) {
    if (!OoTComboMonoReady() || itemId <= (uint16_t)RG_NONE || itemId >= (uint16_t)RG_MAX) {
        return 0;
    }
    const Rando::Item& item = Rando::StaticData::RetrieveItem((RandomizerGet)itemId);
    if (item.GetRandomizerGet() != (RandomizerGet)itemId) {
        return 0;
    }
    return item.GetLogicVal() != LOGIC_NONE ? 1 : 0;
}

/**
 * RED HALF: plant ONE negated edge. Picks, deterministically (lowest region id),
 * a region T other than the root that has EXACTLY ONE inbound exit in the live
 * graph, whose parent is in `startRegions` (regions the round reached before any
 * grant), preferring a T that owns at least one location — then replaces that one
 * exit's condition with "the player does NOT hold `negItem`". While `negItem` is
 * not held, T is reachable from its parent; the grant of `negItem` makes T (and
 * every check in it) unreachable. A grow-check that is not vacuous must go red at
 * exactly that grant.
 *
 * @return T (a RandomizerRegion), or -1 not ready / already armed / `negItem`
 *         not negatable, -2 no candidate region. `outParent` receives T's parent.
 */
extern "C" int OoT_ComboMono_ArmRegionNegation(const uint16_t* startRegions, int count, uint16_t negItem,
                                               uint16_t* outParent) {
    if (!OoTComboMonoReady() || sArmed.live || OoT_ComboMono_NegatableItem(negItem) != 1) {
        return -1;
    }
    std::set<int> start;
    for (int i = 0; i < count; ++i) {
        start.insert((int)startRegions[i]);
    }
    std::vector<int> inbound((size_t)RR_MAX, 0);
    std::vector<Rando::Entrance*> soleEdge((size_t)RR_MAX, nullptr);
    for (int r = (int)RR_NONE + 1; r < (int)RR_MAX; ++r) {
        Region* region = RegionTable((RandomizerRegion)r);
        if (region == nullptr) {
            continue;
        }
        for (Rando::Entrance& exit : region->exits) {
            const int target = (int)exit.GetConnectedRegionKey();
            if (target <= (int)RR_NONE || target >= (int)RR_MAX) {
                continue;
            }
            inbound[(size_t)target]++;
            soleEdge[(size_t)target] = &exit;
        }
    }
    int chosen = -1;
    for (int pass = 0; pass < 2 && chosen < 0; ++pass) {
        for (int t = (int)RR_NONE + 1; t < (int)RR_MAX; ++t) {
            if (t == (int)RR_ROOT || inbound[(size_t)t] != 1 || soleEdge[(size_t)t] == nullptr) {
                continue;
            }
            const int parent = (int)soleEdge[(size_t)t]->GetParentRegionKey();
            if (start.count(parent) == 0 || parent == t) {
                continue;
            }
            const Region* target = RegionTable((RandomizerRegion)t);
            if (pass == 0 && (target == nullptr || target->locations.empty())) {
                continue; // first pass: only a T with a location, so the CHECK set goes red too
            }
            chosen = t;
            break;
        }
    }
    if (chosen < 0) {
        return -2;
    }
    Rando::Entrance* edge = soleEdge[(size_t)chosen];
    sArmed.saved.assign(1, *edge);
    sArmed.entrance = edge;
    sArmed.parent = edge->GetParentRegionKey();
    sArmed.target = (RandomizerRegion)chosen;
    sNegLogicVal = Rando::StaticData::RetrieveItem((RandomizerGet)negItem).GetLogicVal();
    edge->SetCondition(OoTComboMonoNegatedCondition);
    sArmed.live = true;
    if (outParent != nullptr) {
        *outParent = (uint16_t)sArmed.parent;
    }
    printf("[OoT/ComboMono] planted ONE negated edge: region %d -> %d now requires NOT holding item %u (logic "
           "value %d)\n",
           (int)sArmed.parent, chosen, (unsigned)negItem, (int)sNegLogicVal);
    return chosen;
}

/** Put the planted edge back exactly as it was (the whole `Entrance` object).
 *  Returns 1 if something was disarmed, 0 if nothing was armed. */
extern "C" int OoT_ComboMono_Disarm(void) {
    if (!sArmed.live) {
        return 0;
    }
    *sArmed.entrance = sArmed.saved[0];
    sArmed = ArmedEdge();
    sNegLogicVal = LOGIC_NONE;
    return 1;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
