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
 *   C  the combo tracker's OoT panel reads the same set (#1129), over Mido's
 *      chest and the target listed through the adapter's list seam: not known
 *      before Enable Available Checks has run a recalculation, one available at
 *      sphere zero, two after the arrival's drain; and the departure capture
 *      OoT_Game_Suspend makes is what the panel shows once MM is played.
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
 * played, the stale panel keeps that list. On a vanilla file, and at the title
 * screen with SoH's tracker still loaded, the panel never asks SoH's predicate
 * (its vanilla branch would list vanilla-completion checks) and lists the
 * placed checks, as before.
 *
 * OoT_TrackerSohFixes_RunPaired (row TrackerOoTPanelList, after the list legs,
 * over the same paired file; #1420, SoH's tracker fixes). The display-free legs
 * are the redship row OoTTrackerSohFixes (oot_tracker_soh_fixes_test.cpp).
 *   F  SoH #6447: with Ganon's Boss Key at Vanilla, the Ganon's Tower boss key
 *      chest is a tracker row exactly when Triforce Hunt is on (Win or Ganon's
 *      Boss Key), the mode a paired Triforce Hunt world generates (#1389).
 *   G  generation is untouched: the OoT combo engine's round (the generator's own
 *      ReachabilitySearch(ctx->allLocations)) reaches the same set whatever
 *      Link's saved entrance, age and night flag; the new age/time parameter is
 *      inert when the search starts at Root (every generation caller); and
 *      RAT_NONE from a region keeps the starting age's day, as before.
 *   S  SoH #6165, #6237, #7300: after SoH's tracker load (which now stamps the
 *      entrances whatever the entrance shuffle), Available Checks start where the
 *      saved entrance index puts Link: a dungeon Root cannot reach at sphere zero
 *      is reached at Link's age and time (child day, child night, adult day, adult
 *      night), nothing Root reaches is lost, and an unknown index is Root again.
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
#include "soh/Enhancements/randomizer/3drando/fill.hpp" // ReachabilitySearch (#1420's G legs)

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
// randomizer_check_tracker.cpp (#1420): LoadSettings alone, and the region a saved entrance index names.
void RandoTest_CheckTrackerLoadSettings(void);
int OoT_CheckTracker_TestStartingRegion(int32_t savedEntrance);
// TrackerAdapterSingleExe.cpp: the adapter's seams (#458 U5, #1126). The skip seam's
// 2 is "a save is loaded, IS_RANDO read for real"; 0 is "no save is loaded".
int OoT_TrackerAdapter_TestSkipSeam(int saveLoaded);
int OoT_TrackerAdapter_TestListSeam(int settingsLoaded, const uint16_t* visible, int count);
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

/**
 * C (#1129): the combo tracker's OoT panel over Mido's top-left chest and the Lost
 * Woods target (the only checks the list seam lists), read under `game`: its
 * summary's available count (-1 when not known) and each row's bit (-1 not known).
 * OoT's live gate is the seams' (a randomizer save with IS_RANDO read for real,
 * SoH's tracker settings loaded); the set is SoH's own, current or not.
 */
int TrrComboPanel(GameId game, int* outMido, int* outTarget) {
    const uint16_t listed[2] = { (uint16_t)RC_KF_MIDOS_TOP_LEFT_CHEST, (uint16_t)RC_LW_TARGET_IN_WOODS };
    (void)OoT_TrackerAdapter_TestListSeam(1, listed, 2);
    (void)OoT_TrackerAdapter_TestSkipSeam(2);
    const GameId prev = Context_GetCurrentGame();
    Context_SetCurrentGame(game);
    ComboTrackerGameSummary summary;
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &summary);
    int* outs[2] = { outMido, outTarget };
    for (int i = 0; i < 2; i++) {
        ComboTrackerCheckRow row;
        const bool got = Combo_TrackerCheckAt((uint8_t)GAME_OOT, (int)listed[i], &row);
        *outs[i] = !got ? -2 : (row.availabilityKnown ? (row.available ? 1 : 0) : -1);
    }
    Context_SetCurrentGame(prev);
    (void)OoT_TrackerAdapter_TestListSeam(-1, nullptr, 0);
    (void)OoT_TrackerAdapter_TestSkipSeam(-1);
    return summary.availabilityKnown ? summary.available : -1;
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

    // ---- C: the combo panel before any recalculation (#1129) --------------------------
    OoT_TrackerAdapter_Register();
    {
        int mido = -2;
        int target = -2;
        const int available = TrrComboPanel(GAME_OOT, &mido, &target);
        printf("[TEST] C (Enable Available Checks off): combo panel available %d; Mido's chest %d, target %d\n",
               available, mido, target);
        TRR_CHECK(available == -1 && mido == -1 && target == -1,
                  "C: the combo tracker's OoT panel claims an availability before Enable Available Checks has run");
    }

    // ---- sphere zero -----------------------------------------------------------------
    if (TrrEnableAndRecalc()) {
        int comboMido = -2;
        int comboTarget = -2;
        const int comboAvailable = TrrComboPanel(GAME_OOT, &comboMido, &comboTarget);
        printf("[TEST] C (sphere zero): combo panel available %d; Mido's chest %d, target %d\n", comboAvailable,
               comboMido, comboTarget);
        TRR_CHECK(comboAvailable == 1 && comboMido == 1 && comboTarget == 0,
                  "C: the combo tracker's OoT panel does not show SoH's sphere-zero set (one of its two available)");
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

    // ---- C: the combo panel after the drain, then the departure capture (#1129) -------
    {
        int comboMido = -2;
        int comboTarget = -2;
        const int live = TrrComboPanel(GAME_OOT, &comboMido, &comboTarget);
        printf("[TEST] C (after the draw): combo panel available %d; Mido's chest %d, target %d\n", live, comboMido,
               comboTarget);
        TRR_CHECK(live == 2 && comboMido == 1 && comboTarget == 1,
                  "C: the combo tracker's OoT panel does not follow the recalculation the arrival armed");
        // The departure: OoT_Game_Suspend's capture, under the panel's live gate.
        const uint16_t listed[2] = { (uint16_t)RC_KF_MIDOS_TOP_LEFT_CHEST, (uint16_t)RC_LW_TARGET_IN_WOODS };
        (void)OoT_TrackerAdapter_TestListSeam(1, listed, 2);
        (void)OoT_TrackerAdapter_TestSkipSeam(2);
        const GameId prev = Context_GetCurrentGame();
        Context_SetCurrentGame(GAME_OOT);
        OoT_TrackerAdapter_CaptureAvailability();
        Context_SetCurrentGame(prev);
        (void)OoT_TrackerAdapter_TestListSeam(-1, nullptr, 0);
        (void)OoT_TrackerAdapter_TestSkipSeam(-1);
        const int stale = TrrComboPanel(GAME_MM, &comboMido, &comboTarget);
        printf("[TEST] C (MM played, the departure capture): combo panel available %d; Mido's chest %d, target %d\n",
               stale, comboMido, comboTarget);
        TRR_CHECK(stale == 2 && comboMido == 1 && comboTarget == 1,
                  "C: the combo tracker's stale OoT panel does not show the set captured at the departure");
        Combo_TrackerFreezeAvailability((uint8_t)GAME_OOT, nullptr, 0);
    }

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
    int placedRows = 0; // what the placed predicate lists over the panel's own rows
    for (size_t i = 1; i < (size_t)RC_MAX; i++) {
        if (ctx->GetItemLocation(i)->GetPlacedRandomizerGet() != RG_NONE) {
            placedRows++;
        }
    }

    OoT_TrackerAdapter_Register();
    const GameId gameBefore = Context_GetCurrentGame();
    Context_SetCurrentGame(GAME_OOT);
    // OoT plays a loaded file (no play state in this tier); IS_RANDO is read for real.
    (void)OoT_TrackerAdapter_TestSkipSeam(2);
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

    // A vanilla file (ruling D11: randomizer files only): SoH's tracker loads for
    // every file, so its settings are loaded here too, but its predicate's vanilla
    // branch must not become the panel's list. The quest id is put back at once.
    Context_SetCurrentGame(GAME_OOT);
    const u8 questBefore = gSaveContext.ship.quest.id;
    gSaveContext.ship.quest.id = QUEST_NORMAL;
    (void)OoT_TrackerAdapter_TestListSeam(-1, NULL, 0); // zero the question count
    ComboTrackerGameSummary vanilla;
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &vanilla);
    const int vanillaAsked = OoT_TrackerAdapter_TestListSeam(-1, NULL, 0);
    gSaveContext.ship.quest.id = questBefore;
    // The title screen with SoH's tracker still loaded (a paired file MM returned
    // to title from: OoT's OnExitGame, which tears the tracker down, never ran).
    (void)OoT_TrackerAdapter_TestSkipSeam(0);
    ComboTrackerGameSummary title;
    Combo_TrackerGameSummary((uint8_t)GAME_OOT, &title);
    const int titleAsked = OoT_TrackerAdapter_TestListSeam(-1, NULL, 0);
    printf("[TEST] tracker-oot-panel-list (vanilla file): panel shuffled %d, predicate asked %d; (title screen): "
           "panel shuffled %d, predicate asked %d; placed rows %d\n",
           vanilla.shuffled, vanillaAsked, title.shuffled, titleAsked, placedRows);
    TRR_CHECK(vanillaAsked == 0, "the OoT panel asked SoH's predicate on a vanilla file");
    TRR_CHECK(vanilla.shuffled == placedRows, "the OoT panel on a vanilla file does not list the placed checks");
    TRR_CHECK(titleAsked == 0, "the OoT panel asked SoH's predicate with no file loaded");
    TRR_CHECK(title.shuffled == placedRows, "the OoT panel with no file loaded does not list the placed checks");

    (void)OoT_TrackerAdapter_TestSkipSeam(-1);
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

namespace {

// ---- #1420: SoH's tracker fixes over a paired file ------------------------------------

/** What a search reached: each check's ItemLocation::IsAddedToPool, and each
 *  region's four age/time bits, as of the last ReachabilitySearch. */
struct TrrFootprint {
    std::vector<uint8_t> checks;
    std::vector<uint8_t> regions;
    int reachedChecks = 0;

    bool operator==(const TrrFootprint& other) const {
        return checks == other.checks && regions == other.regions;
    }
};

TrrFootprint TrrTakeFootprint() {
    auto ctx = Rando::Context::GetInstance();
    TrrFootprint print;
    print.checks.resize(RC_MAX);
    for (size_t rc = 0; rc < (size_t)RC_MAX; rc++) {
        const bool reached = ctx->GetItemLocation(rc)->IsAddedToPool();
        print.checks[rc] = reached ? 1 : 0;
        print.reachedChecks += reached ? 1 : 0;
    }
    print.regions.resize(RR_MAX);
    for (size_t rr = 0; rr < (size_t)RR_MAX; rr++) {
        const Region* region = RegionTable(static_cast<RandomizerRegion>(rr));
        print.regions[rr] = (uint8_t)((region->childDay ? 1 : 0) | (region->childNight ? 2 : 0) |
                                      (region->adultDay ? 4 : 0) | (region->adultNight ? 8 : 0));
    }
    return print;
}

void TrrSetLink(int32_t entrance, int32_t age, int32_t night) {
    gSaveContext.entranceIndex = entrance;
    gSaveContext.linkAge = age;
    gSaveContext.nightFlag = night;
}

/** One round of the OoT combo engine, the generator's own search
 *  (ComboLogicEngineOoT's expand: ReachabilitySearch(ctx->allLocations)) to its
 *  closure, and what it reached. False when the engine refused the round. */
bool TrrEngineRound(TrrFootprint* out) {
    const ComboLogicEngine* e = Combo_Logic_GetEngine(GAME_OOT);
    if (e == nullptr) {
        return false;
    }
    bool ran = false;
    if (e->beginQuery(e->self) != 0) {
        for (int i = 0; i < 16 && e->expand(e->self) != 0; i++) {}
        *out = TrrTakeFootprint();
        ran = true;
    }
    e->endQuery(e->self); // combo_logic.h: called even when beginQuery refused
    return ran;
}

/** Turn Enable Available Checks on (it arms the recalculation with no play
 *  state) and drain it as the next read would: ComputeAvailableChecks from the
 *  live save's entrance, age and time. The available checks it found. */
std::set<RandomizerCheck> TrrAvailableNow() {
    std::set<RandomizerCheck> available;
    if (!TrrEnableAndRecalc()) {
        return available;
    }
    auto ctx = Rando::Context::GetInstance();
    for (const RandomizerCheck rc : ctx->allLocations) {
        if (TrrAvailable(rc)) {
            available.insert(rc);
        }
    }
    return available;
}

/** F (SoH #6447): the tower chest is a tracker row under Triforce Hunt. */
void TrrGanonBossKeyRow() {
    auto ctx = Rando::Context::GetInstance();
    const uint8_t huntBefore = ctx->GetOption(RSK_TRIFORCE_HUNT).Get();
    const uint8_t keyBefore = ctx->GetOption(RSK_GANONS_BOSS_KEY).Get();
    auto rowWith = [&](uint8_t hunt) {
        ctx->GetOption(RSK_GANONS_BOSS_KEY).Set(RO_GANON_BOSS_KEY_VANILLA);
        ctx->GetOption(RSK_TRIFORCE_HUNT).Set(hunt);
        RandoTest_CheckTrackerLoadSettings();
        return CheckTracker::IsCheckShuffled(RC_GANONS_TOWER_BOSS_KEY_CHEST);
    };
    const bool off = rowWith(RO_TRIFORCE_HUNT_OFF);
    const bool win = rowWith(RO_TRIFORCE_HUNT_WIN);
    const bool gbk = rowWith(RO_TRIFORCE_HUNT_GBK);
    printf("[TEST] F (Ganon's Boss Key Vanilla): tower chest row with no hunt %d, hunt Win %d, hunt Ganon's Boss Key "
           "%d\n",
           off ? 1 : 0, win ? 1 : 0, gbk ? 1 : 0);
    TRR_CHECK(!off, "F: with no Triforce Hunt the tower chest is a row, though it holds Ganon's Boss Key (control)");
    TRR_CHECK(win, "F: under Triforce Hunt (Win, the paired hunt's mode) the tower chest holds a random item but is "
                   "no row, so the total and Available Checks are one short (SoH #6447)");
    TRR_CHECK(gbk, "F: under Triforce Hunt (Ganon's Boss Key) the tower chest is no row");
    ctx->GetOption(RSK_TRIFORCE_HUNT).Set(huntBefore);
    ctx->GetOption(RSK_GANONS_BOSS_KEY).Set(keyBefore);
    RandoTest_CheckTrackerLoadSettings();
}

/** G: generation reads none of Link's state, and the new parameter is inert where generation starts. */
void TrrGenerationUntouched() {
    auto ctx = Rando::Context::GetInstance();
    TrrSetLink(-1, LINK_AGE_CHILD, 0);
    TrrFootprint home;
    const bool ranHome = TrrEngineRound(&home);
    TrrSetLink(ENTR_FIRE_TEMPLE_ENTRANCE, LINK_AGE_ADULT, 1);
    TrrFootprint away;
    const bool ranAway = TrrEngineRound(&away);
    printf(
        "[TEST] G1 (combo engine round): ran %d/%d; reached %d checks with Link at no entrance as a child by day, %d "
        "in the Fire Temple as an adult at night; identical %d\n",
        ranHome ? 1 : 0, ranAway ? 1 : 0, home.reachedChecks, away.reachedChecks, home == away ? 1 : 0);
    TRR_CHECK(ranHome && ranAway, "G1: the OoT combo engine refused a round");
    TRR_CHECK(home.reachedChecks > 0 && home.checks[RC_KF_MIDOS_TOP_LEFT_CHEST] != 0,
              "G1: the engine's round reached nothing, so the generation leg is vacuous");
    TRR_CHECK(home == away, "G1: the generator's search reached a different set when Link's saved entrance, age or "
                            "time changed (generation must not read them)");

    std::vector<RandomizerCheck> targets;
    for (const RandomizerCheck rc : ctx->allLocations) {
        targets.push_back(rc);
    }
    const std::vector<RandomizerCheck> rootDefault = ReachabilitySearch(targets, RG_NONE, true);
    const TrrFootprint rootDefaultPrint = TrrTakeFootprint();
    const std::vector<RandomizerCheck> rootAdultNight =
        ReachabilitySearch(targets, RG_NONE, true, RR_ROOT, RAT_ADULT_NIGHT);
    const TrrFootprint rootAdultNightPrint = TrrTakeFootprint();
    printf("[TEST] G2: from Root %d found, with adult night %d; identical %d\n", (int)rootDefault.size(),
           (int)rootAdultNight.size(),
           (rootDefault == rootAdultNight && rootDefaultPrint == rootAdultNightPrint) ? 1 : 0);
    TRR_CHECK(!rootDefault.empty(), "G2: the search from Root found nothing, so the parameter leg is vacuous");
    TRR_CHECK(rootDefault == rootAdultNight && rootDefaultPrint == rootAdultNightPrint,
              "G2: the age/time parameter changed a search that starts at Root (every generation caller's start)");

    // G3: a region where time passes (Kokiri Forest) and one where it does not (the Fire Temple entryway).
    for (const RandomizerRegion from : { RR_KOKIRI_FOREST, RR_FIRE_TEMPLE_ENTRYWAY }) {
        const std::vector<RandomizerCheck> none = ReachabilitySearch(targets, RG_NONE, true, from, RAT_NONE);
        const TrrFootprint nonePrint = TrrTakeFootprint();
        const std::vector<RandomizerCheck> childDay = ReachabilitySearch(targets, RG_NONE, true, from, RAT_CHILD_DAY);
        const TrrFootprint childDayPrint = TrrTakeFootprint();
        const bool same = none == childDay && nonePrint == childDayPrint;
        printf("[TEST] G3: from region %d with no age/time %d found, as a child by day %d; identical %d\n", (int)from,
               (int)none.size(), (int)childDay.size(), same ? 1 : 0);
        TRR_CHECK(same, "G3: with no age/time a search from a region no longer starts at the starting age's day (a "
                        "child), as it did before the parameter");
    }
}

/** S: Available Checks start where the saved entrance index puts Link, at his age and time. */
void TrrStartFromLink() {
    // The candidates: dungeons whose entryway a child holding nothing cannot reach from Root.
    const int32_t kCandidates[] = {
        ENTR_FIRE_TEMPLE_ENTRANCE,   ENTR_WATER_TEMPLE_ENTRANCE,         ENTR_SHADOW_TEMPLE_ENTRANCE,
        ENTR_SPIRIT_TEMPLE_ENTRANCE, ENTR_FOREST_TEMPLE_ENTRANCE,        ENTR_GERUDO_TRAINING_GROUND_ENTRANCE,
        ENTR_ICE_CAVERN_ENTRANCE,    ENTR_INSIDE_GANONS_CASTLE_ENTRANCE,
    };
    TrrSetLink(-1, LINK_AGE_CHILD, 0);
    const std::set<RandomizerCheck> fromRoot = TrrAvailableNow();
    int32_t entrance = -1;
    RandomizerRegion region = RR_NONE;
    for (const int32_t candidate : kCandidates) {
        const RandomizerRegion named = static_cast<RandomizerRegion>(OoT_CheckTracker_TestStartingRegion(candidate));
        if (named != RR_ROOT && !RegionTable(named)->HasAccess()) {
            entrance = candidate;
            region = named;
            break;
        }
    }
    printf("[TEST] S0: from Root %d available; first dungeon Root does not reach: entrance 0x%X, region %d\n",
           (int)fromRoot.size(), (unsigned)entrance, (int)region);
    TRR_CHECK(!fromRoot.empty(), "S0: nothing is available from Root, so the start legs are vacuous");
    if (entrance < 0) {
        TRR_CHECK(false, "S0: Root reaches every candidate dungeon at sphere zero, or the tracker load left the "
                         "entrances unstamped, so the start legs are vacuous");
        return;
    }

    struct AgeTime {
        const char* name;
        int32_t age;
        int32_t night;
    };
    const AgeTime kAgeTimes[] = {
        { "child day", LINK_AGE_CHILD, 0 },
        { "child night", LINK_AGE_CHILD, 1 },
        { "adult day", LINK_AGE_ADULT, 0 },
        { "adult night", LINK_AGE_ADULT, 1 },
    };
    for (const AgeTime& at : kAgeTimes) {
        TrrSetLink(entrance, at.age, at.night);
        const std::set<RandomizerCheck> fromLink = TrrAvailableNow();
        const Region* here = RegionTable(region);
        const bool reachedAtLinks = at.age == LINK_AGE_CHILD ? (at.night ? here->childNight : here->childDay)
                                                             : (at.night ? here->adultNight : here->adultDay);
        int lost = 0;
        for (const RandomizerCheck rc : fromRoot) {
            if (fromLink.count(rc) == 0) {
                if (lost < 10) {
                    printf("[TEST]   S (%s): %s is available from Root but not with Link in the dungeon\n", at.name,
                           TrrName(rc));
                }
                lost++;
            }
        }
        printf("[TEST] S (%s): Link's region reached at his age and time %d; %d available (Root alone %d); lost %d\n",
               at.name, reachedAtLinks ? 1 : 0, (int)fromLink.size(), (int)fromRoot.size(), lost);
        TRR_CHECK(reachedAtLinks, "S: Available Checks did not start from the region the saved entrance index names, "
                                  "at Link's age and time (SoH #6165, #6237)");
        TRR_CHECK(lost == 0, "S: starting from Link's region lost a check Root alone makes available");
    }

    TrrSetLink(-1, LINK_AGE_CHILD, 0);
    const std::set<RandomizerCheck> backHome = TrrAvailableNow();
    const bool regionDropped = !RegionTable(region)->HasAccess();
    printf("[TEST] S (no known entrance again): %d available, same as Root alone %d; the dungeon dropped %d\n",
           (int)backHome.size(), backHome == fromRoot ? 1 : 0, regionDropped ? 1 : 0);
    TRR_CHECK(backHome == fromRoot && regionDropped,
              "S: with no known entrance the set is not Root's again, so the start came from leftover state");
}

} // namespace

/**
 * #1420 over the paired file the panel-list legs above created (row
 * TrackerOoTPanelList). Returns the failure count.
 */
extern "C" int OoT_TrackerSohFixes_RunPaired(void) {
    printf("[TEST] tracker-soh-fixes (paired file): the tower chest row under Triforce Hunt, generation untouched, "
           "and Available Checks from Link's entrance, age and time (#1420)\n");
    sFailures = 0;
    auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr || !Combo_ForeignPairingActive() || gSaveContext.ship.quest.id != QUEST_RANDOMIZER) {
        printf("[TEST] FAIL: tracker-soh-fixes: no paired randomizer file to drive (the panel-list legs make it)\n");
        return 1;
    }
    TRR_CHECK(ctx->GetOption(RSK_SELECTED_STARTING_AGE).Is(RO_AGE_CHILD),
              "the world does not start as a child, so G3 compares the wrong age");
    const int32_t entranceBefore = gSaveContext.entranceIndex;
    const int32_t ageBefore = gSaveContext.linkAge;
    const int32_t nightBefore = gSaveContext.nightFlag;

    TrrGanonBossKeyRow();

    // SoH's tracker load, as its OnLoadGame hook runs it: the region graph rebuilt and its
    // entrances stamped (0xFE: a slot SaveSection refuses, as in the panel-list legs).
    const int32_t fileNum = gSaveContext.fileNum;
    gSaveContext.fileNum = 0xFE;
    RandoTest_CheckTrackerLoadGame(0);
    gSaveContext.fileNum = fileNum;

    TrrGenerationUntouched();
    TrrStartFromLink();

    TrrSetLink(entranceBefore, ageBefore, nightBefore);
    TrrTrackerOff();
    CheckTracker::Teardown();
    if (sFailures == 0) {
        printf("[TEST] PASS: tracker-soh-fixes (paired file)\n");
    } else {
        printf("[TEST] FAIL: tracker-soh-fixes (paired file): %d check(s) failed\n", sFailures);
    }
    return sFailures;
}
