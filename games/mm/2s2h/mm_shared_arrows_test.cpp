/**
 * @file mm_shared_arrows_test.cpp
 * MM's half of the Shared Fire, Ice and Light Arrows lock (#968; the rules are
 * RSBS_XITEM_SHARED_ARROW_FIRE, _ICE and _LIGHT in src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship row `shared-arrows-merge`
 * (src/common/tests/test_shared_arrows.c) drives MM's REAL
 * MM_HarvestSharedResources / MM_ApplySharedResources through. They only put
 * an arrow's inventory slot in a known state and read it back; every merge
 * decision is the production shims' and the registered family member's. The
 * magic arrows' slots survive the Song of Time (only ammo and the cycle's
 * items are reset). `arrow` is 0 for Fire, 1 for Ice and 2 for Light.
 */

#include "global.h"

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The shared arrows are a single-exe surface; test_runner.cpp declares these
// unconditionally, so a standalone 2ship build links.
extern "C" void MM_SharedArrowsTest_SetHeld(int arrow, int held) {
    (void)arrow;
    (void)held;
}
extern "C" int MM_SharedArrowsTest_Held(int arrow) {
    (void)arrow;
    return -1;
}
extern "C" int MM_SharedArrowsTest_PoolId(int arrow) {
    (void)arrow;
    return -1;
}
#else

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
}

#include "2s2h/Rando/Rando.h"

namespace {
uint8_t MMArrowItem(int arrow) {
    return (arrow == 0) ? (uint8_t)ITEM_ARROW_FIRE : (arrow == 1) ? (uint8_t)ITEM_ARROW_ICE : (uint8_t)ITEM_ARROW_LIGHT;
}
} // namespace

/** Put (1) or take (0) the arrow in its inventory slot, as MM's own
 *  Item_Give(ITEM_ARROW_*) writes it. */
extern "C" void MM_SharedArrowsTest_SetHeld(int arrow, int held) {
    const uint8_t item = MMArrowItem(arrow);
    INV_CONTENT(item) = (held != 0) ? item : (uint8_t)ITEM_NONE;
}

/** 1 when MM's inventory holds the arrow, else 0. */
extern "C" int MM_SharedArrowsTest_Held(int arrow) {
    const uint8_t item = MMArrowItem(arrow);
    return INV_CONTENT(item) == item ? 1 : 0;
}

/** MM's pool id for the arrow: the RI_* its pool places it as. */
extern "C" int MM_SharedArrowsTest_PoolId(int arrow) {
    return (arrow == 0) ? (int)RI_ARROW_FIRE : (arrow == 1) ? (int)RI_ARROW_ICE : (int)RI_ARROW_LIGHT;
}

#endif
