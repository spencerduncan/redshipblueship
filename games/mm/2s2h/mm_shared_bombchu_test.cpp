/**
 * @file mm_shared_bombchu_test.cpp
 * MM's half of the shared-bombchu pickup lock (#1061).
 *
 * THE SHIM HELPERS the redship row `shared-bombchu-pickup`
 * (src/common/tests/test_shared_bombchu.c) drives MM's REAL
 * MM_HarvestSharedResources / MM_ApplySharedResources, MM's real arrival
 * consumer and the REAL pickup core MM_Rando_Foreign_RecordPickup through. They
 * only put MM's live save in a known state and read the bombchu slot back; every
 * merge decision is the production code's. They live here because only an MM
 * TU has MM's `gSaveContext`, its upgrade tables and its item ids in scope.
 */

#include "global.h"

#include <cstring>

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The shared pool is a single-exe surface; test_runner.cpp declares these
// unconditionally, so a standalone 2ship build links.
extern "C" void MM_SharedBombchuTest_ArmLive(int bombBagTier, int count) {
    (void)bombBagTier;
    (void)count;
}
extern "C" void MM_SharedBombchuTest_SetCount(int count) {
    (void)count;
}
extern "C" int MM_SharedBombchuTest_Count(void) {
    return -1;
}
extern "C" int MM_SharedBombchuTest_HasItem(void) {
    return -1;
}
extern "C" int MM_SharedBombchuTest_Capacity(void) {
    return -1;
}
extern "C" int MM_SharedBombchuTest_PackId(void) {
    return -1;
}
extern "C" int MM_SharedBombchuTest_HostCheck(void) {
    return -1;
}
#else

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
}

#include "2s2h/Rando/Rando.h"

/** MM's live save as a cross-game MM session runs it: the 0xFF "no flash slot"
 *  sentinel and GAMEMODE_NORMAL, which Combo_SaveIsLiveFile requires before the
 *  harvest or a pickup credit does anything. Bomb bag at @p bombBagTier (MM's
 *  bombchu ceiling is the bag's capacity: 0, 20, 30, 40) and @p count bombchus,
 *  with the bombchu item in its slot only for a nonzero count. Every item slot
 *  starts at ITEM_NONE, as a real save's does (a zeroed slot reads as item 0,
 *  which MM_EnsureInventoryItem treats as occupied and never fills, and which
 *  never reads as ITEM_BOMBCHU whatever the code under test does). Everything
 *  else zero, the rando check table included. */
extern "C" void MM_SharedBombchuTest_ArmLive(int bombBagTier, int count) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    memset(gSaveContext.save.saveInfo.inventory.items, ITEM_NONE, sizeof(gSaveContext.save.saveInfo.inventory.items));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.flashSaveAvailable = true;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    MM_Inventory_ChangeUpgrade(UPG_BOMB_BAG, (u32)bombBagTier);
    if (bombBagTier > 0) {
        INV_CONTENT(ITEM_BOMB) = ITEM_BOMB;
    }
    if (count > 0) {
        INV_CONTENT(ITEM_BOMBCHU) = ITEM_BOMBCHU;
    }
    AMMO(ITEM_BOMBCHU) = (s8)count;
}

/** Set MM's live bombchu count (what the player holds mid-cycle). */
extern "C" void MM_SharedBombchuTest_SetCount(int count) {
    AMMO(ITEM_BOMBCHU) = (s8)count;
}

/** MM's live bombchu count. */
extern "C" int MM_SharedBombchuTest_Count(void) {
    return (int)AMMO(ITEM_BOMBCHU);
}

/** 1 when the bombchu item is in its slot. */
extern "C" int MM_SharedBombchuTest_HasItem(void) {
    return INV_CONTENT(ITEM_BOMBCHU) == ITEM_BOMBCHU ? 1 : 0;
}

/** MM's live bombchu ceiling: the bomb bag's capacity. */
extern "C" int MM_SharedBombchuTest_Capacity(void) {
    return (int)CUR_CAPACITY(UPG_BOMB_BAG);
}

/** MM's own id for a 10-bombchu pack (RI_BOMBCHU_10). */
extern "C" int MM_SharedBombchuTest_PackId(void) {
    return (int)RI_BOMBCHU_10;
}

/** A real MM check id to host a foreign pack on (the first after RC_UNKNOWN). */
extern "C" int MM_SharedBombchuTest_HostCheck(void) {
    return (int)RC_UNKNOWN + 1;
}

#endif
