/**
 * @file ComboItemTrackerWindow.cpp
 * @brief Renders the unified item view as SoH's Item Tracker overlay (#458 U2).
 *
 * See ComboItemTrackerWindow.h for the contract. The file keeps no state of its
 * own: every section is collected from combo_item_view.h on every frame, so a
 * game switch, a save or a load shows on the next frame; the one number carried
 * between frames (the headers' measured height, for the fit) lives in the
 * overlay window's ImGui state storage. src/common cannot include UIWidgets, so
 * the section headers and notes go through the combo_ui seam.
 */

#include "ComboItemTrackerWindow.h"

#include <cfloat>
#include <cstdio>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>
#include <ship/window/gui/Gui.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "combo_ui.h"
#include "context.h" // GameId

namespace ComboGui {

namespace {

const ComboUiTable& Ui() {
    return *ComboUi_Get();
}

// SoH's default overlay background (BeginFloatingWindows: CVarGetColor of
// BgColor.Value with { 0, 0, 0, 0 }): clear, so only the rows show over the game.
constexpr Color_RGBA8 kBgDefault = { 0, 0, 0, 0 };

const char* const kSectionKeys[COMBO_ITEM_SECTION_COUNT] = {
    RSBS_CVAR_COMBO_ITEMS_SECTION_OOT,
    RSBS_CVAR_COMBO_ITEMS_SECTION_MM,
    RSBS_CVAR_COMBO_ITEMS_SECTION_SHARED,
};

float OverlayOpacity() {
    float opacity = CVarGetFloat(RSBS_CVAR_COMBO_ITEMS_OPACITY, 1.0f);
    if (opacity < 0.1f) {
        opacity = 0.1f;
    }
    return opacity > 1.0f ? 1.0f : opacity;
}

/**
 * BeginFloatingWindows (randomizer_item_tracker.cpp), statement for statement
 * where it applies to one overlay: the flags, the main viewport while floating,
 * the background colour (opaque while docked in the menu's deck, as SoH's is),
 * a transparent border and rounding 4. The overlay's Opacity is one more alpha
 * over all of it. Pushes four style entries; EndOverlay pops them.
 */
void BeginOverlay(int windowType) {
    const bool draggable = CVarGetInteger(RSBS_CVAR_COMBO_ITEMS_DRAGGABLE, 0) != 0;
    const ImGuiWindowFlags flags = (ImGuiWindowFlags)ComboItemTrackerWindowFlags(windowType, draggable);
    if (windowType == COMBO_ITEM_TRACKER_FLOATING) {
        ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    }
    const Color_RGBA8 bg = CVarGetColor(RSBS_CVAR_COMBO_ITEMS_BG_COLOR ".Value", kBgDefault);
    ImVec4 color(bg.r / 255.0f, bg.g / 255.0f, bg.b / 255.0f, bg.a / 255.0f);
    ImGuiWindow* window = ImGui::FindWindowByName(kComboItemTrackerWindowName);
    // The two additions to SoH's chrome, both for text rows. The floating
    // overlay is fitted to the game window (ComboItemTrackerFitGrid); should
    // even the smallest scale not fit, it is held to the game window's bottom
    // edge rather than drawn past it. The window type's title
    // bar is kept whole: ImGui's auto-fit sizes to the contents alone, and a
    // short state ("No data.") would cut the title.
    ImVec2 minSize(0.0f, 0.0f);
    ImVec2 maxSize(FLT_MAX, FLT_MAX);
    if (windowType == COMBO_ITEM_TRACKER_FLOATING) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (window != nullptr && viewport->WorkPos.y + viewport->WorkSize.y - window->Pos.y > 0.0f) {
            maxSize.y = viewport->WorkPos.y + viewport->WorkSize.y - window->Pos.y;
        }
    } else {
        const ImGuiStyle& style = ImGui::GetStyle();
        minSize.x = ImGui::CalcTextSize(kComboItemTrackerWindowName).x + ImGui::GetFontSize() +
                    style.ItemInnerSpacing.x + style.FramePadding.x * 2.0f + style.WindowPadding.x * 2.0f;
    }
    ImGui::SetNextWindowSizeConstraints(minSize, maxSize);
    if (window != nullptr && window->DockTabIsVisible && window->ParentWindow != nullptr &&
        strncmp(window->ParentWindow->Name, "Main - Deck", strlen("Main - Deck")) == 0) {
        color.w = 1.0f;
    }
    ImVec4 noBorder = ImGui::GetStyleColorVec4(ImGuiCol_Border);
    noBorder.w = 0.0f;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, color);
    ImGui::PushStyleColor(ImGuiCol_Border, noBorder);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * OverlayOpacity());
    ImGui::Begin(kComboItemTrackerWindowName, nullptr, flags);
}

void EndOverlay() {
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

/**
 * One section: its header, its freshness note, and its grid of rows, each
 * column starting at the same x in every section (the fit's shared widths).
 * Returns the height the header and note took, for the next frame's fit.
 */
float DrawSection(const ComboItemTrackerSection& section, const ComboItemTrackerFit& fit, float columnGap) {
    const ImGuiWindow* window = ImGui::GetCurrentWindow();
    const float top = window->DC.CursorPos.y;
    Ui().SeparatorText(section.title);
    Ui().NoteText(section.note.c_str());
    const float header = window->DC.CursorPos.y - top;
    if (section.rows.empty()) {
        return header;
    }
    std::vector<ComboItemGridCell> cells;
    ComboItemGridLayout(section.rows, fit.columns, cells);
    std::vector<float> columnX(fit.columnWidths.size(), 0.0f);
    for (size_t c = 1; c < columnX.size(); c++) {
        columnX[c] = columnX[c - 1] + fit.columnWidths[c - 1] * fit.scale + columnGap;
    }
    // Each cell placed at its column and line, as SoH's DrawItemsInRows places
    // each icon (SetCursorPos at column * pitch, row * pitch).
    const ImVec2 start = ImGui::GetCursorPos();
    const float pitch = ImGui::GetFontSize() + ImGui::GetStyle().ItemSpacing.y;
    for (size_t i = 0; i < section.rows.size(); i++) {
        const ComboItemRow& row = section.rows[i];
        const size_t column = (size_t)cells[i].column < columnX.size() ? (size_t)cells[i].column : 0;
        ImGui::SetCursorPos(ImVec2(start.x + columnX[column], start.y + (float)cells[i].line * pitch));
        // A row not held is dimmed the way a disabled row is: the text twin
        // of SoH's faded icon, with no colour of its own.
        const std::string text = ComboItemRowText(row);
        ImGui::BeginDisabled(!row.have);
        ImGui::TextUnformatted(text.c_str());
        ImGui::EndDisabled();
    }
    return header;
}

// Keys in the overlay window's ImGui state storage: the headers' measured
// height and the fit last drawn (the fit's hysteresis).
constexpr const char* kOverheadKey = "##fitOverhead";
constexpr const char* kColumnsKey = "##fitColumns";
constexpr const char* kScaleKey = "##fitScale";

} // namespace

bool ComboItemTrackerShows(int windowType, bool showOnlyPaused, bool paused) {
    return windowType == COMBO_ITEM_TRACKER_WINDOW || !showOnlyPaused || paused;
}

int ComboItemTrackerWindowFlags(int windowType, bool draggable) {
    int flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoResize;
    if (windowType == COMBO_ITEM_TRACKER_FLOATING) {
        flags |= ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar;
        if (!draggable) {
            flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
        }
    }
    return flags;
}

std::string ComboItemRowText(const ComboItemRow& row) {
    std::string text = row.name != nullptr ? row.name : "";
    char amount[32];
    if (!row.have && row.count == 0) {
        return text; // nothing held, nothing counted: the dimmed name says it
    }
    if (row.max > 0) {
        snprintf(amount, sizeof(amount), " %d/%d", row.count, row.max);
        text += amount;
    } else if (row.count > 0) {
        snprintf(amount, sizeof(amount), " %d", row.count);
        text += amount;
    }
    return text;
}

void ComboItemTrackerCollectSection(int section, ComboItemTrackerSection& out) {
    out = ComboItemTrackerSection();
    out.id = section;
    ComboItemRow row;
    if (section == COMBO_ITEM_SECTION_SHARED) {
        out.title = COMBO_ITEM_SHARED_GROUP;
        out.freshness = Combo_ItemSharedFreshness();
        out.note = std::string(Combo_ItemSharedLabel()) + ".";
        const int count = Combo_ItemSharedCount();
        for (int i = 0; i < count && Combo_ItemSharedRowAt(i, &row); i++) {
            out.rows.push_back(row);
        }
        return;
    }
    const uint8_t game = section == COMBO_ITEM_SECTION_MM ? (uint8_t)GAME_MM : (uint8_t)GAME_OOT;
    out.title = game == (uint8_t)GAME_MM ? "Majora's Mask" : "Ocarina of Time";
    out.freshness = Combo_ItemFreshness(game);
    out.note = std::string(Combo_ItemFreshnessLabel(game, out.freshness)) + ".";
    const int count = Combo_ItemCount(game);
    for (int i = 0; i < count && Combo_ItemRowAt(game, i, &row); i++) {
        out.rows.push_back(row);
    }
}

bool ComboItemTrackerSectionShown(int section) {
    if (section < 0 || section >= COMBO_ITEM_SECTION_COUNT) {
        return false;
    }
    return CVarGetInteger(kSectionKeys[section], 1) != 0;
}

void ComboItemGridLayout(const std::vector<ComboItemRow>& rows, int columns, std::vector<ComboItemGridCell>& out) {
    out.clear();
    if (columns < 1) {
        columns = 1;
    }
    int line = -1;
    int column = columns;
    const char* group = nullptr;
    for (const ComboItemRow& row : rows) {
        const bool newGroup =
            group == nullptr || row.group == nullptr || strcmp(row.group, group) != 0; // groups are never NULL
        if (column >= columns || newGroup) {
            line++;
            column = 0;
        }
        out.push_back({ line, column });
        column++;
        group = row.group;
    }
}

int ComboItemGridLines(const std::vector<ComboItemRow>& rows, int columns) {
    std::vector<ComboItemGridCell> cells;
    ComboItemGridLayout(rows, columns, cells);
    return cells.empty() ? 0 : cells.back().line + 1;
}

std::vector<float> ComboItemGridColumnWidths(const std::vector<ComboItemTrackerSection>& sections,
                                             const std::vector<std::vector<float>>& widths, int columns) {
    if (columns < 1) {
        columns = 1;
    }
    std::vector<float> out((size_t)columns, 0.0f);
    std::vector<ComboItemGridCell> cells;
    for (size_t s = 0; s < sections.size() && s < widths.size(); s++) {
        ComboItemGridLayout(sections[s].rows, columns, cells);
        for (size_t r = 0; r < cells.size() && r < widths[s].size(); r++) {
            float& w = out[(size_t)cells[r].column];
            w = widths[s][r] > w ? widths[s][r] : w;
        }
    }
    return out;
}

namespace {

/** The largest scale (at most 1, unquantized) at which `columns` columns fit. */
float FitScaleFor(const std::vector<ComboItemTrackerSection>& sections, const std::vector<std::vector<float>>& widths,
                  const ComboItemTrackerMetrics& metrics, int columns, std::vector<float>& columnWidths) {
    columnWidths = ComboItemGridColumnWidths(sections, widths, columns);
    float textWidth = 0.0f;
    for (float w : columnWidths) {
        textWidth += w;
    }
    int lines = 0;
    for (const ComboItemTrackerSection& section : sections) {
        lines += ComboItemGridLines(section.rows, columns);
    }
    // Width: the text scales, the gaps do not. Height: each line is its text
    // (scales) plus its spacing (does not), under the headers.
    float scale = 1.0f;
    if (textWidth > 0.0f) {
        const float byWidth = (metrics.availWidth - (float)(columns - 1) * metrics.columnGap) / textWidth;
        scale = byWidth < scale ? byWidth : scale;
    }
    if (lines > 0 && metrics.textHeight > 0.0f) {
        const float byHeight = (metrics.availHeight - metrics.overhead - (float)lines * metrics.linePadding) /
                               ((float)lines * metrics.textHeight);
        scale = byHeight < scale ? byHeight : scale;
    }
    return scale;
}

int ColumnDistance(int columns) {
    return columns > kComboItemTrackerColumns ? columns - kComboItemTrackerColumns
                                              : kComboItemTrackerColumns - columns;
}

} // namespace

ComboItemTrackerFit ComboItemTrackerFitGrid(const std::vector<ComboItemTrackerSection>& sections,
                                            const std::vector<std::vector<float>>& widths,
                                            const ComboItemTrackerMetrics& metrics,
                                            const ComboItemTrackerFit* previous) {
    ComboItemTrackerFit best;
    best.scale = -FLT_MAX;
    std::vector<float> columnWidths;
    for (int columns = 1; columns <= kComboItemTrackerMaxColumns; columns++) {
        const float scale = FitScaleFor(sections, widths, metrics, columns, columnWidths);
        const bool larger = scale > best.scale + 1e-4f;
        const bool tie = !larger && scale > best.scale - 1e-4f;
        if (larger || (tie && ColumnDistance(columns) <= ColumnDistance(best.columns))) {
            best.columns = columns;
            best.scale = scale;
            best.columnWidths = columnWidths;
        }
    }
    // Hysteresis: the grid last drawn stays while it fits about as well (its
    // headers' height is measured from the frame before, so the best choice
    // can wobble by a pixel from frame to frame).
    const bool hold = previous != nullptr && previous->columns >= 1 && previous->columns <= kComboItemTrackerMaxColumns;
    if (hold && previous->columns != best.columns) {
        const float held = FitScaleFor(sections, widths, metrics, previous->columns, columnWidths);
        if (held + kComboItemTrackerScaleStep * 0.5f >= best.scale) {
            best.columns = previous->columns;
            best.scale = held;
            best.columnWidths = columnWidths;
        }
    }
    // Whole steps, down: the text never grows past what fits.
    const float raw = best.scale;
    best.scale = raw >= 1.0f ? 1.0f : (float)(int)(raw / kComboItemTrackerScaleStep) * kComboItemTrackerScaleStep;
    // A step up waits until it fits by half a step more; a step down is taken at once.
    if (hold && previous->columns == best.columns && best.scale > previous->scale &&
        raw < previous->scale + kComboItemTrackerScaleStep * 1.5f) {
        best.scale = previous->scale;
    }
    if (best.scale < kComboItemTrackerMinScale) {
        best.scale = kComboItemTrackerMinScale;
    }
    return best;
}

void ComboItemTrackerWindow::Draw() {
    // Read the visibility CVar LIVE rather than the ctor-latched IsVisible()
    // (#489 cause 1), as the Combo Tracker does.
    if (!CVarGetInteger(kComboItemTrackerVisibilityCVar, 0)) {
        return;
    }
    const int windowType = CVarGetInteger(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE, COMBO_ITEM_TRACKER_FLOATING);
    const bool showOnlyPaused = CVarGetInteger(RSBS_CVAR_COMBO_ITEMS_SHOW_ONLY_PAUSED, 0) != 0;
    if (!ComboItemTrackerShows(windowType, showOnlyPaused, showOnlyPaused && Combo_ItemActiveGamePaused())) {
        return;
    }
    BeginOverlay(windowType);
    DrawElement();
    EndOverlay();
}

void ComboItemTrackerWindow::DrawElement() {
    std::vector<ComboItemTrackerSection> sections;
    for (int s = 0; s < COMBO_ITEM_SECTION_COUNT; s++) {
        if (!ComboItemTrackerSectionShown(s)) {
            continue;
        }
        sections.emplace_back();
        ComboItemTrackerCollectSection(s, sections.back());
    }

    // Every row's width at scale 1, for the shared column widths and the fit.
    ImGui::SetWindowFontScale(1.0f);
    std::vector<std::vector<float>> widths(sections.size());
    for (size_t s = 0; s < sections.size(); s++) {
        for (const ComboItemRow& row : sections[s].rows) {
            widths[s].push_back(ImGui::CalcTextSize(ComboItemRowText(row).c_str()).x);
        }
    }
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID overheadId = ImGui::GetID(kOverheadKey);
    ComboItemTrackerMetrics metrics;
    metrics.textHeight = ImGui::GetFontSize();
    metrics.linePadding = style.ItemSpacing.y;
    metrics.columnGap = style.ItemSpacing.x * 2.0f;
    ComboItemTrackerFit fit;
    const int windowType = CVarGetInteger(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE, COMBO_ITEM_TRACKER_FLOATING);
    if (windowType == COMBO_ITEM_TRACKER_FLOATING) {
        // The headers and notes are measured from the last frame drawn (their
        // padding does not scale with the text); before the first, a generous
        // three padded lines per section.
        const float guess =
            (float)sections.size() * 3.0f * (metrics.textHeight + style.FramePadding.y * 2.0f + style.ItemSpacing.y);
        metrics.overhead = storage->GetFloat(overheadId, guess);
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        // One item spacing of margin each way absorbs ImGui's pixel rounding.
        metrics.availWidth = viewport->WorkPos.x + viewport->WorkSize.x - window->Pos.x -
                             style.WindowPadding.x * 2.0f - style.ItemSpacing.x;
        metrics.availHeight = viewport->WorkPos.y + viewport->WorkSize.y - window->Pos.y -
                              style.WindowPadding.y * 2.0f - style.ItemSpacing.y;
        ComboItemTrackerFit previous;
        previous.columns = storage->GetInt(ImGui::GetID(kColumnsKey), 0);
        previous.scale = storage->GetFloat(ImGui::GetID(kScaleKey), 1.0f);
        fit = ComboItemTrackerFitGrid(sections, widths, metrics, &previous);
        storage->SetInt(ImGui::GetID(kColumnsKey), fit.columns);
        storage->SetFloat(ImGui::GetID(kScaleKey), fit.scale);
    } else {
        fit.columnWidths = ComboItemGridColumnWidths(sections, widths, fit.columns);
    }

    ImGui::SetWindowFontScale(fit.scale);
    // What the spacers, headers and notes take, measured as drawn (a grid
    // line is placed at exactly its text plus ItemSpacing.y, the fit's own
    // model). The next frame's fit reads it.
    float overhead = 0.0f;
    for (size_t s = 0; s < sections.size(); s++) {
        if (s > 0) {
            const float top = window->DC.CursorPos.y;
            Ui().Spacer(0.0f);
            overhead += window->DC.CursorPos.y - top;
        }
        overhead += DrawSection(sections[s], fit, metrics.columnGap);
    }
    storage->SetFloat(overheadId, overhead);
}

void RegisterComboItemTrackerWindow(std::shared_ptr<Ship::Gui> gui) {
    if (gui == nullptr) {
        return;
    }
    // Idempotence, the #457 guard (AddGuiWindow rejects duplicates silently).
    if (gui->GetGuiWindow(kComboItemTrackerWindowName) != nullptr) {
        return;
    }
    gui->AddGuiWindow(
        std::make_shared<ComboItemTrackerWindow>(kComboItemTrackerVisibilityCVar, kComboItemTrackerWindowName));
}

} // namespace ComboGui
