/**
 * @file ComboSpoilerWindow.cpp
 * @brief Renders the cross-game spoiler model in-game (#496; ADR 0008).
 *
 * See ComboSpoilerWindow.h for the contract. Every value drawn here comes from
 * combo_spoiler_view.h; this file holds no state of its own and caches
 * nothing, so a crossing collected mid-session updates on the next frame.
 */

#include "ComboSpoilerWindow.h"

#include <imgui.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "combo_spoiler_view.h"

namespace ComboGui {

void ComboSpoilerWindow::Draw() {
    // Read the visibility CVar LIVE rather than trusting the ctor-latched
    // IsVisible(). Ship::GuiWindow reads the CVar exactly once, in its ctor,
    // and only ever writes visibility -> CVar afterwards, so a window with no
    // menu row could otherwise never be opened after registration (#489 cause
    // 1; MM's check tracker does the same at CheckTracker.cpp:454-457).
    if (!CVarGetInteger(kComboSpoilerVisibilityCVar, 0)) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(460.0f, 320.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(kComboSpoilerWindowName, nullptr, ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::End();
        return;
    }

    DrawElement();

    ImGui::End();
}

void ComboSpoilerWindow::DrawElement() {
    ComboSpoilerSummary summary;
    Combo_SpoilerPairingSummary(&summary);

    // "Not paired" and "paired with zero crossings" are different facts and
    // must never render the same way — an empty table under a seed header
    // would tell the player this world has no crossings when the truth is that
    // these two worlds were never paired at all (see combo_spoiler_view.h).
    if (!summary.paired) {
        ImGui::TextWrapped("No paired world.");
        ImGui::Spacing();
        ImGui::TextWrapped("This save was not generated as a cross-game pair, so no OoT items were placed into "
                           "MM checks. Generate a randomized OoT world and start a new MM file from it to pair "
                           "them.");
        return;
    }

    ImGui::Text("Seed: %u", (unsigned)summary.sharedRandoSeed);
    ImGui::Text("Settings digest: %08X", (unsigned)summary.sharedRandoSettingsHash);
    ImGui::Separator();

    const int rowCount = Combo_SpoilerRowCount();
    if (rowCount == 0) {
        ImGui::TextWrapped("Paired, but this world hosts no cross-game items.");
        return;
    }

    ImGui::Text("Cross-game items: %d", rowCount);
    ImGui::Spacing();

    if (ImGui::BeginTable("##ComboSpoilerRows", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        // MM check IDS, not names: common code has no MM check-name table and
        // must not acquire one by including an MM header. Resolving these to
        // readable names needs the MM adapter (#458). Labelled as ids so the
        // column is honest about what it is.
        ImGui::TableSetupColumn("MM Check ID", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Hosts", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Collected", ImGuiTableColumnFlags_WidthFixed, 80.0f);
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
            ImGui::TextUnformatted(row.itemName);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(row.redeemed ? "Yes" : "No");
        }

        ImGui::EndTable();
    }
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
