/**
 * @file oot_shared_tier_test.cpp
 * OoT's half of the shared-tier pickup lock (#1120).
 *
 * THE SHIM HELPERS the redship row `shared-tier-pickup`
 * (src/common/tests/test_shared_tier.c) drives OoT's REAL
 * OoT_HarvestSharedResources / OoT_ApplySharedResources, the REAL pickup core
 * OoT_Rando_Foreign_RecordPickup and the REAL arrival consumer through. They
 * only put OoT's live save in a known state and read the bomb bag row back;
 * every merge decision is the production code's. The wallet legs reuse the
 * shared-wallet row's helpers (oot_shared_wallet_test.cpp) and the bottle legs
 * the shared-bottles row's (oot_shared_bottles_test.cpp).
 *
 * The live save is a VANILLA (QUEST_NORMAL) file, like the bombchu row's: the
 * ammo rows are shared for every world, and outside the randomizer no ammo
 * grant is blocked (OoT_SharedAmmoGrantBlocked), so the row needs no
 * Randomizer context.
 */

#include <cstring>

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

extern "C" void OoT_Inventory_ChangeUpgrade(s16 upgrade, s16 value); // GameExports_SingleExe.cpp's own use

/** OoT's live save as a real loaded vanilla file holding nothing: every item
 *  slot ITEM_NONE (as a real save's are, so a tier rising from zero has an empty
 *  slot to author the item into), GAMEMODE_NORMAL (what Combo_SaveIsLiveFile
 *  requires before the harvest or a pickup credit does anything), everything
 *  else zero, so the harvest offers no kind a nonzero value. */
extern "C" void OoT_SharedTierTest_ArmLive(void) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    memset(gSaveContext.inventory.items, ITEM_NONE, sizeof(gSaveContext.inventory.items));
    gSaveContext.fileNum = 0;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.ship.quest.id = QUEST_NORMAL;
}

/** Bomb bag at @p tier (the bomb item in its slot for a nonzero tier, as OoT's
 *  own give leaves it) holding @p bombs. */
extern "C" void OoT_SharedTierTest_SetBombBag(int tier, int bombs) {
    OoT_Inventory_ChangeUpgrade(UPG_BOMB_BAG, (s16)tier);
    if (tier > 0) {
        INV_CONTENT(ITEM_BOMB) = ITEM_BOMB;
    }
    AMMO(ITEM_BOMB) = (s8)bombs;
}

/** Set OoT's live bomb count (what the player holds mid-stint). */
extern "C" void OoT_SharedTierTest_SetBombs(int bombs) {
    AMMO(ITEM_BOMB) = (s8)bombs;
}

/** OoT's live bomb bag tier (UPG_BOMB_BAG). */
extern "C" int OoT_SharedTierTest_BombBagTier(void) {
    return (int)CUR_UPG_VALUE(UPG_BOMB_BAG);
}

/** OoT's live bomb count. */
extern "C" int OoT_SharedTierTest_Bombs(void) {
    return (int)AMMO(ITEM_BOMB);
}

/** 1 when the bomb item is in its slot. */
extern "C" int OoT_SharedTierTest_HasBombItem(void) {
    return INV_CONTENT(ITEM_BOMB) == ITEM_BOMB ? 1 : 0;
}

/** OoT's own id for a progressive bomb bag (RG_PROGRESSIVE_BOMB_BAG). */
extern "C" int OoT_SharedTierTest_BombBagId(void) {
    return (int)RG_PROGRESSIVE_BOMB_BAG;
}

/** OoT's own id for a progressive wallet (RG_PROGRESSIVE_WALLET). */
extern "C" int OoT_SharedTierTest_WalletId(void) {
    return (int)RG_PROGRESSIVE_WALLET;
}
