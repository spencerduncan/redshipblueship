/**
 * @file mm_shared_bottles_test.cpp
 * MM's half of the Shared Bottles locks (#970; the rule is
 * RSBS_XITEM_SHARED_BOTTLES in src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship rows `shared-bottles-tier` and
 * `shared-bottles-trim` (src/common/tests/test_shared_bottles.c) drive MM's REAL
 * MM_HarvestSharedResources / MM_ApplySharedResources and MM's REAL item
 * classification through. They only put MM's six bottle slots in a known state
 * (after MM_SharedSwordTest_ArmLive) and read them back, and name MM's pool ids
 * for the bottle items; every merge decision is the production shims'. They live
 * here because only an MM TU has MM's `gSaveContext` and item ids in scope.
 *
 * The rows speak a neutral code per slot: 0 no bottle, 1 an empty bottle, 2 a
 * bottle of Chateau Romani, 3 a bottle of Gold Dust; -1 is anything else.
 */

#include "global.h"

#include <cstring>

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The shared bottles are a single-exe surface; test_runner.cpp declares these
// unconditionally, so a standalone 2ship build links.
extern "C" int MM_SharedBottleTest_SlotCount(void) {
    return -1;
}
extern "C" void MM_SharedBottleTest_SetSlot(int slot, int code) {
    (void)slot;
    (void)code;
}
extern "C" int MM_SharedBottleTest_Slot(int slot) {
    (void)slot;
    return -1;
}
extern "C" int MM_SharedBottleTest_PoolId(int which) {
    (void)which;
    return -1;
}
#else

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
}

#include "2s2h/Rando/Rando.h"

namespace {

constexpr int kMMBottleSlots = SLOT_BOTTLE_6 - SLOT_BOTTLE_1 + 1;

u8 MMBottleItemOf(int code) {
    switch (code) {
        case 1:
            return ITEM_BOTTLE;
        case 2:
            return ITEM_CHATEAU;
        case 3:
            return ITEM_GOLD_DUST;
        default:
            return ITEM_NONE;
    }
}

} // namespace

/** How many bottle slots MM has (six). */
extern "C" int MM_SharedBottleTest_SlotCount(void) {
    return kMMBottleSlots;
}

/** Put the neutral code @p code (see the file comment) in bottle slot @p slot
 *  (0-based). Out of range is ignored. */
extern "C" void MM_SharedBottleTest_SetSlot(int slot, int code) {
    if (slot < 0 || slot >= kMMBottleSlots) {
        return;
    }
    gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1 + slot] = MMBottleItemOf(code);
}

/** The neutral code of bottle slot @p slot (0-based), or -1. */
extern "C" int MM_SharedBottleTest_Slot(int slot) {
    if (slot < 0 || slot >= kMMBottleSlots) {
        return -1;
    }
    switch (gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1 + slot]) {
        case ITEM_NONE:
            return 0;
        case ITEM_BOTTLE:
            return 1;
        case ITEM_CHATEAU:
            return 2;
        case ITEM_GOLD_DUST:
            return 3;
        default:
            return -1;
    }
}

/** MM's pool id for a bottle item: 0 RI_BOTTLE_EMPTY, 1 RI_BOTTLE_CHATEAU_ROMANI,
 *  2 RI_BOTTLE_GOLD_DUST, 3 RI_BOTTLE_MILK, 4 RI_BOTTLE_RED_POTION. */
extern "C" int MM_SharedBottleTest_PoolId(int which) {
    switch (which) {
        case 0:
            return (int)RI_BOTTLE_EMPTY;
        case 1:
            return (int)RI_BOTTLE_CHATEAU_ROMANI;
        case 2:
            return (int)RI_BOTTLE_GOLD_DUST;
        case 3:
            return (int)RI_BOTTLE_MILK;
        case 4:
            return (int)RI_BOTTLE_RED_POTION;
        default:
            return -1;
    }
}

#endif
