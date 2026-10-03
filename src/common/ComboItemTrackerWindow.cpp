/**
 * @file ComboItemTrackerWindow.cpp
 * @brief Renders the unified item view as SoH's Item Tracker overlay (#458 U2).
 *
 * See ComboItemTrackerWindow.h for the contract. The file keeps no state: every
 * section is collected from combo_item_view.h on every frame, so a game switch,
 * a save or a load shows on the next frame. src/common cannot include
 * UIWidgets, so the section headers and notes go through the combo_ui seam.
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
    // overlay runs taller than SoH's icon grid, so it is held to the game
    // window's bottom edge rather than drawn past it. The window type's title
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

/** One section: its header, its freshness note, and its grid of rows. */
void DrawSection(const ComboItemTrackerSection& section) {
    Ui().SeparatorText(section.title);
    Ui().NoteText(section.note.c_str());
    if (section.rows.empty()) {
        return;
    }
    std::vector<ComboItemGridCell> cells;
    ComboItemGridLayout(section.rows, kComboItemTrackerColumns, cells);
    ImGui::PushID(section.id);
    if (ImGui::BeginTable("##items", kComboItemTrackerColumns, ImGuiTableFlags_SizingFixedFit)) {
        for (size_t i = 0; i < section.rows.size(); i++) {
            const ComboItemRow& row = section.rows[i];
            if (cells[i].column == 0) {
                ImGui::TableNextRow();
            }
            ImGui::TableSetColumnIndex(cells[i].column);
            // A row not held is dimmed the way a disabled row is: the text twin
            // of SoH's faded icon, with no colour of its own.
            const std::string text = ComboItemRowText(row);
            ImGui::BeginDisabled(!row.have);
            ImGui::TextUnformatted(text.c_str());
            ImGui::EndDisabled();
        }
        ImGui::EndTable();
    }
    ImGui::PopID();
}

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
    bool first = true;
    for (int s = 0; s < COMBO_ITEM_SECTION_COUNT; s++) {
        if (!ComboItemTrackerSectionShown(s)) {
            continue;
        }
        ComboItemTrackerSection section;
        ComboItemTrackerCollectSection(s, section);
        if (!first) {
            Ui().Spacer(0.0f);
        }
        first = false;
        DrawSection(section);
    }
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
