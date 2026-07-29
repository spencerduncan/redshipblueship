/**
 * @file ComboTrackerWindow.cpp
 * @brief Renders the combo tracker model in-game (#458; ADR 0008).
 *
 * See ComboTrackerWindow.h for the contract. Every value drawn here comes
 * from combo_tracker_view.h; this file holds no state of its own and caches
 * nothing, so progress made mid-session updates on the next frame.
 */

#include "ComboTrackerWindow.h"

#include <imgui.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "combo_tracker_view.h"
#include "context.h" // GameId

namespace ComboGui {

namespace {

/**
 * One game's panel: summary line, freshness label, and a default-closed
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
        ImGui::TextDisabled("No data yet.");
        ImGui::TextWrapped("%s", game == (uint8_t)GAME_MM
                                     ? "Majora's Mask has not been entered this session, and no unified save "
                                       "carrying an MM half has been loaded."
                                     : "Ocarina of Time has not booted this session.");
        ImGui::PopID();
        return;
    }

    ImGui::TextDisabled("(%s)", Combo_TrackerFreshnessLabel(game, summary.freshness));

    if (!summary.hasWorld) {
        ImGui::TextWrapped("Not a randomized world.");
        ImGui::PopID();
        return;
    }

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
            const char* mark = row.obtained ? "[x]" : (row.skipped ? "[s]" : "[ ]");
            if (row.name != nullptr) {
                ImGui::Text("%s %s", mark, row.name);
            } else {
                // No name table loaded (e.g. OoT static data before OoT's
                // first boot): the game-local id is still an honest label.
                ImGui::Text("%s Check 0x%04X", mark, (unsigned)row.checkId);
            }
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

/** One direction's placement list. The direction is the accessor — the two
 *  tables are separate key spaces and are never merged (ADR 0009). */
void DrawForeignList(uint8_t hostGame, const char* title) {
    const int count = Combo_TrackerForeignCount(hostGame);
    ImGui::Text("%s: %d", title, count);
    for (int i = 0; i < count; i++) {
        ComboTrackerForeignRow row;
        if (!Combo_TrackerForeignRowAt(hostGame, i, &row)) {
            break;
        }
        ImGui::Bullet();
        if (row.hostCheckName != nullptr) {
            ImGui::Text("%s hosts %s%s", row.hostCheckName, row.itemName, row.redeemed ? " (redeemed)" : "");
        } else {
            ImGui::Text("Check 0x%04X hosts %s%s", (unsigned)row.hostCheckId, row.itemName,
                        row.redeemed ? " (redeemed)" : "");
        }
    }
}

} // namespace

void ComboTrackerWindow::Draw() {
    // Read the visibility CVar LIVE rather than trusting the ctor-latched
    // IsVisible() (#489 cause 1; same pattern as ComboSpoilerWindow).
    if (!CVarGetInteger(kComboTrackerVisibilityCVar, 0)) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(480.0f, 520.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(kComboTrackerWindowName, nullptr, ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::End();
        return;
    }

    DrawElement();

    ImGui::End();
}

void ComboTrackerWindow::DrawElement() {
    // Identity header. Unlike the spoiler, the per-game panels are NOT gated
    // on pairing — a solo OoT rando session has progress worth showing — so
    // an unpaired world gets one honest line, not an early return.
    ComboTrackerIdentity identity;
    Combo_TrackerIdentity(&identity);
    if (identity.paired) {
        ImGui::Text("Paired world — seed %u", (unsigned)identity.sharedRandoSeed);
        ImGui::TextDisabled("Settings digest %08X / MM profile %08X", (unsigned)identity.sharedRandoSettingsHash,
                            (unsigned)identity.mmProfileDigest);
    } else {
        ImGui::TextDisabled("No paired world.");
    }
    ImGui::Separator();

    DrawGamePanel((uint8_t)GAME_OOT, "Ocarina of Time");
    DrawGamePanel((uint8_t)GAME_MM, "Majora's Mask");

    if (identity.paired && ImGui::CollapsingHeader("Cross-Game Placements", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawForeignList((uint8_t)GAME_MM, "OoT items in MM checks");
        ImGui::Spacing();
        DrawForeignList((uint8_t)GAME_OOT, "MM items in OoT checks");
    }
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
