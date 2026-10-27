/**
 * MM HUD editor reader for the single-executable build (W6-HUD-A, #1239).
 *
 * games/mm/2s2h/BenGui/HudEditor.cpp is filtered out of the single-exe link
 * (games/mm/CMakeLists.txt, "All GUI components" / "2s2h/BenGui/.*\\.cpp$"),
 * so MM's HudEditor_* entry points and its two backing globals
 * (hudEditorElements, hudEditorActiveElement) need a linked provider. This file
 * is that provider, compiled against the real prototypes in
 * 2s2h/BenGui/HudEditor.h so the compiler rejects any signature drift (the
 * untyped stubs it replaced in src/common/mm_stubs.c had the wrong parameter
 * counts, types and return types, and an 8-byte hudEditorElements).
 *
 * THE READER. Until #1239 every body here was a no-op that answered "vanilla",
 * so no MM HUD element could be moved, scaled or hidden. The bodies below are
 * 2Ship's reader (HudEditor.cpp:34-160) ported to C: each MM HUD draw makes its
 * element active (HudEditor_SetActiveElement), asks HudEditor_ShouldOverrideDraw,
 * and lets the HudEditor_Modify* helpers rewrite its rectangle from the
 * element's gHudEditor.<Element>.Mode / .Position.X / .Position.Y / .Scale
 * keys. 2Ship's window (HudEditorWindow) stays unlinked: the keys are written by
 * the Majora's Mask section of SoH's Cosmetics Editor, HUD Placement tab
 * (games/oot/soh/Enhancements/cosmetics/CosmeticsEditorMmHud.cpp).
 *
 * ONE TABLE. hudEditorElements is built from RSBS_MM_HUD_EDITOR_ELEMENTS
 * (src/common/mm_hud_editor_elements.h), the same table the section draws, with
 * MM's own HUD_EDITOR_ELEMENT composing the keys. An element outside it keeps an
 * empty row (null keys), and the reader treats that element as vanilla whatever
 * any override says, so no draw ever reads a key the shared menu does not write
 * (and the draw sites' own reads of hudEditorElements[active].modeCvar, which
 * all sit behind HudEditor_ShouldOverrideDraw, never see a null key). W6-HUD-B
 * (#1239) added the other ten elements as table rows, so every element MM's
 * HUD draws now has one: the clock (all three styles), hearts, magic, both
 * timers, carrots and the minigame, rupee, key and skulltula counters.
 *
 * Two deliberate differences from 2Ship: hudEditorOverrideNextElemMode is
 * static (the ODR gate; nothing outside this file reads it), and an element
 * with no row is never overridden. Everything else -- the arithmetic, its s16
 * truncation, the anchored modes' window-edge conversion -- is 2Ship's.
 *
 * Guarded to single-exe: a standalone 2ship build compiles the real
 * HudEditor.cpp, which owns these symbols there.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "2s2h/BenGui/HudEditor.h"
#include "BenPort.h"
#include "macros.h"
#include "mm_hud_editor_elements.h" // src/common: the element table and the seams declared for the shared menu

#include <libultraship/bridge/consolevariablebridge.h>

#include <stddef.h>

_Static_assert(RSBS_MM_HUD_MODE_VANILLA == HUD_EDITOR_ELEMENT_MODE_VANILLA, "mode values drifted");
_Static_assert(RSBS_MM_HUD_MODE_HIDDEN == HUD_EDITOR_ELEMENT_MODE_HIDDEN, "mode values drifted");
_Static_assert(RSBS_MM_HUD_MODE_MOVABLE_43 == HUD_EDITOR_ELEMENT_MODE_MOVABLE_43, "mode values drifted");
_Static_assert(RSBS_MM_HUD_MODE_MOVABLE_LEFT == HUD_EDITOR_ELEMENT_MODE_MOVABLE_LEFT, "mode values drifted");
_Static_assert(RSBS_MM_HUD_MODE_MOVABLE_RIGHT == HUD_EDITOR_ELEMENT_MODE_MOVABLE_RIGHT, "mode values drifted");

HudEditorElementID hudEditorActiveElement = HUD_EDITOR_ELEMENT_NONE;

/* The colour fields stay zero: 2Ship's HUD editor colour keys are read only by its window, never by a draw. */
#define RSBS_MM_HUD_ROW(id, key, label, x, y, ws) [id] = HUD_EDITOR_ELEMENT(id, label, key, x, y, 0, 0, 0, 0),
HudEditorElement hudEditorElements[HUD_EDITOR_ELEMENT_MAX] = { RSBS_MM_HUD_EDITOR_ELEMENTS(RSBS_MM_HUD_ROW) };
#undef RSBS_MM_HUD_ROW

static HudEditorElementMode sOverrideNextElemMode = HUD_EDITOR_ELEMENT_MODE_NONE;

/* The active element's row, or NULL when no element is active or the element has no row in the shared table. */
static const HudEditorElement* ActiveRow(void) {
    if (hudEditorActiveElement <= HUD_EDITOR_ELEMENT_NONE || hudEditorActiveElement >= HUD_EDITOR_ELEMENT_MAX) {
        return NULL;
    }
    const HudEditorElement* e = &hudEditorElements[hudEditorActiveElement];
    return e->modeCvar != NULL ? e : NULL;
}

/* The mode the active element draws in: the override when one is set, else its Mode key. */
static s32 ActiveMode(void) {
    const HudEditorElement* e = ActiveRow();
    if (e == NULL) {
        return HUD_EDITOR_ELEMENT_MODE_VANILLA;
    }
    if (sOverrideNextElemMode != HUD_EDITOR_ELEMENT_MODE_NONE) {
        return sOverrideNextElemMode;
    }
    return CVarGetInteger(e->modeCvar, HUD_EDITOR_ELEMENT_MODE_VANILLA);
}

/* The active element's Scale key (1 with no row). */
static f32 ActiveScale(void) {
    const HudEditorElement* e = ActiveRow();
    return e != NULL ? CVarGetFloat(e->scaleCvar, 1.0f) : 1.0f;
}

/* Allows specifying an override mode to the next active element.
 * Must be called again with HUD_EDITOR_ELEMENT_MODE_NONE when done overriding. */
void HudEditor_OverrideNextElementMode(HudEditorElementMode mode) {
    sOverrideNextElemMode = mode;
}

void HudEditor_SetActiveElement(HudEditorElementID id) {
    hudEditorActiveElement = id;
}

bool HudEditor_ShouldOverrideDraw(void) {
    return ActiveMode() != HUD_EDITOR_ELEMENT_MODE_VANILLA;
}

bool HudEditor_IsActiveElementHidden(void) {
    return ActiveMode() == HUD_EDITOR_ELEMENT_MODE_HIDDEN;
}

f32 HudEditor_GetActiveElementScale(void) {
    return sOverrideNextElemMode == HUD_EDITOR_ELEMENT_MODE_NONE ? ActiveScale() : 1.0f;
}

void HudEditor_ModifyRectPosValuesFromBase(s16 baseX, s16 baseY, s16* rectLeft, s16* rectTop) {
    const f32 scale = ActiveScale();
    s16 offsetFromBaseX = *rectLeft - baseX;
    s16 offsetFromBaseY = *rectTop - baseY;
    *rectLeft = (s16)(baseX + (offsetFromBaseX * scale));
    *rectTop = (s16)(baseY + (offsetFromBaseY * scale));
}

static void ModifyRectPosValuesFloat(f32* rectLeft, f32* rectTop) {
    const HudEditorElement* e = ActiveRow();
    if (e == NULL) {
        return;
    }
    const f32 scale = CVarGetFloat(e->scaleCvar, 1.0f);
    f32 offsetFromBaseX = *rectLeft - e->defaultX;
    f32 offsetFromBaseY = *rectTop - e->defaultY;
    *rectLeft = CVarGetInteger(e->xCvar, e->defaultX) + (offsetFromBaseX * scale);
    *rectTop = CVarGetInteger(e->yCvar, e->defaultY) + (offsetFromBaseY * scale);

    const s32 mode = CVarGetInteger(e->modeCvar, HUD_EDITOR_ELEMENT_MODE_VANILLA);
    if (mode == HUD_EDITOR_ELEMENT_MODE_MOVABLE_LEFT) {
        *rectLeft = OTRGetRectDimensionFromLeftEdge(*rectLeft);
    } else if (mode == HUD_EDITOR_ELEMENT_MODE_MOVABLE_RIGHT) {
        *rectLeft = OTRGetRectDimensionFromRightEdge(*rectLeft);
    }
}

void HudEditor_ModifyRectPosValues(s16* rectLeft, s16* rectTop) {
    f32 newLeft = *rectLeft;
    f32 newTop = *rectTop;

    ModifyRectPosValuesFloat(&newLeft, &newTop);

    *rectLeft = (s16)newLeft;
    *rectTop = (s16)newTop;
}

void HudEditor_ModifyRectSizeValues(s16* rectWidth, s16* rectHeight) {
    const f32 scale = ActiveScale();
    *rectWidth = (s16)(*rectWidth * scale);
    *rectHeight = (s16)(*rectHeight * scale);
}

void HudEditor_ModifyTextureStepValues(s16* dsdx, s16* dtdy) {
    const f32 scale = ActiveScale();
    *dsdx = (s16)(*dsdx / scale);
    *dtdy = (s16)(*dtdy / scale);
}

/* Modify matrix values based on the identity matrix (0,0) centered on the screen */
void HudEditor_ModifyMatrixValues(f32* transX, f32* transY) {
    *transX = ((f32)SCREEN_WIDTH / 2) + *transX;
    *transY = ((f32)SCREEN_HEIGHT / 2) - *transY;

    ModifyRectPosValuesFloat(transX, transY);

    *transX = *transX - ((f32)SCREEN_WIDTH / 2);
    *transY = ((f32)SCREEN_HEIGHT / 2) - *transY;
}

void HudEditor_ModifyKaleidoEquipAnimValues(s16* ulx, s16* uly, s16* shrinkRate) {
    /* Kaleido values are a multiple of 10 on the identity matrix.
     * Normalize them before passing to the modify matrix. */
    f32 transX = *ulx / 10;
    f32 transY = *uly / 10;

    HudEditor_ModifyMatrixValues(&transX, &transY);

    *ulx = (s16)(transX * 10);
    *uly = (s16)(transY * 10);

    /* 320 is the vanilla start size, and 280 is the vanilla end size (or 160 for dpad).
     * So we apply the scale to 280 and subtract to get the shrink rate. */
    s16 endAnimSize = hudEditorActiveElement == HUD_EDITOR_ELEMENT_D_PAD ? 160 : 280;
    *shrinkRate = 320 - (s16)(endAnimSize * ActiveScale());
}

void HudEditor_ModifyDrawValuesFromBase(s16 baseX, s16 baseY, s16* rectLeft, s16* rectTop, s16* rectWidth,
                                        s16* rectHeight, s16* dsdx, s16* dtdy) {
    HudEditor_ModifyRectPosValuesFromBase(baseX, baseY, rectLeft, rectTop);
    HudEditor_ModifyRectSizeValues(rectWidth, rectHeight);
    HudEditor_ModifyTextureStepValues(dsdx, dtdy);
}

void HudEditor_ModifyDrawValues(s16* rectLeft, s16* rectTop, s16* rectWidth, s16* rectHeight, s16* dsdx, s16* dtdy) {
    HudEditor_ModifyRectPosValues(rectLeft, rectTop);
    HudEditor_ModifyRectSizeValues(rectWidth, rectHeight);
    HudEditor_ModifyTextureStepValues(dsdx, dtdy);
}

/* W6-HUD-A (#1239): the seams the shared menu and the locks reach the HUD editor through
 * (src/common/mm_hud_editor_elements.h). */
#define RSBS_MM_HUD_ROW_ID(id, key, label, x, y, ws) id,
static const HudEditorElementID sMmHudTableIds[] = { RSBS_MM_HUD_EDITOR_ELEMENTS(RSBS_MM_HUD_ROW_ID) };
#undef RSBS_MM_HUD_ROW_ID
#define RSBS_MM_HUD_ROW_WIDESCREEN(id, key, label, x, y, ws) ws,
static const s32 sMmHudWidescreenModes[] = { RSBS_MM_HUD_EDITOR_ELEMENTS(RSBS_MM_HUD_ROW_WIDESCREEN) };
#undef RSBS_MM_HUD_ROW_WIDESCREEN
#define RSBS_MM_HUD_TABLE_ROWS ((int)(sizeof(sMmHudTableIds) / sizeof(sMmHudTableIds[0])))

const char* MM_HudEditor_TableKey(int row, int which) {
    if (row < 0 || row >= RSBS_MM_HUD_TABLE_ROWS) {
        return NULL;
    }
    const HudEditorElement* e = &hudEditorElements[sMmHudTableIds[row]];
    switch (which) {
        case RSBS_MM_HUD_KEY_MODE:
            return e->modeCvar;
        case RSBS_MM_HUD_KEY_POSITION_X:
            return e->xCvar;
        case RSBS_MM_HUD_KEY_POSITION_Y:
            return e->yCvar;
        case RSBS_MM_HUD_KEY_SCALE:
            return e->scaleCvar;
        default:
            return NULL;
    }
}

void MM_HudEditor_ApplyPreset(int preset) {
    for (int row = 0; row < RSBS_MM_HUD_TABLE_ROWS; row++) {
        const HudEditorElement* e = &hudEditorElements[sMmHudTableIds[row]];
        CVarClear(e->xCvar);
        CVarClear(e->yCvar);
        CVarClear(e->scaleCvar);
        CVarClear(e->modeCvar);
        if (preset == RSBS_MM_HUD_PRESET_HIDDEN) {
            CVarSetInteger(e->modeCvar, HUD_EDITOR_ELEMENT_MODE_HIDDEN);
        } else if (preset == RSBS_MM_HUD_PRESET_WIDESCREEN) {
            CVarSetInteger(e->modeCvar, sMmHudWidescreenModes[row]);
        }
    }
}

#endif // RSBS_SINGLE_EXECUTABLE
