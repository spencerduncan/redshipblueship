/**
 * @file mm_trick_bindings_test.cpp
 * @brief The red/green lock on every trick binding #578 part 2 authored. CTest
 *        row `mm-trick-bindings` (label `rando`), registered in
 *        src/common/test_runner.cpp.
 *
 * Part 1's two rows already do this shape for its two keys —
 * `mm-trick-table` for finding (a) (on `CanKillEnemy`, a plain inline function,
 * so it is graph-free) and `mm-trick-gbt-gate` for finding (b). This row is the
 * same idea generalised into a TABLE, because part 2 bound eight more keys and
 * eight hand-written probes would rot independently.
 *
 * WHY THE `rando` TIER, stated so nobody "tidies" it into the cheap one: every
 * edge here is a `std::function` inside `Rando::Logic::Regions`, and that map is
 * populated by file-scope ShipInit registrars reached only through
 * `InitOTRForMMFirstBoot`, whose OTRGlobals constructor builds a Fast3dWindow.
 *
 * ============================================================================
 * WHAT IT ASSERTS
 * ============================================================================
 *
 *  (a) PER EDGE, A RED/GREEN PAIR. For each row below: build a save that
 *      satisfies every conjunct of that edge EXCEPT the trick, locate the REAL
 *      lambda in the region graph (never a re-statement of its condition — a
 *      re-statement passes with the binding deleted, which is the whole failure
 *      this file exists to catch), then evaluate it with the frozen trick bit off
 *      and on. Off must be false, on must be true. A binding without both halves
 *      is theatre.
 *  (b) PER EDGE WHOSE TRICK CARRIES AN ITEM TERM, A NEGATIVE CONTROL. Take the
 *      item away and turn the trick ON: the edge must stay closed. This is what
 *      catches a disjunct that REPLACED the item requirement instead of joining
 *      it — e.g. `MM_TRICK(X) || HAS_ITEM(...)` where the trick's own definition
 *      says it needs the item.
 *  (c) COVERAGE, against the shipped `bound` flag rather than against a list in
 *      this file. Every key the pane describes as bound-and-not-reserved must be
 *      probed here or be one of part 1's two (which their own rows cover). So
 *      adding a key to `kBoundTricks` without a probe turns this row red, which
 *      is the anti-staleness lock OptionsUiSingleExe.cpp's comment says it does
 *      not otherwise have.
 *  (d) NON-VACUITY OF THE GRAPH ITSELF. Every region, check, exit key and event
 *      a row names must be present; a missing one fails loudly rather than
 *      skipping. A probe that silently evaluates nothing is worse than no probe.
 *  (e) MONOTONICITY over a REAL generated glitchless world, per bound key: the
 *      tricks-off reachable check set must be a SUBSET of the set with that one
 *      trick on. Every part-2 binding is a `||` disjunct, so enabling one can
 *      only ever widen reach — this is what catches an inverted polarity, a gate
 *      landing on the wrong side of an `&&`, or a mis-parenthesised edit that
 *      turns an existing term into a conjunct of the trick. It also holds
 *      whatever the numbers are, so it does not rot.
 *
 * Deltas are PRINTED, never pinned: pinning them would turn every unrelated pool
 * or logic edit into a failure of this row. The claim they support is the PR's —
 * that with the shipped default (T = ∅) nothing changed at all.
 *
 * WHAT IT DOES NOT ASSERT. That any binding matches OoTMM's own logic graph.
 * It cannot: #500's portability assessment is that all three reference graphs are
 * structurally incompatible with 2ship's `RandoRegionId` graph, so every binding
 * is hand-authored and its fidelity is an argument in the PR, not a computation.
 * What is mechanical is the shape: widening only, item terms preserved, both
 * directions of a two-way edge gated together.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <set>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/StaticData/StaticData.h"
#include "Rando/Logic/Logic.h"

// src/common — outside any extern "C" block; the header manages its own linkage
// (matching mm_trick_table_test.cpp).
#include "combo_mm_tricks_view.h"

extern "C" {
#include "variables.h"
}

// games/mm/2s2h/GameExports_SingleExe.cpp and z_sram_NES.c. Declared here rather
// than reached through a header because the single-exe export surface has none;
// mm_trick_table_test.cpp and mm_rando_gen_test.cpp name the same two.
extern "C" void MM_Rando_InitCore(void);
extern "C" void MM_Sram_InitNewSave(void);

namespace {

int BindFail(int code, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[MM-TRICK-BINDINGS] FAIL(%d): ", code);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    return code;
}

/**
 * A save with NO inventory at all: every slot ITEM_NONE (0xFF), not zero. A
 * zeroed `items[]` reads as holding item id 0x00, so `HAS_ITEM` would be true
 * for it — an "empty" inventory that silently holds something is exactly how a
 * reachability probe becomes vacuous. (Same helper, same reason, as
 * mm_trick_table_test.cpp's.)
 */
void ResetSaveWithEmptyInventory() {
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    memset(gSaveContext.save.saveInfo.inventory.items, ITEM_NONE, sizeof(gSaveContext.save.saveInfo.inventory.items));
}

void Give(ItemId itemId) {
    INV_CONTENT(itemId) = itemId;
}

// ---------------------------------------------------------------------------
// THE PROBE TABLE
// ---------------------------------------------------------------------------

enum EdgeKind {
    EDGE_CHECK,      // region.checks[target]
    EDGE_CONNECTION, // region.connections[target]
    EDGE_EXIT,       // region.exits[target].condition   (target is an ENTRANCE())
    EDGE_EVENT,      // region.events, matched on RandoEvent
};

struct Probe {
    MMRandoTrickId trick;
    /** What the row is about, printed on success and on failure. */
    const char* what;
    EdgeKind kind;
    RandoRegionId region;
    /** RandoCheckId / RandoRegionId / ENTRANCE() / RandoEvent, per `kind`. */
    int32_t target;
    /**
     * `gCurrentRegionTime` for this evaluation. TIME_ALL_SLICES makes every time
     * term true, which is what most rows want; a row whose edge has a time
     * DISJUNCT must narrow it, or the edge is already open with the trick off and
     * there is no red half. Clock shuffle is off in these saves (options are
     * zeroed), so ClockFilter() and the CLOCK_* terms are true regardless.
     */
    uint64_t time;
    /** Everything the edge needs EXCEPT the trick. */
    void (*inventory)();
    /**
     * Optional. The same save minus the item the TRICK itself names. With the
     * trick on, the edge must stay closed. NULL for a trick whose definition
     * carries no item term.
     */
    void (*withoutTrickItem)();
};

// Time masks. Day 1 only, for edges whose vanilla condition has a FINAL_DAY()
// disjunct: with every slice set, that disjunct is already true and the row
// would have no red half to observe.
constexpr uint64_t kAllTime = Rando::Logic::TIME_ALL_SLICES;
constexpr uint64_t kDay1Only = ((Rando::Logic::TIME_BIT_ONE << Rando::Logic::TIME_NIGHT1_PM_06_00) - 1);

void InvEmpty() {
}

void InvGoronAndBomb() {
    Give(ITEM_MASK_GORON);
    Give(ITEM_BOMB);
}

void InvGoronOnly() {
    Give(ITEM_MASK_GORON);
}

void InvSwimAbility() {
    // Human Link's swim is a shuffled ITEM here, so the trick's conjunct reads a
    // rando-inf flag rather than the inventory.
    Flags_SetRandoInf(RANDO_INF_OBTAINED_SWIM);
}

void InvBombchu() {
    Give(ITEM_BOMBCHU);
}

void InvBombchuAndGreatFairyMask() {
    Give(ITEM_BOMBCHU);
    Give(ITEM_MASK_GREAT_FAIRY);
}

void InvGreatFairyMaskOnly() {
    Give(ITEM_MASK_GREAT_FAIRY);
}

const Probe kProbes[] = {
    // MMRT_LENS. Empty inventory and no magic, so the only way in is the trick.
    { MMRT_LENS, "Lone Peak Shrine's invisible chest without Lens of Truth", EDGE_CHECK, RR_LONE_PEAK_SHRINE,
      (int32_t)RC_LONE_PEAK_SHRINE_INVISIBLE_CHEST, kAllTime, InvEmpty, NULL },
    // MMRT_DARMANI_WALL. The site MMRT_LENS excludes by name, which is why it
    // has its own key and its own row: if someone "simplified" the two into one
    // gate, this row and the MMRT_LENS row could not both be green.
    { MMRT_DARMANI_WALL, "the Mountain Village wall to the Goron Graveyard, climbed blind", EDGE_EXIT,
      RR_MOUNTAIN_VILLAGE, (int32_t)ENTRANCE(GORON_GRAVERYARD, 0), kAllTime, InvEmpty, NULL },
    // MMRT_PALACE_BEAN_SKIP. A wholly NEW edge, so the trick is the entire
    // condition; the red half is the edge existing in the map and refusing.
    { MMRT_PALACE_BEAN_SKIP, "Deku Palace's upper cell side from the lower floor, beanless", EDGE_CONNECTION,
      RR_DEKU_PALACE_INSIDE_LOWER, (int32_t)RR_DEKU_PALACE_INSIDE_UPPER_CELL_SIDE, kAllTime, InvEmpty, NULL },
    // MMRT_ZORA_HALL_HUMAN. No Zora Mask; the swim ABILITY is the trick's own
    // item term, so it is also the negative control.
    { MMRT_ZORA_HALL_HUMAN, "Zora Hall's back rooms as Human (Lulu's door)", EDGE_EXIT, RR_ZORA_HALL,
      (int32_t)ENTRANCE(ZORA_HALL_ROOMS, 2), kAllTime, InvSwimAbility, InvEmpty },
    // MMRT_GORON_BOMB_JUMP, both directions. Day 1 only, because the vanilla
    // condition has a FINAL_DAY() disjunct.
    { MMRT_GORON_BOMB_JUMP, "Milk Road over the fence as Goron", EDGE_CONNECTION, RR_MILK_ROAD,
      (int32_t)RR_MILK_ROAD_BEHIND_FENCE, kDay1Only, InvGoronAndBomb, InvGoronOnly },
    { MMRT_GORON_BOMB_JUMP, "Milk Road back over the fence as Goron (the mirror direction)", EDGE_CONNECTION,
      RR_MILK_ROAD_BEHIND_FENCE, (int32_t)RR_MILK_ROAD, kDay1Only, InvGoronAndBomb, InvGoronOnly },
    // MMRT_DOG_RACE_CHEST_NOTHING. "With Nothing" is literal: no item term.
    { MMRT_DOG_RACE_CHEST_NOTHING, "the Doggy Racetrack chest with nothing", EDGE_CHECK, RR_DOGGY_RACETRACK,
      (int32_t)RC_DOGGY_RACETRACK_CHEST, kAllTime, InvEmpty, NULL },
    // MMRT_POST_OFFICE_GAME. No Bunny Hood. Needs the time window, hence
    // kAllTime — the trick widens the MASK term, not the window.
    { MMRT_POST_OFFICE_GAME, "the Post Office game without the Bunny Hood", EDGE_CHECK, RR_POST_OFFICE,
      (int32_t)RC_CLOCK_TOWN_WEST_POSTMAN_MINIGAME, kAllTime, InvEmpty, NULL },
    // MMRT_HIVE_BOMBCHU, both hives the graph gates a check on.
    { MMRT_HIVE_BOMBCHU, "the Pirates' Fortress beehive with a Bombchu", EDGE_EVENT,
      RR_PIRATES_FORTRESS_CAPTAIN_ROOM_UPPER, (int32_t)RE_PIRATE_FORTRESS_BEEHIVE_HIT, kAllTime, InvBombchu, InvEmpty },
    { MMRT_HIVE_BOMBCHU, "Woodfall Temple's water-room beehive with a Bombchu and the Great Fairy Mask", EDGE_CHECK,
      RR_WOODFALL_TEMPLE_WATER_ROOM, (int32_t)RC_WOODFALL_TEMPLE_SF_WATER_ROOM_BEEHIVE, kAllTime,
      InvBombchuAndGreatFairyMask, InvGreatFairyMaskOnly },
};

/** Part 1's keys, whose red/green pairs live in their own rows. Named here so
 *  leg (c) can be TOTAL over the bound set rather than quietly partial. */
const MMRandoTrickId kCoveredElsewhere[] = {
    MMRT_KEG_EXPLOSIVES,   // mm-trick-table check (g)
    MMRT_GBT_BOSS_KEY_ICE, // mm-trick-gbt-gate
};

/**
 * The REAL lambda for one probe's edge, or NULL with `outWhy` set. Returning the
 * stored `std::function` rather than re-deriving the condition is the point of
 * the whole file.
 */
const std::function<bool()>* FindCondition(const Probe& probe, const char** outWhy) {
    auto regionIt = Rando::Logic::Regions.find(probe.region);
    if (regionIt == Rando::Logic::Regions.end()) {
        *outWhy = "the region is not in the graph";
        return NULL;
    }
    const Rando::Logic::RandoRegion& region = regionIt->second;
    switch (probe.kind) {
        case EDGE_CHECK: {
            auto it = region.checks.find((RandoCheckId)probe.target);
            if (it == region.checks.end()) {
                *outWhy = "the region has no such check";
                return NULL;
            }
            return &it->second.first;
        }
        case EDGE_CONNECTION: {
            auto it = region.connections.find((RandoRegionId)probe.target);
            if (it == region.connections.end()) {
                *outWhy = "the region has no connection to that region";
                return NULL;
            }
            return &it->second.first;
        }
        case EDGE_EXIT: {
            auto it = region.exits.find((s32)probe.target);
            if (it == region.exits.end()) {
                *outWhy = "the region has no exit at that entrance index";
                return NULL;
            }
            return &it->second.condition;
        }
        case EDGE_EVENT: {
            for (const auto& entry : region.events) {
                if ((int32_t)entry.first == probe.target) {
                    return &entry.second;
                }
            }
            *outWhy = "the region does not set that event";
            return NULL;
        }
    }
    *outWhy = "unknown edge kind";
    return NULL;
}

void SetFrozenTrick(MMRandoTrickId mmRandoTrickId, bool on) {
    gSaveContext.save.shipSaveInfo.rando.randoSaveTricks[mmRandoTrickId] = on ? 1 : 0;
}

/** Stand up the save for one probe arm: empty, then the row's own setup. */
void ArmSave(const Probe& probe, void (*inventory)()) {
    ResetSaveWithEmptyInventory();
    inventory();
    Rando::Logic::gCurrentRegionTime = probe.time;
}

const char* TrickName(MMRandoTrickId id) {
    auto it = Rando::StaticData::Tricks.find(id);
    return it == Rando::StaticData::Tricks.end() ? "(unknown key)" : it->second.name;
}

} // namespace

extern "C" int MM_TrickBindings_RunHeadless(void) {
    printf("[TEST] mm-trick-bindings: each of part 2's trick bindings closes its edge with the trick off and opens "
           "it with the trick on (#578 part 2)\n");

    // Populate the graph. The region definitions are file-scope ShipInit
    // registrars and InitOTRForMMFirstBoot does NOT fire them — MM_Rando_InitCore
    // is what calls S2H::ShipInit::InitAll(). CORE only, never the asset phase:
    // latching that here with no archives mounted would rob a later real boot of
    // GfxPatcher and the tracker icons. It is idempotent.
    MM_Rando_InitCore();
    // Registers are read by some logic predicates through R_* macros and are only
    // allocated by MM_Regs_Init on a real boot. Same guard, same reason, as the
    // creation seam and the other headless rows.
    if (gRegEditor == NULL) {
        static RegEditor sProbeRegEditor = {};
        gRegEditor = &sProbeRegEditor;
    }
    // The pane's descriptor table, for leg (c). Idempotent; the window init calls
    // it too.
    MM_RandoTricksUi_Register();

    // Heap, not stack: MM's runtime SaveContext is tens of kilobytes and this
    // runs at arbitrary harness stack depths.
    auto saved = std::make_unique<SaveContext>();
    memcpy(saved.get(), &gSaveContext, sizeof(SaveContext));
    const uint64_t savedTime = Rando::Logic::gCurrentRegionTime;

    int rc = 0;

    // ---- (a), (b), (d): the per-edge red/green pairs ----------------------
    for (const Probe& probe : kProbes) {
        const char* why = "";
        const std::function<bool()>* condition = FindCondition(probe, &why);
        if (condition == NULL) {
            rc = BindFail(1, "%s [%s]: %s — the binding's edge is gone, so this probe would be vacuous", probe.what,
                          TrickName(probe.trick), why);
            break;
        }

        ArmSave(probe, probe.inventory);
        SetFrozenTrick(probe.trick, false);
        if ((*condition)()) {
            rc = BindFail(2,
                          "%s [%s]: the edge is OPEN with the trick off — either the disjunct is ungated or this "
                          "row's save satisfies the edge without it, which makes the green half meaningless",
                          probe.what, TrickName(probe.trick));
            break;
        }

        SetFrozenTrick(probe.trick, true);
        if (!(*condition)()) {
            rc = BindFail(3,
                          "%s [%s]: the edge is CLOSED with the trick on — the binding is not wired, or a term "
                          "this row's save does not satisfy was made a conjunct of the trick",
                          probe.what, TrickName(probe.trick));
            break;
        }

        if (probe.withoutTrickItem != NULL) {
            ArmSave(probe, probe.withoutTrickItem);
            SetFrozenTrick(probe.trick, true);
            if ((*condition)()) {
                rc = BindFail(4,
                              "%s [%s]: the trick ALONE opens the edge, without the item its own definition names — "
                              "the disjunct replaced the item requirement instead of joining it",
                              probe.what, TrickName(probe.trick));
                break;
            }
        }

        printf("[TEST]   ok: %s [%s]%s\n", probe.what, TrickName(probe.trick),
               probe.withoutTrickItem != NULL ? " (+ item-still-required control)" : "");
    }

    // ---- (c): coverage, measured against the SHIPPED bound flag -----------
    if (rc == 0) {
        int boundCount = 0;
        for (int id = 0; id < MMRT_MAX && rc == 0; id++) {
            const ComboMMTrickDesc* desc = Combo_MMTrickById((uint16_t)id);
            if (desc == NULL) {
                rc = BindFail(5, "the descriptor table has no row for id %d, so coverage cannot be measured", id);
                break;
            }
            if (!desc->bound || desc->reserved) {
                continue;
            }
            boundCount++;
            bool covered = false;
            for (const Probe& probe : kProbes) {
                if (probe.trick == (MMRandoTrickId)id) {
                    covered = true;
                    break;
                }
            }
            for (MMRandoTrickId elsewhere : kCoveredElsewhere) {
                if (elsewhere == (MMRandoTrickId)id) {
                    covered = true;
                    break;
                }
            }
            if (!covered) {
                rc = BindFail(6,
                              "'%s' is shipped as BOUND but nothing probes it — either add a row here or take it "
                              "out of kBoundTricks; a bound key with no red/green pair is exactly the theatre this "
                              "row exists to prevent",
                              desc->name);
            }
        }
        if (rc == 0) {
            if (boundCount == 0) {
                rc = BindFail(7, "no key is described as bound at all, so the coverage leg passed vacuously");
            } else {
                printf("[TEST]   ok: all %d bound-and-live keys are probed (%d edges here, %d keys in part 1's own "
                       "rows)\n",
                       boundCount, (int)(sizeof(kProbes) / sizeof(kProbes[0])),
                       (int)(sizeof(kCoveredElsewhere) / sizeof(kCoveredElsewhere[0])));
            }
        }
    }

    // ---- (e): monotonicity over a real generated glitchless world ---------
    //
    // The legs above prove each edge moves. This one proves that moving it only
    // ever WIDENS, over the whole graph, on the rung the bindings are about: the
    // shipped Glitchless default (ADR 0010 O11). One world, generated once, then
    // measured once per key with only the frozen trick bit changed — a fresh
    // generation per arm would confound the fill with the logic.
    std::vector<MMRandoTrickId> probedKeys;
    for (const Probe& probe : kProbes) {
        if (std::find(probedKeys.begin(), probedKeys.end(), probe.trick) == probedKeys.end()) {
            probedKeys.push_back(probe.trick);
        }
    }
    if (rc == 0) {
        // GLITCHLESS on purpose: under Nearly No Logic every check is reachable
        // regardless, so every delta would be 0 for a reason that says nothing.
        CVarSetInteger("gRando.Enabled", 1);
        CVarSetInteger("gRando.GenerateSpoiler", 0);
        CVarSetInteger(Rando::StaticData::Options[RO_LOGIC].cvar, RO_LOGIC_GLITCHLESS);
        // A fixed seed LIST, same reason mm-rando-gen and mm-trick-gbt-gate carry
        // one: MM's glitchless fill is a forward fill that can genuinely dead-end
        // on an unlucky seed, deterministically. Take the first that converges.
        static const char* kSeeds[] = { "RSBSBIND1", "RSBSBIND2", "RSBSBIND3", "RSBSBIND4", "RSBSBIND5" };
        bool generated = false;
        for (const char* seed : kSeeds) {
            CVarSetString("gRando.InputSeed", seed);
            memset(&gSaveContext, 0, sizeof(gSaveContext));
            MM_Sram_InitNewSave();
            GameInteractor_ExecuteOnSaveInit(0);
            if (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) {
                printf("[TEST]   reachability world: glitchless solo rando on seed '%s'\n", seed);
                generated = true;
                break;
            }
        }
        if (!generated) {
            rc = BindFail(8, "no seed produced a glitchless rando world, so the monotonicity leg would be measured "
                             "over a vanilla save");
        }
    }
    if (rc == 0) {
        // The shipped default IS tricks-off: nothing set the gRando.Tricks.*
        // CVars, so the resolution froze zeroes. Asserted rather than assumed,
        // because the arms below subtract from it.
        for (int id = 0; id < MMRT_MAX; id++) {
            if (gSaveContext.save.shipSaveInfo.rando.randoSaveTricks[id] != 0) {
                rc = BindFail(9,
                              "a freshly generated world froze trick id %d ON with no CVar set — the shipped default "
                              "is not T = empty, so 'tricks-off logic did not change' would be unmeasurable",
                              id);
                break;
            }
        }
    }
    if (rc == 0) {
        auto measure = [](MMRandoTrickId trick) {
            if (trick != MMRT_MAX) {
                gSaveContext.save.shipSaveInfo.rando.randoSaveTricks[trick] = 1;
            }
            std::set<RandoCheckId> set = Rando::Logic::ComputeReachableCheckSet();
            if (trick != MMRT_MAX) {
                gSaveContext.save.shipSaveInfo.rando.randoSaveTricks[trick] = 0;
            }
            return set;
        };
        const std::set<RandoCheckId> tricksOff = measure(MMRT_MAX);
        if (tricksOff.empty()) {
            rc = BindFail(10, "the tricks-off reachable check set is EMPTY, so every subset comparison below would "
                              "pass vacuously");
        }
        for (MMRandoTrickId trick : probedKeys) {
            if (rc != 0) {
                break;
            }
            const std::set<RandoCheckId> on = measure(trick);
            if (!std::includes(on.begin(), on.end(), tricksOff.begin(), tricksOff.end())) {
                rc = BindFail(11,
                              "turning '%s' ON made some check UNREACHABLE — a trick binding must only ever widen, "
                              "so this is an inverted polarity or a disjunct that became a conjunct",
                              TrickName(trick));
                break;
            }
            // REPORTED, not asserted: a key can legitimately unlock nothing over
            // a full-closure crawl (the item behind it is obtainable another way
            // eventually) without its edge being unbound — legs (a)/(b) are what
            // prove the edge.
            printf("[TEST]   reachable checks: tricks-off %d, +%s %d (delta %d)\n", (int)tricksOff.size(),
                   TrickName(trick), (int)on.size(), (int)(on.size() - tricksOff.size()));
        }
    }

    // Leave global state as it was found. This row sets four CVars to stand up
    // its world, and a leaked gRando.Enabled would silently reconfigure whatever
    // runs after it in the same process.
    CVarClear("gRando.Enabled");
    CVarClear("gRando.GenerateSpoiler");
    CVarClear("gRando.InputSeed");
    CVarClear(Rando::StaticData::Options[RO_LOGIC].cvar);
    memcpy(&gSaveContext, saved.get(), sizeof(SaveContext));
    Rando::Logic::gCurrentRegionTime = savedTime;

    if (rc == 0) {
        printf("[TEST] PASS: %d edges across %d trick keys are closed with the trick off and open with it on, every "
               "shipped bound key is probed, and no key removes reach when enabled\n",
               (int)(sizeof(kProbes) / sizeof(kProbes[0])), (int)probedKeys.size());
    }
    return rc;
}

#endif // RSBS_SINGLE_EXECUTABLE
