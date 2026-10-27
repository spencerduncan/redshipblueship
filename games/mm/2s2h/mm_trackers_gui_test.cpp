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
 *
 * 4. BOOT INDEPENDENCE (#535, leg 6 below). rsbs/src/main.cpp registers them at
 *    startup so the Randomizer > Cross-Game rows that name them resolve in a
 *    default OoT-first session; that is only sound while RegisterWindows needs
 *    nothing of MM's boot. Registration with OoT active and mm.o2r unmounted
 *    must still land all four names and leave the windows inert. Pins the
 *    premise, not the call site — MM_TrackersGui_Init needs a Ship::Window,
 *    which this harness deliberately does not have.
 *
 * 5. IN-GAME CLOSE (#897, leg 7 below). Combo > Windows lost MM's tracker rows
 *    with #458 U6, but a config that still holds gWindows.CheckTracker=1 or
 *    gWindows.ItemTracker=1 opens MM's native tracker every MM frame, and the
 *    vendored windows pass no close flag to ImGui::Begin. Leg 7 drives real
 *    ImGui frames (the font atlas built in memory, no renderer): the old key
 *    opens the Check Tracker, a right-click on it offers Close, and Close
 *    clears the key so the window stays shut; setting the key again reopens
 *    it (the U6 ruling: the keys still open the native windows).
 *
 * 6. THE TIMER WINDOWS (W6-TIMERS, #1235/#1239, leg 8 below). MM's In-Game
 *    Timer and Time Splits list register beside the trackers under "MM "
 *    names, and the list draws only while MM is the running game.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>
#include <cstring>
#include <memory>

#include <ship/Context.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>

#include "TrackersGuiSingleExe.h"
#include "TimersGuiSingleExe.h"
#include "2s2h/Rando/CheckTracker/CheckTracker.h"
#include "2s2h/Enhancements/Trackers/ItemTracker/ItemTracker.h"
#include "2s2h/Enhancements/Trackers/ItemTracker/ItemTrackerSettings.h"
#include "2s2h/Rando/StaticData/StaticData.h" // CheckNames / Checks
#include "2s2h/ShipUtils.h"                   // convertEnumToReadableName
#include "context.h"

#include <imgui.h>
#include <imgui_internal.h> // FindWindowByName, OpenPopupStack: leg 7 reads ImGui's own window state

// games/mm/2s2h/GameExports_SingleExe.cpp — true once LoadMMArchives mounted
// mm.o2r. Read here only to keep the archive-free half of leg 6 honest.
extern "C" bool MM_Rando_AssetsReady(void);

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

    // ---- 4. VISIBILITY: the ctor's CVar latch must not be the last word ----
    // #489 cause 1. Ship::GuiWindow reads its visibility CVar exactly once, in
    // the constructor (libultraship GuiWindow.cpp:13-15), and
    // SyncVisibilityConsoleVariable only ever writes mIsVisible -> CVar, never
    // the reverse. In the single exe nothing calls Show() on these windows —
    // BenMenu, upstream's only route to it, is link-elided — so without a
    // CVar -> visibility sync three of the four MM tracker windows can never be
    // opened at all after MM's first boot.
    //
    // This drives the REAL production path: S2H::TrackersGui::RegisterWindows
    // constructing the real window types through the real Ship::GuiWindow ctor
    // on a fresh Gui, with the visibility CVars CLEARED — the inverse of leg 1
    // above, which forces them on before construction.
    for (const char* cvar : kMMWindowVisibilityCVars) {
        CVarClear(cvar);
    }
    auto visGui = std::make_shared<Ship::Gui>();
    S2H::TrackersGui::RegisterWindows(visGui);

    for (const char* name : S2H::TrackersGui::kAllTrackerWindowNames) {
        TGT_ASSERT(!MM_TrackersGui_ShouldShow(name), "window reports drawable with its visibility CVar cleared");
    }
    TGT_ASSERT(!MM_TrackersGui_ShouldShow("MM Nonexistent Window"), "unknown window name reported drawable");
    TGT_ASSERT(!MM_TrackersGui_ShouldShow(nullptr), "null window name reported drawable");

    // The latch is REAL: setting the CVar after construction changes nothing on
    // its own. Asserting this first is what keeps the leg below from going
    // vacuous — if libultraship ever starts re-syncing per frame, this fails
    // loudly rather than silently making the sync untested.
    CVarSetInteger(S2H::TrackersGui::kItemTrackerVisibilityCVar, 1);
    TGT_ASSERT(!MM_TrackersGui_ShouldShow(S2H::TrackersGui::kItemTrackerWindowName),
               "ctor visibility latch is gone — this leg no longer pins #489 cause 1");

    // ...and the production sync is what makes the window openable. RED before
    // SyncVisibilityFromCVars existed: IsVisible() stayed false forever.
    S2H::TrackersGui::SyncVisibilityFromCVars();
    TGT_ASSERT(MM_TrackersGui_ShouldShow(S2H::TrackersGui::kItemTrackerWindowName),
               "item tracker still not drawable after the CVar->visibility sync (#489 cause 1)");
    // Only the window whose CVar was set: the sync must not open all four.
    TGT_ASSERT(!MM_TrackersGui_ShouldShow(S2H::TrackersGui::kCheckTrackerSettingsWindowName),
               "sync opened a window whose visibility CVar was still cleared");
    TGT_ASSERT(!MM_TrackersGui_ShouldShow(S2H::TrackersGui::kItemTrackerSettingsWindowName),
               "sync opened a window whose visibility CVar was still cleared");

    // Reversible: the settings windows are otherwise unreachable by any route,
    // so closing has to work through the same seam as opening.
    CVarSetInteger(S2H::TrackersGui::kItemTrackerSettingsVisibilityCVar, 1);
    CVarClear(S2H::TrackersGui::kItemTrackerVisibilityCVar);
    S2H::TrackersGui::SyncVisibilityFromCVars();
    TGT_ASSERT(MM_TrackersGui_ShouldShow(S2H::TrackersGui::kItemTrackerSettingsWindowName),
               "item tracker settings window did not open from its CVar");
    TGT_ASSERT(!MM_TrackersGui_ShouldShow(S2H::TrackersGui::kItemTrackerWindowName),
               "clearing the CVar did not close the item tracker");

    // Surviving all of the above is itself an assertion: Show()/Hide() route
    // through GuiWindow::SetVisibility -> SyncVisibilityConsoleVariable, which
    // dereferences Context::GetWindow()->GetGui() when the stored CVar and the
    // new visibility disagree. This harness has NO window, so a sync that ever
    // drove visibility AWAY from the CVar's own value would null-deref here.

    // ---- 5. CheckNames is populated in single-exe builds ------------------
    // #489 cause 2. Rando::StaticData::CheckNames is RC_MAX empty strings
    // (Checks.cpp:11) and PopulateCheckNames' only upstream caller is
    // BenPort.cpp:874, which games/mm/CMakeLists.txt:238 excludes — so every
    // check-tracker row rendered a blank label. RegisterWindows now calls it.
    // This drives the REAL PopulateCheckNames over the REAL StaticData::Checks
    // map; RED before that call site existed.
    {
        const RandoCheckId kProbe = RC_CLOCK_TOWER_ROOF_OCARINA;
        const auto& probeCheck = Rando::StaticData::Checks.at(kProbe);
        TGT_ASSERT(!Rando::StaticData::CheckNames[kProbe].empty(),
                   "CheckNames entry is still empty — PopulateCheckNames never ran (#489 cause 2)");
        TGT_ASSERT(Rando::StaticData::CheckNames[kProbe] == convertEnumToReadableName(probeCheck.name),
                   "CheckNames entry is not its convertEnumToReadableName form");
        // Not just one entry: the whole map was walked.
        size_t namedCount = 0;
        for (const auto& [checkId, staticCheck] : Rando::StaticData::Checks) {
            if (!Rando::StaticData::CheckNames[checkId].empty()) {
                namedCount++;
            }
        }
        TGT_ASSERT(namedCount >= Rando::StaticData::Checks.size() - 1,
                   "PopulateCheckNames left named checks behind (only RC_UNKNOWN may be empty)");
    }

    // ---- 6. Registration does not depend on MM having booted -------------
    // #535. The Randomizer > Cross-Game rows resolve these windows by name on
    // every frame they are drawn, and rsbs/src/main.cpp registers them at
    // startup — before MM_Rando_Init has run, while OoT is the active game and
    // with MM's archives unmounted. That call site is only legitimate as long
    // as RegisterWindows stays independent of MM boot state, which is what this
    // leg pins: fresh Gui, OoT active, no mm.o2r, all four names must still
    // resolve, and the windows must still be inert.
    //
    // Scope honesty: this pins the PREMISE, not the call site. The call site is
    // unreachable ROM-free — MM_TrackersGui_Init needs a Ship::Window, and the
    // assertion at the top of this test is that the harness has none.
    {
        TGT_ASSERT(!MM_Rando_AssetsReady(),
                   "harness unexpectedly has mm.o2r — the archive-free half of this leg would be vacuous");

        // Forced ON before construction, as in leg 1: an ungated Draw() must
        // not be able to early-out on !IsVisible() before reaching ImGui.
        for (const char* cvar : kMMWindowVisibilityCVars) {
            CVarSetInteger(cvar, 1);
        }

        auto preBootGui = std::make_shared<Ship::Gui>();
        for (const char* name : S2H::TrackersGui::kAllTrackerWindowNames) {
            TGT_ASSERT(preBootGui->GetGuiWindow(name) == nullptr, "fresh Gui already carries an MM tracker window");
        }

        // Every early return below this point restores the active game first —
        // this leg is not the last one to read it.
        const GameId prevPreBootGame = Context_GetCurrentGame();
        Context_SetCurrentGame(GAME_OOT);

        S2H::TrackersGui::RegisterWindows(preBootGui);

        for (const char* name : S2H::TrackersGui::kAllTrackerWindowNames) {
            if (preBootGui->GetGuiWindow(name) == nullptr) {
                printf("[TEST] FAIL: %s unresolvable with OoT active and MM unbooted — the menu row that names "
                       "it would draw nothing (#535) (%s:%d)\n",
                       name, __FILE__, __LINE__);
                Context_SetCurrentGame(prevPreBootGame);
                return 1;
            }
        }

        // Same hard tripwire as leg 2, on the windows the startup path would
        // have produced: no ImGui context, visibility forced on, OoT active.
        for (const char* name : S2H::TrackersGui::kAllTrackerWindowNames) {
            auto window = preBootGui->GetGuiWindow(name);
            window->Draw();
            window->Update();
        }

        Context_SetCurrentGame(prevPreBootGame);
    }

    // ---- 7. A tracker an old config left open closes in game ---------------
    // #897. The four Combo > Windows rows that wrote gWindows.* went with #458
    // U6, and the U6 ruling keeps those keys opening MM's native windows. Both
    // trackers call ImGui::Begin with a null p_open (CheckTracker.cpp,
    // ItemTracker.cpp), so ImGui draws no close button, and MMActiveGated::Draw
    // re-syncs visibility from the key every frame. Without an in-game close a
    // config that still says gWindows.CheckTracker=1 shows the tracker for
    // good. This drives real ImGui frames over the REAL wrapped Check Tracker:
    // the old key opens it, a right-click on it must offer Close, Close must
    // clear the key and keep the window shut, and the key must still reopen it.
    // The Item Tracker shares the wrapper path; it is not driven here because
    // its Draw needs MM play state and archive textures.
    {
        TGT_ASSERT(ImGui::GetCurrentContext() == nullptr,
                   "harness already has an ImGui context: legs 2 and 6 above would not be hard tripwires");

        auto closeGui = std::make_shared<Ship::Gui>();
        S2H::TrackersGui::RegisterWindows(closeGui);
        std::shared_ptr<Ship::GuiWindow> checkTracker =
            closeGui->GetGuiWindow(S2H::TrackersGui::kCheckTrackerWindowName);
        TGT_ASSERT(checkTracker != nullptr, "MM Check Tracker missing from a fresh Gui");

        // CheckTracker.cpp's single-exe ImGui id for the window (#392).
        const char* kCheckTrackerImGuiName = "Check Tracker (MM)";

        // One pass per way an old config can leave the tracker on screen:
        // floating, and docked (LUS persists the dock layout in imgui.ini, and
        // the vendored Check Tracker opens without NoDocking). A docked window
        // is begun inside its dock host and carries ImGuiWindowFlags_ChildWindow,
        // so the docked pass is what catches a Close menu that skips it.
        auto runPass = [&](bool docked) -> const char* {
            for (const char* cvar : kMMWindowVisibilityCVars) {
                CVarClear(cvar);
            }
            // The old config: only the key a retired Combo > Windows row wrote.
            CVarSetInteger(S2H::TrackersGui::kCheckTrackerVisibilityCVar, 1);

            ImGuiContext* imgui = ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.LogFilename = nullptr;
            io.DisplaySize = ImVec2(1280.0f, 800.0f);
            io.DeltaTime = 1.0f / 60.0f;
            if (docked) {
                io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
            }
            io.Fonts->AddFontDefault();
            unsigned char* atlasPixels = nullptr;
            int atlasW = 0;
            int atlasH = 0;
            io.Fonts->GetTexDataAsRGBA32(&atlasPixels, &atlasW, &atlasH); // builds the atlas; no renderer needs it

            const GameId prevCloseGame = Context_GetCurrentGame();
            Context_SetCurrentGame(GAME_MM);

            bool dockRequested = false;
            auto drawFrame = [&]() {
                ImGui::NewFrame();
                if (docked) {
                    // LUS begins its main dockspace host before any GuiWindow
                    // draws; this mirrors that order.
                    const ImGuiID dockspace = ImGui::DockSpaceOverViewport();
                    if (!dockRequested) {
                        // What an imgui.ini that holds a docked tracker restores.
                        ImGui::DockBuilderDockWindow(kCheckTrackerImGuiName, dockspace);
                        dockRequested = true;
                    }
                }
                checkTracker->Draw();
                ImGui::Render();
            };
            auto drawnThisFrame = [&]() -> ImGuiWindow* {
                ImGuiWindow* window = ImGui::FindWindowByName(kCheckTrackerImGuiName);
                return (window != nullptr && window->LastFrameActive == ImGui::GetFrameCount()) ? window : nullptr;
            };

            const char* failure = [&]() -> const char* {
                drawFrame();
                drawFrame();
                ImGuiWindow* tracker = drawnThisFrame();
                if (tracker == nullptr) {
                    return "gWindows.CheckTracker=1 did not open MM's Check Tracker: the U6 ruling (the keys still "
                           "open the native windows) no longer holds";
                }
                if (docked && (!tracker->DockIsActive || (tracker->Flags & ImGuiWindowFlags_ChildWindow) == 0)) {
                    return "the docked pass did not dock MM's Check Tracker, so it would not test the docked case";
                }

                // Right-click the middle of the tracker (its "No Rando Save
                // Loaded" pane: no MM play state here).
                io.AddMousePosEvent(tracker->Pos.x + tracker->Size.x * 0.5f, tracker->Pos.y + tracker->Size.y * 0.5f);
                drawFrame();
                io.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
                drawFrame();
                io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
                drawFrame();
                drawFrame(); // a popup's first frame is hidden while it sizes itself
                drawFrame();

                ImGuiContext& g = *ImGui::GetCurrentContext();
                if (g.OpenPopupStack.empty() || g.OpenPopupStack.back().Window == nullptr) {
                    return "right-clicking MM's Check Tracker opened no menu: nothing in the game closes a tracker "
                           "an old config left open (#897)";
                }
                ImGuiWindow* menu = g.OpenPopupStack.back().Window;

                // Left-click the menu's first (only) item.
                const ImGuiStyle& style = ImGui::GetStyle();
                io.AddMousePosEvent(menu->Pos.x + style.WindowPadding.x + 4.0f,
                                    menu->Pos.y + style.WindowPadding.y + ImGui::GetFontSize() * 0.5f);
                drawFrame();
                io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
                drawFrame();
                io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
                drawFrame();

                if (CVarGetInteger(S2H::TrackersGui::kCheckTrackerVisibilityCVar, 0) != 0) {
                    return "choosing Close left gWindows.CheckTracker set: the per-frame re-sync reopens the "
                           "tracker (#897)";
                }
                if (MM_TrackersGui_ShouldShow(S2H::TrackersGui::kCheckTrackerWindowName)) {
                    return "choosing Close left MM's Check Tracker visible (#897)";
                }
                drawFrame();
                S2H::TrackersGui::SyncVisibilityFromCVars();
                drawFrame();
                if (drawnThisFrame() != nullptr) {
                    return "MM's Check Tracker is still drawn two frames after Close (#897)";
                }

                // The key still opens the window: Close is a player action,
                // not a retirement of the fallback.
                CVarSetInteger(S2H::TrackersGui::kCheckTrackerVisibilityCVar, 1);
                drawFrame();
                if (drawnThisFrame() == nullptr) {
                    return "setting gWindows.CheckTracker again after Close did not reopen MM's Check Tracker";
                }
                return nullptr;
            }();

            Context_SetCurrentGame(prevCloseGame);
            ImGui::DestroyContext(imgui);
            return failure;
        };

        for (const bool docked : { false, true }) {
            const char* failure = runPass(docked);
            if (failure != nullptr) {
                printf("[TEST] FAIL: %s pass: %s (%s:%d)\n", docked ? "docked" : "floating", failure, __FILE__,
                       __LINE__);
                for (const char* cvar : kMMWindowVisibilityCVars) {
                    CVarClear(cvar);
                }
                return 1;
            }
            printf("[TEST] leg 7 (%s): gWindows.CheckTracker=1 opens MM's Check Tracker; right-click > Close clears "
                   "the key and the window stays shut; the key reopens it (#897)\n",
                   docked ? "docked" : "floating");
        }
    }

    // ---- 8. THE IN-GAME TIMER AND THE TIME SPLITS LIST (W6-TIMERS) ------
    // #1235, #1239. Upstream 2S2H creates both windows in the excluded
    // BenGui.cpp, so the single exe had neither. RegisterWindows must register
    // them beside the trackers, under "MM "-prefixed names, and gate them on MM
    // being the running game. The list's gate is observed through ImGui: with
    // Time Splits enabled its Draw() begins its window while MM runs and never
    // while OoT runs. The timer shares the wrapper; its own Draw() needs MM play
    // state and archive textures, so under OoT it is driven as a tripwire only.
    {
        TGT_ASSERT(ImGui::GetCurrentContext() == nullptr, "leg 7 left an ImGui context behind");
        auto timersGui = std::make_shared<Ship::Gui>();
        S2H::TrackersGui::RegisterWindows(timersGui);
        std::shared_ptr<Ship::GuiWindow> timer = timersGui->GetGuiWindow(S2H::TimersGui::kInGameTimerWindowName);
        std::shared_ptr<Ship::GuiWindow> splits = timersGui->GetGuiWindow(S2H::TimersGui::kTimeSplitsWindowName);
        TGT_ASSERT(timer != nullptr, "MM's In-Game Timer is not registered on the Gui: nothing in the single exe "
                                     "creates DisplayOverlay.cpp's window (#1235)");
        TGT_ASSERT(splits != nullptr, "MM's Time Splits list is not registered on the Gui: nothing in the single exe "
                                      "creates Timesplits.cpp's window (#1239)");
        // SoH's own timer windows (SohGui.cpp) as well as its trackers.
        const char* sohTimerNames[] = { "Time Splits",     "Additional Timers", kSohOwnedNames[0],
                                        kSohOwnedNames[1], kSohOwnedNames[2],   kSohOwnedNames[3] };
        for (const char* sohName : sohTimerNames) {
            TGT_ASSERT(strcmp(sohName, S2H::TimersGui::kInGameTimerWindowName) != 0 &&
                           strcmp(sohName, S2H::TimersGui::kTimeSplitsWindowName) != 0,
                       "an MM timer window shares a SoH-owned Gui name");
        }

        CVarSetInteger("gSettings.TimeSplits.Enable", 1);
        CVarSetInteger("gDisplayOverlay.Mode", 1);
        ImGuiContext* imgui = ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = ImVec2(1280.0f, 800.0f);
        io.DeltaTime = 1.0f / 60.0f;
        io.Fonts->AddFontDefault();
        unsigned char* atlasPixels = nullptr;
        int atlasW = 0;
        int atlasH = 0;
        io.Fonts->GetTexDataAsRGBA32(&atlasPixels, &atlasW, &atlasH);

        const GameId prevTimersGame = Context_GetCurrentGame();
        auto listBegunUnder = [&](GameId game, bool drawTimer) -> bool {
            Context_SetCurrentGame(game);
            ImGui::NewFrame();
            if (drawTimer) {
                timer->Draw();
                timer->Update();
            }
            splits->Draw();
            splits->Update();
            ImGui::Render();
            ImGuiWindow* list = ImGui::FindWindowByName(S2H::TimersGui::kTimeSplitsImGuiName);
            return list != nullptr && list->LastFrameActive == ImGui::GetFrameCount();
        };
        const bool underOoT = listBegunUnder(GAME_OOT, true);
        const bool underNone = listBegunUnder(GAME_NONE, true);
        const bool underMM = listBegunUnder(GAME_MM, false);
        ImGuiWindow* overlay = ImGui::FindWindowByName(S2H::TimersGui::kInGameTimerImGuiName);
        const bool timerBegun = overlay != nullptr;

        Context_SetCurrentGame(prevTimersGame);
        ImGui::DestroyContext(imgui);
        CVarClear("gSettings.TimeSplits.Enable");
        CVarClear("gDisplayOverlay.Mode");

        TGT_ASSERT(!underOoT && !underNone, "MM's Time Splits list drew while MM was not the running game: its window "
                                            "is not gated on MM (it would read OoT's save as MM's)");
        TGT_ASSERT(!timerBegun, "MM's In-Game Timer began its window while MM was not the running game");
        TGT_ASSERT(underMM, "MM's Time Splits list did not draw while MM runs with Time Splits enabled");
        printf("[TEST] leg 8: MM's In-Game Timer and Time Splits list register beside the trackers and draw only while "
               "MM is the running game (#1235, #1239)\n");
    }

    // Cleanup: don't leak forced-visible tracker CVars into later tests or a
    // saved cvar file.
    for (const char* cvar : kMMWindowVisibilityCVars) {
        CVarClear(cvar);
    }

    printf("[TEST] PASS: mm-trackers-gui — four MM tracker windows register under de-collided names beside "
           "SoH's without MM having booted, their draw path is inert unless MM is the active game, their "
           "visibility tracks the live CVar, CheckNames is populated, and a tracker an old config opened closes "
           "from its right-click menu\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
