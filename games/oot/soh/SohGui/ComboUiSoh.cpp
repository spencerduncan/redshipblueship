/**
 * @file ComboUiSoh.cpp
 * @brief SoH's implementation of the `combo_ui` seam (src/common/combo_ui.h):
 *        our common-owned panes drawn with SoH's own widgets (UI parity M6).
 *
 * Every entry is the helper SoH's menu uses for the same row type, with the
 * options Menu::MenuDrawItem gives it: the player's theme colour on every
 * interactive widget (Menu.cpp forces `options->color = menuThemeIndex` on each
 * one), SoH's label placement (checkbox label on the right, combobox and slider
 * labels above), UIWidgets' tooltip wrapping, SoH's disabled-tooltip rule, and
 * SohGui::RegisterPopup for confirms. Notes and warnings are the WIDGET_TEXT
 * row's own drawing (AlignTextToFramePadding + TextWrapped in a palette colour).
 *
 * INSTALLED FROM A FILE-SCOPE INITIALIZER, the RegisterComboSectionPage_t
 * precedent (SohMenuComboMmEnhancements.cpp): this TU is under games/oot/soh,
 * and soh_port is WHOLE_ARCHIVE'd since #640, so the initializer cannot be elided
 * the way a plain-archive registrar would be. It only stores a pointer; nothing
 * here runs until a pane calls an entry from inside an ImGui frame.
 *
 * Nothing in this file is RedShipBlueShip-specific styling: a pane that wants a
 * look SoH's helpers do not give it has to argue for it in the style guide, not
 * add it here.
 */

#include "combo_ui.h"

#include "soh/SohGui/SohGui.hpp"
#include "soh/SohGui/UIWidgets.hpp"

#include <ship/window/gui/IconsFontAwesome4.h>

#include <cstdio>
#include <map>
#include <string>

namespace {

using UIWidgets::Colors;

/** The player's menu theme colour (THEME_COLOR), or SoH's default before the menu exists. */
Colors Theme() {
    return SohGui::GetSohMenu() != nullptr ? THEME_COLOR : Colors::LightBlue;
}

Colors ToneColor(ComboUiTone tone) {
    switch (tone) {
        case COMBO_UI_TONE_GRAY:
            return Colors::Gray;
        case COMBO_UI_TONE_ORANGE:
            return Colors::Orange;
        case COMBO_UI_TONE_GREEN:
            return Colors::Green;
        case COMBO_UI_TONE_BLUE:
            return Colors::Blue;
        case COMBO_UI_TONE_LIGHT_BLUE:
            return Colors::LightBlue;
        case COMBO_UI_TONE_RED:
            return Colors::Red;
        case COMBO_UI_TONE_PURPLE:
            return Colors::Purple;
        case COMBO_UI_TONE_WHITE:
            return Colors::White;
        case COMBO_UI_TONE_THEME:
        default:
            return Theme();
    }
}

const char* OrEmpty(const char* s) {
    return s != nullptr ? s : "";
}

/** Copies the seam's options onto a UIWidgets option struct. */
template <typename T> T& Apply(T& options, const ComboUiWidgetOpts* opts) {
    if (opts != nullptr) {
        options.tooltip = OrEmpty(opts->tooltip);
        options.disabled = opts->disabled;
        options.disabledTooltip = OrEmpty(opts->disabledTooltip);
    }
    options.color = Theme();
    return options;
}

/** Reports the item just drawn to the seam's rect recorder (the UI snapshot harness's hover finder). */
void Report(const char* label, const ComboUiWidgetOpts* opts) {
    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 b = ImGui::GetItemRectMax();
    ComboUi_NotifyRect(label, ComboUi_ShownTooltip(opts), a.x, a.y, b.x, b.y);
}

bool SohCheckbox(const char* label, bool* value, const ComboUiWidgetOpts* opts) {
    UIWidgets::CheckboxOptions options;
    const bool changed = UIWidgets::Checkbox(label, value, Apply(options, opts));
    Report(label, opts);
    return changed;
}

bool SohCombobox(const char* label, int32_t* index, const char* const* values, int count,
                 const ComboUiWidgetOpts* opts) {
    // The map overload, not the vector one: UIWidgets previews with .at(*value),
    // and an index the table does not hold (a value a later build wrote) must be
    // SHOWN as unknown rather than thrown on or clamped to a value it is not.
    std::map<int32_t, const char*> map;
    for (int i = 0; values != nullptr && i < count; i++) {
        map[(int32_t)i] = values[i];
    }
    char unknown[32];
    if (!map.contains(*index)) {
        snprintf(unknown, sizeof(unknown), "Unknown (%d)", (int)*index);
        map[*index] = unknown;
    }
    UIWidgets::ComboboxOptions options;
    const bool changed = UIWidgets::Combobox<int32_t>(label, index, map, Apply(options, opts));
    Report(label, opts);
    return changed;
}

bool SohSliderInt(const char* label, int32_t* value, int32_t min, int32_t max, const char* format,
                  const ComboUiWidgetOpts* opts) {
    // UIWidgets::SliderInt prints an Above label through ImGui::Text(label,
    // value), SoH's "Name: %d" slider shape. The seam's label is plain text, so
    // a '%' in it is doubled rather than read as a conversion.
    std::string text;
    for (const char* c = label; *c != '\0'; c++) {
        text += *c;
        if (*c == '%') {
            text += '%';
        }
    }
    UIWidgets::IntSliderOptions options;
    Apply(options, opts);
    options.Min(min).Max(max).Format(format != nullptr ? format : "%d").DefaultValue(min);
    const bool changed = UIWidgets::SliderInt(text.c_str(), value, options);
    Report(label, opts);
    return changed;
}

bool SohButton(const char* label, float width, const ComboUiWidgetOpts* opts) {
    UIWidgets::ButtonOptions options;
    Apply(options, opts);
    options.Size(width < 0.0f ? UIWidgets::Sizes::Fill : ImVec2(width, 0.0f));
    const bool clicked = UIWidgets::Button(label, options);
    Report(label, opts);
    return clicked;
}

/**
 * The Check Tracker's search box, statement for statement
 * (randomizer_check_tracker.cpp, CheckTrackerWindow::DrawElement): the field in
 * PushStyleCombobox, 42 px short of the line for the eraser button, and the
 * "Search..." placeholder drawn over the empty field at 40% white, 12 px in (SoH
 * draws it at window x 20 over a field that starts at the 8 px padding).
 */
bool SohSearchInput(const char* id, char* buf, int bufSize) {
    ImGui::PushID(id);
    UIWidgets::PushStyleCombobox(Theme());
    const float startX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 42);
    bool changed = ImGui::InputText("##search", buf, (size_t)bufSize);
    // Both parts report their rects like every seam widget (no tooltip: SoH's
    // search box has none). The eraser goes under "<id>##eraser".
    Report(id, nullptr);
    ImGui::SameLine();
    const bool erase = UIWidgets::Button(
        ICON_FA_ERASER,
        UIWidgets::ButtonOptions().Size(UIWidgets::Sizes::Inline).Color(Theme()).Padding(ImVec2(10.f, 6.f)));
    const std::string eraserLabel = std::string(id) + "##eraser";
    Report(eraserLabel.c_str(), nullptr);
    if (erase) {
        changed = changed || buf[0] != '\0';
        buf[0] = '\0';
    }
    if (buf[0] == '\0') {
        ImGui::SameLine(startX + 12.0f);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.4f), "Search...");
    }
    UIWidgets::PopStyleCombobox();
    ImGui::PopID();
    return changed;
}

void SohSeparatorText(const char* text) {
    // WIDGET_SEPARATOR_TEXT with no colour (Menu.cpp).
    ImGui::SeparatorText(text);
}

/** WIDGET_TEXT in a palette colour (Menu.cpp): the gray note and the orange warning. */
void SohColoredText(const char* text, Colors color) {
    ImGui::PushStyleColor(ImGuiCol_Text, UIWidgets::ColorValues.at(color));
    ImGui::AlignTextToFramePadding();
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

void SohNoteText(const char* text) {
    SohColoredText(text, Colors::Gray);
}

void SohWarningText(const char* text) {
    SohColoredText(text, Colors::Orange);
}

void SohTooltip(const char* text) {
    if (text != nullptr && text[0] != '\0') {
        UIWidgets::Tooltip(text);
    }
}

void SohTagChip(const char* label, ComboUiTone tone, const ComboUiWidgetOpts* opts) {
    // Rando::Tricks::DrawTagChips (tricks.cpp), one chip. SoH draws every chip
    // disabled, and ImGui's BeginDisabled only dims when nothing outside it
    // already has, so on a disabled row (whose checkbox and name are dimmed) the
    // chip is dimmed by the same DisabledAlpha once more, by hand: without it a
    // disabled row's chips would read exactly as bright as a live row's.
    const bool rowDisabled = opts != nullptr && opts->disabled;
    if (rowDisabled) {
        const ImGuiStyle& style = ImGui::GetStyle();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.Alpha * style.DisabledAlpha);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled();
    UIWidgets::PushStyleButton(ToneColor(tone));
    ImGui::SmallButton(label);
    UIWidgets::PopStyleButton();
    ImGui::EndDisabled();
    if (rowDisabled) {
        ImGui::PopStyleVar();
    }
}

void SohConfirm(const char* title, const char* message, const char* confirmLabel, const char* cancelLabel,
                void (*onConfirm)(void*), void* user) {
    SohGui::RegisterPopup(
        OrEmpty(title), OrEmpty(message), OrEmpty(confirmLabel), OrEmpty(cancelLabel),
        [onConfirm, user]() {
            if (onConfirm != nullptr) {
                onConfirm(user);
            }
        },
        nullptr);
}

// SoH's tracker panes theme a collapsing header with the combobox style: the
// Check Tracker Settings pane's section headers (randomizer_check_tracker.cpp,
// ImGuiDrawTwoColorPickerSection) draw rounded, 10x6-padded headers in the theme
// colour at half alpha. PushStyleHeader (CosmeticsEditor.cpp's flat, opaque
// header) is the editor-pane variant; our panes are trackers, and the harness
// captures the Check Tracker Settings pane beside them.
void SohPushTheme() {
    UIWidgets::PushStyleCombobox(Theme());
}

void SohPopTheme() {
    UIWidgets::PopStyleCombobox();
}

void SohSpacer(float height) {
    UIWidgets::Spacer(height);
}

void SohRowText(const char* text, const ComboUiWidgetOpts* opts) {
    // DrawTricksMenu's name cell (SohMenuRandomizer.cpp): ImGui::Text after the
    // chips, then UIWidgets::Tooltip. Wrapped rather than clipped, because a pane
    // is narrower than SoH's trick column and the longest trick names run past
    // it; ImGui starts every wrapped line at the name's own x, so the chips keep
    // their column. The tooltip follows UIWidgets' disabled rule (a disabled row
    // shows its disabled tooltip) and is hovered even while disabled, as a
    // UIWidgets widget's is.
    const bool disabled = opts != nullptr && opts->disabled;
    ImGui::SameLine();
    ImGui::BeginDisabled(disabled);
    ImGui::TextWrapped("%s", text);
    ImGui::EndDisabled();
    const char* shown = ComboUi_ShownTooltip(opts);
    if (shown != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("%s", UIWidgets::WrappedText(shown).c_str());
    }
    Report(text, opts);
}

const ComboUiTable kSohTable = {
    SohCheckbox, SohCombobox, SohSliderInt, SohButton,    SohSearchInput, SohSeparatorText, SohNoteText, SohWarningText,
    SohTooltip,  SohTagChip,  SohConfirm,   SohPushTheme, SohPopTheme,    SohSpacer,        SohRowText,
};

struct InstallSohComboUi {
    InstallSohComboUi() {
        ComboUi_Install(&kSohTable);
    }
};

InstallSohComboUi sInstallSohComboUi;

} // namespace
