/**
 * @file oot_ztarget_mirror_test.cpp
 * OoT's half of the one-targeting-mode lock (#1351).
 *
 * THE HELPER the redship row `mm-startup-restore`
 * (games/mm/2s2h/mm_resume_state_test.cpp) arms OoT's frozen half with: an OoT
 * SaveContext whose Z-targeting mode is the row's, frozen as OoT's departure
 * freezes it. MM's arrival then reads the mode back through the production
 * accessor (OoT_Combo_FrozenZTargetSetting, GameExports_SingleExe.cpp). It
 * lives here because only an OoT TU knows OoT's SaveContext layout. The blob is
 * built off to the side, so the live gSaveContext (which the two games share,
 * src/common/unified_save.c) is left as the row has it.
 */

#include <memory>

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (oot_shared_kokiri_sword_test.cpp says why).
#include <z64.h>

#include "context.h" // src/common: Context_FreezeState

/** Freeze an OoT half whose Z-targeting mode is Hold (@p hold 1) or Switch (0),
 *  every other byte zero. */
extern "C" void OoT_ZTargetMirrorTest_FreezeOoTHalf(int hold) {
    const auto half = std::make_unique<SaveContext>();
    half->zTargetSetting = (u8)(hold != 0 ? 1 : 0);
    Context_FreezeState(GAME_OOT, 0, half.get(), sizeof(SaveContext));
}
