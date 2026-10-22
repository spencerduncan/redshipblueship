/**
 * @file mm_shared_masks_test.cpp
 * MM's half of the shared-masks lock (#978; the rules are
 * RSBS_XITEM_SHARED_GORON, _ZORA, _BUNNY, _KEATON and _TRUTH in
 * src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship row `shared-masks-merge`
 * (src/common/tests/test_shared_masks.c) drives MM's REAL
 * MM_HarvestSharedResources / MM_ApplySharedResources through. They only put a
 * mask's inventory slot in a known state and read it back; every merge
 * decision is the production shims' and the registered family member's. MM's
 * masks survive the Song of Time. `mask` is 0 for the Goron Mask, 1 the Zora
 * Mask, 2 the Bunny Hood, 3 the Keaton Mask and 4 the Mask of Truth (the
 * RSBS_XITEM_* bits' order).
 */

#include "global.h"

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The shared masks are a single-exe surface; test_runner.cpp declares these
// unconditionally, so a standalone 2ship build links.
extern "C" void MM_SharedMasksTest_SetHeld(int mask, int held) {
    (void)mask;
    (void)held;
}
extern "C" int MM_SharedMasksTest_Held(int mask) {
    (void)mask;
    return -1;
}
extern "C" int MM_SharedMasksTest_PoolId(int mask) {
    (void)mask;
    return -1;
}
#else

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
}

#include "2s2h/Rando/Rando.h"

namespace {
uint8_t MMMaskItem(int mask) {
    switch (mask) {
        case 0:
            return (uint8_t)ITEM_MASK_GORON;
        case 1:
            return (uint8_t)ITEM_MASK_ZORA;
        case 2:
            return (uint8_t)ITEM_MASK_BUNNY;
        case 3:
            return (uint8_t)ITEM_MASK_KEATON;
        default:
            return (uint8_t)ITEM_MASK_TRUTH;
    }
}
} // namespace

/** Put (1) or take (0) the mask in its mask slot, as MM's own
 *  Item_Give(ITEM_MASK_*) writes it. */
extern "C" void MM_SharedMasksTest_SetHeld(int mask, int held) {
    const uint8_t item = MMMaskItem(mask);
    INV_CONTENT(item) = (held != 0) ? item : (uint8_t)ITEM_NONE;
}

/** 1 when MM's inventory holds the mask, else 0. */
extern "C" int MM_SharedMasksTest_Held(int mask) {
    const uint8_t item = MMMaskItem(mask);
    return INV_CONTENT(item) == item ? 1 : 0;
}

/** MM's pool id for the mask: the RI_* its pool places it as. */
extern "C" int MM_SharedMasksTest_PoolId(int mask) {
    switch (mask) {
        case 0:
            return (int)RI_MASK_GORON;
        case 1:
            return (int)RI_MASK_ZORA;
        case 2:
            return (int)RI_MASK_BUNNY;
        case 3:
            return (int)RI_MASK_KEATON;
        default:
            return (int)RI_MASK_TRUTH;
    }
}

#endif
