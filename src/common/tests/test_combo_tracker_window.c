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
 *    CLEARED (proving the live-CVar early-out holds).
 *
 * 3. VISIBILITY IS THE ONLY THING GATING DRAW. With the CVar SET, Draw()
 *    must reach ImGui — which would abort here — so that case is
 *    deliberately NOT driven; clearing the CVar being sufficient is what is
 *    asserted.
 *
 *    That early-out is also what would make the state/game loop VACUOUS if it
 *    stopped at Draw(): a shut window never touches the model, so the loop
 *    alone proves nothing about the four world states it sets up. So each
 *    iteration additionally drives the exact model reads DrawElement performs
 *    (TrackerDriveModelReads below) — identity, both panels' summary and check
 *    walk, both directions' crossings — across paired/unpaired x adapters
 *    present/absent. The paired-with-absent-adapters corner is the one no
 *    other lock reaches: a crossing row must still resolve, with a NULL host
 *    check name and a non-NULL item name, when the host game's adapter is not
 *    registered at all.
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

/**
 * The model reads ComboTrackerWindow::DrawElement performs, with the ImGui
 * calls this harness cannot make removed. See point 3 in the header: without
 * this, the loop below only re-proves the visibility early-out.
 *
 * Returns false on a contract violation rather than crashing on one — the
 * crash-shaped failures (a missing adapter dereferenced, a row walked past its
 * table) are caught by the process, these are the quiet ones.
 */
bool TrackerDriveModelReads(void) {
    ComboTrackerIdentity identity;
    Combo_TrackerIdentity(&identity);

    const uint8_t games[2] = { (uint8_t)GAME_OOT, (uint8_t)GAME_MM };
    for (int g = 0; g < 2; g++) {
        ComboTrackerGameSummary summary;
        Combo_TrackerGameSummary(games[g], &summary);
        if (Combo_TrackerFreshnessLabel(games[g], summary.freshness) == NULL) {
            return false; // never NULL: it feeds a printf-family format
        }
        if (summary.freshness != COMBO_TRACKER_FRESH_UNAVAILABLE) {
            const int count = Combo_TrackerCheckCount(games[g]);
            for (int i = 0; i < count; i++) {
                ComboTrackerCheckRow row;
                if (!Combo_TrackerCheckAt(games[g], i, &row)) {
                    break; // the renderer's own stop condition
                }
            }
        } else if (Combo_TrackerCheckCount(games[g]) != 0) {
            return false; // "no data" must not hand the renderer rows to walk
        }

        const int crossings = Combo_TrackerForeignCount(games[g]);
        for (int i = 0; i < crossings; i++) {
            ComboTrackerForeignRow foreignRow;
            if (!Combo_TrackerForeignRowAt(games[g], i, &foreignRow)) {
                return false; // the count and the walk must agree
            }
            if (foreignRow.itemName == NULL) {
                return false; // contractually never NULL (placeholder otherwise)
            }
        }
    }
    return true;
}

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

    // Four model states: paired x adapters-present, both ways. State 3
    // (paired with both adapters UN-registered) is the corner no other lock
    // reaches — a crossing row whose HOST GAME cannot resolve a check name.
    for (int state = 0; state < 4; state++) {
        const bool paired = state >= 2;
        const bool adapters = (state % 2) == 0;

        ComboContext_Init();
        if (adapters) {
            MM_TrackerAdapter_Register();
            OoT_TrackerAdapter_Register();
        } else {
            Combo_Tracker_RegisterMM(NULL);
            Combo_Tracker_RegisterOoT(NULL);
        }
        if (paired) {
            gComboCtx.sourceIsRando = true;
            gComboCtx.sharedRandoSeed = 0xC0FFEE97u;
            gComboCtx.sharedRandoSettingsHash = 0x5EED0497u;
            const ComboForeignItemDef* pool = NULL;
            const int poolCount = Combo_GetForeignItemPool(&pool);
            CTW_ASSERT(poolCount >= 1);
            CTW_ASSERT(Combo_SetForeignPlacement(0x0401, pool[0].item) >= 0);
            CTW_ASSERT(Combo_TrackerForeignCount((uint8_t)GAME_MM) == 1);
        } else {
            CTW_ASSERT(Combo_TrackerForeignCount((uint8_t)GAME_MM) == 0);
        }

        for (GameId game : allGames) {
            Context_SetCurrentGame(game);
            // Surviving these IS the assertion: an ungated path reaches
            // ImGui::Begin with no ImGui context and aborts the process.
            window->Draw();
            window->Update();
            // ...and the shut window never touched the model, so drive the
            // reads its open draw path would make (header point 3).
            CTW_ASSERT(TrackerDriveModelReads());
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
