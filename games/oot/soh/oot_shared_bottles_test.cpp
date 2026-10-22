/**
 * @file oot_shared_bottles_test.cpp
 * OoT's half of the Shared Bottles locks (#970; the rule is
 * RSBS_XITEM_SHARED_BOTTLES in src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship rows `shared-bottles-tier` and
 * `shared-bottles-trim` (src/common/tests/test_shared_bottles.c) drive OoT's
 * REAL OoT_HarvestSharedResources / OoT_ApplySharedResources and OoT's REAL
 * item classification through. They only put OoT's four bottle slots in a known
 * state (after OoT_SharedSwordTest_ArmLive) and read them back, and name OoT's
 * pool ids for the bottle items; every merge decision is the production shims'.
 * They live here because only an OoT TU has OoT's `gSaveContext` and item ids in
 * scope.
 *
 * The rows speak a neutral code per slot: 0 no bottle, 1 an empty bottle, 2 a
 * bottle holding a fairy, 3 Ruto's Letter; -1 is anything else.
 */

#include <cstring>

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

namespace {

constexpr int kOoTBottleSlots = SLOT_BOTTLE_4 - SLOT_BOTTLE_1 + 1;

u8 OoTBottleItemOf(int code) {
    switch (code) {
        case 1:
            return ITEM_BOTTLE;
        case 2:
            return ITEM_FAIRY;
        case 3:
            return ITEM_LETTER_RUTO;
        default:
            return ITEM_NONE;
    }
}

} // namespace

/** How many bottle slots OoT has (four). */
extern "C" int OoT_SharedBottleTest_SlotCount(void) {
    return kOoTBottleSlots;
}

/** Put the neutral code @p code (see the file comment) in bottle slot @p slot
 *  (0-based). Out of range is ignored. */
extern "C" void OoT_SharedBottleTest_SetSlot(int slot, int code) {
    if (slot < 0 || slot >= kOoTBottleSlots) {
        return;
    }
    gSaveContext.inventory.items[SLOT_BOTTLE_1 + slot] = OoTBottleItemOf(code);
}

/** The neutral code of bottle slot @p slot (0-based), or -1. */
extern "C" int OoT_SharedBottleTest_Slot(int slot) {
    if (slot < 0 || slot >= kOoTBottleSlots) {
        return -1;
    }
    switch (gSaveContext.inventory.items[SLOT_BOTTLE_1 + slot]) {
        case ITEM_NONE:
            return 0;
        case ITEM_BOTTLE:
            return 1;
        case ITEM_FAIRY:
            return 2;
        case ITEM_LETTER_RUTO:
            return 3;
        default:
            return -1;
    }
}

/** OoT's pool id for a bottle item: 0 RG_EMPTY_BOTTLE, 1 RG_BOTTLE_WITH_MILK,
 *  2 RG_BOTTLE_WITH_FAIRY, 3 RG_RUTOS_LETTER, 4 RG_BOTTLE_WITH_BLUE_POTION. */
extern "C" int OoT_SharedBottleTest_PoolId(int which) {
    switch (which) {
        case 0:
            return (int)RG_EMPTY_BOTTLE;
        case 1:
            return (int)RG_BOTTLE_WITH_MILK;
        case 2:
            return (int)RG_BOTTLE_WITH_FAIRY;
        case 3:
            return (int)RG_RUTOS_LETTER;
        case 4:
            return (int)RG_BOTTLE_WITH_BLUE_POTION;
        default:
            return -1;
    }
}
