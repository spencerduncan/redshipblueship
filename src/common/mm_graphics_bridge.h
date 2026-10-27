/**
 * @file mm_graphics_bridge.h
 * @brief How SoH's Enhancements page reaches the two pieces of Majora's Mask graphics state that are not CVars
 *        (W6-GFX, #1237): the dynamic motion blur registers, and the authentic display-list patches MM applies once.
 *
 * MM defines every function below in games/mm/2s2h/MmGraphicsSingleExe.cpp. The OoT-side callers (the hosted rows in
 * games/oot/soh/SohGui/HostedMm/SohMenuHostedMmGraphics.cpp and SoH's Fix Out of Bounds Textures row) reach them
 * through this header and never include an MM header, the mm_volume_bridge.h shape.
 *
 * DYNAMIC MOTION BLUR. MM's Play_DrawMotionBlur (z_play.c) blurs while R_MOTION_BLUR_ENABLED is set, at
 * R_MOTION_BLUR_ALPHA. Those are registers (SREG 91 and 90 in MM's gRegEditor), not CVars: MM's own scenes and
 * cutscenes set them (Play_EnableMotionBlur, z_demo.c), and 2Ship's menu wrote them in place. They are meaningful only
 * while Majora's Mask is the running game with a play state, so the rows read them through MM_MotionBlur_RegistersLive
 * and draw disabled with a reason otherwise (ADR 0004's CAPABILITY state, #497). Outside that window the getters return
 * 0 and the setters do nothing.
 *
 * THE AUTHENTIC PATCHES. MM applies its display-list patches once, in its asset phase (MM_Rando_InitAssets,
 * GameExports_SingleExe.cpp), and three keys choose what they patch: Fix Scene Geometry Seams, Disable Scene Geometry
 * Distance Check and SoH's shared Fix Out of Bounds Textures. 2Ship's menu reapplied them on change; here a change
 * reapplies them through MM_GfxPatches_Reapply, and only under MM's resource cache owner (#1000): at once while MM
 * runs, otherwise at MM's next resume (MM_GfxPatches_ApplyPending). Before MM's asset phase has run there is nothing to
 * reapply: the phase reads the keys itself.
 */

#ifndef RSBS_COMMON_MM_GRAPHICS_BRIDGE_H
#define RSBS_COMMON_MM_GRAPHICS_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/** 1 while Majora's Mask is the running game and has a play state, so its motion blur registers mean something. */
int MM_MotionBlur_RegistersLive(void);
/** R_MOTION_BLUR_ENABLED as 0 or 1; 0 when the registers are not live. */
int MM_MotionBlur_GetDynamicEnabled(void);
/** Write R_MOTION_BLUR_ENABLED; no-op when the registers are not live. */
void MM_MotionBlur_SetDynamicEnabled(int enabled);
/** R_MOTION_BLUR_ALPHA (0-255); 0 when the registers are not live. */
int MM_MotionBlur_GetDynamicAlpha(void);
/** Write R_MOTION_BLUR_ALPHA, clamped to 0-255; no-op when the registers are not live. */
void MM_MotionBlur_SetDynamicAlpha(int alpha);

/**
 * Reapply MM's key-chosen authentic patches (geometry seams, geometry distance check, out-of-bounds textures) after one
 * of their keys changed. Returns 1 when it reapplied them now (MM is running), 0 when it deferred them to MM's next
 * resume or when MM's asset phase has not run yet (which then reads the keys itself).
 */
int MM_GfxPatches_Reapply(void);
/** Marks MM's asset phase as having applied the patches (GameExports_SingleExe.cpp, right after it does). */
void MM_GfxPatches_SetApplied(int applied);
/** Called from MM_Game_Resume: reapply a change deferred while MM was suspended. Returns 1 when it reapplied. */
int MM_GfxPatches_ApplyPending(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_MM_GRAPHICS_BRIDGE_H
