/**
 * @file ComboItemTrackerWindow.cpp
 * @brief Renders the unified item view as SoH's Item Tracker overlay (#458 U2),
 *        each game's section as that game's own icon grid (#458 U3).
 *
 * See ComboItemTrackerWindow.h for the contract. The file keeps no state of its
 * own (bar one flag, the Move to Default Position request, consumed by the next draw):
 * every section is collected from combo_item_view.h on every frame, so a
 * game switch, a save or a load shows on the next frame; the numbers carried
 * between frames (each section's measured header height, the fit last drawn)
 * live in the overlay window's ImGui state storage. src/common cannot include
 * UIWidgets, so the headers, notes, icons and counts go through the combo_ui seam.
 */

#include "ComboItemTrackerWindow.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>
#include <nlohmann/json.hpp>
#include <fast/resource/ResourceType.h>
#include <ship/Context.h>
#include <ship/config/Config.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "ComboTrackerWindow.h" // ComboTrackerComboButtonMask, ComboTrackerHeldPadButtons: SoH's hold (#1141)
#include "combo_ui.h"
#include "context.h" // GameId

namespace ComboGui {

namespace {

const ComboUiTable& Ui() {
    return *ComboUi_Get();
}

// Set by ComboItemTrackerResetPosition, consumed by the next floating draw (UI thread only).
bool gResetPositionRequested = false;

// Active customizable groups & layout across the 3 overlay columns.
ComboItemTrackerLayout gCustomLayout;

void CollectCustomSectionRows(int section, int sharedIconGame, std::vector<ComboItemRow>& out);

// SoH's default overlay background (BeginFloatingWindows reads BgColor.Value
// with { 0, 0, 0, 0 }): clear, so only the icons show over the game.
constexpr Color_RGBA8 kBgDefault = { 0, 0, 0, 0 };

/**
 * BeginFloatingWindows (randomizer_item_tracker.cpp), statement for statement
 * where it applies to one overlay: the flags, the main viewport while floating,
 * the background colour (opaque while docked in the menu's deck, as SoH's is),
 * a transparent border and rounding 4, all from SoH's keys (`keys`, `chrome`).
 * The overlay's Opacity is one more alpha over all of it. Pushes four style
 * entries; EndOverlay pops them.
 */
void BeginOverlay(const ComboItemTrackerKeys& keys, const ComboItemTrackerChrome& chrome) {
    const int windowType = keys.windowType;
    const ImGuiWindowFlags flags = (ImGuiWindowFlags)chrome.flags;
    if (chrome.mainViewport) {
        const ImGuiViewport* main = ImGui::GetMainViewport();
        ImGui::SetNextWindowViewport(main->ID);
        // Where it first appears (#962): under the HUD's hearts, not on them.
        // FirstUseEver leaves a saved or dragged position alone; Move to
        // Default Position (ComboItemTrackerResetPosition) forces the default
        // for one frame.
        ImVec2 pos;
        ComboItemTrackerDefaultPos(main->Pos.x, main->Pos.y, main->Size.x, main->Size.y, &pos.x, &pos.y);
        ImGui::SetNextWindowPos(pos, gResetPositionRequested ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
        gResetPositionRequested = false;
    }
    ImVec4 color(keys.bg[0] / 255.0f, keys.bg[1] / 255.0f, keys.bg[2] / 255.0f, keys.bg[3] / 255.0f);
    ImGuiWindow* window = ImGui::FindWindowByName(kComboItemTrackerWindowName);
    // The two additions to SoH's chrome. The floating overlay is fitted to the
    // game window (ComboItemTrackerFitBoxes); should even the smallest scale
    // not fit, it is held to the game window's bottom edge rather than drawn
    // past it. The window type's title bar is kept whole: ImGui's auto-fit
    // sizes to the contents alone, and a short state (headers with no rows)
    // would cut the title.
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
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * keys.opacity);
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
 * A cell's count, under SoH's count settings (#1143: ComboItemCountFor). SoH's
 * DrawItemCount draws it in the Item Tracker's own font (the seam's
 * PushCountFont: SoH's 16 px mono face) at its Text size's scale
 * (ComboItemCountTextScale, 1 at 13); without that font it is the window's
 * font at kComboItemTrackerCountPx (`countScale`). MM's DrawItemCounts draws it
 * in the window's font at the tracker's scale, at the same Text size. Where it
 * sits is its game's (ComboItemCountPos). `cellMin` and `cell` are the cell's
 * screen position and edge.
 */
void DrawCount(const ComboItemRow& row, const ComboItemTrackerKeys& keys, uint8_t countStyle, ImVec2 cellMin,
               float cell, float scale, float countScale) {
    ComboItemCount count;
    if (!ComboItemCountFor(row, keys, count)) {
        return;
    }
    const bool sohFont = countStyle != COMBO_ITEM_COUNT_MM && Ui().PushCountFont();
    const float textScale = ComboItemCountTextScale(keys.textSize);
    ImGui::SetWindowFontScale(((sohFont || countStyle == COMBO_ITEM_COUNT_MM) ? scale : countScale) * textScale);
    const float amountWidth = ImGui::CalcTextSize(count.amount.c_str()).x;
    const float width = amountWidth + ImGui::CalcTextSize(count.ceiling.c_str()).x;
    const float height = ImGui::GetFontSize();
    ImVec2 at;
    ComboItemCountPos(countStyle, count.alignLeft, cellMin.x, cellMin.y, cell, width, height, scale, &at.x, &at.y);
    ImGui::SetCursorScreenPos(at);
    Ui().ToneText(count.amount.c_str(), count.amountTone);
    if (!count.ceiling.empty()) {
        ImGui::SetCursorScreenPos(ImVec2(at.x + amountWidth, at.y));
        Ui().ToneText(count.ceiling.c_str(), count.ceilingTone);
    }
    if (sohFont) {
        Ui().PopCountFont();
    }
    ImGui::SetWindowFontScale(scale);
}

/**
 * SoH's Hookshot Identifier (#1143): the hookshot's "H" or "L" on its icon, in
 * the count's font at the icon's size over 36 (DrawItemCount's
 * textScalingFactor), placed by ComboItemIdentifierPos. `unit` is that scale
 * times the overlay's.
 */
void DrawIdentifier(const ComboItemRow& row, const ComboItemTrackerKeys& keys, uint8_t countStyle, ImVec2 cellMin,
                    float cell, float unit, float scale) {
    const char* letter = ComboItemIdentifierFor(row, keys);
    if (letter == nullptr) {
        return;
    }
    const bool sohFont = countStyle != COMBO_ITEM_COUNT_MM && Ui().PushCountFont();
    ImGui::SetWindowFontScale(unit);
    ImVec2 at;
    ComboItemIdentifierPos(cellMin.x, cellMin.y, cell, ImGui::CalcTextSize(letter).x, unit, &at.x, &at.y);
    ImGui::SetCursorScreenPos(at);
    Ui().ToneText(letter, COMBO_UI_TONE_WHITE);
    if (sohFont) {
        Ui().PopCountFont();
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

/**
 * An icon section's grid at `origin`, as its game's own tracker draws it, at
 * SoH's Icon size and Icon margins (#1143: ComboItemGridMetricsFor).
 */
void DrawIconGrid(const ComboItemTrackerSection& section, const SectionPlan& plan, ImVec2 origin,
                  const ComboItemTrackerKeys& keys, float scale, float countScale) {
    const ComboItemGridStyle& grid = *section.grid;
    const float iconSize = (float)keys.iconSize;
    const float iconSpacing = (float)keys.iconSpacing;
    const ComboItemGridMetrics metrics = ComboItemGridMetricsFor(grid, iconSize, iconSpacing);
    const float unit = ComboItemIconUnit(iconSize) * scale;
    const float cell = metrics.cell * scale;
    const float pitch = metrics.pitch * scale;
    const float linePitch = metrics.linePitch * scale;
    const float baseAlpha = ImGui::GetStyle().Alpha;
    for (size_t i = 0; i < section.rows.size() && i < plan.cells.size(); i++) {
        const ComboItemRow& row = section.rows[i];
        if (row.pad) {
            continue; // a blank cell, as SoH's ITEM_NONE (#1141)
        }
        const ImVec2 cellMin(origin.x + (float)plan.cells[i].column * pitch,
                             origin.y + (float)plan.cells[i].line * linePitch);
        const ComboItemIconPick pick = ComboItemPickIcon(row);
        // Held to the cell (#1141): MM's wide owl face no longer runs into its neighbour.
        const ComboItemIconRect icon = ComboItemIconFit(cell, row.iconAspect);
        ImGui::SetCursorScreenPos(ImVec2(cellMin.x + icon.x, cellMin.y + icon.y));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, baseAlpha * pick.alpha);
        const bool drawn = Ui().Image(pick.key, icon.w, icon.h, row.iconTint);
        ImGui::PopStyleVar();
        if (!drawn) {
            DrawCellText(row, cellMin, cell);
        }
        // The item's name on hover, as both native trackers do (SoH's
        // Tooltip(SohUtils::GetItemName(item.id)); MM's GetItemTrackerItemName).
        Ui().Tooltip(row.name);
        DrawCount(row, keys, grid.countStyle, cellMin, cell, scale, countScale);
        DrawIdentifier(row, keys, grid.countStyle, cellMin, cell, unit, scale);
        if (row.label != nullptr) {
            // SoH's short name over the icon (DrawItem), in white, at the Text size DrawItemCount left set.
            ImGui::SetWindowFontScale(scale * ComboItemCountTextScale(keys.textSize));
            ImVec2 at;
            ComboItemLabelPos(cellMin.x, cellMin.y, cell, ImGui::CalcTextSize(row.label).x, scale, &at.x, &at.y);
            ImGui::SetCursorScreenPos(at);
            Ui().ToneText(row.label, COMBO_UI_TONE_WHITE);
            ImGui::SetWindowFontScale(scale);
        }
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
        if (row.pad) {
            continue; // a blank cell (#1141)
        }
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

const char* const kComboItemTrackerSohSectionGroups[kComboItemTrackerSohSectionCount] = {
    "Inventory", "Equipment", "Misc", "Dungeon Rewards", "Songs",
};

// #1142: OoT's adapter's group strings (ItemAdapterSingleExe.cpp kGatedGroups and its dungeon tables).
const char* const kComboItemTrackerSohOptionalGroups[kComboItemTrackerSohOptionalCount] = {
    "Dungeon Items", "Greg",        "Fishing Pole",    "Bean Souls",
    "Boss Souls",    "Jabber Nuts", "Ocarina Buttons", "Overworld Keys",
};

ComboItemTrackerKeys ComboReadItemTrackerKeys() {
    // SoH's keys and defaults (randomizer_item_tracker.cpp: CVAR_TRACKER_ITEM,
    // read by ItemTrackerWindow::DrawElement, BeginFloatingWindows,
    // DrawItemsInRows and DrawItemCount, written by ItemTrackerSettingsWindow).
    static const char* const kSectionDisplayKeys[kComboItemTrackerSohSectionCount] = {
        "gTrackers.ItemTracker.DisplayType.Inventory", "gTrackers.ItemTracker.DisplayType.Equipment",
        "gTrackers.ItemTracker.DisplayType.Misc",      "gTrackers.ItemTracker.DisplayType.DungeonRewards",
        "gTrackers.ItemTracker.DisplayType.Songs",
    };
    ComboItemTrackerKeys keys;
    keys.windowType = CVarGetInteger("gTrackers.ItemTracker.WindowType", COMBO_ITEM_TRACKER_FLOATING);
    keys.draggable = CVarGetInteger("gTrackers.ItemTracker.Draggable", 0) != 0;
    keys.showOnlyPaused = CVarGetInteger("gTrackers.ItemTracker.ShowOnlyPaused", 0) != 0;
    keys.displayMode = CVarGetInteger("gTrackers.ItemTracker.DisplayType.Main", kComboItemTrackerDisplayAlways);
    keys.comboButton1 = CVarGetInteger("gTrackers.ItemTracker.ComboButton1", kComboItemTrackerComboButtonL);
    keys.comboButton2 = CVarGetInteger("gTrackers.ItemTracker.ComboButton2", kComboItemTrackerComboButtonR);
    const Color_RGBA8 bg = CVarGetColor("gTrackers.ItemTracker.BgColor.Value", kBgDefault);
    keys.bg[0] = bg.r;
    keys.bg[1] = bg.g;
    keys.bg[2] = bg.b;
    keys.bg[3] = bg.a;
    // SoH's Icon size slider bounds (25..128).
    const int iconSize = CVarGetInteger("gTrackers.ItemTracker.IconSize", kComboItemTrackerIconSize);
    keys.iconSize = iconSize < kComboItemTrackerIconSizeMin   ? kComboItemTrackerIconSizeMin
                    : iconSize > kComboItemTrackerIconSizeMax ? kComboItemTrackerIconSizeMax
                                                              : iconSize;
    // SoH's Icon margins (-5..50) and Text size (1..30) slider bounds (#1143).
    const int iconSpacing = CVarGetInteger("gTrackers.ItemTracker.IconSpacing", kComboItemTrackerIconSpacing);
    keys.iconSpacing = iconSpacing < kComboItemTrackerIconSpacingMin   ? kComboItemTrackerIconSpacingMin
                       : iconSpacing > kComboItemTrackerIconSpacingMax ? kComboItemTrackerIconSpacingMax
                                                                       : iconSpacing;
    const int textSize = CVarGetInteger("gTrackers.ItemTracker.TextSize", kComboItemTrackerTextSize);
    keys.textSize = textSize < kComboItemTrackerTextSizeMin   ? kComboItemTrackerTextSizeMin
                    : textSize > kComboItemTrackerTextSizeMax ? kComboItemTrackerTextSizeMax
                                                              : textSize;
    keys.itemCountType = CVarGetInteger("gTrackers.ItemTracker.ItemCountType", kComboItemTrackerCountCapacityOnly);
    keys.itemCountAlignLeft = CVarGetInteger("gTrackers.ItemTracker.ItemCountAlignLeft", 0) != 0;
    keys.keyCounts = CVarGetInteger("gTrackers.ItemTracker.KeyCounts", kComboItemTrackerKeysCollectedMax);
    keys.triforcePieceCounts =
        CVarGetInteger("gTrackers.ItemTracker.TriforcePieceCounts", kComboItemTrackerTriforceRequiredMax);
    keys.hookshotIdentifier = CVarGetInteger("gTrackers.ItemTracker.HookshotIdentifier", 0) != 0;
    keys.dungeonItemsDisplayMaps = CVarGetInteger("gTrackers.ItemTracker.DungeonItems.DisplayMaps", 1) != 0;
    for (int i = 0; i < kComboItemTrackerSohSectionCount; i++) {
        keys.sectionDisplay[i] = CVarGetInteger(kSectionDisplayKeys[i], kComboItemTrackerSectionMainWindow);
    }
    static const char* const kOptionalDisplayKeys[kComboItemTrackerSohOptionalCount] = {
        "gTrackers.ItemTracker.DisplayType.DungeonItems",   "gTrackers.ItemTracker.DisplayType.Greg",
        "gTrackers.ItemTracker.DisplayType.FishingPole",    "gTrackers.ItemTracker.DisplayType.BeanSouls",
        "gTrackers.ItemTracker.DisplayType.BossSouls",      "gTrackers.ItemTracker.DisplayType.JabberNuts",
        "gTrackers.ItemTracker.DisplayType.OcarinaButtons", "gTrackers.ItemTracker.DisplayType.OverworldKeys",
    };
    for (int i = 0; i < kComboItemTrackerSohOptionalCount; i++) {
        keys.optionalDisplay[i] = CVarGetInteger(kOptionalDisplayKeys[i], kComboItemTrackerSectionHidden);
    }
    // The overlay's own keys, which SoH's overlay has no row for.
    const float opacity = CVarGetFloat(RSBS_CVAR_COMBO_ITEMS_OPACITY, 1.0f);
    keys.opacity = opacity < 0.1f ? 0.1f : opacity > 1.0f ? 1.0f : opacity;
    keys.sectionShown[COMBO_ITEM_SECTION_OOT] = CVarGetInteger(RSBS_CVAR_COMBO_ITEMS_SECTION_OOT, 1) != 0;
    keys.sectionShown[COMBO_ITEM_SECTION_MM] = CVarGetInteger(RSBS_CVAR_COMBO_ITEMS_SECTION_MM, 1) != 0;
    keys.sectionShown[COMBO_ITEM_SECTION_SHARED] = CVarGetInteger(RSBS_CVAR_COMBO_ITEMS_SECTION_SHARED, 1) != 0;
    return keys;
}

bool ComboItemTrackerSohGroupShown(const ComboItemTrackerKeys& keys, const char* group) {
    if (group == nullptr) {
        return true;
    }
    for (int i = 0; i < kComboItemTrackerSohSectionCount; i++) {
        if (strcmp(kComboItemTrackerSohSectionGroups[i], group) == 0) {
            return keys.sectionDisplay[i] != kComboItemTrackerSectionHidden;
        }
    }
    // #1142: SoH's optional sections, hidden by default; one of the two dungeon tables, by DisplayMaps.
    const bool keysOnly = strcmp(group, "Dungeon Keys") == 0;
    for (int i = 0; i < kComboItemTrackerSohOptionalCount; i++) {
        if (strcmp(kComboItemTrackerSohOptionalGroups[i], keysOnly ? "Dungeon Items" : group) == 0) {
            const bool table = i != 0 || keys.dungeonItemsDisplayMaps != keysOnly;
            return table && keys.optionalDisplay[i] != kComboItemTrackerSectionHidden;
        }
    }
    return true;
}

ComboItemTrackerChrome ComboItemTrackerChromeFor(const ComboItemTrackerKeys& keys, bool paused, uint32_t padButtons) {
    ComboItemTrackerChrome chrome;
    const bool floating = keys.windowType == COMBO_ITEM_TRACKER_FLOATING;
    chrome.flags = ComboItemTrackerWindowFlags(keys.windowType, keys.draggable);
    chrome.mainViewport = floating;
    // DrawElement's gate: a window always; floating, Only Enable While Paused,
    // then Combo Button Hold (both of the player's two buttons on the first pad).
    chrome.draws = ComboItemTrackerShows(keys.windowType, keys.showOnlyPaused, paused);
    if (floating && keys.displayMode == kComboItemTrackerDisplayComboButton) {
        const uint32_t button1 = ComboTrackerComboButtonMask(keys.comboButton1);
        const uint32_t button2 = ComboTrackerComboButtonMask(keys.comboButton2);
        const bool held = button1 != 0 && button2 != 0 && (padButtons & button1) != 0 && (padButtons & button2) != 0;
        if (!held) {
            chrome.draws = false;
        }
    }
    return chrome;
}

int ComboItemTrackerCarryRetiredKeys() {
    int carried = 0;
    for (std::size_t i = 0; i < RSBS::kComboRetiredKeyCount; i++) {
        const RSBS::ComboRetiredKey& retired = RSBS::kComboRetiredKeys[i];
        const std::string from = std::string(retired.key) + retired.stored;
        if (!CVarExists(from.c_str())) {
            continue;
        }
        // The twin's value is what the overlay drew, so it wins over SoH's.
        const std::string to = std::string(retired.carriedTo) + retired.stored;
        CVarCopy(from.c_str(), to.c_str());
        CVarClear(from.c_str());
        carried++;
    }
    return carried;
}

ComboItemIconRect ComboItemIconFit(float cell, float aspect) {
    ComboItemIconRect r;
    r.w = cell;
    r.h = cell;
    if (aspect > 0.0f && aspect < 1.0f) {
        r.w = cell * aspect; // a song's note: the cell's height, narrower
    } else if (aspect > 1.0f) {
        r.h = cell / aspect; // MM's owl face: the cell's width, shorter
    }
    r.x = (cell - r.w) * 0.5f;
    r.y = (cell - r.h) * 0.5f;
    return r;
}

void ComboItemLabelPos(float cellX, float cellY, float cell, float textWidth, float scale, float* outX, float* outY) {
    *outX = cellX + cell * 0.5f - textWidth * 0.5f;
    *outY = cellY - 13.0f * scale;
}

void ComboItemTrackerResetPosition() {
    gResetPositionRequested = true;
}

// The magic bar's lowest placement (hearts past one row push it down to the
// large Y): top at R_MAGIC_BAR_LARGE_Y (z_construct.c), drawn 16 px tall
// (gMagicMeterEndTex, z_parameter.c), on the HUD's 320x240 grid.
constexpr float kMagicBarLargeTop = 42.0f;
constexpr float kMagicBarHeight = 16.0f;

ComboItemTrackerRect ComboItemTrackerHudBand(float viewX, float viewY, float viewW, float viewH) {
    (void)viewW;
    const float unit = viewH / 240.0f; // the HUD's grid is 240 high, scaled to the view's height
    ComboItemTrackerRect band;
    band.x = viewX;
    band.y = viewY;
    band.w = 136.0f * unit;
    band.h = (kMagicBarLargeTop + kMagicBarHeight) * unit;
    return band;
}

void ComboItemTrackerDefaultPos(float viewX, float viewY, float viewW, float viewH, float* outX, float* outY) {
    const ComboItemTrackerRect band = ComboItemTrackerHudBand(viewX, viewY, viewW, viewH);
    // Flush to the left edge, as the HUD is, and 4 grid units under the band.
    *outX = viewX + 8.0f;
    *outY = band.y + band.h + 4.0f * (viewH / 240.0f);
}

std::string ComboItemRowText(const ComboItemRow& row) {
    if (row.pad) {
        return std::string(); // a blank cell (#1141)
    }
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
        out.note = Combo_ItemSharedLabel();
        out.grid = &kComboItemSharedGrid;
        if (gCustomLayout.customEnabled[section]) {
            if (out.freshness != COMBO_TRACKER_FRESH_UNAVAILABLE) {
                CollectCustomSectionRows(section, sharedIconGame, out.rows);
            }
            return;
        }
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
    out.note = Combo_ItemFreshnessLabel(game, out.freshness);
    out.grid = Combo_ItemGridStyle(game);
    if (gCustomLayout.customEnabled[section]) {
        if (out.freshness != COMBO_TRACKER_FRESH_UNAVAILABLE) {
            CollectCustomSectionRows(section, sharedIconGame, out.rows);
        }
        return;
    }
    // #1141: a group the player hid is left out: MM's under its own key, OoT's
    // under SoH's DisplayType.<Section>.
    const ComboItemTrackerKeys keys = ComboReadItemTrackerKeys();
    const int count = Combo_ItemCount(game);
    for (int i = 0; i < count && Combo_ItemRowAt(game, i, &row); i++) {
        if (!Combo_ItemGroupShown(game, row.group)) {
            continue;
        }
        if (game == (uint8_t)GAME_OOT && !ComboItemTrackerSohGroupShown(keys, row.group)) {
            continue;
        }
        out.rows.push_back(row);
    }
}

bool ComboItemTrackerSectionShown(int section) {
    if (section < 0 || section >= COMBO_ITEM_SECTION_COUNT) {
        return false;
    }
    return ComboReadItemTrackerKeys().sectionShown[section];
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
        const bool newGroup = row.customCols > 0 ? (line < 0 || row.customOwnLine)
                                                 : (group == nullptr || row.group == nullptr ||
                                                    strcmp(row.group, group) != 0); // groups are never NULL
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
    bool groupOwnLine = false; // the previous row's group takes lines of its own (#1141)
    for (const ComboItemRow& row : rows) {
        if (row.customCols > 0) {
            if (line < 0 || row.customOwnLine || column >= (int)row.customCols) {
                columns = (int)row.customCols;
                line++;
                column = 0;
            }
            out.push_back({ line, column });
            column++;
            group = row.group;
            groupOwnLine = true;
            continue;
        }
        const bool newGroup = group == nullptr || row.group == nullptr || strcmp(row.group, group) != 0;
        const bool ownLine = grid.groupOwnLine != nullptr && row.group != nullptr && grid.groupOwnLine(row.group);
        if (line < 0 || (grid.groupLines && newGroup) || (newGroup && (ownLine || groupOwnLine))) {
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
        groupOwnLine = ownLine;
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

bool ComboItemCountFor(const ComboItemRow& row, const ComboItemTrackerKeys& keys, ComboItemCount& out) {
    out = ComboItemCount();
    if (row.pad) {
        return false; // a blank cell (#1141)
    }
    if (row.iconNumber < 0) {
        return false; // the icon itself shows the tier
    }
    // DrawItemCount's branches, in its order. A dungeon's small keys first: in
    // every count mode, No Numbers included, and with none collected yet.
    if (row.countKind == COMBO_ITEM_COUNT_KIND_KEYS && row.max > 0) {
        if (keys.keyCounts == kComboItemTrackerKeysCurrentCollectedMax ||
            keys.keyCounts == kComboItemTrackerKeysCurrentMax) {
            out.amount += std::to_string(row.held) + "/";
        }
        if (keys.keyCounts == kComboItemTrackerKeysCollectedMax ||
            keys.keyCounts == kComboItemTrackerKeysCurrentCollectedMax) {
            out.amount += std::to_string(row.count) + "/";
        }
        out.ceiling = std::to_string(row.max);
        return true;
    }
    const int mode = keys.itemCountType;
    if (row.countKind != COMBO_ITEM_COUNT_KIND_TRIFORCE && mode != kComboItemTrackerCountNone) {
        // shouldAlignToLeft: the two single-number modes offer it.
        const bool alignLeft =
            keys.itemCountAlignLeft && mode != kComboItemTrackerCountCapacity && mode != kComboItemTrackerCountAmmo;
        if (row.iconNumber > 0) {
            // A printed capacity (the pool's wallet, whose row counts its tier): that number alone.
            out.amount = std::to_string(row.iconNumber);
            out.alignLeft = alignLeft;
            return true;
        }
        if (!row.have && row.count == 0) {
            return false; // a faded icon carries no number
        }
        if (row.max <= 0) {
            // No capacity to read (the pool's rupees and bombchus): the amount alone.
            if (row.count <= 0) {
                return false;
            }
            out.amount = std::to_string(row.count);
            out.alignLeft = alignLeft;
            return true;
        }
        // The amount and its slash take the amount's tone; the ceiling, and a
        // full amount, the row's accent (red for the skulltula tokens).
        const ComboUiTone accent = row.countAccent == COMBO_ITEM_ACCENT_RED ? COMBO_UI_TONE_RED : COMBO_UI_TONE_GREEN;
        // A capacity that never grows (capacityMax 0: SoH's bombchus, beans,
        // tokens and hearts) shows the amount in every mode.
        const bool showAmmo =
            mode == kComboItemTrackerCountAmmo || mode == kComboItemTrackerCountAmmoOnly || row.capacityMax <= 0;
        const bool showMax = mode != kComboItemTrackerCountCapacityOnly && mode != kComboItemTrackerCountAmmoOnly;
        out.alignLeft = alignLeft;
        out.ceilingTone = accent;
        if (showAmmo) {
            out.amount = std::to_string(row.count);
            out.amountTone = row.count >= row.max ? accent : COMBO_UI_TONE_WHITE;
            if (showMax) {
                out.amount += "/";
                out.ceiling = std::to_string(row.max);
            }
            if (row.count <= 0) {
                out.amountTone = COMBO_UI_TONE_GRAY;
            }
        } else {
            out.amount = std::to_string(row.max);
            if (row.max >= row.capacityMax) {
                out.amountTone = COMBO_UI_TONE_GREEN; // full: the capacity alone
            } else if (showMax) {
                out.amount += "/";
                out.ceiling = std::to_string(row.capacityMax);
            }
        }
        return true;
    }
    // The triforce pieces under a hunt: collected of required (of the total), in every count mode.
    if (row.countKind == COMBO_ITEM_COUNT_KIND_TRIFORCE && row.max > 0) {
        out.amount = std::to_string(row.count) + "/";
        if (keys.triforcePieceCounts == kComboItemTrackerTriforceRequiredMax) {
            out.amount += std::to_string(row.max) + "/";
            out.ceiling = std::to_string(row.capacityMax);
        } else if (keys.triforcePieceCounts == kComboItemTrackerTriforceRequired) {
            out.ceiling = std::to_string(row.max);
        }
        out.amountTone = row.count >= row.max ? COMBO_UI_TONE_GREEN : COMBO_UI_TONE_WHITE;
        return true;
    }
    if (row.countKind == COMBO_ITEM_COUNT_KIND_TRIFORCE && mode != kComboItemTrackerCountNone && row.count > 0) {
        out.amount = std::to_string(row.count); // pieces with no hunt armed: the count alone
        return true;
    }
    return false;
}

float ComboItemCountTextScale(int textSize) {
    return (float)textSize / (float)kComboItemTrackerTextSize;
}

namespace {
// SoH draws a count's top 14 px above the line under the icon, which sits 4 px
// (the item spacing) below the icon: 10 px up from the icon's bottom at its
// 16 px font, whatever the text size. The overlay draws it 0.6 of that font's
// height up (9.6), as it did before Text size was honoured.
constexpr float kCountRise = 0.6f * kComboItemTrackerCountPx;
} // namespace

void ComboItemCountPos(uint8_t countStyle, bool alignLeft, float cellX, float cellY, float cell, float width,
                       float height, float scale, float* outX, float* outY) {
    if (countStyle == COMBO_ITEM_COUNT_MM) {
        // MM's DrawItemCounts: the cell's bottom-right corner, 2 px in; aligned left, the bottom-left one.
        *outX = alignLeft ? cellX + 2.0f * scale : cellX + cell - width - 2.0f * scale;
        *outY = cellY + cell - height - 2.0f * scale;
    } else {
        // SoH's DrawItemCount: centred under the icon; aligned left, at its left edge.
        *outX = alignLeft ? cellX : cellX + (cell - width) * 0.5f;
        *outY = cellY + cell - kCountRise * scale;
    }
}

const char* ComboItemIdentifierFor(const ComboItemRow& row, const ComboItemTrackerKeys& keys) {
    if (!keys.hookshotIdentifier || !row.have || row.pad) {
        return nullptr;
    }
    switch (row.identifier) {
        case 'H':
            return "H";
        case 'L':
            return "L";
        default:
            return nullptr;
    }
}

void ComboItemIdentifierPos(float cellX, float cellY, float cell, float textWidth, float unit, float* outX,
                            float* outY) {
    // DrawItemCount: p.x + iconSize / 2 - width / 2 + 8, p.y - 22, at iconSize / 36, where p is the line
    // under the icon, 4 px of item spacing below it.
    *outX = cellX + cell * 0.5f - textWidth * 0.5f + 8.0f * unit;
    *outY = cellY + cell + 4.0f * unit - 22.0f * unit;
}

ComboItemGridMetrics ComboItemGridMetricsFor(const ComboItemGridStyle& grid, float iconSize, float iconSpacing) {
    const float unit = ComboItemIconUnit(iconSize);
    ComboItemGridMetrics m;
    m.cell = grid.cellPx * unit;
    if (grid.countStyle == COMBO_ITEM_COUNT_MM) {
        // MM's table: its own cell padding at the icon's size, in proportion to the margin (12, SoH's
        // default, keeps MM's own look).
        const float margin = iconSpacing / (float)kComboItemTrackerIconSpacing;
        m.pitch = m.cell + grid.gapPx * unit * margin;
        m.linePitch = m.cell + ComboItemGridLineGap(grid) * unit * margin;
    } else {
        // SoH's DrawItemsInRows: column * (iconSize + iconSpacing), the margin in pixels at any icon size.
        m.pitch = m.cell + iconSpacing;
        m.linePitch = m.cell + iconSpacing;
    }
    return m;
}

bool ComboItemSectionDrawsIcons(const std::vector<ComboItemRow>& rows, bool (*hasImage)(const char*)) {
    if (hasImage == nullptr) {
        return false;
    }
    for (const ComboItemRow& row : rows) {
        const ComboItemIconPick pick = ComboItemPickIcon(row);
        if (!row.pad && pick.key != nullptr && hasImage(pick.key)) {
            return true;
        }
    }
    return false;
}

bool ComboItemKeyIsPath(const char* key) {
    return key != nullptr && std::strncmp(key, "__OTR__", 7) == 0;
}

int ComboItemTrackerLoadIcons(const std::vector<ComboItemRow>& rows, bool (*hasImage)(const char*),
                              bool (*loadImage)(const char*), int epoch, ComboItemIconLoadMemo& memo) {
    if (hasImage == nullptr || loadImage == nullptr) {
        return 0;
    }
    if (memo.epoch != epoch) {
        memo.epoch = epoch;
        memo.missed.clear();
    }
    int loaded = 0;
    for (const ComboItemRow& row : rows) {
        const char* key = ComboItemPickIcon(row).key;
        if (row.pad || !ComboItemKeyIsPath(key) || hasImage(key) || memo.missed.count(key) != 0) {
            continue;
        }
        if (loadImage(key)) {
            loaded++;
        } else {
            memo.missed.insert(key);
        }
    }
    return loaded;
}

bool ComboItemLoadIconByPath(const char* key) {
    if (!ComboItemKeyIsPath(key)) {
        return false;
    }
    auto ctx = Ship::Context::GetInstance();
    auto window = ctx != nullptr ? ctx->GetWindow() : nullptr;
    auto gui = window != nullptr ? window->GetGui() : nullptr;
    auto resources = ctx != nullptr ? ctx->GetResourceManager() : nullptr;
    auto archives = resources != nullptr ? resources->GetArchiveManager() : nullptr;
    if (gui == nullptr || archives == nullptr) {
        return false;
    }
    if (gui->HasTextureByName(key)) {
        return true;
    }
    // A path no mounted archive holds is not asked of the loader: it would only
    // cache the miss. The archive key has no "__OTR__" signature.
    if (!archives->HasFile(std::string(key + 7))) {
        return false;
    }
    // The texture is loaded through the resource manager and handed to the Gui
    // as a parsed Fast::Texture, so a path that is not a texture (or does not
    // parse) is a miss here and not a null dereference inside the Gui's own
    // path overload.
    auto resource = resources->LoadResource(key, true);
    if (resource == nullptr || resource->GetInitData() == nullptr ||
        resource->GetInitData()->Type != static_cast<uint32_t>(Fast::ResourceType::Texture)) {
        return false;
    }
    // The identity tint: the texture as the archive holds it (MM's own loader passes the same).
    constexpr float kNoTint = 1.0f;
    gui->LoadGuiTexture(key, *static_cast<Fast::Texture*>(resource.get()), ImVec4(kNoTint, kNoTint, kNoTint, kNoTint));
    return gui->HasTextureByName(key);
}

// ============================================================================
// The fit
// ============================================================================

float ComboItemGridLineGap(const ComboItemGridStyle& grid) {
    return grid.lineGapPx > 0.0f ? grid.lineGapPx : grid.gapPx;
}

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
    // SoH's Icon size and Icon margins (#1143), as DrawIconGrid lays the cells out.
    const ComboItemGridMetrics m = ComboItemGridMetricsFor(grid, metrics.iconSize, metrics.iconSpacing);
    box.width = (float)(columns - 1) * m.pitch + m.cell;
    box.height = (float)(cells.back().line + 1) * m.linePitch;
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
    // SoH's Item Tracker Settings (#1141, ruling IT-D6): its keys, its floating
    // gates on the active game's pause and the first pad's held buttons (each
    // read only where SoH reads it), its flags.
    const ComboItemTrackerKeys keys = ComboReadItemTrackerKeys();
    const bool floating = keys.windowType == COMBO_ITEM_TRACKER_FLOATING;
    const bool paused = floating && keys.showOnlyPaused && Combo_ItemActiveGamePaused();
    const uint32_t pad =
        floating && keys.displayMode == kComboItemTrackerDisplayComboButton ? ComboTrackerHeldPadButtons() : 0;
    const ComboItemTrackerChrome chrome = ComboItemTrackerChromeFor(keys, paused, pad);
    if (!chrome.draws) {
        return;
    }
    BeginOverlay(keys, chrome);
    DrawElement();
    EndOverlay();
}

void ComboItemTrackerWindow::DrawElement() {
    const ComboItemTrackerKeys keys = ComboReadItemTrackerKeys();
    std::vector<ComboItemTrackerSection> sections;
    for (int s = 0; s < COMBO_ITEM_SECTION_COUNT; s++) {
        if (!keys.sectionShown[s]) {
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
    metrics.iconSize = (float)keys.iconSize;
    metrics.iconSpacing = (float)keys.iconSpacing;
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
        // #963: a game's icons are registered by its own init, and MM's has not
        // run yet when the session starts in Hyrule. Its keys are resource
        // paths, so the ones not loaded are loaded from the archives mounted
        // now. The only state kept here: what could not be loaded since the
        // last game switch, so a miss is one attempt and not one per frame.
        static ComboItemIconLoadMemo iconLoadMemo[COMBO_ITEM_SECTION_COUNT];
        ComboItemTrackerLoadIcons(section.rows, Ui().HasImage, ComboItemLoadIconByPath, (int)Context_GetCurrentGame(),
                                  iconLoadMemo[section.id]);
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
        // A column is never narrower than its header: the title between two
        // padded rules (SeparatorText) with at least two lines' height of rule
        // after it, so a section with no rows still reads as SoH's ruled
        // header, and any note on one line, so the header's height does not
        // change with the column's width.
        const float title =
            ImGui::CalcTextSize(section.title).x + style.SeparatorTextPadding.x * 2.0f + metrics.textHeight * 2.0f;
        const float note = section.note.empty() ? 0.0f : ImGui::CalcTextSize(section.note.c_str()).x + 1.0f;
        const float headerWidth = title > note ? title : note;
        boxes[s].width = boxes[s].width > headerWidth ? boxes[s].width : headerWidth;
        plan.box = boxes[s];
    }

    ComboItemTrackerFit fit;
    if (keys.windowType == COMBO_ITEM_TRACKER_FLOATING) {
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
    // keeps to its own column.
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
        // The scaled contents plus the parts that do not scale, less the
        // table's own cell padding (ImGui adds that around the column).
        const float columnWidth = plans[s].box.width * fit.scale + plans[s].box.fixedWidth - metrics.cellPadding;
        ImGui::TableSetupColumn(sections[s].title, ImGuiTableColumnFlags_WidthFixed, columnWidth);
    }
    ImGui::TableNextRow();
    for (size_t s = 0; s < sections.size(); s++) {
        const ComboItemTrackerSection& section = sections[s];
        ImGui::TableNextColumn();
        const float top = ImGui::GetCursorScreenPos().y;
        Ui().SeparatorText(section.title);
        if (!section.note.empty()) {
            Ui().NoteText(section.note.c_str());
        }
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        // The next frame's fit reads the header's height as drawn.
        storage->SetFloat(headerIds[s], origin.y - top);
        if (section.rows.empty()) {
            continue;
        }
        if (plans[s].icons) {
            DrawIconGrid(section, plans[s], origin, keys, fit.scale, countScale * fit.scale);
        } else {
            DrawTextGrid(section, plans[s], origin, fit.scale, metrics);
        }
    }
    ImGui::EndTable();
}

// ============================================================================
// Customizable Groups & Layout (2Ship-style Group & Slot Editor)
// ============================================================================

namespace {

constexpr const char* kCustomLayoutConfigBlock = "ComboItemTrackerLayout";

ComboItemTrackerSlot MakeSlot(uint8_t origin, const char* key) {
    ComboItemTrackerSlot s;
    if (key == nullptr || std::strcmp(key, "pad") == 0 || key[0] == '\0') {
        s.origin = COMBO_SLOT_PAD;
        s.key = "pad";
    } else {
        s.origin = origin;
        s.key = key;
    }
    return s;
}

ComboItemTrackerGroup MakeMmKeyItemsGroup() {
    ComboItemTrackerGroup g;
    g.name = "Key Items";
    g.section = COMBO_ITEM_SECTION_MM;
    g.columns = 6;
    g.autoHideUnshuffled = false;
    static const char* const kSlots[] = {
        // Woodfall Temple: Remains, Sonata, Boss Key, Small Key, Boss Soul, Stray Fairy
        "remains_odolwa",
        "song_sonata",
        "boss_key_woodfall",
        "small_key_woodfall",
        "boss_soul_odolwa",
        "fairy_woodfall",
        // Snowhead Temple: Remains, Lullaby, Boss Key, Small Key, Boss Soul, Stray Fairy
        "remains_goht",
        "song_lullaby",
        "boss_key_snowhead",
        "small_key_snowhead",
        "boss_soul_goht",
        "fairy_snowhead",
        // Great Bay Temple: Remains, Bossa Nova, Boss Key, Small Key, Boss Soul, Stray Fairy
        "remains_gyorg",
        "song_bossa_nova",
        "boss_key_great_bay",
        "small_key_great_bay",
        "boss_soul_gyorg",
        "fairy_great_bay",
        // Stone Tower Temple: Remains, Elegy, Boss Key, Small Key, Boss Soul, Stray Fairy
        "remains_twinmold",
        "song_elegy",
        "boss_key_stone_tower",
        "small_key_stone_tower",
        "boss_soul_twinmold",
        "fairy_stone_tower",
        // Endgame / Clock Town: Oath to Order, Majora's Soul, Clock Town Stray Fairy
        "song_oath",
        "boss_soul_majora",
        "fairy_clock_town",
    };
    for (const char* k : kSlots) {
        g.slots.push_back(MakeSlot(COMBO_SLOT_MM, k));
    }
    return g;
}

ComboItemTrackerGroup MakeOotKeyItemsGroup() {
    ComboItemTrackerGroup g;
    g.name = "Key Items";
    g.section = COMBO_ITEM_SECTION_OOT;
    g.columns = 5;
    g.autoHideUnshuffled = false;
    static const char* const kSlots[] = {
        // Deku Tree: Emerald, Minuet, Pad, Pad, Gohma's Soul
        "emerald",
        "song_minuet",
        "pad",
        "pad",
        "boss_soul_gohma",
        // Dodongo's Cavern: Ruby, Bolero, Pad, Pad, King Dodongo's Soul
        "ruby",
        "song_bolero",
        "pad",
        "pad",
        "boss_soul_kd",
        // Jabu-Jabu's Belly: Sapphire, Serenade, Pad, Pad, Barinade's Soul
        "sapphire",
        "song_serenade",
        "pad",
        "pad",
        "boss_soul_bari",
        // Forest Temple: Medallion, Minuet, Small Key, Boss Key, Phantom Ganon's Soul
        "medallion_forest",
        "song_minuet",
        "small_key_forest",
        "boss_key_forest",
        "boss_soul_pg",
        // Fire Temple: Medallion, Bolero, Small Key, Boss Key, Volvagia's Soul
        "medallion_fire",
        "song_bolero",
        "small_key_fire",
        "boss_key_fire",
        "boss_soul_volv",
        // Water Temple: Medallion, Serenade, Small Key, Boss Key, Morpha's Soul
        "medallion_water",
        "song_serenade",
        "small_key_water",
        "boss_key_water",
        "boss_soul_morph",
        // Spirit Temple: Medallion, Requiem, Small Key, Boss Key, Twinrova's Soul
        "medallion_spirit",
        "song_requiem",
        "small_key_spirit",
        "boss_key_spirit",
        "boss_soul_twin",
        // Shadow Temple: Medallion, Nocturne, Small Key, Boss Key, Bongo Bongo's Soul
        "medallion_shadow",
        "song_nocturne",
        "small_key_shadow",
        "boss_key_shadow",
        "boss_soul_bongo",
        // Light / Ganon's Castle: Medallion, Prelude, Small Key, Boss Key, Ganon's Soul
        "medallion_light",
        "song_prelude",
        "small_key_ganon",
        "boss_key_ganon",
        "boss_soul_ganon",
        // Mini-Dungeons: BotW, GTG, Thieves' Hideout, Gerudo Card, Light Arrows
        "small_key_botw",
        "small_key_gtg",
        "small_key_hideout",
        "gerudo_card",
        "arrow_light",
    };
    for (const char* k : kSlots) {
        g.slots.push_back(MakeSlot(COMBO_SLOT_OOT, k));
    }
    return g;
}

bool IsGatedGroupName(uint8_t game, const char* groupName) {
    if (groupName == nullptr) {
        return false;
    }
    if (game == (uint8_t)GAME_MM) {
        static const char* const kMmGated[] = {
            "Ocarina Buttons", "Boss Souls", "Enemy Souls", "Owl Statues", "Time", "Misc",
        };
        for (const char* g : kMmGated) {
            if (std::strcmp(groupName, g) == 0) {
                return true;
            }
        }
        return false;
    }
    static const char* const kOotGated[] = {
        "Greg", "Fishing Pole", "Bean Souls", "Boss Souls", "Jabber Nuts", "Ocarina Buttons", "Overworld Keys",
    };
    for (const char* g : kOotGated) {
        if (std::strcmp(groupName, g) == 0) {
            return true;
        }
    }
    return false;
}

void BuildCatalogGroupsForGame(int section, std::vector<ComboItemTrackerGroup>& out, bool includeDungeonTable) {
    const uint8_t game = section == COMBO_ITEM_SECTION_MM ? (uint8_t)GAME_MM : (uint8_t)GAME_OOT;
    const uint8_t origin = section == COMBO_ITEM_SECTION_MM ? (uint8_t)COMBO_SLOT_MM : (uint8_t)COMBO_SLOT_OOT;
    const ComboItemGridStyle* grid = Combo_ItemGridStyle(game);
    const int count = Combo_ItemCatalogCount(game);
    for (int i = 0; i < count; i++) {
        ComboItemRow row{};
        if (!Combo_ItemCatalogRowAt(game, i, &row) || row.group == nullptr || row.slotKey == nullptr) {
            continue;
        }
        if (!includeDungeonTable && game == (uint8_t)GAME_OOT && std::strcmp(row.group, "Dungeon Items") == 0) {
            continue;
        }
        ComboItemTrackerGroup* target = nullptr;
        for (ComboItemTrackerGroup& existing : out) {
            if (existing.name == row.group) {
                target = &existing;
                break;
            }
        }
        if (target == nullptr) {
            ComboItemTrackerGroup g;
            g.name = row.group;
            g.section = section;
            g.columns = (grid != nullptr && grid->groupColumns != nullptr) ? grid->groupColumns(row.group)
                        : (grid != nullptr && grid->columns > 0)           ? grid->columns
                                                                           : 6;
            g.autoHideUnshuffled = IsGatedGroupName(game, row.group);
            out.push_back(std::move(g));
            target = &out.back();
        }
        // Preserve SoH's 24th-cell spacer in "Dungeon Items" so its 6x6 compact table aligns.
        if (game == (uint8_t)GAME_OOT && target->name == "Dungeon Items" && target->slots.size() == 23) {
            target->slots.push_back(MakeSlot(COMBO_SLOT_PAD, "pad"));
        }
        target->slots.push_back(MakeSlot(origin, row.slotKey));
    }
}

struct ActiveSeedSlotCache {
    bool gameLoaded[2] = { false, false };
    std::vector<const char*> gameKeys[2];
    bool sharedLoaded = false;
    std::vector<const char*> sharedKeys;

    const std::vector<const char*>& ForGame(uint8_t game) {
        const int idx = (game == (uint8_t)GAME_MM) ? 1 : 0;
        if (!gameLoaded[idx]) {
            gameLoaded[idx] = true;
            const int count = Combo_ItemCount(game);
            gameKeys[idx].reserve((size_t)(count > 0 ? count : 0));
            ComboItemRow row{};
            for (int i = 0; i < count && Combo_ItemRowAt(game, i, &row); i++) {
                if (row.slotKey != nullptr) {
                    gameKeys[idx].push_back(row.slotKey);
                }
            }
        }
        return gameKeys[idx];
    }

    bool IsGameSlotActive(uint8_t game, const std::string& slotKey) {
        for (const char* k : ForGame(game)) {
            if (slotKey == k) {
                return true;
            }
        }
        return false;
    }

    const std::vector<const char*>& ForShared() {
        if (!sharedLoaded) {
            sharedLoaded = true;
            const int count = Combo_ItemSharedCount();
            sharedKeys.reserve((size_t)(count > 0 ? count : 0));
            ComboItemRow row{};
            for (int i = 0; i < count && Combo_ItemSharedRowAt(i, &row); i++) {
                if (row.slotKey != nullptr) {
                    sharedKeys.push_back(row.slotKey);
                }
            }
        }
        return sharedKeys;
    }

    bool IsSharedSlotActive(const std::string& slotKey) {
        for (const char* k : ForShared()) {
            if (slotKey == k) {
                return true;
            }
        }
        return false;
    }
};

void ResolveGroupRowsWithCache(const ComboItemTrackerGroup& group, int sharedIconGame, bool filterActive,
                               ActiveSeedSlotCache& cache, std::vector<ComboItemRow>& out) {
    if (group.slots.empty()) {
        return;
    }
    if (filterActive && group.autoHideUnshuffled) {
        bool anyGameSlot = false;
        bool anyActiveGameSlot = false;
        for (const ComboItemTrackerSlot& slot : group.slots) {
            if (slot.origin == COMBO_SLOT_OOT || slot.origin == COMBO_SLOT_MM) {
                anyGameSlot = true;
                const uint8_t game = slot.origin == COMBO_SLOT_MM ? (uint8_t)GAME_MM : (uint8_t)GAME_OOT;
                if (cache.IsGameSlotActive(game, slot.key)) {
                    anyActiveGameSlot = true;
                    break;
                }
            }
        }
        if (anyGameSlot && !anyActiveGameSlot) {
            return;
        }
    }

    const int cols = group.columns < kComboItemTrackerGroupColumnsMin   ? kComboItemTrackerGroupColumnsMin
                     : group.columns > kComboItemTrackerGroupColumnsMax ? kComboItemTrackerGroupColumnsMax
                                                                        : group.columns;
    bool firstInGroup = true;
    for (const ComboItemTrackerSlot& slot : group.slots) {
        if (filterActive && group.autoHideUnshuffled && slot.origin == COMBO_SLOT_SHARED) {
            if (!cache.IsSharedSlotActive(slot.key)) {
                continue;
            }
        }
        ComboItemRow row{};
        if (!ComboItemTrackerResolveSlot(slot, sharedIconGame, &row)) {
            continue;
        }
        row.customCols = (uint8_t)cols;
        row.customOwnLine = firstInGroup;
        firstInGroup = false;
        out.push_back(row);
    }
}

void CollectCustomSectionRows(int section, int sharedIconGame, std::vector<ComboItemRow>& out) {
    ActiveSeedSlotCache cache;
    for (int s = 0; s < COMBO_ITEM_SECTION_COUNT; s++) {
        for (const ComboItemTrackerGroup& group : gCustomLayout.activeGroups[s]) {
            if (group.section != section) {
                continue;
            }
            ResolveGroupRowsWithCache(group, sharedIconGame, true, cache, out);
        }
    }
}

} // namespace

void ComboItemTrackerDefaultActiveGroups(int section, std::vector<ComboItemTrackerGroup>& out) {
    out.clear();
    if (section == COMBO_ITEM_SECTION_SHARED) {
        ComboItemTrackerGroup g;
        g.name = COMBO_ITEM_SHARED_GROUP;
        g.section = COMBO_ITEM_SECTION_SHARED;
        g.columns = kComboItemSharedGrid.columns;
        g.autoHideUnshuffled = true;
        const int count = Combo_ItemSharedCatalogCount();
        for (int i = 0; i < count; i++) {
            ComboItemRow row{};
            if (Combo_ItemSharedCatalogRowAt(i, (uint8_t)GAME_OOT, &row) && row.slotKey != nullptr) {
                g.slots.push_back(MakeSlot(COMBO_SLOT_SHARED, row.slotKey));
            }
        }
        out.push_back(std::move(g));
        return;
    }
    if (section == COMBO_ITEM_SECTION_MM || section == COMBO_ITEM_SECTION_OOT) {
        BuildCatalogGroupsForGame(section, out, section == COMBO_ITEM_SECTION_MM);
    }
}

void ComboItemTrackerAvailableGroups(int section, std::vector<ComboItemTrackerGroup>& out) {
    out.clear();
    if (section == COMBO_ITEM_SECTION_MM) {
        out.push_back(MakeMmKeyItemsGroup());
        BuildCatalogGroupsForGame(COMBO_ITEM_SECTION_MM, out, true);
        return;
    }
    if (section == COMBO_ITEM_SECTION_OOT) {
        out.push_back(MakeOotKeyItemsGroup());
        BuildCatalogGroupsForGame(COMBO_ITEM_SECTION_OOT, out, true);
        // Also provide the keys-only compact dungeon group in Available Groups.
        ComboItemTrackerGroup keysOnly;
        keysOnly.name = "Dungeon Keys";
        keysOnly.section = COMBO_ITEM_SECTION_OOT;
        keysOnly.columns = 6;
        keysOnly.autoHideUnshuffled = false;
        static const char* const kKeysOnlySlots[] = {
            "small_key_forest", "small_key_fire",  "small_key_water", "small_key_spirit", "small_key_shadow",
            "small_key_ganon",  "boss_key_forest", "boss_key_fire",   "boss_key_water",   "boss_key_spirit",
            "boss_key_shadow",  "boss_key_ganon",  "small_key_botw",  "small_key_gtg",    "small_key_hideout",
        };
        for (const char* k : kKeysOnlySlots) {
            keysOnly.slots.push_back(MakeSlot(COMBO_SLOT_OOT, k));
        }
        out.push_back(std::move(keysOnly));
        return;
    }
    if (section == COMBO_ITEM_SECTION_SHARED) {
        ComboItemTrackerDefaultActiveGroups(COMBO_ITEM_SECTION_SHARED, out);
        ComboItemTrackerGroup resources;
        resources.name = "Shared Resources & Gear";
        resources.section = COMBO_ITEM_SECTION_SHARED;
        resources.columns = 4;
        resources.autoHideUnshuffled = false;
        static const char* const kResSlots[] = {
            "rupees",   "wallet", "hearts", "heart_pieces", "double_defense", "magic",   "arrows",          "bombs",
            "bombchus", "sticks", "nuts",   "hookshot",     "sword_kokiri",   "bottles", "triforce_pieces",
        };
        for (const char* k : kResSlots) {
            resources.slots.push_back(MakeSlot(COMBO_SLOT_SHARED, k));
        }
        out.push_back(std::move(resources));

        ComboItemTrackerGroup songsAndBtns;
        songsAndBtns.name = "Shared Songs & Buttons";
        songsAndBtns.section = COMBO_ITEM_SECTION_SHARED;
        songsAndBtns.columns = 4;
        songsAndBtns.autoHideUnshuffled = false;
        static const char* const kSongBtnSlots[] = {
            "song_epona",         "song_storms",         "song_time",          "ocarina_btn_a",
            "ocarina_btn_c_down", "ocarina_btn_c_right", "ocarina_btn_c_left", "ocarina_btn_c_up",
        };
        for (const char* k : kSongBtnSlots) {
            songsAndBtns.slots.push_back(MakeSlot(COMBO_SLOT_SHARED, k));
        }
        out.push_back(std::move(songsAndBtns));

        ComboItemTrackerGroup masksAndArrows;
        masksAndArrows.name = "Shared Masks & Arrows";
        masksAndArrows.section = COMBO_ITEM_SECTION_SHARED;
        masksAndArrows.columns = 4;
        masksAndArrows.autoHideUnshuffled = false;
        static const char* const kMaskArrowSlots[] = {
            "lens",      "arrow_fire", "arrow_ice",   "arrow_light", "mask_goron",
            "mask_zora", "mask_bunny", "mask_keaton", "mask_truth",
        };
        for (const char* k : kMaskArrowSlots) {
            masksAndArrows.slots.push_back(MakeSlot(COMBO_SLOT_SHARED, k));
        }
        out.push_back(std::move(masksAndArrows));
    }
}

void ComboItemTrackerApplySolverPreset(int section) {
    if (section == COMBO_ITEM_SECTION_MM) {
        std::vector<ComboItemTrackerGroup> catalog;
        BuildCatalogGroupsForGame(COMBO_ITEM_SECTION_MM, catalog, true);
        auto findCat = [&](const char* name) -> const ComboItemTrackerGroup* {
            for (const auto& g : catalog) {
                if (g.name == name) {
                    return &g;
                }
            }
            return nullptr;
        };

        std::vector<ComboItemTrackerGroup> groups;
        // 1. Time (auto-hidden when Clock Shuffle is off; 6 wide at the top when on, matching 2Ship screenshot)
        if (const auto* timeGrp = findCat("Time")) {
            groups.push_back(*timeGrp);
        }
        // 2. Key Items (Dungeon Solver: 6 wide [Remains, Song, Boss Key, Small Key, Boss Soul, Stray Fairy])
        groups.push_back(MakeMmKeyItemsGroup());
        // 3. Inventory (with non-dungeon Quest upgrades appended, matching the 2Ship screenshot)
        if (const auto* invGrp = findCat("Inventory")) {
            ComboItemTrackerGroup inv = *invGrp;
            inv.columns = 6;
            static const char* const kQuestUpgrades[] = {
                "bombers_notebook", "wallet", "sword", "shield", "magic", "double_defense", "heart_pieces",
            };
            for (const char* k : kQuestUpgrades) {
                inv.slots.push_back(MakeSlot(COMBO_SLOT_MM, k));
            }
            groups.push_back(std::move(inv));
        }
        // 4. Masks
        if (const auto* masksGrp = findCat("Masks")) {
            groups.push_back(*masksGrp);
        }
        // 5. Remaining utility Songs (Time, Healing, Epona, Soaring, Storms)
        {
            ComboItemTrackerGroup songs;
            songs.name = "Songs";
            songs.section = COMBO_ITEM_SECTION_MM;
            songs.columns = 5;
            songs.autoHideUnshuffled = false;
            for (const char* k : { "song_time", "song_healing", "song_epona", "song_soaring", "song_storms" }) {
                songs.slots.push_back(MakeSlot(COMBO_SLOT_MM, k));
            }
            groups.push_back(std::move(songs));
        }
        // 6. Tokens & Remaining Gated Groups (Ocarina Buttons, Owl Statues, Enemy Souls, Misc)
        for (const char* name : { "Tokens", "Ocarina Buttons", "Owl Statues", "Enemy Souls", "Misc" }) {
            if (const auto* g = findCat(name)) {
                groups.push_back(*g);
            }
        }
        gCustomLayout.customEnabled[COMBO_ITEM_SECTION_MM] = true;
        gCustomLayout.activeGroups[COMBO_ITEM_SECTION_MM] = std::move(groups);
        ComboItemTrackerSaveLayoutToConfig();
        return;
    }

    if (section == COMBO_ITEM_SECTION_OOT) {
        std::vector<ComboItemTrackerGroup> catalog;
        BuildCatalogGroupsForGame(COMBO_ITEM_SECTION_OOT, catalog, true);
        auto findCat = [&](const char* name) -> const ComboItemTrackerGroup* {
            for (const auto& g : catalog) {
                if (g.name == name) {
                    return &g;
                }
            }
            return nullptr;
        };

        std::vector<ComboItemTrackerGroup> groups;
        // 1. Key Items (Dungeon Solver: 5 wide [Reward, Warp Song, Small Key, Boss Key, Boss Soul])
        groups.push_back(MakeOotKeyItemsGroup());
        // 2. Inventory
        if (const auto* invGrp = findCat("Inventory")) {
            groups.push_back(*invGrp);
        }
        // 3. Equipment & Misc
        if (const auto* eqGrp = findCat("Equipment")) {
            groups.push_back(*eqGrp);
        }
        if (const auto* miscGrp = findCat("Misc")) {
            groups.push_back(*miscGrp);
        }
        // 4. Non-warp Songs
        {
            ComboItemTrackerGroup songs;
            songs.name = "Songs";
            songs.section = COMBO_ITEM_SECTION_OOT;
            songs.columns = 6;
            songs.autoHideUnshuffled = false;
            for (const char* k :
                 { "song_lullaby", "song_epona", "song_saria", "song_sun", "song_time", "song_storms" }) {
                songs.slots.push_back(MakeSlot(COMBO_SLOT_OOT, k));
            }
            groups.push_back(std::move(songs));
        }
        // 5. Gated Groups (auto-hidden unless shuffled)
        for (const char* name :
             { "Greg", "Fishing Pole", "Bean Souls", "Jabber Nuts", "Ocarina Buttons", "Overworld Keys" }) {
            if (const auto* g = findCat(name)) {
                groups.push_back(*g);
            }
        }
        gCustomLayout.customEnabled[COMBO_ITEM_SECTION_OOT] = true;
        gCustomLayout.activeGroups[COMBO_ITEM_SECTION_OOT] = std::move(groups);
        ComboItemTrackerSaveLayoutToConfig();
    }
}

bool ComboItemTrackerResolveSlot(const ComboItemTrackerSlot& slot, int sharedIconGame, ComboItemRow* out) {
    if (out == nullptr) {
        return false;
    }
    *out = ComboItemRow{};
    if (slot.origin == COMBO_SLOT_PAD || slot.key.empty() || slot.key == "pad") {
        out->name = "";
        out->group = "";
        out->slotKey = "pad";
        out->pad = true;
        return true;
    }
    if (slot.origin == COMBO_SLOT_OOT) {
        return Combo_ItemCatalogRowByKey((uint8_t)GAME_OOT, slot.key.c_str(), out);
    }
    if (slot.origin == COMBO_SLOT_MM) {
        return Combo_ItemCatalogRowByKey((uint8_t)GAME_MM, slot.key.c_str(), out);
    }
    if (slot.origin == COMBO_SLOT_SHARED) {
        const uint8_t iconGame = (sharedIconGame == (int)GAME_OOT || sharedIconGame == (int)GAME_MM)
                                     ? (uint8_t)sharedIconGame
                                     : (uint8_t)Context_GetCurrentGame();
        return Combo_ItemSharedCatalogRowByKey(slot.key.c_str(), iconGame, out);
    }
    return false;
}

void ComboItemTrackerResolveGroupRows(const ComboItemTrackerGroup& group, int sharedIconGame, bool filterActive,
                                      std::vector<ComboItemRow>& out) {
    ActiveSeedSlotCache cache;
    ResolveGroupRowsWithCache(group, sharedIconGame, filterActive, cache, out);
}

const ComboItemTrackerLayout& ComboItemTrackerGetLayout() {
    return gCustomLayout;
}

void ComboItemTrackerSetLayout(const ComboItemTrackerLayout& layout) {
    gCustomLayout = layout;
}

void ComboItemTrackerRestoreDefaultLayout(int section) {
    if (section < 0) {
        gCustomLayout = ComboItemTrackerLayout{};
    } else if (section >= 0 && section < COMBO_ITEM_SECTION_COUNT) {
        gCustomLayout.customEnabled[section] = false;
        gCustomLayout.activeGroups[section].clear();
    }
    ComboItemTrackerSaveLayoutToConfig();
}

std::string ComboItemTrackerLayoutToJson(const ComboItemTrackerLayout& layout) {
    nlohmann::json root = nlohmann::json::object();
    root["version"] = 1;
    nlohmann::json enabledArr = nlohmann::json::array();
    nlohmann::json sectionsArr = nlohmann::json::array();
    for (int s = 0; s < COMBO_ITEM_SECTION_COUNT; s++) {
        enabledArr.push_back(layout.customEnabled[s]);
        nlohmann::json groupsArr = nlohmann::json::array();
        for (const ComboItemTrackerGroup& g : layout.activeGroups[s]) {
            nlohmann::json gj = nlohmann::json::object();
            gj["name"] = g.name;
            gj["section"] = g.section;
            gj["columns"] = g.columns;
            gj["autoHideUnshuffled"] = g.autoHideUnshuffled;
            nlohmann::json slotsArr = nlohmann::json::array();
            for (const ComboItemTrackerSlot& slot : g.slots) {
                nlohmann::json sj = nlohmann::json::object();
                sj["origin"] = slot.origin;
                sj["key"] = slot.key;
                slotsArr.push_back(std::move(sj));
            }
            gj["slots"] = std::move(slotsArr);
            groupsArr.push_back(std::move(gj));
        }
        sectionsArr.push_back(std::move(groupsArr));
    }
    root["customEnabled"] = std::move(enabledArr);
    root["activeGroups"] = std::move(sectionsArr);
    return root.dump();
}

bool ComboItemTrackerLayoutFromJson(const std::string& jsonStr, ComboItemTrackerLayout& out) {
    if (jsonStr.empty()) {
        return false;
    }
    const nlohmann::json root = nlohmann::json::parse(jsonStr, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        return false;
    }
    ComboItemTrackerLayout parsed{};
    if (root.contains("customEnabled") && root["customEnabled"].is_array()) {
        const auto& arr = root["customEnabled"];
        for (int s = 0; s < COMBO_ITEM_SECTION_COUNT && (size_t)s < arr.size(); s++) {
            if (arr[(size_t)s].is_boolean()) {
                parsed.customEnabled[s] = arr[(size_t)s].get<bool>();
            }
        }
    }
    if (root.contains("activeGroups") && root["activeGroups"].is_array()) {
        const auto& secArr = root["activeGroups"];
        for (int s = 0; s < COMBO_ITEM_SECTION_COUNT && (size_t)s < secArr.size(); s++) {
            if (!secArr[(size_t)s].is_array()) {
                continue;
            }
            for (const auto& gj : secArr[(size_t)s]) {
                if (!gj.is_object()) {
                    continue;
                }
                ComboItemTrackerGroup g;
                g.name = gj.value("name", std::string("Group"));
                g.section = std::clamp(gj.value("section", s), 0, COMBO_ITEM_SECTION_COUNT - 1);
                g.columns = std::clamp(gj.value("columns", 6), kComboItemTrackerGroupColumnsMin,
                                       kComboItemTrackerGroupColumnsMax);
                g.autoHideUnshuffled = gj.value("autoHideUnshuffled", false);
                if (gj.contains("slots") && gj["slots"].is_array()) {
                    for (const auto& sj : gj["slots"]) {
                        if (!sj.is_object()) {
                            continue;
                        }
                        ComboItemTrackerSlot slot;
                        slot.origin = (uint8_t)std::clamp(sj.value("origin", 0), 0, 3);
                        slot.key = sj.value("key", std::string("pad"));
                        g.slots.push_back(std::move(slot));
                    }
                }
                parsed.activeGroups[s].push_back(std::move(g));
            }
        }
    }
    out = std::move(parsed);
    return true;
}

bool ComboItemTrackerLoadLayoutFromConfig() {
    auto ctx = Ship::Context::GetInstance();
    auto cfg = ctx != nullptr ? ctx->GetConfig() : nullptr;
    if (cfg == nullptr) {
        return false;
    }
    const auto& nested = cfg->GetNestedJson();
    if (!nested.is_object() || !nested.contains(kCustomLayoutConfigBlock)) {
        return false;
    }
    const auto& block = nested[kCustomLayoutConfigBlock];
    if (!block.is_object()) {
        return false;
    }
    ComboItemTrackerLayout loaded;
    if (!ComboItemTrackerLayoutFromJson(block.dump(), loaded)) {
        return false;
    }
    gCustomLayout = std::move(loaded);
    return true;
}

bool ComboItemTrackerSaveLayoutToConfig() {
    auto ctx = Ship::Context::GetInstance();
    auto cfg = ctx != nullptr ? ctx->GetConfig() : nullptr;
    if (cfg == nullptr) {
        return false;
    }
    const nlohmann::json block = nlohmann::json::parse(ComboItemTrackerLayoutToJson(gCustomLayout), nullptr, false);
    if (block.is_discarded()) {
        return false;
    }
    cfg->SetBlock(kCustomLayoutConfigBlock, block);
    cfg->Save();
    return true;
}

void RegisterComboItemTrackerWindow(std::shared_ptr<Ship::Gui> gui) {
    if (gui == nullptr) {
        return;
    }
    // #1141: a value saved under a retired twin moves into SoH's key before the
    // overlay first reads it, and the config is saved once it has.
    if (ComboItemTrackerCarryRetiredKeys() > 0) {
        gui->SaveConsoleVariablesNextFrame();
    }
    ComboItemTrackerLoadLayoutFromConfig();
    // Idempotence, the #457 guard (AddGuiWindow rejects duplicates silently).
    if (gui->GetGuiWindow(kComboItemTrackerWindowName) != nullptr) {
        return;
    }
    gui->AddGuiWindow(
        std::make_shared<ComboItemTrackerWindow>(kComboItemTrackerVisibilityCVar, kComboItemTrackerWindowName));
}

} // namespace ComboGui
