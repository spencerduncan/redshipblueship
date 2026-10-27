/**
 * @file CosmeticsEditorMmHud.cpp
 * @brief Majora's Mask's HUD placement rows in SoH's Cosmetics Editor (W6-HUD-A, #1239).
 *
 * 2Ship moved, scaled and hid MM's HUD elements from its own HUD Editor window (2s2h/BenGui/HudEditor.cpp), which the
 * single exe does not link. SoH keeps its own HUD placements in the Cosmetics Editor's HUD Placement tab (ADR 0004
 * puts the HUD editor under Cosmetics), so MM's placements go there too, after SoH's, in SoH's own widgets and words:
 * a "Majora's Mask" heading, a HUD Preset row (2Ship's Vanilla / Hidden / Widescreen presets), and one collapsing
 * "MM <element> position" header per element, holding SoH's position radio buttons and position sliders and MM's
 * scale slider. There is no second window: 2Ship's HUD Editor popout (gWindows.HudEditor) resolves to SoH's
 * "Popout Cosmetics Editor Window".
 *
 * SoH's wording maps onto MM's modes one for one: Original position is Vanilla, Anchor to the left / right are
 * Movable (Align Left / Right), No anchors is Movable (Align Center), Hidden is Hidden.
 *
 * The elements come from src/common/mm_hud_editor_elements.h, the same table MM's reader is built from
 * (games/mm/2s2h/mm_hudeditor_stubs.c), and every key is composed from RSBS_CVAR_MM_HUD_EDITOR_PREFIX: games/oot never
 * spells an MM key (the classification lock's leg 3d-3 holds both). There is no colour row: 2Ship's HUD editor
 * colour keys are read only by its window, never by a draw.
 */
#include "CosmeticsEditorMmHud.h"

#include "CosmeticsEditor.h"
#include "soh/SohGui/HostedMm/SohMenuHostedMm.h"
#include "soh/SohGui/SohGui.hpp"
#include "soh/SohGui/UIWidgets.hpp"

#include "combo_ui.h"               // src/common: ComboUi_NotifyRect, so the UI snapshot can hover an MM row
#include "cvar_shared_keys.h"       // src/common: RSBS_CVAR_MM_HUD_EDITOR_PREFIX
#include "mm_hud_editor_elements.h" // src/common: the element table, the modes and MM_HudEditor_ApplyPreset

#include <string>

namespace {

struct MmHudElement {
    const char* key;
    const char* label;
    int32_t defaultX;
    int32_t defaultY;
};

#define RSBS_MM_HUD_UI_ROW(id, key, label, x, y, ws) { key, label, x, y },
constexpr MmHudElement kMmHudElements[] = { RSBS_MM_HUD_EDITOR_ELEMENTS(RSBS_MM_HUD_UI_ROW) };
#undef RSBS_MM_HUD_UI_ROW

const char* kMmHudPresetNames[] = { "Vanilla (4:3)", "Hidden", "Widescreen" };

constexpr const char* kPresetLabel = "HUD Preset";
constexpr const char* kPresetTooltip =
    "Sets every Majora's Mask HUD element below to the preset and resets its position and scale: Vanilla draws each "
    "where Majora's Mask does, Hidden hides them all, and Widescreen moves each to the side of your game window it "
    "sits nearest. Majora's Mask only.";

// MmHudPlacement_OpenElementForTest's request: whether it is set, and the element it opens.
bool sOpenForTestSet = false;
std::string sOpenForTest;

std::string MmHudKey(const MmHudElement& e, const char* field) {
    return std::string(RSBS_CVAR_MM_HUD_EDITOR_PREFIX) + e.key + field;
}

void DrawMmHudModeRadio(const std::string& header, const char* label, const std::string& modeKey, int32_t mode,
                        const char* tooltip) {
    UIWidgets::CVarRadioButton(label, modeKey.c_str(), mode,
                               UIWidgets::RadioButtonsOptions().Color(THEME_COLOR).Tooltip(tooltip));
    // The radio's label is its last item, the one whose hover shows the tooltip. Reported as "<header>: <label>", so
    // the UI snapshot can find one element's radio among the identical labels of the others.
    ComboUi_NotifyRect((header + ": " + label).c_str(), tooltip, ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y,
                       ImGui::GetItemRectMax().x, ImGui::GetItemRectMax().y);
}

void DrawMmHudElement(const MmHudElement& e) {
    const std::string modeKey = MmHudKey(e, ".Mode");
    const std::string xKey = MmHudKey(e, ".Position.X");
    const std::string yKey = MmHudKey(e, ".Position.Y");
    const std::string scaleKey = MmHudKey(e, ".Scale");
    const std::string header = std::string("MM ") + e.label + " position";
    const std::string column = std::string("MM ") + e.label + " settings";

    ImGui::PushID(e.key);
    UIWidgets::PushStyleHeader(THEME_COLOR);
    if (sOpenForTestSet) {
        ImGui::SetNextItemOpen(sOpenForTest == e.key, ImGuiCond_Always);
    }
    if (ImGui::CollapsingHeader(header.c_str())) {
        if (ImGui::BeginTable("tableMmHudElement", 1, FlagsTable)) {
            ImGui::TableSetupColumn(column.c_str(), FlagsCell, TablesCellsWidth);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 2);
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 60);

            const int32_t before = CVarGetInteger(modeKey.c_str(), RSBS_MM_HUD_MODE_VANILLA);
            DrawMmHudModeRadio(header, "Original position", modeKey, RSBS_MM_HUD_MODE_VANILLA,
                               "Uses the element's original position. Majora's Mask only.");
            DrawMmHudModeRadio(header, "Anchor to the left", modeKey, RSBS_MM_HUD_MODE_MOVABLE_LEFT,
                               "Makes the element follow the left side of your game window. Majora's Mask only.");
            DrawMmHudModeRadio(header, "Anchor to the right", modeKey, RSBS_MM_HUD_MODE_MOVABLE_RIGHT,
                               "Makes the element follow the right side of your game window. Majora's Mask only.");
            DrawMmHudModeRadio(header, "No anchors", modeKey, RSBS_MM_HUD_MODE_MOVABLE_43,
                               "Keeps the element from following any side of your game window. Better used for "
                               "center elements. Majora's Mask only.");
            DrawMmHudModeRadio(header, "Hidden", modeKey, RSBS_MM_HUD_MODE_HIDDEN,
                               "Hides the element. Majora's Mask only.");
            const int32_t mode = CVarGetInteger(modeKey.c_str(), RSBS_MM_HUD_MODE_VANILLA);
            if (mode != before) {
                // A new mode starts the element from its vanilla position and size, as 2Ship's Mode combobox did.
                CVarClear(xKey.c_str());
                CVarClear(yKey.c_str());
                CVarClear(scaleKey.c_str());
            }
            if (mode >= RSBS_MM_HUD_MODE_MOVABLE_43) {
                UIWidgets::CVarSliderInt("Up <-> Down : %d", yKey.c_str(),
                                         UIWidgets::IntSliderOptions()
                                             .Min(-10)
                                             .Max(250)
                                             .DefaultValue(e.defaultY)
                                             .Size(ImVec2(300.0f, 0.0f))
                                             .Color(THEME_COLOR)
                                             .Tooltip("Moves the element up and down. Majora's Mask only."));
                UIWidgets::CVarSliderInt("Left <-> Right : %d", xKey.c_str(),
                                         UIWidgets::IntSliderOptions()
                                             .Min(-10)
                                             .Max(330)
                                             .DefaultValue(e.defaultX)
                                             .Size(ImVec2(300.0f, 0.0f))
                                             .Color(THEME_COLOR)
                                             .Tooltip("Moves the element left and right. Majora's Mask only."));
                UIWidgets::CVarSliderFloat("Scale : %.2fx", scaleKey.c_str(),
                                           UIWidgets::FloatSliderOptions()
                                               .Min(0.25f)
                                               .Max(4.0f)
                                               .DefaultValue(1.0f)
                                               .Step(0.05f)
                                               .Format("%.2f")
                                               .Size(ImVec2(300.0f, 0.0f))
                                               .Color(THEME_COLOR)
                                               .Tooltip("Scales the element. Majora's Mask only."));
            }
            ImGui::PopItemWidth();
            ImGui::EndTable();
        }
    }
    UIWidgets::PopStyleHeader();
    ImGui::PopID();
}

} // namespace

namespace SohGui {

void MmHudPlacement_OpenElementForTest(const char* key) {
    sOpenForTestSet = key != nullptr;
    sOpenForTest = key != nullptr ? key : "";
}

void DrawMmHudPlacementSection() {
    static int32_t sPreset = RSBS_MM_HUD_PRESET_VANILLA;

    UIWidgets::Separator(true, true, 2.0f, 2.0f);
    ImGui::SeparatorText(kHostedMmBlockLabel);
    if (UIWidgets::Combobox(kPresetLabel, &sPreset, kMmHudPresetNames,
                            UIWidgets::ComboboxOptions().Color(THEME_COLOR).Tooltip(kPresetTooltip))) {
        MM_HudEditor_ApplyPreset(sPreset);
        Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
    ComboUi_NotifyRect(kPresetLabel, kPresetTooltip, ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y,
                       ImGui::GetItemRectMax().x, ImGui::GetItemRectMax().y);
    for (const MmHudElement& e : kMmHudElements) {
        DrawMmHudElement(e);
    }
}

} // namespace SohGui
