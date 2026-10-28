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
 * a compile error instead of a runtime wild write. The wrappers delegate
 * to the base drawers in games/mm/src/code/z_parameter.c, which ARE part of
 * the single-exe link.
 *
 * The colour lookup behind them, CosmeticEditor_GetChangedColor, is real
 * since W6-COS-A (#1239), ported from 2Ship's reader (CosmeticEditor.cpp,
 * whose editor window stays excluded): it returns the changed colour by
 * element id from kMmCosmeticElements below. 2Ship's editor is not hosted;
 * SoH's one Cosmetics Editor drives MM instead. An element with an SoH twin
 * reads the twin's .Changed / .Value (one colour for both games, the
 * one-game reading of #1239's hosting choice); an element with none reads
 * MM's own gCosmetic.<element>.Color / .Changed, which an MM row in SoH's
 * Cosmetics Editor writes. The lookup reads the CVars live: it does not wait
 * for OoT's frame or for the #1040 resource-cache owner gate, which hold
 * SoH's own GFX patches only.
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

#include <libultraship/bridge/consolevariablebridge.h>

#include "cvar_shared_keys.h" // src/common: RSBS_CVAR_MM_COSMETIC_PREFIX (W6-COS-A, #1239)

#include <cstdio>
#include <cstring>
#include <string>

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

} // extern "C"

namespace {

/**
 * One MM colour element the lookup changes (W6-COS-A, #1239). MM's own keys are composed from
 * RSBS_CVAR_MM_COSMETIC_PREFIX exactly as 2Ship's COSMETIC_EDITOR_ELEMENT composes them, so a 2Ship config's colours
 * keep their spelling. A twinned element reads SoH's Cosmetics Editor row (sohValue / sohChanged) and, when that row
 * is unchanged, an optional fallback row (SoH's C Buttons under its per-button rows); MM's own keys are then read by
 * nothing, as for every kTranslatedKeys pair. An MM-only element (sohValue null) reads its own keys.
 */
struct MmCosmeticElement {
    u8 id; ///< CosmeticEditorElementID
    const char* mmColor;
    const char* mmChanged;
    const char* sohValue;
    const char* sohChanged;
    const char* sohFallbackValue;
    const char* sohFallbackChanged;
};

#define MM_COSMETIC_OWN(id, name)                                                                              \
    {                                                                                                          \
        id, RSBS_CVAR_MM_COSMETIC_PREFIX name ".Color", RSBS_CVAR_MM_COSMETIC_PREFIX name ".Changed", nullptr, \
            nullptr, nullptr, nullptr                                                                          \
    }
#define MM_COSMETIC_TWIN(id, name, soh)                                                                             \
    {                                                                                                               \
        id, RSBS_CVAR_MM_COSMETIC_PREFIX name ".Color", RSBS_CVAR_MM_COSMETIC_PREFIX name ".Changed", soh ".Value", \
            soh ".Changed", nullptr, nullptr                                                                        \
    }
#define MM_COSMETIC_TWIN_OR(id, name, soh, fallback)                                                                \
    {                                                                                                               \
        id, RSBS_CVAR_MM_COSMETIC_PREFIX name ".Color", RSBS_CVAR_MM_COSMETIC_PREFIX name ".Changed", soh ".Value", \
            soh ".Changed", fallback ".Value", fallback ".Changed"                                                  \
    }

// The one element table. Every pair it reads is a kTranslatedKeys row (src/common/cvar_shared_keys.h) and every
// MM-only key an MM row's; the classification lock's colour-family leg (3c-3) holds both. Later lanes insert only at
// their markers.
// clang-format off
const MmCosmeticElement kMmCosmeticElements[] = {
    // Buttons and HUD: SoH's HUD tab. OoT's HUD reads a C button's own row over SoH's C Buttons row (z_parameter.c).
    MM_COSMETIC_TWIN(COSMETIC_ELEMENT_B_BUTTON,          "Buttons.B",      "gCosmetics.HUD.BButton"),
    MM_COSMETIC_TWIN(COSMETIC_ELEMENT_A_BUTTON,          "Buttons.A",      "gCosmetics.HUD.AButton"),
    MM_COSMETIC_TWIN_OR(COSMETIC_ELEMENT_C_LEFT_BUTTON,  "Buttons.CLeft",  "gCosmetics.HUD.CLeftButton",  "gCosmetics.HUD.CButtons"),
    MM_COSMETIC_TWIN_OR(COSMETIC_ELEMENT_C_DOWN_BUTTON,  "Buttons.CDown",  "gCosmetics.HUD.CDownButton",  "gCosmetics.HUD.CButtons"),
    MM_COSMETIC_TWIN_OR(COSMETIC_ELEMENT_C_RIGHT_BUTTON, "Buttons.CRight", "gCosmetics.HUD.CRightButton", "gCosmetics.HUD.CButtons"),
    MM_COSMETIC_TWIN(COSMETIC_ELEMENT_D_PAD_BUTTON,      "Buttons.DPad",   "gCosmetics.HUD.Dpad"),
    MM_COSMETIC_TWIN(COSMETIC_ELEMENT_START_BUTTON,      "Buttons.Start",  "gCosmetics.HUD.StartButton"),
    MM_COSMETIC_TWIN(COSMETIC_ELEMENT_SMALL_KEY,         "HUD.SmallKey",   "gCosmetics.HUD.KeyCount"),
    MM_COSMETIC_TWIN(COSMETIC_ELEMENT_MINIMAP,           "HUD.Minimap",    "gCosmetics.HUD.Minimap"),
    MM_COSMETIC_OWN(COSMETIC_ELEMENT_RUPEE_ICON,         "HUD.RupeeIcon"),
    // W6-COS-B
    // Spin Attack: SoH's Effects tab. OoT draws a level's charge AND its burst from that level's Primary
    // (z_en_m_thunder.c); its Secondary is the charge's env colour, which MM's draw does not route through the lookup.
    MM_COSMETIC_TWIN(COSMETIC_ELEMENT_SPIN_SLASH_CHARGE, "Effects.SpinSlashCharge", "gCosmetics.SpinAttack.Level1Primary"),
    MM_COSMETIC_TWIN(COSMETIC_ELEMENT_SPIN_SLASH_BURST,  "Effects.SpinSlashBurst",  "gCosmetics.SpinAttack.Level1Primary"),
    MM_COSMETIC_TWIN(COSMETIC_ELEMENT_GREAT_SPIN_CHARGE, "Effects.GreatSpinCharge", "gCosmetics.SpinAttack.Level2Primary"),
    MM_COSMETIC_TWIN(COSMETIC_ELEMENT_GREAT_SPIN_BURST,  "Effects.GreatSpinBurst",  "gCosmetics.SpinAttack.Level2Primary"),
    // W6-COS-C
};
// clang-format on

#undef MM_COSMETIC_OWN
#undef MM_COSMETIC_TWIN
#undef MM_COSMETIC_TWIN_OR

constexpr int kMmCosmeticElementCount = (int)(sizeof(kMmCosmeticElements) / sizeof(kMmCosmeticElements[0]));

const MmCosmeticElement* FindMmCosmeticElement(u8 elementId) {
    for (const MmCosmeticElement& element : kMmCosmeticElements) {
        if (element.id == elementId) {
            return &element;
        }
    }
    return nullptr;
}

} // namespace

extern "C" {

/**
 * W6-COS-A (#1239): the colour table's read-only view, for the classification lock's colour-family leg
 * (src/common/tests/test_cvar_classification.c). Element @p index's id, MM's own .Color / .Changed keys, and the SoH
 * twin's .Value / .Changed keys it reads (null for an MM-only element) with the SoH fallback twin's (null for none).
 * Returns 0 when @p index is out of range.
 */
int MM_CosmeticElement_Count(void) {
    return kMmCosmeticElementCount;
}

int MM_CosmeticElement_At(int index, int* id, const char** mmColor, const char** mmChanged, const char** sohValue,
                          const char** sohChanged, const char** sohFallbackValue, const char** sohFallbackChanged) {
    if (index < 0 || index >= kMmCosmeticElementCount) {
        return 0;
    }
    const MmCosmeticElement& element = kMmCosmeticElements[index];
    *id = element.id;
    *mmColor = element.mmColor;
    *mmChanged = element.mmChanged;
    *sohValue = element.sohValue;
    *sohChanged = element.sohChanged;
    *sohFallbackValue = element.sohFallbackValue;
    *sohFallbackChanged = element.sohFallbackChanged;
    return 1;
}

/**
 * The changed colour of MM's element @p elementId, or the draw site's own colour when nothing changed it. Ported from
 * 2Ship's reader (BenGui/CosmeticEditor.cpp, CosmeticEditor_GetChangedColor): a changed colour comes back opaque, and
 * the wrappers below keep the draw site's alpha.
 */
Color_RGBA8 CosmeticEditor_GetChangedColor(u8 r, u8 g, u8 b, u8 a, u8 elementId) {
    Color_RGBA8 color = { r, g, b, a };
    const MmCosmeticElement* element = FindMmCosmeticElement(elementId);
    if (element == nullptr) {
        return color;
    }
    const char* valueKey = nullptr;
    if (element->sohValue == nullptr) {
        if (CVarGetInteger(element->mmChanged, 0)) {
            valueKey = element->mmColor;
        }
    } else if (CVarGetInteger(element->sohChanged, 0)) {
        valueKey = element->sohValue;
    } else if (element->sohFallbackValue != nullptr && CVarGetInteger(element->sohFallbackChanged, 0)) {
        valueKey = element->sohFallbackValue;
    }
    if (valueKey != nullptr) {
        const Color_RGBA8 changed = CVarGetColor(valueKey, color);
        color.r = changed.r;
        color.g = changed.g;
        color.b = changed.b;
        color.a = 255;
    }
    return color;
}

void gDPSetPrimColorOverride(Gfx* pkt, u8 m, u8 l, u8 r, u8 g, u8 b, u8 a, u8 elementId) {
    const Color_RGBA8 color = CosmeticEditor_GetChangedColor(r, g, b, a, elementId);
    gDPSetPrimColor(pkt, m, l, color.r, color.g, color.b, a);
}

void gDPSetEnvColorOverride(Gfx* pkt, u8 r, u8 g, u8 b, u8 a, u8 elementId) {
    const Color_RGBA8 color = CosmeticEditor_GetChangedColor(r, g, b, a, elementId);
    gDPSetEnvColor(pkt, color.r, color.g, color.b, a);
}

Gfx* Gfx_DrawTexRectIA8_DropShadowOverride(Gfx* pkt, TexturePtr texture, s16 textureWidth, s16 textureHeight,
                                           s16 rectLeft, s16 rectTop, s16 rectWidth, s16 rectHeight, u16 dsdx, u16 dtdy,
                                           s16 r, s16 g, s16 b, s16 a, u8 elementId) {
    const Color_RGBA8 color = CosmeticEditor_GetChangedColor(r, g, b, a, elementId);
    return Gfx_DrawTexRectIA8_DropShadow(pkt, texture, textureWidth, textureHeight, rectLeft, rectTop, rectWidth,
                                         rectHeight, dsdx, dtdy, color.r, color.g, color.b, a);
}

Gfx* Gfx_DrawRect_DropShadowOverride(Gfx* pkt, s16 rectLeft, s16 rectTop, s16 rectWidth, s16 rectHeight, u16 dsdx,
                                     u16 dtdy, s16 r, s16 g, s16 b, s16 a, u8 elementId) {
    const Color_RGBA8 color = CosmeticEditor_GetChangedColor(r, g, b, a, elementId);
    return Gfx_DrawRect_DropShadow(pkt, rectLeft, rectTop, rectWidth, rectHeight, dsdx, dtdy, color.r, color.g, color.b,
                                   a);
}

Gfx* Gfx_DrawTexRectIA16_DropShadowOverride(Gfx* pkt, TexturePtr texture, s16 textureWidth, s16 textureHeight,
                                            s16 rectLeft, s16 rectTop, s16 rectWidth, s16 rectHeight, u16 dsdx,
                                            u16 dtdy, s16 r, s16 g, s16 b, s16 a, u8 elementId) {
    const Color_RGBA8 color = CosmeticEditor_GetChangedColor(r, g, b, a, elementId);
    return Gfx_DrawTexRectIA16_DropShadow(pkt, texture, textureWidth, textureHeight, rectLeft, rectTop, rectWidth,
                                          rectHeight, dsdx, dtdy, color.r, color.g, color.b, a);
}

Gfx* Gfx_DrawTexRectIA8_DropShadowOffsetOverride(Gfx* pkt, TexturePtr texture, s16 textureWidth, s16 textureHeight,
                                                 s16 rectLeft, s16 rectTop, s16 rectWidth, s16 rectHeight, u16 dsdx,
                                                 u16 dtdy, s16 r, s16 g, s16 b, s16 a, s32 masks, s32 rects,
                                                 u8 elementId) {
    const Color_RGBA8 color = CosmeticEditor_GetChangedColor(r, g, b, a, elementId);
    return Gfx_DrawTexRectIA8_DropShadowOffset(pkt, texture, textureWidth, textureHeight, rectLeft, rectTop, rectWidth,
                                               rectHeight, dsdx, dtdy, color.r, color.g, color.b, a, masks, rects);
}

/**
 * W6-COS-B (#1239): the seams of Link's per-form model colours, for the cosmetics-patch-owner-gate row. Passes is how
 * many patch passes reached the patches, Owed whether one is owed to MM's next frame, SetBodySkipped stops each pass
 * once it is counted (the row has no archive to patch), and OnMmFrame is what MM's frame runs.
 */
int MM_CosmeticPlayerPatches_Passes(void) {
    return 0;
}

int MM_CosmeticPlayerPatches_Owed(void) {
    return 0;
}

void MM_CosmeticPlayerPatches_SetBodySkipped(int skip) {
    (void)skip;
}

void MM_CosmeticPlayerPatches_OnMmFrame(void) {
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

    // W6-COS-A (#1239): the lookup returns the changed colour by element id. A twinned element reads its SoH row in
    // the Cosmetics Editor, an MM-only element its own key. Every key set here is cleared again, pass or fail.
    {
        static const char* const kKeys[] = {
            "gCosmetics.HUD.BButton.Value",
            "gCosmetics.HUD.BButton.Changed",
            "gCosmetics.HUD.CButtons.Value",
            "gCosmetics.HUD.CButtons.Changed",
            "gCosmetics.HUD.CLeftButton.Value",
            "gCosmetics.HUD.CLeftButton.Changed",
            "gCosmetics.SpinAttack.Level1Primary.Value",
            "gCosmetics.SpinAttack.Level1Primary.Changed",
            RSBS_CVAR_MM_COSMETIC_PREFIX "Buttons.B.Color",
            RSBS_CVAR_MM_COSMETIC_PREFIX "Buttons.B.Changed",
            RSBS_CVAR_MM_COSMETIC_PREFIX "HUD.RupeeIcon.Color",
            RSBS_CVAR_MM_COSMETIC_PREFIX "HUD.RupeeIcon.Changed",
        };
        auto clearKeys = []() {
            for (const char* key : kKeys) {
                CVarClear(key);
            }
        };
        auto setColor = [](const char* stem, const char* child, u8 r, u8 g, u8 b) {
            Color_RGBA8 color = { r, g, b, 255 };
            CVarSetColor((std::string(stem) + child).c_str(), color);
            CVarSetInteger((std::string(stem) + ".Changed").c_str(), 1);
        };
        int failures = 0;
        auto expect = [&failures](const char* what, Color_RGBA8 got, u8 r, u8 g, u8 b, u8 a) {
            if (got.r != r || got.g != g || got.b != b || got.a != a) {
                printf("[TEST] FAIL: %s: got %u,%u,%u,%u, want %u,%u,%u,%u\n", what, got.r, got.g, got.b, got.a, r, g,
                       b, a);
                failures++;
            }
        };
        clearKeys();

        // 1. Nothing changed: the draw site's own colour, alpha included.
        expect("an unchanged B button", CosmeticEditor_GetChangedColor(100, 255, 120, 167, COSMETIC_ELEMENT_B_BUTTON),
               100, 255, 120, 167);

        // 2. SoH's B Button changed: MM's B button takes its colour (alpha 255, as 2Ship's reader returns), and the
        //    wrapper draws it with the draw site's alpha.
        setColor("gCosmetics.HUD.BButton", ".Value", 10, 20, 30);
        expect("SoH's B Button on MM's B button",
               CosmeticEditor_GetChangedColor(100, 255, 120, 167, COSMETIC_ELEMENT_B_BUTTON), 10, 20, 30, 255);
        {
            static Gfx want[1];
            memset(buffer, kPoison, sizeof(buffer));
            memset(want, kPoison, sizeof(want));
            gDPSetPrimColorOverride(buffer, 0, 0, 100, 255, 120, 167, COSMETIC_ELEMENT_B_BUTTON);
            gDPSetPrimColor(want, 0, 0, 10, 20, 30, 167);
            if (memcmp(buffer, want, sizeof(Gfx)) != 0) {
                printf("[TEST] FAIL: gDPSetPrimColorOverride did not draw SoH's B Button colour\n");
                failures++;
            }
        }

        // 3. SoH's Reset clears .Changed: MM keeps its own colour even with a .Value left behind.
        CVarSetInteger("gCosmetics.HUD.BButton.Changed", 0);
        expect("a B button whose SoH row was reset",
               CosmeticEditor_GetChangedColor(100, 255, 120, 167, COSMETIC_ELEMENT_B_BUTTON), 100, 255, 120, 167);

        // 4. A twinned element never reads MM's own key: nothing in the single exe writes it.
        setColor(RSBS_CVAR_MM_COSMETIC_PREFIX "Buttons.B", ".Color", 1, 2, 3);
        expect("MM's own B key on a twinned element",
               CosmeticEditor_GetChangedColor(100, 255, 120, 167, COSMETIC_ELEMENT_B_BUTTON), 100, 255, 120, 167);

        // 5. C Left reads SoH's C Buttons, and SoH's own C Left row over it (the order OoT's HUD reads them in).
        setColor("gCosmetics.HUD.CButtons", ".Value", 40, 50, 60);
        expect("SoH's C Buttons on MM's C Left",
               CosmeticEditor_GetChangedColor(255, 240, 0, 200, COSMETIC_ELEMENT_C_LEFT_BUTTON), 40, 50, 60, 255);
        setColor("gCosmetics.HUD.CLeftButton", ".Value", 70, 80, 90);
        expect("SoH's C Left Button on MM's C Left",
               CosmeticEditor_GetChangedColor(255, 240, 0, 200, COSMETIC_ELEMENT_C_LEFT_BUTTON), 70, 80, 90, 255);

        // 6. OoT draws its spin attack's charge and burst from one Primary per level: so do MM's two elements.
        setColor("gCosmetics.SpinAttack.Level1Primary", ".Value", 11, 22, 33);
        expect("SoH's Level 1 Primary on MM's Spin Slash Charge",
               CosmeticEditor_GetChangedColor(170, 255, 255, 128, COSMETIC_ELEMENT_SPIN_SLASH_CHARGE), 11, 22, 33, 255);
        expect("SoH's Level 1 Primary on MM's Spin Slash Burst",
               CosmeticEditor_GetChangedColor(170, 255, 255, 128, COSMETIC_ELEMENT_SPIN_SLASH_BURST), 11, 22, 33, 255);
        expect("an unchanged Great Spin Burst",
               CosmeticEditor_GetChangedColor(255, 255, 170, 128, COSMETIC_ELEMENT_GREAT_SPIN_BURST), 255, 255, 170,
               128);

        // 7. An MM-only element (no SoH twin) reads its own key, which its MM row in the Cosmetics Editor writes.
        setColor(RSBS_CVAR_MM_COSMETIC_PREFIX "HUD.RupeeIcon", ".Color", 5, 6, 7);
        expect("MM's own Rupee Icon key",
               CosmeticEditor_GetChangedColor(200, 255, 100, 255, COSMETIC_ELEMENT_RUPEE_ICON), 5, 6, 7, 255);
        // 8. W6-COS-B (#1239): hearts and magic. SoH's Hearts colours MM's normal hearts and leaves its beating heart,
        //    an MM row's own key; SoH's Magic colours MM's normal meter and leaves the Chateau Romani meter.
        {
            static const char* const kHeartKeys[] = {
                "gCosmetics.Consumable.Hearts.Value",
                "gCosmetics.Consumable.Hearts.Changed",
                "gCosmetics.Consumable.Magic.Value",
                "gCosmetics.Consumable.Magic.Changed",
                RSBS_CVAR_MM_COSMETIC_PREFIX "Hearts.Beating.Color",
                RSBS_CVAR_MM_COSMETIC_PREFIX "Hearts.Beating.Changed",
                RSBS_CVAR_MM_COSMETIC_PREFIX "Magic.Chateau.Color",
                RSBS_CVAR_MM_COSMETIC_PREFIX "Magic.Chateau.Changed",
            };
            setColor("gCosmetics.Consumable.Hearts", ".Value", 12, 34, 56);
            expect("SoH's Hearts on MM's normal hearts",
                   CosmeticEditor_GetChangedColor(255, 70, 50, 200, COSMETIC_ELEMENT_HEART_NORMAL), 12, 34, 56, 255);
            expect("SoH's Hearts on MM's beating heart",
                   CosmeticEditor_GetChangedColor(255, 70, 50, 200, COSMETIC_ELEMENT_HEART_NORMAL_BEATING), 255, 70, 50,
                   200);
            setColor(RSBS_CVAR_MM_COSMETIC_PREFIX "Hearts.Beating", ".Color", 21, 22, 23);
            expect("MM's own Beating Hearts key",
                   CosmeticEditor_GetChangedColor(255, 70, 50, 200, COSMETIC_ELEMENT_HEART_NORMAL_BEATING), 21, 22, 23,
                   255);
            setColor("gCosmetics.Consumable.Magic", ".Value", 31, 32, 33);
            expect("SoH's Magic on MM's magic meter",
                   CosmeticEditor_GetChangedColor(0, 200, 0, 255, COSMETIC_ELEMENT_MAGIC_NORMAL), 31, 32, 33, 255);
            expect("SoH's Magic on MM's Chateau meter",
                   CosmeticEditor_GetChangedColor(0, 0, 200, 255, COSMETIC_ELEMENT_MAGIC_CHATEAU), 0, 0, 200, 255);
            setColor(RSBS_CVAR_MM_COSMETIC_PREFIX "Magic.Chateau", ".Color", 41, 42, 43);
            expect("MM's own Chateau Magic key",
                   CosmeticEditor_GetChangedColor(0, 0, 200, 255, COSMETIC_ELEMENT_MAGIC_CHATEAU), 41, 42, 43, 255);
            for (const char* key : kHeartKeys) {
                CVarClear(key);
            }
        }
        // W6-COS-C: the arrow and MM-only trail cases go here.

        clearKeys();
        if (failures != 0) {
            return 1;
        }
    }

    printf("[TEST] cosmetic-gfx-stub: all wrappers write commands and advance the display list, and the colour "
           "lookup reads SoH's twins and MM's own keys\n");
    return 0;
}

} // extern "C"

#endif // RSBS_SINGLE_EXECUTABLE
