/**
 * @file ComboItemTrackerWindow.cpp
 * @brief Renders the unified item view as SoH's Item Tracker overlay (#458 U2),
 *        each game's section as that game's own icon grid (#458 U3).
 *
 * See ComboItemTrackerWindow.h for the contract. The file keeps no state of its
 * own: every section is collected from combo_item_view.h on every frame, so a
 * game switch, a save or a load shows on the next frame; the numbers carried
 * between frames (each section's measured header height, the fit last drawn)
 * live in the overlay window's ImGui state storage. src/common cannot include
 * UIWidgets, so the headers, notes, icons and counts go through the combo_ui seam.
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
// BgColor.Value with { 0, 0, 0, 0 }): clear, so only the icons show over the game.
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

float IconSizeSetting() {
    int size = CVarGetInteger(RSBS_CVAR_COMBO_ITEMS_ICON_SIZE, kComboItemTrackerIconSize);
    if (size < kComboItemTrackerIconSizeMin) {
        size = kComboItemTrackerIconSizeMin;
    }
    return (float)(size > kComboItemTrackerIconSizeMax ? kComboItemTrackerIconSizeMax : size);
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
    // The two additions to SoH's chrome. The floating overlay is fitted to the
    // game window (ComboItemTrackerFitBoxes); should even the smallest scale
    // not fit, it is held to the game window's bottom edge rather than drawn
    // past it. The window type's title bar is kept whole: ImGui's auto-fit
    // sizes to the contents alone, and a short state ("No data.") would cut
    // the title.
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

/** One section's layout for this frame: icons or text, its cells, its box. */
struct SectionPlan {
    bool icons = false;
    std::vector<ComboItemGridCell> cells;
    std::vector<float> textWidths; // text fallback: each row's width at scale 1
    ComboItemSectionBox box;
};

/** The window-local x of screen x, for PushTextWrapPos. */
float LocalX(float screenX) {
    return screenX - ImGui::GetWindowPos().x + ImGui::GetScrollX();
}

/**
 * A cell's count, at kComboItemTrackerCountPx: SoH's DrawItemCount centres it
 * on the icon's bottom edge (its text starts 14 px above the line under the
 * icon), MM's DrawItemCounts puts it in the cell's bottom-right corner, 2 px
 * in. `cellMin` and `cell` are the cell's screen position and edge.
 */
void DrawCount(const ComboItemRow& row, uint8_t countStyle, ImVec2 cellMin, float cell, float scale, float countScale) {
    ComboItemCount count;
    if (!ComboItemCountFor(row, countStyle, count)) {
        return;
    }
    ImGui::SetWindowFontScale(countScale);
    const float amountWidth = ImGui::CalcTextSize(count.amount.c_str()).x;
    const float width = amountWidth + ImGui::CalcTextSize(count.ceiling.c_str()).x;
    const float height = ImGui::GetFontSize();
    ImVec2 at;
    if (countStyle == COMBO_ITEM_COUNT_MM) {
        at = ImVec2(cellMin.x + cell - width - 2.0f * scale, cellMin.y + cell - height - 2.0f * scale);
    } else {
        at = ImVec2(cellMin.x + (cell - width) * 0.5f, cellMin.y + cell - height * 0.6f);
    }
    ImGui::SetCursorScreenPos(at);
    Ui().ToneText(count.amount.c_str(), count.amountTone);
    if (!count.ceiling.empty()) {
        ImGui::SetCursorScreenPos(ImVec2(at.x + amountWidth, at.y));
        Ui().ToneText(count.ceiling.c_str(), count.ceilingTone);
    }
    ImGui::SetWindowFontScale(scale);
}

/** A cell of an icon section whose own texture is missing: its name, wrapped and clipped to the cell. */
void DrawCellText(const ComboItemRow& row, ImVec2 cellMin, float cell) {
    const ImVec2 cellMax(cellMin.x + cell, cellMin.y + cell);
    ImGui::PushClipRect(cellMin, cellMax, true);
    ImGui::SetCursorScreenPos(cellMin);
    ImGui::PushTextWrapPos(LocalX(cellMax.x));
    ImGui::BeginDisabled(!row.have);
    ImGui::TextUnformatted(row.name);
    ImGui::EndDisabled();
    ImGui::PopTextWrapPos();
    ImGui::PopClipRect();
}

/** An icon section's grid at `origin`, as its game's own tracker draws it. */
void DrawIconGrid(const ComboItemTrackerSection& section, const SectionPlan& plan, ImVec2 origin, float iconSize,
                  float scale, float countScale) {
    const ComboItemGridStyle& grid = *section.grid;
    const float unit = ComboItemIconUnit(iconSize) * scale;
    const float cell = grid.cellPx * unit;
    const float pitch = (grid.cellPx + grid.gapPx) * unit;
    const float baseAlpha = ImGui::GetStyle().Alpha;
    for (size_t i = 0; i < section.rows.size() && i < plan.cells.size(); i++) {
        const ComboItemRow& row = section.rows[i];
        const ImVec2 cellMin(origin.x + (float)plan.cells[i].column * pitch, origin.y + (float)plan.cells[i].line * pitch);
        const ComboItemIconPick pick = ComboItemPickIcon(row);
        const float iconWidth = row.iconAspect > 0.0f ? cell * row.iconAspect : cell;
        ImGui::SetCursorScreenPos(ImVec2(cellMin.x + (cell - iconWidth) * 0.5f, cellMin.y));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, baseAlpha * pick.alpha);
        const bool drawn = Ui().Image(pick.key, iconWidth, cell);
        ImGui::PopStyleVar();
        if (!drawn) {
            DrawCellText(row, cellMin, cell);
        }
        // The item's name (and its count) on hover, as both native trackers do.
        Ui().Tooltip(ComboItemRowText(row).c_str());
        DrawCount(row, grid.countStyle, cellMin, cell, scale, countScale);
    }
}

/** A text-fallback section at `origin`: U2's grid, each row dimmed while not held. */
void DrawTextGrid(const ComboItemTrackerSection& section, const SectionPlan& plan, ImVec2 origin, float scale,
                  const ComboItemTrackerMetrics& metrics) {
    std::vector<ComboItemTrackerSection> one(1, section);
    const std::vector<std::vector<float>> widths(1, plan.textWidths);
    const std::vector<float> columnWidths = ComboItemGridColumnWidths(one, widths, kComboItemTrackerColumns);
    std::vector<float> columnX(columnWidths.size(), 0.0f);
    for (size_t c = 1; c < columnX.size(); c++) {
        columnX[c] = columnX[c - 1] + columnWidths[c - 1] * scale + metrics.columnGap;
    }
    std::vector<ComboItemGridCell> cells;
    ComboItemGridLayout(section.rows, kComboItemTrackerColumns, cells);
    const float pitch = metrics.textHeight * scale + metrics.linePadding;
    for (size_t i = 0; i < section.rows.size(); i++) {
        const ComboItemRow& row = section.rows[i];
        const size_t column = (size_t)cells[i].column < columnX.size() ? (size_t)cells[i].column : 0;
        ImGui::SetCursorScreenPos(ImVec2(origin.x + columnX[column], origin.y + (float)cells[i].line * pitch));
        // A row not held is dimmed the way a disabled row is: the text twin
        // of SoH's faded icon, with no colour of its own.
        const std::string text = ComboItemRowText(row);
        ImGui::BeginDisabled(!row.have);
        ImGui::TextUnformatted(text.c_str());
        ImGui::EndDisabled();
    }
}

// Keys in the overlay window's ImGui state storage: each section's measured
// header height, and the scale last drawn (the fit's hysteresis).
constexpr const char* kHeaderKeys[COMBO_ITEM_SECTION_COUNT] = { "##fitHeaderOoT", "##fitHeaderMM",
                                                                "##fitHeaderShared" };
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

void ComboItemTrackerCollectSection(int section, ComboItemTrackerSection& out, int sharedIconGame) {
    out = ComboItemTrackerSection();
    out.id = section;
    ComboItemRow row;
    if (section == COMBO_ITEM_SECTION_SHARED) {
        out.title = COMBO_ITEM_SHARED_GROUP;
        out.freshness = Combo_ItemSharedFreshness();
        out.note = std::string(Combo_ItemSharedLabel()) + ".";
        out.grid = &kComboItemSharedGrid;
        const int count = Combo_ItemSharedCount();
        for (int i = 0; i < count; i++) {
            const bool ok = (sharedIconGame == (int)GAME_OOT || sharedIconGame == (int)GAME_MM)
                                ? Combo_ItemSharedRowAtWithIcons(i, (uint8_t)sharedIconGame, &row)
                                : Combo_ItemSharedRowAt(i, &row);
            if (!ok) {
                break;
            }
            out.rows.push_back(row);
        }
        return;
    }
    const uint8_t game = section == COMBO_ITEM_SECTION_MM ? (uint8_t)GAME_MM : (uint8_t)GAME_OOT;
    out.title = game == (uint8_t)GAME_MM ? "Majora's Mask" : "Ocarina of Time";
    out.freshness = Combo_ItemFreshness(game);
    out.note = std::string(Combo_ItemFreshnessLabel(game, out.freshness)) + ".";
    out.grid = Combo_ItemGridStyle(game);
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

// ============================================================================
// Icons (#458 U3)
// ============================================================================

void ComboItemIconLayout(const std::vector<ComboItemRow>& rows, const ComboItemGridStyle& grid,
                         std::vector<ComboItemGridCell>& out) {
    out.clear();
    int line = -1;
    int column = 0;
    int columns = 0;
    const char* group = nullptr;
    for (const ComboItemRow& row : rows) {
        const bool newGroup = group == nullptr || row.group == nullptr || strcmp(row.group, group) != 0;
        if (line < 0 || (grid.groupLines && newGroup)) {
            // A new line, at the group's own width.
            columns = (grid.groupColumns != nullptr) ? grid.groupColumns(row.group) : grid.columns;
            columns = columns < 1 ? 1 : columns;
            line++;
            column = 0;
        } else if (column >= columns) {
            line++;
            column = 0;
        }
        out.push_back({ line, column });
        column++;
        group = row.group;
    }
}

ComboItemIconPick ComboItemPickIcon(const ComboItemRow& row) {
    ComboItemIconPick pick;
    if (row.have) {
        pick.key = row.iconKey;
        return pick;
    }
    pick.key = row.iconKeyFaded;
    pick.alpha = row.fadedAlpha > 0.0f ? row.fadedAlpha : 1.0f;
    return pick;
}

bool ComboItemCountFor(const ComboItemRow& row, uint8_t countStyle, ComboItemCount& out) {
    out = ComboItemCount();
    if (!row.have && row.count == 0) {
        return false; // a faded icon carries no number
    }
    if (countStyle == COMBO_ITEM_COUNT_MM) {
        if (row.count <= 0) {
            return false;
        }
        out.amount = std::to_string(row.count);
        return true;
    }
    if (row.max > 0) {
        out.amount = std::to_string(row.count);
        out.ceiling = "/" + std::to_string(row.max);
        out.amountTone = row.count >= row.max ? COMBO_UI_TONE_GREEN
                         : row.count <= 0     ? COMBO_UI_TONE_GRAY
                                              : COMBO_UI_TONE_WHITE;
        return true;
    }
    if (row.count > 0) {
        out.amount = std::to_string(row.count);
        return true;
    }
    return false;
}

bool ComboItemSectionDrawsIcons(const std::vector<ComboItemRow>& rows, bool (*hasImage)(const char*)) {
    if (hasImage == nullptr) {
        return false;
    }
    for (const ComboItemRow& row : rows) {
        const ComboItemIconPick pick = ComboItemPickIcon(row);
        if (pick.key != nullptr && hasImage(pick.key)) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// The fit
// ============================================================================

float ComboItemIconUnit(float iconSize) {
    return iconSize / (float)kComboItemTrackerIconSize;
}

ComboItemSectionBox ComboItemIconBox(const std::vector<ComboItemGridCell>& cells, const ComboItemGridStyle& grid,
                                     const ComboItemTrackerMetrics& metrics, float header) {
    ComboItemSectionBox box;
    box.fixedWidth = metrics.cellPadding;
    box.fixedHeight = header;
    if (cells.empty()) {
        return box;
    }
    int columns = 0;
    for (const ComboItemGridCell& c : cells) {
        columns = c.column + 1 > columns ? c.column + 1 : columns;
    }
    const float unit = ComboItemIconUnit(metrics.iconSize);
    const float pitch = (grid.cellPx + grid.gapPx) * unit;
    box.width = (float)(columns - 1) * pitch + grid.cellPx * unit;
    box.height = (float)(cells.back().line + 1) * pitch;
    return box;
}

ComboItemSectionBox ComboItemTextBox(const ComboItemTrackerSection& section, const std::vector<float>& widths,
                                     const ComboItemTrackerMetrics& metrics, float header) {
    ComboItemSectionBox box;
    box.fixedWidth = metrics.cellPadding;
    box.fixedHeight = header;
    if (section.rows.empty()) {
        return box;
    }
    const std::vector<ComboItemTrackerSection> one(1, section);
    const std::vector<std::vector<float>> w(1, widths);
    const std::vector<float> columnWidths = ComboItemGridColumnWidths(one, w, kComboItemTrackerColumns);
    int used = 0;
    for (size_t c = 0; c < columnWidths.size(); c++) {
        box.width += columnWidths[c];
        used += columnWidths[c] > 0.0f ? 1 : 0;
    }
    box.fixedWidth += (float)(used > 1 ? used - 1 : 0) * metrics.columnGap;
    const int lines = ComboItemGridLines(section.rows, kComboItemTrackerColumns);
    box.height = (float)lines * metrics.textHeight;
    box.fixedHeight += (float)lines * metrics.linePadding;
    return box;
}

ComboItemTrackerFit ComboItemTrackerFitBoxes(const std::vector<ComboItemSectionBox>& boxes, float availWidth,
                                             float availHeight, const ComboItemTrackerFit* previous) {
    float raw = 1.0f;
    float scalableWidth = 0.0f;
    float fixedWidth = 0.0f;
    for (const ComboItemSectionBox& box : boxes) {
        scalableWidth += box.width;
        fixedWidth += box.fixedWidth;
        if (box.height > 0.0f) {
            const float byHeight = (availHeight - box.fixedHeight) / box.height;
            raw = byHeight < raw ? byHeight : raw;
        }
    }
    if (scalableWidth > 0.0f) {
        const float byWidth = (availWidth - fixedWidth) / scalableWidth;
        raw = byWidth < raw ? byWidth : raw;
    }
    ComboItemTrackerFit fit;
    // Whole steps, down: the overlay never grows past what fits.
    fit.scale = raw >= 1.0f ? 1.0f : (float)(int)(raw / kComboItemTrackerScaleStep) * kComboItemTrackerScaleStep;
    // A step up waits until it fits by half a step more; a step down is taken at once.
    if (previous != nullptr && fit.scale > previous->scale &&
        raw < previous->scale + kComboItemTrackerScaleStep * 1.5f) {
        fit.scale = previous->scale;
    }
    if (fit.scale < kComboItemTrackerMinScale) {
        fit.scale = kComboItemTrackerMinScale;
    }
    return fit;
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
    if (sections.empty()) {
        return;
    }

    // Measured at scale 1, for the boxes and the fit.
    ImGui::SetWindowFontScale(1.0f);
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGuiStorage* storage = ImGui::GetStateStorage();
    ComboItemTrackerMetrics metrics;
    metrics.iconSize = IconSizeSetting();
    metrics.textHeight = ImGui::GetFontSize();
    metrics.linePadding = style.ItemSpacing.y;
    metrics.columnGap = style.ItemSpacing.x * 2.0f;
    metrics.cellPadding = style.CellPadding.x * 2.0f;
    const float countScale = kComboItemTrackerCountPx / metrics.textHeight;

    std::vector<SectionPlan> plans(sections.size());
    std::vector<ComboItemSectionBox> boxes(sections.size());
    for (size_t s = 0; s < sections.size(); s++) {
        ComboItemTrackerSection& section = sections[s];
        SectionPlan& plan = plans[s];
        plan.icons = ComboItemSectionDrawsIcons(section.rows, Ui().HasImage);
        if (!plan.icons && section.id == COMBO_ITEM_SECTION_SHARED && !section.rows.empty()) {
            // The active game's icons are not loaded (MM's, before MM has
            // booted): the pool takes the other game's.
            const int other = Context_GetCurrentGame() == GAME_MM ? (int)GAME_OOT : (int)GAME_MM;
            ComboItemTrackerSection again;
            ComboItemTrackerCollectSection(section.id, again, other);
            if (ComboItemSectionDrawsIcons(again.rows, Ui().HasImage)) {
                section = again;
                plan.icons = true;
            }
        }
        // The header and note, as measured last frame; before the first, a
        // generous three padded lines.
        const float guess = 3.0f * (metrics.textHeight + style.FramePadding.y * 2.0f + style.ItemSpacing.y);
        const float header = storage->GetFloat(ImGui::GetID(kHeaderKeys[section.id]), guess);
        if (plan.icons) {
            ComboItemIconLayout(section.rows, *section.grid, plan.cells);
            boxes[s] = ComboItemIconBox(plan.cells, *section.grid, metrics, header);
        } else {
            for (const ComboItemRow& row : section.rows) {
                plan.textWidths.push_back(ImGui::CalcTextSize(ComboItemRowText(row).c_str()).x);
            }
            boxes[s] = ComboItemTextBox(section, plan.textWidths, metrics, header);
        }
        // A column is never narrower than its title (an empty "No data." section).
        const float title = ImGui::CalcTextSize(section.title).x;
        boxes[s].width = boxes[s].width > title ? boxes[s].width : title;
        plan.box = boxes[s];
    }

    ComboItemTrackerFit fit;
    const int windowType = CVarGetInteger(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE, COMBO_ITEM_TRACKER_FLOATING);
    if (windowType == COMBO_ITEM_TRACKER_FLOATING) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        // One item spacing of margin each way absorbs ImGui's pixel rounding.
        const float availWidth = viewport->WorkPos.x + viewport->WorkSize.x - window->Pos.x -
                                 style.WindowPadding.x * 2.0f - style.ItemSpacing.x;
        const float availHeight = viewport->WorkPos.y + viewport->WorkSize.y - window->Pos.y -
                                  style.WindowPadding.y * 2.0f - style.ItemSpacing.y;
        ComboItemTrackerFit previous;
        previous.scale = storage->GetFloat(ImGui::GetID(kScaleKey), 1.0f);
        fit = ComboItemTrackerFitBoxes(boxes, availWidth, availHeight, &previous);
        storage->SetFloat(ImGui::GetID(kScaleKey), fit.scale);
    }

    // The sections side by side, one table column each, so a section's header
    // and its wrapped note keep to its own column.
    std::vector<ImGuiID> headerIds;
    for (const ComboItemTrackerSection& section : sections) {
        headerIds.push_back(ImGui::GetID(kHeaderKeys[section.id]));
    }
    ImGui::SetWindowFontScale(fit.scale);
    const ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings;
    if (!ImGui::BeginTable("##sections", (int)sections.size(), tableFlags)) {
        return;
    }
    for (size_t s = 0; s < sections.size(); s++) {
        ImGui::TableSetupColumn(sections[s].title, ImGuiTableColumnFlags_WidthFixed, plans[s].box.width * fit.scale);
    }
    ImGui::TableNextRow();
    for (size_t s = 0; s < sections.size(); s++) {
        const ComboItemTrackerSection& section = sections[s];
        ImGui::TableNextColumn();
        const float top = ImGui::GetCursorScreenPos().y;
        Ui().SeparatorText(section.title);
        Ui().NoteText(section.note.c_str());
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        // The next frame's fit reads the header's height as drawn.
        storage->SetFloat(headerIds[s], origin.y - top);
        if (section.rows.empty()) {
            continue;
        }
        if (plans[s].icons) {
            DrawIconGrid(section, plans[s], origin, metrics.iconSize, fit.scale, countScale * fit.scale);
        } else {
            DrawTextGrid(section, plans[s], origin, fit.scale, metrics);
        }
    }
    ImGui::EndTable();
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
