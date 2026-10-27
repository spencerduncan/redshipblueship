#pragma once

namespace SohGui {

/**
 * The Majora's Mask section of SoH's Cosmetics Editor, HUD Placement tab (W6-HUD-A, #1239): MM's HUD Preset and one
 * "MM <element> position" header per element in src/common/mm_hud_editor_elements.h. The tab calls it in one line,
 * after SoH's own placements (CosmeticsEditorMmHud.cpp).
 */
void DrawMmHudPlacementSection();

} // namespace SohGui
