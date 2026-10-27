/**
 * @file test_foreign_outbox_full.c
 * @brief ROM-free locks for #1222: a foreign pickup the full shared outbox would
 *        refuse is held, not lost.
 *
 * THE OUTBOX. Every cross-game pickup is one row of gComboCtx.sharedItemsTagged
 * (RSBS_SHARED_ITEM_CAP, 64 rows), redeemed when the other game next arrives.
 * When all 64 rows hold items the other game has not redeemed yet, the record is
 * REFUSED (src/common/shared_items.c, AppendSharedItem): nothing is written and
 * the durable gComboCtx.sharedItemOverflowCount goes up. Before #1222 the MM give
 * marked the host delivered whatever the record answered (MiscBehavior/
 * CheckQueue.cpp, GiveForeignCheck), and a shop charged the price and sold the
 * slot before the answer was known (ActorBehavior/EnGirlA.cpp), so the OoT item
 * was lost for the file with nothing shown to the player.
 *
 * MM (Test_ForeignOutboxFullMM), with the 64 rows full:
 *  Q CHECKQUEUE HOLDS an eligible foreign host back (Rando::Foreign::
 *    ForeignOutboxFullFor): it stays `eligible` and delivers once a crossing
 *    frees a row.
 *  G THE GIVE ITSELF (GiveForeignCheck, which CheckQueue's lambda and a shop's
 *    purchase call) leaves a refused host open: not `obtained`, not
 *    `cycleObtained`, still `eligible`; the refusal is counted once.
 *  F THE SHOP: the counter refuses the sale; a buy forced past it charges
 *    nothing, crosses nothing, and leaves the slot unsold and the shelf standing.
 *  R ROOM AGAIN: with one row redeemed (reclaimable), the same host is no longer
 *    held and its give delivers.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * beside test_foreign_award.c; every symbol it drives is C-linkage.
 */

#include "../context.h"
#include "../foreign_items.h"
#include "../shared_items.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>
#include <cstring>

extern "C" {
int MM_Rando_Foreign_TestIsForeignHostClass(uint16_t randoCheckId);
int MM_Rando_Foreign_TestCheckIdMax(void);
int MM_Rando_Foreign_TestCheckShopClass(uint16_t randoCheckId, int* outIsShop, int* outIsTingleShop);
void MM_Rando_Foreign_TestSetCheckBits(uint16_t randoCheckId, int obtained, int cycleObtained, int eligible);
int MM_Rando_Foreign_TestGiveForeignCheck(uint16_t randoCheckId, int* outObtained, int* outCycleObtained,
                                          int* outEligible);
int MM_Rando_Foreign_TestQueueHolds(uint16_t randoCheckId);
int MM_EnGirlA_TestForeignPurchaseOutboxFull(uint16_t randoCheckId);
}

#define FO_ASSERT(cond, msg)                                                \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            return TEST_FAIL;                                               \
        }                                                                   \
    } while (0)

namespace {

void OutboxTestPair(void) {
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0xC0FFEE22u;
    gComboCtx.sharedRandoSettingsHash = 0x5EED1222u;
}

// Fill every shared row with an un-redeemed crossing of `item`. Returns the
// number recorded (RSBS_SHARED_ITEM_CAP on success).
int FillOutbox(SharedItem item) {
    int recorded = 0;
    for (uint32_t i = 0; i < RSBS_SHARED_ITEM_CAP; i++) {
        if (Combo_RecordSharedItemCrossing((GameId)item.originGame, item.id) >= 0) {
            recorded++;
        }
    }
    return recorded;
}

// How many rows hold `item` (any origin flag), redeemed or not.
int RowsHolding(SharedItem item) {
    int n = 0;
    for (uint32_t i = 0; i < RSBS_SHARED_ITEM_CAP; i++) {
        const SharedItem& row = gComboCtx.sharedItemsTagged[i];
        if (row.originGame == item.originGame && row.id == item.id) {
            n++;
        }
    }
    return n;
}

} // namespace

TestResult Test_ForeignOutboxFullMM(void) {
    printf("[TEST] foreign-outbox-full-mm: a foreign host whose crossing the full outbox would refuse stays open, "
           "and its shop slot stays unsold (#1222)\n");

    SharedItem hammer;
    SharedItem boots;
    FO_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Megaton Hammer", &hammer), "named item Megaton Hammer");
    FO_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Hover Boots", &boots), "named item Hover Boots");

    // A foreign host that is not a shop (a chest or an NPC: CheckQueue's give),
    // and a shop slot (the counter's purchase), both from MM's real check table.
    uint16_t host = 0;
    uint16_t shopCheck = 0;
    const int checkIdMax = MM_Rando_Foreign_TestCheckIdMax();
    for (int id = 1; id < checkIdMax && (host == 0 || shopCheck == 0); id++) {
        int isShop = 0;
        int isTingle = 0;
        if (MM_Rando_Foreign_TestIsForeignHostClass((uint16_t)id) != 1 ||
            MM_Rando_Foreign_TestCheckShopClass((uint16_t)id, &isShop, &isTingle) != 1) {
            continue;
        }
        if (!isShop && !isTingle && host == 0) {
            host = (uint16_t)id;
        } else if (isShop && !isTingle && shopCheck == 0) {
            shopCheck = (uint16_t)id;
        }
    }
    FO_ASSERT(host != 0 && shopCheck != 0, "MM's check table has a non-shop foreign host and a shop slot");
    printf("[TEST]   host check %u, shop check %u\n", (unsigned)host, (unsigned)shopCheck);

    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    OutboxTestPair();
    FO_ASSERT(Combo_ForeignPairingActive(), "pairing live");
    FO_ASSERT(Combo_SetForeignPlacement(host, hammer) >= 0, "host placement accepted");
    FO_ASSERT(Combo_SetForeignPlacement(shopCheck, hammer) >= 0, "shop placement accepted");
    MM_Rando_Foreign_TestSetCheckBits(host, 0, 0, 1); // picked up: CheckQueue sees it eligible

    FO_ASSERT(FillOutbox(boots) == (int)RSBS_SHARED_ITEM_CAP, "every shared row recorded");
    FO_ASSERT(Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/false) == (int)RSBS_SHARED_ITEM_CAP,
              "the outbox holds 64 un-redeemed crossings");
    const uint32_t overflowBefore = Combo_GetSharedItemOverflowCount();

    // Every leg runs before any verdict, so a red run shows them all.
    // ---- Q: CheckQueue's hold ----------------------------------------------------
    const int held = MM_Rando_Foreign_TestQueueHolds(host);
    printf("[TEST]   Q CheckQueue holds the eligible host while the outbox is full: %d\n", held);

    // ---- G: the give itself ------------------------------------------------------
    int obtained = -1;
    int cycleObtained = -1;
    int eligible = -1;
    const int crossed = MM_Rando_Foreign_TestGiveForeignCheck(host, &obtained, &cycleObtained, &eligible);
    const uint32_t overflowAfter = Combo_GetSharedItemOverflowCount();
    const int hammerRows = RowsHolding(hammer);
    printf("[TEST]   G GiveForeignCheck on the full outbox: crossed=%d obtained=%d cycleObtained=%d eligible=%d; "
           "overflow count %u -> %u; rows holding the hammer=%d\n",
           crossed, obtained, cycleObtained, eligible, (unsigned)overflowBefore, (unsigned)overflowAfter, hammerRows);

    // ---- F: the shop -------------------------------------------------------------
    const int shop = MM_EnGirlA_TestForeignPurchaseOutboxFull(shopCheck);
    printf("[TEST]   F shop purchase on the full outbox: step code %d\n", shop);

    // ---- R: room again -----------------------------------------------------------
    // A crossing the other game redeemed is reclaimable (ReclaimOldestRedeemedSlot).
    gComboCtx.sharedItemsTagged[0].flags |= RSBS_SHARED_ITEM_REDEEMED;
    MM_Rando_Foreign_TestSetCheckBits(host, 0, 0, 1);
    const int heldWithRoom = MM_Rando_Foreign_TestQueueHolds(host);
    int obtainedR = -1;
    int cycleObtainedR = -1;
    int eligibleR = -1;
    const int crossedR = MM_Rando_Foreign_TestGiveForeignCheck(host, &obtainedR, &cycleObtainedR, &eligibleR);
    const int hammerRowsR = RowsHolding(hammer);
    printf("[TEST]   R one row redeemed: held=%d; give crossed=%d obtained=%d eligible=%d; rows holding the "
           "hammer=%d\n",
           heldWithRoom, crossedR, obtainedR, eligibleR, hammerRowsR);

    MM_Rando_Foreign_TestSetCheckBits(host, 0, 0, 0);
    Combo_ClearSharedItemOutbox();
    ComboContext_Init();

    FO_ASSERT(held == 1, "Q CheckQueue holds an eligible foreign host back while the outbox is full (see the Q line)");
    FO_ASSERT(crossed == 0 && hammerRows == 0, "G nothing crosses on a full outbox");
    FO_ASSERT(obtained == 0 && cycleObtained == 0,
              "G a refused give leaves the host open: not delivered, so its OoT item is not lost (see the G line)");
    FO_ASSERT(eligible == 1, "G a refused give keeps the host eligible, so CheckQueue delivers it once a row frees");
    FO_ASSERT(overflowAfter == overflowBefore + 1u, "G the refusal is counted once in the durable overflow count");
    FO_ASSERT(shop == 0, "F the shop refuses the sale and a forced buy charges nothing, crosses nothing and sells "
                         "nothing (see the F line; 1 = sold, 2 = charged, 4 = slot sold out, 5 = shelf emptied)");
    FO_ASSERT(heldWithRoom == 0 && crossedR == 1 && obtainedR == 1 && eligibleR == 0 && hammerRowsR == 1,
              "R once a row is free the held host delivers its crossing (see the R line)");

    printf("[TEST] PASS: a full outbox holds a foreign pickup back instead of losing it, and its shop slot stays "
           "unsold\n");
    return TEST_PASS;
}
