/**
 * @file mm_dev_tools_bridge.h
 * @brief How Ship of Harkinian's Dev Tools rows reach Majora's Mask's developer tools (#1238, W6-DEV).
 *
 * THE DEFECT. SoH's Frame Advance rows bind OoT_gPlayState->frameAdvCtx and hide while OoT_gPlayState is NULL, which
 * OoT's Play_Destroy leaves it whenever Majora's Mask runs; so the rows vanished exactly when a Termina report needed
 * them, although MM honours the shared gDeveloperTools.FrameAdvanceTick (z_pause.c) and has its own frameAdvCtx.
 * games/oot may not include MM's headers (ADR 0008), so the rows reach MM's play state through this header.
 *
 * THE BRIDGE. MM exports plain C functions, defined in games/mm/2s2h/MmDevToolsSingleExe.cpp; the OoT-side callers
 * (SohMenu.cpp's running-game predicates and the hosted MM rows on Dev Tools > General) reach them here and never
 * include an MM header. Every accessor answers for Majora's Mask only while it is the RUNNING game: MM_gPlayState is
 * left dangling over a suspend, so the current-game check lives on MM's side, next to the pointer it guards.
 *
 * Locked ROM-free by MenuCapabilityGating's leg 5b (games/oot/soh/soh_menu_capability_test.cpp).
 */

#ifndef RSBS_COMMON_MM_DEV_TOOLS_BRIDGE_H
#define RSBS_COMMON_MM_DEV_TOOLS_BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * MM's FrameAdvanceContext::enabled (an s32, read and written as SoH reads OoT's) while Majora's Mask is the running
 * game and has a play state; NULL otherwise (OoT running, MM suspended, MM's title or file select).
 */
int32_t* MM_DevTools_FrameAdvanceEnabled(void);

/**
 * Test-only: point MM_gPlayState at a zeroed stand-in play state (1), or put back the pointer it replaced (0). Lets a
 * ROM-free row drive the accessors above as if Majora's Mask were in gameplay.
 */
void MM_DevTools_SetStandInPlayState(int on);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_MM_DEV_TOOLS_BRIDGE_H
