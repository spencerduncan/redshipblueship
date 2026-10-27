/**
 * @file CosmeticsEditorMm.cpp
 * @brief Majora's Mask's rows in SoH's Cosmetics Editor (W6-COS-A, #1239). See CosmeticsEditorMm.h.
 *
 * Each row is SoH's own cosmetic row (DrawCosmeticRow: a colour swatch, then Random, then Reset once changed) drawn on
 * MM's own colour keys, "gCosmetic.<element>.Color" beside ".Changed", which MM's lookup reads for an element with no
 * SoH twin. games/oot never spells an MM key whole: each is composed from RSBS_CVAR_MM_COSMETIC_PREFIX. The rows have
 * no Rainbow or Locked box: SoH's rainbow tick and its lock serve SoH's own options only.
 */
#include "CosmeticsEditorMm.h"

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstddef>
#include <cstdint>
#include <string>

#include <libultraship/libultraship.h>

#include "soh/SohGui/SohGui.hpp"
#include "soh/SohGui/UIWidgets.hpp"
#include "soh/SohGui/HostedMm/SohMenuHostedMm.h" // SohGui::kHostedMmBlockLabel

#include "combo_ui.h"         // src/common: ComboUi_NotifyRect, the harness's hover seam
#include "cvar_shared_keys.h" // src/common: RSBS_CVAR_MM_COSMETIC_PREFIX

namespace {

/** One MM colour row on MM's own keys. */
struct MmCosmeticRow {
    const char* label;
    const char* colorKey;
    const char* changedKey;
    Color_RGBA8 defaultColor; ///< 2Ship's default for the element (CosmeticEditor.cpp's table)
    const char* tooltip;
};

#define MM_COSMETIC_ROW_KEYS(name) \
    RSBS_CVAR_MM_COSMETIC_PREFIX name ".Color", RSBS_CVAR_MM_COSMETIC_PREFIX name ".Changed"

// HUD tab: MM's HUD colours with no SoH twin.
const MmCosmeticRow kMmHudRows[] = {
    { "Rupee Icon",
      MM_COSMETIC_ROW_KEYS("HUD.RupeeIcon"),
      { 200, 255, 100, 255 },
      "Changes the color of the icon beside the rupee counter, for every wallet. Majora's Mask only." },
};
// W6-COS-B: the Link & Items tab's row tables go here.
// W6-COS-C: the Effects tab's row tables go here.

#undef MM_COSMETIC_ROW_KEYS

// SoH's column for a cosmetic row's buttons: DrawCosmeticRow and DrawCosmeticGroup place them after the longest
// option name. SetCursorPosX after SameLine() is SameLine(x) in two steps.
float CosmeticButtonColumn() {
    return ImGui::CalcTextSize("Message Light Blue (None No Shadow)").x + 60.0f;
}

void SetMmColor(const MmCosmeticRow& row, Color_RGBA8 color) {
    CVarSetColor(row.colorKey, color);
    CVarSetInteger(row.changedKey, 1);
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void ResetMmColor(const MmCosmeticRow& row) {
    CVarClear(row.colorKey);
    CVarClear(row.changedKey);
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

Color_RGBA8 RandomMmColor() {
    const ImVec4 random = GetRandomValue(); // UIWidgets.hpp, at file scope
    return { static_cast<uint8_t>(random.x * 255.0f), static_cast<uint8_t>(random.y * 255.0f),
             static_cast<uint8_t>(random.z * 255.0f), 255 };
}

/** SoH's DrawCosmeticRow on MM's keys. */
void DrawMmCosmeticRow(const MmCosmeticRow& row) {
    const bool changed = CVarGetInteger(row.changedKey, 0) != 0;
    const Color_RGBA8 color = changed ? CVarGetColor(row.colorKey, row.defaultColor) : row.defaultColor;
    float rgb[3] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f };

    // UIWidgets::CVarColorPicker's swatch, which cannot take MM's key: it always writes "<key>.Value", and MM's
    // colour lives in "<key>.Color". The picker's DarkGray combobox style sits over the button bracket, and the frame
    // border is put back to the style's own, which the picker never changes.
    const float frameBorder = ImGui::GetStyle().FrameBorderSize;
    UIWidgets::PushStyleButton(THEME_COLOR);
    UIWidgets::PushStyleCombobox(UIWidgets::Colors::DarkGray);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, frameBorder);
    const bool edited = ImGui::ColorEdit3(row.label, rgb, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha);
    ImGui::PopStyleVar();
    UIWidgets::PopStyleCombobox();
    UIWidgets::PopStyleButton();
    UIWidgets::Tooltip(row.tooltip);
    ComboUi_NotifyRect(row.label, row.tooltip, ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y,
                       ImGui::GetItemRectMax().x, ImGui::GetItemRectMax().y);
    if (edited) {
        SetMmColor(row, { static_cast<uint8_t>(rgb[0] * 255.0f), static_cast<uint8_t>(rgb[1] * 255.0f),
                          static_cast<uint8_t>(rgb[2] * 255.0f), 255 });
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(CosmeticButtonColumn());
    if (UIWidgets::Button(
            (std::string("Random##") + row.label).c_str(),
            UIWidgets::ButtonOptions().Size(ImVec2(80, 31)).Padding(ImVec2(2.0f, 0.0f)).Color(THEME_COLOR))) {
        SetMmColor(row, RandomMmColor());
    }
    if (changed) {
        ImGui::SameLine();
        if (UIWidgets::Button((std::string("Reset##") + row.label).c_str(),
                              UIWidgets::ButtonOptions().Size(ImVec2(80, 31)).Padding(ImVec2(2.0f, 0.0f)))) {
            ResetMmColor(row);
        }
    }
}

/** SoH's DrawCosmeticGroup on MM's rows, under the "Majora's Mask" heading the hosted MM blocks use. */
void DrawMmCosmeticGroup(const char* id, const MmCosmeticRow* rows, std::size_t count) {
    ImGui::Text("%s", SohGui::kHostedMmBlockLabel);
    ImGui::SameLine();
    ImGui::SetCursorPosX(CosmeticButtonColumn());
    if (UIWidgets::Button(
            (std::string("Random##") + id).c_str(),
            UIWidgets::ButtonOptions().Size(ImVec2(80, 31)).Padding(ImVec2(2.0f, 0.0f)).Color(THEME_COLOR))) {
        for (std::size_t i = 0; i < count; i++) {
            SetMmColor(rows[i], RandomMmColor());
        }
    }
    ImGui::SameLine();
    if (UIWidgets::Button((std::string("Reset##") + id).c_str(),
                          UIWidgets::ButtonOptions().Size(ImVec2(80, 31)).Padding(ImVec2(2.0f, 0.0f)))) {
        for (std::size_t i = 0; i < count; i++) {
            ResetMmColor(rows[i]);
        }
    }
    UIWidgets::Spacer();
    for (std::size_t i = 0; i < count; i++) {
        DrawMmCosmeticRow(rows[i]);
    }
    UIWidgets::Separator(true, true, 2.0f, 2.0f);
}

} // namespace

void CosmeticsEditor_DrawMmHudRows() {
    DrawMmCosmeticGroup("MM HUD", kMmHudRows, sizeof(kMmHudRows) / sizeof(kMmHudRows[0]));
}
// W6-COS-B: the Link & Items tab's functions go here.
// W6-COS-C: the Effects tab's functions go here.

#endif // RSBS_SINGLE_EXECUTABLE
