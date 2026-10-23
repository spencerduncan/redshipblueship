/**
 * @file mm_tracker_in_logic_test.cpp
 * MM's half of the tracker-reachability rows (#994; audit
 * docs/tracker-reachability-audit-2026-10-18.md, R4). Nothing else drives the MM
 * Check Tracker's in-logic computation across an arrival.
 *
 * WHAT IS UNDER TEST is Rando::CheckTracker::ComputeChecksInLogic, the gate-free
 * callable RefreshChecksInLogic draws from (CheckTracker.cpp, split out with the
 * #995 bracket), over MM's REAL region graph (MM_Rando_InitCore, the
 * MMComboLogicEngine bring-up).
 *
 * MM_TrackerInLogic_RunArrival (row MMTrackerInLogicArrival), over an MM live
 * save as an arrival leaves it (save.entrance = 0xD800, South Clock Town):
 *   B1 the South Clock Town lower chest (hookshot-gated, Regions/Central.cpp) is
 *      out of logic, while the platform heart piece in the same region (needs
 *      nothing) is in: the crawl reached the region.
 *   B2 a pool carrying the hookshot tier (an OoT departure holding the
 *      hookshot) applied through the REAL MM_ApplySharedResources puts the
 *      hookshot in MM's inventory, and the chest comes into logic, not obtained.
 *   B3 marked obtained, it drops out; the heart piece stays in.
 *   B4 every computation leaves the SaveContext byte-identical (RANDO_EVENTS is
 *      poisoned first, so an unbracketed crawl's zero-and-refire shows).
 *   C  the combo tracker's MM panel reads the same set (#1129): with Enable
 *      Available Checks on and MM's play state stood in by the adapter's seam,
 *      its LIVE summary counts 1, 2 and 1 available through B1-B3 and the chest's
 *      row follows; the departure capture MM_Game_Suspend makes at B2 holds the
 *      chest.
 *
 * MM_TrackerFillSuperset_RunMM (row TrackerFillSuperset, MM half): over the
 * MM half the paired creation armed in the shadow, at the arrival entrance:
 * MM's fill engine (Combo_Logic_GetEngine(GAME_MM), one round, nothing assumed)
 * reaches a set of shuffled checks, and every one not obtained is in the
 * tracker's in-logic set. The caller ran the creation (OoT half).
 *
 * Each entry point returns its failure count.
 */

#include "global.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <set>

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The rows are single-exe surfaces; test_runner.cpp declares these
// unconditionally, so a standalone 2ship build links.
extern "C" int MM_TrackerInLogic_RunArrival(void) {
    return 0;
}
extern "C" int MM_TrackerFillSuperset_RunMM(void) {
    return 0;
}
#else

#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/CheckTracker/CheckTracker.h"
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/Rando/StaticData/StaticData.h"

#include "combo_logic.h"
#include "context.h"
#include "shared_resources.h"
#include "combo_tracker_view.h" // #1129: the combo tracker's MM panel

#include <libultraship/bridge/consolevariablebridge.h>

// TrackerAdapterSingleExe.cpp (#1129): MM's play state stood in, and the cached set dropped.
extern "C" void MM_TrackerAdapter_TestLiveSeam(int live);
extern "C" int MM_TrackerAdapter_TestInLogicSeam(int authored, const uint16_t* ids, int count);

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
void MM_Rando_InitCore(void);
void MM_ApplySharedResources(void);
}

namespace {

int sFailures = 0;

#define TIL_CHECK(cond, msg)                                              \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            sFailures++;                                                  \
        }                                                                 \
    } while (0)

constexpr s32 kArrivalEntrance = ENTRANCE(SOUTH_CLOCK_TOWN, 0);

/** Restores the whole live save on scope exit. */
struct TilSaveGuard {
    std::unique_ptr<SaveContext> saved = std::make_unique<SaveContext>();
    TilSaveGuard() {
        memcpy(saved.get(), &gSaveContext, sizeof(SaveContext));
    }
    ~TilSaveGuard() {
        memcpy(&gSaveContext, saved.get(), sizeof(SaveContext));
    }
    TilSaveGuard(const TilSaveGuard&) = delete;
    TilSaveGuard& operator=(const TilSaveGuard&) = delete;
};

/** The tracker's in-logic set, and whether computing it changed any byte of the
 *  live save. RANDO_EVENTS is poisoned first: an unbracketed crawl zeroes it. */
std::set<RandoCheckId> TilInLogic(bool* saveUnchanged) {
    for (int i = 0; i < RE_MAX; i++) {
        RANDO_EVENTS[i] = 0x5A;
    }
    auto before = std::make_unique<SaveContext>();
    memcpy(before.get(), &gSaveContext, sizeof(SaveContext));
    std::set<RandoCheckId> inLogic = Rando::CheckTracker::ComputeChecksInLogic(gSaveContext.save.entrance);
    *saveUnchanged = memcmp(before.get(), &gSaveContext, sizeof(SaveContext)) == 0;
    return inLogic;
}

const char* const kTilAvailableKey = "gTrackers.CheckTracker.EnableAvailableChecks";

/**
 * C (#1129): the combo tracker's MM panel on the live save, its cached set dropped
 * first so it computes on this save: the summary's available count (-1 when not
 * known, -2 when the panel does not read the live save) and the lower chest's row
 * (-1 not known).
 */
int TilComboPanel(int* outChest) {
    (void)MM_TrackerAdapter_TestInLogicSeam(0, nullptr, 0);
    ComboTrackerGameSummary summary;
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    ComboTrackerCheckRow row;
    *outChest = !Combo_TrackerCheckAt((uint8_t)GAME_MM, (int)RC_CLOCK_TOWN_SOUTH_CHEST_LOWER, &row) ? -2
                : row.availabilityKnown ? (row.available ? 1 : 0)
                                        : -1;
    if (summary.freshness != COMBO_TRACKER_FRESH_LIVE) {
        return -2;
    }
    return summary.availabilityKnown ? summary.available : -1;
}

} // namespace

extern "C" int MM_TrackerInLogic_RunArrival(void) {
    printf("[TEST] mm-tracker-in-logic-arrival: the MM Check Tracker's in-logic set follows a hookshot tier applied at "
           "an arrival into Termina (#994)\n");
    sFailures = 0;

    MM_Rando_InitCore();
    if (Rando::Logic::Regions.empty()) {
        printf("[TEST] FAIL: Rando::Logic::Regions is empty after MM_Rando_InitCore, so every leg would be vacuous\n");
        return 1;
    }
    TIL_CHECK(kArrivalEntrance == 0xD800, "ENTRANCE(SOUTH_CLOCK_TOWN, 0) is not the combo's MM arrival 0xD800");

    TilSaveGuard guard;

    // MM's live save as an arrival leaves it: a rando file, no flash slot
    // (0xFF), human form, nothing in the inventory, standing at the arrival.
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    memset(gSaveContext.save.saveInfo.inventory.items, ITEM_NONE, sizeof(gSaveContext.save.saveInfo.inventory.items));
    gSaveContext.save.entrance = kArrivalEntrance;
    RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_SOUTH_CHEST_LOWER].shuffled = true;
    RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_SOUTH_PLATFORM_PIECE_OF_HEART].shuffled = true;
    // The new-file marker every MM file carries, so the combo panel reads this save (C).
    memcpy(gSaveContext.save.saveInfo.playerData.newf, "ZELDA3", 6);

    // C (#1129): the combo tracker's MM panel on this save, as while MM is played
    // with Enable Available Checks on.
    MM_TrackerAdapter_Register();
    MM_TrackerAdapter_TestLiveSeam(1);
    const GameId prevGame = Context_GetCurrentGame();
    Context_SetCurrentGame(GAME_MM);
    const bool hadAvailableKey = CVarExists(kTilAvailableKey);
    const int32_t availableKey = CVarGetInteger(kTilAvailableKey, 0);
    CVarSetInteger(kTilAvailableKey, 1);
    int comboChest = -2;

    // The pool: an OoT departure holding the hookshot (OoT's harvest of tier 1).
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    Combo_HarvestSharedResource(GAME_OOT, (uint8_t)RSBS_SHARED_RES_HOOKSHOT_TIER, 1);
    uint16_t poolTier = 0;
    TIL_CHECK(Combo_GetSharedResource((uint8_t)RSBS_SHARED_RES_HOOKSHOT_TIER, &poolTier) && poolTier == 1,
              "the pool does not carry hookshot tier 1 after OoT's harvest");

    // ---- B1: before the apply ----------------------------------------------------------
    bool unchanged1 = false;
    const std::set<RandoCheckId> before = TilInLogic(&unchanged1);
    const bool chestBefore = before.count(RC_CLOCK_TOWN_SOUTH_CHEST_LOWER) != 0;
    const bool heartBefore = before.count(RC_CLOCK_TOWN_SOUTH_PLATFORM_PIECE_OF_HEART) != 0;
    printf("[TEST] B1 (before the apply): in logic %d checks; lower chest %d, platform heart piece %d\n",
           (int)before.size(), chestBefore ? 1 : 0, heartBefore ? 1 : 0);
    TIL_CHECK(heartBefore, "B1: the South Clock Town platform heart piece (needs nothing) is not in logic at the "
                           "arrival, so the crawl did not reach the region and the chest legs would be vacuous");
    TIL_CHECK(!chestBefore, "B1: the hookshot-gated South Clock Town lower chest is in logic with no hookshot");
    const int combo1 = TilComboPanel(&comboChest);
    printf("[TEST] C (B1): combo panel available %d, lower chest %d\n", combo1, comboChest);
    TIL_CHECK(combo1 == 1 && comboChest == 0,
              "C: the combo tracker's MM panel does not show B1's set (the heart piece)");

    // ---- B2: the arrival's apply -------------------------------------------------------
    MM_ApplySharedResources();
    const bool holdsHookshot = INV_CONTENT(ITEM_HOOKSHOT) == ITEM_HOOKSHOT;
    bool unchanged2 = false;
    const std::set<RandoCheckId> after = TilInLogic(&unchanged2);
    const bool chestAfter = after.count(RC_CLOCK_TOWN_SOUTH_CHEST_LOWER) != 0;
    const bool chestObtained = RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_SOUTH_CHEST_LOWER].obtained;
    printf("[TEST] B2 (after the apply): hookshot in the inventory %d; in logic %d checks; lower chest %d, obtained "
           "%d\n",
           holdsHookshot ? 1 : 0, (int)after.size(), chestAfter ? 1 : 0, chestObtained ? 1 : 0);
    TIL_CHECK(holdsHookshot, "B2: MM_ApplySharedResources did not put the pool's hookshot tier in MM's inventory");
    TIL_CHECK(chestAfter && !chestObtained,
              "B2: the lower chest is not in logic (and unobtained) once the hookshot tier is applied");
    const int combo2 = TilComboPanel(&comboChest);
    printf("[TEST] C (B2): combo panel available %d, lower chest %d\n", combo2, comboChest);
    TIL_CHECK(combo2 == 2 && comboChest == 1, "C: the combo tracker's MM panel does not follow the applied hookshot");
    // The departure: MM_Game_Suspend's capture, computed afresh on this save.
    Combo_TrackerFreezeAvailability((uint8_t)GAME_MM, nullptr, 0);
    MM_TrackerAdapter_CaptureAvailability();
    const int frozenChest =
        Combo_TrackerFrozenAvailability((uint8_t)GAME_MM, (uint16_t)RC_CLOCK_TOWN_SOUTH_CHEST_LOWER);
    const int frozenHeart =
        Combo_TrackerFrozenAvailability((uint8_t)GAME_MM, (uint16_t)RC_CLOCK_TOWN_SOUTH_PLATFORM_PIECE_OF_HEART);
    printf("[TEST] C (B2 departure capture): lower chest %d, platform heart piece %d\n", frozenChest, frozenHeart);
    TIL_CHECK(frozenChest == 1 && frozenHeart == 1, "C: the departure capture does not hold B2's in-logic set");

    // ---- B3: collected -----------------------------------------------------------------
    RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_SOUTH_CHEST_LOWER].obtained = true;
    bool unchanged3 = false;
    const std::set<RandoCheckId> collected = TilInLogic(&unchanged3);
    const bool chestCollected = collected.count(RC_CLOCK_TOWN_SOUTH_CHEST_LOWER) != 0;
    const bool heartCollected = collected.count(RC_CLOCK_TOWN_SOUTH_PLATFORM_PIECE_OF_HEART) != 0;
    printf("[TEST] B3 (chest obtained): lower chest %d, platform heart piece %d\n", chestCollected ? 1 : 0,
           heartCollected ? 1 : 0);
    TIL_CHECK(!chestCollected, "B3: an obtained check is still listed in logic");
    TIL_CHECK(heartCollected, "B3: marking the chest obtained dropped an unrelated check");
    const int combo3 = TilComboPanel(&comboChest);
    printf("[TEST] C (B3): combo panel available %d, lower chest %d\n", combo3, comboChest);
    TIL_CHECK(combo3 == 1 && comboChest == 0, "C: the combo tracker's MM panel still counts an obtained check");

    // ---- B4: the computation writes nothing ---------------------------------------------
    printf("[TEST] B4: SaveContext byte-identical around each computation: %d %d %d\n", unchanged1 ? 1 : 0,
           unchanged2 ? 1 : 0, unchanged3 ? 1 : 0);
    TIL_CHECK(unchanged1 && unchanged2 && unchanged3,
              "B4: the in-logic computation changed the live SaveContext (the #995 bracket)");

    if (hadAvailableKey) {
        CVarSetInteger(kTilAvailableKey, availableKey);
    } else {
        CVarClear(kTilAvailableKey);
    }
    MM_TrackerAdapter_TestLiveSeam(-1);
    (void)MM_TrackerAdapter_TestInLogicSeam(0, nullptr, 0);
    Context_SetCurrentGame(prevGame);
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    if (sFailures != 0) {
        printf("[TEST] FAIL: mm-tracker-in-logic-arrival: %d check(s) failed\n", sFailures);
    } else {
        printf("[TEST] PASS: mm-tracker-in-logic-arrival\n");
    }
    return sFailures;
}

extern "C" int MM_TrackerFillSuperset_RunMM(void) {
    printf("[TEST] tracker-fill-superset (MM): the MM Check Tracker's in-logic set contains the fill's reached set on "
           "the paired creation's MM half (#994)\n");
    sFailures = 0;

    MM_Rando_InitCore();
    const ComboLogicEngine* e = Combo_Logic_GetEngine(GAME_MM);
    const void* shadow = Context_GetMMSaveContext();
    if (Rando::Logic::Regions.empty() || e == nullptr || e->snapshot == nullptr || e->restore == nullptr ||
        shadow == nullptr || Context_HasFrozenState(GAME_MM) == 0) {
        printf("[TEST] FAIL: no MM region graph, engine or armed MM shadow to compare (run after the OoT half)\n");
        return 1;
    }

    TilSaveGuard guard;
    // The MM half the creation armed, as the first arrival restores it.
    memcpy(&gSaveContext, shadow, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.save.entrance = kArrivalEntrance;
    TIL_CHECK(IS_RANDO, "the armed MM half is not a rando save");

    // ---- the tracker ---------------------------------------------------------------------
    bool unchanged = false;
    const std::set<RandoCheckId> tracker = TilInLogic(&unchanged);
    TIL_CHECK(unchanged, "the in-logic computation changed the live SaveContext");

    // ---- the fill's round: nothing assumed, every shuffled host withheld (sphere
    // zero, whatever placements the engine still holds) ---------------------------------
    std::set<RandoCheckId> fill;
    if (e->snapshot(e->self) != 0) {
        if (e->beginQuery(e->self) != 0) {
            if (e->withholdHost != nullptr) {
                for (int id = 1; id < RC_MAX; id++) {
                    if (RANDO_SAVE_CHECKS[id].shuffled) {
                        e->withholdHost(e->self, (uint16_t)id);
                    }
                }
            }
            for (int i = 0; i < RSBS_COMBO_LOGIC_MAX_ROUND_ITERATIONS; ++i) {
                if (!e->expand(e->self)) {
                    break;
                }
            }
            for (int id = 1; id < RC_MAX; id++) {
                if (e->checkReached(e->self, (uint16_t)id) != 0 && RANDO_SAVE_CHECKS[id].shuffled) {
                    fill.insert((RandoCheckId)id);
                }
            }
        } else {
            TIL_CHECK(false, "the MM engine refused to open a round");
        }
        e->restore(e->self);
        e->endQuery(e->self);
    } else {
        TIL_CHECK(false, "the MM engine could not snapshot the live save");
    }

    int missing = 0;
    int obtained = 0;
    for (RandoCheckId id : fill) {
        if (RANDO_SAVE_CHECKS[id].obtained) {
            obtained++;
            continue;
        }
        if (tracker.count(id) == 0) {
            if (missing < 40) {
                const auto it = Rando::StaticData::Checks.find(id);
                printf("[TEST] superset (MM): the fill reaches %s (%d), the tracker does not list it in logic\n",
                       it != Rando::StaticData::Checks.end() ? it->second.name : "?", (int)id);
            }
            missing++;
        }
    }
    int trackerOnly = 0;
    for (RandoCheckId id : tracker) {
        trackerOnly += fill.count(id) == 0 ? 1 : 0;
    }
    printf("[TEST] superset (MM): fill reached %d shuffled checks (%d obtained), tracker in logic %d; fill-only %d, "
           "tracker-only %d (informational)\n",
           (int)fill.size(), obtained, (int)tracker.size(), missing, trackerOnly);
    TIL_CHECK(fill.size() >= 10, "the fill's round reached almost no shuffled check, so the superset would be vacuous");
    TIL_CHECK(missing == 0, "the MM fill reaches shuffled checks the MM Check Tracker does not list in logic on the "
                            "same save");
    if (sFailures == 0) {
        printf("[TEST] PASS: tracker-fill-superset (MM)\n");
    }
    return sFailures;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
