/**
 * @file ComboSpoilerWindow.cpp
 * @brief Renders the cross-game spoiler model in-game (#496; ADR 0008).
 *
 * See ComboSpoilerWindow.h for the contract. Every value drawn here comes from
 * combo_spoiler_view.h; this file holds no state of its own and caches
 * nothing, so a crossing collected mid-session updates on the next frame.
 *
 * It is drawn the way SoH draws its own panes (docs/ui-style-guide.md section
 * 10): the title bar's close button, a gray note for state, a section header,
 * and SoH's table shape (cell padding 8x8, horizontal and vertical borders, a
 * header row). The header row stays in ImGui's table-header gray, as SoH's own
 * tables do (the Check Tracker Settings pane): the theme's Header /
 * HeaderHovered / HeaderActive colours reach a table's header row only while
 * it is hovered or clicked. src/common cannot include UIWidgets, so every styled element goes
 * through the combo_ui seam.
 */

#include "ComboSpoilerWindow.h"

#include <cstdio>

#include <imgui.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "combo_spoiler_view.h"
#include "combo_ui.h"

namespace ComboGui {

namespace {

const ComboUiTable& Ui() {
    return *ComboUi_Get();
}

} // namespace

void ComboSpoilerWindow::Draw() {
    // Read the visibility CVar LIVE rather than trusting the ctor-latched
    // IsVisible(). Ship::GuiWindow reads the CVar exactly once, in its ctor,
    // and only ever writes visibility -> CVar afterwards, so a window with no
    // menu row could otherwise never be opened after registration (#489 cause
    // 1; MM's check tracker does the same at CheckTracker.cpp:454-457).
    if (!CVarGetInteger(kComboSpoilerVisibilityCVar, 0)) {
        return;
    }

    // SoH's pane chrome (docs/ui-style-guide.md section 10): Ship::GuiWindow::Draw
    // passes its visibility to ImGui::Begin, so an SoH pane has a close button.
    // Closing clears the visibility CVar through SetVisibility, which also
    // schedules the save, as a closed SoH pane does.
    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(460.0f, 320.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(kComboSpoilerWindowName, &open, ImGuiWindowFlags_NoFocusOnAppearing)) {
        DrawElement();
    }
    ImGui::End();
    if (!open) {
        SetVisibility(false);
    }
}

void ComboSpoilerWindow::DrawElement() {
    ComboSpoilerSummary summary;
    Combo_SpoilerPairingSummary(&summary);

    // "Not paired" and "paired with zero crossings" are different facts and
    // must never render the same way — an empty table under a seed header
    // would tell the player this world has no crossings when the truth is that
    // these two worlds were never paired at all (see combo_spoiler_view.h).
    if (!summary.paired) {
        Ui().NoteText("No paired world. This save was not created as a cross-game pair, so no Ocarina of Time items "
                      "were placed in Majora's Mask checks.");
        return;
    }

    // "Paired Seed", the Combo Tracker's word for the same number.
    ImGui::Text("Paired Seed: %u", (unsigned)summary.sharedRandoSeed);
    Ui().Spacer(0.0f);

    // A short Title Case header (R-N1) with no count in it (R-N4): the gray
    // note under it says how many and which way, or that there are none.
    const int rowCount = Combo_SpoilerRowCount();
    Ui().SeparatorText("In MM Checks");
    if (rowCount == 0) {
        Ui().NoteText("No Ocarina of Time items were placed in Majora's Mask checks.");
        return;
    }
    char note[96];
    snprintf(note, sizeof(note), "%d Ocarina of Time %s placed in Majora's Mask checks.", rowCount,
             rowCount == 1 ? "item was" : "items were");
    Ui().NoteText(note);

    Ui().PushTheme();
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 8.0f));
    // No ScrollY: the pane scrolls as a whole, as SoH's tables inside a pane do;
    // a scrolling table with no height fills the pane with empty bordered rows.
    if (ImGui::BeginTable("##ComboSpoilerRows", 3, ImGuiTableFlags_BordersH | ImGuiTableFlags_BordersV)) {
        // MM check IDS, not names. The MM tracker adapter can already resolve
        // them (Combo_TrackerCheckName, which the Combo Tracker's placement
        // table uses); switching this column to names is #757, kept out of the
        // UI-parity change. Labelled as ids so the column is honest about what
        // it is.
        ImGui::TableSetupColumn("MM Check ID", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Collected", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        for (int i = 0; i < rowCount; i++) {
            ComboSpoilerRow row;
            if (!Combo_SpoilerRowAt(i, &row)) {
                break;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("0x%04X", (unsigned)row.mmCheckId);
            ImGui::TableNextColumn();
            // itemName is never NULL — the model substitutes a visible
            // placeholder rather than hand "%s" an invalid pointer.
            ImGui::TextWrapped("%s", row.itemName);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(row.redeemed ? "Yes" : "No");
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    Ui().PopTheme();
}

void RegisterComboSpoilerWindow(std::shared_ptr<Ship::Gui> gui) {
    if (gui == nullptr) {
        return;
    }
    // Idempotence, the #457 guard: the production entry point may be reached
    // from more than one bring-up path, and AddGuiWindow rejects duplicates
    // silently rather than loudly.
    if (gui->GetGuiWindow(kComboSpoilerWindowName) != nullptr) {
        return;
    }

    gui->AddGuiWindow(std::make_shared<ComboSpoilerWindow>(kComboSpoilerVisibilityCVar, kComboSpoilerWindowName));
}

} // namespace ComboGui

extern "C" void Combo_SpoilerWindow_Init(void) {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetWindow() == nullptr) {
        // ROM-free unit harness: shared subsystems without a window/Gui.
        return;
    }
    auto gui = ctx->GetWindow()->GetGui();
    if (gui == nullptr) {
        return;
    }
    ComboGui::RegisterComboSpoilerWindow(gui);
}
