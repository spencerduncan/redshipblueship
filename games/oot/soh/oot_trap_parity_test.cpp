/**
 * @file oot_trap_parity_test.cpp
 * OoT's half of the trap-type parity lock (#1424; OoTMM's Drain, Anti-Magic and
 * Rupoor traps in SoH's Additional Traps).
 *
 * THE SHIM HELPERS the redship row `trap-type-parity`
 * (src/common/tests/test_trap_type_parity.c) drives OoT's REAL
 * OoT_HarvestSharedResources through, around the traps' own save half
 * (ExtraTraps.cpp, OoT_ExtraTraps_TestApplySaveEffect). They only put OoT's live
 * save in a known state and read it back; what the pool keeps is the
 * production harvest's. They live here because only an OoT TU has OoT's
 * `gSaveContext` in scope.
 */

#include <cstring>

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"
#include "macros.h"

extern "C" void OoT_Inventory_ChangeUpgrade(s16 upgrade, s16 value); // GameExports_SingleExe.cpp's own use

/** OoT's live save as a real loaded vanilla-quest file (GAMEMODE_NORMAL, which
 *  the harvest's live-file gate requires) with a single magic meter, a wallet of
 *  tier 0 (99 rupees), @p healthCapacity and @p health in health units (0x10 a
 *  heart), @p magic and @p rupees. Everything else zero, and the magic machine
 *  idle, so the harvest reads exactly these. */
extern "C" void OoT_TrapParityTest_ArmLive(int health, int healthCapacity, int magic, int rupees) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.ship.quest.id = QUEST_NORMAL;
    OoT_Inventory_ChangeUpgrade(UPG_WALLET, 0);
    gSaveContext.healthCapacity = (s16)healthCapacity;
    gSaveContext.health = (s16)health;
    gSaveContext.isMagicAcquired = 1;
    gSaveContext.magicLevel = 1;
    gSaveContext.magic = (s8)magic;
    gSaveContext.magicState = MAGIC_STATE_IDLE;
    gSaveContext.rupees = (s16)rupees;
}

/** OoT's live health, magic and rupees, the rupees as the harvest settles them
 *  (count plus the pending accumulator, OoT_HarvestSharedRupees). */
extern "C" void OoT_TrapParityTest_Vitals(int* health, int* magic, int* rupees) {
    *health = (int)gSaveContext.health;
    *magic = (int)gSaveContext.magic;
    *rupees = (int)gSaveContext.rupees + (int)gSaveContext.rupeeAccumulator;
}
