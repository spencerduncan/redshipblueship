/**
 * @file oot_shared_arrows_test.cpp
 * OoT's half of the Shared Fire, Ice and Light Arrows lock (#968; the rules are
 * RSBS_XITEM_SHARED_ARROW_FIRE, _ICE and _LIGHT in src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship row `shared-arrows-merge`
 * (src/common/tests/test_shared_arrows.c) drives OoT's REAL
 * OoT_HarvestSharedResources / OoT_ApplySharedResources through. They only put
 * an arrow's inventory slot in a known state and read it back; every merge
 * decision is the production shims' and the registered family member's. They
 * live here because only an OoT TU has OoT's `gSaveContext` and inventory
 * macros in scope. The live save itself is armed by OoT_SharedSwordTest_ArmLive
 * (oot_shared_kokiri_sword_test.cpp). `arrow` is 0 for Fire, 1 for Ice and 2
 * for Light.
 */

#include <cstring>

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (oot_shared_kokiri_sword_test.cpp says why).
#include <z64.h>
#include "variables.h"
// INV_CONTENT / SLOT. After z64.h, as the other OoT TUs include it.
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

namespace {
uint8_t OoTArrowItem(int arrow) {
    return (arrow == 0) ? (uint8_t)ITEM_ARROW_FIRE : (arrow == 1) ? (uint8_t)ITEM_ARROW_ICE : (uint8_t)ITEM_ARROW_LIGHT;
}
} // namespace

/** Put (1) or take (0) the arrow in its inventory slot, as OoT's own
 *  Item_Give(ITEM_ARROW_*) writes it. */
extern "C" void OoT_SharedArrowsTest_SetHeld(int arrow, int held) {
    const uint8_t item = OoTArrowItem(arrow);
    INV_CONTENT(item) = (held != 0) ? item : (uint8_t)ITEM_NONE;
}

/** 1 when OoT's inventory holds the arrow, else 0. */
extern "C" int OoT_SharedArrowsTest_Held(int arrow) {
    const uint8_t item = OoTArrowItem(arrow);
    return INV_CONTENT(item) == item ? 1 : 0;
}

/** OoT's pool id for the arrow: the RG_* its pool places it as. */
extern "C" int OoT_SharedArrowsTest_PoolId(int arrow) {
    return (arrow == 0) ? (int)RG_FIRE_ARROWS : (arrow == 1) ? (int)RG_ICE_ARROWS : (int)RG_LIGHT_ARROWS;
}
