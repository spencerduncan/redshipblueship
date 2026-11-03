/**
 * @file oot_mask_quest_paired_test.cpp
 * A paired world's Mask Quest and Mask Shop Hint (#973), run by the rando row
 * `rando-mask-quest-paired`.
 *
 * The Happy Mask Shop door is the OoT<->MM crossing (entrance.cpp pins it out of
 * every shuffle pool), so the shop's room never loads. SoH's Mask Quest
 * "Vanilla" credits borrowing the masks there and "Completed" credits them once
 * the shop opens, and the Mask Shop Hint is the sign in that room. A paired
 * creation therefore generates Mask Quest as "Shuffle" and the hint off
 * (3drando/playthrough.cpp), as OoTMM does: its Mask Shop is only the crossing
 * and it shuffles all eight OoT masks.
 *
 * The row runs a real headless paired generation for each authored mode a player
 * can arrive with ("Vanilla", the option's default, and "Completed", the shipped
 * presets' value and the operator's 2026-10-17 playtest seed), with the Mask
 * Shop Hint authored on, and checks:
 *   M1 the generated settings: Mask Quest "Shuffle", Mask Shop Hint off;
 *   M2 the pool: each of the eight OoT masks is in the world, in the general-pass
 *      pool the single bag takes or already placed;
 *   M3 the logic, over the REGISTERED OoT engine, every placed item withheld and
 *      everything else in the world assumed: the Deku Theater is reached, yet
 *      neither of its checks is until its own mask is assumed; the Skull Mask
 *      opens only its check, and the Mask of Truth then opens the other;
 *   M4 the menu: each row shows the generated value (it is the row's default),
 *      a stored value (a preset's) is cleared back to it, and the callbacks never
 *      store one, which would take a config off the shipped defaults the
 *      integration rows require.
 *   M5 #1350, the same seam's Logic pin: a paired generation authored with OoT's
 *      Logic "No Logic" (every OoT condition true, so the pair's proof of OoT's
 *      half would be vacuous) generates Glitchless (M5a), leaves the player's
 *      CVar as authored (M5b) and reports the pin Generate's toast reads (M5c);
 *      and over the registered OoT engine, with nothing assumed and every placed
 *      host withheld but the Triforce's, the generated world reaches neither the
 *      Skull Mask check nor OoT's goal nor every location (M5d), while the same
 *      world with the context's rules at No Logic reaches the Skull Mask check
 *      and more locations (the RED half, kept in the row so a change to No
 *      Logic's semantics is reported instead of passing on an empty difference).
 *   M6 #1367, the same seam's derivation from [Both Games] Logic: a paired
 *      generation at "No Logic" (rung 1) is generated with OoT's Logic No Logic
 *      and All Locations Reachable off, with no pin reported (the rung asked for
 *      it) and the frozen record at rung 1 (M6a); at "All Locations" (rung 3)
 *      OoT is Glitchless with All Locations Reachable on, an authored OoT No
 *      Logic is overridden and reported (M6b); and neither the generation nor
 *      the read-only Logic / All Locations Reachable rows' callbacks write
 *      either key (M6c). Rung 3's own fill outcome is printed, not asserted:
 *      the All Locations proof is its own lane.
 * Every check runs and reports, and the row fails if any did, so one red run
 * shows every leg a change breaks.
 *
 * RED HALVES, run before landing: without playthrough.cpp's #973 block, M1 and
 * M2 fail under both modes and M3 fails over the "Completed" world (both checks
 * are reached before any mask is assumed); without the two settings.cpp
 * callbacks, or with Mask Quest's default left at "Vanilla", M4 fails. M5's red
 * half is derived, not run (#1350's lane had no local build): without
 * playthrough.cpp's #1350 block the world keeps No Logic, so M5a and M5c fail,
 * and M5d's generated round IS the No Logic round, which the row's own RED half
 * shows reaching the Skull Mask check with nothing assumed.
 */

#include <cstdio>
#include <vector>

#include <libultraship/bridge.h>

#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/settings.h"

#include "combo_logic.h"      // Combo_Logic_GetEngine, the engine vtable
#include "combo_single_bag.h" // OoT_ComboLogic_ExportPool
#include "cvar_shared_keys.h" // RSBS_CVAR_COMBO_RANDO_LOGIC: [Both Games] Logic (#1367)
#include "foreign_items.h"    // RSBS_COMBO_RUNG_*, gComboCtx

// games/oot/soh/Enhancements/randomizer/3drando/menu.cpp: the headless generation
// bridge the RandoGen rows use.
extern "C" int Rando_HeadlessSeedTest(const char* seedStr);
// games/oot/soh/Enhancements/randomizer/3drando/playthrough.cpp (#1350): 1 when the
// last generation generated an authored non-Glitchless OoT Logic as Glitchless.
extern "C" int OoT_Rando_PairedLogicRulesPinned(void);

namespace {

int sFailures = 0;

/** A check that reports and counts, so every leg runs. */
#define MQP_CHECK(cond, msg)                                              \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            sFailures++;                                                  \
        }                                                                 \
    } while (0)

/** A precondition: without it the legs after it would prove nothing. */
#define MQP_REQUIRE(cond, msg)                                            \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            sFailures++;                                                  \
            return;                                                       \
        }                                                                 \
    } while (0)

const char* const kMaskQuestCVar = CVAR_RANDOMIZER_SETTING("CompleteMaskQuest");
const char* const kMaskShopHintCVar = CVAR_RANDOMIZER_SETTING("MaskShopHint");
const char* const kLogicRulesCVar = CVAR_RANDOMIZER_SETTING("LogicRules");
const char* const kAlrCVar = CVAR_RANDOMIZER_SETTING("AllLocationsReachable");

const RandomizerGet kMasks[8] = {
    RG_KEATON_MASK, RG_SKULL_MASK, RG_SPOOKY_MASK, RG_BUNNY_HOOD,
    RG_GORON_MASK,  RG_ZORA_MASK,  RG_GERUDO_MASK, RG_MASK_OF_TRUTH,
};

bool MqpIsRealItem(RandomizerGet rg) {
    return rg > RG_NONE && rg < RG_MAX && Rando::StaticData::RetrieveItem(rg).GetRandomizerGet() == rg;
}

/** The general-pass pool the single bag takes (export source 0). */
std::vector<uint16_t> MqpPoolRows() {
    const int total = OoT_ComboLogic_ExportPool(0, nullptr, nullptr, nullptr, 0);
    std::vector<uint16_t> rows(total > 0 ? (size_t)total : 0);
    if (total > 0) {
        OoT_ComboLogic_ExportPool(0, rows.data(), nullptr, nullptr, total);
    }
    return rows;
}

/** Copies of `rg` in the world: in the pool, or at a location already. */
int MqpCopiesInWorld(RandomizerGet rg, const std::vector<uint16_t>& pool) {
    int copies = 0;
    for (const uint16_t row : pool) {
        copies += (row == (uint16_t)rg) ? 1 : 0;
    }
    auto ctx = Rando::Context::GetInstance();
    for (const RandomizerCheck rc : ctx->allLocations) {
        copies += (ctx->GetItemLocation(rc)->GetPlacedRandomizerGet() == rg) ? 1 : 0;
    }
    return copies;
}

void MqpExpandToFixpoint(const ComboLogicEngine* e) {
    for (int i = 0; i < 16 && e->expand(e->self) != 0; i++) {}
}

/** M1 and M2 over one generation authored with `authoredMode`. Returns false
 *  when there is no world to check the later legs against. */
bool MqpGenerateAndCheck(uint8_t authoredMode, const char* modeName) {
    CVarSetInteger(kMaskQuestCVar, authoredMode);
    CVarSetInteger(kMaskShopHintCVar, 1);
    const int genRc = Rando_HeadlessSeedTest("RSBSMASKQUEST973");
    CVarClear(kMaskQuestCVar);
    CVarClear(kMaskShopHintCVar);
    auto ctx = Rando::Context::GetInstance();
    if (genRc != 0 || ctx == nullptr) {
        printf("[TEST] FAIL: headless paired generation (authored %s) failed, rc=%d (%s:%d)\n", modeName, genRc,
               __FILE__, __LINE__);
        sFailures++;
        return false;
    }

    // ---- M1: the generated settings ----------------------------------------
    const int mode = (int)ctx->GetOption(RSK_MASK_QUEST).Get();
    const int hint = (int)ctx->GetOption(RSK_MASK_SHOP_HINT).Get();
    printf("[TEST] M1 (authored %s, Mask Shop Hint on): generated Mask Quest %d, Mask Shop Hint %d\n", modeName, mode,
           hint);
    MQP_CHECK(mode == RO_MASK_QUEST_SHUFFLE,
              "M1: a paired world kept a Mask Quest that needs the Happy Mask Shop, whose door leads to Termina");
    MQP_CHECK(hint == RO_GENERIC_OFF,
              "M1: a paired world kept the Mask Shop Hint, whose sign is inside the Happy Mask Shop");

    // ---- M2: the masks are in the world ------------------------------------
    const std::vector<uint16_t> pool = MqpPoolRows();
    if (pool.empty()) {
        printf("[TEST] FAIL: M2: the general-pass pool is empty, so the pool leg would be vacuous (%s:%d)\n", __FILE__,
               __LINE__);
        sFailures++;
        return true;
    }
    int missing = 0;
    for (const RandomizerGet mask : kMasks) {
        if (MqpCopiesInWorld(mask, pool) < 1) {
            printf("[TEST] FAIL: M2 (authored %s): %s is in neither the pool (%d rows) nor at any location (%s:%d)\n",
                   modeName, Rando::StaticData::RetrieveItem(mask).GetName().GetEnglish().c_str(), (int)pool.size(),
                   __FILE__, __LINE__);
            missing++;
        }
    }
    sFailures += missing;
    if (missing == 0) {
        printf("[TEST] M2 (authored %s): all eight OoT masks are in the world (pool %d rows)\n", modeName,
               (int)pool.size());
    }
    return true;
}

/** M3 over the world the last generation left. */
void MqpCheckTheaterNeedsTheMasks() {
    auto ctx = Rando::Context::GetInstance();
    const ComboLogicEngine* e = Combo_Logic_GetEngine(GAME_OOT);
    MQP_REQUIRE(e != nullptr, "M3: no OoT combo-logic engine is registered");

    // Everything in the world but the two theater masks: the pool rows and every
    // placed item. Placed items are also withheld, so the round's inventory is
    // exactly this set plus the starting inventory, whatever the fill put where.
    std::vector<uint16_t> assumed = MqpPoolRows();
    std::vector<RandomizerCheck> placedHosts;
    for (const RandomizerCheck rc : ctx->allLocations) {
        const RandomizerGet placed = ctx->GetItemLocation(rc)->GetPlacedRandomizerGet();
        if (placed == RG_NONE) {
            continue;
        }
        placedHosts.push_back(rc);
        assumed.push_back((uint16_t)placed);
    }
    if (e->beginQuery(e->self) == 0) {
        // combo_logic.h: endQuery is still called when beginQuery refuses.
        e->endQuery(e->self);
        printf("[TEST] FAIL: M3: the OoT engine refused to open a round (%s:%d)\n", __FILE__, __LINE__);
        sFailures++;
        return;
    }
    for (const RandomizerCheck rc : placedHosts) {
        e->withholdHost(e->self, (uint16_t)rc);
    }
    int assumedCount = 0;
    for (const uint16_t id : assumed) {
        if (id == (uint16_t)RG_SKULL_MASK || id == (uint16_t)RG_MASK_OF_TRUTH || !MqpIsRealItem((RandomizerGet)id)) {
            continue;
        }
        e->assumeOwnItem(e->self, id);
        assumedCount++;
    }
    MqpExpandToFixpoint(e);
    const bool theater = RegionTable(RR_DEKU_THEATER)->Child();
    const bool skullBefore = e->checkReached(e->self, (uint16_t)RC_DEKU_THEATER_SKULL_MASK) != 0;
    const bool truthBefore = e->checkReached(e->self, (uint16_t)RC_DEKU_THEATER_MASK_OF_TRUTH) != 0;
    printf("[TEST] M3: %d items assumed, %d hosts withheld: Deku Theater reached %d, Skull Mask check %d, Mask of "
           "Truth check %d\n",
           assumedCount, (int)placedHosts.size(), theater ? 1 : 0, skullBefore ? 1 : 0, truthBefore ? 1 : 0);

    e->assumeOwnItem(e->self, (uint16_t)RG_SKULL_MASK);
    MqpExpandToFixpoint(e);
    const bool skullWithSkull = e->checkReached(e->self, (uint16_t)RC_DEKU_THEATER_SKULL_MASK) != 0;
    const bool truthWithSkull = e->checkReached(e->self, (uint16_t)RC_DEKU_THEATER_MASK_OF_TRUTH) != 0;
    printf("[TEST] M3: + Skull Mask: Skull Mask check %d, Mask of Truth check %d\n", skullWithSkull ? 1 : 0,
           truthWithSkull ? 1 : 0);

    e->assumeOwnItem(e->self, (uint16_t)RG_MASK_OF_TRUTH);
    MqpExpandToFixpoint(e);
    const bool truthWithBoth = e->checkReached(e->self, (uint16_t)RC_DEKU_THEATER_MASK_OF_TRUTH) != 0;
    printf("[TEST] M3: + Mask of Truth: Mask of Truth check %d\n", truthWithBoth ? 1 : 0);
    e->endQuery(e->self);

    MQP_REQUIRE(theater, "M3: the Deku Theater itself is not reached with everything but its masks, so the mask "
                         "legs would prove nothing");
    MQP_CHECK(!skullBefore, "M3: the Skull Mask check is reached before the Skull Mask is assumed - the logic credits "
                            "a mask the Happy Mask Shop would lend, and its room never loads");
    MQP_CHECK(!truthBefore, "M3: the Mask of Truth check is reached before the Mask of Truth is assumed - the logic "
                            "credits a mask the Happy Mask Shop would lend, and its room never loads");
    MQP_CHECK(skullWithSkull, "M3: the Skull Mask did not open the Skull Mask check");
    MQP_CHECK(!truthWithSkull, "M3: the Skull Mask opened the Mask of Truth check");
    MQP_CHECK(truthWithBoth, "M3: the Mask of Truth did not open the Mask of Truth check");
}

/** What one M5 round over the OoT engine reached. */
struct MqpRound {
    bool skull = false;
    bool goal = false;
    bool crossing = false;
    int reached = 0;
};

/** One round over the registered OoT engine on the world the last generation left,
 *  with NOTHING assumed (the starting inventory alone) and every placed host
 *  withheld but the Triforce's, so the goal is reached exactly when Ganon is.
 *  Returns false when the engine refused the round. */
bool MqpRoundWithNothing(const ComboLogicEngine* e, MqpRound* out) {
    auto ctx = Rando::Context::GetInstance();
    if (e->beginQuery(e->self) == 0) {
        // combo_logic.h: endQuery is still called when beginQuery refuses.
        e->endQuery(e->self);
        return false;
    }
    for (const RandomizerCheck rc : ctx->allLocations) {
        const RandomizerGet placed = ctx->GetItemLocation(rc)->GetPlacedRandomizerGet();
        if (placed != RG_NONE && placed != RG_TRIFORCE) {
            e->withholdHost(e->self, (uint16_t)rc);
        }
    }
    MqpExpandToFixpoint(e);
    out->skull = e->checkReached(e->self, (uint16_t)RC_DEKU_THEATER_SKULL_MASK) != 0;
    out->goal = e->goalReached(e->self) != 0;
    out->crossing = e->crossingOpen(e->self) != 0;
    out->reached = 0;
    for (const RandomizerCheck rc : ctx->allLocations) {
        out->reached += (e->checkReached(e->self, (uint16_t)rc) != 0) ? 1 : 0;
    }
    e->endQuery(e->self);
    return true;
}

/** M5 (#1350): a paired generation authored with OoT's Logic "No Logic". */
void MqpCheckNoLogicPinned() {
    CVarSetInteger(kLogicRulesCVar, RO_LOGIC_NO_LOGIC);
    const int genRc = Rando_HeadlessSeedTest("RSBSNOLOGIC1350");
    // Read before the clear: the pin must move the context's value, never the CVar.
    const bool cvarKept = CVarExists(kLogicRulesCVar) && CVarGetInteger(kLogicRulesCVar, -1) == RO_LOGIC_NO_LOGIC;
    CVarClear(kLogicRulesCVar);
    auto ctx = Rando::Context::GetInstance();
    MQP_REQUIRE(genRc == 0 && ctx != nullptr, "M5: the headless paired generation authored with OoT No Logic failed");

    // ---- M5a-c: the generated rules, the CVar, the toast's flag ----------------
    const int rules = (int)ctx->GetOption(RSK_LOGIC_RULES).Get();
    const int pinned = OoT_Rando_PairedLogicRulesPinned();
    printf("[TEST] M5 (authored No Logic): generated Logic %d, the CVar still holds No Logic %d, pin reported %d\n",
           rules, cvarKept ? 1 : 0, pinned);
    MQP_CHECK(rules == RO_LOGIC_GLITCHLESS,
              "M5a: a paired world kept OoT's No Logic, under which every OoT condition is true and the pair's proof "
              "of OoT's half is vacuous (#1350)");
    MQP_CHECK(cvarKept, "M5b: the generation rewrote the player's Logic CVar; only the generated value may move");
    MQP_CHECK(pinned == 1, "M5c: the generation did not report the pin, so Generate raises no toast for it");

    // ---- M5d: the OoT engine the pair proves with is not vacuous ---------------
    const ComboLogicEngine* e = Combo_Logic_GetEngine(GAME_OOT);
    MQP_REQUIRE(e != nullptr, "M5d: no OoT combo-logic engine is registered");
    MqpRound generated;
    MQP_REQUIRE(MqpRoundWithNothing(e, &generated), "M5d: the OoT engine refused to open a round");
    // The RED half, in the row: the SAME world and the SAME round with the
    // context's rules at No Logic, which is the world main generated here. If this
    // stops reaching the Skull Mask check with nothing, No Logic is no longer
    // vacuous and the pin wants re-deriving, rather than this row passing on an
    // empty difference.
    const uint8_t savedRules = ctx->GetOption(RSK_LOGIC_RULES).Get();
    ctx->GetOption(RSK_LOGIC_RULES).Set(RO_LOGIC_NO_LOGIC);
    MqpRound noLogic;
    const bool noLogicOpened = MqpRoundWithNothing(e, &noLogic);
    ctx->GetOption(RSK_LOGIC_RULES).Set(savedRules);
    printf(
        "[TEST] M5d: nothing assumed, %d locations: generated rules reach %d (Skull Mask check %d, goal %d, crossing "
        "%d); the same world under No Logic reaches %d (Skull Mask check %d, goal %d, crossing %d)\n",
        (int)ctx->allLocations.size(), generated.reached, generated.skull ? 1 : 0, generated.goal ? 1 : 0,
        generated.crossing ? 1 : 0, noLogic.reached, noLogic.skull ? 1 : 0, noLogic.goal ? 1 : 0,
        noLogic.crossing ? 1 : 0);
    MQP_REQUIRE(noLogicOpened, "M5d: the OoT engine refused to open the No Logic round");
    MQP_REQUIRE(noLogic.skull, "M5d: under No Logic the round with nothing no longer reaches the Skull Mask check, so "
                               "the vacuity this pin answers is gone: re-derive #1350 against upstream's "
                               "ConditionsMet");
    MQP_CHECK(!generated.skull, "M5d: the Skull Mask check is reached with nothing assumed: the generated world's OoT "
                                "logic answers true without the Skull Mask (#1350)");
    MQP_CHECK(!generated.goal, "M5d: OoT's goal is reached with nothing assumed: the pair would stamp OoT's half "
                               "proved over a vacuous logic (#1350)");
    MQP_CHECK(generated.reached < noLogic.reached && generated.reached < (int)ctx->allLocations.size(),
              "M5d: the generated world's OoT logic reaches as much with nothing assumed as No Logic does (#1350)");
}

/** M6 (#1367): OoT's Logic and All Locations Reachable derive from [Both Games] Logic. */
void MqpCheckComboLogicDerived() {
    auto ctx = Rando::Context::GetInstance();

    // ---- M6a: No Logic (rung 1), nothing authored on OoT's own rows ---------
    CVarSetInteger(RSBS_CVAR_COMBO_RANDO_LOGIC, (int32_t)RSBS_COMBO_RUNG_NONE);
    const int noLogicRc = Rando_HeadlessSeedTest("RSBSCOMBOLOGIC1367");
    const int noLogicRules = ctx != nullptr ? (int)ctx->GetOption(RSK_LOGIC_RULES).Get() : -1;
    const int noLogicAlr = ctx != nullptr ? (int)ctx->GetOption(RSK_ALL_LOCATIONS_REACHABLE).Get() : -1;
    const int noLogicPinned = OoT_Rando_PairedLogicRulesPinned();
    const int noLogicRung = (int)gComboCtx.comboSettings.logicRung;
    const bool noLogicStored = CVarExists(kLogicRulesCVar) || CVarExists(kAlrCVar);
    CVarClear(RSBS_CVAR_COMBO_RANDO_LOGIC);
    printf("[TEST] M6a ([Both Games] Logic No Logic): generation rc %d, OoT Logic %d, All Locations Reachable %d, "
           "pin reported %d, frozen rung %d, OoT keys written %d\n",
           noLogicRc, noLogicRules, noLogicAlr, noLogicPinned, noLogicRung, noLogicStored ? 1 : 0);
    MQP_CHECK(noLogicRc == 0, "M6a: the paired generation at [Both Games] Logic \"No Logic\" failed");
    MQP_CHECK(noLogicRules == RO_LOGIC_NO_LOGIC,
              "M6a: [Both Games] Logic \"No Logic\" did not generate OoT's Logic as No Logic (#1367)");
    MQP_CHECK(noLogicAlr == RO_GENERIC_OFF,
              "M6a: [Both Games] Logic \"No Logic\" did not generate All Locations Reachable off (#1367)");
    MQP_CHECK(noLogicPinned == 0, "M6a: the No Logic the rung asked for was reported as a pin (a false toast)");
    MQP_CHECK(noLogicRung == (int)RSBS_COMBO_RUNG_NONE, "M6a: the paired record did not freeze rung No Logic");
    MQP_CHECK(!noLogicStored, "M6a: the generation wrote OoT's Logic or All Locations Reachable key");

    // ---- M6b: All Locations (rung 3) over an authored OoT No Logic, ALR off --
    CVarSetInteger(RSBS_CVAR_COMBO_RANDO_LOGIC, (int32_t)RSBS_COMBO_RUNG_ALL_REACHABLE);
    CVarSetInteger(kLogicRulesCVar, RO_LOGIC_NO_LOGIC);
    CVarSetInteger(kAlrCVar, RO_GENERIC_OFF);
    const int allRc = Rando_HeadlessSeedTest("RSBSCOMBOLOGIC1367");
    const int allRules = ctx != nullptr ? (int)ctx->GetOption(RSK_LOGIC_RULES).Get() : -1;
    const int allAlr = ctx != nullptr ? (int)ctx->GetOption(RSK_ALL_LOCATIONS_REACHABLE).Get() : -1;
    const int allPinned = OoT_Rando_PairedLogicRulesPinned();
    // Read before the clears: the derivation moves the context's values, never the keys.
    const bool allKept =
        CVarGetInteger(kLogicRulesCVar, -1) == RO_LOGIC_NO_LOGIC && CVarGetInteger(kAlrCVar, -1) == RO_GENERIC_OFF;
    CVarClear(RSBS_CVAR_COMBO_RANDO_LOGIC);
    CVarClear(kLogicRulesCVar);
    CVarClear(kAlrCVar);
    printf("[TEST] M6b ([Both Games] Logic All Locations, OoT authored No Logic / ALR off): generation rc %d (not "
           "asserted), OoT Logic %d, All Locations Reachable %d, pin reported %d, authored keys kept %d\n",
           allRc, allRules, allAlr, allPinned, allKept ? 1 : 0);
    MQP_CHECK(allRules == RO_LOGIC_GLITCHLESS,
              "M6b: [Both Games] Logic \"All Locations\" did not generate OoT's Logic as Glitchless (#1367)");
    MQP_CHECK(allAlr == RO_GENERIC_ON,
              "M6b: [Both Games] Logic \"All Locations\" did not generate All Locations Reachable on (#1367)");
    MQP_CHECK(allPinned == 1, "M6b: the authored OoT No Logic a proving rung overrode was not reported");
    MQP_CHECK(allKept, "M6b: the generation rewrote an authored OoT key; only the generated values may move");

    // ---- M6c: the read-only rows' callbacks write nothing ---------------------
    auto settings = Rando::Settings::GetInstance();
    settings->GetOption(RSK_LOGIC_RULES).RunCallback();
    settings->GetOption(RSK_ALL_LOCATIONS_REACHABLE).RunCallback();
    const bool rowsStored = CVarExists(kLogicRulesCVar) || CVarExists(kAlrCVar);
    printf("[TEST] M6c: the read-only Logic / All Locations Reachable rows' callbacks stored a key %d\n",
           rowsStored ? 1 : 0);
    MQP_CHECK(!rowsStored, "M6c: a read-only Logic row's callback stored a randomizer setting the player never set");
    CVarClear(kLogicRulesCVar);
    CVarClear(kAlrCVar);
}

/** M4: the menu rows show the generated value, and the callbacks never store a
 *  setting (a stored one would take a config off the shipped defaults, which the
 *  integration rows refuse). */
void MqpCheckMenuRows() {
    auto settings = Rando::Settings::GetInstance();
    Rando::Option& maskQuest = settings->GetOption(RSK_MASK_QUEST);
    Rando::Option& maskShopHint = settings->GetOption(RSK_MASK_SHOP_HINT);

    // A preset's (or an old config's) stored values are cleared to the defaults.
    CVarSetInteger(kMaskQuestCVar, RO_MASK_QUEST_COMPLETED);
    CVarSetInteger(kMaskShopHintCVar, 1);
    maskQuest.RunCallback();
    maskShopHint.RunCallback();
    const bool storedMode = CVarExists(kMaskQuestCVar);
    const bool storedHint = CVarExists(kMaskShopHintCVar);
    const int shownMode = maskQuest.GetOptionIndex();
    const int shownHint = maskShopHint.GetOptionIndex();
    CVarClear(kMaskQuestCVar);
    CVarClear(kMaskShopHintCVar);
    printf("[TEST] M4: stored Completed / hint on -> stored afterwards %d / %d, the rows show Mask Quest %d, Mask Shop "
           "Hint %d\n",
           storedMode ? 1 : 0, storedHint ? 1 : 0, shownMode, shownHint);
    MQP_CHECK(shownMode == RO_MASK_QUEST_SHUFFLE, "M4: the Mask Quest row does not show \"Shuffle\"");
    MQP_CHECK(shownHint == RO_GENERIC_OFF, "M4: the Mask Shop Hint row does not show off");
    MQP_CHECK(!storedMode && !storedHint, "M4: a stored Mask Quest / Mask Shop Hint value was not cleared");

    // Nothing stored: the callbacks store nothing, and the rows still show the
    // generated values (they are the defaults).
    maskQuest.RunCallback();
    maskShopHint.RunCallback();
    const bool createdMode = CVarExists(kMaskQuestCVar);
    const bool createdHint = CVarExists(kMaskShopHintCVar);
    printf("[TEST] M4: nothing stored -> stored afterwards %d / %d, the rows show Mask Quest %d, Mask Shop Hint %d\n",
           createdMode ? 1 : 0, createdHint ? 1 : 0, (int)maskQuest.GetOptionIndex(),
           (int)maskShopHint.GetOptionIndex());
    MQP_CHECK(!createdMode && !createdHint, "M4: a menu callback stored a randomizer setting the player never set");
    MQP_CHECK(maskQuest.GetOptionIndex() == RO_MASK_QUEST_SHUFFLE && maskShopHint.GetOptionIndex() == RO_GENERIC_OFF,
              "M4: with nothing stored the rows do not show \"Shuffle\" / off");
    CVarClear(kMaskQuestCVar);
    CVarClear(kMaskShopHintCVar);
}

} // namespace

extern "C" int OoT_MaskQuestPaired_RunGenerated(void) {
    printf("[TEST] rando-mask-quest-paired (OoT): a paired world shuffles the Happy Mask Shop's masks and drops its "
           "sign hint, because the shop's door leads to Termina (#973)\n");
    sFailures = 0;
    MqpGenerateAndCheck(RO_MASK_QUEST_VANILLA, "Vanilla");
    if (MqpGenerateAndCheck(RO_MASK_QUEST_COMPLETED, "Completed")) {
        MqpCheckTheaterNeedsTheMasks();
    }
    MqpCheckNoLogicPinned();
    MqpCheckComboLogicDerived();
    MqpCheckMenuRows();
    if (sFailures != 0) {
        printf("[TEST] FAIL: rando-mask-quest-paired (OoT): %d check(s) failed\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: rando-mask-quest-paired (OoT)\n");
    return 0;
}
