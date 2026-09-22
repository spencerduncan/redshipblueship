/**
 * @file mm_trick_bindings_test.cpp
 * @brief The red/green lock on every trick binding #578 parts 2 and 3 authored.
 *        CTest row `mm-trick-bindings` (label `rando`), registered in
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
 *  (b) PER EDGE THAT STILL REQUIRES SOMETHING, A NEGATIVE CONTROL. Take that
 *      requirement away and turn the trick ON: the edge must stay closed. This
 *      catches a disjunct that REPLACED a requirement instead of joining it —
 *      `MM_TRICK(X) || HAS_ITEM(...)` where it should have been `&&`.
 *      SCOPE, stated exactly because an earlier version of this comment
 *      overclaimed: leg (b) proves only that the trick did not replace THE TERM
 *      THIS ARM REMOVES. On a `(A || MM_TRICK(X)) && B` widening whose control
 *      removes something inside the trick's own disjunct, it says nothing about
 *      B — with `(A || (X && G)) && B` and a control that clears G, an edit that
 *      had swallowed B into the parentheses passes every arm. That hole is what
 *      leg (f) closes.
 *  (f) PER SURVIVING OUTER CONJUNCT, AN ARM THAT CLEARS IT. Where the vanilla
 *      condition keeps a conjunct outside the widened term — `(A || MM_TRICK(X))
 *      && B` — satisfy the VANILLA disjunct A, clear B, and require the edge shut
 *      with the trick both OFF and ON. Off catches B swallowed into the
 *      parentheses (`A || (X && G && B)`), which is a tricks-OFF widening and the
 *      mis-parenthesisation legs (a)/(b) cannot see; on catches B deleted
 *      outright. `kSurvivorProbes` holds these, and unlike the other legs it does
 *      not stop at the first failure, so one broken build shows every arm it
 *      breaks.
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
 *      trick on. Every binding is a `||` disjunct, so enabling one can only ever
 *      widen reach — this catches an inverted polarity or a gate that makes
 *      turning the trick ON take reach AWAY.
 *      WHAT LEG (e) DOES NOT DO, because an earlier version of this file leaned
 *      on it for coverage it cannot provide: both sets come from the SAME binary,
 *      one with the bit off and one with it on, so a tricks-OFF TIGHTENING (an
 *      `&&` where a `||` was meant, an existing term pulled inside the trick's
 *      parentheses) drops the check from BOTH sets and the subset relation still
 *      holds. Leg (e) therefore cannot stand in for a probe on an unprobed edge,
 *      and it is not evidence that a sibling row survived an edit. What covers a
 *      tricks-off tightening is the per-edge red half above plus the main-binary
 *      vs branch-binary digest comparison in the PR; what covers each edge is its
 *      own row, which is why all eight ISTT rupees, both Tingle maps and both
 *      bank rows have one.
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
     * Optional. The same save minus ONE thing the edge must still require with
     * the trick on: the item the trick's own definition names, or a conjunct of
     * the vanilla condition that the widening had to leave standing. With the
     * trick on, the edge must stay closed. NULL only when the trick carries no
     * item term AND the widened term was the edge's whole condition — then there
     * is nothing left to take away and an arm here would pass vacuously.
     */
    void (*withoutTrickItem)();
};

// Time masks. Day 1 only, for edges whose vanilla condition has a FINAL_DAY()
// disjunct: with every slice set, that disjunct is already true and the row
// would have no red half to observe.
constexpr uint64_t kAllTime = Rando::Logic::TIME_ALL_SLICES;
constexpr uint64_t kDay1Only = ((Rando::Logic::TIME_BIT_ONE << Rando::Logic::TIME_NIGHT1_PM_06_00) - 1);
// Night 1 only, for leg (f) on an edge whose surviving conjunct is IS_DAY(): every
// slice here sits at or above TIME_NIGHT1_PM_06_00 and below TIME_DAY2_AM_06_00, so
// RawBefore(NIGHT1_PM_06_00) and both RawBetween day windows are empty and IS_DAY()
// is false whatever the CLOCK_* half-day terms say.
constexpr uint64_t kNight1Only = ((Rando::Logic::TIME_BIT_ONE << Rando::Logic::TIME_DAY2_AM_06_00) - 1) &
                                 ~((Rando::Logic::TIME_BIT_ONE << Rando::Logic::TIME_NIGHT1_PM_06_00) - 1);

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

// ---- part 3's setups -----------------------------------------------------

void InvZoraMask() {
    Give(ITEM_MASK_ZORA);
}

void InvHookshot() {
    Give(ITEM_HOOKSHOT);
}

void InvBow() {
    Give(ITEM_BOW);
}

/** The Great Fairy's Sword rather than an equipped blade: CAN_USE_HUMAN_SWORD
 *  reads the equips bitfield, and an item slot is what Give() can set. */
void InvGreatFairySword() {
    Give(ITEM_SWORD_GREAT_FAIRY);
}

void InvGoronAndDeedLand() {
    Give(ITEM_MASK_GORON);
    Flags_SetRandoInf(RANDO_INF_OBTAINED_DEED_LAND);
}

void InvDeedLandOnly() {
    Flags_SetRandoInf(RANDO_INF_OBTAINED_DEED_LAND);
}

void InvGoronAndDeedMountain() {
    Give(ITEM_MASK_GORON);
    Flags_SetRandoInf(RANDO_INF_OBTAINED_DEED_MOUNTAIN);
}

void InvDeedMountainOnly() {
    Flags_SetRandoInf(RANDO_INF_OBTAINED_DEED_MOUNTAIN);
}

// ---- leg (f)'s setups: the vanilla route satisfied, one outer conjunct NOT ----

/** The Deku-flower route up, and no title deed of either kind. */
void InvDekuOnly() {
    Give(ITEM_MASK_DEKU);
}

/** Deku AND Goron — every mask term the Zora Hall row names — but no deed. */
void InvDekuAndGoron() {
    Give(ITEM_MASK_DEKU);
    Give(ITEM_MASK_GORON);
}

/** The Mountain Title Deed and the Deku route, but not the outer CAN_BE_GORON. */
void InvDekuAndDeedMountain() {
    Give(ITEM_MASK_DEKU);
    Flags_SetRandoInf(RANDO_INF_OBTAINED_DEED_MOUNTAIN);
}

// ---- part 3's second pass ------------------------------------------------

/** CAN_USE_MAGIC_ARROW(ICE) in full: the Bow, the Ice Arrows, and magic. All
 *  three, because the macro is a conjunction and a save missing any one of them
 *  gives a green half that never goes green. */
void InvBowIceArrowsAndMagic() {
    Give(ITEM_BOW);
    Give(ITEM_ARROW_ICE);
    gSaveContext.save.saveInfo.playerData.isMagicAcquired = true;
}

/** Three Stone Tower Temple small keys AND the Goron Mask: the wind room's
 *  outbound edge is `KEY_COUNT(..) >= 3 && (CAN_BE_DEKU || (trick && (CAN_BE_GORON
 *  || bomb)))`, so the green half needs the key count satisfied and no Deku Mask.
 *  KEY_COUNT reads shipSaveInfo.rando.foundDungeonKeys, which is the RANDO tally
 *  and not inventory.dungeonKeys — the wrong one of those two is a green half that
 *  never goes green. */
void InvStoneTowerKeys3AndGoron() {
    gSaveContext.save.shipSaveInfo.rando.foundDungeonKeys[DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE] = 3;
    Give(ITEM_MASK_GORON);
}

/** Leg (b)'s control for that edge: the keys stay, the trick's own terms (Goron
 *  Mask, bomb) are gone, so with the trick ON the edge must still refuse. */
void InvStoneTowerKeys3Only() {
    gSaveContext.save.shipSaveInfo.rando.foundDungeonKeys[DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE] = 3;
}

/** Leg (f)'s arm for that edge: the VANILLA disjunct satisfied (Deku Mask) and
 *  the surviving outer conjunct — the three small keys — cleared. Shut with the
 *  trick off and on, or KEY_COUNT was deleted or swallowed into the trick's
 *  parentheses. */
void InvDekuNoStoneTowerKeys() {
    Give(ITEM_MASK_DEKU);
}

/** The same save minus the ONE item the trick's own text names — the Ice
 *  Arrows — so the control takes away the arrow rather than the whole bow-and-
 *  magic apparatus. */
void InvBowAndMagicNoIceArrows() {
    Give(ITEM_BOW);
    gSaveContext.save.saveInfo.playerData.isMagicAcquired = true;
}

/** A Bow (so CAN_USE_PROJECTILE, the vanilla disjunct, is true) AND the sword the
 *  trick names, so the arm is meaningful with the trick both off and on; what the
 *  row withholds is IS_DAY(), through its clock rather than its inventory. */
void InvBowAndGreatFairySword() {
    Give(ITEM_BOW);
    Give(ITEM_SWORD_GREAT_FAIRY);
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

    // ---- #578 part 3 ----------------------------------------------------
    //
    // Every row below is a plain widening over a key part 1 declared. Where the
    // control arm takes away a VANILLA conjunct rather than an item the trick
    // names, the row says so: that arm is what proves the surviving conjunct is
    // still outside the parentheses.

    // MMRT_NO_SEAHORSE. Zora Mask and no pictograph box, so the seahorse event is
    // false; the control removes the Zora Mask, which the widening kept.
    { MMRT_NO_SEAHORSE, "Pinnacle Rock's interior without the seahorse to guide you", EDGE_CONNECTION,
      RR_PINNACLE_ROCK_ENTRANCE, (int32_t)RR_PINNACLE_ROCK_INNER, kAllTime, InvZoraMask, InvEmpty },
    // MMRT_ICELESS_IKANA. Hookshot and no Ice Arrows. The Hookshot is both the
    // trick's own item term and the vanilla conjunct, so one control covers both.
    { MMRT_ICELESS_IKANA, "Ikana Canyon's upper half by hookshotting the first tree, without Ice Arrows",
      EDGE_CONNECTION, RR_IKANA_CANYON_LOWER, (int32_t)RR_IKANA_CANYON_UPPER, kAllTime, InvHookshot, InvEmpty },
    // MMRT_BOMBER_GUESS. No hide-and-seek events are set in a zeroed save, so all
    // three day-triples are false. "Guess the code" carries no item term.
    { MMRT_BOMBER_GUESS, "the Bombers' code guessed instead of played for", EDGE_EVENT, RR_CLOCK_TOWN_NORTH,
      (int32_t)RE_BOMBER_CODE, kAllTime, InvEmpty, NULL },
    // MMRT_SHT_PILLAR_ROOM_HOOKSHOT. Hookshot only: the vanilla route wants Deku
    // AND Fire Arrows, so the red half is genuinely closed.
    { MMRT_SHT_PILLAR_ROOM_HOOKSHOT, "Snowhead Temple's pillar room from the ground floor with a Hookshot",
      EDGE_CONNECTION, RR_SNOWHEAD_TEMPLE_PILLARS_ROOM_LOWER, (int32_t)RR_SNOWHEAD_TEMPLE_PILLARS_ROOM_UPPER, kAllTime,
      InvHookshot, InvEmpty },
    // MMRT_SOUTHERN_SWAMP_SCRUB_HP_GORON. Goron Mask + the Land Title Deed and no
    // Deku Mask. The control keeps the deed and drops the Goron Mask — the trick's
    // own item term.
    { MMRT_SOUTHERN_SWAMP_SCRUB_HP_GORON, "the Southern Swamp scrub's heart piece by Goron pound, without Deku",
      EDGE_CHECK, RR_SOUTHERN_SWAMP_NORTH, (int32_t)RC_SOUTHERN_SWAMP_PIECE_OF_HEART, kAllTime, InvGoronAndDeedLand,
      InvDeedLandOnly },
    // MMRT_ZORA_HALL_SCRUB_HP_NO_DEKU. Same shape one deed over; dropping the
    // Goron Mask also drops the vanilla CAN_BE_GORON conjunct, so this control
    // covers the trick's term and the surviving term at once.
    { MMRT_ZORA_HALL_SCRUB_HP_NO_DEKU, "the Zora Hall scrub's heart piece as Goron, without Deku", EDGE_CHECK,
      RR_ZORA_HALL_LULUS_ROOM, (int32_t)RC_ZORA_HALL_SCRUB_PIECE_OF_HEART, kAllTime, InvGoronAndDeedMountain,
      InvDeedMountainOnly },
    // MMRT_WELL_HSW. No Bow, so the Dexihand cannot be killed. "Grab the water
    // before the hand grabs you" carries no item term and the Bow term was the
    // event's whole condition, so there is nothing left for a control arm.
    { MMRT_WELL_HSW, "the well's hot spring water without killing the Dexihand", EDGE_EVENT,
      RR_BENEATH_THE_WELL_DEXIHAND_ROOM, (int32_t)RE_ACCESS_HOT_SPRING_WATER, kAllTime, InvEmpty, NULL },
    // MMRT_GBT_ENTRANCE_BOW. A Bow with no Fire Arrows and no Deku Stick, so
    // CAN_LIGHT_TORCH_NEAR_ANOTHER is false; the control takes the Bow away.
    { MMRT_GBT_ENTRANCE_BOW, "Great Bay Temple's entrance chest with plain arrows", EDGE_CHECK,
      RR_GREAT_BAY_TEMPLE_ENTRANCE, (int32_t)RC_GREAT_BAY_TEMPLE_ENTRANCE_CHEST, kAllTime, InvBow, InvEmpty },
    // MMRT_BANK_NO_WALLET, BOTH rows it widens. A zeroed save holds the Child
    // Wallet (upgrade level 0), which is exactly what the trick says is enough, so
    // there is no item to take. The two rows are separately probed rather than one
    // standing for the other: leg (e) cannot substantiate a sibling (see its note
    // above), and the INTEREST row is the one MMRT_BANK_ONE_WALLET's text argues
    // about, so a future edit there must break a probe, not just a digest.
    { MMRT_BANK_NO_WALLET, "the bank's heart piece with no wallet upgrade", EDGE_CHECK, RR_CLOCK_TOWN_WEST,
      (int32_t)RC_CLOCK_TOWN_WEST_BANK_PIECE_OF_HEART, kAllTime, InvEmpty, NULL },
    { MMRT_BANK_NO_WALLET, "the bank's interest reward with no wallet upgrade", EDGE_CHECK, RR_CLOCK_TOWN_WEST,
      (int32_t)RC_CLOCK_TOWN_WEST_BANK_INTEREST, kAllTime, InvEmpty, NULL },
    // MMRT_ISTT_RUPEES_GORON, all EIGHT gated rupees. One row per check, because
    // "one of eight stands for the set" was never true: leg (e) reads both of its
    // sets off the same binary, so a tightening on an unprobed sibling drops the
    // check from BOTH and passes. Eight table entries cost nothing and make each
    // condition its own red/green pair.
    { MMRT_ISTT_RUPEES_GORON, "ISTT's floating pre-Twinmold rupee 04 as Goron", EDGE_CHECK,
      RR_STONE_TOWER_TEMPLE_INVERTED_SPIKED_BAR_ROOM_LOWER,
      (int32_t)RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_FREESTANDING_RUPEE_04, kAllTime, InvGoronOnly, InvEmpty },
    { MMRT_ISTT_RUPEES_GORON, "ISTT's floating pre-Twinmold rupee 05 as Goron", EDGE_CHECK,
      RR_STONE_TOWER_TEMPLE_INVERTED_SPIKED_BAR_ROOM_LOWER,
      (int32_t)RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_FREESTANDING_RUPEE_05, kAllTime, InvGoronOnly, InvEmpty },
    { MMRT_ISTT_RUPEES_GORON, "ISTT's floating pre-Twinmold rupee 06 as Goron", EDGE_CHECK,
      RR_STONE_TOWER_TEMPLE_INVERTED_SPIKED_BAR_ROOM_LOWER,
      (int32_t)RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_FREESTANDING_RUPEE_06, kAllTime, InvGoronOnly, InvEmpty },
    { MMRT_ISTT_RUPEES_GORON, "ISTT's floating pre-Twinmold rupee 07 as Goron", EDGE_CHECK,
      RR_STONE_TOWER_TEMPLE_INVERTED_SPIKED_BAR_ROOM_LOWER,
      (int32_t)RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_FREESTANDING_RUPEE_07, kAllTime, InvGoronOnly, InvEmpty },
    { MMRT_ISTT_RUPEES_GORON, "ISTT's floating pre-Twinmold rupee 08 as Goron", EDGE_CHECK,
      RR_STONE_TOWER_TEMPLE_INVERTED_SPIKED_BAR_ROOM_LOWER,
      (int32_t)RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_FREESTANDING_RUPEE_08, kAllTime, InvGoronOnly, InvEmpty },
    { MMRT_ISTT_RUPEES_GORON, "ISTT's floating pre-Twinmold rupee 09 as Goron", EDGE_CHECK,
      RR_STONE_TOWER_TEMPLE_INVERTED_SPIKED_BAR_ROOM_LOWER,
      (int32_t)RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_FREESTANDING_RUPEE_09, kAllTime, InvGoronOnly, InvEmpty },
    { MMRT_ISTT_RUPEES_GORON, "ISTT's floating pre-Twinmold rupee 10 as Goron", EDGE_CHECK,
      RR_STONE_TOWER_TEMPLE_INVERTED_SPIKED_BAR_ROOM_LOWER,
      (int32_t)RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_FREESTANDING_RUPEE_10, kAllTime, InvGoronOnly, InvEmpty },
    { MMRT_ISTT_RUPEES_GORON, "ISTT's floating pre-Twinmold rupee 11 as Goron", EDGE_CHECK,
      RR_STONE_TOWER_TEMPLE_INVERTED_SPIKED_BAR_ROOM_LOWER,
      (int32_t)RC_STONE_TOWER_TEMPLE_INVERTED_PRE_BOSS_FREESTANDING_RUPEE_11, kAllTime, InvGoronOnly, InvEmpty },
    // MMRT_NCT_TINGLE, BOTH maps. The Great Fairy's Sword and nothing else: no
    // Bow, Hookshot, Deku or Zora, so CAN_USE_PROJECTILE is false. kAllTime
    // satisfies IS_DAY(); kSurvivorProbes is what pins IS_DAY() outside the
    // parentheses.
    { MMRT_NCT_TINGLE, "North Clock Town's first Tingle map by jump slash", EDGE_CHECK, RR_CLOCK_TOWN_NORTH,
      (int32_t)RC_CLOCK_TOWN_NORTH_TINGLE_MAP_01, kAllTime, InvGreatFairySword, InvEmpty },
    { MMRT_NCT_TINGLE, "North Clock Town's second Tingle map by jump slash", EDGE_CHECK, RR_CLOCK_TOWN_NORTH,
      (int32_t)RC_CLOCK_TOWN_NORTH_TINGLE_MAP_02, kAllTime, InvGreatFairySword, InvEmpty },

    // ---- #578 part 3, second pass --------------------------------------
    //
    // Four keys over sixteen edges. THREE of the four need no leg-(f) arm, and that
    // is a claim about their shape rather than an omission: their disjuncts append at
    // the TOP level of the condition, so no conjunct is left outside the trick's
    // parentheses for an arm to clear. Where a vanilla conjunction survives (the
    // water wheel's Zora-and-swim pair, Woodfall rupee 06's Deku-and-explosive pair)
    // it survives INSIDE its own disjunct, which legs (a)/(b) cover.
    //
    // MMRT_ST_UPDRAFTS IS THE EXCEPTION, which an earlier version of this comment
    // denied in a blanket sentence: its sixth edge, the wind room's outbound
    // traversal, is `KEY_COUNT(STONE_TOWER_TEMPLE) >= 3 && (CAN_BE_DEKU || (trick &&
    // ...))`, so the key count survives outside the parentheses and kSurvivorProbes
    // carries the arm that clears it.

    // MMRT_GBT_WATERWHEEL_GORON. The Goron Mask alone: no Zora Mask and no swim
    // flag, so the vanilla disjunct is false. The control drops the mask, which is
    // the trick's own item term.
    { MMRT_GBT_WATERWHEEL_GORON, "Great Bay Temple's central room from the water wheel as Goron", EDGE_CONNECTION,
      RR_GREAT_BAY_TEMPLE_WATER_WHEEL_ROOM, (int32_t)RR_GREAT_BAY_TEMPLE_CENTRAL_ROOM, kAllTime, InvGoronOnly,
      InvEmpty },
    // MMRT_WFT_RUPEES_ICE, all SIX rupees, one row each for the reason the ISTT
    // rupees have eight: leg (e) reads both of its sets off one binary, so an
    // unprobed sibling's tightening passes it. Bow + Ice Arrows + magic and no mask
    // of any kind, so every vanilla disjunct on all six rows is false — including
    // row 05's CAN_USE_EXPLOSIVE, since the save holds no bomb, chu, Blast Mask or
    // keg.
    { MMRT_WFT_RUPEES_ICE, "Woodfall Temple's pre-Odolwa rupee 01 off an ice platform", EDGE_CHECK,
      RR_WOODFALL_TEMPLE_PRE_BOSS_ROOM, (int32_t)RC_WOODFALL_TEMPLE_PRE_BOSS_FREESTANDING_RUPEE_01, kAllTime,
      InvBowIceArrowsAndMagic, InvBowAndMagicNoIceArrows },
    { MMRT_WFT_RUPEES_ICE, "Woodfall Temple's pre-Odolwa rupee 02 off an ice platform", EDGE_CHECK,
      RR_WOODFALL_TEMPLE_PRE_BOSS_ROOM, (int32_t)RC_WOODFALL_TEMPLE_PRE_BOSS_FREESTANDING_RUPEE_02, kAllTime,
      InvBowIceArrowsAndMagic, InvBowAndMagicNoIceArrows },
    { MMRT_WFT_RUPEES_ICE, "Woodfall Temple's pre-Odolwa rupee 03 off an ice platform", EDGE_CHECK,
      RR_WOODFALL_TEMPLE_PRE_BOSS_ROOM, (int32_t)RC_WOODFALL_TEMPLE_PRE_BOSS_FREESTANDING_RUPEE_03, kAllTime,
      InvBowIceArrowsAndMagic, InvBowAndMagicNoIceArrows },
    { MMRT_WFT_RUPEES_ICE, "Woodfall Temple's pre-Odolwa rupee 04 off an ice platform", EDGE_CHECK,
      RR_WOODFALL_TEMPLE_PRE_BOSS_ROOM, (int32_t)RC_WOODFALL_TEMPLE_PRE_BOSS_FREESTANDING_RUPEE_04, kAllTime,
      InvBowIceArrowsAndMagic, InvBowAndMagicNoIceArrows },
    { MMRT_WFT_RUPEES_ICE, "Woodfall Temple's pre-Odolwa rupee 05 off an ice platform", EDGE_CHECK,
      RR_WOODFALL_TEMPLE_PRE_BOSS_ROOM, (int32_t)RC_WOODFALL_TEMPLE_PRE_BOSS_FREESTANDING_RUPEE_05, kAllTime,
      InvBowIceArrowsAndMagic, InvBowAndMagicNoIceArrows },
    { MMRT_WFT_RUPEES_ICE, "Woodfall Temple's pre-Odolwa rupee 06 off an ice platform", EDGE_CHECK,
      RR_WOODFALL_TEMPLE_PRE_BOSS_ROOM, (int32_t)RC_WOODFALL_TEMPLE_PRE_BOSS_FREESTANDING_RUPEE_06, kAllTime,
      InvBowIceArrowsAndMagic, InvBowAndMagicNoIceArrows },
    // MMRT_ST_UPDRAFTS, all SIX updraft checks. The Goron Mask alone satisfies the
    // trick's disjunct; the control empties the save, which clears both of its arms
    // (mask and bomb) at once.
    { MMRT_ST_UPDRAFTS, "ISTT's updraft bridge pot 01 as Goron", EDGE_CHECK, RR_STONE_TOWER_TEMPLE_INVERTED_WIND_ROOM,
      (int32_t)RC_STONE_TOWER_TEMPLE_INVERTED_UPDRAFTS_BRIDGE_POT_01, kAllTime, InvGoronOnly, InvEmpty },
    { MMRT_ST_UPDRAFTS, "ISTT's updraft bridge pot 02 as Goron", EDGE_CHECK, RR_STONE_TOWER_TEMPLE_INVERTED_WIND_ROOM,
      (int32_t)RC_STONE_TOWER_TEMPLE_INVERTED_UPDRAFTS_BRIDGE_POT_02, kAllTime, InvGoronOnly, InvEmpty },
    { MMRT_ST_UPDRAFTS, "ISTT's updraft ledge pot 01 as Goron", EDGE_CHECK, RR_STONE_TOWER_TEMPLE_INVERTED_WIND_ROOM,
      (int32_t)RC_STONE_TOWER_TEMPLE_INVERTED_UPDRAFTS_LEDGE_POT_01, kAllTime, InvGoronOnly, InvEmpty },
    { MMRT_ST_UPDRAFTS, "ISTT's updraft ledge pot 02 as Goron", EDGE_CHECK, RR_STONE_TOWER_TEMPLE_INVERTED_WIND_ROOM,
      (int32_t)RC_STONE_TOWER_TEMPLE_INVERTED_UPDRAFTS_LEDGE_POT_02, kAllTime, InvGoronOnly, InvEmpty },
    { MMRT_ST_UPDRAFTS, "ISTT's updraft ledge pot 03 as Goron", EDGE_CHECK, RR_STONE_TOWER_TEMPLE_INVERTED_WIND_ROOM,
      (int32_t)RC_STONE_TOWER_TEMPLE_INVERTED_UPDRAFTS_LEDGE_POT_03, kAllTime, InvGoronOnly, InvEmpty },
    { MMRT_ST_UPDRAFTS, "ISTT's updraft ledge pot 04 as Goron", EDGE_CHECK, RR_STONE_TOWER_TEMPLE_INVERTED_WIND_ROOM,
      (int32_t)RC_STONE_TOWER_TEMPLE_INVERTED_UPDRAFTS_LEDGE_POT_04, kAllTime, InvGoronOnly, InvEmpty },
    // MMRT_ST_UPDRAFTS's sixth edge: the room's outbound TRAVERSAL, whose Deku term
    // the mirror edge's missing one identifies as the in-room climb (the region
    // file's note carries the argument). Three keys and the Goron Mask for the green
    // half; the control keeps the keys and drops the trick's own terms, so a disjunct
    // that had REPLACED the key count would be caught by leg (b) here and a key count
    // swallowed into the parentheses by the leg-(f) arm below.
    { MMRT_ST_UPDRAFTS, "ISTT's wind room to the flipped lava room as Goron", EDGE_CONNECTION,
      RR_STONE_TOWER_TEMPLE_INVERTED_WIND_ROOM, (int32_t)RR_STONE_TOWER_TEMPLE_INVERTED_LAVA_FLIP_ROOM, kAllTime,
      InvStoneTowerKeys3AndGoron, InvStoneTowerKeys3Only },
    // MMRT_IKANA_ROOF_PARKOUR. A wholly NEW edge, so the trick IS the condition and
    // the red half is the edge existing in the map and refusing — the same shape as
    // part 2's MMRT_PALACE_BEAN_SKIP.
    { MMRT_IKANA_ROOF_PARKOUR, "Ikana Castle's outer roof from the inner roof", EDGE_CONNECTION,
      RR_IKANA_CASTLE_INNER_ROOF, (int32_t)RR_IKANA_CASTLE_OUTER_ROOF, kAllTime, InvEmpty, NULL },
};

/**
 * Leg (f). Each row satisfies the VANILLA disjunct of a `(A || MM_TRICK(X)) && B`
 * widening and CLEARS one conjunct B that the widening left outside the
 * parentheses. The edge must be shut with the trick OFF (which is what catches B
 * swallowed into the trick's own disjunct — a tricks-off widening) and shut with
 * it ON (which catches B deleted). Rows whose surviving conjunct is already the
 * thing their leg-(b) control removes are not repeated here: MMRT_ICELESS_IKANA's
 * outer Hookshot is exactly what `InvEmpty` takes away.
 */
struct SurvivorProbe {
    MMRandoTrickId trick;
    /** "<edge> stays shut with <B> cleared". */
    const char* what;
    EdgeKind kind;
    RandoRegionId region;
    int32_t target;
    uint64_t time;
    /** Satisfies A (and anything else the edge wants) but NOT the conjunct B. */
    void (*inventory)();
};

const SurvivorProbe kSurvivorProbes[] = {
    // (CAN_BE_DEKU || (trick && CAN_BE_GORON)) && deed. The Deku route is
    // satisfied and the Land Title Deed is not: if the deed were dropped or
    // absorbed into the trick's parentheses, the trick-OFF half of this arm opens.
    { MMRT_SOUTHERN_SWAMP_SCRUB_HP_GORON,
      "the Southern Swamp scrub's heart piece stays shut for a Deku with no Land Title Deed", EDGE_CHECK,
      RR_SOUTHERN_SWAMP_NORTH, (int32_t)RC_SOUTHERN_SWAMP_PIECE_OF_HEART, kAllTime, InvDekuOnly },
    // deed && CAN_BE_GORON && (CAN_BE_DEKU || (trick && (CAN_BE_GORON || CAN_BE_ZORA))).
    // TWO outer conjuncts, so two arms: the deed cleared, and the outer
    // CAN_BE_GORON cleared. The second is the one the region file's own comment
    // turns on — it is why the trick's Zora leg is dead — so it is pinned here.
    { MMRT_ZORA_HALL_SCRUB_HP_NO_DEKU,
      "the Zora Hall scrub's heart piece stays shut for a Goron Deku with no Mountain Title Deed", EDGE_CHECK,
      RR_ZORA_HALL_LULUS_ROOM, (int32_t)RC_ZORA_HALL_SCRUB_PIECE_OF_HEART, kAllTime, InvDekuAndGoron },
    { MMRT_ZORA_HALL_SCRUB_HP_NO_DEKU,
      "the Zora Hall scrub's heart piece stays shut for a deed-holding Deku who is not Goron", EDGE_CHECK,
      RR_ZORA_HALL_LULUS_ROOM, (int32_t)RC_ZORA_HALL_SCRUB_PIECE_OF_HEART, kAllTime, InvDekuAndDeedMountain },
    // (CAN_USE_PROJECTILE || (trick && sword)) && CAN_AFFORD(..) && IS_DAY().
    // The surviving conjunct here is a TIME term, so the arm clears it with the
    // clock rather than the inventory: night 1 only, where IS_DAY() is false.
    { MMRT_NCT_TINGLE, "North Clock Town's first Tingle map stays shut at night", EDGE_CHECK, RR_CLOCK_TOWN_NORTH,
      (int32_t)RC_CLOCK_TOWN_NORTH_TINGLE_MAP_01, kNight1Only, InvBowAndGreatFairySword },
    { MMRT_NCT_TINGLE, "North Clock Town's second Tingle map stays shut at night", EDGE_CHECK, RR_CLOCK_TOWN_NORTH,
      (int32_t)RC_CLOCK_TOWN_NORTH_TINGLE_MAP_02, kNight1Only, InvBowAndGreatFairySword },
    // KEY_COUNT(STONE_TOWER_TEMPLE) >= 3 && (CAN_BE_DEKU || (trick && (CAN_BE_GORON ||
    // bomb))). The Deku route is satisfied and the key count is zero: with the trick
    // OFF this arm catches the key count pulled inside the trick's parentheses (which
    // would widen the TRICKS-OFF condition — a locked door opened by a Deku Mask), and
    // with it ON, the key count deleted outright.
    { MMRT_ST_UPDRAFTS, "ISTT's wind room stays shut to a keyless Deku", EDGE_CONNECTION,
      RR_STONE_TOWER_TEMPLE_INVERTED_WIND_ROOM, (int32_t)RR_STONE_TOWER_TEMPLE_INVERTED_LAVA_FLIP_ROOM, kAllTime,
      InvDekuNoStoneTowerKeys },
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
const std::function<bool()>* FindCondition(EdgeKind kind, RandoRegionId regionId, int32_t target, const char** outWhy) {
    auto regionIt = Rando::Logic::Regions.find(regionId);
    if (regionIt == Rando::Logic::Regions.end()) {
        *outWhy = "the region is not in the graph";
        return NULL;
    }
    const Rando::Logic::RandoRegion& region = regionIt->second;
    switch (kind) {
        case EDGE_CHECK: {
            auto it = region.checks.find((RandoCheckId)target);
            if (it == region.checks.end()) {
                *outWhy = "the region has no such check";
                return NULL;
            }
            return &it->second.first;
        }
        case EDGE_CONNECTION: {
            auto it = region.connections.find((RandoRegionId)target);
            if (it == region.connections.end()) {
                *outWhy = "the region has no connection to that region";
                return NULL;
            }
            return &it->second.first;
        }
        case EDGE_EXIT: {
            auto it = region.exits.find((s32)target);
            if (it == region.exits.end()) {
                *outWhy = "the region has no exit at that entrance index";
                return NULL;
            }
            return &it->second.condition;
        }
        case EDGE_EVENT: {
            for (const auto& entry : region.events) {
                if ((int32_t)entry.first == target) {
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
void ArmSave(uint64_t time, void (*inventory)()) {
    ResetSaveWithEmptyInventory();
    inventory();
    Rando::Logic::gCurrentRegionTime = time;
}

const char* TrickName(MMRandoTrickId id) {
    auto it = Rando::StaticData::Tricks.find(id);
    return it == Rando::StaticData::Tricks.end() ? "(unknown key)" : it->second.name;
}

} // namespace

extern "C" int MM_TrickBindings_RunHeadless(void) {
    printf("[TEST] mm-trick-bindings: each of parts 2 and 3's trick bindings closes its edge with the trick off and "
           "opens it with the trick on (#578 parts 2, 3)\n");

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
        const std::function<bool()>* condition = FindCondition(probe.kind, probe.region, probe.target, &why);
        if (condition == NULL) {
            rc = BindFail(1, "%s [%s]: %s — the binding's edge is gone, so this probe would be vacuous", probe.what,
                          TrickName(probe.trick), why);
            break;
        }

        ArmSave(probe.time, probe.inventory);
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
            ArmSave(probe.time, probe.withoutTrickItem);
            SetFrozenTrick(probe.trick, true);
            if ((*condition)()) {
                rc = BindFail(4,
                              "%s [%s]: the trick ALONE opens the edge, without a term the edge must still require — "
                              "the disjunct replaced that requirement instead of joining it",
                              probe.what, TrickName(probe.trick));
                break;
            }
        }

        printf("[TEST]   ok: %s [%s]%s\n", probe.what, TrickName(probe.trick),
               probe.withoutTrickItem != NULL ? " (+ item-still-required control)" : "");
    }

    // ---- (f): the surviving OUTER conjunct of each `(A || trick) && B` ----
    //
    // Deliberately does NOT break on the first failure: these arms exist because
    // the reviewer of #703 showed leg (b) never observed its red half for these
    // rows, so when one of them is broken it should be possible to see ALL of them
    // in one run rather than one per rebuild.
    if (rc == 0) {
        for (const SurvivorProbe& probe : kSurvivorProbes) {
            const char* why = "";
            const std::function<bool()>* condition = FindCondition(probe.kind, probe.region, probe.target, &why);
            if (condition == NULL) {
                rc = BindFail(12, "%s [%s]: %s — the edge this arm guards is gone", probe.what, TrickName(probe.trick),
                              why);
                continue;
            }

            bool broke = false;
            for (int on = 0; on <= 1; on++) {
                ArmSave(probe.time, probe.inventory);
                SetFrozenTrick(probe.trick, on != 0);
                if ((*condition)()) {
                    rc = BindFail(13,
                                  "%s [%s], trick %s: the edge is OPEN with a conjunct the widening had to leave "
                                  "OUTSIDE its parentheses cleared — that term was deleted or swallowed into the "
                                  "trick's disjunct, which widens the TRICKS-OFF condition",
                                  probe.what, TrickName(probe.trick), on ? "ON" : "OFF");
                    broke = true;
                }
            }
            if (!broke) {
                printf("[TEST]   ok: %s [%s] (surviving-conjunct arm, trick off and on)\n", probe.what,
                       TrickName(probe.trick));
            }
        }
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
        printf("[TEST] PASS: %d edges across %d trick keys are closed with the trick off and open with it on, %d "
               "surviving-conjunct arms stay shut with the trick off AND on, every shipped bound key is probed, and no "
               "key removes reach when enabled\n",
               (int)(sizeof(kProbes) / sizeof(kProbes[0])), (int)probedKeys.size(),
               (int)(sizeof(kSurvivorProbes) / sizeof(kSurvivorProbes[0])));
    }
    return rc;
}

#endif // RSBS_SINGLE_EXECUTABLE
