/**
 * @file test_crossing_toasts.c
 * @brief ROM-free lock: a crossing announces nothing its pickup already did (#878).
 *
 * The operator's playtest of 2026-10-09: "you get duplicate toasts when you
 * cross over". Reproduced at main b0cc1934 with the paired first-crossing drive:
 * an MM item taken from an OoT chest shows OoT's get-item textbox ("You found
 * the Fierce Deity Mask!"), and the next arrival in Majora's Mask toasted "You
 * got the Fierce Deity Mask" for the same item; an OoT item taken in Majora's
 * Mask shows MM's textbox ("You found a Bottle with Ruto's Letter!"), and the
 * next arrival in Ocarina of Time toasted "You got a Bottle with Ruto's
 * Letter". One pickup, announced twice, the second time at the crossing.
 *
 * The event is the pickup (the paired world is one game: the item was received
 * when it was taken), and the pickup's own presentation is its one
 * announcement. This row drives both legs' REAL arrival consumers and counts
 * every toast the overlay is asked to show between the crossing's record and
 * the end of the arrival:
 *
 *   OoT -> MM   Combo_RecordSharedItemCrossing(GAME_MM, id)  the real pickup producer
 *                 -> MM_ConsumeSharedItems()            MM's real arrival hook
 *                 -> MM_ForeignItem_FlushPending()      the real first-frame drain, where
 *                                                       the give and its presentation run
 *                                                       (over a stand-in PlayState: see
 *                                                       MM_ForeignItem_TestFlushWithStandInPlay)
 *   MM -> OoT   Combo_RecordSharedItemCrossing(GAME_OOT, id)
 *                 -> OoT_ConsumeSharedItems()           OoT's real arrival hook
 *
 * Each leg must redeem its crossing and must emit zero toasts. Before #878
 * each leg emitted one ("You got ..."). The MM leg's give runs (its deferred
 * drain is flushed and counted). The OoT leg's give is unavailable ROM-free
 * (no live rando context: "[OoT] foreign give unavailable"), so for OoT only
 * the redeem and the toast count are asserted. The unfixed OoT_AwardSharedItem
 * toasted whether or not its give ran, which is why that leg was red too.
 *
 * A SOURCED grant (Combo_SubmitSourcedGrant: the netplay relay's seam) was never
 * presented at a pickup in this player's game, so its arrival toast is its only
 * presentation and stays (#878's addendum): the same two arrivals, fed by a
 * sourced grant of the same items, must each toast exactly once.
 *
 * The record is OoT's test-only toast record (soh/Notification/Notification.cpp),
 * the one overlay both games draw into. The row mutes the overlay so no unmuted
 * toast reaches OoT's audio in a display-free process.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE, compiled as C++;
 * its runner wrapper (Test_CrossingArrivalToasts) lives beside the rows that
 * need the display-free shared Context bring-up, because the overlay's Emit
 * reads the CVar store.
 */

#include "../context.h"
#include "../foreign_items.h"
#include "../notification_bridge.h"
#include "../shared_items.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>
#include <cstring>

extern "C" {
// The two arrival hooks z_play.c calls (one per game).
void MM_ConsumeSharedItems(void);
void OoT_ConsumeSharedItems(void);

// MM's deferred give (2s2h/Rando/ForeignItemsSingleExe.cpp).
int MM_ForeignItem_TestPendingCount(void);
void MM_ForeignItem_TestResetPending(void);
uint16_t MM_ForeignItem_TestStandInItemId(void);
int MM_ForeignItem_TestFlushWithStandInPlay(void);
}

#define CAT_ASSERT(cond)                                                                                               \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            return TEST_FAIL;                                                                                          \
        }                                                                                                              \
    } while (0)

/** Print every toast recorded from index @p from on; returns how many. */
static int CrossingToastsSince(int from, const char* leg) {
    const int total = OoT_Notification_EmittedCountForTest();
    for (int i = from; i < total; i++) {
        char toast[256] = { 0 };
        OoT_Notification_EmittedAtForTest(i, toast, sizeof(toast));
        printf("[TEST] crossing-arrival-toasts: the arrival in %s toasted \"%s\"\n", leg, toast);
    }
    return total - from;
}

/** The body; Test_CrossingArrivalToasts (test_runner.cpp) brings up the Context first. */
static TestResult CrossingArrivalToasts_Run(void) {
    printf("[TEST] crossing-arrival-toasts: a crossing toasts nothing its pickup already announced (#878)\n");

    SharedItem lens;
    CAT_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Lens of Truth", &lens));
    const uint16_t mmId = MM_ForeignItem_TestStandInItemId();

    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    MM_ForeignItem_TestResetPending();
    OoT_Notification_RecordForTest(1);

    // ---- Leg 1: an MM item taken in Hyrule, then the arrival in Termina. ----
    int mark = OoT_Notification_EmittedCountForTest();
    CAT_ASSERT(Combo_RecordSharedItemCrossing(GAME_MM, mmId) >= 0);
    CAT_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) == 1);
    MM_ConsumeSharedItems();
    CAT_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) == 0); // redeemed at the arrival
    CAT_ASSERT(MM_ForeignItem_TestPendingCount() == 1);                          // the give waits for play
    CAT_ASSERT(MM_ForeignItem_TestFlushWithStandInPlay() == 1); // the first gameplay frame's drain ran the give
    const int mmToasts = CrossingToastsSince(mark, "Majora's Mask");

    // ---- Leg 2: an OoT item taken in Termina, then the arrival in Hyrule. ----
    mark = OoT_Notification_EmittedCountForTest();
    CAT_ASSERT(Combo_RecordSharedItemCrossing(GAME_OOT, lens.id) >= 0);
    CAT_ASSERT(Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/false) == 1);
    OoT_ConsumeSharedItems();
    CAT_ASSERT(Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/false) == 0); // redeemed at the arrival
    const int ootToasts = CrossingToastsSince(mark, "Ocarina of Time");

    // ---- Legs 3 and 4: the same items as a peer's sourced grant, never picked
    // up in this player's game: each arrival toasts it once. ----
    const uint32_t kSource = 0x878u;
    mark = OoT_Notification_EmittedCountForTest();
    CAT_ASSERT(Combo_SubmitSourcedGrant(kSource, 1, GAME_MM, mmId) == RSBS_GRANT_ACCEPTED);
    MM_ConsumeSharedItems();
    CAT_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) == 0);
    CAT_ASSERT(MM_ForeignItem_TestFlushWithStandInPlay() == 1);
    const int mmSourcedToasts = CrossingToastsSince(mark, "Majora's Mask (sourced grant)");

    mark = OoT_Notification_EmittedCountForTest();
    CAT_ASSERT(Combo_SubmitSourcedGrant(kSource, 2, GAME_OOT, lens.id) == RSBS_GRANT_ACCEPTED);
    OoT_ConsumeSharedItems();
    CAT_ASSERT(Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/false) == 0);
    const int ootSourcedToasts = CrossingToastsSince(mark, "Ocarina of Time (sourced grant)");

    printf("[TEST] crossing-arrival-toasts: crossing pickups: the arrival in Majora's Mask emitted %d toast(s), the "
           "arrival in Ocarina of Time %d (each was announced at its pickup, so each must be 0); sourced grants: %d "
           "and %d (never picked up here, so each must be 1)\n",
           mmToasts, ootToasts, mmSourcedToasts, ootSourcedToasts);

    OoT_Notification_RecordForTest(0);
    MM_ForeignItem_TestResetPending();
    Combo_ClearSharedItemOutbox();
    ComboContext_Init();

    CAT_ASSERT(mmToasts == 0);
    CAT_ASSERT(ootToasts == 0);
    CAT_ASSERT(mmSourcedToasts == 1);
    CAT_ASSERT(ootSourcedToasts == 1);
    printf("[TEST] PASS: both arrivals redeemed their crossing and toasted nothing; a sourced grant toasts once\n");
    return TEST_PASS;
}
