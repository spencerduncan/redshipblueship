/**
 * @file oot_logic_gates_test.cpp
 * @brief #1168: the talking and grabbing gates Ship of Harkinian added to its
 *        location logic after our import base, driven through OoT's REAL search
 *        primitives. CTest row `OoTLogicGates` (label `rando`), test name
 *        `oot-logic-gates`, registered in src/common/test_runner.cpp.
 *
 * Every leg evaluates the location exactly the way the search does for one
 * region (3drando/fill.cpp's ReachabilitySearch): the region is given its
 * age/time access, its events are fired to a fixpoint with Region::UpdateEvents,
 * and the location is asked LocationAccess::ConditionsMet(region, false) with
 * logic->CurrentCheckKey set to it. The lambdas are the ones RegionTable_Init
 * builds — never a re-statement of a condition, which would pass with the gate
 * deleted.
 *
 * The world is Shuffle Speak ON and Shuffle Grab ON (forced here, restored on
 * exit), so Logic::Reset grants neither ability and each leg says what it holds.
 *
 *  A. Song from Malon (RC_SONG_FROM_MALON): a child with Zelda's Letter and the
 *     Fairy Ocarina, by day, but no Hylian jabber nut, must NOT reach it; with
 *     the nut it must. (SoH c906c71f7.)
 *  B. Talon's chickens (RC_LLR_TALONS_CHICKENS): wallet, nut and letter by day
 *     but no Power Bracelet (Shuffle Grab's grab ability) must NOT reach it; with
 *     the bracelet it must. (SoH 63597ec63, logic half.)
 *  C. The four Thieves' Hideout carpenters: a fighter without the nut must NOT
 *     reach a carpenter's check; with the nut the cell's own "could free" event
 *     fires and the check is reached. (SoH 1413ceb33, logic half.)
 *  D. The Haunted Wasteland carpet merchant (RC_WASTELAND_BOMBCHU_SALESMAN): a
 *     jumpslash alone used to satisfy it (`&&` binds tighter than `||`). Without
 *     the nut it must NOT be reached; with the nut and a price the wallet covers
 *     it must; with the nut and a price above the wallet it must NOT.
 *     (SoH 0702b11b9.)
 *  E. The four carpenters' check rows (#1191, SoH 2bc50f605): each cell's Gerudo
 *     guard (En_GeldB) drops its key as En_Item00 with params keyFlag | 0x11
 *     (z_en_geldb.c), and the randomizer names the check that drop awards by
 *     (actor, scene, params) through CheckFromActorMultimap and marks it
 *     collected by the scene collectible flag. Each carpenter's row must carry
 *     its OWN cell's guard key flag, and the location table must hold exactly
 *     one En_Item00 row for that drop, the carpenter's. The flags are the guards'
 *     params in the Thieves' Hideout rooms (gerudoway_room_N actor lists; rooms
 *     per randomizer_entrance_tracker.cpp): room 2 1 Torch 0x0C, room 1 Dead End
 *     0x0A, room 5 Double Cell 0x0F, room 4 Steep Slope 0x0E. The pots each row
 *     of ShufflePots.cpp names for a cell stand in the same room as its guard.
 *     A data leg: no search, no save.
 *
 *  F. The interior pots whose scene disables the item (#1282, SoH 041295f05):
 *     with Shuffle Pots and Shuffle Grab ON and grab withheld, CanBreakPots must
 *     not count an item the scene's OoT_sRestrictionFlags row disables
 *     (z_parameter.c). Link's House, the Twins' and Know-It-All Brothers' houses
 *     and the back-alley house disable B, the hookshot and every C item: a child
 *     with the Kokiri Sword, sticks, slingshot, boomerang and bombs must NOT
 *     reach their pots; with grab he must. Talon's house disables C items and
 *     the hookshot but not B: the C items alone must NOT, the Kokiri Sword must.
 *     The guard house disables only the hookshot and longshot: an adult with the
 *     longshot must NOT reach its adult pots; with a bomb bag he must. Control:
 *     the Kokiri Sword alone still breaks a Goron City pot (no restriction).
 *
 * #1277, Shuffle Climb ON as well (and Shuffle Open Chest ON, tricks forced
 * off unless a leg names one; all restored): the ladders and the neighbouring
 * gates Ship of Harkinian re-gated after our import base (bundle O5).
 *  F. Kak Backyard (b6cdc6c49): no exit beyond the three ground-level ones
 *     opens for an adult by day without Climb (the potion shop's back side is up
 *     a ladder); one does with it. The watchtower jump to the rooftop is any age
 *     under RT_UNINTUITIVE_JUMPS and closed without it.
 *  G. DMC wall Piece of Heart (ec27775e8, the PoH clause): an adult with 3
 *     hearts must NOT reach it without Climb; with Climb it must.
 *  H. DC Armos Room -> Bomb Room Lower (fe5dce24b): a child with the bracelet
 *     must NOT pass without Climb; with Climb it must.
 *  I. Child Spirit "before climb" (fe5dce24b): its locked door leads to Sun On
 *     Floor 1F, not 2F. A data leg.
 *  J. Deku Tree 2F grass (fe5dce24b): a child with a sword searching from the
 *     lobby must NOT cut the two 2F grass checks without Climb; with it, must.
 *  K. Deku Tree map chest (743103fbe): not from the lobby floor without Climb,
 *     not without Open Chest, reached with both.
 *  L. GV waterfall alcove (09b0fdbc9 without its RT_HOOKSHOT_LADDERS clause):
 *     the Longshot alone and the Hookshot plus Climb must NOT open it; the
 *     Longshot plus Climb must.
 *  M. SFM (ed4868f62): a child that never killed the Wolfos must NOT walk back
 *     out through the gate; after a search opened it, and as adult, it must.
 *  N. MQ DC torch room (848106039, CanClimbLadder read as HasItem(RG_CLIMB)): a
 *     child with nothing must NOT reach the Dodongo room from the torch room
 *     floor; with Climb, or with the bracelet, it must.
 *  O. Jabu MQ near-boss chest (7be5a4fea): the slingshot without Open Chest
 *     must NOT reach it; with Open Chest it must.
 *  P. MQ Spirit child doors (688913270): under RT_HOVER_BOOST_SIMPLE each door
 *     needs one key more (1 -> 2, 2 -> 3); without the trick one key still opens
 *     the first.
 *
 * #1300, RT_HOOKSHOT_LADDERS / CanClimbLadder / CanClimbHighLadder (SoH
 * 60ec65592, bundle O28, plus the two deferred clauses from 09b0fdbc9 and
 * 848106039):
 *  Q. GV waterfall alcove (09b0fdbc9): under RT_HOOKSHOT_LADDERS the Longshot
 *     without Climb opens it (locking 09b0fdbc9); the Hookshot without Climb
 *     still must NOT.
 *  R. Dodongo's Cavern MQ compass chest (60ec65592): behind the pushed block in
 *     the MQ Dodongo room, reaching the chest without killing the Dodongos
 *     needs CanClimbLadder (Climb, or Hookshot under RT_HOOKSHOT_LADDERS;
 *     closed on Hookshot with the trick off).
 *  S. Bottom of the Well (60ec65592): both the vanilla basement ladder
 *     (RR_BOTW_B3_OOZE -> RR_BOTW_HIDDEN_POTS) and the MQ basement ladder
 *     (RR_BOTW_MQ_B3 -> RR_BOTW_MQ_PERIMETER) are CanClimbHighLadder (Climb, or
 *     Longshot under RT_HOOKSHOT_LADDERS; closed on Longshot with the trick off
 *     and on short Hookshot with the trick on).
 *  T. Shadow Temple dock (60ec65592, vanilla): RR_SHADOW_TEMPLE_DOCK ->
 *     RR_SHADOW_TEMPLE_BEYOND_BOAT requires RT_HOOKSHOT_LADDERS for the
 *     Hookshot path (closed on Hookshot with the trick off, open with it, and
 *     open with Goron's Bracelet + Climb).
 *  U. DMC and Hyrule Field (60ec65592): DMC ladder area -> upper nearby and
 *     both directions of the Hyrule Field <-> Lake Hylia fence ladder
 *     (RR_HYRULE_FIELD <-> RR_HF_TO_LAKE_HYLIA) are CanClimbLadder (closed on
 *     Hookshot with the trick off, open with it).
 *  V. Lost Woods and SFM (60ec65592): the Lost Woods under-bridge exit
 *     (RR_LW_UNDER_BRIDGE -> RR_THE_LOST_WOODS) and both SFM maze ladders
 *     (RR_SACRED_FOREST_MEADOW -> RR_SFM_ABOVE_MAZE and
 *     RR_SFM_OUTSIDE_FAIRY_GROTTO -> RR_SFM_ABOVE_MAZE) are CanClimbLadder
 *     (closed on Hookshot with the trick off, open with it).
 *  W. Gerudo Fortress (60ec65592): RR_GF_OUTSKIRTS -> RR_GF_TOWER is
 *     CanClimbHighLadder, where LOGIC_GF_GATE_OPEN fires; the wasteland gate
 *     sidehop skip (RT_GF_WASTELAND_GATE_SIDEHOP_SKIP) opens for a child.
 *  X. Zora's River (60ec65592): RR_ZORAS_RIVER -> RR_ZR_ATOP_LADDER replaces
 *     its unconditional Hookshot term with CanClimbLadder (closed on Hookshot
 *     with the trick off, open with it).
 *
 *  Note: the Hookshot terms in 848106039 (dodongos_cavern.cpp:428), ZD island
 *  (zoras_domain.cpp:49), DC Armos (dodongos_cavern.cpp:159), the BotW corridor
 *  (bottom_of_the_well.cpp:23) and the KF child branch (kokiri_forest.cpp:75)
 *  are ported faithfully but cannot change logic for either age because
 *  Logic::CanUse(RG_HOOKSHOT/RG_LONGSHOT) requires IsAdult (logic.cpp:829-831),
 *  so they are not lockable. Q2 locks 09b0fdbc9.
 *
 * Every "must NOT" leg is paired with a "must" leg on the same save plus one
 * thing, so a location that is never reachable (a vacuous probe) fails too.
 * Each probe sets logic->CurrentRegionKey to its region, as ProcessRegion does.
 */

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include <libultraship/bridge.h>

#include "soh/OTRGlobals.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/entrance.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/settings.h"

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"

namespace {

int sFailures = 0;

void GateFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[OOT-LOGIC-GATES] FAIL: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    sFailures++;
}

const LocationAccess* FindLocation(Region* region, RandomizerCheck rc) {
    for (const LocationAccess& loc : region->locations) {
        if (loc.GetLocation() == rc) {
            return &loc;
        }
    }
    return nullptr;
}

/** A fresh logic save under the forced options, then nothing else: each leg
 *  grants exactly what it names. */
void ResetLogic() {
    logic->Reset(true);
    logic->CurrentCheckKey = RC_UNKNOWN_CHECK;
    logic->CalculatingAvailableChecks = false;
    // Reset already withholds both under Shuffle Speak / Shuffle Grab ON; stated
    // anyway so a change to Reset's defaults cannot hand a leg the very thing it
    // is about.
    logic->SetRandoInf(RAND_INF_CAN_SPEAK_HYLIAN, false);
    logic->SetRandoInf(RAND_INF_CAN_GRAB, false);
}

void GiveNut() {
    logic->SetRandoInf(RAND_INF_CAN_SPEAK_HYLIAN, true);
}
void GiveGrab() {
    logic->SetRandoInf(RAND_INF_CAN_GRAB, true);
}
void GiveChildWallet() {
    logic->SetRandoInf(RAND_INF_HAS_WALLET, true);
}
void GiveZeldasLetter() {
    logic->SetRandoInf(RAND_INF_ZELDAS_LETTER, true);
}
void GiveFairyOcarina() {
    logic->SetInventory(ITEM_OCARINA_FAIRY, ITEM_OCARINA_FAIRY);
}
/** A child's weapon for CanJumpslash / CanKillEnemy(RE_GERUDO_WARRIOR). */
void GiveSticks() {
    logic->SetUpgrade(UPG_STICKS, 1);
    logic->Set(LOGIC_STICK_ACCESS, true);
}
/** One item, the way the search grants it (Item::ApplyEffect -> Logic::ApplyItemEffect). */
void Give(RandomizerGet rg) {
    Rando::StaticData::RetrieveItem(rg).ApplyEffect();
}

/**
 * The search's question for one location of one region, as the child or the
 * adult (day and night), with the region current as ProcessRegion makes it.
 * Returns -1 when the probe could not be made (region or location missing),
 * which the caller counts as a failure whatever it expected.
 */
int Reaches(RandomizerRegion regionKey, RandomizerCheck rc, bool adult) {
    Region* region = RegionTable(regionKey);
    const LocationAccess* loc = region != nullptr ? FindLocation(region, rc) : nullptr;
    if (loc == nullptr) {
        return -1;
    }
    const bool saved[4] = { region->childDay, region->childNight, region->adultDay, region->adultNight };
    const RandomizerRegion savedRegionKey = logic->CurrentRegionKey;
    region->childDay = !adult;
    region->childNight = !adult;
    region->adultDay = adult;
    region->adultNight = adult;
    logic->CurrentRegionKey = regionKey;
    while (region->UpdateEvents()) {}
    logic->CurrentCheckKey = rc;
    const bool met = loc->ConditionsMet(region, false);
    logic->CurrentCheckKey = RC_UNKNOWN_CHECK;
    logic->CurrentRegionKey = savedRegionKey;
    region->childDay = saved[0];
    region->childNight = saved[1];
    region->adultDay = saved[2];
    region->adultNight = saved[3];
    return met ? 1 : 0;
}

void ExpectAs(bool adult, const char* leg, RandomizerRegion regionKey, RandomizerCheck rc, bool expected,
              const char* why) {
    const int got = Reaches(regionKey, rc, adult);
    if (got < 0) {
        GateFail("%s: location %d is not in region %d, so this probe evaluates nothing", leg, (int)rc, (int)regionKey);
        return;
    }
    if ((got == 1) != expected) {
        GateFail("%s: %s (expected %s, got %s)", leg, why, expected ? "reachable" : "unreachable",
                 got ? "reachable" : "unreachable");
        return;
    }
    printf("[TEST]   ok: %s: %s\n", leg, expected ? "reachable" : "unreachable");
}

void Expect(const char* leg, RandomizerRegion regionKey, RandomizerCheck rc, bool expected, const char* why) {
    ExpectAs(false, leg, regionKey, rc, expected, why);
}

/** A pot whose scene disables B, the hookshot and every C item (leg F). */
struct HousePot {
    const char* name;
    RandomizerRegion region;
    RandomizerCheck check;
};

const HousePot kHousePots[] = {
    { "Link's House pot", RR_KF_LINKS_HOUSE, RC_KF_LINKS_HOUSE_POT },
    { "Twins' House pot 1", RR_KF_HOUSE_OF_TWINS, RC_KF_TWINS_HOUSE_POT_1 },
    { "Twins' House pot 2", RR_KF_HOUSE_OF_TWINS, RC_KF_TWINS_HOUSE_POT_2 },
    { "Know-It-All Brothers' pot 1", RR_KF_KNOW_IT_ALL_HOUSE, RC_KF_BROTHERS_HOUSE_POT_1 },
    { "Know-It-All Brothers' pot 2", RR_KF_KNOW_IT_ALL_HOUSE, RC_KF_BROTHERS_HOUSE_POT_2 },
    { "back-alley house pot 1", RR_MARKET_MAN_IN_GREEN_HOUSE, RC_MK_BACK_ALLEY_HOUSE_POT_1 },
    { "back-alley house pot 2", RR_MARKET_MAN_IN_GREEN_HOUSE, RC_MK_BACK_ALLEY_HOUSE_POT_2 },
    { "back-alley house pot 3", RR_MARKET_MAN_IN_GREEN_HOUSE, RC_MK_BACK_ALLEY_HOUSE_POT_3 },
};

/** Every child item CanBreakPots names: B (Kokiri Sword, sticks) and C (slingshot, boomerang, bombs). */
void GiveChildPotBreakers() {
    Give(RG_KOKIRI_SWORD);
    GiveSticks();
    Give(RG_PROGRESSIVE_SLINGSHOT);
    Give(RG_BOOMERANG);
    Give(RG_PROGRESSIVE_BOMB_BAG);
}

struct Carpenter {
    const char* name;
    RandomizerRegion region;
    RandomizerCheck check;
    /** The cell guard's En_GeldB params >> 8 in the scene's actor list: the
     *  collectible flag of the key it drops (leg E). */
    uint8_t guardKeyFlag;
};

const Carpenter kCarpenters[] = {
    { "1 Torch carpenter", RR_TH_1_TORCH_CELL, RC_TH_1_TORCH_CARPENTER, 0x0C },             // gerudoway_room_2
    { "Double Cell carpenter", RR_TH_DOUBLE_CELL, RC_TH_DOUBLE_CELL_CARPENTER, 0x0F },      // gerudoway_room_5
    { "Dead End carpenter", RR_TH_DEAD_END_CELL, RC_TH_DEAD_END_CARPENTER, 0x0A },          // gerudoway_room_1
    { "Steep Slope carpenter", RR_TH_STEEP_SLOPE_CELL, RC_TH_STEEP_SLOPE_CARPENTER, 0x0E }, // gerudoway_room_4
};

/** Leg E for one carpenter: its row carries the guard's drop and flag, and no
 *  other row answers that drop. */
void ExpectCarpenterRow(const Carpenter& carpenter) {
    const int32_t dropParams = ((int32_t)carpenter.guardKeyFlag << 8) | ITEM00_SMALL_KEY;
    const Rando::Location* row = Rando::StaticData::GetLocation(carpenter.check);
    const Rando::SpoilerCollectionCheck cc = row->GetCollectionCheck();
    bool ok = true;
    if (row->GetActorID() != ACTOR_EN_ITEM00 || row->GetScene() != SCENE_THIEVES_HIDEOUT) {
        GateFail("E %s: the row is actor %d in scene %d, not the guard's En_Item00 drop in the hideout", carpenter.name,
                 (int)row->GetActorID(), (int)row->GetScene());
        ok = false;
    }
    if (row->GetActorParams() != dropParams) {
        GateFail("E %s: the row's actor params are 0x%04X but the cell's guard drops 0x%04X (#1191): the "
                 "carpenter's key awards another cell's check",
                 carpenter.name, (unsigned)row->GetActorParams(), (unsigned)dropParams);
        ok = false;
    }
    if (cc.type != SPOILER_CHK_COLLECTABLE || cc.scene != SCENE_THIEVES_HIDEOUT || cc.flag != carpenter.guardKeyFlag) {
        GateFail("E %s: the row's collection check is type %d scene %d flag 0x%02X, not the cell guard's "
                 "collectible flag 0x%02X (#1191): the tracker marks another cell's check",
                 carpenter.name, (int)cc.type, (int)cc.scene, (unsigned)cc.flag, (unsigned)carpenter.guardKeyFlag);
        ok = false;
    }
    // The runtime lookup (Randomizer::GetCheckFromActor over the multimap
    // InitHashMaps builds from this table) must find this row and only it.
    int matches = 0;
    RandomizerCheck found = RC_UNKNOWN_CHECK;
    for (const Rando::Location& loc : Rando::StaticData::GetLocationTable()) {
        if (loc.GetActorID() == ACTOR_EN_ITEM00 && loc.GetScene() == SCENE_THIEVES_HIDEOUT &&
            loc.GetActorParams() == dropParams) {
            matches++;
            found = loc.GetRandomizerCheck();
        }
    }
    if (matches != 1 || found != carpenter.check) {
        GateFail("E %s: %d row(s) answer the guard's drop 0x%04X (last %d), expected exactly the carpenter's (%d)",
                 carpenter.name, matches, (unsigned)dropParams, (int)found, (int)carpenter.check);
        ok = false;
    }
    if (ok) {
        printf("[TEST]   ok: E %s: params 0x%04X, flag 0x%02X\n", carpenter.name, (unsigned)dropParams,
               (unsigned)carpenter.guardKeyFlag);
    }
}

// ---- #1277: Shuffle Climb legs (F..P) --------------------------------------
// The ladders SoH re-gated after our import base (bundle O5). Every probe runs
// the real lambdas RegionTable_Init built, under Shuffle Climb ON with
// RAND_INF_CAN_CLIMB withheld unless the leg grants it.

void GiveClimb() {
    logic->SetRandoInf(RAND_INF_CAN_CLIMB, true);
}
void GiveOpenChest() {
    logic->SetRandoInf(RAND_INF_CAN_OPEN_CHEST, true);
}
void GiveKokiriSword() {
    logic->ApplyItemEffect(Rando::StaticData::RetrieveItem(RG_KOKIRI_SWORD), true);
}
void GiveSlingshot() {
    logic->ApplyItemEffect(Rando::StaticData::RetrieveItem(RG_PROGRESSIVE_SLINGSHOT), true);
}
/** ITEM_HOOKSHOT's slot holding the hookshot or the longshot. */
void GiveHookshot(bool longshot) {
    logic->SetInventory(ITEM_HOOKSHOT, longshot ? ITEM_LONGSHOT : ITEM_HOOKSHOT);
}

/** ResetLogic, plus the Shuffle Climb / Shuffle Open Chest abilities withheld. */
void ResetClimbLogic() {
    ResetLogic();
    logic->SetRandoInf(RAND_INF_CAN_CLIMB, false);
    logic->SetRandoInf(RAND_INF_CAN_OPEN_CHEST, false);
}

bool AnyAccess(const Region* region) {
    return region->childDay || region->childNight || region->adultDay || region->adultNight;
}

/** One exit's own lambda, for one age, by day or by night. -1 when the region
 *  has no exit to `to` (the probe would evaluate nothing). */
int ExitOpen(RandomizerRegion from, RandomizerRegion to, bool adult) {
    if (from <= RR_NONE || from >= RR_MAX || to <= RR_NONE || to >= RR_MAX) {
        return -1;
    }
    for (Rando::Entrance& exit : RegionTable(from)->exits) {
        if (exit.GetConnectedRegionKey() != to) {
            continue;
        }
        logic->CurrentRegionKey = from;
        const bool day = exit.CheckConditionAtAgeTime(adult ? logic->IsAdult : logic->IsChild, logic->AtDay);
        const bool night = exit.CheckConditionAtAgeTime(adult ? logic->IsAdult : logic->IsChild, logic->AtNight);
        logic->CurrentRegionKey = RR_NONE;
        return (day || night) ? 1 : 0;
    }
    return -1;
}

void ExpectExit(const char* leg, RandomizerRegion from, RandomizerRegion to, bool adult, bool expected,
                const char* why) {
    const int got = ExitOpen(from, to, adult);
    if (got < 0) {
        GateFail("%s: region %d has no exit to region %d, so this probe evaluates nothing", leg, (int)from, (int)to);
        return;
    }
    if ((got == 1) != expected) {
        GateFail("%s: %s (expected %s, got %s)", leg, why, expected ? "open" : "closed", got ? "open" : "closed");
        return;
    }
    printf("[TEST]   ok: %s: %s\n", leg, expected ? "open" : "closed");
}

/** ChildReaches for either age (day and night). */
int AgeReaches(RandomizerRegion regionKey, RandomizerCheck rc, bool adult) {
    Region* region = RegionTable(regionKey);
    const LocationAccess* loc = region != nullptr ? FindLocation(region, rc) : nullptr;
    if (loc == nullptr) {
        return -1;
    }
    const bool saved[4] = { region->childDay, region->childNight, region->adultDay, region->adultNight };
    region->childDay = !adult;
    region->childNight = !adult;
    region->adultDay = adult;
    region->adultNight = adult;
    logic->CurrentRegionKey = regionKey;
    while (region->UpdateEvents()) {}
    logic->CurrentCheckKey = rc;
    const bool met = loc->ConditionsMet(region, false);
    logic->CurrentCheckKey = RC_UNKNOWN_CHECK;
    logic->CurrentRegionKey = RR_NONE;
    region->childDay = saved[0];
    region->childNight = saved[1];
    region->adultDay = saved[2];
    region->adultNight = saved[3];
    return met ? 1 : 0;
}

void ExpectAt(const char* leg, RandomizerRegion regionKey, RandomizerCheck rc, bool adult, bool expected,
              const char* why) {
    const int got = AgeReaches(regionKey, rc, adult);
    if (got < 0) {
        GateFail("%s: location %d is not in region %d, so this probe evaluates nothing", leg, (int)rc, (int)regionKey);
        return;
    }
    if ((got == 1) != expected) {
        GateFail("%s: %s (expected %s, got %s)", leg, why, expected ? "reachable" : "unreachable",
                 got ? "reachable" : "unreachable");
        return;
    }
    printf("[TEST]   ok: %s: %s\n", leg, expected ? "reachable" : "unreachable");
}

/**
 * A one-age search confined to the start region's scene: every region's access
 * is cleared, the start gets the age (day and night), and access spreads through
 * the exits' own lambdas while each reached region fires its events, to a
 * fixpoint (3drando/fill.cpp's ProcessRegion / ProcessExits, without time
 * travel). With rc == RC_UNKNOWN_CHECK it answers whether `targetRegion` was
 * reached; otherwise whether a reached region of the scene holding `rc` meets
 * its condition (-1 when no region of the scene holds `rc`). Access is restored.
 */
int SceneSearch(RandomizerRegion start, bool adult, RandomizerRegion targetRegion, RandomizerCheck rc) {
    std::vector<std::array<bool, 4>> saved(RR_MAX);
    for (int r = RR_NONE + 1; r < RR_MAX; r++) {
        Region* region = RegionTable((RandomizerRegion)r);
        saved[r] = { region->childDay, region->childNight, region->adultDay, region->adultNight };
        region->childDay = region->childNight = region->adultDay = region->adultNight = false;
    }
    Region* first = RegionTable(start);
    const SceneID scene = first->scene;
    first->childDay = first->childNight = !adult;
    first->adultDay = first->adultNight = adult;

    bool changed = true;
    while (changed) {
        changed = false;
        for (int r = RR_NONE + 1; r < RR_MAX; r++) {
            Region* region = RegionTable((RandomizerRegion)r);
            if (!AnyAccess(region)) {
                continue;
            }
            logic->CurrentRegionKey = (RandomizerRegion)r;
            while (region->UpdateEvents()) {
                changed = true;
            }
            for (Rando::Entrance& exit : region->exits) {
                if (exit.GetConnectedRegionKey() == RR_NONE) {
                    continue;
                }
                Region* to = exit.GetConnectedRegion();
                if (to->scene != scene) {
                    continue;
                }
                bool* const from[4] = { &region->childDay, &region->childNight, &region->adultDay,
                                        &region->adultNight };
                bool* const into[4] = { &to->childDay, &to->childNight, &to->adultDay, &to->adultNight };
                for (int i = 0; i < 4; i++) {
                    if (!*from[i] || *into[i]) {
                        continue;
                    }
                    if (exit.CheckConditionAtAgeTime(i < 2 ? logic->IsChild : logic->IsAdult,
                                                     (i % 2) == 0 ? logic->AtDay : logic->AtNight)) {
                        *into[i] = true;
                        changed = true;
                    }
                }
            }
        }
    }

    int result = 0;
    if (rc == RC_UNKNOWN_CHECK) {
        result = AnyAccess(RegionTable(targetRegion)) ? 1 : 0;
    } else {
        bool held = false;
        for (int r = RR_NONE + 1; r < RR_MAX && result == 0; r++) {
            Region* region = RegionTable((RandomizerRegion)r);
            const LocationAccess* loc = region->scene == scene ? FindLocation(region, rc) : nullptr;
            if (loc == nullptr) {
                continue;
            }
            held = true;
            if (!AnyAccess(region)) {
                continue;
            }
            logic->CurrentRegionKey = (RandomizerRegion)r;
            logic->CurrentCheckKey = rc;
            result = loc->ConditionsMet(region, false) ? 1 : 0;
            logic->CurrentCheckKey = RC_UNKNOWN_CHECK;
        }
        if (!held) {
            result = -1;
        }
    }
    logic->CurrentRegionKey = RR_NONE;

    for (int r = RR_NONE + 1; r < RR_MAX; r++) {
        Region* region = RegionTable((RandomizerRegion)r);
        region->childDay = saved[r][0];
        region->childNight = saved[r][1];
        region->adultDay = saved[r][2];
        region->adultNight = saved[r][3];
    }
    return result;
}

void ExpectSearch(const char* leg, RandomizerRegion start, bool adult, RandomizerRegion targetRegion,
                  RandomizerCheck rc, bool expected, const char* why) {
    const int got = SceneSearch(start, adult, targetRegion, rc);
    if (got < 0) {
        GateFail("%s: no region of the start's scene holds location %d, so this probe evaluates nothing", leg, (int)rc);
        return;
    }
    if ((got == 1) != expected) {
        GateFail("%s: %s (expected %s, got %s)", leg, why, expected ? "reachable" : "unreachable",
                 got ? "reachable" : "unreachable");
        return;
    }
    printf("[TEST]   ok: %s: %s\n", leg, expected ? "reachable" : "unreachable");
}

/**
 * Leg F's question: does any exit of Kak Backyard other than its three
 * ground-level ones (the village, the open grotto, Granny's shop door) open for
 * an adult by day? Those others lead to the potion shop's back side, up the
 * backyard ladder. -1 when the backyard has no such exit.
 */
int KakBackyardLadderSideOpen() {
    int found = 0;
    int open = 0;
    for (Rando::Entrance& exit : RegionTable(RR_KAK_BACKYARD)->exits) {
        const RandomizerRegion to = exit.GetConnectedRegionKey();
        if (to == RR_KAKARIKO_VILLAGE || to == RR_KAK_OPEN_GROTTO || to == RR_KAK_ODD_POTION_BUILDING ||
            to == RR_NONE) {
            continue;
        }
        found++;
        logic->CurrentRegionKey = RR_KAK_BACKYARD;
        if (exit.CheckConditionAtAgeTime(logic->IsAdult, logic->AtDay)) {
            open++;
        }
        logic->CurrentRegionKey = RR_NONE;
    }
    return found == 0 ? -1 : (open > 0 ? 1 : 0);
}

bool RegionHasExitTo(RandomizerRegion from, RandomizerRegion to) {
    for (Rando::Entrance& exit : RegionTable(from)->exits) {
        if (exit.GetConnectedRegionKey() == to) {
            return true;
        }
    }
    return false;
}

void RunShuffleClimbLegs(const std::shared_ptr<Rando::Context>& randoCtx) {
    const uint8_t savedShuffleClimb = randoCtx->GetOption(RSK_SHUFFLE_CLIMB).Get();
    const uint8_t savedShuffleOpenChest = randoCtx->GetOption(RSK_SHUFFLE_OPEN_CHEST).Get();
    const uint8_t savedUnintuitive = randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Get();
    const uint8_t savedHoverBoost = randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Get();
    randoCtx->GetOption(RSK_SHUFFLE_CLIMB).Set(1);
    randoCtx->GetOption(RSK_SHUFFLE_OPEN_CHEST).Set(1);
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(0);
    randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Set(0);

    // ---- F: the Kak backyard ladder to the potion shop's back (SoH b6cdc6c49)
    ResetClimbLogic();
    const int f1 = KakBackyardLadderSideOpen();
    if (f1 < 0) {
        GateFail("F1 Kak backyard: no exit beyond the ground-level three, so the probe evaluates nothing");
    } else if (f1 != 0) {
        GateFail("F1 Kak backyard, adult by day, no Climb: the potion shop's back side opens from the backyard "
                 "without climbing its ladder (#1277)");
    } else {
        printf("[TEST]   ok: F1 Kak backyard ladder, adult by day, no Climb: closed\n");
    }
    GiveClimb();
    if (KakBackyardLadderSideOpen() != 1) {
        GateFail("F2 Kak backyard, adult by day, with Climb: the potion shop's back side stays closed");
    } else {
        printf("[TEST]   ok: F2 Kak backyard ladder, adult by day, with Climb: open\n");
    }
    // The watchtower jump to the rooftop is any age under the trick (same commit).
    ResetClimbLogic();
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(1);
    ExpectExit("F3 Kak watchtower to rooftop, adult, unintuitive jumps", RR_KAK_WATCHTOWER, RR_KAK_ROOFTOP, true, true,
               "the trick's watchtower jump to the rooftop is child-only");
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(0);
    ExpectExit("F4 Kak watchtower to rooftop, adult, no trick", RR_KAK_WATCHTOWER, RR_KAK_ROOFTOP, true, false,
               "the watchtower jump is in logic without the trick");

    // ---- G: the DMC wall Piece of Heart needs Climb (SoH ec27775e8, PoH clause)
    ResetClimbLogic();
    if (logic->Hearts() < 3) {
        GateFail("G: Logic::Reset left %d hearts, so the heat term fails and the legs are vacuous",
                 (int)logic->Hearts());
    }
    ExpectAt("G1 DMC wall PoH, adult with 3 hearts, no Climb", RR_DMC_UPPER_LOCAL, RC_DMC_WALL_FREESTANDING_POH, true,
             false, "the wall alcove is in logic without climbing down to it (#1277)");
    GiveClimb();
    ExpectAt("G2 DMC wall PoH, adult with 3 hearts and Climb", RR_DMC_UPPER_LOCAL, RC_DMC_WALL_FREESTANDING_POH, true,
             true, "the wall alcove is out of logic with Climb and the hearts");

    // ---- H: DC Armos Room to Bomb Room Lower (SoH fe5dce24b) ---------------
    ResetClimbLogic();
    GiveGrab();
    ExpectExit("H1 DC Armos Room to Bomb Room Lower, child with bracelet, no Climb", RR_DODONGOS_CAVERN_ARMOS_ROOM,
               RR_DODONGOS_CAVERN_BOMB_ROOM_LOWER, false, false,
               "a child with the bracelet passes the ladder without Climb (#1277)");
    GiveClimb();
    ExpectExit("H2 DC Armos Room to Bomb Room Lower, child with bracelet and Climb", RR_DODONGOS_CAVERN_ARMOS_ROOM,
               RR_DODONGOS_CAVERN_BOMB_ROOM_LOWER, false, true, "a child with bracelet and Climb cannot pass");

    // ---- I: the Child Spirit locked door leads to Sun On Floor 1F (SoH fe5dce24b)
    if (RegionHasExitTo(RR_SPIRIT_TEMPLE_CHILD_BOXES, RR_SPIRIT_TEMPLE_SUN_ON_FLOOR_2F) ||
        !RegionHasExitTo(RR_SPIRIT_TEMPLE_CHILD_BOXES, RR_SPIRIT_TEMPLE_SUN_ON_FLOOR_1F)) {
        GateFail("I Child Spirit before climb: the locked door leads to Sun On Floor 2F, past the climb, instead of "
                 "1F (#1277)");
    } else {
        printf("[TEST]   ok: I Child Spirit before climb: the locked door leads to Sun On Floor 1F\n");
    }

    // ---- J: the Deku Tree 2F grass is on 2F (SoH fe5dce24b) ----------------
    // The two grass checks after LOBBY_GRASS_3 (the fix renames them 2F_GRASS_1/2;
    // the enum values do not move).
    for (int i = 1; i <= 2; i++) {
        const RandomizerCheck grass = (RandomizerCheck)(RC_DEKU_TREE_LOBBY_GRASS_3 + i);
        char leg[96];
        ResetClimbLogic();
        GiveKokiriSword();
        snprintf(leg, sizeof(leg), "J%d Deku Tree 2F grass %d, child with a sword, no Climb", i * 2 - 1, i);
        ExpectSearch(leg, RR_DEKU_TREE_LOBBY, false, RR_NONE, grass, false,
                     "grass on the 2F ledge is cut from the lobby floor without climbing (#1277)");
        GiveClimb();
        snprintf(leg, sizeof(leg), "J%d Deku Tree 2F grass %d, child with a sword and Climb", i * 2, i);
        ExpectSearch(leg, RR_DEKU_TREE_LOBBY, false, RR_NONE, grass, true, "the 2F grass is out of reach with Climb");
    }

    // ---- K: the Deku Tree map chest is on 2F and needs Open Chest (SoH 743103fbe)
    ResetClimbLogic();
    GiveOpenChest();
    ExpectSearch("K1 Deku Tree map chest, child with Open Chest, no Climb", RR_DEKU_TREE_LOBBY, false, RR_NONE,
                 RC_DEKU_TREE_MAP_CHEST, false, "the 2F map chest is listed on the lobby floor (#1277)");
    ResetClimbLogic();
    GiveClimb();
    ExpectSearch("K2 Deku Tree map chest, child with Climb, no Open Chest", RR_DEKU_TREE_LOBBY, false, RR_NONE,
                 RC_DEKU_TREE_MAP_CHEST, false, "the 2F map chest ignores Shuffle Open Chest (#1277)");
    GiveOpenChest();
    ExpectSearch("K3 Deku Tree map chest, child with Climb and Open Chest", RR_DEKU_TREE_LOBBY, false, RR_NONE,
                 RC_DEKU_TREE_MAP_CHEST, true, "the map chest is out of reach with Climb and Open Chest");

    // ---- L: the GV waterfall alcove needs the Longshot AND Climb (SoH 09b0fdbc9,
    // without its RT_HOOKSHOT_LADDERS clause) -------------------------------------
    ResetClimbLogic();
    GiveHookshot(true);
    ExpectExit("L1 GV waterfall alcove, adult with Longshot, no Climb", RR_GV_UPPER_STREAM, RR_GV_WATERFALL_ALCOVE,
               true, false, "the Longshot alone reaches the alcove up the ladder (#1277)");
    ResetClimbLogic();
    GiveHookshot(false);
    GiveClimb();
    ExpectExit("L2 GV waterfall alcove, adult with Hookshot and Climb", RR_GV_UPPER_STREAM, RR_GV_WATERFALL_ALCOVE,
               true, false, "the Hookshot cannot reach the alcove's ladder, but logic says it can (#1277)");
    ResetClimbLogic();
    GiveHookshot(true);
    GiveClimb();
    ExpectExit("L3 GV waterfall alcove, adult with Longshot and Climb", RR_GV_UPPER_STREAM, RR_GV_WATERFALL_ALCOVE,
               true, true, "the Longshot and Climb cannot reach the alcove");

    // ---- M: a child leaves SFM only through the gate it opened (SoH ed4868f62)
    ResetClimbLogic();
    ExpectExit("M1 SFM to entryway, child who never killed the Wolfos", RR_SACRED_FOREST_MEADOW, RR_SFM_ENTRYWAY, false,
               false, "a child walks back through the Wolfos gate it never opened (#1277)");
    GiveKokiriSword();
    if (SceneSearch(RR_SFM_ENTRYWAY, false, RR_SACRED_FOREST_MEADOW, RC_UNKNOWN_CHECK) != 1) {
        GateFail("M2 setup: a child with the Kokiri Sword does not reach the meadow from the entryway");
    }
    // The search fired the entryway's events, and they stay set in logic.
    ExpectExit("M2 SFM to entryway, child who killed the Wolfos", RR_SACRED_FOREST_MEADOW, RR_SFM_ENTRYWAY, false, true,
               "the opened Wolfos gate does not let the child back out");
    ResetClimbLogic();
    ExpectExit("M3 SFM to entryway, adult", RR_SACRED_FOREST_MEADOW, RR_SFM_ENTRYWAY, true, true,
               "an adult cannot walk out of the meadow");

    // ---- N: the MQ DC torch room's ladder to the entrance-side bridge (SoH
    // 848106039, CanClimbLadder read as HasItem(RG_CLIMB): RT_HOOKSHOT_LADDERS is
    // not ported) ------------------------------------------------------------
    ResetClimbLogic();
    ExpectSearch("N1 MQ DC torch room, child with nothing", RR_DODONGOS_CAVERN_MQ_TORCH_PUZZLE_LOWER, false,
                 RR_DODONGOS_CAVERN_MQ_DODONGO_ROOM, RC_UNKNOWN_CHECK, false,
                 "the Dodongo room is reached from the torch room floor without climbing (#1277)");
    GiveClimb();
    ExpectSearch("N2 MQ DC torch room, child with Climb", RR_DODONGOS_CAVERN_MQ_TORCH_PUZZLE_LOWER, false,
                 RR_DODONGOS_CAVERN_MQ_DODONGO_ROOM, RC_UNKNOWN_CHECK, true,
                 "a child with Climb cannot reach the Dodongo room from the torch room");
    ResetClimbLogic();
    GiveGrab();
    ExpectSearch("N3 MQ DC torch room, child with the bracelet", RR_DODONGOS_CAVERN_MQ_TORCH_PUZZLE_LOWER, false,
                 RR_DODONGOS_CAVERN_MQ_DODONGO_ROOM, RC_UNKNOWN_CHECK, true,
                 "a child with the bracelet cannot reach the Dodongo room from the torch room");

    // ---- O: the Jabu MQ near-boss chest needs Open Chest (SoH 7be5a4fea) ---
    ResetClimbLogic();
    GiveSlingshot();
    ExpectAt("O1 Jabu MQ near-boss chest, child with slingshot, no Open Chest", RR_JABU_JABUS_BELLY_MQ_NEAR_BOSS_ROOM,
             RC_JABU_JABUS_BELLY_MQ_NEAR_BOSS_CHEST, false, false, "the chest ignores Shuffle Open Chest (#1277)");
    GiveOpenChest();
    ExpectAt("O2 Jabu MQ near-boss chest, child with slingshot and Open Chest", RR_JABU_JABUS_BELLY_MQ_NEAR_BOSS_ROOM,
             RC_JABU_JABUS_BELLY_MQ_NEAR_BOSS_CHEST, false, true, "the chest is out of logic with the slingshot");

    // ---- P: the MQ Spirit child doors under hover boosts (SoH 688913270) ---
    // With the hover-boost trick a child can recoil past the jet and spend a key
    // without Statue Room access, so each door needs one key more.
    randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Set(1);
    ResetClimbLogic();
    logic->SetSmallKeyCount(SCENE_SPIRIT_TEMPLE, 1);
    ExpectExit("P1 MQ Spirit to Under Like Like, child, 1 key, hover boosts", RR_SPIRIT_TEMPLE_MQ_1F_CHEST_SWITCH,
               RR_SPIRIT_TEMPLE_MQ_UNDER_LIKE_LIKE, false, false,
               "one key opens the door though a hover boost can waste it (#1277)");
    logic->SetSmallKeyCount(SCENE_SPIRIT_TEMPLE, 2);
    ExpectExit("P2 MQ Spirit to Under Like Like, child, 2 keys, hover boosts", RR_SPIRIT_TEMPLE_MQ_1F_CHEST_SWITCH,
               RR_SPIRIT_TEMPLE_MQ_UNDER_LIKE_LIKE, false, true, "two keys do not open the door");
    ExpectExit("P3 MQ Spirit to Statue Room Child, child, 2 keys, hover boosts", RR_SPIRIT_TEMPLE_MQ_SUN_ON_FLOOR,
               RR_SPIRIT_TEMPLE_MQ_STATUE_ROOM_CHILD, false, false,
               "two keys open the statue room door though a hover boost can waste one (#1277)");
    logic->SetSmallKeyCount(SCENE_SPIRIT_TEMPLE, 3);
    ExpectExit("P4 MQ Spirit to Statue Room Child, child, 3 keys, hover boosts", RR_SPIRIT_TEMPLE_MQ_SUN_ON_FLOOR,
               RR_SPIRIT_TEMPLE_MQ_STATUE_ROOM_CHILD, false, true, "three keys do not open the door");
    randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Set(0);
    ResetClimbLogic();
    logic->SetSmallKeyCount(SCENE_SPIRIT_TEMPLE, 1);
    ExpectExit("P5 MQ Spirit to Under Like Like, child, 1 key, no hover boosts", RR_SPIRIT_TEMPLE_MQ_1F_CHEST_SWITCH,
               RR_SPIRIT_TEMPLE_MQ_UNDER_LIKE_LIKE, false, true, "without the trick one key no longer opens the door");

    randoCtx->GetOption(RSK_SHUFFLE_CLIMB).Set(savedShuffleClimb);
    randoCtx->GetOption(RSK_SHUFFLE_OPEN_CHEST).Set(savedShuffleOpenChest);
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(savedUnintuitive);
    randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Set(savedHoverBoost);
}

void GiveZeldasLullaby() {
    GiveFairyOcarina();
    Give(RG_ZELDAS_LULLABY);
}

void GiveGoronsBracelet() {
    Give(RG_PROGRESSIVE_STRENGTH);
    Give(RG_PROGRESSIVE_STRENGTH);
}

// Appended by #1300 right after RR_DODONGOS_CAVERN_MQ_ENTRANCE_SIDE_BRIDGE and
// RT_GANONS_CASTLE_GOLD_GAUNTLET_SKIP (and RT_GF_WASTELAND_GATE_SIDEHOP_SKIP
// renames RT_GF_CHILD_SKIP_WASTELAND_GATE in place); guarded against RR_MAX /
// RT_MAX so the [lock] commit is test-only and compiles on the pre-fix tree.
constexpr RandomizerRegion kLwUnderBridge =
    static_cast<RandomizerRegion>(RR_DODONGOS_CAVERN_MQ_ENTRANCE_SIDE_BRIDGE + 1);
constexpr RandomizerRegion kGfTower = static_cast<RandomizerRegion>(RR_DODONGOS_CAVERN_MQ_ENTRANCE_SIDE_BRIDGE + 2);
constexpr RandomizerTrick kHookshotLaddersTrick = static_cast<RandomizerTrick>(RT_GANONS_CASTLE_GOLD_GAUNTLET_SKIP + 1);
constexpr RandomizerTrick kGfWastelandGateSidehopSkip = static_cast<RandomizerTrick>(RT_PASS_GUARDS_WITH_NOTHING + 1);
static_assert(kLwUnderBridge == RR_LW_UNDER_BRIDGE, "kLwUnderBridge drifted from RR_LW_UNDER_BRIDGE");
static_assert(kGfTower == RR_GF_TOWER, "kGfTower drifted from RR_GF_TOWER");
static_assert(kHookshotLaddersTrick == RT_HOOKSHOT_LADDERS, "kHookshotLaddersTrick drifted from RT_HOOKSHOT_LADDERS");
static_assert(kGfWastelandGateSidehopSkip == RT_GF_WASTELAND_GATE_SIDEHOP_SKIP,
              "kGfWastelandGateSidehopSkip drifted from RT_GF_WASTELAND_GATE_SIDEHOP_SKIP");

uint8_t GetHookshotLadders(const std::shared_ptr<Rando::Context>& randoCtx) {
    return kHookshotLaddersTrick < RT_MAX ? randoCtx->GetTrickOption(kHookshotLaddersTrick).Get() : 0;
}

void SetHookshotLadders(const std::shared_ptr<Rando::Context>& randoCtx, uint8_t val) {
    if (kHookshotLaddersTrick < RT_MAX) {
        randoCtx->GetTrickOption(kHookshotLaddersTrick).Set(val);
    }
}

void RunHookshotLaddersLegs(const std::shared_ptr<Rando::Context>& randoCtx) {
    const uint8_t savedShuffleClimb = randoCtx->GetOption(RSK_SHUFFLE_CLIMB).Get();
    const uint8_t savedShuffleOpenChest = randoCtx->GetOption(RSK_SHUFFLE_OPEN_CHEST).Get();
    const uint8_t savedHookshotLadders = GetHookshotLadders(randoCtx);
    const uint8_t savedUnintuitive = randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Get();
    const uint8_t savedGateSkip = randoCtx->GetTrickOption(kGfWastelandGateSidehopSkip).Get();
    randoCtx->GetOption(RSK_SHUFFLE_CLIMB).Set(1);
    randoCtx->GetOption(RSK_SHUFFLE_OPEN_CHEST).Set(1);
    SetHookshotLadders(randoCtx, 0);
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(0);
    randoCtx->GetTrickOption(kGfWastelandGateSidehopSkip).Set(0);

    // ---- Q: GV waterfall alcove with RT_HOOKSHOT_LADDERS (SoH 09b0fdbc9) ---
    SetHookshotLadders(randoCtx, 1);
    ResetClimbLogic();
    GiveHookshot(false);
    ExpectExit("Q1 GV waterfall alcove, adult with Hookshot, RT_HOOKSHOT_LADDERS on, no Climb", RR_GV_UPPER_STREAM,
               RR_GV_WATERFALL_ALCOVE, true, false,
               "the Hookshot reaches the waterfall alcove though only the Longshot can (#1300)");
    GiveHookshot(true);
    ExpectExit("Q2 GV waterfall alcove, adult with Longshot, RT_HOOKSHOT_LADDERS on, no Climb", RR_GV_UPPER_STREAM,
               RR_GV_WATERFALL_ALCOVE, true, true,
               "the Longshot with RT_HOOKSHOT_LADDERS does not reach the waterfall alcove (#1300)");
    SetHookshotLadders(randoCtx, 0);

    // ---- R: Dodongo's Cavern MQ compass chest (SoH 60ec65592) --------------
    ResetClimbLogic();
    GiveGoronsBracelet();
    GiveOpenChest();
    ExpectAt("R1 DC MQ compass chest, adult with Goron's Bracelet, no weapon, no Climb",
             RR_DODONGOS_CAVERN_MQ_DODONGO_ROOM, RC_DODONGOS_CAVERN_MQ_COMPASS_CHEST, true, false,
             "pushing the block reaches the MQ compass chest without climbing the ladder (#1300)");
    GiveHookshot(false);
    ExpectAt("R2 DC MQ compass chest, adult with Goron's Bracelet and Hookshot, trick off, no Climb",
             RR_DODONGOS_CAVERN_MQ_DODONGO_ROOM, RC_DODONGOS_CAVERN_MQ_COMPASS_CHEST, true, false,
             "the Hookshot climbs the MQ compass chest ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    SetHookshotLadders(randoCtx, 1);
    ExpectAt("R3 DC MQ compass chest, adult with Goron's Bracelet and Hookshot, RT_HOOKSHOT_LADDERS on",
             RR_DODONGOS_CAVERN_MQ_DODONGO_ROOM, RC_DODONGOS_CAVERN_MQ_COMPASS_CHEST, true, true,
             "the Hookshot with RT_HOOKSHOT_LADDERS does not climb the MQ compass chest ladder (#1300)");
    SetHookshotLadders(randoCtx, 0);
    GiveClimb();
    ExpectAt("R4 DC MQ compass chest, adult with Goron's Bracelet and Climb", RR_DODONGOS_CAVERN_MQ_DODONGO_ROOM,
             RC_DODONGOS_CAVERN_MQ_COMPASS_CHEST, true, true,
             "Goron's Bracelet and Climb cannot reach the MQ compass chest");

    // ---- S: Bottom of the Well ladders (SoH 60ec65592) ---------------------
    ResetClimbLogic();
    GiveHookshot(true);
    ExpectExit("S1 BotW B3 ooze to hidden pots, adult with Longshot, trick off, no Climb", RR_BOTW_B3_OOZE,
               RR_BOTW_HIDDEN_POTS, true, false,
               "the Longshot climbs the BotW basement high ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    ExpectExit("S2 BotW MQ B3 to MQ perimeter, adult with Longshot, trick off, no Climb", RR_BOTW_MQ_B3,
               RR_BOTW_MQ_PERIMETER, true, false,
               "the Longshot climbs the BotW MQ basement high ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    SetHookshotLadders(randoCtx, 1);
    GiveHookshot(false);
    ExpectExit("S3 BotW B3 ooze to hidden pots, adult with Hookshot, RT_HOOKSHOT_LADDERS on, no Climb", RR_BOTW_B3_OOZE,
               RR_BOTW_HIDDEN_POTS, true, false, "the short Hookshot climbs the BotW basement high ladder (#1300)");
    GiveHookshot(true);
    ExpectExit("S4 BotW B3 ooze to hidden pots, adult with Longshot, RT_HOOKSHOT_LADDERS on, no Climb", RR_BOTW_B3_OOZE,
               RR_BOTW_HIDDEN_POTS, true, true,
               "the Longshot with RT_HOOKSHOT_LADDERS does not climb the BotW basement high ladder (#1300)");
    ExpectExit("S5 BotW MQ B3 to MQ perimeter, adult with Longshot, RT_HOOKSHOT_LADDERS on, no Climb", RR_BOTW_MQ_B3,
               RR_BOTW_MQ_PERIMETER, true, true,
               "the Longshot with RT_HOOKSHOT_LADDERS does not climb the BotW MQ basement high ladder (#1300)");
    SetHookshotLadders(randoCtx, 0);

    // ---- T: Shadow Temple boat dock ladder (SoH 60ec65592) -----------------
    ResetClimbLogic();
    GiveZeldasLullaby();
    GiveHookshot(false);
    ExpectExit("T1 Shadow dock to beyond boat, adult with Lullaby and Hookshot, trick off, no Climb",
               RR_SHADOW_TEMPLE_DOCK, RR_SHADOW_TEMPLE_BEYOND_BOAT, true, false,
               "the Hookshot climbs the boat dock ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    SetHookshotLadders(randoCtx, 1);
    ExpectExit("T2 Shadow dock to beyond boat, adult with Lullaby and Hookshot, RT_HOOKSHOT_LADDERS on, no Climb",
               RR_SHADOW_TEMPLE_DOCK, RR_SHADOW_TEMPLE_BEYOND_BOAT, true, true,
               "the Hookshot with RT_HOOKSHOT_LADDERS does not climb the boat dock ladder (#1300)");
    SetHookshotLadders(randoCtx, 0);
    ResetClimbLogic();
    GiveZeldasLullaby();
    GiveGoronsBracelet();
    GiveClimb();
    ExpectExit("T3 Shadow dock to beyond boat, adult with Lullaby, Goron's Bracelet and Climb", RR_SHADOW_TEMPLE_DOCK,
               RR_SHADOW_TEMPLE_BEYOND_BOAT, true, true,
               "Goron's Bracelet and Climb do not reach the Shadow Temple boat from the dock");

    // ---- U: DMC ladder and Hyrule Field to Lake Hylia fence (SoH 60ec65592)
    ResetClimbLogic();
    GiveHookshot(false);
    ExpectExit("U1 DMC ladder area to upper nearby, adult with Hookshot, trick off, no Climb",
               RR_DMC_LADDER_AREA_NEARBY, RR_DMC_UPPER_NEARBY, true, false,
               "the Hookshot climbs the DMC ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    ExpectExit("U2 Hyrule Field to Lake Hylia, adult with Hookshot, trick off, no Climb", RR_HYRULE_FIELD,
               RR_HF_TO_LAKE_HYLIA, true, false,
               "the Hookshot climbs the Lake Hylia fence ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    ExpectExit(
        "U3 HF to Lake Hylia back to Hyrule Field, adult with Hookshot, trick off, no Climb", RR_HF_TO_LAKE_HYLIA,
        RR_HYRULE_FIELD, true, false,
        "the Hookshot climbs the Lake Hylia fence ladder from the lake side with RT_HOOKSHOT_LADDERS off (#1300)");
    SetHookshotLadders(randoCtx, 1);
    ExpectExit("U4 DMC ladder area to upper nearby, adult with Hookshot, RT_HOOKSHOT_LADDERS on, no Climb",
               RR_DMC_LADDER_AREA_NEARBY, RR_DMC_UPPER_NEARBY, true, true,
               "the Hookshot with RT_HOOKSHOT_LADDERS does not climb the DMC ladder (#1300)");
    ExpectExit("U5 Hyrule Field to Lake Hylia, adult with Hookshot, RT_HOOKSHOT_LADDERS on, no Climb", RR_HYRULE_FIELD,
               RR_HF_TO_LAKE_HYLIA, true, true,
               "the Hookshot with RT_HOOKSHOT_LADDERS does not climb the Lake Hylia fence ladder (#1300)");
    ExpectExit(
        "U6 HF to Lake Hylia back to Hyrule Field, adult with Hookshot, RT_HOOKSHOT_LADDERS on, no Climb",
        RR_HF_TO_LAKE_HYLIA, RR_HYRULE_FIELD, true, true,
        "the Hookshot with RT_HOOKSHOT_LADDERS does not climb the Lake Hylia fence ladder from the lake side (#1300)");
    SetHookshotLadders(randoCtx, 0);

    // ---- V: Lost Woods under bridge and Sacred Forest Meadow (SoH 60ec65592)
    ResetClimbLogic();
    ExpectExit("V1 LW Under Bridge to Lost Woods, child with nothing", kLwUnderBridge, RR_THE_LOST_WOODS, false, false,
               "a child climbs from under the Lost Woods bridge without Climb");
    GiveHookshot(false);
    ExpectExit("V2 LW Under Bridge to Lost Woods, adult with Hookshot, trick off, no Climb", kLwUnderBridge,
               RR_THE_LOST_WOODS, true, false,
               "the Hookshot climbs the Lost Woods under-bridge ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    SetHookshotLadders(randoCtx, 1);
    ExpectExit("V3 LW Under Bridge to Lost Woods, adult with Hookshot, RT_HOOKSHOT_LADDERS on, no Climb",
               kLwUnderBridge, RR_THE_LOST_WOODS, true, true,
               "the Hookshot with RT_HOOKSHOT_LADDERS does not climb the Lost Woods under-bridge ladder (#1300)");
    SetHookshotLadders(randoCtx, 0);
    GiveClimb();
    ExpectExit("V4 LW Under Bridge to Lost Woods, child with Climb", kLwUnderBridge, RR_THE_LOST_WOODS, false, true,
               "Climb does not reach Lost Woods from under the bridge (#1300)");

    ResetClimbLogic();
    GiveHookshot(false);
    ExpectExit("V5 SFM to Above Maze, adult with Hookshot, trick off, no Climb", RR_SACRED_FOREST_MEADOW,
               RR_SFM_ABOVE_MAZE, true, false,
               "the Hookshot climbs the SFM maze ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    ExpectExit("V6 SFM Outside Fairy Grotto to Above Maze, adult with Hookshot, trick off, no Climb",
               RR_SFM_OUTSIDE_FAIRY_GROTTO, RR_SFM_ABOVE_MAZE, true, false,
               "the Hookshot climbs the SFM fairy grotto ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    SetHookshotLadders(randoCtx, 1);
    ExpectExit("V7 SFM to Above Maze, adult with Hookshot, RT_HOOKSHOT_LADDERS on, no Climb", RR_SACRED_FOREST_MEADOW,
               RR_SFM_ABOVE_MAZE, true, true,
               "the Hookshot with RT_HOOKSHOT_LADDERS does not climb the SFM maze ladder (#1300)");
    ExpectExit("V8 SFM Outside Fairy Grotto to Above Maze, adult with Hookshot, RT_HOOKSHOT_LADDERS on, no Climb",
               RR_SFM_OUTSIDE_FAIRY_GROTTO, RR_SFM_ABOVE_MAZE, true, true,
               "the Hookshot with RT_HOOKSHOT_LADDERS does not climb the SFM fairy grotto ladder (#1300)");
    SetHookshotLadders(randoCtx, 0);

    // ---- W: Gerudo Fortress tower and wasteland gate skip (SoH 60ec65592) --
    ResetClimbLogic();
    Give(RG_GERUDO_MEMBERSHIP_CARD);
    GiveHookshot(true);
    ExpectExit("W1 GF Outskirts to Tower, adult with Card and Longshot, trick off, no Climb", RR_GF_OUTSKIRTS, kGfTower,
               true, false, "the Longshot climbs the GF tower high ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    SetHookshotLadders(randoCtx, 1);
    GiveHookshot(false);
    ExpectExit("W2 GF Outskirts to Tower, adult with Card and Hookshot, RT_HOOKSHOT_LADDERS on, no Climb",
               RR_GF_OUTSKIRTS, kGfTower, true, false, "the short Hookshot climbs the GF tower high ladder (#1300)");
    GiveHookshot(true);
    ExpectExit("W3 GF Outskirts to Tower, adult with Card and Longshot, RT_HOOKSHOT_LADDERS on, no Climb",
               RR_GF_OUTSKIRTS, kGfTower, true, true,
               "the Longshot with RT_HOOKSHOT_LADDERS does not climb the GF tower high ladder (#1300)");
    logic->SetRandoInf(RAND_INF_CAN_SPEAK_GERUDO, true);
    SetHookshotLadders(randoCtx, 0);
    ExpectSearch("W4 GF Outskirts to Outside Gate, adult with Longshot, Card and Speak, trick off, no Climb",
                 RR_GF_OUTSKIRTS, true, RR_GF_OUTSIDE_GATE, RC_UNKNOWN_CHECK, false,
                 "LOGIC_GF_GATE_OPEN fires without climbing the GF tower (#1300)");
    SetHookshotLadders(randoCtx, 1);
    ExpectSearch("W5 GF Outskirts to Outside Gate, adult with Longshot, Card and Speak, RT_HOOKSHOT_LADDERS on",
                 RR_GF_OUTSKIRTS, true, RR_GF_OUTSIDE_GATE, RC_UNKNOWN_CHECK, true,
                 "an adult on the GF tower with Card and Speak does not open the wasteland gate (#1300)");
    SetHookshotLadders(randoCtx, 0);

    ResetClimbLogic();
    GiveClimb();
    ExpectSearch("W6 GF Outskirts to Outside Gate, child with Climb, gate skip off", RR_GF_OUTSKIRTS, false,
                 RR_GF_OUTSIDE_GATE, RC_UNKNOWN_CHECK, false,
                 "a child passes the wasteland gate without the sidehop skip trick");
    randoCtx->GetTrickOption(kGfWastelandGateSidehopSkip).Set(1);
    ExpectSearch("W7 GF Outskirts to Outside Gate, child with Climb, RT_GF_WASTELAND_GATE_SIDEHOP_SKIP on",
                 RR_GF_OUTSKIRTS, false, RR_GF_OUTSIDE_GATE, RC_UNKNOWN_CHECK, true,
                 "the wasteland gate sidehop skip stays closed for a child (IsAdult && IsChild typo, #1300)");
    randoCtx->GetTrickOption(kGfWastelandGateSidehopSkip).Set(0);

    // ---- X: Zora's River ladder (SoH 60ec65592) ----------------------------
    ResetClimbLogic();
    GiveHookshot(false);
    ExpectExit("X1 Zora's River to Atop Ladder, adult with Hookshot, trick off, no Climb", RR_ZORAS_RIVER,
               RR_ZR_ATOP_LADDER, true, false,
               "the Hookshot climbs the Zora's River ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    SetHookshotLadders(randoCtx, 1);
    ExpectExit("X2 Zora's River to Atop Ladder, adult with Hookshot, RT_HOOKSHOT_LADDERS on, no Climb", RR_ZORAS_RIVER,
               RR_ZR_ATOP_LADDER, true, true,
               "the Hookshot with RT_HOOKSHOT_LADDERS does not climb the Zora's River ladder (#1300)");
    SetHookshotLadders(randoCtx, 0);

    randoCtx->GetOption(RSK_SHUFFLE_CLIMB).Set(savedShuffleClimb);
    randoCtx->GetOption(RSK_SHUFFLE_OPEN_CHEST).Set(savedShuffleOpenChest);
    SetHookshotLadders(randoCtx, savedHookshotLadders);
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(savedUnintuitive);
    randoCtx->GetTrickOption(kGfWastelandGateSidehopSkip).Set(savedGateSkip);
}

} // namespace

extern "C" int OoT_LogicGates_RunHeadless(void) {
    printf("[TEST] oot-logic-gates: Song from Malon, Talon's chickens, the Thieves' Hideout carpenters and the carpet "
           "merchant need what the game needs (#1168)\n");
    sFailures = 0;

    // The same fallback Rando_HeadlessSeedTest uses when the bring-up did not
    // create the randomizer context (3drando/menu.cpp).
    auto randoCtx = Rando::Context::GetInstance();
    if (!randoCtx) {
        randoCtx = Rando::Context::CreateInstance();
        randoCtx->InitStaticData();
        Rando::Settings::GetInstance()->AssignContext(randoCtx);
        Rando::StaticData::InitItemTable();
        Rando::StaticData::InitLocationTable();
    }

    // The options the gates are about, forced and restored. Glitchless, because
    // LocationAccess::GetConditionsMet answers true for everything otherwise.
    const uint8_t savedLogicRules = randoCtx->GetOption(RSK_LOGIC_RULES).Get();
    const uint8_t savedShuffleSpeak = randoCtx->GetOption(RSK_SHUFFLE_SPEAK).Get();
    const uint8_t savedShuffleGrab = randoCtx->GetOption(RSK_SHUFFLE_GRAB).Get();
    const uint8_t savedShufflePots = randoCtx->GetOption(RSK_SHUFFLE_POTS).Get();
    randoCtx->GetOption(RSK_LOGIC_RULES).Set(RO_LOGIC_GLITCHLESS);
    randoCtx->GetOption(RSK_SHUFFLE_SPEAK).Set(1);
    randoCtx->GetOption(RSK_SHUFFLE_GRAB).Set(1);

    // The world graph, bound to this context's logic (location_access.cpp).
    RegionTable_Init();
    if (!logic) {
        GateFail("RegionTable_Init bound no logic object");
        return 1;
    }
    const bool savedChild = logic->IsChild;
    const bool savedAdult = logic->IsAdult;
    const bool savedDay = logic->AtDay;
    const bool savedNight = logic->AtNight;

    // ---- A: Song from Malon needs the Hylian jabber nut (SoH c906c71f7) ----
    ResetLogic();
    GiveZeldasLetter();
    GiveFairyOcarina();
    Expect("A1 Song from Malon, no jabber nut", RR_LON_LON_RANCH, RC_SONG_FROM_MALON, false,
           "Malon teaches her song to a child who cannot talk to her (#1168)");
    GiveNut();
    Expect("A2 Song from Malon, with the jabber nut", RR_LON_LON_RANCH, RC_SONG_FROM_MALON, true,
           "a child with the letter, the ocarina and the nut cannot get Malon's song");

    // ---- B: Talon's chickens need the Power Bracelet (SoH 63597ec63) -------
    ResetLogic();
    GiveChildWallet();
    GiveNut();
    GiveZeldasLetter();
    Expect("B1 Talon's chickens, no Power Bracelet", RR_LLR_TALONS_HOUSE, RC_LLR_TALONS_CHICKENS, false,
           "the cucco game is in logic for a child who cannot pick up a cucco");
    GiveGrab();
    Expect("B2 Talon's chickens, with the Power Bracelet", RR_LLR_TALONS_HOUSE, RC_LLR_TALONS_CHICKENS, true,
           "a child with wallet, nut, letter and bracelet cannot play the cucco game");

    // ---- C: each carpenter needs the cell's "could free" event (SoH 1413ceb33)
    for (const Carpenter& carpenter : kCarpenters) {
        char leg[96];
        ResetLogic();
        GiveSticks();
        snprintf(leg, sizeof(leg), "C1 %s, no jabber nut", carpenter.name);
        Expect(leg, carpenter.region, carpenter.check, false,
               "the carpenter's check is in logic for a fighter who cannot talk to the carpenter");
        GiveNut();
        snprintf(leg, sizeof(leg), "C2 %s, with the jabber nut", carpenter.name);
        Expect(leg, carpenter.region, carpenter.check, true,
               "a fighter with the nut does not reach the carpenter (the cell's event did not fire)");
    }

    // ---- D: the carpet merchant's parentheses (SoH 0702b11b9) --------------
    // GetCheckPrice reads the merchant's ItemLocation through OTRGlobals.
    if (OTRGlobals::Instance == nullptr || !OTRGlobals::Instance->gRandoContext) {
        GateFail("D: no OTRGlobals randomizer context, so the merchant's price cannot be read");
    } else {
        Rando::ItemLocation* merchant = randoCtx->GetItemLocation(RC_WASTELAND_BOMBCHU_SALESMAN);
        const uint16_t savedPrice = merchant->GetPrice();
        merchant->SetPrice(60);
        if (merchant->GetPrice() != 60) {
            GateFail("D: the merchant's price did not take (a custom price is pinned), so the price legs are vacuous");
        } else {
            ResetLogic();
            GiveSticks();
            GiveChildWallet(); // 99 rupees
            Expect("D1 carpet merchant, jumpslash and wallet, no jabber nut", RR_HAUNTED_WASTELAND,
                   RC_WASTELAND_BOMBCHU_SALESMAN, false,
                   "a jumpslash alone satisfies the merchant: the `||` swallows the nut and price terms");
            GiveNut();
            Expect("D2 carpet merchant, nut and a 60-rupee price in a 99 wallet", RR_HAUNTED_WASTELAND,
                   RC_WASTELAND_BOMBCHU_SALESMAN, true, "the merchant is out of logic with the nut and the rupees");
            merchant->SetPrice(200);
            Expect("D3 carpet merchant, nut but a 200-rupee price in a 99 wallet", RR_HAUNTED_WASTELAND,
                   RC_WASTELAND_BOMBCHU_SALESMAN, false,
                   "the merchant is in logic for a price the wallet cannot hold: the price term is bypassed");
        }
        merchant->SetPrice(savedPrice);
    }

    // ---- E: each carpenter's row carries its own cell's key (SoH 2bc50f605)
    for (const Carpenter& carpenter : kCarpenters) {
        ExpectCarpenterRow(carpenter);
    }

    // ---- F: a pot counts only items its scene allows (#1282, SoH 041295f05) --
    randoCtx->GetOption(RSK_SHUFFLE_POTS).Set(RO_SHUFFLE_POTS_ALL);
    for (const HousePot& pot : kHousePots) {
        char leg[96];
        ResetLogic();
        GiveChildPotBreakers();
        snprintf(leg, sizeof(leg), "F1 %s, sword/sticks/slingshot/boomerang/bombs, no grab", pot.name);
        Expect(leg, pot.region, pot.check, false,
               "a weapon the house disables (B, hookshot and C items off) counts for its pot (#1282)");
        GiveGrab();
        snprintf(leg, sizeof(leg), "F2 %s, with grab", pot.name);
        Expect(leg, pot.region, pot.check, true, "a child who can lift and throw cannot break the house pot");
    }
    for (RandomizerCheck rc : { RC_LLR_TALONS_HOUSE_POT_1, RC_LLR_TALONS_HOUSE_POT_2, RC_LLR_TALONS_HOUSE_POT_3 }) {
        const int n = (int)(rc - RC_LLR_TALONS_HOUSE_POT_1) + 1;
        char leg[96];
        ResetLogic();
        GiveSticks();
        Give(RG_PROGRESSIVE_SLINGSHOT);
        Give(RG_BOOMERANG);
        Give(RG_PROGRESSIVE_BOMB_BAG);
        snprintf(leg, sizeof(leg), "F3 Talon's house pot %d, sticks/slingshot/boomerang/bombs, no grab", n);
        Expect(leg, RR_LLR_TALONS_HOUSE, rc, false, "an item Talon's house disables counts for its pot (#1282)");
        Give(RG_KOKIRI_SWORD);
        snprintf(leg, sizeof(leg), "F4 Talon's house pot %d, plus the Kokiri Sword", n);
        Expect(leg, RR_LLR_TALONS_HOUSE, rc, true, "the sword (B is allowed in Talon's house) does not break the pot");
    }
    for (RandomizerCheck rc : { RC_MK_GUARD_HOUSE_ADULT_POT_1, RC_MK_GUARD_HOUSE_ADULT_POT_11 }) {
        const int n = (int)(rc - RC_MK_GUARD_HOUSE_ADULT_POT_1) + 1;
        char leg[96];
        ResetLogic();
        Give(RG_PROGRESSIVE_HOOKSHOT);
        Give(RG_PROGRESSIVE_HOOKSHOT); // the longshot
        snprintf(leg, sizeof(leg), "F5 guard house adult pot %d, longshot only, no grab", n);
        ExpectAs(true, leg, RR_MARKET_GUARD_HOUSE, rc, false,
                 "the hookshot/longshot the guard house disables counts for its pot (#1282)");
        Give(RG_PROGRESSIVE_BOMB_BAG);
        snprintf(leg, sizeof(leg), "F6 guard house adult pot %d, plus a bomb bag", n);
        ExpectAs(true, leg, RR_MARKET_GUARD_HOUSE, rc, true, "bombs (allowed in the guard house) do not break its pot");
    }
    ResetLogic();
    Expect("F7 Goron City pot, nothing", RR_GORON_CITY, RC_GC_LOWER_STAIRCASE_POT_1, false,
           "a Goron City pot is in logic with no way to break it");
    Give(RG_KOKIRI_SWORD);
    Expect("F8 Goron City pot, Kokiri Sword only", RR_GORON_CITY, RC_GC_LOWER_STAIRCASE_POT_1, true,
           "the sword no longer breaks a pot in a scene that allows it");

    // ---- F..P: the Shuffle Climb ladders (#1277, SoH bundle O5) -------------
    RunShuffleClimbLegs(randoCtx);
    // ---- Q..X: Hookshot Ladders and ladder cleanups (#1300, SoH bundle O28) -
    RunHookshotLaddersLegs(randoCtx);
    // ---- restore -------------------------------------------------------------
    randoCtx->GetOption(RSK_LOGIC_RULES).Set(savedLogicRules);
    randoCtx->GetOption(RSK_SHUFFLE_SPEAK).Set(savedShuffleSpeak);
    randoCtx->GetOption(RSK_SHUFFLE_GRAB).Set(savedShuffleGrab);
    randoCtx->GetOption(RSK_SHUFFLE_POTS).Set(savedShufflePots);
    logic->Reset(true);
    logic->IsChild = savedChild;
    logic->IsAdult = savedAdult;
    logic->AtDay = savedDay;
    logic->AtNight = savedNight;

    if (sFailures == 0) {
        printf("[TEST] PASS: oot-logic-gates\n");
    } else {
        printf("[TEST] FAIL: oot-logic-gates: %d leg(s) failed\n", sFailures);
    }
    return sFailures;
}
