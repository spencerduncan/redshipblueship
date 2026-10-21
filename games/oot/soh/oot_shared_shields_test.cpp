/**
 * @file oot_shared_shields_test.cpp
 * OoT's half of the Shared Shields locks (#1068; the rule is
 * RSBS_XITEM_SHARED_SHIELDS in src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship rows `shared-shields-pairs` and
 * `shared-shields-arrival` (src/common/tests/test_shared_shields.c) drive OoT's
 * REAL OoT_HarvestSharedResources / OoT_ApplySharedResources through. They only
 * put OoT's live save in a known state (after OoT_SharedSwordTest_ArmLive) and
 * read the shields back; every merge decision is the production shims' and the
 * family members OoT_RegisterSharedShieldFamilies registers
 * (GameExports_SingleExe.cpp). They live here because only an OoT TU has OoT's
 * `gSaveContext` and equipment macros in scope.
 */

#include <cstring>

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

// z_parameter.c's equip writer (functions.h), declared as SoH's own C++ TUs do
// (AssignableTunicsAndBoots.cpp).
extern "C" void OoT_Inventory_ChangeEquipment(s16 equipment, u16 value);

namespace {

/** 1 = the Hylian Shield, 2 = the Mirror Shield (the row's own numbering). */
int OoTShieldInv(int which) {
    return (which == 2) ? EQUIP_INV_SHIELD_MIRROR : EQUIP_INV_SHIELD_HYLIAN;
}

} // namespace

/** Give (1) or take (0) the shield's owned-equipment bit, as OoT's own
 *  Item_Give(ITEM_SHIELD_*) writes it (z_parameter.c). */
extern "C" void OoT_SharedShieldTest_SetOwned(int which, int owned) {
    const u16 flag = (u16)OWNED_EQUIP_FLAG(EQUIP_TYPE_SHIELD, OoTShieldInv(which));
    if (owned != 0) {
        gSaveContext.inventory.equipment |= flag;
    } else {
        gSaveContext.inventory.equipment &= (u16)~flag;
    }
}

/** 1 when OoT owns the shield, else 0. */
extern "C" int OoT_SharedShieldTest_Owned(int which) {
    return CHECK_OWNED_EQUIP(EQUIP_TYPE_SHIELD, OoTShieldInv(which)) ? 1 : 0;
}

/** The shield Link wears: 0 none, 1 Deku, 2 Hylian, 3 Mirror (EQUIP_VALUE_SHIELD_*). */
extern "C" int OoT_SharedShieldTest_Equipped(void) {
    return (int)CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
}

/** Put Link at an age: 1 adult, 0 child. */
extern "C" void OoT_SharedShieldTest_SetAdult(int adult) {
    gSaveContext.linkAge = (adult != 0) ? LINK_AGE_ADULT : LINK_AGE_CHILD;
}

/** OoT's pool id for the shield (RG_HYLIAN_SHIELD / RG_MIRROR_SHIELD). */
extern "C" int OoT_SharedShieldTest_PoolId(int which) {
    return (which == 2) ? (int)RG_MIRROR_SHIELD : (int)RG_HYLIAN_SHIELD;
}

/** Own and wear the shield at @p equipValue (0 none, 1 Deku, 2 Hylian, 3
 *  Mirror), as a player who equipped it from the Equipment subscreen. */
extern "C" void OoT_SharedShieldTest_Wear(int equipValue) {
    if (equipValue != EQUIP_VALUE_SHIELD_NONE) {
        gSaveContext.inventory.equipment |=
            (u16)OWNED_EQUIP_FLAG(EQUIP_TYPE_SHIELD, equipValue - EQUIP_VALUE_SHIELD_DEKU + EQUIP_INV_SHIELD_DEKU);
    }
    OoT_Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, (u16)equipValue);
}
