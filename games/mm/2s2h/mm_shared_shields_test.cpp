/**
 * @file mm_shared_shields_test.cpp
 * MM's half of the Shared Shields locks (#1068; the rule is
 * RSBS_XITEM_SHARED_SHIELDS in src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship rows `shared-shields-pairs` and
 * `shared-shields-arrival` (src/common/tests/test_shared_shields.c) drive MM's
 * REAL MM_HarvestSharedResources / MM_ApplySharedResources through. They only
 * set MM's shield equipment value (after MM_SharedSwordTest_ArmLive) and read
 * it back; every merge decision is the production shims' and the family members
 * MM_RegisterSharedShieldFamilies registers (GameExports_SingleExe.cpp). They
 * live here because only an MM TU has MM's `gSaveContext` and equipment macros
 * in scope.
 */

#include "global.h"

#include <cstring>

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The shared shields are a single-exe surface; test_runner.cpp declares these
// unconditionally, so a standalone 2ship build links.
extern "C" void MM_SharedShieldTest_SetShield(int equipValue) {
    (void)equipValue;
}
extern "C" int MM_SharedShieldTest_Shield(void) {
    return -1;
}
extern "C" int MM_SharedShieldTest_PoolId(int which) {
    (void)which;
    return -1;
}
#else

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
}

#include "2s2h/Rando/Rando.h"

/** Set MM's shield equipment value (EQUIP_VALUE_SHIELD_NONE / _HERO / _MIRROR),
 *  which in MM is both what Link owns and what he wears. */
extern "C" void MM_SharedShieldTest_SetShield(int equipValue) {
    SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, equipValue);
}

/** MM's shield equipment value: 0 none, 1 Hero's, 2 Mirror. */
extern "C" int MM_SharedShieldTest_Shield(void) {
    return (int)GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
}

/** MM's pool id for the shield: 1 = RI_SHIELD_HERO, 2 = RI_SHIELD_MIRROR. */
extern "C" int MM_SharedShieldTest_PoolId(int which) {
    return (which == 2) ? (int)RI_SHIELD_MIRROR : (int)RI_SHIELD_HERO;
}

#endif
