/**
 * @file ComboSpoilerWindow.cpp
 * @brief Renders the cross-game spoiler model in-game (#496; ADR 0008).
 *
 * See ComboSpoilerWindow.h for the contract. Every value drawn here comes from
 * combo_spoiler_view.h and the combo tracker's crossing rows (which are the
 * same rows); this file holds no state of its own and caches nothing, so a
 * crossing collected mid-session updates on the next frame.
 *
 * It is drawn the way SoH draws its own panes (docs/ui-style-guide.md section
 * 10): the title bar's close button, a gray note for state, and, per
 * direction, the Combo Tracker's DrawCrossingList (a section header, a gray
 * note, SoH's table shape). src/common cannot include UIWidgets, so every
 * styled element goes through the combo_ui seam.
 */

#include "ComboSpoilerWindow.h"

#include <imgui.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "ComboTrackerWindow.h" // DrawCrossingList: the one crossing table both panes draw
#include "combo_spoiler_view.h"
#include "combo_ui.h"
#include "context.h" // GameId

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
        Ui().NoteText("No paired world. This save was not created as a cross-game pair, so neither game holds the "
                      "other's items.");
        return;
    }

    // "Paired Seed", the Combo Tracker's word for the same number.
    ImGui::Text("Paired Seed: %u", (unsigned)summary.sharedRandoSeed);
    Ui().Spacer(0.0f);

    // Both directions, in the one spoiler's order (combo.crossingStore lists
    // ootItemsInMM, then mmItemsInOoT), drawn by the Combo Tracker's own list so
    // the two panes cannot disagree (#755).
    Ui().PushTheme();
    DrawCrossingList((uint8_t)GAME_MM);
    Ui().Spacer(0.0f);
    DrawCrossingList((uint8_t)GAME_OOT);
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
