/**
 * @file test_foreign_outbox_full.c
 * @brief ROM-free locks for #1222: a foreign pickup the full shared outbox would
 *        refuse is held, not lost.
 *
 * THE OUTBOX. Every cross-game pickup is one row of the shared-item outbox
 * (RSBS_SHARED_ITEM_LOGICAL_CAP rows: gComboCtx.sharedItemsTagged's 64, then the
 * overflow segment, since 2026-10-27), redeemed when the other game next arrives.
 * When every row holds an item the other game has not redeemed yet, the record is
 * REFUSED (src/common/shared_items.c, AppendSharedItem): nothing is written and
 * the durable gComboCtx.sharedItemOverflowCount goes up. Before #1222 the MM give
 * marked the host delivered whatever the record answered (MiscBehavior/
 * CheckQueue.cpp, GiveForeignCheck), and a shop charged the price and sold the
 * slot before the answer was known (ActorBehavior/EnGirlA.cpp), so the OoT item
 * was lost for the file with nothing shown to the player.
 *
 * MM (Test_ForeignOutboxFullMM), with every row full:
 *  Q CHECKQUEUE HOLDS an eligible foreign host back (Rando::Foreign::
 *    ForeignOutboxFullFor): it stays `eligible` and delivers once a crossing
 *    frees a row.
 *  G THE GIVE ITSELF (GiveForeignCheck, which CheckQueue's lambda and a shop's
 *    purchase call) leaves a refused host open: not `obtained`, not
 *    `cycleObtained`, still `eligible`; the refusal is counted once.
 *  F THE SHOP: the counter refuses the sale; a buy forced past it charges
 *    nothing, crosses nothing, and leaves the slot unsold and the shelf standing.
 *  T THE PLAYER IS TOLD: the refused sale shows a toast naming the full outbox
 *    ("Outbox full: ..."), through the shared overlay.
 *  M TINGLE'S MAP SLOT: his "already have" (TingleMapSlotSold) refuses the sale
 *    while the outbox is full (his price is charged before the arm, so a sale
 *    that would only be held is refused first), and refuses a second sale of a
 *    slot whose purchase is held (`eligible`), also once a row is free: one
 *    purchase, one charge, one crossing.
 *  R ROOM AGAIN: with one row redeemed (reclaimable), the same host is no longer
 *    held and its give delivers.
 *
 * OoT (Test_ForeignOutboxFullOoT), the same hole in the other direction: the
 * RC-queue drain (hook_handlers.cpp) recorded through OoT_Rando_Foreign_
 * RecordPickup and, on a refusal, fell through to the ordinary give of the junk
 * cover, which marks the check collected; the MM item was lost. With every row
 * full:
 *  H THE DRAIN HOLDS the check (OoT_Rando_Foreign_HoldPickup): nothing recorded,
 *    nothing counted, asked twice still one hold;
 *  W while the rows stay full nothing is handed back;
 *  R with one row redeemed the held check is handed back once
 *    (OoT_Rando_Foreign_TakeReleasedHolds) and its record then succeeds.
 *  C A FILE LOAD FORGETS THE HOLDS (OoT_Rando_Foreign_ClearHolds, called where
 *    OnLoadGame resets the drain's queue): a check held in one file is never
 *    handed to the next file's drain once that file has room.
 *
 * The MM row is wrapped in test_runner.cpp (Test_ForeignOutboxFullMM): the shop
 * leg's toast reaches the overlay's Emit, which reads the CVar store, so the row
 * needs the display-free shared Context bring-up and mutes the overlay.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * beside test_foreign_award.c; every symbol it drives is C-linkage.
 */

#include "../context.h"
#include "../foreign_items.h"
#include "../notification_bridge.h" // OoT_Notification_RecordForTest: the toasts a leg emits
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
uint16_t MM_EnBal_TestTingleMapCheck(void);
int MM_EnBal_TestTingleSlotSold(uint16_t randoCheckId);
}

#define FO_ASSERT(cond, msg)                                                \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            return TEST_FAIL;                                               \
        }                                                                   \
    } while (0)

namespace {

// How many recorded toasts name the full outbox.
int OutboxToasts(void) {
    int n = 0;
    char toast[256];
    for (int i = 0; i < OoT_Notification_EmittedCountForTest(); i++) {
        if (OoT_Notification_EmittedAtForTest(i, toast, sizeof(toast)) == 1 && std::strstr(toast, "Outbox full")) {
            n++;
        }
    }
    return n;
}

void OutboxTestPair(void) {
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0xC0FFEE22u;
    gComboCtx.sharedRandoSettingsHash = 0x5EED1222u;
}

// Fill every shared row (both segments) with an un-redeemed crossing of `item`.
// Returns the number recorded (RSBS_SHARED_ITEM_LOGICAL_CAP on success).
int FillOutbox(SharedItem item) {
    int recorded = 0;
    for (uint32_t i = 0; i < RSBS_SHARED_ITEM_LOGICAL_CAP; i++) {
        if (Combo_RecordSharedItemCrossing((GameId)item.originGame, item.id) >= 0) {
            recorded++;
        }
    }
    return recorded;
}

// How many rows hold `item` (any origin flag), redeemed or not.
int RowsHolding(SharedItem item) {
    int n = 0;
    for (int i = 0; i < (int)RSBS_SHARED_ITEM_LOGICAL_CAP; i++) {
        const SharedItem& row = *Combo_SharedItemSlot(i);
        if (row.originGame == item.originGame && row.id == item.id) {
            n++;
        }
    }
    return n;
}

} // namespace

static TestResult ForeignOutboxFullMM_Run(void) {
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
    const uint16_t tingleCheck = MM_EnBal_TestTingleMapCheck();
    printf("[TEST]   host check %u, shop check %u, Tingle map check %u\n", (unsigned)host, (unsigned)shopCheck,
           (unsigned)tingleCheck);

    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    OutboxTestPair();
    FO_ASSERT(Combo_ForeignPairingActive(), "pairing live");
    FO_ASSERT(Combo_SetForeignPlacement(host, hammer) >= 0, "host placement accepted");
    FO_ASSERT(Combo_SetForeignPlacement(shopCheck, hammer) >= 0, "shop placement accepted");
    FO_ASSERT(Combo_SetForeignPlacement(tingleCheck, hammer) >= 0, "Tingle placement accepted");
    MM_Rando_Foreign_TestSetCheckBits(host, 0, 0, 1); // picked up: CheckQueue sees it eligible

    FO_ASSERT(FillOutbox(boots) == (int)RSBS_SHARED_ITEM_LOGICAL_CAP, "every shared row recorded");
    FO_ASSERT(Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/false) == (int)RSBS_SHARED_ITEM_LOGICAL_CAP,
              "the outbox holds a full logical array of un-redeemed crossings");
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
    OoT_Notification_RecordForTest(1);
    const int shop = MM_EnGirlA_TestForeignPurchaseOutboxFull(shopCheck);
    const int shopToasts = OutboxToasts();
    OoT_Notification_RecordForTest(0);
    printf("[TEST]   F shop purchase on the full outbox: step code %d; outbox toasts %d\n", shop, shopToasts);

    // ---- M: Tingle's map slot ----------------------------------------------------
    MM_Rando_Foreign_TestSetCheckBits(tingleCheck, 0, 0, 0);
    const int tingleRefusedFull = MM_EnBal_TestTingleSlotSold(tingleCheck);
    MM_Rando_Foreign_TestSetCheckBits(tingleCheck, 0, 0, 1); // bought, its crossing held
    const int tingleRefusedHeld = MM_EnBal_TestTingleSlotSold(tingleCheck);
    printf("[TEST]   M Tingle map slot on the full outbox: sale refused=%d; bought and held, a second sale "
           "refused=%d\n",
           tingleRefusedFull, tingleRefusedHeld);

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
    const int tingleRefusedHeldR = MM_EnBal_TestTingleSlotSold(tingleCheck); // still `eligible` from M
    printf("[TEST]   R one row redeemed: held=%d; give crossed=%d obtained=%d eligible=%d; rows holding the "
           "hammer=%d; M the held Tingle slot's second sale refused=%d\n",
           heldWithRoom, crossedR, obtainedR, eligibleR, hammerRowsR, tingleRefusedHeldR);

    MM_Rando_Foreign_TestSetCheckBits(host, 0, 0, 0);
    MM_Rando_Foreign_TestSetCheckBits(tingleCheck, 0, 0, 0);
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
    FO_ASSERT(shopToasts >= 1, "T the refused sale tells the player the outbox is full (an \"Outbox full\" toast)");
    FO_ASSERT(tingleRefusedFull == 1, "M Tingle refuses a map slot's sale while the outbox is full, before his price "
                                      "is charged (see the M line)");
    FO_ASSERT(
        tingleRefusedHeld == 1 && tingleRefusedHeldR == 1,
        "M a Tingle map slot whose purchase is held cannot be bought (and charged) again (see the M and R lines)");
    FO_ASSERT(heldWithRoom == 0 && crossedR == 1 && obtainedR == 1 && eligibleR == 0 && hammerRowsR == 1,
              "R once a row is free the held host delivers its crossing (see the R line)");

    printf("[TEST] PASS: a full outbox holds a foreign pickup back instead of losing it, and its shop slot stays "
           "unsold\n");
    return TEST_PASS;
}

TestResult Test_ForeignOutboxFullOoT(void) {
    printf("[TEST] foreign-outbox-full-oot: OoT's drain holds a pickup the full outbox would refuse and hands it back "
           "once a row is free (#1222)\n");

    SharedItem mmLens;
    SharedItem boots;
    FO_ASSERT(TestNamedItem((uint8_t)GAME_MM, "Lens of Truth", &mmLens), "named item Lens of Truth (MM)");
    FO_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Hover Boots", &boots), "named item Hover Boots");
    // An OoT check id the placement table stores opaquely (test_foreign_items.c's
    // reverse legs use the same one).
    const uint16_t kOoTHost = 0x0123;
    const uint16_t kOoTOtherHost = 0x0124; // leg C's: held in one file, then the file is left
    uint16_t released[4] = { 0, 0, 0, 0 };

    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    OutboxTestPair();
    FO_ASSERT(Combo_ForeignPairingActive(), "pairing live");
    FO_ASSERT(Combo_SetForeignPlacementOoT(kOoTHost, mmLens) >= 0, "OoT host placement accepted");
    FO_ASSERT(Combo_SetForeignPlacementOoT(kOoTOtherHost, mmLens) >= 0, "leg C's OoT host placement accepted");
    OoT_Rando_Foreign_ClearHolds(); // no hold left by an earlier row
    FO_ASSERT(FillOutbox(boots) == (int)RSBS_SHARED_ITEM_LOGICAL_CAP, "every shared row recorded");
    const uint32_t overflowBefore = Combo_GetSharedItemOverflowCount();

    // ---- H: the drain holds it ---------------------------------------------------
    const int held = OoT_Rando_Foreign_HoldPickup(kOoTHost);
    const int heldAgain = OoT_Rando_Foreign_HoldPickup(kOoTHost);
    const int lensRows = RowsHolding(mmLens);
    const uint32_t overflowHeld = Combo_GetSharedItemOverflowCount();
    // ---- W: still full -----------------------------------------------------------
    const int releasedWhileFull = OoT_Rando_Foreign_TakeReleasedHolds(released, 4);
    printf("[TEST]   H drain on the full outbox: held=%d, asked again=%d; rows holding the lens=%d; overflow %u -> "
           "%u; W handed back while full=%d\n",
           held, heldAgain, lensRows, (unsigned)overflowBefore, (unsigned)overflowHeld, releasedWhileFull);

    // ---- R: room again -----------------------------------------------------------
    gComboCtx.sharedItemsTagged[0].flags |= RSBS_SHARED_ITEM_REDEEMED;
    std::memset(released, 0, sizeof(released));
    const int releasedWithRoom = OoT_Rando_Foreign_TakeReleasedHolds(released, 4);
    const uint16_t releasedCheck = released[0];
    const int releasedTwice = OoT_Rando_Foreign_TakeReleasedHolds(released, 4);
    const int heldWithRoom = OoT_Rando_Foreign_HoldPickup(kOoTHost);
    const int recorded = OoT_Rando_Foreign_RecordPickup(kOoTHost);
    const int lensRowsR = RowsHolding(mmLens);
    printf("[TEST]   R one row redeemed: handed back=%d (check %u), again=%d; held=%d; record=%d; rows holding the "
           "lens=%d\n",
           releasedWithRoom, (unsigned)releasedCheck, releasedTwice, heldWithRoom, recorded, lensRowsR);

    // ---- C: a file load forgets the holds ----------------------------------------
    // The record above took the freed row, so every row is un-redeemed again.
    const int heldOther = OoT_Rando_Foreign_HoldPickup(kOoTOtherHost);
    OoT_Rando_Foreign_ClearHolds(); // OnLoadGame: the player left for file select and loaded a file
    gComboCtx.sharedItemsTagged[1].flags |= RSBS_SHARED_ITEM_REDEEMED; // the loaded file has room
    std::memset(released, 0, sizeof(released));
    const int releasedAfterLoad = OoT_Rando_Foreign_TakeReleasedHolds(released, 4);
    printf("[TEST]   C held in one file=%d; after a file load with room, handed back=%d (check %u)\n", heldOther,
           releasedAfterLoad, (unsigned)released[0]);

    OoT_Rando_Foreign_ClearHolds();
    Combo_ClearSharedItemOutbox();
    ComboContext_Init();

    FO_ASSERT(held == 1 && heldAgain == 1,
              "H OoT's drain holds an MM item whose crossing the full outbox would refuse (see the H line)");
    FO_ASSERT(lensRows == 0 && overflowHeld == overflowBefore, "H a hold records nothing and counts nothing");
    FO_ASSERT(releasedWhileFull == 0, "W nothing is handed back while every row is still full");
    FO_ASSERT(releasedWithRoom == 1 && releasedCheck == kOoTHost && releasedTwice == 0,
              "R once a row is free the held check is handed back to the drain, once (see the R line)");
    FO_ASSERT(heldWithRoom == 0 && recorded == 1 && lensRowsR == 1, "R with room the check is recorded as before");
    FO_ASSERT(heldOther == 1, "C the full outbox holds a second check (see the C line)");
    FO_ASSERT(releasedAfterLoad == 0, "C a file load forgets the held checks: none is handed to the next file's drain, "
                                      "which never opened it (see the C line)");

    printf("[TEST] PASS: OoT's drain holds a pickup the full outbox would refuse and hands it back with room\n");
    return TEST_PASS;
}
