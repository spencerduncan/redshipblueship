/**
 * @file test_combo_settings_window.c
 * @brief ROM-free lock for the combo settings WINDOW (ADR 0011 increment 2).
 *
 * The authoring surface has its own lock (combo-settings-authoring, in
 * test_combo_settings.c). This one covers the window that renders it, in the
 * shape of the spoiler window's lock (test_combo_spoiler_window.c):
 *
 * 1. REGISTRATION + IDEMPOTENCE + NAME DE-COLLISION. The pane lands on a bare
 *    Ship::Gui under kComboSettingsWindowName, a second registration is a
 *    no-op, and stand-ins already holding a SoH tracker name, an MM tracker
 *    name and the sibling common-owned spoiler window's name are undisturbed.
 *    (The MM randomizer options were a third sibling until 2026-09-27, when
 *    they became Combo menu pages; there is no such window to collide with.)
 *    Gui::AddGuiWindow rejects duplicates SILENTLY from the caller's side, so a
 *    collision would present only as a window that never appears.
 *
 * 2. GAME-AGNOSTICISM — the hard tripwire. ADR 0008 rule 5's claim is that a
 *    common-owned window reads gComboCtx and never gSaveContext, which is what
 *    lets it draw under GAME_OOT, GAME_MM and GAME_NONE alike. This harness has
 *    NO ImGui context, so any Draw() that reaches ImGui::Begin aborts the test
 *    process. Draw()/Update() are called under all three GameIds with the
 *    visibility CVar CLEARED, in an unpaired world, a paired-but-unfrozen
 *    (legacy) world, and a paired-and-frozen world — the three states the pane
 *    renders differently.
 *
 * 3. THE PRODUCTION ENTRY POINT IS HEADLESS-SAFE. Combo_ComboSettingsWindow_Init()
 *    must return cleanly with no window on the shared context.
 *
 * Deliberately absent: any assertion about appearance, and the write gate —
 * that is the authoring lock's, because the gate is on the writers and not
 * the widget (ADR 0004 §6).
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++)
 * — it drives the C++-linkage ComboGui::RegisterComboSettingsWindow.
 *
 * Entry point is a RunHeadless bridge, not a TestResult body, for the same
 * reason the sibling window locks' are: constructing a Ship::GuiWindow reads
 * ConsoleVariables off the Ship::Context singleton, so it needs the
 * display-free shared bring-up that lives (static) in test_runner.cpp.
 */

#include "../ComboSettingsWindow.h"
#include "../ComboSpoilerWindow.h"
#include "../combo_settings_view.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <memory>

#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>
#include <libultraship/bridge/consolevariablebridge.h>

#define CSW_ASSERT(cond)                                                   \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return TEST_FAIL;                                              \
        }                                                                  \
    } while (0)

namespace {

// Inert stand-in, matching the sibling window locks': empty cvar (no
// ConsoleVariables traffic in the ctor) and no-op overrides.
class ComboSettingsStandinWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override {
    }
    void DrawElement() override {
    }
    void UpdateElement() override {
    }
};

const char* const kComboSettingsNeighbourNames[] = {
    "Check Tracker",    // SoH-owned
    "MM Check Tracker", // MM-owned
};

} // namespace

extern "C" int Combo_SettingsWindow_RunHeadless(void) {
    printf("[TEST] combo-settings-window: the common-owned combo settings pane registers de-collided and is inert "
           "under every active game and every pairing state (ADR 0011 increment 2, ADR 0008)\n");

    // Production entry point must be headless-safe: with no window on the
    // shared context it returns without touching a Gui.
    Combo_ComboSettingsWindow_Init();

    auto gui = std::make_shared<Ship::Gui>();

    std::shared_ptr<Ship::GuiWindow> neighbours[2];
    for (int i = 0; i < 2; i++) {
        neighbours[i] = std::make_shared<ComboSettingsStandinWindow>("", kComboSettingsNeighbourNames[i]);
        gui->AddGuiWindow(neighbours[i]);
    }

    // The sibling common-owned window: both live in the same unprefixed name
    // space, so a collision between them is what this file is uniquely placed
    // to catch. Its visibility CVar is cleared first so its ctor latches
    // "closed" and its Draw() path stays out of ImGui below.
    CVarClear(ComboGui::kComboSpoilerVisibilityCVar);
    ComboGui::RegisterComboSpoilerWindow(gui);
    auto spoiler = gui->GetGuiWindow(ComboGui::kComboSpoilerWindowName);
    CSW_ASSERT(spoiler != nullptr);

    // Keep the pane shut for every Draw() below. The live-CVar early-out is the
    // only thing between Draw() and ImGui::Begin in a process with no ImGui
    // context, which is what makes the tripwire survivable.
    CVarClear(ComboGui::kComboSettingsVisibilityCVar);

    ComboGui::RegisterComboSettingsWindow(gui);
    auto window = gui->GetGuiWindow(ComboGui::kComboSettingsWindowName);
    CSW_ASSERT(window != nullptr);
    CSW_ASSERT(window != spoiler);

    // Idempotence: a second registration must not replace the instance.
    ComboGui::RegisterComboSettingsWindow(gui);
    CSW_ASSERT(gui->GetGuiWindow(ComboGui::kComboSettingsWindowName) == window);

    // De-collision: neither game's window names were disturbed, the sibling
    // combo window still resolves to itself, and the pane took none of them.
    for (int i = 0; i < 2; i++) {
        CSW_ASSERT(gui->GetGuiWindow(kComboSettingsNeighbourNames[i]) == neighbours[i]);
        CSW_ASSERT(gui->GetGuiWindow(kComboSettingsNeighbourNames[i]) != window);
    }
    CSW_ASSERT(gui->GetGuiWindow(ComboGui::kComboSpoilerWindowName) == spoiler);

    // The common-owned windows must not share a visibility CVar either — that
    // would make one un-openable without the other.
    CSW_ASSERT(strcmp(ComboGui::kComboSettingsVisibilityCVar, ComboGui::kComboSpoilerVisibilityCVar) != 0);

    // ---- Game-agnosticism tripwire ---------------------------------------
    // Three pairing states, because the pane renders each differently
    // (unpaired header / legacy "not frozen" header + authoring notice /
    // "already decided" banner with values from the save), and each must stay
    // out of ImGui with the visibility CVar cleared.
    const GameId prevGame = Context_GetCurrentGame();
    const GameId allGames[] = { GAME_OOT, GAME_MM, GAME_NONE };

    for (int state = 0; state <= 2; state++) {
        ComboContext_Init();
        if (state >= 1) {
            gComboCtx.sourceIsRando = true;
            gComboCtx.sharedRandoSeed = 0xC0FFEE11u;
            gComboCtx.sharedRandoSettingsHash = 0x5EED0011u;
            gComboCtx.mmProfileDigest = 0x4D4D0011u;
        }
        if (state == 2) {
            ComboSettingsRecord rules;
            Combo_ResolveComboSettings(&rules);
            Combo_FreezeComboSettings(&rules);
            CSW_ASSERT(Combo_ComboSettingsFrozen());
        }

        for (GameId game : allGames) {
            Context_SetCurrentGame(game);
            // Surviving these IS the assertion: an ungated path reaches
            // ImGui::Begin with no ImGui context and aborts the process.
            window->Draw();
            window->Update();
        }
    }

    Context_SetCurrentGame(prevGame);

    // Leave global state clean for any subsequent test.
    CVarClear(ComboGui::kComboSettingsVisibilityCVar);
    ComboContext_Init();

    printf("[TEST] PASS: the combo settings pane registers de-collided and idempotently beside both games' windows "
           "and both sibling combo windows, and its draw path is inert under GAME_OOT/GAME_MM/GAME_NONE in every "
           "pairing state\n");
    return TEST_PASS;
}
