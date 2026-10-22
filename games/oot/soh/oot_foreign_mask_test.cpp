/**
 * @file oot_foreign_mask_test.cpp
 * OoT's half of the cross-game worn-mask locks (#977; the shared rules are
 * src/common/foreign_masks.h, OoT's are soh/Enhancements/randomizer/ForeignMaskOoT.cpp).
 *
 * THE HELPERS the redship rows `foreign-mask-rules`, `foreign-mask-persistence`
 * and `oot-child-trade-cycle` (src/common/tests/test_foreign_masks.c) drive OoT's
 * REAL rule functions, freeze/restore calls and trade cycle through. They only
 * put OoT's live save and a heap Player in a known state and read them back;
 * every decision is the production code's. They live here because only an OoT
 * TU has OoT's `gSaveContext`, `Player` and inventory macros in scope.
 */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ and GCC refuses C++ templates with C linkage. Its include guard makes the
// nested include in functions.h a no-op.
#include <z64.h>
extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
}

#include "soh/Enhancements/randomizer/ForeignMaskOoT.h"
extern "C" {
#include "soh/Enhancements/randomizer/ShuffleTradeItems.h" // C definitions, no linkage guard of its own
}

#include "context.h"  // src/common: Combo_FreezeState, Context_RestoreState
#include "entrance.h" // src/common: OOT_ENTR_MARKET_FROM_MASK_SHOP

extern "C" void OoT_ChildTradeCycle_ClearExtrasForTest(void);

namespace {
// A heap Player: the rule functions read and write only currentMask.
std::unique_ptr<Player> sPlayer;

Player* TestPlayer() {
    if (!sPlayer) {
        sPlayer = std::make_unique<Player>();
        std::memset(sPlayer.get(), 0, sizeof(Player));
    }
    return sPlayer.get();
}

constexpr int kChildTradeCount = ITEM_MASK_TRUTH - ITEM_WEIRD_EGG + 1;
} // namespace

/** OoT's live save as a real loaded child file (GAMEMODE_NORMAL), all else zero. */
extern "C" void OoT_ForeignMaskTest_ArmLive(void) {
    std::memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.linkAge = LINK_AGE_CHILD;
    INV_CONTENT(ITEM_TRADE_CHILD) = ITEM_NONE;
}

/** Write the worn field's raw byte, as a save or a corrupt blob could hold it. */
extern "C" void OoT_ForeignMaskTest_SetStored(uint8_t value) {
    gSaveContext.ship.foreignMask = value;
}

/** The worn field's raw byte. */
extern "C" uint8_t OoT_ForeignMaskTest_Stored(void) {
    return gSaveContext.ship.foreignMask;
}

extern "C" void OoT_ForeignMaskTest_SetAdult(int adult) {
    gSaveContext.linkAge = adult ? LINK_AGE_ADULT : LINK_AGE_CHILD;
}

extern "C" void OoT_ForeignMaskTest_SetMaskMemory(uint8_t mask) {
    gSaveContext.ship.maskMemory = mask;
}

extern "C" uint8_t OoT_ForeignMaskTest_MaskMemory(void) {
    return gSaveContext.ship.maskMemory;
}

extern "C" void OoT_ForeignMaskTest_SetTradeSlot(uint8_t item) {
    INV_CONTENT(ITEM_TRADE_CHILD) = item;
}

/** The test Player's native mask (currentMask). */
extern "C" void OoT_ForeignMaskTest_SetPlayerMask(uint8_t mask) {
    TestPlayer()->currentMask = mask;
}

extern "C" int OoT_ForeignMaskTest_PlayerMask(void) {
    return (int)TestPlayer()->currentMask;
}

/** OoT's own bound on currentMask: PLAYER_MASK_MAX (sMaskDlists has MAX - 1 rows). */
extern "C" int OoT_ForeignMaskTest_PlayerMaskMax(void) {
    return (int)PLAYER_MASK_MAX;
}

extern "C" int OoT_ForeignMaskTest_Use(uint8_t mask) {
    return OoT_ForeignMask_Use(TestPlayer(), mask);
}

extern "C" void OoT_ForeignMaskTest_ApplyRules(void) {
    OoT_ForeignMask_ApplyRules(TestPlayer());
}

extern "C" void OoT_ForeignMaskTest_PlayerInit(int persistentMasks) {
    OoT_ForeignMask_OnPlayerInit(TestPlayer(), persistentMasks);
}

/** The worn field's offset and SaveContext's size: the field is appended. */
extern "C" size_t OoT_ForeignMaskTest_FieldOffset(void) {
    return offsetof(SaveContext, ship) + offsetof(ShipSaveContextData, foreignMask);
}

extern "C" size_t OoT_ForeignMaskTest_PrevLastOffset(void) {
    return offsetof(SaveContext, ship) + offsetof(ShipSaveContextData, randomizerInf);
}

extern "C" size_t OoT_ForeignMaskTest_SaveSize(void) {
    return sizeof(SaveContext);
}

/** A crossing's freeze of the live OoT half, as Combo_CheckEntranceSwitch takes it. */
extern "C" void OoT_ForeignMaskTest_Freeze(void) {
    Combo_FreezeState("oot", OOT_ENTR_MARKET_FROM_MASK_SHOP, &gSaveContext, sizeof(gSaveContext));
}

/** OoT_Game_Resume's restore of the frozen half into the live save. */
extern "C" int OoT_ForeignMaskTest_Restore(void) {
    return Context_RestoreState(GAME_OOT, &gSaveContext, sizeof(gSaveContext));
}

// ---- The child trade cycle --------------------------------------------------

/** Put `item` in the child trade slot and own exactly the trade items whose bit
 *  (item - ITEM_WEIRD_EGG) is set in `ownedBits`. */
extern "C" void OoT_TradeCycleTest_Set(uint8_t item, uint16_t ownedBits) {
    INV_CONTENT(ITEM_TRADE_CHILD) = item;
    for (int i = 0; i < kChildTradeCount; i++) {
        const RandomizerInf flag = (RandomizerInf)(RAND_INF_CHILD_TRADES_HAS_WEIRD_EGG + i);
        if (ownedBits & (1u << i)) {
            Flags_SetRandomizerInf(flag);
        } else {
            Flags_UnsetRandomizerInf(flag);
        }
    }
}

/** The production cycle the pause screen calls (ShuffleTradeItems.c). */
extern "C" uint8_t OoT_TradeCycleTest_Next(void) {
    return Randomizer_GetNextChildTradeItem();
}

extern "C" uint8_t OoT_TradeCycleTest_Prev(void) {
    return Randomizer_GetPrevChildTradeItem();
}

/**
 * The ORACLE: ShuffleTradeItems.c's contiguous cycle as it stood before #977,
 * verbatim (it is still that file's #else branch).
 */
extern "C" uint8_t OoT_TradeCycleTest_LegacyNext(void) {
    const u8 numTradeItems = ITEM_MASK_TRUTH - ITEM_WEIRD_EGG + 1;
    u8 currentTradeItemIndex = INV_CONTENT(ITEM_TRADE_CHILD) - ITEM_WEIRD_EGG;
    for (int i = 0; i < numTradeItems; i++) {
        u8 tradeIndex = (currentTradeItemIndex + i + 1) % numTradeItems;
        if (Flags_GetRandomizerInf((RandomizerInf)(tradeIndex + RAND_INF_CHILD_TRADES_HAS_WEIRD_EGG))) {
            return ITEM_WEIRD_EGG + tradeIndex;
        }
    }
    return ITEM_NONE;
}

extern "C" uint8_t OoT_TradeCycleTest_LegacyPrev(void) {
    const u8 numTradeItems = ITEM_MASK_TRUTH - ITEM_WEIRD_EGG + 1;
    u8 currentTradeItemIndex = INV_CONTENT(ITEM_TRADE_CHILD) - ITEM_WEIRD_EGG;
    for (int i = 0; i < numTradeItems; i++) {
        u8 tradeIndex = (currentTradeItemIndex - i - 1 + numTradeItems) % numTradeItems;
        if (Flags_GetRandomizerInf((RandomizerInf)(tradeIndex + RAND_INF_CHILD_TRADES_HAS_WEIRD_EGG))) {
            return ITEM_WEIRD_EGG + tradeIndex;
        }
    }
    return ITEM_NONE;
}

extern "C" int OoT_TradeCycleTest_ChildTradeCount(void) {
    return kChildTradeCount;
}

extern "C" uint8_t OoT_TradeCycleTest_FirstItem(void) {
    return ITEM_WEIRD_EGG;
}

extern "C" uint8_t OoT_TradeCycleTest_ItemNone(void) {
    return ITEM_NONE;
}

/** The first custom ItemID after Roc's Feather: what P4's first mask will be. */
extern "C" uint8_t OoT_TradeCycleTest_FirstFreeCustomItem(void) {
    return (uint8_t)(ITEM_ROCS_FEATHER + 1);
}

extern "C" int OoT_TradeCycleTest_RegisterExtra(uint8_t itemId, OoTChildTradeOwnedFn owned) {
    return OoT_ChildTradeCycle_RegisterExtra(itemId, owned);
}

extern "C" void OoT_TradeCycleTest_ClearExtras(void) {
    OoT_ChildTradeCycle_ClearExtrasForTest();
}
