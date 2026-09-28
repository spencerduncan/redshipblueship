/**
 * @file SohMenuComboSaveFiles.cpp
 * @brief Combo > Save Files: each save file's cross-game state, read-only, as a
 *        CONTRIBUTED page in the tier-4 Combo section.
 *
 * WHAT IT REPLACES. ADR 0004 section 4 gave the Combo section a save-slots page
 * that "absorbs ComboMenuBar's .redsave file-select panel". ComboMenuBar was
 * compiled but never constructed, so that panel never reached a player; it was
 * deleted on 2026-09-28 and this page keeps the one thing it showed that no other
 * surface shows: whether each file will pair, and if not, why, before the player
 * loads it. The model is src/common/combo_save_files_view.h, whose header says
 * which refusals have a toast and which have only this page.
 *
 * WHAT IT LEAVES OUT. The panel's Load, Delete and Save-to-slot buttons. Ocarina
 * of Time's file select already loads, creates and erases these files (its erase
 * reaches RsbsSave_DeleteSave through OnDeleteFile, which also frees a refused
 * slot), and a second way to erase a file would need its own confirm. The note
 * sends the player there.
 *
 * THE LOOK is SoH's bordered table (SohMenuRandomizer.cpp's DrawLocationsMenu:
 * 8x8 cell padding, BordersH | BordersV, TableSetupColumn + TableHeadersRow with
 * the header row's items disabled), under a gray note and a separator, one
 * column, like Combo > MM Mods. UiSnapshot compares it with Randomizer >
 * Tricks/Glitches, SoH's captured table page.
 *
 * Locked by ComboSaveFilesView (the model's words for every state and reason) and
 * drawn by UiSnapshot (Combo > Save Files, states "" and "listed", authored
 * through Combo_SaveFiles_SetMetaForTest).
 */

#include "SohMenu.h"
#include "soh/OTRGlobals.h"
#include "soh/SohGui/SohGui.hpp"

// src/common only (ADR 0002 / ADR 0008 rule 5): the model.
#include "combo_save_files_view.h"

#include <string>

namespace SohGui {

using namespace UIWidgets;

namespace {

/** The sidebar label. Selection persists by display name, so this is the one
 *  spelling; UiSnapshot's reference table (soh_ui_snapshot.cpp, kCompare) repeats
 *  it and fails when no registered Combo page carries it. */
constexpr const char* kSaveFilesPage = "Save Files";

/** The note row's text, rewritten by its PreFunc every frame. */
std::string sSaveFilesNote;

void SaveFilesNotePreFunc(WidgetInfo& info) {
    ComboSaveFileRow rows[RSBS_SAVE_MAX_SLOTS];
    const int count = Combo_SaveFiles_Rows(rows, RSBS_SAVE_MAX_SLOTS);
    sSaveFilesNote = Combo_SaveFiles_Note(rows, count);
    info.name = sSaveFilesNote;
}

void DrawSaveFileList(WidgetInfo& info) {
    ComboSaveFileRow rows[RSBS_SAVE_MAX_SLOTS];
    const int count = Combo_SaveFiles_Rows(rows, RSBS_SAVE_MAX_SLOTS);

    static ImVec2 cellPadding(8.0f, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, cellPadding);
    if (ImGui::BeginTable("tableSaveFiles", 5, ImGuiTableFlags_BordersH | ImGuiTableFlags_BordersV)) {
        ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch, 60.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 150.0f);
        ImGui::TableSetupColumn("Started", ImGuiTableColumnFlags_WidthStretch, 90.0f);
        ImGui::TableSetupColumn("Last Played", ImGuiTableColumnFlags_WidthStretch, 90.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 240.0f);
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::TableHeadersRow();
        ImGui::PopItemFlag();

        for (int i = 0; i < count; i++) {
            const ComboSaveFileRow& row = rows[i];
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", row.file.c_str());
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", row.name.c_str());
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", row.started.c_str());
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", row.lastPlayed.c_str());
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", row.status.c_str());
            if (!row.tooltip.empty()) {
                Tooltip(row.tooltip.c_str());
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

} // namespace

/**
 * The contributed page's registrar. One column: the gray state note, the
 * section and the table.
 */
void AddSaveFilesWidgets(SohMenu& menu, WidgetPath& path) {
    path.column = SECTION_COLUMN_1;
    // Which of the three cases the files are in (none yet, all pair, one does
    // not). Rewritten every frame, so kept out of search.
    menu.AddWidget(path, "Save Files Status", WIDGET_TEXT)
        .RaceDisable(false)
        .HideInSearch(true)
        .PreFunc(SaveFilesNotePreFunc)
        .Options(TextOptions().Color(Colors::Gray));
    menu.AddWidget(path, "File Status", WIDGET_SEPARATOR_TEXT);
    menu.AddWidget(path, "Save File List", WIDGET_CUSTOM).RaceDisable(false).CustomFunction(DrawSaveFileList);
}

/** File-scope, so it runs before AddMenuElements() (SohMenu.h's contract); this
 *  TU is under games/oot/soh, whose archives are WHOLE_ARCHIVE'd, so the
 *  registrar is not elided. The page always holds its note, section and table,
 *  so it is never #640's empty page. */
static RegisterComboSectionPage_t sSaveFilesPage(kSaveFilesPage, 1, AddSaveFilesWidgets);

} // namespace SohGui
