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

#include <stddef.h>
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

/*
 * MM's warp point (2Ship's DeveloperTools/WarpPoint.cpp), drawn by SoH's Dev Tools > General rows beside SoH's own Warp
 * Points. SoH's widget stores OoT entrances in its own config block and warps OoT's play state, so MM keeps 2Ship's
 * single point in 2Ship's keys (gDeveloperTools.WarpPoint.*), which only these functions read or write. Setting and
 * warping need MM in gameplay (MM_DevTools_FrameAdvanceEnabled's condition); 2Ship's Boot to Warp Point is out of the
 * single exe (WarpPoint.cpp says why).
 */

/** 1 while Majora's Mask is the running game in gameplay with Link spawned: a warp point can be set or warped to. */
int MM_DevTools_WarpPointAvailable(void);
/** Save where Link stands as MM's warp point. Returns 1 when it did, 0 when MM is not in gameplay. */
int MM_DevTools_SetWarpPoint(void);
/** 1 when a warp point is saved. */
int MM_DevTools_WarpPointSaved(void);
/** Write "<scene>, room <n>" for the saved warp point into @p out; returns 0 (and an empty string) when none is. */
int MM_DevTools_DescribeWarpPoint(char* out, size_t size);
/** Warp Link to the saved warp point. Returns 1 when it did, 0 when none is saved or MM is not in gameplay. */
int MM_DevTools_WarpToWarpPoint(void);
/** Forget the saved warp point. */
void MM_DevTools_ClearWarpPoint(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_MM_DEV_TOOLS_BRIDGE_H
