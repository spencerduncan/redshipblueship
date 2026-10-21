/**
 * @file oot_shared_bombchu_test.cpp
 * OoT's half of the shared-bombchu pickup lock (#1061).
 *
 * THE SHIM HELPERS the redship row `shared-bombchu-pickup`
 * (src/common/tests/test_shared_bombchu.c) drives OoT's REAL
 * OoT_HarvestSharedResources / OoT_ApplySharedResources and the REAL pickup
 * core OoT_Rando_Foreign_RecordPickup through. They only put OoT's live save in
 * a known state and read the bombchu slot back; every merge decision is the
 * production code's.
 *
 * The live save is a VANILLA (QUEST_NORMAL) file: the bombchu row is shared for
 * every world, and outside the randomizer OoT's bombchu ceiling is the fixed 50
 * with no bag gate (OoT_BombchuCapacity, OoT_SharedAmmoGrantBlocked), so the row
 * needs no Randomizer context and reads one deterministic cap.
 */

#include <cstring>

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

/** OoT's live save as a real loaded file holding the bombchu item and @p count
 *  bombchus (the bag owned). GAMEMODE_NORMAL is what Combo_SaveIsLiveFile
 *  requires before the harvest or a pickup credit does anything. Everything
 *  else zero, so the harvest offers no other kind a nonzero value. */
extern "C" void OoT_SharedBombchuTest_ArmLive(int count) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.ship.quest.id = QUEST_NORMAL;
    INV_CONTENT(ITEM_BOMBCHU) = ITEM_BOMBCHU;
    AMMO(ITEM_BOMBCHU) = (s8)count;
}

/** Set OoT's live bombchu count (what the player holds mid-stint). */
extern "C" void OoT_SharedBombchuTest_SetCount(int count) {
    AMMO(ITEM_BOMBCHU) = (s8)count;
}

/** OoT's live bombchu count. */
extern "C" int OoT_SharedBombchuTest_Count(void) {
    return (int)AMMO(ITEM_BOMBCHU);
}

/** OoT's own id for a 10-bombchu pack (RG_BOMBCHU_10). */
extern "C" int OoT_SharedBombchuTest_PackId(void) {
    return (int)RG_BOMBCHU_10;
}
