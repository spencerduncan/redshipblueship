/**
 * @file oot_tracker_reachability_test.cpp
 * OoT's half of the tracker-reachability rows (#994; audit
 * docs/tracker-reachability-audit-2026-10-18.md, R4). Nothing else in the tree
 * runs RecalculateAvailableChecks or reads ItemLocation::IsAvailable, so the OoT
 * Check Tracker's tracker-mode search (ReachabilitySearch with
 * calculatingAvailableChecks = true) had no coverage at all.
 *
 * TWO ENTRY POINTS, each over a REAL paired creation: a headless generation,
 * then the file select's own order (OoT_Sram_InitSave's new file, the quest id,
 * then OoT_Creation_AuthorRandoFile: the creation event and
 * Randomizer_InitSaveFile), so the live save the tracker reads is the one a
 * player's new paired file starts with.
 *
 * OoT_TrackerAvailability_RunArrival (row TrackerAvailabilityArrival):
 *   authored Mask Quest "Completed" with the Mask Shop Hint on (the playtest
 *   seed's values, which a paired creation overrides, #973), the Kakariko gate
 *   open and the forest open, so a child reaches the Market, the Mask Shop room
 *   and the Deku Theater with nothing found. The player holds Zelda's Letter.
 *   A1 sphere zero: Mido's top-left chest is available.
 *   A2 a gated check is not: the Lost Woods target needs the slingshot.
 *   A3 #973: the Deku Theater is reached and the "Completed" prerequisites hold
 *      (the letter, the Kakariko gate), yet neither Deku Theater mask check is
 *      available, and no check in the Mask Shop room is counted.
 *   A4 the arrival: an OoT slingshot found in Termina waits; the arrival flag is
 *      set; the real OoT_Play_ApplyArrivalShared (apply, redeem, tracker arm)
 *      runs; the slingshot is in the inventory and the deferred flag is armed.
 *   A5 the set is stale until the draw consumes the flag (event-driven, which
 *      is why A4's arm is load-bearing).
 *   A6 the drain recalculates: the target flips to available; Mido's chest stays
 *      available; the A3 checks stay out.
 *
 * OoT_TrackerFillSuperset_RunOoT (row TrackerFillSuperset, OoT half): over the
 * shipped defaults (the ComboSingleBag pinned seed), the tracker's available set
 * against the fill engine's sphere-zero reached set on the same paired file
 * (every placed host withheld, nothing assumed): tracker-available must contain
 * fill-reached minus obtained. The other direction is printed, not asserted:
 * the tracker reads the live save (Link's Pocket and every creation-time give
 * included), the round starts from the settings, so the tracker may see more.
 *
 * OoT_TrackerOoTPanelList_Run (row TrackerOoTPanelList, #1126): over the same
 * pinned seed's paired file, with one placed chest excluded on the heap and SoH's
 * Check Tracker loaded as its OnLoadGame hook loads it, the combo tracker's OoT
 * panel lists what SoH's tracker lists: its summary counts GetTotalChecks()
 * checks (more than none), a shuffled chest is listed, and the excluded chest and
 * an unshuffled shop slot, both holding a placed item, are not. While MM is
 * played, the stale panel keeps that list.
 *
 * Every check runs and reports; the entry point returns the failure count.
 */

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <set>
#include <vector>

#include <libultraship/bridge.h>

#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/randomizer_check_tracker.h"

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"

#include "combo_logic.h"        // src/common: Combo_Logic_GetEngine, the engine vtable
#include "combo_tracker_view.h" // src/common: the combo tracker's OoT panel (#1126)
#include "foreign_items.h"      // src/common: Combo_ForeignPairingActive
#include "shared_items.h"       // src/common: the crossing record and its redeem

extern "C" {
// 3drando/menu.cpp: the headless generation bridge the RandoGen rows use.
int Rando_HeadlessSeedTest(const char* seedStr);
// ItemAdapterSingleExe.cpp: the new file Ship of Harkinian's own init path
// authors (variant 0: Save_InitFile(false), what OoT_Sram_InitNewSave runs).
int OoT_ItemAdapter_TestAuthorNewFile(void* buf, size_t size, int variant, int* outNewfMarked);
// ForeignItemsSingleExe.cpp: the creation event, then Randomizer_InitSaveFile;
// and the #1055 bridge that removes a stale spoiler of this seed first.
int OoT_Creation_AuthorRandoFile(int slot);
int OoT_Creation_TestClearPairedSpoiler(void);
// z_play.c: what OoT_Play_Init runs at a cross-game arrival (apply, redeem,
// tracker arm; #990).
void OoT_Play_ApplyArrivalShared(void);
// src/common/entrance.h (not included: the name collides with the randomizer's).
void Combo_SetStartupEntrance(uint16_t entrance);
void Combo_ClearStartupEntrance(void);
bool Combo_HasStartupEntranceForGame(const char* gameId);
// src/common/context.h (likewise kept out of this TU's include set).
void Context_ClearFrozenState(GameId game);
// randomizer_check_tracker.cpp: the deferred flag, the option's handler, and the
// draw's drain of the flag without its play-state gate (#990, #994).
int RandoTest_CheckTrackerRecalcPending(void);
void RandoTest_CheckTrackerClearRecalcPending(void);
void RandoTest_CheckTrackerSetEnableAvailable(int enabled);
int RandoTest_CheckTrackerDrainPendingRecalc(void);
// randomizer_check_tracker.cpp: CheckTrackerLoadGame, as its OnLoadGame hook runs it (#1126).
void RandoTest_CheckTrackerLoadGame(int32_t fileNum);
s32 Flags_GetRandomizerInf(RandomizerInf flag);
void Flags_SetRandomizerInf(RandomizerInf flag);
}

namespace {

int sFailures = 0;

#define TRR_CHECK(cond, msg)                                              \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            sFailures++;                                                  \
        }                                                                 \
    } while (0)

/** A precondition: without it every leg after it would prove nothing. */
#define TRR_REQUIRE(cond, msg)                                            \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            sFailures++;                                                  \
            return false;                                                 \
        }                                                                 \
    } while (0)

const char* const kMaskQuestCVar = CVAR_RANDOMIZER_SETTING("CompleteMaskQuest");
const char* const kMaskShopHintCVar = CVAR_RANDOMIZER_SETTING("MaskShopHint");
const char* const kKakarikoGateCVar = CVAR_RANDOMIZER_SETTING("KakarikoGate");
const char* const kClosedForestCVar = CVAR_RANDOMIZER_SETTING("ClosedForest");

const char* TrrName(RandomizerCheck rc) {
    return Rando::StaticData::GetLocation(rc)->GetName().c_str();
}

bool TrrAvailable(RandomizerCheck rc) {
    return Rando::Context::GetInstance()->GetItemLocation(rc)->IsAvailable();
}

bool TrrChildReaches(RandomizerRegion region) {
    const Region* r = RegionTable(region);
    return r != nullptr && r->Child();
}

/**
 * A paired file as the file select creates one: a headless generation of
 * `seed`, the new file Ship of Harkinian authors, the quest id OoT_Sram_InitSave
 * sets, then the creation event and Randomizer_InitSaveFile in their production
 * order. Leaves OoT's live save as a loaded file (GAMEMODE_NORMAL).
 */
bool TrrCreatePairedFile(const char* seed) {
    const int genRc = Rando_HeadlessSeedTest(seed);
    auto ctx = Rando::Context::GetInstance();
    TRR_REQUIRE(genRc == 0 && ctx != nullptr, "the headless paired generation failed");
    // #1055: the build directory is shared by every run of every build, so a
    // spoiler of this seed under another identity must not meet this creation.
    OoT_Creation_TestClearPairedSpoiler();

    auto newFile = std::make_unique<SaveContext>();
    TRR_REQUIRE(OoT_ItemAdapter_TestAuthorNewFile(newFile.get(), sizeof(SaveContext), 0, nullptr) == 1,
                "Ship of Harkinian's new-file init could not author a save (missing OoT bring-up)");
    memcpy(&gSaveContext, newFile.get(), sizeof(SaveContext));
    gSaveContext.fileNum = 0;
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    Context_ClearFrozenState(GAME_MM);
    TRR_REQUIRE(OoT_Creation_AuthorRandoFile(0) == 1, "the production creation event refused the paired file");
    TRR_REQUIRE(Combo_ForeignPairingActive(), "the creation left no paired identity");
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    TRR_REQUIRE(ctx->GetOption(RSK_SELECTED_STARTING_AGE).Is(RO_AGE_CHILD),
                "the world does not start as a child, so every child-only leg below would be vacuous");
    return true;
}

/** Turn Enable Available Checks on with no OoT play state (it arms the flag,
 *  #990) and drain it as the next draw would. */
bool TrrEnableAndRecalc() {
    RandoTest_CheckTrackerClearRecalcPending();
    RandoTest_CheckTrackerSetEnableAvailable(1);
    TRR_REQUIRE(RandoTest_CheckTrackerRecalcPending() == 1,
                "turning Enable Available Checks on with no play state did not schedule a recalculation");
    TRR_REQUIRE(RandoTest_CheckTrackerDrainPendingRecalc() == 1, "the pending recalculation did not run");
    return true;
}

void TrrTrackerOff() {
    RandoTest_CheckTrackerSetEnableAvailable(0);
    RandoTest_CheckTrackerClearRecalcPending();
}

/** A3: the Happy Mask Shop never lends a mask on a paired file (#973). */
void TrrCheckMaskShop(const char* when) {
    auto logic = Rando::Context::GetInstance()->GetLogic();
    const bool theater = TrrChildReaches(RR_DEKU_THEATER);
    const bool shopRoom = TrrChildReaches(RR_MARKET_MASK_SHOP);
    const bool letter = Flags_GetRandomizerInf(RAND_INF_ZELDAS_LETTER) != 0;
    const bool gate = logic->Get(LOGIC_KAKARIKO_GATE_OPEN);
    const bool skull = TrrAvailable(RC_DEKU_THEATER_SKULL_MASK);
    const bool truth = TrrAvailable(RC_DEKU_THEATER_MASK_OF_TRUTH);
    int shopCounted = 0;
    if (const Region* room = RegionTable(RR_MARKET_MASK_SHOP)) {
        for (const LocationAccess& loc : room->locations) {
            const RandomizerCheck rc = loc.GetLocation();
            if (TrrAvailable(rc) && CheckTracker::IsVisibleInCheckTracker(rc)) {
                printf("[TEST] A3 (%s): the Mask Shop room's %s is counted available\n", when, TrrName(rc));
                shopCounted++;
            }
        }
    }
    printf("[TEST] A3 (%s): Deku Theater reached %d, Mask Shop room reached %d, letter %d, Kakariko gate %d -> Skull "
           "Mask check %d, Mask of Truth check %d, Mask Shop room checks counted %d\n",
           when, theater ? 1 : 0, shopRoom ? 1 : 0, letter ? 1 : 0, gate ? 1 : 0, skull ? 1 : 0, truth ? 1 : 0,
           shopCounted);
    TRR_CHECK(theater && shopRoom && letter && gate,
              "A3: the Deku Theater, the Mask Shop room, Zelda's Letter and the open Kakariko gate are not all in "
              "reach, so the mask legs would be vacuous");
    TRR_CHECK(!skull, "A3: the Deku Theater's Skull Mask check is available with no Skull Mask - the tracker credits "
                      "a mask the Happy Mask Shop would lend, and its room is the crossing (#973)");
    TRR_CHECK(!truth, "A3: the Deku Theater's Mask of Truth check is available with no Mask of Truth (#973)");
    TRR_CHECK(shopCounted == 0, "A3: a check inside the Happy Mask Shop is counted available on a paired file");
}

} // namespace

extern "C" int OoT_TrackerAvailability_RunArrival(void) {
    printf("[TEST] tracker-availability-arrival (OoT): the Check Tracker's Available Checks on a paired creation "
           "save, across a cross-game arrival (#994)\n");
    sFailures = 0;

    CVarSetInteger(kMaskQuestCVar, RO_MASK_QUEST_COMPLETED);
    CVarSetInteger(kMaskShopHintCVar, 1);
    CVarSetInteger(kKakarikoGateCVar, RO_KAK_GATE_OPEN);
    CVarSetInteger(kClosedForestCVar, RO_CLOSED_FOREST_OFF);
    const bool created = TrrCreatePairedFile("RSBSTRACKER994");
    CVarClear(kMaskQuestCVar);
    CVarClear(kMaskShopHintCVar);
    CVarClear(kKakarikoGateCVar);
    CVarClear(kClosedForestCVar);
    if (!created) {
        printf("[TEST] FAIL: tracker-availability-arrival (OoT): no paired file to drive\n");
        return sFailures;
    }
    auto ctx = Rando::Context::GetInstance();
    const int maskQuest = (int)ctx->GetOption(RSK_MASK_QUEST).Get();
    printf("[TEST] tracker-availability-arrival: generated Mask Quest %d (authored Completed %d)\n", maskQuest,
           (int)RO_MASK_QUEST_COMPLETED);

    // The player holds Zelda's Letter (the "Completed" mask rule's first half).
    Flags_SetRandomizerInf(RAND_INF_ZELDAS_LETTER);
    const bool slingshotAtCreation = gSaveContext.inventory.items[SLOT_SLINGSHOT] == ITEM_SLINGSHOT;
    TRR_CHECK(!slingshotAtCreation, "the creation save already holds the slingshot, so the arrival legs are vacuous");

    // ---- sphere zero -----------------------------------------------------------------
    if (TrrEnableAndRecalc()) {
        const bool mido = TrrAvailable(RC_KF_MIDOS_TOP_LEFT_CHEST);
        const bool woods = TrrChildReaches(RR_THE_LOST_WOODS);
        const bool target = TrrAvailable(RC_LW_TARGET_IN_WOODS);
        printf("[TEST] A1/A2 (sphere zero): Mido's top-left chest %d; Lost Woods reached %d, target %d\n", mido ? 1 : 0,
               woods ? 1 : 0, target ? 1 : 0);
        TRR_CHECK(mido, "A1: Mido's top-left chest (needs nothing) is not available on the creation save");
        TRR_CHECK(woods, "A2: the Lost Woods is not reached as a child, so the target leg would be vacuous");
        TRR_CHECK(!target, "A2: the Lost Woods target (needs the slingshot) is available with no slingshot");
        TrrCheckMaskShop("sphere zero");
    }

    // ---- the arrival -------------------------------------------------------------------
    Combo_ClearSharedItemOutbox();
    TRR_CHECK(Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/false) == 0,
              "A4: an OoT item was already waiting before the arrival under test");
    TRR_CHECK(Combo_RecordSharedItemCrossing(GAME_OOT, (uint16_t)RG_PROGRESSIVE_SLINGSHOT) >= 0,
              "A4: the slingshot crossing could not be recorded");
    Combo_SetStartupEntrance(0x0100);
    TRR_CHECK(Combo_HasStartupEntranceForGame("oot"), "A4: the OoT arrival flag is not set");
    RandoTest_CheckTrackerClearRecalcPending();

    OoT_Play_ApplyArrivalShared();

    const bool redeemed = Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/false) == 0;
    const bool holdsSlingshot = gSaveContext.inventory.items[SLOT_SLINGSHOT] == ITEM_SLINGSHOT;
    const int armed = RandoTest_CheckTrackerRecalcPending();
    printf("[TEST] A4 (arrival): redeemed %d, slingshot in the inventory %d, deferred recalculation armed %d\n",
           redeemed ? 1 : 0, holdsSlingshot ? 1 : 0, armed);
    TRR_CHECK(redeemed && holdsSlingshot, "A4: the arrival's real redeem did not give the slingshot");
    TRR_CHECK(armed == 1, "A4: the arrival did not schedule the tracker's recalculation (#990)");

    // ---- stale until the draw, then recomputed -------------------------------------------
    const bool staleTarget = TrrAvailable(RC_LW_TARGET_IN_WOODS);
    printf("[TEST] A5 (before the draw): target %d\n", staleTarget ? 1 : 0);
    TRR_CHECK(!staleTarget, "A5: the target flipped before any recalculation ran, so the set is not the "
                            "event-driven one this row is about");

    const int drained = RandoTest_CheckTrackerDrainPendingRecalc();
    const bool target = TrrAvailable(RC_LW_TARGET_IN_WOODS);
    const bool mido = TrrAvailable(RC_KF_MIDOS_TOP_LEFT_CHEST);
    printf("[TEST] A6 (after the draw): drained %d, target %d, Mido's top-left chest %d\n", drained, target ? 1 : 0,
           mido ? 1 : 0);
    TRR_CHECK(drained == 1, "A6: the draw found no pending recalculation to run");
    TRR_CHECK(target, "A6: the slingshot redeemed at the arrival did not make the Lost Woods target available");
    TRR_CHECK(mido, "A6: Mido's top-left chest dropped out after the arrival");
    TrrCheckMaskShop("after the arrival");

    Combo_ClearStartupEntrance();
    Combo_ClearSharedItemOutbox();
    TrrTrackerOff();

    if (sFailures != 0) {
        printf("[TEST] FAIL: tracker-availability-arrival (OoT): %d check(s) failed\n", sFailures);
    } else {
        printf("[TEST] PASS: tracker-availability-arrival (OoT)\n");
    }
    return sFailures;
}

/**
 * The fill-vs-tracker superset, OoT half. Creates the paired file itself (the
 * shipped defaults over the ComboSingleBag pinned seed) and leaves it, and MM's
 * armed shadow, for the MM half.
 */
extern "C" int OoT_TrackerFillSuperset_RunOoT(void) {
    printf("[TEST] tracker-fill-superset (OoT): the Check Tracker's available set contains the fill's sphere-zero "
           "reached set on the same paired file (#994)\n");
    sFailures = 0;
    if (!TrrCreatePairedFile("RSBSSINGLEBAG1")) {
        return sFailures;
    }
    auto ctx = Rando::Context::GetInstance();

    // ---- the tracker, over the live save -----------------------------------------------
    std::set<RandomizerCheck> tracker;
    std::set<RandomizerCheck> obtained;
    if (TrrEnableAndRecalc()) {
        for (const RandomizerCheck rc : ctx->allLocations) {
            if (ctx->GetItemLocation(rc)->HasObtained()) {
                obtained.insert(rc);
            } else if (TrrAvailable(rc)) {
                tracker.insert(rc);
            }
        }
    }
    TrrTrackerOff();

    // ---- the fill's round: nothing assumed, every placed host withheld ----------------
    std::set<RandomizerCheck> fill;
    const ComboLogicEngine* e = Combo_Logic_GetEngine(GAME_OOT);
    TRR_CHECK(e != nullptr && e->withholdHost != nullptr, "no OoT combo-logic engine (with withholdHost) registered");
    if (e != nullptr && e->withholdHost != nullptr) {
        if (e->beginQuery(e->self) != 0) {
            for (const RandomizerCheck rc : ctx->allLocations) {
                if (ctx->GetItemLocation(rc)->GetPlacedRandomizerGet() != RG_NONE) {
                    e->withholdHost(e->self, (uint16_t)rc);
                }
            }
            for (int i = 0; i < 16 && e->expand(e->self) != 0; i++) {}
            for (const RandomizerCheck rc : ctx->allLocations) {
                if (e->checkReached(e->self, (uint16_t)rc) != 0) {
                    fill.insert(rc);
                }
            }
        } else {
            TRR_CHECK(false, "the OoT engine refused to open a round");
        }
        e->endQuery(e->self); // combo_logic.h: called even when beginQuery refused
    }

    int missing = 0;
    for (const RandomizerCheck rc : fill) {
        if (obtained.count(rc) == 0 && tracker.count(rc) == 0) {
            if (missing < 40) {
                printf("[TEST] superset (OoT): the fill reaches %s in sphere zero, the tracker does not show it "
                       "available\n",
                       TrrName(rc));
            }
            missing++;
        }
    }
    int trackerOnly = 0;
    for (const RandomizerCheck rc : tracker) {
        if (fill.count(rc) == 0) {
            if (trackerOnly < 10) {
                printf("[TEST] superset (OoT, other direction, informational): the tracker shows %s, the fill's "
                       "sphere zero does not reach it\n",
                       TrrName(rc));
            }
            trackerOnly++;
        }
    }
    printf("[TEST] superset (OoT): fill reached %d, tracker available %d, obtained %d; fill-only %d, tracker-only %d\n",
           (int)fill.size(), (int)tracker.size(), (int)obtained.size(), missing, trackerOnly);
    TRR_CHECK(fill.count(RC_KF_MIDOS_TOP_LEFT_CHEST) != 0 && fill.size() >= 10,
              "the fill's sphere zero reached almost nothing, so the superset would be vacuous");
    TRR_CHECK(missing == 0, "the fill's sphere zero reaches checks the Check Tracker does not show available on the "
                            "same paired file (the #973 class: a proof crediting what the save cannot do)");
    if (sFailures == 0) {
        printf("[TEST] PASS: tracker-fill-superset (OoT)\n");
    }
    return sFailures;
}

/**
 * The combo tracker's OoT panel against SoH's Check Tracker (#1126). The pinned
 * seed's paired file; one placed chest excluded on the heap (what a player's
 * excluded location is to the tracker: ItemLocation::IsExcluded); SoH's tracker
 * loaded as its OnLoadGame hook loads it. The panel's summary must count exactly
 * GetTotalChecks() checks, and the rows must agree on three named checks.
 */
extern "C" int OoT_TrackerOoTPanelList_Run(void) {
    printf("[TEST] tracker-oot-panel-list: the combo tracker's OoT panel lists what SoH's Check Tracker lists on a "
           "paired creation save (#1126)\n");
    sFailures = 0;
    if (!TrrCreatePairedFile("RSBSSINGLEBAG1")) {
        return sFailures;
    }
    auto ctx = Rando::Context::GetInstance();

    const RandomizerCheck kListed = RC_KF_MIDOS_TOP_LEFT_CHEST;
    const RandomizerCheck kExcluded = RC_KF_MIDOS_BOTTOM_RIGHT_CHEST;
    Rando::ItemLocation* excluded = ctx->GetItemLocation(kExcluded);
    TRR_CHECK(excluded->GetPlacedRandomizerGet() != RG_NONE && !excluded->IsExcluded(),
              "Mido's bottom-right chest holds no item or is already excluded, so the excluded leg is vacuous");
    excluded->SetExcludedOption(RO_LOCATION_EXCLUDE);

    // SoH's Check Tracker loads. The ROM-free tier never registers its tracker-data
    // section, so the load's saves would land on the base section: 0xFE is a slot
    // number SaveSection refuses outright.
    const int32_t fileNum = gSaveContext.fileNum;
    gSaveContext.fileNum = 0xFE;
    RandoTest_CheckTrackerLoadGame(0);
    gSaveContext.fileNum = fileNum;
    const int total = (int)CheckTracker::GetTotalChecks();

    // An unshuffled shop slot that holds an item: SoH lists it only when its
    // shops are shown and Hide Unshuffled Shop Item Checks is off.
    RandomizerCheck shopSlot = RC_UNKNOWN_CHECK;
    for (const RandomizerCheck rc : ctx->allLocations) {
        if (Rando::StaticData::GetLocation(rc)->GetRCType() == RCTYPE_SHOP &&
            ctx->GetItemLocation(rc)->GetPlacedRandomizerGet() != RG_NONE && !CheckTracker::IsCheckShuffled(rc) &&
            !CheckTracker::IsVisibleInCheckTracker(rc)) {
            shopSlot = rc;
            break;
        }
    }
    int placed = 0;
    for (const RandomizerCheck rc : ctx->allLocations) {
        if (ctx->GetItemLocation(rc)->GetPlacedRandomizerGet() != RG_NONE) {
            placed++;
        }
    }

    OoT_TrackerAdapter_Register();
    const GameId gameBefore = Context_GetCurrentGame();
    Context_SetCurrentGame(GAME_OOT);
    ComboTrackerGameSummary live;
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &live);
    ComboTrackerCheckRow listedRow = {};
    ComboTrackerCheckRow excludedRow = {};
    ComboTrackerCheckRow shopRow = {};
    const bool haveListed = Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)kListed, &listedRow);
    const bool haveExcluded = Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)kExcluded, &excludedRow);
    const bool haveShop =
        shopSlot != RC_UNKNOWN_CHECK && Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)shopSlot, &shopRow);
    printf("[TEST] tracker-oot-panel-list (OoT played): panel shuffled %d obtained %d skipped %d; SoH's tracker "
           "total %d; placed %d; %s listed %d; %s (excluded) listed %d; %s listed %d\n",
           live.shuffled, live.obtained, live.skipped, total, placed, TrrName(kListed), listedRow.shuffled ? 1 : 0,
           TrrName(kExcluded), excludedRow.shuffled ? 1 : 0,
           shopSlot != RC_UNKNOWN_CHECK ? TrrName(shopSlot) : "(no shop slot)", shopRow.shuffled ? 1 : 0);
    TRR_CHECK(total > 0, "SoH's Check Tracker lists nothing after its load, so the comparison is vacuous");
    TRR_CHECK(live.freshness == COMBO_TRACKER_FRESH_LIVE, "the OoT panel is not LIVE while OoT is played");
    TRR_CHECK(live.shuffled == total, "the OoT panel's check count is not SoH's Check Tracker's total");
    TRR_CHECK(haveListed && listedRow.shuffled && !ctx->GetItemLocation(kListed)->IsExcluded(),
              "Mido's top-left chest (shuffled, not excluded) is not listed");
    TRR_CHECK(haveExcluded && !excludedRow.shuffled, "an excluded location holding a placed item is listed");
    TRR_CHECK(haveShop, "no unshuffled shop slot holding an item was found, so the shop leg is vacuous");
    TRR_CHECK(haveShop && !shopRow.shuffled, "an unshuffled shop slot holding an item is listed");

    // Majora's Mask played: the stale panel keeps the list OoT's tracker gave it.
    Context_SetCurrentGame(GAME_MM);
    ComboTrackerGameSummary stale;
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &stale);
    ComboTrackerCheckRow staleExcluded = {};
    const bool haveStaleExcluded = Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)kExcluded, &staleExcluded);
    printf("[TEST] tracker-oot-panel-list (MM played): freshness %u, panel shuffled %d; excluded listed %d; the OoT "
           "save's quest id sits at offset 0x%zX, MM's save blob is 0x%X bytes\n",
           (unsigned)stale.freshness, stale.shuffled, staleExcluded.shuffled ? 1 : 0,
           offsetof(SaveContext, ship.quest.id), (unsigned)MM_SAVE_CONTEXT_SIZE);
    TRR_CHECK(stale.freshness == COMBO_TRACKER_FRESH_STALE, "the OoT panel is not STALE while MM is played");
    TRR_CHECK(stale.shuffled == total, "the stale OoT panel's count moved off SoH's Check Tracker's total");
    TRR_CHECK(haveStaleExcluded && !staleExcluded.shuffled, "the stale OoT panel lists the excluded location");

    Context_SetCurrentGame(gameBefore);
    excluded->SetExcludedOption(RO_LOCATION_INCLUDE);
    CheckTracker::Teardown();
    if (sFailures == 0) {
        printf("[TEST] PASS: tracker-oot-panel-list\n");
    } else {
        printf("[TEST] FAIL: tracker-oot-panel-list: %d check(s) failed\n", sFailures);
    }
    return sFailures;
}
