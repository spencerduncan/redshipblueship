/**
 * @file mm_shared_kokiri_sword_test.cpp
 * MM's half of the Shared Kokiri Sword lock (#886; the rule is
 * RSBS_COMBO_FLAG_SHARED_KOKIRI_SWORD in src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship row `shared-kokiri-sword`
 * (src/common/tests/test_shared_kokiri_sword.c) drives MM's REAL
 * MM_HarvestSharedResources / MM_ApplySharedResources through. They only put
 * MM's live save in a known state and read the sword equipment value and the
 * human form's B button back; every merge decision is the production shims'.
 * They live here because only an MM TU has MM's `gSaveContext` and equipment
 * macros in scope.
 */

#include "global.h"

#include <cstring>

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The shared sword is a single-exe surface; test_runner.cpp declares these
// unconditionally, so a standalone 2ship build links.
extern "C" void MM_SharedSwordTest_ArmLive(void) {
}
extern "C" void MM_SharedSwordTest_SetSword(int equipValue) {
    (void)equipValue;
}
extern "C" int MM_SharedSwordTest_Sword(void) {
    return -1;
}
extern "C" int MM_SharedSwordTest_BButton(void) {
    return -1;
}
extern "C" int MM_SharedSwordTest_ItemFor(int equipValue) {
    (void)equipValue;
    return -1;
}
extern "C" int MM_SharedSwordTest_ProgressiveSwordGives(void) {
    return -1;
}
extern "C" int MM_SharedSwordTest_PoolSwordId(void) {
    return -1;
}
#else

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
}

#include "2s2h/Rando/Rando.h"

/** MM's live save as a cross-game MM session runs it: the 0xFF "no flash slot"
 *  sentinel and GAMEMODE_NORMAL, which Combo_SaveIsLiveFile requires before
 *  MM_HarvestSharedResources does anything at all. Human form, everything else
 *  zero (no sword, B empty), so the harvest offers no other kind a nonzero
 *  value and the pool holds only what the row puts there. */
extern "C" void MM_SharedSwordTest_ArmLive(void) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.flashSaveAvailable = true;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = ITEM_NONE;
}

/** Set MM's sword equipment value (EQUIP_VALUE_SWORD_*) and the human form's B
 *  button to match, as MM's own give (and 2Ship's RemoveItem) write them. */
extern "C" void MM_SharedSwordTest_SetSword(int equipValue) {
    SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, equipValue);
    BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) =
        (equipValue == EQUIP_VALUE_SWORD_NONE) ? ITEM_NONE : (u8)(ITEM_SWORD_KOKIRI + equipValue - 1);
}

/** MM's sword equipment value (EQUIP_VALUE_SWORD_NONE .. _GILDED). */
extern "C" int MM_SharedSwordTest_Sword(void) {
    return (int)GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
}

/** The human form's B-button item. */
extern "C" int MM_SharedSwordTest_BButton(void) {
    return (int)BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B);
}

/** MM's item id for the sword at @p equipValue, or ITEM_NONE for none. */
extern "C" int MM_SharedSwordTest_ItemFor(int equipValue) {
    return (equipValue == EQUIP_VALUE_SWORD_NONE) ? (int)ITEM_NONE : (int)(ITEM_SWORD_KOKIRI + equipValue - 1);
}

/** What MM's NEXT progressive-sword pickup resolves to from the live save, read
 *  through MM's own Rando::ConvertItem (what Rando::GiveItem runs for
 *  RI_PROGRESSIVE_SWORD): the EQUIP_VALUE_SWORD_* it gives (1 Kokiri, 2 Razor,
 *  3 Gilded), or 0 when it gives no sword (a surplus copy, resolved as junk). */
extern "C" int MM_SharedSwordTest_ProgressiveSwordGives(void) {
    switch (Rando::ConvertItem(RI_PROGRESSIVE_SWORD)) {
        case RI_SWORD_KOKIRI:
            return EQUIP_VALUE_SWORD_KOKIRI;
        case RI_SWORD_RAZOR:
            return EQUIP_VALUE_SWORD_RAZOR;
        case RI_SWORD_GILDED:
            return EQUIP_VALUE_SWORD_GILDED;
        default:
            return 0;
    }
}

/** MM's pool sword: the id every MM sword copy is placed as (RI_PROGRESSIVE_SWORD). */
extern "C" int MM_SharedSwordTest_PoolSwordId(void) {
    return (int)RI_PROGRESSIVE_SWORD;
}

#endif
