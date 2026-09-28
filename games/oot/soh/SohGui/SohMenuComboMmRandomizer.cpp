/**
 * @file SohMenuComboMmRandomizer.cpp
 * @brief Majora's Mask's randomizer options and tricks as two CONTRIBUTED pages
 *        of the tier-4 Combo section: Combo > MM Randomizer and Combo > MM
 *        Tricks (ADR 0004's 2026-09-27 host amendment; #497, #578, #682).
 *
 * WHY PAGES AND NOT A WINDOW. SoH splits its surfaces by WHEN they are used.
 * Settings chosen before or between sessions are menu pages (Randomizer >
 * General and Logic/Access, whose option groups option.cpp registers as rows,
 * and Randomizer > Tricks/Glitches, SohMenuRandomizer.cpp's DrawTricksMenu).
 * Tools used during play are floating windows toggled from a page (Item
 * Tracker, Check Tracker, Entrance Tracker, the Cosmetics and Audio editors).
 * MM's randomizer options and tricks freeze into the paired world at the
 * creation event: they are settings, so they are pages. Until now they were a
 * common-owned pop-out window (ComboMmOptionsWindow.cpp, retired with this
 * file), kept a window in 2026-07 only because src/common could not draw with
 * SoH's widgets. This TU is under games/oot/soh/SohGui, so it can.
 *
 * TIMING IS UNCHANGED. The options must be reachable before the combo file is
 * created (ADR 0004 section 4.1a, as amended 2026-07-30). The Combo section is
 * part of OoT's live menu, which is up at OoT's file select, before creation;
 * Combo > Cross-Game Rules already authors the tier-4 rules that freeze at the
 * same event from the same place.
 *
 * WHERE THE DECISIONS LIVE. src/common/combo_mm_options_page.h: the column of
 * each option group, each row's state and reason, the notes and warnings, the
 * trick headline, and Reset's action. This file turns them into SohMenu rows.
 * It reads the MM descriptor tables through src/common's accessors and writes
 * only through their gated writers (Combo_MMOptionSetValue, Combo_MMTrickSetValue,
 * the Clear pair), which refuse while the profile is frozen, so a greyed row is
 * honest presentation and never the gate (ADR 0004 section 6). No MM header and
 * no gSaveContext (ADR 0002, ADR 0008 rule 5).
 *
 * WHY POINTER WIDGETS OVER STAGING BUFFERS, not WIDGET_CVAR_*: a WIDGET_CVAR_*
 * row is its own writer (UIWidgets::CVarCheckbox calls CVarSetInteger itself),
 * which would put a second, ungated writer beside the freeze gate. So each row
 * is a pointer widget over a staging value its PreFunc refreshes from the model
 * every frame, and its Callback offers the edit to the gated writer. A refused
 * write simply loses: the next PreFunc restores the model's value. This is
 * Combo > Cross-Game Rules' pattern (SohMenuCombo.cpp), for the same reason.
 *
 * REGISTERED where the Majora's Mask page registers
 * (SohMenuComboMmEnhancements.cpp), so the three pages sit together in the
 * sidebar in a fixed order: Majora's Mask, MM Randomizer, MM Tricks.
 *
 * Locked by MenuMmRandomizerPages (games/oot/soh/soh_menu_mm_randomizer_pages_test.cpp)
 * and ComboMMOptionsPage (src/common/tests/test_combo_mm_options_page.c); drawn and
 * compared by UiSnapshot (Randomizer > General and Randomizer > Tricks/Glitches).
 */

#include "SohMenu.h"
#include "soh/OTRGlobals.h"
#include "soh/SohGui/SohGui.hpp"

// src/common only: the page model, the two descriptor views, and the combo_ui
// seam (its SoH tag chip, and the rect recorder the trick names report to).
#include "combo_mm_options_page.h"
#include "combo_ui.h"

#include <imgui_internal.h> // ImGui::TreeNodeSetOpen, as DrawTricksMenu uses it

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace SohGui {

using namespace UIWidgets;

namespace {

/** Rows the staging buffers hold. The table has 47; a larger one is refused by
 *  name at registration rather than written past the end. */
constexpr int kMaxOptionRows = 128;

int32_t sOptionValue[kMaxOptionRows];
bool sOptionOn[kMaxOptionRows];
// A combo row's value the table has no label for (a later build's), taught to
// its map as "Unknown (N)" so UIWidgets::Combobox's map.at() cannot throw.
int32_t sOptionUnknown[kMaxOptionRows];
bool sOptionUnknownTaught[kMaxOptionRows];
char sUnknownLabel[kMaxOptionRows][24];
// The Final Hours row's in-slider text ("05:59"), rewritten by its PreFunc.
char sTimeFormat[kMaxOptionRows][8];

/** The note rows' names, rewritten each frame by their PreFuncs. */
std::string sStatusNote;
std::string sTricksNote;

SohMenuPresentation ToPresentation(ComboMMRowState state) {
    switch (state) {
        case COMBO_MM_ROW_BLOCKED:
            return SOH_MENU_PRESENT_CAPABILITY;
        case COMBO_MM_ROW_FROZEN:
            return SOH_MENU_PRESENT_FROZEN;
        case COMBO_MM_ROW_LIVE:
        default:
            return SOH_MENU_PRESENT_LIVE;
    }
}

/** ADR 0004 section 6's presentation for option row @p index, from the model. */
void ApplyOptionState(WidgetInfo& info, int index) {
    const char* reason = "";
    const ComboMMRowState state = Combo_MMOptionsPage_OptionState(Combo_MMOptionAt(index), &reason);
    SohMenu::ApplyPresentation(info, info.name, ToPresentation(state), state == COMBO_MM_ROW_LIVE ? nullptr : reason);
}

/** A plain label as an ImGui::Text format: UIWidgets::SliderInt prints an Above
 *  label through ImGui::Text(label, value), so a '%' in it is doubled. */
std::string EscapePercent(const char* label) {
    std::string out;
    for (const char* c = label; c != nullptr && *c != '\0'; c++) {
        out += *c;
        if (*c == '%') {
            out += '%';
        }
    }
    return out;
}

void AddOptionRow(SohMenu& menu, WidgetPath& path, int index) {
    const ComboMMOptionDesc* desc = Combo_MMOptionAt(index);
    if (desc == nullptr) {
        return;
    }
    const std::string label = desc->label != nullptr ? desc->label : "";
    const char* tooltip = desc->tooltip != nullptr ? desc->tooltip : "";

    switch ((ComboMMOptionWidget)desc->widget) {
        case COMBO_MM_WIDGET_CHECKBOX:
            menu.AddWidget(path, label + COMBO_MM_OPTIONS_ROW_ID_SUFFIX, WIDGET_CHECKBOX)
                .ValuePointer(&sOptionOn[index])
                .PreFunc([index](WidgetInfo& info) {
                    sOptionOn[index] = Combo_MMOptionGetValue(Combo_MMOptionAt(index)) != 0;
                    ApplyOptionState(info, index);
                })
                .Callback([index](WidgetInfo& info) {
                    Combo_MMOptionSetValue(Combo_MMOptionAt(index), sOptionOn[index] ? 1 : 0);
                })
                .Options(CheckboxOptions().Tooltip(tooltip));
            break;
        case COMBO_MM_WIDGET_COMBO: {
            std::map<int32_t, const char*> values;
            for (int v = 0; desc->valueLabels != nullptr && v < (int)desc->valueCount; v++) {
                values[(int32_t)v] = desc->valueLabels[v];
            }
            menu.AddWidget(path, label + COMBO_MM_OPTIONS_ROW_ID_SUFFIX, WIDGET_COMBOBOX)
                .ValuePointer(&sOptionValue[index])
                .PreFunc([index](WidgetInfo& info) {
                    sOptionValue[index] = Combo_MMOptionGetValue(Combo_MMOptionAt(index));
                    // std::map::at throws on a key it does not hold, and
                    // MenuDrawItem catches only bad_variant_access, so a value
                    // the table has no label for is taught as "Unknown (N)" and
                    // withdrawn again once the value is a known one. Shown as
                    // what it is, never clamped to a label it is not.
                    auto options = std::static_pointer_cast<ComboboxOptions>(info.options);
                    if (sOptionUnknownTaught[index] && sOptionUnknown[index] != sOptionValue[index]) {
                        options->comboMap.erase(sOptionUnknown[index]);
                        sOptionUnknownTaught[index] = false;
                    }
                    if (!options->comboMap.contains(sOptionValue[index])) {
                        snprintf(sUnknownLabel[index], sizeof(sUnknownLabel[index]), "Unknown (%d)",
                                 (int)sOptionValue[index]);
                        options->comboMap[sOptionValue[index]] = sUnknownLabel[index];
                        sOptionUnknown[index] = sOptionValue[index];
                        sOptionUnknownTaught[index] = true;
                    }
                    ApplyOptionState(info, index);
                })
                .Callback(
                    [index](WidgetInfo& info) { Combo_MMOptionSetValue(Combo_MMOptionAt(index), sOptionValue[index]); })
                .Options(ComboboxOptions().ComboMap(values).Tooltip(tooltip));
            break;
        }
        case COMBO_MM_WIDGET_SLIDER:
            // SoH's "Name: %d" slider shape (docs/ui-style-guide.md R-N3).
            menu.AddWidget(path, EscapePercent(label.c_str()) + ": %d", WIDGET_SLIDER_INT)
                .ValuePointer(&sOptionValue[index])
                .PreFunc([index](WidgetInfo& info) {
                    sOptionValue[index] = Combo_MMOptionGetValue(Combo_MMOptionAt(index));
                    ApplyOptionState(info, index);
                })
                .Callback(
                    [index](WidgetInfo& info) { Combo_MMOptionSetValue(Combo_MMOptionAt(index), sOptionValue[index]); })
                .Options(IntSliderOptions()
                             .Min(desc->minValue)
                             .Max(desc->maxValue)
                             .DefaultValue(desc->defaultValue)
                             .Tooltip(tooltip));
            break;
        case COMBO_MM_WIDGET_TIME:
            // Minutes since midnight, shown as the clock time a player reads. The
            // clock is the slider's whole in-slider text, rewritten each frame.
            menu.AddWidget(path, EscapePercent(label.c_str()), WIDGET_SLIDER_INT)
                .ValuePointer(&sOptionValue[index])
                .PreFunc([index](WidgetInfo& info) {
                    const int32_t value = Combo_MMOptionGetValue(Combo_MMOptionAt(index));
                    sOptionValue[index] = value;
                    snprintf(sTimeFormat[index], sizeof(sTimeFormat[index]), "%02d:%02d", (int)value / 60,
                             (int)value % 60);
                    std::static_pointer_cast<IntSliderOptions>(info.options)->Format(sTimeFormat[index]);
                    ApplyOptionState(info, index);
                })
                .Callback(
                    [index](WidgetInfo& info) { Combo_MMOptionSetValue(Combo_MMOptionAt(index), sOptionValue[index]); })
                .Options(IntSliderOptions()
                             .Min(desc->minValue)
                             .Max(desc->maxValue)
                             .DefaultValue(desc->defaultValue)
                             .Tooltip(tooltip));
            break;
        default:
            // A widget kind this build cannot draw: say so rather than drop the
            // row, which would read as "MM has no such option".
            menu.AddWidget(path, label + " cannot be shown in this build.", WIDGET_TEXT)
                .RaceDisable(false)
                .HideInSearch(true)
                .Options(TextOptions().Color(Colors::Gray));
            break;
    }
}

/** A gray note row whose sentence its PreFunc supplies. */
WidgetInfo& AddNote(SohMenu& menu, WidgetPath& path, const char* name, WidgetFunc preFunc) {
    return menu.AddWidget(path, name, WIDGET_TEXT)
        .RaceDisable(false)
        .HideInSearch(true)
        .PreFunc(std::move(preFunc))
        .Options(TextOptions().Color(Colors::Gray));
}

// ---- the trick list ------------------------------------------------------------

/** One pending move, applied after the table is drawn so the two columns are not
 *  changed while they are being walked (the shape of OoT's own lists). */
struct TrickMove {
    int index = -1;
    bool on = false;
};

bool InColumn(const ComboMMTrickDesc* desc, bool enabledColumn) {
    return Combo_MMTrickGetValue(desc) == enabledColumn;
}

/** A combo_ui tone as SoH's palette entry (the chip colours ComboUiSoh.cpp draws). */
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
            return THEME_COLOR;
    }
}

/**
 * SoH's tag order on its Tricks page (tricks.cpp's Tag enum: Novice,
 * Intermediate, Advanced, Expert, Extreme, Experimental, Glitch), by the
 * palette tone each rung shares with SoH; "OoT Items" (gray) is ours and last.
 */
int ToneRank(ComboUiTone tone) {
    switch (tone) {
        case COMBO_UI_TONE_GREEN:
            return 0;
        case COMBO_UI_TONE_ORANGE:
            return 1;
        case COMBO_UI_TONE_BLUE:
            return 2;
        case COMBO_UI_TONE_RED:
            return 3;
        case COMBO_UI_TONE_PURPLE:
            return 4;
        case COMBO_UI_TONE_LIGHT_BLUE:
            return 5;
        case COMBO_UI_TONE_WHITE:
            return 6;
        default:
            return 7;
    }
}

/** One tag of the filter bar: a chip label the table uses, its tone, and
 *  whether rows carrying it are shown. */
struct TagFilter {
    std::string label;
    ComboUiTone tone;
    bool shown;
};

/**
 * The filter bar's tags, built once from the chips the table carries (the tag
 * enum is MM's, so this TU learns the tags from the descriptors, never the
 * enum), in SoH's order. Every tag starts shown. SoH starts Glitch hidden; here
 * nothing is hidden until the player chooses, so the page opens on every trick
 * the pop-out window used to list.
 */
std::vector<TagFilter>& TagFilters() {
    static std::vector<TagFilter> filters;
    if (!filters.empty()) {
        return filters;
    }
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* desc = Combo_MMTrickAt(i);
        for (int c = 0; desc != nullptr && c < (int)desc->chipCount && c < COMBO_MM_TRICK_MAX_CHIPS; c++) {
            bool known = false;
            for (const TagFilter& f : filters) {
                known = known || f.label == desc->chipLabels[c];
            }
            if (!known && desc->chipLabels[c] != nullptr) {
                filters.push_back(TagFilter{ desc->chipLabels[c], desc->chipTones[c], true });
            }
        }
    }
    std::stable_sort(filters.begin(), filters.end(),
                     [](const TagFilter& a, const TagFilter& b) { return ToneRank(a.tone) < ToneRank(b.tone); });
    return filters;
}

/**
 * A trick row's name cell, as DrawTricksMenu draws it: ImGui::Text after the
 * chips, clipped rather than wrapped (the column's child scrolls horizontally,
 * as SoH's does; wrapping inside a narrow column broke a long name into a word
 * or a letter per line), then UIWidgets::Tooltip. A disabled row's name is
 * dimmed by the disabled alpha rather than drawn inside BeginDisabled, so it
 * stays hoverable and shows its disabled tooltip, the tooltip a UIWidgets
 * widget with the same options would show (ComboUi_ShownTooltip). The cell's
 * rectangle goes to the combo_ui rect recorder, which is how the UI snapshot
 * harness finds and hovers a trick row.
 */
void DrawTrickName(const char* label, const ComboUiWidgetOpts& opts) {
    ImGui::SameLine();
    if (opts.disabled) {
        const ImGuiStyle& style = ImGui::GetStyle();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.Alpha * style.DisabledAlpha);
    }
    ImGui::Text("%s", label);
    if (opts.disabled) {
        ImGui::PopStyleVar();
    }
    const char* shown = ComboUi_ShownTooltip(&opts);
    if (shown != nullptr && shown[0] != '\0') {
        UIWidgets::Tooltip(shown);
    }
    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 b = ImGui::GetItemRectMax();
    ComboUi_NotifyRect(label, shown, a.x, a.y, b.x, b.y);
}

/** SoH's Rando::Tricks::CheckTags: a row shows only when every one of its tags does. */
bool TagsShown(const ComboMMTrickDesc* desc) {
    if (desc->chipCount == 0) {
        return false;
    }
    for (int c = 0; c < (int)desc->chipCount && c < COMBO_MM_TRICK_MAX_CHIPS; c++) {
        for (const TagFilter& f : TagFilters()) {
            if (f.label == desc->chipLabels[c] && !f.shown) {
                return false;
            }
        }
    }
    return true;
}

/**
 * Combo > MM Tricks' list, in the shape of SoH's Randomizer > Tricks/Glitches
 * (DrawTricksMenu, SohMenuRandomizer.cpp): a filter, "Disable All" and "Enable
 * All" at SoH's 250 px, the tag filter bar, then a bordered two-column
 * Disabled/Enabled table with
 * "Collapse All", "Open All" and "Enable/Disable Visible" over each column,
 * area tree nodes that start open, and one row per trick: SoH's themed arrow
 * button, the tag chips with the difficulty rung first, then the name, whose
 * tooltip is the trick's description.
 *
 * Where it differs from SoH, and why. A trick that cannot be turned on in this
 * build (reserved for an Ocarina of Time item, or not yet consulted by the
 * logic) stays in the Disabled column with its arrow disabled and its reason in
 * the disabled tooltip in SoH's shape; SoH has no such rows. Once the world is
 * frozen every arrow and button is disabled with "Already Decided", but the
 * areas still open, because a frozen trick set is worth reading. The name cell
 * (DrawTrickName) reports the row to the UI snapshot harness's hover finder
 * through the combo_ui rect recorder. Every tag in the
 * filter bar starts shown (SoH starts Glitch hidden), so the page opens on every
 * trick the pop-out window listed.
 */
void DrawMmTrickList(WidgetInfo& info) {
    static ImGuiTextFilter trickFilter;
    static std::map<int, bool> areaOpenDisabled;
    static std::map<int, bool> areaOpenEnabled;

    const int count = Combo_MMTrickCount();
    if (count == 0) {
        return; // the page's note says the table is missing
    }
    const bool frozen = Combo_MMProfileFrozen();
    const char* frozenTooltip = SohMenu::DisabledTooltip(COMBO_MM_OPTIONS_FROZEN_REASON);
    TrickMove move;
    bool clearAll = false;
    bool enableAll = false;

    static ImVec2 cellPadding(8.0f, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, cellPadding);
    ImGui::BeginDisabled(CVarGetInteger(CVAR_SETTING("DisableChanges"), 0));

    UIWidgets::PushStyleInput(THEME_COLOR);
    trickFilter.Draw("Filter (inc,-exc)##MMTricks", 490.0f);
    UIWidgets::PopStyleInput();

    ButtonOptions allOptions = ButtonOptions().Color(THEME_COLOR).Size(ImVec2(250.f, 0.f));
    allOptions.disabled = frozen;
    allOptions.disabledTooltip = frozenTooltip;
    ImGui::SameLine();
    if (UIWidgets::Button("Disable All##MMTricks",
                          ButtonOptions(allOptions).Tooltip("Turns off every Majora's Mask trick.")) &&
        !frozen) {
        clearAll = true;
    }
    ImGui::SameLine();
    if (UIWidgets::Button(
            "Enable All##MMTricks",
            ButtonOptions(allOptions).Tooltip("Turns on every Majora's Mask trick the randomizer logic supports.")) &&
        !frozen) {
        enableAll = true;
    }

    // SoH's tag filter bar (DrawTricksMenu's "trickTags" table): one selectable
    // per tag in the tag's colour; a row shows only while all its tags do.
    std::vector<TagFilter>& tags = TagFilters();
    if (!tags.empty() &&
        ImGui::BeginTable("mmTrickTags", (int)tags.size(),
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders)) {
        for (TagFilter& tag : tags) {
            ImGui::TableNextColumn();
            // SoH's GetTextColor: white on every tag but the white one.
            const Colors text = (tag.shown && tag.tone == COMBO_UI_TONE_WHITE) ? Colors::Black : Colors::White;
            ImGui::PushStyleColor(ImGuiCol_Text, UIWidgets::ColorValues.at(text));
            ImGui::PushStyleColor(ImGuiCol_Header, UIWidgets::ColorValues.at(ToneColor(tag.tone)));
            ImGui::Selectable((tag.label + "##MMTrickTag").c_str(), &tag.shown);
            ImGui::PopStyleColor(2);
        }
        ImGui::EndTable();
    }
    auto visible = [](const ComboMMTrickDesc* desc) { return trickFilter.PassFilter(desc->label) && TagsShown(desc); };

    if (ImGui::BeginTable("tableMmTricks", 2, ImGuiTableFlags_BordersH | ImGuiTableFlags_BordersV)) {
        ImGui::TableSetupColumn("Disabled Tricks", ImGuiTableColumnFlags_WidthStretch, 200.0f);
        ImGui::TableSetupColumn("Enabled Tricks", ImGuiTableColumnFlags_WidthStretch, 200.0f);
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::TableHeadersRow();
        ImGui::PopItemFlag();
        ImGui::TableNextRow();

        for (int column = 0; column < 2; column++) {
            const bool enabledColumn = column == 1;
            std::map<int, bool>& areaOpen = enabledColumn ? areaOpenEnabled : areaOpenDisabled;
            const std::string tag = enabledColumn ? "##enabled" : "##disabled";
            ImGui::TableNextColumn();

            if (UIWidgets::Button(("Collapse All" + tag).c_str(),
                                  ButtonOptions().Color(THEME_COLOR).Size(ImVec2(0.f, 0.f)))) {
                for (int area = 0; area < 256; area++) {
                    areaOpen[area] = false;
                }
            }
            ImGui::SameLine();
            if (UIWidgets::Button(("Open All" + tag).c_str(),
                                  ButtonOptions().Color(THEME_COLOR).Size(ImVec2(0.f, 0.f)))) {
                for (int area = 0; area < 256; area++) {
                    areaOpen[area] = true;
                }
            }
            ImGui::SameLine();
            ButtonOptions visibleOptions = ButtonOptions().Color(THEME_COLOR).Size(ImVec2(0.f, 0.f));
            visibleOptions.disabled = frozen;
            visibleOptions.disabledTooltip = frozenTooltip;
            if (UIWidgets::Button(enabledColumn ? "Disable Visible##MMTricks" : "Enable Visible##MMTricks",
                                  visibleOptions) &&
                !frozen) {
                for (int i = 0; i < count; i++) {
                    const ComboMMTrickDesc* desc = Combo_MMTrickAt(i);
                    const bool settable = desc != nullptr && desc->bound && !desc->reserved;
                    if (settable && InColumn(desc, enabledColumn) && visible(desc) &&
                        (!areaOpen.contains((int)desc->area) || areaOpen.at((int)desc->area))) {
                        Combo_MMTrickSetValue(desc, !enabledColumn);
                    }
                }
            }

            ImGui::BeginChild(enabledColumn ? "ChildMmTricksEnabled" : "ChildMmTricksDisabled", ImVec2(0, -8), false,
                              ImGuiWindowFlags_HorizontalScrollbar);
            // Areas in area-enum order, as the table numbers them; the bound is
            // the uint8_t field's range, so this TU learns no MM enum.
            for (int area = 0; area < 256; area++) {
                const char* areaName = nullptr;
                for (int i = 0; i < count && areaName == nullptr; i++) {
                    const ComboMMTrickDesc* desc = Combo_MMTrickAt(i);
                    if (desc != nullptr && (int)desc->area == area && InColumn(desc, enabledColumn) && visible(desc)) {
                        areaName = desc->areaName;
                    }
                }
                if (areaName == nullptr) {
                    continue;
                }
                const std::string node = std::string(areaName) + tag;
                if (!areaOpen.contains(area)) {
                    areaOpen[area] = true; // SoH's areas start open
                }
                ImGui::TreeNodeSetOpen(ImGui::GetID(node.c_str()), areaOpen[area]);
                if (ImGui::TreeNode(node.c_str())) {
                    for (int i = 0; i < count; i++) {
                        const ComboMMTrickDesc* desc = Combo_MMTrickAt(i);
                        if (desc == nullptr || (int)desc->area != area || !InColumn(desc, enabledColumn) ||
                            !visible(desc)) {
                            continue;
                        }
                        const char* reason = "";
                        const ComboMMRowState state = Combo_MMOptionsPage_TrickState(desc, &reason);
                        ComboUiWidgetOpts opts;
                        opts.tooltip = desc->tooltip;
                        opts.disabled = state != COMBO_MM_ROW_LIVE;
                        opts.disabledTooltip = opts.disabled ? SohMenu::DisabledTooltip(reason) : "";

                        ImGui::PushID(i);
                        UIWidgets::PushStyleButton(THEME_COLOR, ImVec2(7.f, 5.f));
                        ImGui::BeginDisabled(opts.disabled);
                        if (ImGui::ArrowButton("move", enabledColumn ? ImGuiDir_Left : ImGuiDir_Right)) {
                            move.index = i;
                            move.on = !enabledColumn;
                        }
                        ImGui::EndDisabled();
                        UIWidgets::PopStyleButton();
                        for (int c = 0; c < (int)desc->chipCount && c < COMBO_MM_TRICK_MAX_CHIPS; c++) {
                            ComboUi_Get()->TagChip(desc->chipLabels[c], desc->chipTones[c], &opts);
                        }
                        DrawTrickName(desc->label, opts);
                        ImGui::PopID();
                    }
                    areaOpen[area] = true;
                    ImGui::TreePop();
                } else {
                    areaOpen[area] = false;
                }
            }
            ImGui::EndChild();
        }
        ImGui::EndTable();
    }
    ImGui::EndDisabled();
    ImGui::PopStyleVar(1);

    // The writers refuse while frozen and refuse a reserved or unbound trick, so
    // these apply only what the rows above offered.
    if (move.index >= 0) {
        Combo_MMTrickSetValue(Combo_MMTrickAt(move.index), move.on);
    }
    if (clearAll) {
        for (int i = 0; i < count; i++) {
            Combo_MMTrickClear(Combo_MMTrickAt(i));
        }
    }
    if (enableAll) {
        for (int i = 0; i < count; i++) {
            const ComboMMTrickDesc* desc = Combo_MMTrickAt(i);
            if (desc != nullptr && desc->bound && !desc->reserved) {
                Combo_MMTrickSetValue(desc, true);
            }
        }
    }
}

} // namespace

/**
 * Combo > MM Randomizer: MM's 47 randomizer options as rows, laid out like SoH's
 * Randomizer > General (two columns, SEPARATOR_TEXT groups in MM's own
 * taxonomy, the primary button at 250 px). Column 1 opens with the page's state
 * notes and its two warnings, then Logic & Conditions and Shuffle Options;
 * column 2 holds Items, Starting Items, Hints and Reset. The column of each
 * group is the model's (Combo_MMOptionsPage_GroupColumn).
 *
 * No "[Both Games]" marker: every row is a tier-3 Majora's Mask key, none is in
 * RSBS::kSharedIntentKeys, so no row claims to apply to both games (ADR 0004
 * section 4.1a's consequence, unchanged by the move).
 */
void AddMmRandomizerOptionsWidgets(SohMenu& menu, WidgetPath& path) {
    // The menu is built during OoT's start-up, before rsbs/src/main.cpp's own
    // call, so the tables are published here first. Idempotent.
    Combo_MMOptionsPages_Init();

    path.column = SECTION_COLUMN_1;
    // The state note: frozen, a legacy pair, or no pair yet. Legible without
    // hovering, and the one part of the freeze a race lockout cannot replace.
    AddNote(menu, path, "MM Randomizer Status", [](WidgetInfo& info) {
        sStatusNote = Combo_MMOptionsPage_StatusNote();
        info.name = sStatusNote;
    });
    // ADR 0004 section 6's editable-but-not-active note, SohMenu's idiom for it.
    AddNote(menu, path, "MM Randomizer Suspended", [](WidgetInfo& info) {
        SohMenu::ApplyPresentationNote(
            info, Combo_MMOptionsPage_Suspended() ? SOH_MENU_PRESENT_INACTIVE_GAME : SOH_MENU_PRESENT_LIVE,
            Combo_MMOptionsPage_SuspendedNote());
    });

    const int count = Combo_MMOptionCount();
    if (count == 0) {
        // The MM table did not register. Say so rather than draw an empty page,
        // which would read as "MM has no options" (and would be #640).
        menu.AddWidget(path, "The Majora's Mask option table is not available in this build.", WIDGET_TEXT)
            .RaceDisable(false)
            .HideInSearch(true)
            .Options(TextOptions().Color(Colors::Gray));
        return;
    }
    if (count > kMaxOptionRows) {
        SPDLOG_ERROR("Combo > MM Randomizer: the MM option table holds {} rows, more than the page's {}; the rest "
                     "are not drawn",
                     count, kMaxOptionRows);
    }

    // The page-wide warnings, in SoH's warning colour: properties of the paired
    // generation pipeline, not of any one option.
    menu.AddWidget(path, "MM Randomizer Lock Warning", WIDGET_TEXT)
        .RaceDisable(false)
        .HideInSearch(true)
        .PreFunc([](WidgetInfo& info) {
            const char* warning = Combo_MMOptionsPage_LockWarning();
            info.isHidden = warning == nullptr;
            if (warning != nullptr && info.name != warning) {
                info.name = warning;
            }
        })
        .Options(TextOptions().Color(Colors::Orange));
    menu.AddWidget(path, Combo_MMOptionsPage_FallbackWarning(), WIDGET_TEXT)
        .RaceDisable(false)
        .HideInSearch(true)
        .Options(TextOptions().Color(Colors::Orange));

    for (int column = 0; column < COMBO_MM_OPTIONS_PAGE_COLUMNS; column++) {
        path.column = column == 0 ? SECTION_COLUMN_1 : SECTION_COLUMN_2;
        for (uint8_t group = 0; group < (uint8_t)COMBO_MM_GROUP_COUNT; group++) {
            if (Combo_MMOptionsPage_GroupColumn(group) != column) {
                continue;
            }
            // No header for an empty group: a permanently empty section implies
            // options are missing from it.
            int inGroup = 0;
            for (int i = 0; i < count && i < kMaxOptionRows; i++) {
                const ComboMMOptionDesc* desc = Combo_MMOptionAt(i);
                if (desc != nullptr && desc->group == group) {
                    inGroup++;
                }
            }
            if (inGroup == 0) {
                continue;
            }
            menu.AddWidget(path, Combo_MMOptionGroupName(group), WIDGET_SEPARATOR_TEXT);
            // The group's gray note while any of its rows is disabled by
            // capability (ADR 0004's 2026-09-27 amendment, item 3): the state
            // legible without hovering. Hidden while every row is live, and
            // while frozen, where the status note already says it.
            AddNote(menu, path, "MM Randomizer Group Note", [group, count](WidgetInfo& info) {
                bool blocked = false;
                for (int i = 0; i < count && i < kMaxOptionRows && !blocked; i++) {
                    const ComboMMOptionDesc* desc = Combo_MMOptionAt(i);
                    blocked = desc != nullptr && desc->group == group &&
                              Combo_MMOptionsPage_OptionState(desc, nullptr) == COMBO_MM_ROW_BLOCKED;
                }
                SohMenu::ApplyPresentationNote(info, blocked ? SOH_MENU_PRESENT_CAPABILITY : SOH_MENU_PRESENT_LIVE,
                                               COMBO_MM_GROUP_NOTE_UNAVAILABLE);
            });
            for (int i = 0; i < count && i < kMaxOptionRows; i++) {
                const ComboMMOptionDesc* desc = Combo_MMOptionAt(i);
                if (desc != nullptr && desc->group == group) {
                    AddOptionRow(menu, path, i);
                }
            }
        }
    }

    // Reset confirms first, as SoH's destructive buttons do ("Clear Config",
    // SohMenuSettings.cpp); the popup's own Reset button does the work. Under a
    // frozen profile it is disabled with the freeze as its reason: a live Reset
    // there would be a control that (correctly) does nothing.
    path.column = SECTION_COLUMN_2;
    menu.AddWidget(path, "Reset MM Randomizer", WIDGET_BUTTON)
        .Callback([](WidgetInfo& info) {
            SohGui::RegisterPopup(
                COMBO_MM_OPTIONS_RESET_TITLE,
                "This will reset every Majora's Mask randomizer option and trick to its default "
                "value.\nContinue?",
                "Reset", "Cancel", []() { Combo_MMOptionsPage_ResetAll(); }, nullptr);
        })
        .PreFunc([](WidgetInfo& info) {
            SohMenu::ApplyPresentation(
                info, info.name, Combo_MMProfileFrozen() ? SOH_MENU_PRESENT_FROZEN : SOH_MENU_PRESENT_LIVE, nullptr);
        })
        .Options(ButtonOptions()
                     .Size(ImVec2(250.f, 0.f))
                     .Tooltip("Resets every Majora's Mask randomizer option and trick to its default value."));
}

/**
 * Combo > MM Tricks: one column, like SoH's Randomizer > Tricks/Glitches: the
 * page's gray note (the honest headline, or the freeze), a section header, and
 * the trick list (DrawMmTrickList).
 */
void AddMmTricksWidgets(SohMenu& menu, WidgetPath& path) {
    Combo_MMOptionsPages_Init();

    path.column = SECTION_COLUMN_1;
    AddNote(menu, path, "MM Tricks Status", [](WidgetInfo& info) {
        char note[256];
        sTricksNote = Combo_MMOptionsPage_TricksNote(note, sizeof(note));
        info.name = sTricksNote;
    });
    menu.AddWidget(path, "Majora's Mask Tricks", WIDGET_SEPARATOR_TEXT);
    // Kept out of the menu search: the list draws its own table, and SoH's
    // search indexes a row by its name, which here is not a trick.
    menu.AddWidget(path, "MM Trick List", WIDGET_CUSTOM)
        .RaceDisable(false)
        .HideInSearch(true)
        .CustomFunction(DrawMmTrickList);
}

} // namespace SohGui
