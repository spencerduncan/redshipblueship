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
 *
 * Every "must NOT" leg is paired with a "must" leg on the same save plus one
 * thing, so a location that is never reachable (a vacuous probe) fails too.
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

/**
 * The search's question for one location of one region, as the child (day and
 * night). Returns -1 when the probe could not be made (region or location
 * missing), which the caller counts as a failure whatever it expected.
 */
int ChildReaches(RandomizerRegion regionKey, RandomizerCheck rc) {
    Region* region = RegionTable(regionKey);
    const LocationAccess* loc = region != nullptr ? FindLocation(region, rc) : nullptr;
    if (loc == nullptr) {
        return -1;
    }
    const bool saved[4] = { region->childDay, region->childNight, region->adultDay, region->adultNight };
    region->childDay = true;
    region->childNight = true;
    region->adultDay = false;
    region->adultNight = false;
    while (region->UpdateEvents()) {}
    logic->CurrentCheckKey = rc;
    const bool met = loc->ConditionsMet(region, false);
    logic->CurrentCheckKey = RC_UNKNOWN_CHECK;
    region->childDay = saved[0];
    region->childNight = saved[1];
    region->adultDay = saved[2];
    region->adultNight = saved[3];
    return met ? 1 : 0;
}

void Expect(const char* leg, RandomizerRegion regionKey, RandomizerCheck rc, bool expected, const char* why) {
    const int got = ChildReaches(regionKey, rc);
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

struct Carpenter {
    const char* name;
    RandomizerRegion region;
    RandomizerCheck check;
};

const Carpenter kCarpenters[] = {
    { "1 Torch carpenter", RR_TH_1_TORCH_CELL, RC_TH_1_TORCH_CARPENTER },
    { "Double Cell carpenter", RR_TH_DOUBLE_CELL, RC_TH_DOUBLE_CELL_CARPENTER },
    { "Dead End carpenter", RR_TH_DEAD_END_CELL, RC_TH_DEAD_END_CARPENTER },
    { "Steep Slope carpenter", RR_TH_STEEP_SLOPE_CELL, RC_TH_STEEP_SLOPE_CARPENTER },
};

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

    // ---- restore -------------------------------------------------------------
    randoCtx->GetOption(RSK_LOGIC_RULES).Set(savedLogicRules);
    randoCtx->GetOption(RSK_SHUFFLE_SPEAK).Set(savedShuffleSpeak);
    randoCtx->GetOption(RSK_SHUFFLE_GRAB).Set(savedShuffleGrab);
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
