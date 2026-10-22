/**
 * @file mm_shared_ocarina_buttons_test.cpp
 * MM's half of the Shared Ocarina Buttons locks (#1067; the rule is
 * RSBS_XITEM_SHARED_OCARINA_BUTTONS in src/common/foreign_items.h).
 *
 * THE SHIM HELPERS the redship rows `shared-ocarina-buttons-pairs` and
 * `shared-ocarina-buttons-arrival` (src/common/tests/test_shared_ocarina_buttons.c)
 * drive MM's REAL MM_HarvestSharedResources / MM_ApplySharedResources through.
 * They only put MM's live save in a known state (after
 * MM_SharedSwordTest_ArmLive) and read the button bits back; every merge
 * decision is the production shims' and the family members
 * MM_RegisterSharedOcarinaButtonFamilies registers (GameExports_SingleExe.cpp).
 * They live here because only an MM TU has MM's `gSaveContext`, its RI_* ids
 * and its RANDO_INF_* flags in scope.
 *
 * Buttons are numbered in OoT's order, the row's numbering for both games: 0 A,
 * 1 C-Up, 2 C-Down, 3 C-Left, 4 C-Right. MM's own order differs (RI_OCARINA_BUTTON_A,
 * _C_DOWN, _C_RIGHT, _C_LEFT, _C_UP, and RANDO_INF_OBTAINED_OCARINA_BUTTON_* in
 * the same order), so every helper maps through MMButtonItem.
 */

#include "global.h"

#include <cstring>

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The shared buttons are a single-exe surface; test_runner.cpp declares these
// unconditionally, so a standalone 2ship build links.
extern "C" void MM_SharedButtonsTest_Set(int shuffle, unsigned heldMask) {
    (void)shuffle;
    (void)heldMask;
}
extern "C" unsigned MM_SharedButtonsTest_Held(void) {
    return 0u;
}
extern "C" int MM_SharedButtonsTest_PoolId(int button) {
    (void)button;
    return -1;
}
#else

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
}

#include "2s2h/Rando/Rando.h"

namespace {

constexpr int kButtonCount = 5;

/** MM's item for button @p button of the row's (OoT's) order. */
RandoItemId MMButtonItem(int button) {
    static const RandoItemId kItems[kButtonCount] = { RI_OCARINA_BUTTON_A, RI_OCARINA_BUTTON_C_UP,
                                                      RI_OCARINA_BUTTON_C_DOWN, RI_OCARINA_BUTTON_C_LEFT,
                                                      RI_OCARINA_BUTTON_C_RIGHT };
    return kItems[button];
}

/** The randoInf flag MM's own give sets for button @p button (GiveItem.cpp:
 *  RANDO_INF_OBTAINED_OCARINA_BUTTON_A + (item - RI_OCARINA_BUTTON_A)). */
int MMButtonFlag(int button) {
    return RANDO_INF_OBTAINED_OCARINA_BUTTON_A + ((int)MMButtonItem(button) - (int)RI_OCARINA_BUTTON_A);
}

} // namespace

/** MM's live save as a randomizer file with the button shuffle @p shuffle (0
 *  off, 1 on) and exactly the buttons in @p heldMask (bit i = button i) owned.
 *  Call after MM_SharedSwordTest_ArmLive. */
extern "C" void MM_SharedButtonsTest_Set(int shuffle, unsigned heldMask) {
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    gSaveContext.save.shipSaveInfo.rando.randoSaveOptions[RO_SHUFFLE_OCARINA_BUTTONS] =
        (shuffle != 0) ? RO_GENERIC_YES : RO_GENERIC_OFF;
    for (int i = 0; i < kButtonCount; i++) {
        const int flag = MMButtonFlag(i);
        const u16 bit = (u16)(1u << (flag & 0xF));
        if ((heldMask & (1u << i)) != 0u) {
            gSaveContext.save.shipSaveInfo.rando.randoInf[flag >> 4] |= bit;
        } else {
            gSaveContext.save.shipSaveInfo.rando.randoInf[flag >> 4] &= (u16)~bit;
        }
    }
}

/** The buttons MM's live save owns, as a mask (bit i = button i). */
extern "C" unsigned MM_SharedButtonsTest_Held(void) {
    unsigned mask = 0;
    for (int i = 0; i < kButtonCount; i++) {
        const int flag = MMButtonFlag(i);
        if ((gSaveContext.save.shipSaveInfo.rando.randoInf[flag >> 4] & (1u << (flag & 0xF))) != 0u) {
            mask |= 1u << i;
        }
    }
    return mask;
}

/** MM's pool id for button @p button (RI_OCARINA_BUTTON_*). */
extern "C" int MM_SharedButtonsTest_PoolId(int button) {
    return (int)MMButtonItem(button);
}

#endif
