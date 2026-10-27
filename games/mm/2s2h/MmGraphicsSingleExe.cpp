/**
 * @file MmGraphicsSingleExe.cpp
 * @brief MM's half of src/common/mm_graphics_bridge.h (W6-GFX, #1237): the dynamic motion blur registers SoH's
 *        Enhancements > Graphics page reads and writes, and the reapply of MM's key-chosen authentic patches.
 *
 * See the bridge header for the contract. This TU is single-exe only: upstream 2Ship's menu (BenMenu.cpp, excluded
 * here) wrote the registers in place and called GfxPatcher_ApplyGeometryIssuePatches from its row callbacks.
 */

#include "global.h"

#include "2s2h/Enhancements/GfxPatcher/AuthenticGfxPatches.h"

#include "context.h"            // src/common: Context_GetCurrentGame
#include "mm_graphics_bridge.h" // src/common

namespace {
// 1 once MM's asset phase has applied the authentic patches (MM_Rando_InitAssets). Never cleared: the phase latches
// once per process, and the patched display lists stay patched across a suspend.
int sGfxPatchesApplied = 0;
// 1 when a patch key changed while MM was not the running game; MM_Game_Resume reapplies it under MM's owner.
int sGfxPatchesPending = 0;

// The patches the three keys choose, each an idempotent patch-or-unpatch on its key's current value
// (AuthenticGfxPatches.cpp). The other authentic patches are unconditional and were applied once for good.
void ApplyKeyedPatches() {
    GfxPatcher_ApplyOverflowTexturePatches();
    GfxPatcher_ApplyGeometryIssuePatches();
}
} // namespace

extern "C" int MM_MotionBlur_RegistersLive(void) {
    return (Context_GetCurrentGame() == GAME_MM && MM_gPlayState != NULL && gRegEditor != NULL) ? 1 : 0;
}

extern "C" int MM_MotionBlur_GetDynamicEnabled(void) {
    return MM_MotionBlur_RegistersLive() && R_MOTION_BLUR_ENABLED != 0 ? 1 : 0;
}

extern "C" void MM_MotionBlur_SetDynamicEnabled(int enabled) {
    if (MM_MotionBlur_RegistersLive()) {
        R_MOTION_BLUR_ENABLED = enabled != 0 ? 1 : 0;
    }
}

extern "C" int MM_MotionBlur_GetDynamicAlpha(void) {
    return MM_MotionBlur_RegistersLive() ? (int)R_MOTION_BLUR_ALPHA : 0;
}

extern "C" void MM_MotionBlur_SetDynamicAlpha(int alpha) {
    if (MM_MotionBlur_RegistersLive()) {
        R_MOTION_BLUR_ALPHA = (s16)(alpha < 0 ? 0 : (alpha > 255 ? 255 : alpha));
    }
}

extern "C" int MM_GfxPatches_Reapply(void) {
    if (!sGfxPatchesApplied) {
        // MM's asset phase has not run in this process; it reads the three keys itself when it does.
        return 0;
    }
    if (Context_GetCurrentGame() != GAME_MM) {
        // The display lists are MM resources: resolved now, they would land under OoT's resource cache owner (#1000).
        sGfxPatchesPending = 1;
        return 0;
    }
    sGfxPatchesPending = 0;
    ApplyKeyedPatches();
    return 1;
}

extern "C" void MM_GfxPatches_SetApplied(int applied) {
    sGfxPatchesApplied = applied != 0 ? 1 : 0;
}

extern "C" int MM_GfxPatches_ApplyPending(void) {
    if (!sGfxPatchesApplied || !sGfxPatchesPending) {
        return 0;
    }
    sGfxPatchesPending = 0;
    ApplyKeyedPatches();
    return 1;
}
