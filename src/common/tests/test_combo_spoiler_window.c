/**
 * @file test_combo_spoiler_window.c
 * @brief ROM-free lock for the cross-game spoiler WINDOW (#496 steps 3-4).
 *
 * The model has its own lock (test_combo_spoiler_view.c). This one covers the
 * window that renders it, in the shape of games/mm/2s2h/mm_trackers_gui_test.cpp:
 *
 * 1. REGISTRATION + IDEMPOTENCE + NAME DE-COLLISION. The window lands on a
 *    bare Ship::Gui under kComboSpoilerWindowName, a second registration is a
 *    no-op, and a stand-in already holding a SoH/MM tracker name is undisturbed.
 *    Gui::AddGuiWindow rejects duplicates SILENTLY from the caller's side, so a
 *    name collision would present only as a window that never appears.
 *
 * 2. GAME-AGNOSTICISM — the hard tripwire. ADR 0008's whole claim is that a
 *    common-owned window reads gComboCtx and never gSaveContext, which is what
 *    lets it draw under GAME_OOT, GAME_MM and GAME_NONE alike. This harness has
 *    NO ImGui context, so any Draw() that reaches ImGui::Begin aborts the test
 *    process. Draw()/Update() are called under all three GameIds with the
 *    visibility CVar CLEARED (proving the live-CVar early-out holds) and the
 *    surrounding gComboCtx in both paired and unpaired states.
 *
 * 3. VISIBILITY IS THE ONLY THING GATING DRAW. With the CVar SET, Draw() must
 *    reach ImGui — which would abort here — so that case is deliberately NOT
 *    driven. What is asserted instead is that clearing the CVar is sufficient
 *    to keep it out of ImGui regardless of active game or pairing state.
 *
 * 4. DRAWN THE SoH WAY (UI parity M8), as far as a source scan can hold it:
 *    through the combo_ui seam, with SoH's close-button chrome and no settings
 *    digest.
 *
 * Appearance itself — column widths, the unpaired copy, whether the collected
 * column reads at a glance — is judged from the UiSnapshot row's captures
 * (Cross-Game Spoiler @paired, @crossings and @unpaired), not here.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as
 * C++) — it drives the C++-linkage ComboGui::RegisterComboSpoilerWindow.
 *
 * Entry point is a RunHeadless bridge, not a TestResult body, matching
 * games/mm/2s2h/mm_trackers_gui_test.cpp: constructing a Ship::GuiWindow reads
 * ConsoleVariables off the Ship::Context singleton, so this needs the same
 * display-free shared bring-up MMTrackersGui does. That bring-up lives in
 * test_runner.cpp (CreateHarnessStyleContext is static there); keeping it out
 * of this file is what lets the file stay a pure exercise of the window.
 */

#include "../ComboSpoilerWindow.h"
#include "../combo_spoiler_view.h"
#include "../combo_ui.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>
#include <libultraship/bridge/consolevariablebridge.h>

#define CSW_ASSERT(cond)                                                                                               \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            return TEST_FAIL;                                                                                          \
        }                                                                                                              \
    } while (0)

namespace {

// Inert stand-in, matching mm_trackers_gui_test.cpp's: empty cvar (no
// ConsoleVariables traffic in the ctor) and no-op overrides.
class SpoilerStandinWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override {
    }
    void DrawElement() override {
    }
    void UpdateElement() override {
    }
};

const char* const kSpoilerNeighbourNames[] = {
    "Check Tracker",    // SoH-owned
    "MM Check Tracker", // MM-owned
};

} // namespace

extern "C" int Combo_SpoilerWindow_RunHeadless(void) {
    printf("[TEST] combo-spoiler-window: the common-owned spoiler window registers de-collided and is inert under "
           "every active game (#496, ADR 0008)\n");

    // Production entry point must be headless-safe: with no window on the
    // shared context it returns without touching a Gui.
    Combo_SpoilerWindow_Init();

    auto gui = std::make_shared<Ship::Gui>();

    std::shared_ptr<Ship::GuiWindow> neighbours[2];
    for (int i = 0; i < 2; i++) {
        neighbours[i] = std::make_shared<SpoilerStandinWindow>("", kSpoilerNeighbourNames[i]);
        gui->AddGuiWindow(neighbours[i]);
    }

    // Keep the window shut for every Draw() below. This is what makes the
    // tripwire survivable: the live-CVar early-out is the only thing between
    // Draw() and ImGui::Begin in a process with no ImGui context.
    CVarClear(ComboGui::kComboSpoilerVisibilityCVar);

    ComboGui::RegisterComboSpoilerWindow(gui);
    auto window = gui->GetGuiWindow(ComboGui::kComboSpoilerWindowName);
    CSW_ASSERT(window != nullptr);

    // Idempotence: a second registration must not replace the instance.
    ComboGui::RegisterComboSpoilerWindow(gui);
    CSW_ASSERT(gui->GetGuiWindow(ComboGui::kComboSpoilerWindowName) == window);

    // De-collision: neither game's window names were disturbed, and the
    // spoiler did not take one of them.
    for (int i = 0; i < 2; i++) {
        CSW_ASSERT(gui->GetGuiWindow(kSpoilerNeighbourNames[i]) == neighbours[i]);
        CSW_ASSERT(gui->GetGuiWindow(kSpoilerNeighbourNames[i]) != window);
    }

    // ---- Game-agnosticism tripwire ---------------------------------------
    const GameId prevGame = Context_GetCurrentGame();
    const GameId allGames[] = { GAME_OOT, GAME_MM, GAME_NONE };

    // Unpaired first (the zero-extended state), then a populated paired world:
    // the window must be inert in both, and must not care which game is live.
    for (int paired = 0; paired <= 1; paired++) {
        ComboContext_Init();
        if (paired) {
            gComboCtx.sourceIsRando = true;
            gComboCtx.sharedRandoSeed = 0xC0FFEE97u;
            gComboCtx.sharedRandoSettingsHash = 0x5EED0497u;
            SharedItem item;
            CSW_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Lens of Truth", &item));
            CSW_ASSERT(Combo_SetForeignPlacement(0x0401, item) >= 0);
            CSW_ASSERT(Combo_SpoilerRowCount() == 1);
        } else {
            CSW_ASSERT(Combo_SpoilerRowCount() == 0);
        }

        for (GameId game : allGames) {
            Context_SetCurrentGame(game);
            // Surviving these IS the assertion: an ungated path reaches
            // ImGui::Begin with no ImGui context and aborts the process.
            window->Draw();
            window->Update();
        }
    }

#ifdef RSBS_SOURCE_DIR
    // ---- 4. Drawn the SoH way (UI parity M8) -------------------------------
    // Appearance itself is judged from the UiSnapshot captures; what a source
    // scan can hold is the shape: the pane draws its notes, spacing and themed
    // headers through the combo_ui seam (whose SoH table is installed), passes
    // an open flag to ImGui::Begin and clears its visibility through
    // SetVisibility when closed (SoH's pane chrome), and prints no settings
    // digest and no TextDisabled/hand-spacing call.
    {
        CSW_ASSERT(ComboUi_IsInstalled());
        std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/src/common/ComboSpoilerWindow.cpp", std::ios::binary);
        CSW_ASSERT(in.good());
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        CSW_ASSERT(text.find("ComboUi_Get()") != std::string::npos);
        CSW_ASSERT(text.find(", &open,") != std::string::npos);
        CSW_ASSERT(text.find("SetVisibility(false)") != std::string::npos);
        CSW_ASSERT(text.find("ImGui::TextDisabled") == std::string::npos);
        CSW_ASSERT(text.find("ImGui::Spacing") == std::string::npos);
        CSW_ASSERT(text.find("digest") == std::string::npos);
        // Each styled element through the seam, not just a ComboUi_Get() helper
        // that nothing calls.
        CSW_ASSERT(text.find("Ui().NoteText(") != std::string::npos);
        CSW_ASSERT(text.find("Ui().SeparatorText(") != std::string::npos);
        CSW_ASSERT(text.find("Ui().PushTheme()") != std::string::npos);
        CSW_ASSERT(text.find("Ui().Spacer(") != std::string::npos);
        // SoH's table shape (style guide section 10): 8x8 cells, both borders, a
        // header row, and no ScrollY inside a pane that scrolls as a whole.
        CSW_ASSERT(text.find("ImGuiStyleVar_CellPadding, ImVec2(8.0f, 8.0f)") != std::string::npos);
        CSW_ASSERT(text.find("ImGuiTableFlags_BordersH | ImGuiTableFlags_BordersV") != std::string::npos);
        CSW_ASSERT(text.find("TableHeadersRow()") != std::string::npos);
        CSW_ASSERT(text.find("ImGuiTableFlags_ScrollY") == std::string::npos);
        CSW_ASSERT(text.find("ImGuiTableFlags_RowBg") == std::string::npos);
        // The section header carries no count (R-N4); the seed is named as the
        // Combo Tracker names it.
        CSW_ASSERT(text.find("(%d)") == std::string::npos);
        CSW_ASSERT(text.find("\"Paired Seed: %u\"") != std::string::npos);
    }
#endif

    Context_SetCurrentGame(prevGame);

    // Leave global state clean for any subsequent test.
    CVarClear(ComboGui::kComboSpoilerVisibilityCVar);
    ComboContext_Init();

    printf("[TEST] PASS: spoiler window registers de-collided and idempotently, and its draw path is inert under "
           "GAME_OOT/GAME_MM/GAME_NONE in both paired and unpaired worlds\n");
    return TEST_PASS;
}
