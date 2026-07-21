/**
 * ROM-free lock for MM's scaled framebuffer-draw binding (#386). CTest label
 * "redship", row mm-fb-effects-binding in src/common/test_runner.cpp.
 *
 * What was broken: games/mm/2s2h/framebuffer_effects.c is excluded from the
 * single-exe build (games/mm/CMakeLists.txt) because its FB_* symbols collide
 * wholesale with OoT's soh/framebuffer_effects.c, so exactly one definition of
 * each FB_* helper survived the link and it was OoT's. MM's callers write the
 * FB_* names unprefixed, so MM executed OoT's bodies.
 *
 * Four of the five are game-agnostic (they read only the 320x240 N64 constants
 * and the shared render window), so sharing OoT's body is correct. The fifth,
 * FB_DrawFromFramebufferScaled, scales about the screen center using the ACTIVE
 * game's framebuffer dimensions — OoT's surviving body reads OoT_gScreenWidth /
 * OoT_gScreenHeight, which are SEPARATE storage from MM_gScreenWidth /
 * MM_gScreenHeight and diverge (MM goes to HiRes 576 while the Bombers'
 * Notebook is open; OoT stays 320). MM's VisFbuf shrink-window scaled draw
 * (z_visfbuf.c) then mis-scales. No link error can catch it: only ONE
 * definition survived, so GNU ld — the repo's only working ODR gate (Windows
 * links /FORCE:MULTIPLE) — had nothing to reject. Silent mis-scale.
 *
 * The fix (mirroring #382's culling family) renames MM's caller-visible
 * FB_DrawFromFramebufferScaled to MM_FB_DrawFromFramebufferScaled via
 * include/mm_framebuffer_effects_prefix.h and gives MM its own body reading MM's
 * own dimensions in 2s2h/mm_framebuffer_effects.c.
 *
 * This lock is link-time, not behavioral: FB_DrawFromFramebufferScaled emits
 * gfx commands through OTRGetGameRenderWidth()/OTRGetRectDimension*(), which
 * dereference the Fast3dWindow — absent in the display-free unit-test tier, so
 * calling it here would crash rather than test. Instead it proves MM's
 * unprefixed source-level call resolves to a DIFFERENT body than OoT's
 * surviving one (assertion 1), and that the two ports' screen dimensions are
 * genuinely separate storage — the precondition that makes binding the wrong
 * body a mis-scale (assertion 2).
 *
 * Assertion 1 is robust against identical-code-folding: MM's body reads the
 * MM_gScreen* symbols and OoT's reads the OoT_gScreen* symbols, so the two
 * bodies carry different relocations and cannot be merged into one address by
 * the linker. Before the fix the two names resolve to the same OoT body and the
 * assertion fails loudly; that is the regression this row exists to catch.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "mm_framebuffer_effects_prefix.h" // activates the FB_DrawFromFramebufferScaled rename

#include <cstdint>
#include <cstdio>

// With the rename in force this declares MM_FB_DrawFromFramebufferScaled — the
// body an ordinary MM caller's unprefixed source-level call binds to. Declared
// opaque and extern "C" (C-linkage, no parameter types in the symbol name);
// only ever addressed here, never called.
extern "C" void FB_DrawFromFramebufferScaled(void);
static void* const sMmBound = reinterpret_cast<void*>(&FB_DrawFromFramebufferScaled);

// Drop the rename so the unprefixed OoT-side symbol can be named directly.
#undef FB_DrawFromFramebufferScaled

extern "C" {
// OoT's surviving body (games/oot/soh/framebuffer_effects.c).
void FB_DrawFromFramebufferScaled(void);

// The two ports' screen-dimension storage. Separate globals
// (mm/src/code/main.c vs oot/src/code/main.c); the bug is that OoT's body reads
// the OoT pair for MM's draw.
extern int32_t MM_gScreenWidth;
extern int32_t MM_gScreenHeight;
extern int32_t OoT_gScreenWidth;
extern int32_t OoT_gScreenHeight;
}

static void* const sOotBound = reinterpret_cast<void*>(&FB_DrawFromFramebufferScaled);

namespace {

#define FB_ASSERT(cond, msg)                                              \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

} // namespace

extern "C" int MM_FbEffectsBinding_RunHeadless(void) {
    // ---- 1. MM binds its own body, not OoT's shared one -----------------
    printf("[TEST] FB_DrawFromFramebufferScaled — MM %p, OoT %p\n", sMmBound, sOotBound);
    FB_ASSERT(sMmBound != sOotBound,
              "MM's FB_DrawFromFramebufferScaled still binds OoT's body (reads OoT_gScreenWidth) — #386");

    // ---- 2. The two ports' screen dimensions are separate storage -------
    // If these ever share one global the whole fault class evaporates and this
    // row should be revisited; while they differ, binding OoT's body mis-scales
    // MM's draw whenever MM's dimensions diverge (HiRes notebook = 576 vs 320).
    printf("[TEST] gScreenWidth storage — MM %p, OoT %p\n", (void*)&MM_gScreenWidth, (void*)&OoT_gScreenWidth);
    FB_ASSERT((void*)&MM_gScreenWidth != (void*)&OoT_gScreenWidth,
              "MM and OoT gScreenWidth are the same storage — the premise of #386 no longer holds, revisit");
    FB_ASSERT((void*)&MM_gScreenHeight != (void*)&OoT_gScreenHeight,
              "MM and OoT gScreenHeight are the same storage — the premise of #386 no longer holds, revisit");

    printf("[TEST] PASS: mm-fb-effects-binding — MM's scaled framebuffer draw binds its own body against MM's "
           "dimensions\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
