/**
 * @file ComboTrackerWindow.cpp
 * @brief Renders the combo tracker model in-game (#458; ADR 0008).
 *
 * See ComboTrackerWindow.h for the contract. Every value drawn here comes
 * from combo_tracker_view.h; this file holds no state of its own and caches
 * nothing, so progress made mid-session updates on the next frame.
 *
 * It is drawn the way SoH draws its own tracker and editor panes
 * (docs/ui-style-guide.md section 10): the title bar's close button, themed
 * collapsing headers (UIWidgets::PushStyleHeader(THEME_COLOR), as
 * CosmeticsEditor.cpp does), gray notes for state, FontAwesome glyphs for a
 * check's status, and SoH's table shape (cell padding 8x8, horizontal and
 * vertical borders, a header row). src/common cannot include UIWidgets, so
 * every styled element goes through the combo_ui seam.
 */

#include "ComboTrackerWindow.h"

#include <cstdio>

#include <imgui.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/IconsFontAwesome4.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "combo_tracker_view.h"
#include "combo_ui.h"
#include "context.h" // GameId

namespace ComboGui {

namespace {

const ComboUiTable& Ui() {
    return *ComboUi_Get();
}

/**
 * A check's status as a glyph, the way SoH's check tracker marks a row with an
 * icon rather than bracketed text: a ticked box when collected, a box with a
 * minus when the player skipped it, an empty box otherwise.
 */
const char* CheckGlyph(const ComboTrackerCheckRow& row) {
    if (row.obtained) {
        return ICON_FA_CHECK_SQUARE_O;
    }
    return row.skipped ? ICON_FA_MINUS_SQUARE_O : ICON_FA_SQUARE_O;
}

/**
 * One game's panel: freshness note, summary lines, and a default-closed
 * per-check list. Fed ONLY by that game's adapter through the view — the
 * game argument is the origin tag, and nothing here compares ids across
 * panels (ADR 0002).
 */
void DrawGamePanel(uint8_t game, const char* title) {
    if (!ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    ImGui::PushID((int)game);

    ComboTrackerGameSummary summary;
    Combo_TrackerGameSummary(game, &summary);

    if (summary.freshness == COMBO_TRACKER_FRESH_UNAVAILABLE) {
        // "No data" is NOT "zero progress": an MM shadow that was never
        // written and a never-created OoT heap context land here, and an
        // empty check list under a 0/0 counter would misreport them.
        Ui().NoteText(game == (uint8_t)GAME_MM
                          ? "No data yet. Majora's Mask has not been played this session, and no save holding its "
                            "progress is loaded."
                          : "No data yet. Ocarina of Time has not started this session.");
        ImGui::PopID();
        return;
    }

    if (!summary.hasWorld) {
        // One note, not two: how fresh the data is says nothing without a world.
        Ui().NoteText("Not a randomized world.");
        ImGui::PopID();
        return;
    }

    char note[96];
    snprintf(note, sizeof(note), "%s.", Combo_TrackerFreshnessLabel(game, summary.freshness));
    Ui().NoteText(note);

    ImGui::Text("Seed: %u", (unsigned)summary.seed);
    ImGui::Text("Checks: %d / %d", summary.obtained, summary.shuffled);
    if (summary.skipped > 0) {
        ImGui::Text("Skipped: %d", summary.skipped);
    }

    // Default-closed so the (potentially long) walk only runs when asked for.
    if (ImGui::TreeNode("Checks")) {
        const int count = Combo_TrackerCheckCount(game);
        for (int i = 0; i < count; i++) {
            ComboTrackerCheckRow row;
            if (!Combo_TrackerCheckAt(game, i, &row)) {
                break;
            }
            if (!row.shuffled) {
                continue;
            }
            if (row.name != nullptr) {
                ImGui::Text("%s %s", CheckGlyph(row), row.name);
            } else {
                // No name table loaded (e.g. OoT static data before OoT's
                // first boot): the game-local id is still an honest label.
                ImGui::Text("%s Check 0x%04X", CheckGlyph(row), (unsigned)row.checkId);
            }
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

/**
 * One direction's placement table. The direction is the accessor — the two
 * tables are separate key spaces and are never merged (ADR 0009). SoH's table
 * shape (SohMenuRandomizer.cpp's location tables; the check tracker's
 * settings table): cell padding 8x8, horizontal and vertical borders, a header
 * row.
 */
void DrawForeignList(uint8_t hostGame, const char* title, const char* emptyNote) {
    const int count = Combo_TrackerForeignCount(hostGame);
    char header[64];
    snprintf(header, sizeof(header), "%s (%d)", title, count);
    Ui().SeparatorText(header);
    if (count == 0) {
        Ui().NoteText(emptyNote);
        return;
    }
    ImGui::PushID((int)hostGame);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 8.0f));
    if (ImGui::BeginTable("##Placements", 3, ImGuiTableFlags_BordersH | ImGuiTableFlags_BordersV)) {
        // Check names run longer than item names ("Stone Tower Temple ..."), so the
        // check column takes the larger share and both wrap rather than clip.
        ImGui::TableSetupColumn("Check", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Collected", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (int i = 0; i < count; i++) {
            ComboTrackerForeignRow row;
            if (!Combo_TrackerForeignRowAt(hostGame, i, &row)) {
                break;
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (row.hostCheckName != nullptr) {
                ImGui::TextWrapped("%s", row.hostCheckName);
            } else {
                ImGui::TextWrapped("Check 0x%04X", (unsigned)row.hostCheckId);
            }
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", row.itemName);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(row.redeemed ? "Yes" : "No");
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    ImGui::PopID();
}

} // namespace

void ComboTrackerWindow::Draw() {
    // Read the visibility CVar LIVE rather than trusting the ctor-latched
    // IsVisible() (#489 cause 1; same pattern as ComboSpoilerWindow).
    if (!CVarGetInteger(kComboTrackerVisibilityCVar, 0)) {
        return;
    }

    // SoH's pane chrome (docs/ui-style-guide.md section 10): Ship::GuiWindow::Draw
    // passes its visibility to ImGui::Begin, so an SoH pane has a close button.
    // Closing clears the visibility CVar through SetVisibility, which also
    // schedules the save, as a closed SoH pane does.
    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(480.0f, 520.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(kComboTrackerWindowName, &open, ImGuiWindowFlags_NoFocusOnAppearing)) {
        DrawElement();
    }
    ImGui::End();
    if (!open) {
        SetVisibility(false);
    }
}

void ComboTrackerWindow::DrawElement() {
    // Identity header. Unlike the spoiler, the per-game panels are NOT gated
    // on pairing — a solo OoT rando session has progress worth showing — so
    // an unpaired world gets one honest line, not an early return.
    ComboTrackerIdentity identity;
    Combo_TrackerIdentity(&identity);
    if (identity.paired) {
        ImGui::Text("Seed: %u", (unsigned)identity.sharedRandoSeed);
    } else {
        Ui().NoteText("No paired world.");
    }
    Ui().Spacer(0.0f);

    Ui().PushTheme();
    DrawGamePanel((uint8_t)GAME_OOT, "Ocarina of Time");
    DrawGamePanel((uint8_t)GAME_MM, "Majora's Mask");

    if (identity.paired && ImGui::CollapsingHeader("Cross-Game Placements", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawForeignList((uint8_t)GAME_MM, "OoT Items in MM Checks",
                        "No Ocarina of Time items were placed in Majora's Mask checks.");
        Ui().Spacer(0.0f);
        DrawForeignList((uint8_t)GAME_OOT, "MM Items in OoT Checks",
                        "No Majora's Mask items were placed in Ocarina of Time checks.");
    }
    Ui().PopTheme();
}

void RegisterComboTrackerWindow(std::shared_ptr<Ship::Gui> gui) {
    if (gui == nullptr) {
        return;
    }
    // Idempotence, the #457 guard: the production entry point may be reached
    // from more than one bring-up path, and AddGuiWindow rejects duplicates
    // silently rather than loudly.
    if (gui->GetGuiWindow(kComboTrackerWindowName) != nullptr) {
        return;
    }

    gui->AddGuiWindow(std::make_shared<ComboTrackerWindow>(kComboTrackerVisibilityCVar, kComboTrackerWindowName));
}

} // namespace ComboGui

extern "C" void Combo_TrackerWindow_Init(void) {
    // Register both adapters first, unconditionally — even in the headless
    // case below — so the model is populated for tests that never construct a
    // Gui (the Combo_MMOptionsWindow_Init precedent). Explicit calls, not
    // file-scope registrars: the MM half reads std::maps in other TUs whose
    // static init order is unspecified, and a call site cannot be link-elided
    // the way an unreferenced registrar can (#516's dead-registrar class).
    MM_TrackerAdapter_Register();
    OoT_TrackerAdapter_Register();

    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetWindow() == nullptr) {
        // ROM-free unit harness: shared subsystems without a window/Gui.
        return;
    }
    auto gui = ctx->GetWindow()->GetGui();
    if (gui == nullptr) {
        return;
    }
    ComboGui::RegisterComboTrackerWindow(gui);
}
