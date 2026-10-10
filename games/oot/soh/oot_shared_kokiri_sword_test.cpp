/**
 * @file oot_shared_kokiri_sword_test.cpp
 * OoT's half of the Shared Kokiri Sword lock (#886; the rule is
 * RSBS_COMBO_FLAG_SHARED_KOKIRI_SWORD in src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship row `shared-kokiri-sword`
 * (src/common/tests/test_shared_kokiri_sword.c) drives OoT's REAL
 * OoT_HarvestSharedResources / OoT_ApplySharedResources through. They only put
 * OoT's live save in a known state and read the Kokiri Sword's owned-equipment
 * bit back; every merge decision is the production shims'. They live here
 * because only an OoT TU has OoT's `gSaveContext` and equipment macros in scope.
 */

#include <cstring>

extern "C" {
#include <z64.h>
#include "variables.h"
#include "macros.h" // OWNED_EQUIP_FLAG / CHECK_OWNED_EQUIP
}

/** OoT's live save as a real loaded file: GAMEMODE_NORMAL is what
 *  Combo_SaveIsLiveFile requires before OoT_HarvestSharedResources does
 *  anything. Everything else zero, so the harvest offers no other kind a nonzero
 *  value and the pool holds only what the row puts there. */
extern "C" void OoT_SharedSwordTest_ArmLive(void) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
}

/** Give (1) or take (0) the Kokiri Sword's owned-equipment bit, as OoT's own
 *  Item_Give(ITEM_SWORD_KOKIRI) writes it. */
extern "C" void OoT_SharedSwordTest_SetOwned(int owned) {
    const u16 flag = (u16)OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI);
    if (owned != 0) {
        gSaveContext.inventory.equipment |= flag;
    } else {
        gSaveContext.inventory.equipment &= (u16)~flag;
    }
}

/** 1 when OoT owns the Kokiri Sword, else 0. */
extern "C" int OoT_SharedSwordTest_Owned(void) {
    return CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI) ? 1 : 0;
}
