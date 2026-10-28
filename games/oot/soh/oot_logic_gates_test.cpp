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
 * Every "must NOT" leg is paired with a "must" leg on the same save plus one
 * thing, so a location that is never reachable (a vacuous probe) fails too.
 * Each probe sets logic->CurrentRegionKey to its region, as ProcessRegion does.
 */

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>

#include <libultraship/bridge.h>

#include "soh/OTRGlobals.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/location_access.h"
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
