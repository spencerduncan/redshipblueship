/**
 * @file test_combo_tracker_window.c
 * @brief ROM-free lock for the combo tracker WINDOW (#458; ADR 0008).
 *
 * The model has its own lock (test_combo_tracker_view.c). This one covers the
 * window that renders it, in the shape of test_combo_spoiler_window.c:
 *
 * 1. REGISTRATION + IDEMPOTENCE + NAME DE-COLLISION. The window lands on a
 *    bare Ship::Gui under kComboTrackerWindowName, a second registration is a
 *    no-op, and stand-ins already holding SoH/MM tracker names and the other
 *    common-owned window names are undisturbed. Gui::AddGuiWindow rejects
 *    duplicates SILENTLY from the caller's side, so a collision would present
 *    only as a window that never appears.
 *
 * 2. GAME-AGNOSTICISM — the hard tripwire. ADR 0008's claim is that a
 *    common-owned window reads gComboCtx and the adapter surfaces, never
 *    gSaveContext through either game's layout, which is what lets it draw
 *    under GAME_OOT, GAME_MM and GAME_NONE alike. This harness has NO ImGui
 *    context, so any Draw() that reaches ImGui::Begin aborts the process.
 *    Draw()/Update() run under all three GameIds with the visibility CVar
 *    CLEARED (proving the live-CVar early-out holds), in both paired and
 *    unpaired worlds, AND with both adapters deliberately UN-registered —
 *    the window must never turn "unavailable" into a null deref.
 *
 * 3. VISIBILITY IS THE ONLY THING GATING DRAW. With the CVar SET, Draw()
 *    must reach ImGui — which would abort here — so that case is
 *    deliberately NOT driven; clearing the CVar being sufficient is what is
 *    asserted.
 *
 * Deliberately absent: any assertion about appearance — operator
 * verification, no headless stand-in exists.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as
 * C++) — it drives the C++-linkage ComboGui::RegisterComboTrackerWindow.
 * Entry point is a RunHeadless bridge (GuiWindow ctors read ConsoleVariables
 * off the Ship::Context singleton, so it needs the display-free shared
 * bring-up that lives in test_runner.cpp).
 */

#include "../ComboTrackerWindow.h"
#include "../combo_tracker_view.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../test_runner.h"

#include <cstdio>
#include <memory>

#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>
#include <libultraship/bridge/consolevariablebridge.h>

#define CTW_ASSERT(cond)                                                                                               \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            return TEST_FAIL;                                                                                          \
        }                                                                                                              \
    } while (0)

namespace {

// Inert stand-in, matching mm_trackers_gui_test.cpp's: empty cvar (no
// ConsoleVariables traffic in the ctor) and no-op overrides.
class TrackerStandinWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override {
    }
    void DrawElement() override {
    }
    void UpdateElement() override {
    }
};

const char* const kTrackerNeighbourNames[] = {
    "Check Tracker",     // SoH-owned
    "MM Check Tracker",  // MM-owned
    "Cross-Game Spoiler" // the other common-owned window (ADR 0008)
};
constexpr int kTrackerNeighbourCount = 3;

} // namespace

extern "C" int Combo_TrackerWindow_RunHeadless(void) {
    printf("[TEST] combo-tracker-window: the combo tracker window registers de-collided and is inert under every "
           "active game (#458, ADR 0008)\n");

    // Production entry point must be headless-safe: with no window on the
    // shared context it registers the adapters and returns without touching a
    // Gui.
    Combo_TrackerWindow_Init();

    auto gui = std::make_shared<Ship::Gui>();

    std::shared_ptr<Ship::GuiWindow> neighbours[kTrackerNeighbourCount];
    for (int i = 0; i < kTrackerNeighbourCount; i++) {
        neighbours[i] = std::make_shared<TrackerStandinWindow>("", kTrackerNeighbourNames[i]);
        gui->AddGuiWindow(neighbours[i]);
    }

    // Keep the window shut for every Draw() below: the live-CVar early-out is
    // the only thing between Draw() and ImGui::Begin in a process with no
    // ImGui context.
    CVarClear(ComboGui::kComboTrackerVisibilityCVar);

    ComboGui::RegisterComboTrackerWindow(gui);
    auto window = gui->GetGuiWindow(ComboGui::kComboTrackerWindowName);
    CTW_ASSERT(window != nullptr);

    // Idempotence: a second registration must not replace the instance.
    ComboGui::RegisterComboTrackerWindow(gui);
    CTW_ASSERT(gui->GetGuiWindow(ComboGui::kComboTrackerWindowName) == window);

    // De-collision: no neighbour's name was disturbed or taken.
    for (int i = 0; i < kTrackerNeighbourCount; i++) {
        CTW_ASSERT(gui->GetGuiWindow(kTrackerNeighbourNames[i]) == neighbours[i]);
        CTW_ASSERT(gui->GetGuiWindow(kTrackerNeighbourNames[i]) != window);
    }

    // ---- Game-agnosticism tripwire ---------------------------------------
    const GameId prevGame = Context_GetCurrentGame();
    const GameId allGames[] = { GAME_OOT, GAME_MM, GAME_NONE };

    // Three model states: unpaired with adapters REGISTERED, unpaired with
    // both adapters UN-registered (the window must not deref a missing
    // adapter), and a populated paired world. Inert in all of them, under
    // every active game.
    for (int state = 0; state < 3; state++) {
        ComboContext_Init();
        if (state == 1) {
            Combo_Tracker_RegisterMM(NULL);
            Combo_Tracker_RegisterOoT(NULL);
        } else if (state == 2) {
            MM_TrackerAdapter_Register();
            OoT_TrackerAdapter_Register();
            gComboCtx.sourceIsRando = true;
            gComboCtx.sharedRandoSeed = 0xC0FFEE97u;
            gComboCtx.sharedRandoSettingsHash = 0x5EED0497u;
            const ComboForeignItemDef* pool = NULL;
            const int poolCount = Combo_GetForeignItemPool(&pool);
            CTW_ASSERT(poolCount >= 1);
            CTW_ASSERT(Combo_SetForeignPlacement(0x0401, pool[0].item) >= 0);
            CTW_ASSERT(Combo_TrackerForeignCount((uint8_t)GAME_MM) == 1);
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

    // Leave global state clean: adapters registered (the production state),
    // combo context reset, CVar cleared.
    MM_TrackerAdapter_Register();
    OoT_TrackerAdapter_Register();
    CVarClear(ComboGui::kComboTrackerVisibilityCVar);
    ComboContext_Init();

    printf("[TEST] PASS: tracker window registers de-collided and idempotently, and its draw path is inert under "
           "GAME_OOT/GAME_MM/GAME_NONE with adapters present, absent, and a paired world\n");
    return TEST_PASS;
}
