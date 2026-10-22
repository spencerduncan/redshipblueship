/**
 * @file oot_shared_lens_test.cpp
 * OoT's half of the Shared Lens of Truth lock (#1065; the rule is
 * RSBS_XITEM_SHARED_LENS in src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship row `shared-lens-merge`
 * (src/common/tests/test_shared_lens.c) drives OoT's REAL
 * OoT_HarvestSharedResources / OoT_ApplySharedResources through. They only put
 * the Lens of Truth's inventory slot in a known state and read it back; every
 * merge decision is the production shims' and the registered family member's.
 * They live here because only an OoT TU has OoT's `gSaveContext` and inventory
 * macros in scope. The live save itself is armed by OoT_SharedSwordTest_ArmLive
 * (oot_shared_kokiri_sword_test.cpp).
 */

#include <cstring>

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (oot_shared_kokiri_sword_test.cpp says why).
#include <z64.h>
#include "variables.h"
// INV_CONTENT / SLOT. After z64.h, as the other OoT TUs include it.
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

/** Put (1) or take (0) the Lens of Truth in its inventory slot, as OoT's own
 *  Item_Give(ITEM_LENS) writes it. */
extern "C" void OoT_SharedLensTest_SetHeld(int held) {
    INV_CONTENT(ITEM_LENS) = (held != 0) ? ITEM_LENS : ITEM_NONE;
}

/** 1 when OoT's inventory holds the Lens of Truth, else 0. */
extern "C" int OoT_SharedLensTest_Held(void) {
    return INV_CONTENT(ITEM_LENS) == ITEM_LENS ? 1 : 0;
}

/** OoT's pool lens: the id OoT's Lens of Truth is placed as (RG_LENS_OF_TRUTH). */
extern "C" int OoT_SharedLensTest_PoolLensId(void) {
    return (int)RG_LENS_OF_TRUTH;
}
