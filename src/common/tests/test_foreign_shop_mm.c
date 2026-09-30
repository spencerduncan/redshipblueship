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
void MM_ForeignModel_TestSetMountOverride(int value);
int MM_ForeignModel_TestShopDraw(uint16_t mmCheckId, int hand, const ComboModel* want);
uint16_t MM_ForeignModel_TestBombShopHandCheck(void);
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
    FS_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Megaton Hammer", &hammer), "named item Megaton Hammer");
    FS_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Hover Boots", &boots), "named item Hover Boots");
    ComboModelAnswer bootsA;
    FS_ASSERT(Combo_GetForeignItemModel((uint8_t)GAME_MM, boots, &bootsA) == COMBO_MODEL_ANSWER_DESCRIPTOR,
              "the Hover Boots are a DESCRIPTOR in MM");

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

    // ---- S1 ----------------------------------------------------------------------
    FS_ASSERT(MM_Rando_Foreign_TestIsForeignHostClass(shopCheck) == 1, "S1 a shop slot is a foreign host class");
    FS_ASSERT(MM_Rando_Foreign_TestIsForeignHostClass(handCheck) == 1,
              "S1 the Bomb Shop owner's hand slot is a foreign host class");

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
    FS_ASSERT(paired == 0, "S2 buying the OoT item hands it to the shared structure once and sells the slot out "
                           "(see the S-line above; 2 = nothing crossed, 6 = restocked, 7 = sold twice)");
    {
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
    FS_ASSERT(unpaired == 0, "S3 with no live pairing the purchase authors no record and the slot stays sold (see the "
                             "S-line above)");

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
    MM_ForeignModel_TestSetMountOverride(-1);
    ComboContext_Init();

    FS_ASSERT(shelfDrawn == 0, "S4 the shelf draws OoT's Hover Boots model (see the Q-line above)");
    FS_ASSERT(shelfStandIn == 0, "S4 with no drawable model the shelf keeps the model-less stand-in, never the cover");
    FS_ASSERT(handDrawn == 0, "S5 the owner's hand draws OoT's Hover Boots model (see the Q-line above)");
    FS_ASSERT(handStandIn == 0, "S5 with no drawable model the hand keeps the model-less stand-in, never the cover");

    Context_ClearAllFrozenStates();
    Combo_ClearSharedItemOutbox();
    ComboContext_Init();
    printf("[TEST] PASS: an OoT item in an MM shop slot is sold once, crosses once, and is drawn as itself\n");
    return TEST_PASS;
}

#undef FS_ASSERT
