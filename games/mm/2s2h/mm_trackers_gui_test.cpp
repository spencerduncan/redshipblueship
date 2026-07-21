/**
 * ROM-free lock for the MM tracker registration surface (#392). CTest label
 * "redship", row mm-trackers-gui in src/common/test_runner.cpp.
 *
 * What it locks (docs/unified-surface-findings.md §3, the BenMenu-bypass
 * tracker route):
 *
 * 1. REGISTRATION + NAME DE-COLLISION. Gui::AddGuiWindow rejects duplicate
 *    names, and SoH already owns "Check Tracker" / "Item Tracker" / their
 *    settings twins on the shared Gui — so if MM's windows ever regress to
 *    the upstream names, they silently never register. Stand-in windows
 *    occupy SoH's names first; S2H::TrackersGui::RegisterWindows must then
 *    land all four MM windows under their "MM "-prefixed names while the
 *    stand-ins keep theirs. Also locks AddGuiWindow's duplicate rejection
 *    itself (the premise the prefixing rests on) and registration
 *    idempotence.
 *
 * 2. ACTIVE-GAME GATING. gSaveContext storage is unified, so an MM tracker
 *    drawing while OoT is active reads OoT bytes through MM's SaveContext
 *    layout. MM_TrackersGui_ShouldDraw() must flip with
 *    Context_SetCurrentGame, and — the wired half — calling Draw()/Update()
 *    on every registered MM window while OoT (or nothing) is active must be
 *    a strict no-op. That second half is a hard tripwire: this harness has
 *    NO ImGui context, and the windows' visibility CVars are forced on, so
 *    an ungated Draw() reaches ImGui::Begin and aborts the test process.
 *    (Draw() with MM active is deliberately NOT called: it would need an
 *    ImGui context plus MM play state, which is operator/integration
 *    territory.)
 *
 * 3. HEADLESS SAFETY of the production entry point: MM_TrackersGui_Init()
 *    must no-op cleanly when the shared context has no window (the exact
 *    state MM_Rando_Init sees in ROM-free harnesses).
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>
#include <memory>

#include <ship/Context.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>

#include "TrackersGuiSingleExe.h"
#include "2s2h/Rando/CheckTracker/CheckTracker.h"
#include "2s2h/Enhancements/Trackers/ItemTracker/ItemTracker.h"
#include "2s2h/Enhancements/Trackers/ItemTracker/ItemTrackerSettings.h"
#include "context.h"

namespace BenGui {
// Defined in 2s2h/TrackersGuiSingleExe.cpp; populated by RegisterWindows.
extern std::shared_ptr<Rando::CheckTracker::CheckTrackerWindow> mRandoCheckTrackerWindow;
extern std::shared_ptr<Rando::CheckTracker::SettingsWindow> mRandoCheckTrackerSettingsWindow;
extern std::shared_ptr<ItemTrackerWindow> mItemTrackerWindow;
extern std::shared_ptr<ItemTrackerSettingsWindow> mItemTrackerSettingsWindow;
} // namespace BenGui

namespace {

#define TGT_ASSERT(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

// Minimal stand-in for a SoH-owned window: empty cvar (no ConsoleVariables
// traffic in the ctor) and inert overrides.
class StandinWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override {
    }
    void DrawElement() override {
    }
    void UpdateElement() override {
    }
};

// The names SoH registers on the shared Gui (games/oot/soh/SohGui/SohGui.cpp).
const char* kSohOwnedNames[] = {
    "Check Tracker",
    "Check Tracker Settings",
    "Item Tracker",
    "Item Tracker Settings",
};

// Visibility CVars of the four MM windows, forced ON so that an UNGATED
// Draw() cannot early-out on !IsVisible() before reaching ImGui — the gate
// must be what stops it.
const char* kMMWindowVisibilityCVars[] = {
    "gWindows.CheckTracker",
    "gWindows.CheckTrackerSettings",
    "gWindows.ItemTracker",
    "gWindows.ItemTrackerSettings",
};

} // namespace

extern "C" int MM_TrackersGui_RunHeadless(void) {
    auto ctx = Ship::Context::GetInstance();
    TGT_ASSERT(ctx != nullptr, "Ship::Context singleton missing — run the shared bring-up first");
    TGT_ASSERT(ctx->GetConsoleVariables() != nullptr, "ConsoleVariables missing — GuiWindow ctors need them");
    TGT_ASSERT(ctx->GetConfig() != nullptr, "Config missing — ItemTrackerSettings::InitElement needs it");
    TGT_ASSERT(ctx->GetConsole() != nullptr, "Console missing — Gui's default ConsoleWindow needs it");
    TGT_ASSERT(ctx->GetWindow() == nullptr,
               "harness unexpectedly has a real window — the headless-safety leg below would be vacuous");

    // ---- 3. Production entry point is headless-safe ----------------------
    // MM_Rando_Init calls this in every ROM-free harness; with no window it
    // must return without touching a Gui.
    MM_TrackersGui_Init();

    // ---- 1. Registration + de-collision on a standalone Gui --------------
    // A bare Ship::Gui never Init()ed: AddGuiWindow/GetGuiWindow work without
    // an ImGui context, which is exactly what makes the gate tripwire below a
    // hard one.
    auto gui = std::make_shared<Ship::Gui>();

    std::shared_ptr<Ship::GuiWindow> standins[4];
    for (int i = 0; i < 4; i++) {
        standins[i] = std::make_shared<StandinWindow>("", kSohOwnedNames[i]);
        gui->AddGuiWindow(standins[i]);
        TGT_ASSERT(gui->GetGuiWindow(kSohOwnedNames[i]) == standins[i], "stand-in failed to register");
    }

    // Force every MM window visible BEFORE construction (the ctor reads the
    // CVar; setting it afterwards would need Show(), which dereferences the
    // null window through SaveConsoleVariablesNextFrame).
    for (const char* cvar : kMMWindowVisibilityCVars) {
        CVarSetInteger(cvar, 1);
    }

    S2H::TrackersGui::RegisterWindows(gui);

    for (const char* name : S2H::TrackersGui::kAllTrackerWindowNames) {
        TGT_ASSERT(gui->GetGuiWindow(name) != nullptr, "MM tracker window missing from the Gui registry");
    }
    TGT_ASSERT(gui->GetGuiWindow(S2H::TrackersGui::kCheckTrackerWindowName) == BenGui::mRandoCheckTrackerWindow,
               "registry entry is not the BenGui check-tracker global the vendored TUs link against");
    TGT_ASSERT(gui->GetGuiWindow(S2H::TrackersGui::kCheckTrackerSettingsWindowName) ==
                   BenGui::mRandoCheckTrackerSettingsWindow,
               "registry entry is not the BenGui check-tracker-settings global");
    TGT_ASSERT(gui->GetGuiWindow(S2H::TrackersGui::kItemTrackerWindowName) == BenGui::mItemTrackerWindow,
               "registry entry is not the BenGui item-tracker global the vendored TUs link against");
    TGT_ASSERT(gui->GetGuiWindow(S2H::TrackersGui::kItemTrackerSettingsWindowName) ==
                   BenGui::mItemTrackerSettingsWindow,
               "registry entry is not the BenGui item-tracker-settings global");

    // SoH's names must still belong to the stand-ins: MM registration must
    // not have displaced (or been swallowed by) the unprefixed names.
    for (int i = 0; i < 4; i++) {
        TGT_ASSERT(gui->GetGuiWindow(kSohOwnedNames[i]) == standins[i],
                   "MM registration disturbed a SoH-owned window name");
    }

    // AddGuiWindow's duplicate rejection — the premise the prefixing rests
    // on. A late arrival under an MM name must bounce off.
    auto usurper = std::make_shared<StandinWindow>("", S2H::TrackersGui::kCheckTrackerWindowName);
    gui->AddGuiWindow(usurper);
    TGT_ASSERT(gui->GetGuiWindow(S2H::TrackersGui::kCheckTrackerWindowName) == BenGui::mRandoCheckTrackerWindow,
               "duplicate AddGuiWindow displaced the registered MM window");

    // Idempotence: a second RegisterWindows must leave the registry as-is.
    auto before = BenGui::mRandoCheckTrackerWindow;
    S2H::TrackersGui::RegisterWindows(gui);
    TGT_ASSERT(BenGui::mRandoCheckTrackerWindow == before, "re-registration replaced the window instances");

    // ---- 2. Active-game gating -------------------------------------------
    const GameId prevGame = Context_GetCurrentGame();

    std::shared_ptr<Ship::GuiWindow> mmWindows[] = {
        BenGui::mRandoCheckTrackerWindow,
        BenGui::mRandoCheckTrackerSettingsWindow,
        BenGui::mItemTrackerWindow,
        BenGui::mItemTrackerSettingsWindow,
    };

    const GameId inactiveGames[] = { GAME_OOT, GAME_NONE };
    for (GameId game : inactiveGames) {
        Context_SetCurrentGame(game);
        if (MM_TrackersGui_ShouldDraw()) {
            printf("[TEST] FAIL: MM_TrackersGui_ShouldDraw() true while game %d is active\n", (int)game);
            Context_SetCurrentGame(prevGame);
            return 1;
        }
        // Hard tripwire: visibility CVars are ON and there is NO ImGui
        // context, so an ungated Draw() reaches ImGui::Begin and aborts.
        // Surviving these calls == the gate held for every window.
        for (auto& window : mmWindows) {
            window->Draw();
            window->Update();
        }
    }

    Context_SetCurrentGame(GAME_MM);
    const bool mmActiveDraws = MM_TrackersGui_ShouldDraw();
    Context_SetCurrentGame(prevGame);
    TGT_ASSERT(mmActiveDraws, "MM_TrackersGui_ShouldDraw() false while MM is the active game");

    // Cleanup: don't leak forced-visible tracker CVars into later tests or a
    // saved cvar file.
    for (const char* cvar : kMMWindowVisibilityCVars) {
        CVarClear(cvar);
    }

    printf("[TEST] PASS: mm-trackers-gui — four MM tracker windows register under de-collided names beside "
           "SoH's, and their draw path is inert unless MM is the active game\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
