/**
 * @file oot_shared_masks_test.cpp
 * OoT's half of the shared-masks lock (#978; the rules are
 * RSBS_XITEM_SHARED_GORON, _ZORA, _BUNNY, _KEATON and _TRUTH in
 * src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship row `shared-masks-merge`
 * (src/common/tests/test_shared_masks.c) drives OoT's REAL
 * OoT_HarvestSharedResources / OoT_ApplySharedResources through, and the logic
 * probe the rando row `shared-masks-bag` reads. They only put a mask's
 * ownership in a known state and read it back; every merge decision is the
 * production shims' and the registered family member's. They live here because
 * only an OoT TU has OoT's `gSaveContext`, its inventory macros and its logic in
 * scope. The live save itself is armed by OoT_SharedSwordTest_ArmLive
 * (oot_shared_kokiri_sword_test.cpp).
 *
 * OoT owns a shuffled mask through its RAND_INF_CHILD_TRADES_HAS_MASK_* flag
 * (randomizer.cpp's get-item, and logic.cpp's RandoGetToRandInf), and its
 * get-item also puts the mask in the child trade slot when that slot is empty.
 * `mask` is 0 for the Goron Mask, 1 the Zora Mask, 2 the Bunny Hood, 3 the
 * Keaton Mask and 4 the Mask of Truth (the RSBS_XITEM_* bits' order).
 */

#include <cstring>
#include <memory>

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (oot_shared_kokiri_sword_test.cpp says why).
#include <z64.h>
#include "variables.h"
// INV_CONTENT / SLOT. After z64.h, as the other OoT TUs include it.
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"

extern "C" s32 Flags_GetRandomizerInf(RandomizerInf flag);
extern "C" void Flags_SetRandomizerInf(RandomizerInf flag);
extern "C" void Flags_UnsetRandomizerInf(RandomizerInf flag);

namespace {
struct OoTMaskIds {
    RandomizerGet rg;
    RandomizerInf flag;
    uint8_t item;
};
OoTMaskIds OoTMask(int mask) {
    switch (mask) {
        case 0:
            return { RG_GORON_MASK, RAND_INF_CHILD_TRADES_HAS_MASK_GORON, (uint8_t)ITEM_MASK_GORON };
        case 1:
            return { RG_ZORA_MASK, RAND_INF_CHILD_TRADES_HAS_MASK_ZORA, (uint8_t)ITEM_MASK_ZORA };
        case 2:
            return { RG_BUNNY_HOOD, RAND_INF_CHILD_TRADES_HAS_MASK_BUNNY, (uint8_t)ITEM_MASK_BUNNY };
        case 3:
            return { RG_KEATON_MASK, RAND_INF_CHILD_TRADES_HAS_MASK_KEATON, (uint8_t)ITEM_MASK_KEATON };
        default:
            return { RG_MASK_OF_TRUTH, RAND_INF_CHILD_TRADES_HAS_MASK_TRUTH, (uint8_t)ITEM_MASK_TRUTH };
    }
}
} // namespace

/** Give (1) or take (0) OoT's ownership of the mask: its RAND_INF flag, which
 *  is what OoT's own get-item sets for a shuffled mask. */
extern "C" void OoT_SharedMasksTest_SetHeld(int mask, int held) {
    if (held != 0) {
        Flags_SetRandomizerInf(OoTMask(mask).flag);
    } else {
        Flags_UnsetRandomizerInf(OoTMask(mask).flag);
    }
}

/** 1 when OoT owns the mask (its RAND_INF flag), else 0. */
extern "C" int OoT_SharedMasksTest_Held(int mask) {
    return Flags_GetRandomizerInf(OoTMask(mask).flag) ? 1 : 0;
}

/** OoT's pool id for the mask: the RG_* its pool places it as. */
extern "C" int OoT_SharedMasksTest_PoolId(int mask) {
    return (int)OoTMask(mask).rg;
}

/** The mask's OoT item id (ITEM_MASK_*), what the child trade slot holds. */
extern "C" int OoT_SharedMasksTest_ItemOf(int mask) {
    return (int)OoTMask(mask).item;
}

/** Put the mask's item in the child trade slot, or empty it (@p mask < 0). */
extern "C" void OoT_SharedMasksTest_SetTradeSlot(int mask) {
    INV_CONTENT(ITEM_TRADE_CHILD) = (mask < 0) ? (uint8_t)ITEM_NONE : OoTMask(mask).item;
}

/** The child trade slot's item, or -1 when it is empty. */
extern "C" int OoT_SharedMasksTest_TradeSlot(void) {
    const uint8_t item = INV_CONTENT(ITEM_TRADE_CHILD);
    return item == (uint8_t)ITEM_NONE ? -1 : (int)item;
}

/**
 * OoT's logic for the mask: HasItem over a real Rando::Logic whose save is a
 * private zeroed one with only the mask's ownership flag set (@p held) or not,
 * under the generated world's randomizer context (Mask Quest "Shuffle" on every
 * paired world, #973). 1 or 0; -1 with no randomizer context to read.
 */
extern "C" int OoT_SharedMasksTest_LogicHasMask(int mask, int held) {
    std::shared_ptr<Rando::Context> ctx = Rando::Context::GetInstance();
    if (ctx == nullptr) {
        return -1;
    }
    auto save = std::make_unique<SaveContext>();
    std::memset(save.get(), 0, sizeof(SaveContext));
    Rando::Logic probe;
    probe.SetContext(ctx);
    probe.SetSaveContext(save.get());
    probe.SetRandoInf((uint32_t)OoTMask(mask).flag, held != 0);
    return probe.HasItem(OoTMask(mask).rg) ? 1 : 0;
}
