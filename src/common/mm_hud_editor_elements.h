/**
 * @file mm_hud_editor_elements.h
 * @brief The Majora's Mask HUD elements the shared menu can move, scale and hide (W6-HUD-A, #1239): ONE table, read
 *        by both halves of the mechanism.
 *
 * 2Ship's HUD editor (games/mm/2s2h/BenGui/HudEditor.cpp) is excluded from the single exe with the rest of BenGui, so
 * its window and its reader never linked: MM's HUD draws asked stand-ins that always answered "vanilla". The single
 * exe now has
 *  - the READER, games/mm/2s2h/mm_hudeditor_stubs.c: HudEditor.cpp's reader ported into the linked stand-in, with
 *    hudEditorElements built from this table, so an element outside it keeps drawing vanilla and no reader ever reads a
 *    key the shared menu does not write;
 *  - the ROWS, games/oot/soh/Enhancements/cosmetics/CosmeticsEditorMmHud.cpp: a Majora's Mask section in SoH's
 *    Cosmetics Editor, HUD Placement tab, after SoH's own placements, drawn from this table.
 *
 * One row per element: X(id, key, label, defaultX, defaultY, widescreenMode)
 *  - id: MM's HudEditorElementID enumerator (2s2h/BenGui/HudEditor.h). A bare token: only MM's TU and MM's test give
 *    it a value; everyone else stringifies it or ignores it.
 *  - key: the element's segment of its four keys. RSBS_CVAR_MM_HUD_EDITOR_PREFIX key ".Mode", ".Position.X",
 *    ".Position.Y" and ".Scale", exactly as HudEditor.h's HUD_EDITOR_ELEMENT macro composes them for MM's reader
 *    (the classification lock's HUD-editor leg compares the two compositions key by key).
 *  - label: the element in SoH's own wording (the "<label> position" headers of Draw_Placements).
 *  - defaultX, defaultY: the element's vanilla 4:3 position, 2Ship's values (HudEditor.cpp:10-31). The reader moves an
 *    element by its offset from these, and the position sliders start from them.
 *  - widescreenMode: the mode the Widescreen preset gives the element (HudEditor.cpp:189-232).
 * The order is the section's draw order, SoH's own button order.
 *
 * W6-HUD-B adds the other ten elements as rows at the marker that ends the list, and nothing else: the reader, the
 * presets, the section and both locks follow the table. The HUD editor's colour keys (gColors.<E>.Color / .Changed)
 * are deliberately not here: in 2Ship only the editor's window reads them, never a draw, so a colour row would write
 * a key nothing in Majora's Mask reads.
 *
 * C-compatible on purpose: MM's reader is a C TU.
 */
#ifndef RSBS_MM_HUD_EDITOR_ELEMENTS_H
#define RSBS_MM_HUD_EDITOR_ELEMENTS_H

/* MM's HudEditorElementMode values, which the rows write and the presets set. mm_hudeditor_stubs.c asserts each equals
 * MM's own enumerator at compile time. */
#define RSBS_MM_HUD_MODE_VANILLA 0
#define RSBS_MM_HUD_MODE_HIDDEN 1
#define RSBS_MM_HUD_MODE_MOVABLE_43 2
#define RSBS_MM_HUD_MODE_MOVABLE_LEFT 3
#define RSBS_MM_HUD_MODE_MOVABLE_RIGHT 4

/* clang-format off */
#define RSBS_MM_HUD_EDITOR_ELEMENTS(X)                                                                              \
    X(HUD_EDITOR_ELEMENT_B, "B", "B Button", 167, 17, RSBS_MM_HUD_MODE_MOVABLE_RIGHT)                               \
    X(HUD_EDITOR_ELEMENT_A, "A", "A Button", 191, 18, RSBS_MM_HUD_MODE_MOVABLE_RIGHT)                               \
    X(HUD_EDITOR_ELEMENT_START, "Start", "Start Button", 136, 17, RSBS_MM_HUD_MODE_MOVABLE_RIGHT)                   \
    X(HUD_EDITOR_ELEMENT_C_UP, "CUp", "C Button Up", 254, 16, RSBS_MM_HUD_MODE_MOVABLE_RIGHT)                       \
    X(HUD_EDITOR_ELEMENT_C_DOWN, "CDown", "C Button Down", 249, 34, RSBS_MM_HUD_MODE_MOVABLE_RIGHT)                 \
    X(HUD_EDITOR_ELEMENT_C_LEFT, "CLeft", "C Button Left", 227, 18, RSBS_MM_HUD_MODE_MOVABLE_RIGHT)                 \
    X(HUD_EDITOR_ELEMENT_C_RIGHT, "CRight", "C Button Right", 271, 18, RSBS_MM_HUD_MODE_MOVABLE_RIGHT)              \
    X(HUD_EDITOR_ELEMENT_D_PAD, "DPad", "DPad items", 271, 55, RSBS_MM_HUD_MODE_MOVABLE_RIGHT)                      \
    X(HUD_EDITOR_ELEMENT_MINIMAP, "Minimap", "Minimaps", 295, 220, RSBS_MM_HUD_MODE_MOVABLE_RIGHT)                  \
    /* W6-HUD-B: the remaining elements go on the lines above this marker, one row each */
/* clang-format on */

/* The presets of the section's HUD Preset row, in its order (2Ship's presetNames, HudEditor.cpp:166-170). */
#define RSBS_MM_HUD_PRESET_VANILLA 0
#define RSBS_MM_HUD_PRESET_HIDDEN 1
#define RSBS_MM_HUD_PRESET_WIDESCREEN 2

/* Which of an element's four keys MM_HudEditor_TableKey returns. */
#define RSBS_MM_HUD_KEY_MODE 0
#define RSBS_MM_HUD_KEY_POSITION_X 1
#define RSBS_MM_HUD_KEY_POSITION_Y 2
#define RSBS_MM_HUD_KEY_SCALE 3

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The key MM's reader reads for row @p row of RSBS_MM_HUD_EDITOR_ELEMENTS (its field @p which, an RSBS_MM_HUD_KEY_*),
 * straight out of hudEditorElements; null when the reader has no key for it (or @p row / @p which is out of range).
 */
const char* MM_HudEditor_TableKey(int row, int which);

/**
 * Apply the section's HUD Preset @p preset (an RSBS_MM_HUD_PRESET_*) to every element in the table: each element's
 * position and scale are cleared and its mode is cleared (Vanilla), set to Hidden (Hidden) or set to its widescreen
 * mode (Widescreen), as 2Ship's preset combobox did (HudEditor.cpp:179-238). An unknown preset only clears.
 */
void MM_HudEditor_ApplyPreset(int preset);

#ifdef __cplusplus
}
#endif

#endif /* RSBS_MM_HUD_EDITOR_ELEMENTS_H */
