#pragma once

namespace SohGui {

/**
 * The Majora's Mask section of SoH's Cosmetics Editor, HUD Placement tab (W6-HUD-A, #1239): MM's HUD Preset and one
 * "MM <element> position" header per element in src/common/mm_hud_editor_elements.h. The tab calls it in one line,
 * after SoH's own placements (CosmeticsEditorMmHud.cpp).
 */
void DrawMmHudPlacementSection();

/**
 * Test seam for the UI snapshot (W6-HUD-B, #1239): while @p key is non-null, the section opens the header of the
 * element whose key segment is @p key (src/common/mm_hud_editor_elements.h) and closes every other one, so a capture
 * can show one element's rows; "" closes them all, and null hands the headers back to the player.
 */
void MmHudPlacement_OpenElementForTest(const char* key);

} // namespace SohGui
