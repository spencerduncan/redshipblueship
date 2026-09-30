/**
 * ROM-free lock for OoT's active-game window gate (#797). CTest label "redship", row OoTWindowsGate
 * (oot-windows-gate) in src/common/test_runner.cpp, which also runs the source scan over
 * SohGui::SetupGuiElements before calling this.
 *
 * gSaveContext is one shared buffer, so an OoT window that runs while Majora's Mask is the running game reads MM's
 * bytes through OoT's SaveContext layout and, for the Save Editor, writes them: DrawInfoTab clamps health and assigns
 * magicCapacity / magic on every drawn frame. Eight SoH windows touch OoT save or play state, and each is reached three
 * ways: Draw() from the Gui's per-frame window loop, Update() from its unconditional per-frame update, and
 * DrawElement() straight from the Port Menu when the window is hidden and its row embeds (SohGui/Menu.cpp), which
 * bypasses visibility entirely.
 *
 * The hard tripwire: this harness has NO ImGui context and NO OTRGlobals::Instance, and every window's visibility
 * CVar is forced on before construction (the ctor latches it; Show() would dereference the null Ship::Window). So any
 * of the three paths that is not gated reaches ImGui or dereferences a null pointer and kills the process. Surviving
 * the calls with Majora's Mask (and with no game) running is the assertion. The calls run DrawElement() first, so an
 * ungated build dies on the embed path, the one that writes MM's save with the window closed.
 *
 * That tripwire only locks paths whose body reaches ImGui, and it never runs a body under OoT. The real windows'
 * UpdateElement bodies are empty or wait for a click, so the Update leg is locked by a counting spy base window instead
 * (GateSpy below): through OoTActiveGated<GateSpy>, each of the three bodies must run exactly once under OoT and never
 * under Majora's Mask or with no game running. Removing any one of the wrapper's three overrides fails that check.
 *
 * The windows are constructed without Gui::AddGuiWindow on purpose: AddGuiWindow calls Init(), and the check
 * tracker's InitElement dereferences a null SaveManager::Instance here. The Message Viewer is leaked on purpose: its
 * InitElement allocates the buffers its destructor frees, and InitElement never runs.
 *
 * The window types and CVars below mirror SohGui::SetupGuiElements; the source scan in test_runner.cpp holds that
 * function to exactly these eight wrapped sites.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>
#include <memory>
#include <string>

#include <libultraship/libultraship.h>
#include <ship/Context.h>
#include <ship/window/gui/GuiWindow.h>

#include "soh/SohGui/SohGui.hpp"
#include "soh/SohGui/OoTActiveGated.h"
#include "soh/Enhancements/debugger/MessageViewer.h"
#include "soh/cvar_prefixes.h"
#include "context.h"

namespace {

#define OWG_ASSERT(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            fflush(stdout);                                               \
            return 1;                                                     \
        }                                                                 \
    } while (0)

struct GatedWindow {
    const char* name;
    const char* visibilityCVar;
    std::shared_ptr<Ship::GuiWindow> window;
};

// The trackers' layout CVars: a floating tracker or a "only while paused" one takes a different early branch in
// DrawElement, so they are cleared to leave each window on its default path into ImGui.
const char* kTrackerLayoutCVars[] = {
    CVAR_TRACKER_ITEM("WindowType"),      CVAR_TRACKER_ITEM("ShowOnlyPaused"), CVAR_TRACKER_CHECK("WindowType"),
    CVAR_TRACKER_CHECK("ShowOnlyPaused"), CVAR_TRACKER_ENTRANCE("WindowType"), CVAR_TRACKER_ENTRANCE("ShowOnlyPaused"),
};

#define OWG_SPY_CVAR CVAR_WINDOW("OoTWindowsGateSpy")

// A base window that counts how often each of the three bodies runs and touches nothing else, so OoTActiveGated<> can
// be driven under every game, OoT included. The eight real windows cannot lock the Update leg: their UpdateElement
// bodies are empty or act only after a click, so an ungated Update() on them returns harmlessly. Nor can they show
// pass-through, because under OoT their bodies reach ImGui, which this harness does not have. The spy overrides Draw()
// itself (GuiWindow::Draw would reach ImGui), and that override is what OoTActiveGated<>::Draw forwards to.
class GateSpy : public Ship::GuiWindow {
  public:
    GateSpy(const std::string& consoleVariable, const std::string& name) : Ship::GuiWindow(consoleVariable, name) {
    }

    int draws = 0;
    int drawElements = 0;
    int updateElements = 0;

    void Draw() override {
        draws++;
    }
    void DrawElement() override {
        drawElements++;
    }

  protected:
    void InitElement() override {
    }
    void UpdateElement() override {
        updateElements++;
    }
};

// Drives the three ways into a window through the wrapper under one game, through the GuiWindow pointer the Gui and the
// Port Menu hold, and counts the failures: each body must run once when wantBodies, and never otherwise.
int RunSpy(GameId game, const char* gameName, bool wantBodies) {
    auto spy = std::make_shared<OoTActiveGated<GateSpy>>(OWG_SPY_CVAR, "OoTWindowsGate spy");
    std::shared_ptr<Ship::GuiWindow> asWindow = spy;
    Context_SetCurrentGame(game);
    asWindow->DrawElement();
    asWindow->Update();
    asWindow->Draw();
    printf("[TEST] oot-windows-gate: spy, %s running: DrawElement body ran %d, UpdateElement body ran %d, Draw body "
           "ran %d\n",
           gameName, spy->drawElements, spy->updateElements, spy->draws);
    const int want = wantBodies ? 1 : 0;
    int failures = 0;
    if (spy->drawElements != want) {
        printf("[TEST] FAIL: oot-windows-gate: %s running, the menu-embed DrawElement() body ran %d time(s), want %d\n",
               gameName, spy->drawElements, want);
        failures++;
    }
    if (spy->updateElements != want) {
        printf("[TEST] FAIL: oot-windows-gate: %s running, the per-frame Update() body ran %d time(s), want %d\n",
               gameName, spy->updateElements, want);
        failures++;
    }
    if (spy->draws != want) {
        printf("[TEST] FAIL: oot-windows-gate: %s running, the Draw() body ran %d time(s), want %d\n", gameName,
               spy->draws, want);
        failures++;
    }
    fflush(stdout);
    return failures;
}

} // namespace

extern "C" int OoT_WindowsGate_RunHeadless(void) {
    auto ctx = Ship::Context::GetInstance();
    OWG_ASSERT(ctx != nullptr, "Ship::Context singleton missing: run the shared bring-up first");
    OWG_ASSERT(ctx->GetConsoleVariables() != nullptr, "ConsoleVariables missing: GuiWindow ctors read them");
    OWG_ASSERT(ctx->GetWindow() == nullptr, "harness unexpectedly has a real window: the tripwire below would be soft");

    for (const char* cvar : kTrackerLayoutCVars) {
        CVarClear(cvar);
    }

    GatedWindow windows[] = {
        { "Save Editor", CVAR_WINDOW("SaveEditor"), nullptr },
        { "Value Viewer", CVAR_WINDOW("ValueViewer"), nullptr },
        { "Message Viewer", CVAR_WINDOW("MessageViewer"), nullptr },
        { "Gameplay Stats", CVAR_WINDOW("GameplayStats"), nullptr },
        { "Check Tracker", CVAR_WINDOW("CheckTracker"), nullptr },
        { "Entrance Tracker", CVAR_WINDOW("EntranceTracker"), nullptr },
        { "Item Tracker", CVAR_WINDOW("ItemTracker"), nullptr },
        { "Time Splits", CVAR_WINDOW("TimeSplits"), nullptr },
    };

    // Visible BEFORE construction, so an ungated Draw() cannot leave on !IsVisible() before reaching ImGui.
    for (const GatedWindow& w : windows) {
        CVarSetInteger(w.visibilityCVar, 1);
    }

    windows[0].window = std::make_shared<OoTActiveGated<SaveEditorWindow>>(windows[0].visibilityCVar, windows[0].name,
                                                                           ImVec2(520, 600));
    windows[1].window = std::make_shared<OoTActiveGated<ValueViewerWindow>>(windows[1].visibilityCVar, windows[1].name,
                                                                            ImVec2(520, 600));
    {
        // Leaked on purpose (see the file comment): the holder is never deleted, so ~MessageViewer never frees the
        // buffers InitElement never allocated.
        auto* leaked = new std::shared_ptr<Ship::GuiWindow>(std::make_shared<OoTActiveGated<MessageViewer>>(
            windows[2].visibilityCVar, windows[2].name, ImVec2(520, 600)));
        windows[2].window = *leaked;
    }
    windows[3].window = std::make_shared<OoTActiveGated<GameplayStatsWindow>>(windows[3].visibilityCVar,
                                                                              windows[3].name, ImVec2(480, 550));
    windows[4].window = std::make_shared<OoTActiveGated<CheckTracker::CheckTrackerWindow>>(
        windows[4].visibilityCVar, windows[4].name, ImVec2(400, 540));
    windows[5].window = std::make_shared<OoTActiveGated<EntranceTracker::EntranceTrackerWindow>>(
        windows[5].visibilityCVar, windows[5].name, ImVec2(500, 750));
    windows[6].window = std::make_shared<OoTActiveGated<ItemTrackerWindow>>(windows[6].visibilityCVar, windows[6].name,
                                                                            ImVec2(350, 600));
    windows[7].window =
        std::make_shared<OoTActiveGated<TimeSplitWindow>>(windows[7].visibilityCVar, windows[7].name, ImVec2(450, 660));

    for (const GatedWindow& w : windows) {
        if (!w.window->IsVisible()) {
            printf("[TEST] FAIL: %s is not visible after its CVar was forced on: the tripwire would be vacuous\n",
                   w.name);
            return 1;
        }
    }

    const GameId prevGame = Context_GetCurrentGame();

    // ---- 1. The predicate ------------------------------------------------------------------------------------------
    Context_SetCurrentGame(GAME_OOT);
    const bool drawsUnderOoT = OoT_Gui_ShouldDraw();
    Context_SetCurrentGame(GAME_MM);
    const bool drawsUnderMM = OoT_Gui_ShouldDraw();
    Context_SetCurrentGame(GAME_NONE);
    const bool drawsUnderNone = OoT_Gui_ShouldDraw();
    Context_SetCurrentGame(prevGame);
    OWG_ASSERT(drawsUnderOoT, "OoT_Gui_ShouldDraw() false while OoT is the running game");
    OWG_ASSERT(!drawsUnderMM, "OoT_Gui_ShouldDraw() true while Majora's Mask is the running game");
    OWG_ASSERT(!drawsUnderNone, "OoT_Gui_ShouldDraw() true with no game running (GAME_NONE must be excluded)");

    // ---- 2. The wrapper, on a counting spy: each path blocked unless OoT runs, each path passed through when it does
    // -
    int spyFailures = 0;
    spyFailures += RunSpy(GAME_OOT, "Ocarina of Time", true);
    spyFailures += RunSpy(GAME_MM, "Majora's Mask", false);
    spyFailures += RunSpy(GAME_NONE, "no game", false);
    Context_SetCurrentGame(prevGame);
    CVarClear(OWG_SPY_CVAR);
    OWG_ASSERT(spyFailures == 0,
               "OoTActiveGated<> lets a body run under a non-OoT game or blocks one under OoT (above)");

    // ---- 3. The wired gate on the eight real windows: all three paths, every window ---------------------------------
    const GameId inactiveGames[] = { GAME_MM, GAME_NONE };
    for (GameId game : inactiveGames) {
        Context_SetCurrentGame(game);
        for (const GatedWindow& w : windows) {
            // One line per window before its calls, so a RED run names the window and path that faulted.
            printf("[TEST] oot-windows-gate: %s running, %s: DrawElement() (menu embed), Update(), Draw()\n",
                   game == GAME_MM ? "Majora's Mask" : "no game", w.name);
            fflush(stdout);
            w.window->DrawElement();
            w.window->Update();
            w.window->Draw();
        }
    }
    Context_SetCurrentGame(prevGame);

    // ---- 4. Dormant, not hidden ------------------------------------------------------------------------------------
    // A gated window must come back when OoT resumes, so the gate may not Hide() it or touch its CVar.
    for (const GatedWindow& w : windows) {
        if (!w.window->IsVisible() || CVarGetInteger(w.visibilityCVar, 0) != 1) {
            printf("[TEST] FAIL: %s lost its visibility while gated: it would not reappear when OoT resumes\n", w.name);
            return 1;
        }
    }

    for (const GatedWindow& w : windows) {
        CVarClear(w.visibilityCVar);
    }

    printf("[TEST] PASS: oot-windows-gate: the eight OoT save/play-state windows run no Draw, Update or menu-embed "
           "DrawElement body unless OoT is the running game, the wrapper passes all three through under OoT, and the "
           "windows stay visible while gated\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
