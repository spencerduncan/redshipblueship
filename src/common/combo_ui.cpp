/**
 * @file combo_ui.cpp
 * @brief The `combo_ui` registry, the SoH disabled-tooltip composer, the rect
 *        recorder, and the raw-ImGui fallback table (see combo_ui.h).
 *
 * The fallback is what a pane draws when NO table is installed: a link that
 * lacks games/oot/soh/SohGui/ComboUiSoh.cpp. The shipped binary always installs
 * SoH's table (ComboUiSoh.cpp's file-scope initializer; the ComboMMOptionsWindow
 * lock asserts it), so no player sees these widgets. They are deliberately plain
 * ImGui with no colour of their own: this TU has no SoH palette to take one from,
 * and a hand-picked colour is the thing docs/ui-style-guide.md R-C1 forbids. The
 * UI parity lint lists this file as EXCLUDED for that reason
 * (.github/scripts/ui-lint-files.txt).
 */

#include "combo_ui.h"

#include <cfloat>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

#include <imgui.h>

namespace {

const ComboUiTable* sInstalled = nullptr;
ComboUiRectRecorder sRecorder = nullptr;
void* sRecorderUser = nullptr;

bool Empty(const char* s) {
    return s == nullptr || s[0] == '\0';
}

const ComboUiWidgetOpts& OptsOrDefault(const ComboUiWidgetOpts* opts) {
    static const ComboUiWidgetOpts kDefault = { nullptr, false, nullptr };
    return opts != nullptr ? *opts : kDefault;
}

/** Hover text and rect report for the item just drawn; shared by every fallback widget. */
void FinishItem(const char* label, const ComboUiWidgetOpts& o) {
    const char* shown = ComboUi_ShownTooltip(&o);
    if (!Empty(shown) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("%s", shown);
    }
    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 b = ImGui::GetItemRectMax();
    ComboUi_NotifyRect(label, shown, a.x, a.y, b.x, b.y);
}

bool FallbackCheckbox(const char* label, bool* value, const ComboUiWidgetOpts* opts) {
    const ComboUiWidgetOpts& o = OptsOrDefault(opts);
    ImGui::BeginDisabled(o.disabled);
    const bool changed = ImGui::Checkbox(label, value);
    ImGui::EndDisabled();
    FinishItem(label, o);
    return changed;
}

bool FallbackCombobox(const char* label, int32_t* index, const char* const* values, int count,
                      const ComboUiWidgetOpts* opts) {
    const ComboUiWidgetOpts& o = OptsOrDefault(opts);
    char unknown[32];
    const char* preview = unknown;
    if (values != nullptr && *index >= 0 && *index < count) {
        preview = values[*index];
    } else {
        snprintf(unknown, sizeof(unknown), "Unknown (%d)", (int)*index);
    }
    bool changed = false;
    ImGui::PushID(label);
    ImGui::BeginGroup();
    ImGui::BeginDisabled(o.disabled);
    ImGui::TextUnformatted(label);
    if (ImGui::BeginCombo("##value", preview)) {
        for (int i = 0; values != nullptr && i < count; i++) {
            if (ImGui::Selectable(values[i], *index == i)) {
                *index = i;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    ImGui::EndGroup();
    ImGui::PopID();
    FinishItem(label, o);
    return changed;
}

bool FallbackSliderInt(const char* label, int32_t* value, int32_t min, int32_t max, const char* format,
                       const ComboUiWidgetOpts* opts) {
    const ComboUiWidgetOpts& o = OptsOrDefault(opts);
    int v = (int)*value;
    ImGui::PushID(label);
    ImGui::BeginGroup();
    ImGui::BeginDisabled(o.disabled);
    ImGui::TextUnformatted(label);
    const bool changed = ImGui::SliderInt("##value", &v, (int)min, (int)max, format != nullptr ? format : "%d");
    ImGui::EndDisabled();
    ImGui::EndGroup();
    ImGui::PopID();
    if (changed) {
        *value = (int32_t)v;
    }
    FinishItem(label, o);
    return changed;
}

bool FallbackButton(const char* label, float width, const ComboUiWidgetOpts* opts) {
    const ComboUiWidgetOpts& o = OptsOrDefault(opts);
    ImGui::BeginDisabled(o.disabled);
    const bool clicked = ImGui::Button(label, ImVec2(width < 0.0f ? -FLT_MIN : width, 0.0f));
    ImGui::EndDisabled();
    FinishItem(label, o);
    return clicked;
}

void FallbackSeparatorText(const char* text) {
    ImGui::SeparatorText(text);
}

void FallbackNoteText(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

void FallbackWarningText(const char* text) {
    ImGui::TextWrapped("%s", text);
}

void FallbackTooltip(const char* text) {
    if (!Empty(text) && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", text);
    }
}

void FallbackTagChip(const char* label, ComboUiTone, const ComboUiWidgetOpts* opts) {
    const ComboUiWidgetOpts& o = OptsOrDefault(opts);
    ImGui::SameLine();
    ImGui::BeginDisabled(o.disabled);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    const char* hidden = strstr(label, "##"); // an id suffix is not drawn, as in ImGui's own labels
    ImGui::TextUnformatted(label, hidden);
    ImGui::PopStyleColor();
    ImGui::EndDisabled();
}

/**
 * No popup machinery without SoH's: run the action at once, which is what the
 * pane's Reset did before it gained a confirm. Only a link without the SoH table
 * reaches this.
 */
void FallbackConfirm(const char*, const char*, const char*, const char*, void (*onConfirm)(void*), void* user) {
    if (onConfirm != nullptr) {
        onConfirm(user);
    }
}

void FallbackPushTheme() {
}

void FallbackPopTheme() {
}

void FallbackSpacer(float height) {
    ImGui::Dummy(ImVec2(0.0f, height));
}

void FallbackRowText(const char* text, const ComboUiWidgetOpts* opts) {
    const ComboUiWidgetOpts& o = OptsOrDefault(opts);
    ImGui::SameLine();
    ImGui::BeginDisabled(o.disabled);
    ImGui::TextWrapped("%s", text);
    ImGui::EndDisabled();
    FinishItem(text, o);
}

const ComboUiTable kFallback = {
    FallbackCheckbox,  FallbackCombobox,    FallbackSliderInt, FallbackButton,  FallbackSeparatorText,
    FallbackNoteText,  FallbackWarningText, FallbackTooltip,   FallbackTagChip, FallbackConfirm,
    FallbackPushTheme, FallbackPopTheme,    FallbackSpacer,    FallbackRowText,
};

} // namespace

extern "C" {

void ComboUi_Install(const ComboUiTable* table) {
    sInstalled = table;
}

bool ComboUi_IsInstalled(void) {
    return sInstalled != nullptr;
}

const ComboUiTable* ComboUi_Get(void) {
    return sInstalled != nullptr ? sInstalled : &kFallback;
}

const char* ComboUi_DisabledTooltip(const char* reason1, const char* reason2) {
    // Node-based storage: a stored string's c_str() survives rehashing, and one
    // copy per distinct composition bounds it by the reasons the panes can show
    // (the SohMenu::DisabledTooltip precedent).
    static std::unordered_map<std::string, std::string> sTooltips;
    std::string key;
    for (const char* reason : { reason1, reason2 }) {
        if (!Empty(reason)) {
            key += std::string("\n- ") + reason;
        }
    }
    if (key.empty()) {
        return "";
    }
    auto it = sTooltips.find(key);
    if (it == sTooltips.end()) {
        it = sTooltips.emplace(key, std::string("This setting is disabled because: \n") + key).first;
    }
    return it->second.c_str();
}

const char* ComboUi_ShownTooltip(const ComboUiWidgetOpts* opts) {
    if (opts == nullptr) {
        return nullptr;
    }
    if (opts->disabled && !Empty(opts->disabledTooltip)) {
        return opts->disabledTooltip;
    }
    return Empty(opts->tooltip) ? nullptr : opts->tooltip;
}

void ComboUi_SetRectRecorder(ComboUiRectRecorder recorder, void* user) {
    sRecorder = recorder;
    sRecorderUser = user;
}

void ComboUi_NotifyRect(const char* label, const char* tooltip, float minX, float minY, float maxX, float maxY) {
    if (sRecorder != nullptr) {
        sRecorder(sRecorderUser, label, tooltip, minX, minY, maxX, maxY);
    }
}

} // extern "C"
