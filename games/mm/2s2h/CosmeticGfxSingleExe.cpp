/**
 * CosmeticEditor gfx-wrapper implementations for single-executable builds.
 *
 * BenGui/ (including CosmeticEditor.cpp) is excluded from single-exe builds
 * (games/mm/CMakeLists.txt "All GUI components" filter), but MM's HUD draw
 * path calls these wrappers on every HUD-visible frame and CONSUMES the Gfx*
 * return value as the new display-list write pointer, e.g. z_parameter.c
 * MM_Interface_DrawItemButtons:
 *
 *     OVERLAY_DISP = Gfx_DrawTexRectIA8_DropShadowOverride(OVERLAY_DISP, ...);
 *
 * The old placeholders in src/common/mm_stubs.c declared these as VOID with
 * truncated parameter lists, so the caller consumed whatever garbage the
 * stub left in RAX as its new write pointer. First HUD-visible MM frame in
 * a single-exe build then wrote a gfx command through that garbage — the
 * int-gameplay-roundtrip harness caught it as a WRITE access violation at
 * 0xA7 inside MM_Interface_DrawItemButtons on the OoT->MM leg (2026-07-17,
 * docs/ci-gameplay-repro-postmortem.md). Earlier crashes in the same class
 * (#356/#357/#367) masked this one: no single-exe build had survived far
 * enough into HUD-visible MM gameplay to reach it.
 *
 * This TU is compiled against 2s2h/BenGui/CosmeticEditor.h so every
 * signature below is compiler-checked against the declarations the real
 * callers use — the failure mode being fixed (stub signature drift) becomes
 * a compile error instead of a runtime wild write. The color-override
 * lookup is a no-op here (the editor UI is excluded), so the wrappers
 * delegate straight to the base drawers in games/mm/src/code/z_parameter.c,
 * which ARE part of the single-exe link. HUD rendering is therefore
 * identical to 2ship-with-default-cosmetics.
 *
 * CosmeticGfxStub_RunHeadless() is the ROM-free lock (CTest "redship" label,
 * row cosmetic-gfx-stub in src/common/test_runner.cpp): it builds a display
 * list into a poisoned buffer through the wrappers and asserts the returned
 * write pointer advanced sanely and commands were actually written — the
 * exact contract the void stubs violated. Display-list BUILDING never
 * dereferences the texture pointer (that happens at execute time), so a
 * dummy texture address is safe headless.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <ship/window/gui/GuiWindow.h>

#include "BenPort.h"
#include "global.h"

#include "2s2h/BenGui/CosmeticEditor.h"

#include <cstdio>
#include <cstring>

extern "C" {
// Base drawers (games/mm/src/code/z_parameter.c) — the same local extern
// declarations CosmeticEditor.cpp itself compiles against.
Gfx* Gfx_DrawTexRectIA8_DropShadow(Gfx* gfx, TexturePtr texture, s16 textureWidth, s16 textureHeight, s16 rectLeft,
                                   s16 rectTop, s16 rectWidth, s16 rectHeight, u16 dsdx, u16 dtdy, s16 r, s16 g, s16 b,
                                   s16 a);
Gfx* Gfx_DrawRect_DropShadow(Gfx* gfx, s16 rectLeft, s16 rectTop, s16 rectWidth, s16 rectHeight, u16 dsdx, u16 dtdy,
                             s16 r, s16 g, s16 b, s16 a);
Gfx* Gfx_DrawTexRectIA16_DropShadow(Gfx* gfx, TexturePtr texture, s16 textureWidth, s16 textureHeight, s16 rectLeft,
                                    s16 rectTop, s16 rectWidth, s16 rectHeight, u16 dsdx, u16 dtdy, s16 r, s16 g, s16 b,
                                    s16 a);
Gfx* Gfx_DrawTexRectIA8_DropShadowOffset(Gfx* gfx, TexturePtr texture, s16 textureWidth, s16 textureHeight,
                                         s16 rectLeft, s16 rectTop, s16 rectWidth, s16 rectHeight, u16 dsdx, u16 dtdy,
                                         s16 r, s16 g, s16 b, s16 a, s32 masks, s32 rects);

Color_RGBA8 CosmeticEditor_GetChangedColor(u8 r, u8 g, u8 b, u8 a, u8 elementId) {
    (void)elementId;
    Color_RGBA8 c = { r, g, b, a };
    return c;
}

void gDPSetPrimColorOverride(Gfx* pkt, u8 m, u8 l, u8 r, u8 g, u8 b, u8 a, u8 elementId) {
    (void)elementId;
    gDPSetPrimColor(pkt, m, l, r, g, b, a);
}

void gDPSetEnvColorOverride(Gfx* pkt, u8 r, u8 g, u8 b, u8 a, u8 elementId) {
    (void)elementId;
    gDPSetEnvColor(pkt, r, g, b, a);
}

Gfx* Gfx_DrawTexRectIA8_DropShadowOverride(Gfx* pkt, TexturePtr texture, s16 textureWidth, s16 textureHeight,
                                           s16 rectLeft, s16 rectTop, s16 rectWidth, s16 rectHeight, u16 dsdx, u16 dtdy,
                                           s16 r, s16 g, s16 b, s16 a, u8 elementId) {
    (void)elementId;
    return Gfx_DrawTexRectIA8_DropShadow(pkt, texture, textureWidth, textureHeight, rectLeft, rectTop, rectWidth,
                                         rectHeight, dsdx, dtdy, r, g, b, a);
}

Gfx* Gfx_DrawRect_DropShadowOverride(Gfx* pkt, s16 rectLeft, s16 rectTop, s16 rectWidth, s16 rectHeight, u16 dsdx,
                                     u16 dtdy, s16 r, s16 g, s16 b, s16 a, u8 elementId) {
    (void)elementId;
    return Gfx_DrawRect_DropShadow(pkt, rectLeft, rectTop, rectWidth, rectHeight, dsdx, dtdy, r, g, b, a);
}

Gfx* Gfx_DrawTexRectIA16_DropShadowOverride(Gfx* pkt, TexturePtr texture, s16 textureWidth, s16 textureHeight,
                                            s16 rectLeft, s16 rectTop, s16 rectWidth, s16 rectHeight, u16 dsdx,
                                            u16 dtdy, s16 r, s16 g, s16 b, s16 a, u8 elementId) {
    (void)elementId;
    return Gfx_DrawTexRectIA16_DropShadow(pkt, texture, textureWidth, textureHeight, rectLeft, rectTop, rectWidth,
                                          rectHeight, dsdx, dtdy, r, g, b, a);
}

Gfx* Gfx_DrawTexRectIA8_DropShadowOffsetOverride(Gfx* pkt, TexturePtr texture, s16 textureWidth, s16 textureHeight,
                                                 s16 rectLeft, s16 rectTop, s16 rectWidth, s16 rectHeight, u16 dsdx,
                                                 u16 dtdy, s16 r, s16 g, s16 b, s16 a, s32 masks, s32 rects,
                                                 u8 elementId) {
    (void)elementId;
    return Gfx_DrawTexRectIA8_DropShadowOffset(pkt, texture, textureWidth, textureHeight, rectLeft, rectTop, rectWidth,
                                               rectHeight, dsdx, dtdy, r, g, b, a, masks, rects);
}

/**
 * ROM-free lock for the wrapper contract (see file header). Returns 0 on
 * pass, non-zero on the first failed check.
 */
int CosmeticGfxStub_RunHeadless(void) {
    // Big enough for the longest wrapper's command sequence, poisoned so
    // "wrote nothing" is detectable.
    static Gfx buffer[256];
    const unsigned char kPoison = 0xAA;
    // A dummy, never-dereferenced texture address (display-list building
    // only embeds the pointer).
    TexturePtr dummyTexture = (TexturePtr)0x12345678;

    struct Case {
        const char* name;
        Gfx* (*run)(Gfx* start, TexturePtr tex);
    };

    static const Case cases[] = {
        { "Gfx_DrawTexRectIA8_DropShadowOverride",
          [](Gfx* start, TexturePtr tex) {
              return Gfx_DrawTexRectIA8_DropShadowOverride(start, tex, 32, 32, 100, 100, 29, 29, 1024, 1024, 100, 255,
                                                           120, 167, 0);
          } },
        { "Gfx_DrawRect_DropShadowOverride",
          [](Gfx* start, TexturePtr tex) {
              (void)tex;
              return Gfx_DrawRect_DropShadowOverride(start, 100, 100, 27, 27, 1024, 1024, 255, 240, 0, 167, 0);
          } },
        { "Gfx_DrawTexRectIA16_DropShadowOverride",
          [](Gfx* start, TexturePtr tex) {
              return Gfx_DrawTexRectIA16_DropShadowOverride(start, tex, 32, 32, 100, 100, 29, 29, 1024, 1024, 100, 255,
                                                            120, 167, 0);
          } },
        { "Gfx_DrawTexRectIA8_DropShadowOffsetOverride",
          [](Gfx* start, TexturePtr tex) {
              return Gfx_DrawTexRectIA8_DropShadowOffsetOverride(start, tex, 32, 32, 100, 100, 29, 29, 1024, 1024, 100,
                                                                 255, 120, 167, 5, 3, 0);
          } },
    };

    for (const Case& c : cases) {
        memset(buffer, kPoison, sizeof(buffer));
        Gfx* end = c.run(buffer, dummyTexture);

        // The void-stub failure mode: return value not a sane advancement of
        // the input write pointer.
        if (end <= buffer || end > buffer + (sizeof(buffer) / sizeof(buffer[0]))) {
            printf("[TEST] FAIL: %s returned %p for buffer %p (not a sane display-list advance)\n", c.name, (void*)end,
                   (void*)buffer);
            return 1;
        }

        // And it must have actually written commands, not just advanced.
        if (memcmp(buffer, &kPoison, 1) == 0) {
            unsigned char stillPoisoned = 1;
            for (size_t i = 0; i < sizeof(Gfx); i++) {
                if (((unsigned char*)buffer)[i] != kPoison) {
                    stillPoisoned = 0;
                    break;
                }
            }
            if (stillPoisoned) {
                printf("[TEST] FAIL: %s advanced the pointer but wrote no commands\n", c.name);
                return 1;
            }
        }
    }

    // The in-place color writers must write into the packet they are given.
    {
        memset(buffer, kPoison, sizeof(buffer));
        gDPSetPrimColorOverride(buffer, 0, 0, 10, 20, 30, 40, 0);
        unsigned char stillPoisoned = 1;
        for (size_t i = 0; i < sizeof(Gfx); i++) {
            if (((unsigned char*)buffer)[i] != kPoison) {
                stillPoisoned = 0;
                break;
            }
        }
        if (stillPoisoned) {
            printf("[TEST] FAIL: gDPSetPrimColorOverride wrote nothing into its packet\n");
            return 1;
        }

        memset(buffer, kPoison, sizeof(buffer));
        gDPSetEnvColorOverride(buffer, 10, 20, 30, 40, 0);
        stillPoisoned = 1;
        for (size_t i = 0; i < sizeof(Gfx); i++) {
            if (((unsigned char*)buffer)[i] != kPoison) {
                stillPoisoned = 0;
                break;
            }
        }
        if (stillPoisoned) {
            printf("[TEST] FAIL: gDPSetEnvColorOverride wrote nothing into its packet\n");
            return 1;
        }
    }

    printf("[TEST] cosmetic-gfx-stub: all wrappers write commands and advance the display list\n");
    return 0;
}

} // extern "C"

#endif // RSBS_SINGLE_EXECUTABLE
