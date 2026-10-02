/**
 * @file test_foreign_shop_mm.c
 * @brief #800 pass 1, MM side: an OoT item in an MM shop slot.
 *
 * ROM-free. An MM shop slot (RCTYPE_SHOP) may host a crossing, and every piece
 * of the shop that touches the slot must then deal in the OoT item, not in the
 * junk cover the slot physically holds (the MM engine's `place` writes RI_JUNK
 * for every OoT-origin item; the item itself lives in the placement tables):
 *
 *  S1 THE HOST CLASS: a shop slot is a foreign host (Rando::Foreign::
 *     IsForeignHostClass, the predicate the MM engine's hostAcceptsForeign and the
 *     spoiler-load gate both answer with). The full sweep lives in
 *     foreign-host-eligibility.
 *  S2 THE PURCHASE, PAIRED: the REAL shelf functions a shopkeeper calls
 *     (EnGirlA_RandoCanBuyFunc, EnGirlA_RandoBuyFunc, then the restock) on a slot
 *     hosting an OoT item. The buy charges the price and hands the item to the
 *     shared structure (one OoT-tagged CROSSING row), which OoT's real redeem walk
 *     then awards exactly once; the slot never restocks and refuses a second sale.
 *     Before #800 the buy handed MM's cover to Rando::GiveItem, which gives nothing
 *     for RI_JUNK, and the slot restocked the cover forever.
 *  S3 THE PURCHASE, UNPAIRED (#610): no live pairing, so no record is authored;
 *     the slot still sells once and stays sold.
 *  S4 THE SHELF: EnGirlA_RandoDrawFunc draws the origin's model (the descriptor,
 *     read back from a real MM GraphicsContext), and with no drawable model the
 *     model-less stand-in, never the cover's model.
 *  S5 THE BOMB SHOP OWNER'S HAND (EnSob1_DrawCustomItem): the same, for the one
 *     shop item drawn in an NPC's hand.
 *  S6 THE HAGS' MUSHROOM SLOT, RE-ARMED: its line (text 0x884) arms the slot
 *     for CheckQueue's give, and its weekly flags reset every cycle, so a later
 *     cycle's mushroom reaches it again. Hosting an OoT item already delivered,
 *     the line must not arm it again (it would offer and "find" the OoT item a
 *     second time while RecordForeignPickup refuses the crossing). A native slot
 *     still re-arms every cycle, as in 2Ship.
 *
 * #800 pass 2, Tingle's map slots (RCTYPE_TINGLE_SHOP):
 *  T1 THE HOST CLASS: a Tingle map slot is a foreign host.
 *  T2 THE PURCHASE, PAIRED: his offer names the OoT item; the sale is allowed
 *     once; the give his hook arms takes CheckQueue's foreign branch and authors
 *     one crossing; a second sale is refused, also on a later cycle. Before pass 2
 *     the slot's "already have" asked IsItemObtainable of the junk cover, which
 *     is always obtainable, so the OoT item was offered (and "found") again.
 *  T3 THE PURCHASE, UNPAIRED (#610): no record is authored; the slot stays sold.
 *
 * What S4/S5's stand-in legs do NOT prove: in this display-free process the
 * cover's own draw (Rando::CurrentJunkItem with no live frame count) emits no
 * model list either, so those two legs pass with or without #800. The model legs
 * are the ones a revert turns red. The real cover model (a rupee on a shelf
 * where the OoT item should be) is what the playtest shows.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++);
 * every symbol it drives is C-linkage.
 */

#include "../context.h"
#include "../foreign_items.h"
#include "../foreign_model.h"
#include "../shared_items.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>
#include <cstring>

extern "C" {
int MM_Rando_Foreign_TestIsForeignHostClass(uint16_t randoCheckId);
int MM_Rando_Foreign_TestCheckIdMax(void);
int MM_Rando_Foreign_TestCheckShopClass(uint16_t randoCheckId, int* outIsShop, int* outIsTingleShop);
int MM_EnGirlA_TestForeignPurchase(uint16_t randoCheckId, int paired);
uint16_t MM_EnGirlA_TestHagsMushroomCheck(void);
void MM_EnGirlA_TestHagsMushroomRearm(int* outFirstArm, int* outCrossed, int* outLaterArm);
uint16_t MM_EnBal_TestTingleMapCheck(void);
int MM_EnBal_TestForeignPurchase(uint16_t randoCheckId, int paired, const char* wantName);
void MM_ForeignModel_TestSetMountOverride(int value);
int MM_ForeignModel_TestShopDraw(uint16_t mmCheckId, int hand, const ComboModel* want);
uint16_t MM_ForeignModel_TestBombShopHandCheck(void);
int MM_ComboModel_TestForDrawRow(int drawId, ComboModel* out, const char** reason);
}

#define FS_ASSERT(cond, msg)                                                \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            return TEST_FAIL;                                               \
        }                                                                   \
    } while (0)

namespace {

struct ShopAwardCtx {
    int awardCount;
    uint16_t lastId;
};

void ShopTestAward(const SharedItem* item, void* ctx) {
    ShopAwardCtx* c = (ShopAwardCtx*)ctx;
    c->awardCount++;
    c->lastId = item->id;
}

void ShopTestPair(void) {
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0xC0FFEE80u;
    gComboCtx.sharedRandoSettingsHash = 0x5EED0800u;
}

} // namespace

TestResult Test_ForeignItemGiveShop(void) {
    printf("[TEST] foreign-item-give-shop: an OoT item in an MM shop slot is sold, delivered once and drawn (#800)\n");

    SharedItem hammer;
    SharedItem boots;
    SharedItem hookshot;
    FS_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Megaton Hammer", &hammer), "named item Megaton Hammer");
    FS_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Hover Boots", &boots), "named item Hover Boots");
    FS_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Progressive Hookshot", &hookshot), "named item Progressive Hookshot");
    ComboModelAnswer bootsA;
    FS_ASSERT(Combo_GetForeignItemModel((uint8_t)GAME_MM, boots, &bootsA) == COMBO_MODEL_ANSWER_DESCRIPTOR,
              "the Hover Boots are a DESCRIPTOR in MM");
    // #577 M5: OoT's hookshot collides, and MM's host-native table (#577 M7) maps
    // it to MM's own hookshot row; the shelf and the hand draw that row as MM's
    // own recipe gives it.
    ComboModelAnswer hookshotA;
    FS_ASSERT(Combo_GetForeignItemModel((uint8_t)GAME_MM, hookshot, &hookshotA) == COMBO_MODEL_ANSWER_HOST_NATIVE,
              "OoT's (colliding) hookshot is HOST_NATIVE in MM");
    ComboModel mmHookshot;
    const char* mmHookshotReason = nullptr;
    FS_ASSERT(MM_ComboModel_TestForDrawRow(hookshotA.hostKey, &mmHookshot, &mmHookshotReason) == 1,
              "MM draws the hookshot's host-native row with its own recipe");

    // A real shop slot of MM's check table (not the owner's hand item, which S5
    // covers on its own).
    const uint16_t handCheck = MM_ForeignModel_TestBombShopHandCheck();
    uint16_t shopCheck = 0;
    const int checkIdMax = MM_Rando_Foreign_TestCheckIdMax();
    for (int id = 1; id < checkIdMax && shopCheck == 0; id++) {
        int isShop = 0;
        if (MM_Rando_Foreign_TestCheckShopClass((uint16_t)id, &isShop, nullptr) == 1 && isShop &&
            (uint16_t)id != handCheck) {
            shopCheck = (uint16_t)id;
        }
    }
    FS_ASSERT(shopCheck != 0, "MM's check table has a shop slot");
    int handIsShop = 0;
    FS_ASSERT(MM_Rando_Foreign_TestCheckShopClass(handCheck, &handIsShop, nullptr) == 1 && handIsShop,
              "the Bomb Shop owner's hand item is a shop slot");
    printf("[TEST]   shop slot %u, hand slot %u\n", (unsigned)shopCheck, (unsigned)handCheck);

    // Every leg runs before any leg's verdict, so a red run shows them all.
    // ---- S1 ----------------------------------------------------------------------
    const int shopHost = MM_Rando_Foreign_TestIsForeignHostClass(shopCheck);
    const int handHost = MM_Rando_Foreign_TestIsForeignHostClass(handCheck);
    printf("[TEST]   S1 host class: shop slot %d, hand slot %d\n", shopHost, handHost);

    // ---- S2: paired --------------------------------------------------------------
    ComboContext_Init();
    Context_InitFrozenStates();
    Context_ClearAllFrozenStates();
    Combo_ClearSharedItemOutbox();
    ShopTestPair();
    FS_ASSERT(Combo_ForeignPairingActive(), "S2 pairing live");
    FS_ASSERT(Combo_SetForeignPlacement(shopCheck, hammer) >= 0, "S2 placement accepted");
    const int paired = MM_EnGirlA_TestForeignPurchase(shopCheck, 1);
    printf("[TEST]   S2 paired purchase: step code %d\n", paired);
    if (paired == 0) {
        ShopAwardCtx award;
        std::memset(&award, 0, sizeof(award));
        FS_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, ShopTestAward, &award) == 1,
                  "S2 OoT's redeem walk awards the bought item");
        FS_ASSERT(award.awardCount == 1 && award.lastId == hammer.id, "S2 the award is the Megaton Hammer, once");
        std::memset(&award, 0, sizeof(award));
        FS_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, ShopTestAward, &award) == 0 && award.awardCount == 0,
                  "S2 a second arrival awards nothing");
    }

    // ---- S3: unpaired (#610) -----------------------------------------------------
    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    FS_ASSERT(!Combo_ForeignPairingActive(), "S3 no pairing");
    FS_ASSERT(Combo_SetForeignPlacement(shopCheck, hammer) >= 0, "S3 placement accepted");
    const int unpaired = MM_EnGirlA_TestForeignPurchase(shopCheck, 0);
    printf("[TEST]   S3 unpaired purchase: step code %d\n", unpaired);

    // ---- S4 / S5: the shelf and the owner's hand ---------------------------------
    ComboContext_Init();
    MM_ForeignModel_TestSetMountOverride(1);
    FS_ASSERT(Combo_SetForeignPlacement(shopCheck, boots) >= 0, "S4 placement accepted");
    printf("[TEST]   S4 Hover Boots on a shop shelf, archive mounted:\n");
    const int shelfDrawn = MM_ForeignModel_TestShopDraw(shopCheck, 0, &bootsA.model);
    MM_ForeignModel_TestSetMountOverride(0);
    printf("[TEST]   S4 Hover Boots on a shop shelf, archive NOT mounted:\n");
    const int shelfStandIn = MM_ForeignModel_TestShopDraw(shopCheck, 0, nullptr);
    MM_ForeignModel_TestSetMountOverride(1);
    FS_ASSERT(Combo_SetForeignPlacement(handCheck, boots) >= 0, "S5 placement accepted");
    printf("[TEST]   S5 Hover Boots in the Bomb Shop owner's hand, archive mounted:\n");
    const int handDrawn = MM_ForeignModel_TestShopDraw(handCheck, 1, &bootsA.model);
    MM_ForeignModel_TestSetMountOverride(0);
    printf("[TEST]   S5 Hover Boots in the owner's hand, archive NOT mounted:\n");
    const int handStandIn = MM_ForeignModel_TestShopDraw(handCheck, 1, nullptr);
    MM_ForeignModel_TestSetMountOverride(1);
    // #577 M5: a colliding OoT model with a host-native row, on both surfaces.
    Combo_ClearForeignPlacements();
    FS_ASSERT(Combo_SetForeignPlacement(shopCheck, hookshot) >= 0, "S4 hookshot placement accepted");
    printf("[TEST]   S4 Progressive Hookshot (colliding, host-native row: MM's own hookshot) on a shop shelf:\n");
    const int shelfNative = MM_ForeignModel_TestShopDraw(shopCheck, 0, &mmHookshot);
    FS_ASSERT(Combo_SetForeignPlacement(handCheck, hookshot) >= 0, "S5 hookshot placement accepted");
    printf("[TEST]   S5 Progressive Hookshot (colliding, host-native row) in the owner's hand:\n");
    const int handNative = MM_ForeignModel_TestShopDraw(handCheck, 1, &mmHookshot);
    MM_ForeignModel_TestSetMountOverride(-1);
    ComboContext_Init();

    // ---- S6: the Hags' mushroom slot on a later cycle -------------------------------
    const uint16_t hagsCheck = MM_EnGirlA_TestHagsMushroomCheck();
    Combo_ClearSharedItemOutbox();
    ShopTestPair();
    FS_ASSERT(Combo_SetForeignPlacement(hagsCheck, hammer) >= 0, "S6 placement accepted");
    int hagsFirst = -1;
    int hagsCrossed = -1;
    int hagsLater = -1;
    MM_EnGirlA_TestHagsMushroomRearm(&hagsFirst, &hagsCrossed, &hagsLater);
    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    int nativeFirst = -1;
    int nativeCrossed = -1;
    int nativeLater = -1;
    MM_EnGirlA_TestHagsMushroomRearm(&nativeFirst, &nativeCrossed, &nativeLater);
    ComboContext_Init();

    // ---- T1-T3: Tingle's map slots (#800 pass 2) -------------------------------------
    const uint16_t tingleCheck = MM_EnBal_TestTingleMapCheck();
    int tingleIsShop = -1;
    int tingleIsTingle = 0;
    FS_ASSERT(MM_Rando_Foreign_TestCheckShopClass(tingleCheck, &tingleIsShop, &tingleIsTingle) == 1 && tingleIsTingle,
              "T the bridge's Tingle check is a Tingle map slot");
    const int tingleHost = MM_Rando_Foreign_TestIsForeignHostClass(tingleCheck);
    printf("[TEST]   T1 host class: Tingle map slot %u -> %d\n", (unsigned)tingleCheck, tingleHost);
    const char* hammerName = Combo_GetForeignItemName(hammer);
    Combo_ClearSharedItemOutbox();
    ShopTestPair();
    FS_ASSERT(Combo_SetForeignPlacement(tingleCheck, hammer) >= 0, "T2 placement accepted");
    const int tinglePaired = MM_EnBal_TestForeignPurchase(tingleCheck, 1, hammerName);
    printf("[TEST]   T2 paired Tingle purchase: step code %d\n", tinglePaired);
    if (tinglePaired == 0) {
        ShopAwardCtx award;
        std::memset(&award, 0, sizeof(award));
        FS_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, ShopTestAward, &award) == 1 && award.awardCount == 1 &&
                      award.lastId == hammer.id,
                  "T2 OoT's redeem walk awards the Megaton Hammer bought from Tingle, once");
    }
    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    FS_ASSERT(!Combo_ForeignPairingActive(), "T3 no pairing");
    FS_ASSERT(Combo_SetForeignPlacement(tingleCheck, hammer) >= 0, "T3 placement accepted");
    const int tingleUnpaired = MM_EnBal_TestForeignPurchase(tingleCheck, 0, hammerName);
    printf("[TEST]   T3 unpaired Tingle purchase: step code %d\n", tingleUnpaired);
    ComboContext_Init();

    FS_ASSERT(shopHost == 1, "S1 a shop slot is a foreign host class");
    FS_ASSERT(handHost == 1, "S1 the Bomb Shop owner's hand slot is a foreign host class");
    FS_ASSERT(paired == 0, "S2 buying the OoT item hands it to the shared structure once and sells the slot out "
                           "(see the S-line above; 2 = nothing crossed, 6 = restocked, 7 = sold twice)");
    FS_ASSERT(unpaired == 0, "S3 with no live pairing the purchase authors no record and the slot stays sold (see the "
                             "S-line above)");
    FS_ASSERT(shelfDrawn == 0, "S4 the shelf draws OoT's Hover Boots model (see the Q-line above)");
    FS_ASSERT(shelfStandIn == 0, "S4 with no drawable model the shelf keeps the model-less stand-in, never the cover");
    FS_ASSERT(handDrawn == 0, "S5 the owner's hand draws OoT's Hover Boots model (see the Q-line above)");
    FS_ASSERT(handStandIn == 0, "S5 with no drawable model the hand keeps the model-less stand-in, never the cover");
    FS_ASSERT(shelfNative == 0, "S4 a colliding OoT model with a host-native row draws MM's OWN model for that row on "
                                "the shelf (#577 M5; see the Q-line above)");
    FS_ASSERT(handNative == 0, "S5 a colliding OoT model with a host-native row draws MM's OWN model for that row in "
                               "the owner's hand (#577 M5; see the Q-line above)");
    FS_ASSERT(hagsFirst == 1 && hagsCrossed == 1, "S6 the first cycle's mushroom arms the Hags' slot and its OoT item "
                                                  "crosses (see the S6 lines above)");
    FS_ASSERT(hagsLater == 0, "S6 a later cycle's mushroom does not re-arm a Hags' slot whose OoT item was delivered "
                              "(see the S6 lines above)");
    FS_ASSERT(nativeFirst == 1 && nativeCrossed == 0 && nativeLater == 1,
              "S6 a native Hags' slot still re-arms every cycle (see the S6 lines above)");
    FS_ASSERT(tingleHost == 1, "T1 a Tingle map slot is a foreign host class");
    FS_ASSERT(tinglePaired == 0, "T2 Tingle's offer names the OoT item, sells it once and hands it to the shared "
                                 "structure (see the T-line above; 1 = offer names the cover, 4 = nothing crossed, "
                                 "6 = sold twice)");
    FS_ASSERT(tingleUnpaired == 0, "T3 with no live pairing a Tingle purchase authors no record and the slot stays "
                                   "sold (see the T-line above)");

    Context_ClearAllFrozenStates();
    Combo_ClearSharedItemOutbox();
    ComboContext_Init();
    printf("[TEST] PASS: an OoT item in an MM shop slot or a Tingle map slot is sold once, crosses once, and is "
           "presented as itself\n");
    return TEST_PASS;
}

#undef FS_ASSERT
