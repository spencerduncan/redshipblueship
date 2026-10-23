/**
 * @file mm_shared_tier_test.cpp
 * MM's half of the shared-tier pickup lock (#1120).
 *
 * THE SHIM HELPERS the redship row `shared-tier-pickup`
 * (src/common/tests/test_shared_tier.c) drives MM's REAL
 * MM_HarvestSharedResources / MM_ApplySharedResources, MM's real arrival
 * consumer and the REAL pickup core MM_Rando_Foreign_RecordPickup through. They
 * only put MM's live save in a known state and read the bomb bag and wallet
 * rows back; every merge decision is the production code's. They live here
 * because only an MM TU has MM's `gSaveContext`, its upgrade tables and its
 * item ids in scope.
 */

#include "global.h"

#include <cstring>

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The shared pool is a single-exe surface; test_runner.cpp declares these
// unconditionally, so a standalone 2ship build links.
extern "C" void MM_SharedTierTest_ArmLive(void) {
}
extern "C" void MM_SharedTierTest_SetBombBag(int tier, int bombs) {
    (void)tier;
    (void)bombs;
}
extern "C" void MM_SharedTierTest_SetBombs(int bombs) {
    (void)bombs;
}
extern "C" int MM_SharedTierTest_BombBagTier(void) {
    return -1;
}
extern "C" int MM_SharedTierTest_Bombs(void) {
    return -1;
}
extern "C" int MM_SharedTierTest_HasBombItem(void) {
    return -1;
}
extern "C" void MM_SharedTierTest_SetWallet(int tier, int rupees) {
    (void)tier;
    (void)rupees;
}
extern "C" int MM_SharedTierTest_WalletTier(void) {
    return -1;
}
extern "C" int MM_SharedTierTest_Rupees(void) {
    return -1;
}
extern "C" int MM_SharedTierTest_BombBagId(void) {
    return -1;
}
extern "C" int MM_SharedTierTest_WalletId(void) {
    return -1;
}
extern "C" int MM_SharedTierTest_BottleId(void) {
    return -1;
}
extern "C" int MM_SharedTierTest_HostCheck(int which) {
    (void)which;
    return -1;
}
#else

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
}

#include "2s2h/Rando/Rando.h"

/** MM's live save as a cross-game MM session runs it, holding nothing: the 0xFF
 *  "no flash slot" sentinel and GAMEMODE_NORMAL (what Combo_SaveIsLiveFile
 *  requires before the harvest or a pickup credit does anything), every item
 *  slot at ITEM_NONE as a real save's are (a zeroed slot reads as item 0, which
 *  MM_EnsureInventoryItem treats as occupied), everything else zero, the rando
 *  check table included. */
extern "C" void MM_SharedTierTest_ArmLive(void) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    memset(gSaveContext.save.saveInfo.inventory.items, ITEM_NONE, sizeof(gSaveContext.save.saveInfo.inventory.items));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.flashSaveAvailable = true;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
}

/** Bomb bag at @p tier (the bomb item in its slot for a nonzero tier, as MM's
 *  own give leaves it) holding @p bombs. */
extern "C" void MM_SharedTierTest_SetBombBag(int tier, int bombs) {
    MM_Inventory_ChangeUpgrade(UPG_BOMB_BAG, (u32)tier);
    if (tier > 0) {
        INV_CONTENT(ITEM_BOMB) = ITEM_BOMB;
    }
    AMMO(ITEM_BOMB) = (s8)bombs;
}

/** Set MM's live bomb count (what the player holds mid-cycle). */
extern "C" void MM_SharedTierTest_SetBombs(int bombs) {
    AMMO(ITEM_BOMB) = (s8)bombs;
}

/** MM's live bomb bag tier (UPG_BOMB_BAG). */
extern "C" int MM_SharedTierTest_BombBagTier(void) {
    return (int)CUR_UPG_VALUE(UPG_BOMB_BAG);
}

/** MM's live bomb count. */
extern "C" int MM_SharedTierTest_Bombs(void) {
    return (int)AMMO(ITEM_BOMB);
}

/** 1 when the bomb item is in its slot. */
extern "C" int MM_SharedTierTest_HasBombItem(void) {
    return INV_CONTENT(ITEM_BOMB) == ITEM_BOMB ? 1 : 0;
}

/** Wallet at @p tier holding @p rupees. */
extern "C" void MM_SharedTierTest_SetWallet(int tier, int rupees) {
    MM_Inventory_ChangeUpgrade(UPG_WALLET, (u32)tier);
    gSaveContext.save.saveInfo.playerData.rupees = (s16)rupees;
}

/** MM's live wallet tier (UPG_WALLET). */
extern "C" int MM_SharedTierTest_WalletTier(void) {
    return (int)CUR_UPG_VALUE(UPG_WALLET);
}

/** MM's live rupee count. */
extern "C" int MM_SharedTierTest_Rupees(void) {
    return (int)gSaveContext.save.saveInfo.playerData.rupees;
}

/** MM's own id for a progressive bomb bag (RI_PROGRESSIVE_BOMB_BAG): what MM's
 *  pool places (StaticData/Checks.cpp), and what the operator found in Hyrule. */
extern "C" int MM_SharedTierTest_BombBagId(void) {
    return (int)RI_PROGRESSIVE_BOMB_BAG;
}

/** MM's own id for a progressive wallet (RI_PROGRESSIVE_WALLET). */
extern "C" int MM_SharedTierTest_WalletId(void) {
    return (int)RI_PROGRESSIVE_WALLET;
}

/** MM's own id for an empty bottle (RI_BOTTLE_EMPTY): a bottle-count copy whose
 *  kind the frozen Shared Bottles rule arms or not. */
extern "C" int MM_SharedTierTest_BottleId(void) {
    return (int)RI_BOTTLE_EMPTY;
}

/** The @p which-th real MM check id to host a foreign item on (the ones after
 *  RC_UNKNOWN; the bombchu row uses the first). */
extern "C" int MM_SharedTierTest_HostCheck(int which) {
    return (int)RC_UNKNOWN + 1 + which;
}

#endif
