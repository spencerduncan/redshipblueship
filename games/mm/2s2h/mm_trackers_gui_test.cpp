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
#include "2s2h/Rando/StaticData/StaticData.h" // CheckNames / Checks
#include "2s2h/ShipUtils.h"                   // convertEnumToReadableName
#include "context.h"

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

    // Cleanup: don't leak forced-visible tracker CVars into later tests or a
    // saved cvar file.
    for (const char* cvar : kMMWindowVisibilityCVars) {
        CVarClear(cvar);
    }

    printf("[TEST] PASS: mm-trackers-gui — four MM tracker windows register under de-collided names beside "
           "SoH's without MM having booted, their draw path is inert unless MM is the active game, their "
           "visibility tracks the live CVar, and CheckNames is populated\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
