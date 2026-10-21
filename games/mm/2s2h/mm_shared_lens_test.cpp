/**
 * @file mm_shared_lens_test.cpp
 * MM's half of the Shared Lens of Truth lock (#1065; the rule is
 * RSBS_XITEM_SHARED_LENS in src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship row `shared-lens-merge`
 * (src/common/tests/test_shared_lens.c) drives MM's REAL
 * MM_HarvestSharedResources / MM_ApplySharedResources through. They only put
 * the Lens of Truth's inventory slot in a known state and read it back; every
 * merge decision is the production shims' and the registered family member's.
 * They live here because only an MM TU has MM's `gSaveContext` and inventory
 * macros in scope. The live save itself is armed by MM_SharedSwordTest_ArmLive
 * (mm_shared_kokiri_sword_test.cpp).
 */

#include "global.h"

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The shared lens is a single-exe surface; test_runner.cpp declares these
// unconditionally, so a standalone 2ship build links.
extern "C" void MM_SharedLensTest_SetHeld(int held) {
    (void)held;
}
extern "C" int MM_SharedLensTest_Held(void) {
    return -1;
}
extern "C" int MM_SharedLensTest_PoolLensId(void) {
    return -1;
}
#else

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
}

#include "2s2h/Rando/Rando.h"

/** Put (1) or take (0) the Lens of Truth in its inventory slot, as MM's own
 *  Item_Give(ITEM_LENS_OF_TRUTH) writes it. */
extern "C" void MM_SharedLensTest_SetHeld(int held) {
    INV_CONTENT(ITEM_LENS_OF_TRUTH) = (held != 0) ? ITEM_LENS_OF_TRUTH : ITEM_NONE;
}

/** 1 when MM's inventory holds the Lens of Truth, else 0. */
extern "C" int MM_SharedLensTest_Held(void) {
    return INV_CONTENT(ITEM_LENS_OF_TRUTH) == ITEM_LENS_OF_TRUTH ? 1 : 0;
}

/** MM's pool lens: the id MM's Lens of Truth is placed as (RI_LENS). */
extern "C" int MM_SharedLensTest_PoolLensId(void) {
    return (int)RI_LENS;
}

#endif
