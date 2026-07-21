#ifndef MM_FRAMEBUFFER_EFFECTS_PREFIX_H
#define MM_FRAMEBUFFER_EFFECTS_PREFIX_H

/**
 * Single-exe symbol-prefix shim for MM's scaled framebuffer draw (#386).
 *
 * Both ports define an identically-named extern-C FB_* framebuffer-effects API.
 * `games/mm/2s2h/framebuffer_effects.c` is excluded from the single-exe build
 * (games/mm/CMakeLists.txt) because its symbols collide wholesale with OoT's
 * soh/framebuffer_effects.c, so exactly one definition of each FB_* helper
 * survives the link and it is OoT's. MM's callers (z_visfbuf.c, z_play.c,
 * sys_cfb.c, ovl_En_Arrow) call them unprefixed, so MM executes OoT's bodies.
 *
 * For four of the five that is harmless: FB_CreateFramebuffers,
 * FB_CopyToFramebuffer, FB_WriteFramebufferSliceToCPU and FB_DrawFromFramebuffer
 * read only the N64-resolution constants (SCREEN_WIDTH/HEIGHT, 320x240 in both
 * ports) and the SHARED render-window state (OTRGetGameRenderWidth/Height,
 * OTRGetAspectRatio), so either port's body is correct.
 *
 * FB_DrawFromFramebufferScaled is the exception and the whole of #386. It scales
 * about the center of the screen using the ACTIVE game's framebuffer dimensions:
 *
 *     x0 = gScreenWidth  * 0.5f * scaleX;   // and the symmetric width/height
 *     y0 = gScreenHeight * 0.5f * scaleY;   // reads at the far edge
 *
 * OoT's surviving body reads OoT_gScreenWidth / OoT_gScreenHeight
 * (games/oot/soh/framebuffer_effects.c). MM's excluded twin read
 * MM_gScreenWidth / MM_gScreenHeight. Those two are SEPARATE storage
 * (oot/src/code/main.c vs mm/src/code/main.c) and diverge: MM assigns
 * gCfbWidth, which becomes HIRES_BUFFER_WIDTH (576) while the Bombers' Notebook
 * is open (sys_cfb.c / MM_Play_Draw), where OoT_gScreenWidth stays 320. When
 * MM's VisFbuf shrink-window scaled draw (z_visfbuf.c) runs against OoT's body
 * it mis-scales — the exact latent correctness landmine #386 records.
 *
 * There is no link error to catch it: only ONE FB_DrawFromFramebufferScaled
 * definition existed, so GNU ld (this repo's only working ODR gate — Windows
 * links with /FORCE:MULTIPLE by design) had nothing to reject. Same silent
 * cross-bind class already reconciled in soh/mixer.c and #382's culling family.
 *
 * The fix mirrors #382: rename MM's caller-visible FB_DrawFromFramebufferScaled
 * to MM_FB_DrawFromFramebufferScaled, and give MM its own body — reading MM's
 * OWN dimensions — in 2s2h/mm_framebuffer_effects.c. Every MM caller reaches
 * this shim through "framebuffer_effects.h", which they already include; the
 * MM body TU includes the same header, so its definition is renamed to match.
 * The soh side keeps the unprefixed name. The other four FB_* stay shared —
 * their cross-bind is correct, not a bug — so only the scaled draw is renamed.
 *
 * The ROM-free lock mm-fb-effects-binding (games/mm/2s2h/mm_fb_effects_test.cpp)
 * asserts the rename is actually in force by comparing the address MM's
 * unprefixed source-level call resolves to against OoT's surviving body.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#define FB_DrawFromFramebufferScaled MM_FB_DrawFromFramebufferScaled

#endif // RSBS_SINGLE_EXECUTABLE

#endif // MM_FRAMEBUFFER_EFFECTS_PREFIX_H
