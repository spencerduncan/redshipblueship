/**
 * @file CosmeticsEditorMm.h
 * @brief Majora's Mask's rows in SoH's Cosmetics Editor (W6-COS-A, #1239).
 *
 * SoH's one Cosmetics Editor colours both games. An MM colour with an SoH twin has no row of its own: the twin's row
 * drives it through MM's colour lookup (games/mm/2s2h/CosmeticGfxSingleExe.cpp, a kTranslatedKeys pair). An MM colour
 * with no twin gets an MM row here, drawn in a "Majora's Mask" block that SoH's editor calls in one line after the
 * group the colour belongs with. 2Ship's own Cosmetic Editor window stays excluded: there is no second window.
 */
#pragma once

#ifdef RSBS_SINGLE_EXECUTABLE

/** HUD tab, after SoH's HUD group: MM's HUD colours with no SoH twin (the Rupee Icon). */
void CosmeticsEditor_DrawMmHudRows();
// W6-COS-B: the Link & Items tab's functions go here.
// W6-COS-C: the Effects tab's functions go here. (The Spin Attack group has no MM row: all four of MM's spin elements
// twin SoH's Level 1 / Level 2 Primary.)

#endif
