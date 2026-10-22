/**
 * @file ForeignMaskOoT.cpp
 * @brief OoT wears and draws a Majora's Mask mask (#977, epic #975 P3).
 *
 * [lock] SEAM: today's behaviour. OoT has no worn-mask field yet, so nothing is
 * worn, put on or kept, and the child trade slot cycles OoT's eleven contiguous
 * items exactly as ShuffleTradeItems.c does, with no room for a custom ItemID.
 * The fix commit replaces these bodies.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstddef>
#include <cstdint>

#include "ForeignMaskOoT.h"
#include "foreign_masks.h" // src/common

// At file scope first: z64.h includes <memory> under C++, which must not get C
// linkage. Its include guard makes the nested include below a no-op.
#include <z64.h>
extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
}

extern "C" uint8_t OoT_ForeignMask_Worn(void) {
    return COMBO_FOREIGN_MASK_NONE;
}

extern "C" int OoT_ForeignMask_Use(Player* player, uint8_t mask) {
    (void)player;
    (void)mask;
    return 0;
}

extern "C" void OoT_ForeignMask_ApplyRules(Player* player) {
    (void)player;
}

extern "C" void OoT_ForeignMask_OnPlayerInit(Player* player, int persistentMasks) {
    (void)player;
    (void)persistentMasks;
}

extern "C" void OoT_ForeignMask_DrawOnHead(PlayState* play, Player* player, int headDrawn) {
    (void)play;
    (void)player;
    (void)headDrawn;
}

extern "C" uint8_t OoT_ChildTradeCycle_Next(void) {
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

extern "C" uint8_t OoT_ChildTradeCycle_Prev(void) {
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

extern "C" int OoT_ChildTradeCycle_RegisterExtra(uint8_t itemId, OoTChildTradeOwnedFn owned) {
    (void)itemId;
    (void)owned;
    return 0;
}

extern "C" void OoT_ChildTradeCycle_ClearExtrasForTest(void) {
}

extern "C" uint8_t OoT_ForeignMask_StoredRaw(void) {
    return COMBO_FOREIGN_MASK_NONE;
}

extern "C" void OoT_ForeignMask_SetStoredRaw(uint8_t value) {
    (void)value;
}

extern "C" size_t OoT_ForeignMask_FieldOffset(void) {
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
