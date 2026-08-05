/**
 * MM HudEditor dormant stubs for the single-executable build (relocation
 * only).
 *
 * games/mm/2s2h/BenGui/HudEditor.cpp is filtered out of the single-exe link
 * (games/mm/CMakeLists.txt, "All GUI components" / "2s2h/BenGui/.*\\.cpp$"),
 * so MM's HudEditor_* entry points and its two backing globals
 * (hudEditorElements, hudEditorActiveElement) had no linked provider. They
 * previously lived as untyped redeclarations in src/common/mm_stubs.c, which
 * includes no real headers and so could not catch signature drift against
 * 2s2h/BenGui/HudEditor.h.
 *
 * That drift was real, not theoretical: the old stubs took the wrong
 * PARAMETER COUNT for HudEditor_ModifyMatrixValues (3 vs the real 2),
 * HudEditor_ModifyDrawValuesFromBase (5 vs the real 8) and
 * HudEditor_ModifyDrawValues (3 vs the real 6), the wrong TYPE for every
 * s16* rect/step/kaleido parameter (stubbed as float* / int*), and the wrong
 * RETURN type for HudEditor_ShouldOverrideDraw / HudEditor_IsActiveElementHidden
 * (stubbed int, real bool). None of that surfaced as a build break because
 * mm_stubs.c's redeclarations are exactly what every MM caller linked
 * against -- an unchecked prototype the compiler had no way to compare
 * against 2s2h/BenGui/HudEditor.h.
 *
 * Worse was the storage: `hudEditorElements` is a
 * `HudEditorElement[HUD_EDITOR_ELEMENT_MAX]` array in the real header (used
 * directly, not just through the HudEditor_* functions, by
 * games/mm/src/overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_mask.c,
 * z_kaleido_item.c and games/mm/src/code/z_parameter.c), but the old stub
 * defined it as a single `void*` set to NULL -- 8 bytes of storage standing
 * in for what every other single-exe TU compiles as a
 * HUD_EDITOR_ELEMENT_MAX-row struct array. Any direct
 * hudEditorElements[hudEditorActiveElement].field access reads past that
 * 8-byte allocation. It happens to be dead today because every such access
 * is guarded by HudEditor_ShouldOverrideDraw(), which this file (like the
 * old stub) always returns false from -- but that guard is accidental, not
 * structural, and the fix belongs at the type, not at the call site.
 *
 * Moved here, header-checked, by relocation only: no dispatch is wired and
 * no HUD editing actually happens. Every function body is the same no-op
 * behavior src/common/mm_stubs.c always ran (HudEditor_SetActiveElement
 * still does not write hudEditorActiveElement, exactly as before), just
 * compiled against the real prototypes (2s2h/BenGui/HudEditor.h) so the
 * compiler rejects any future drift instead of leaving an ABI mismatch to be
 * discovered at runtime. `hudEditorElements` / `hudEditorActiveElement` are
 * now defined at their real types and sizes -- still zero-valued / inert,
 * just no longer undersized. It lives in the MM target rather than in
 * src/common/mm_stubs.c for the same reason as
 * games/mm/2s2h/mm_save_manager_stubs.c: mm_stubs.c is built into
 * redship_common, which is NOT compiled with RSBS_SINGLE_EXECUTABLE, and
 * HudEditor.h needs MM's z64.h surface (s16/f32/bool) to declare these at
 * all.
 *
 * hudEditorActiveElement initializes to HUD_EDITOR_ELEMENT_NONE (matching
 * HudEditor.cpp's own real initializer) rather than the old stub's bare 0
 * (== HUD_EDITOR_ELEMENT_B): NONE is the sentinel every direct-array-access
 * call site above already gates on, so this is the inert value, not a
 * behavior change -- HudEditor_ShouldOverrideDraw() still always returns
 * false regardless of which element is "active".
 *
 * Guarded to single-exe: a standalone 2ship build compiles the real
 * HudEditor.cpp, which owns these symbols there.
 *
 * HudEditor_OverrideNextElementMode is declared in HudEditor.h and called
 * from games/mm/2s2h/Enhancements/Songs/BetterSongOfDoubleTime.cpp, but was
 * never stubbed in src/common/mm_stubs.c and is not added here: 2ship_enh
 * links with plain archive semantics, and nothing else in the single-exe
 * link needs a symbol BetterSongOfDoubleTime.o defines, so that object file
 * (and its unresolved reference) is never pulled into the link. Adding a
 * stub for it is new dispatch surface this relocation does not add.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "2s2h/BenGui/HudEditor.h"

HudEditorElementID hudEditorActiveElement = HUD_EDITOR_ELEMENT_NONE;
HudEditorElement hudEditorElements[HUD_EDITOR_ELEMENT_MAX];

void HudEditor_SetActiveElement(HudEditorElementID id) {
    (void)id;
}

bool HudEditor_ShouldOverrideDraw(void) {
    return false;
}

f32 HudEditor_GetActiveElementScale(void) {
    return 1.0f;
}

bool HudEditor_IsActiveElementHidden(void) {
    return false;
}

void HudEditor_ModifyRectPosValuesFromBase(s16 baseX, s16 baseY, s16* rectLeft, s16* rectTop) {
    (void)baseX;
    (void)baseY;
    (void)rectLeft;
    (void)rectTop;
}

void HudEditor_ModifyRectPosValues(s16* rectLeft, s16* rectTop) {
    (void)rectLeft;
    (void)rectTop;
}

void HudEditor_ModifyRectSizeValues(s16* rectWidth, s16* rectHeight) {
    (void)rectWidth;
    (void)rectHeight;
}

void HudEditor_ModifyTextureStepValues(s16* dsdx, s16* dtdy) {
    (void)dsdx;
    (void)dtdy;
}

void HudEditor_ModifyMatrixValues(f32* transX, f32* transY) {
    (void)transX;
    (void)transY;
}

void HudEditor_ModifyKaleidoEquipAnimValues(s16* ulx, s16* uly, s16* shrinkRate) {
    (void)ulx;
    (void)uly;
    (void)shrinkRate;
}

void HudEditor_ModifyDrawValuesFromBase(s16 baseX, s16 baseY, s16* rectLeft, s16* rectTop, s16* rectWidth,
                                         s16* rectHeight, s16* dsdx, s16* dtdy) {
    (void)baseX;
    (void)baseY;
    (void)rectLeft;
    (void)rectTop;
    (void)rectWidth;
    (void)rectHeight;
    (void)dsdx;
    (void)dtdy;
}

void HudEditor_ModifyDrawValues(s16* rectLeft, s16* rectTop, s16* rectWidth, s16* rectHeight, s16* dsdx, s16* dtdy) {
    (void)rectLeft;
    (void)rectTop;
    (void)rectWidth;
    (void)rectHeight;
    (void)dsdx;
    (void)dtdy;
}

#endif // RSBS_SINGLE_EXECUTABLE
