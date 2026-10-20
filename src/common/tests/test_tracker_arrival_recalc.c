/**
 * @file test_tracker_arrival_recalc.c
 * @brief ROM-free lock: a cross-game arrival into OoT schedules the Check Tracker's
 *        Available Checks recalculation (#990).
 *
 * The tracker's Available Checks set is event-driven. Its deferred flag is armed
 * by exactly two events: OnLoadGame and a full SoH SaveFile. A cross-game arrival
 * is neither (the frozen save is restored at Play_ConsumeStartupEntrance without
 * Save_LoadFile, and the crossing commits through RsbsSave_Save), and the apply
 * and redeem that change the live state run before OoT_gPlayState is assigned, so
 * every handler that could notice them bails on IsSaveLoaded. Items redeemed and
 * shared resources applied from Termina therefore left the set stale until an
 * unrelated trigger.
 *
 * The row drives OoT_Play_ApplyArrivalShared, the function OoT_Play_Init calls at
 * the arrival (z_play.c), so skipping the tracker call there fails the row:
 *
 *   precondition  the deferred flag is clear and the OoT arrival flag is set
 *   arrival       a crossing item is waiting; the real apply + redeem run
 *   assertions    the item was redeemed (the real consumer ran), and the flag is armed
 *
 * and the Enable Available Checks option's change handler, which called
 * RecalculateAvailableChecks directly (a no-op while Majora's Mask is the active
 * game), must arm the flag when no OoT play state exists.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE, compiled as C++;
 * the runner wrapper (Test_TrackerArrivalRecalc) lives beside the rows that need
 * the display-free shared Context bring-up.
 */

#include "../context.h"
#include "../entrance.h"
#include "../shared_items.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>

extern "C" {
// z_play.c: the arrival's apply + redeem + tracker notification, as OoT_Play_Init runs it.
void OoT_Play_ApplyArrivalShared(void);
// randomizer_check_tracker.cpp: test bridges onto the deferred flag and the option handler.
int RandoTest_CheckTrackerRecalcPending(void);
void RandoTest_CheckTrackerClearRecalcPending(void);
void RandoTest_CheckTrackerSetEnableAvailable(int enabled);
}

#define TAR_ASSERT(cond)                                                   \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            Combo_ClearStartupEntrance();                                  \
            return TEST_FAIL;                                              \
        }                                                                  \
    } while (0)

static TestResult TrackerArrivalRecalc_Run(void) {
    printf("[TEST] tracker-arrival-recalc: a cross-game arrival into OoT schedules the tracker's Available Checks "
           "recalculation (#990)\n");

    SharedItem lens;
    TAR_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Lens of Truth", &lens));

    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    RandoTest_CheckTrackerClearRecalcPending();

    // ---- The arrival: an OoT item taken in Termina waits; a switch into OoT is in flight. ----
    TAR_ASSERT(Combo_RecordSharedItemCrossing(GAME_OOT, lens.id) >= 0);
    TAR_ASSERT(Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/false) == 1);
    Combo_SetStartupEntrance(0x0100);
    TAR_ASSERT(Combo_HasStartupEntranceForGame("oot"));
    TAR_ASSERT(RandoTest_CheckTrackerRecalcPending() == 0); // precondition: nothing armed it yet

    OoT_Play_ApplyArrivalShared();

    TAR_ASSERT(Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/false) == 0); // the real redeem ran
    const int armedAfterArrival = RandoTest_CheckTrackerRecalcPending();
    printf("[TEST] tracker-arrival-recalc: after the arrival's apply + redeem the deferred recalculation flag is %s\n",
           armedAfterArrival ? "armed" : "NOT armed");
    TAR_ASSERT(armedAfterArrival == 1);
    Combo_ClearStartupEntrance();

    // ---- The Enable Available Checks toggle with no OoT play state (MM active, or the arrival window). ----
    RandoTest_CheckTrackerClearRecalcPending();
    RandoTest_CheckTrackerSetEnableAvailable(1);
    const int armedByToggle = RandoTest_CheckTrackerRecalcPending();
    printf("[TEST] tracker-arrival-recalc: toggling Enable Available Checks with no play state leaves the flag %s\n",
           armedByToggle ? "armed" : "NOT armed");
    RandoTest_CheckTrackerSetEnableAvailable(0); // restore the module default
    RandoTest_CheckTrackerClearRecalcPending();
    TAR_ASSERT(armedByToggle == 1);

    Combo_ClearSharedItemOutbox();
    ComboContext_Init();
    printf("[TEST] PASS: the arrival and the option toggle both schedule the recalculation\n");
    return TEST_PASS;
}
