/**
 * MM-owned scaled framebuffer draw (#386).
 *
 * games/mm/2s2h/framebuffer_effects.c is excluded from the single-exe build
 * (its FB_* symbols collide wholesale with OoT's soh/framebuffer_effects.c), so
 * OoT's bodies survive the link. Four of the five FB_* helpers are game-agnostic
 * and correctly shared. FB_DrawFromFramebufferScaled is NOT: it scales about the
 * screen center using the ACTIVE game's framebuffer dimensions, and OoT's
 * surviving body reads OoT_gScreenWidth/Height — the wrong dimensions for MM,
 * which diverge to HiRes 576 while the Bombers' Notebook is open.
 *
 * This TU gives MM its own body, reading MM_gScreenWidth/MM_gScreenHeight. The
 * include of framebuffer_effects.h pulls in mm_framebuffer_effects_prefix.h,
 * which renames the definition below to MM_FB_DrawFromFramebufferScaled in
 * single-exe builds; MM's callers are renamed by the same shim, so they bind
 * here rather than to OoT's body. Locked by mm-fb-effects-binding
 * (games/mm/2s2h/mm_fb_effects_test.cpp). The body is otherwise a verbatim copy
 * of the excluded twin — this is a binding fix, not a change to the effect.
 *
 * Guarded to single-exe: in a standalone 2ship build the excluded twin is
 * compiled instead and owns the unprefixed FB_DrawFromFramebufferScaled, so this
 * TU must contribute nothing there or the two definitions collide.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "framebuffer_effects.h"
#include "global.h"
#include "BenPort.h"

/**
 * Similar to FB_DrawFromFramebuffer, but scales the image relative to the center
 * of the screen. This function uses opcodes from f3dex2 but may be called when
 * s2dex is loaded, such as during shrink window. Make sure f3dex2 is loaded
 * before this function is called.
 */
void FB_DrawFromFramebufferScaled(Gfx** gfxp, s32 fb, u8 alpha, float scaleX, float scaleY) {
    Gfx* gfx = *gfxp;

    gDPSetEnvColor(gfx++, 255, 255, 255, alpha);

    gDPSetOtherMode(gfx++,
                    G_AD_NOISE | G_CD_NOISE | G_CK_NONE | G_TC_FILT | G_TF_POINT | G_TT_NONE | G_TL_TILE | G_TD_CLAMP |
                        G_TP_NONE | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PRIM | G_RM_CLD_SURF | G_RM_CLD_SURF2);

    gDPSetCombineLERP(gfx++, TEXEL0, 0, ENVIRONMENT, 0, 0, 0, 0, ENVIRONMENT, TEXEL0, 0, ENVIRONMENT, 0, 0, 0, 0,
                      ENVIRONMENT);

    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    gDPSetTextureImageFB(gfx++, 0, 0, 0, fb);

    float x0 = MM_gScreenWidth * 0.5f * scaleX;
    float y0 = MM_gScreenHeight * 0.5f * scaleY;

    gDPImageRectangle(gfx++, OTRGetRectDimensionFromLeftEdge(x0) << 2, (int)(y0) << 2, 0, 0,
                      OTRGetRectDimensionFromRightEdge((float)(MM_gScreenWidth - x0)) << 2,
                      (int)((float)(MM_gScreenHeight - y0)) << 2, OTRGetGameRenderWidth(), OTRGetGameRenderHeight(),
                      G_TX_RENDERTILE, OTRGetGameRenderWidth(), OTRGetGameRenderHeight());

    *gfxp = gfx;
}

#endif // RSBS_SINGLE_EXECUTABLE
