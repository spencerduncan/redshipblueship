/**
 * @file mm_trap_parity_test.cpp
 * MM's half of the trap-type parity lock (#1424; OoTMM's Fire, Drain,
 * Anti-Magic, Knockback and Rupoor traps in MM's randomizer).
 *
 * THE SHIM HELPERS the redship row `trap-type-parity`
 * (src/common/tests/test_trap_type_parity.c) drives MM's REAL
 * MM_HarvestSharedResources through, around the traps' own save half
 * (Rando/MiscBehavior/Traps.cpp, MM_Rando_Traps_TestApplySaveEffect). They only
 * put MM's live save in a known state and read it back; what the pool keeps is
 * the production harvest's. They live here because only an MM TU has MM's
 * `gSaveContext` and week-event macros in scope.
 */

#include "global.h"

#include <cstring>

#ifdef RSBS_SINGLE_EXECUTABLE

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
}

/** MM's live save as a cross-game MM session runs it (the 0xFF "no flash slot"
 *  sentinel and GAMEMODE_NORMAL, which the harvest's live-file gate requires;
 *  mm_shared_kokiri_sword_test.cpp) in human form, with a single magic meter, a
 *  wallet of tier 0 (99 rupees), @p healthCapacity and @p health in health units
 *  (0x10 a heart), @p magic, @p rupees and the Chateau Romani's endless magic
 *  when @p chateau. Everything else zero, and the magic machine idle, so the
 *  harvest reads exactly these. */
extern "C" void MM_TrapParityTest_ArmLive(int health, int healthCapacity, int magic, int rupees, int chateau) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.flashSaveAvailable = true;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = ITEM_NONE;
    SavePlayerData& player = gSaveContext.save.saveInfo.playerData;
    player.healthCapacity = (s16)healthCapacity;
    player.health = (s16)health;
    player.isMagicAcquired = true;
    player.magicLevel = 1;
    player.magic = (s8)magic;
    player.rupees = (s16)rupees;
    gSaveContext.magicState = MAGIC_STATE_IDLE;
    if (chateau != 0) {
        SET_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI);
    }
}

/** MM's live health, magic, rupees (as the harvest settles them: the count plus
 *  the pending accumulator, MM_HarvestSharedRupees) and the Chateau Romani flag. */
extern "C" void MM_TrapParityTest_Vitals(int* health, int* magic, int* rupees, int* chateau) {
    const SavePlayerData& player = gSaveContext.save.saveInfo.playerData;
    *health = (int)player.health;
    *magic = (int)player.magic;
    *rupees = (int)player.rupees + (int)gSaveContext.rupeeAccumulator;
    *chateau = CHECK_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI) ? 1 : 0;
}

/** MM's PLAYER_FORM_* values the Fire Trap row asks about. */
extern "C" int MM_TrapParityTest_Form(int which) {
    switch (which) {
        case 0:
            return (int)PLAYER_FORM_HUMAN;
        case 1:
            return (int)PLAYER_FORM_GORON;
        case 2:
            return (int)PLAYER_FORM_ZORA;
        case 3:
            return (int)PLAYER_FORM_DEKU;
        default:
            return (int)PLAYER_FORM_FIERCE_DEITY;
    }
}

#endif // RSBS_SINGLE_EXECUTABLE
