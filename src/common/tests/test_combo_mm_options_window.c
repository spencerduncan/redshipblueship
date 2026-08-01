/**
 * @file test_combo_mm_options_window.c
 * @brief ROM-free lock for the MM randomizer options WINDOW (#497 step 4).
 *
 * The table has its own lock, MM-side (games/mm/2s2h/mm_rando_options_test.cpp),
 * because that is the only place both option tables are in scope. This one
 * covers the window that renders it, in the shape of test_combo_spoiler_window.c:
 *
 * 1. REGISTRATION + IDEMPOTENCE + NAME DE-COLLISION. The pane lands on a bare
 *    Ship::Gui under kComboMMOptionsWindowName, a second registration is a
 *    no-op, and stand-ins already holding a SoH tracker name, an MM tracker
 *    name and the sibling combo spoiler's name are undisturbed.
 *    Gui::AddGuiWindow rejects duplicates SILENTLY from the caller's side, so a
 *    collision would present only as a window that never appears.
 *
 * 2. GAME-AGNOSTICISM — the hard tripwire. ADR 0008 rule 5's claim is that a
 *    common-owned window reads gComboCtx and never gSaveContext, which is what
 *    lets it draw under GAME_OOT, GAME_MM and GAME_NONE alike. This harness has
 *    NO ImGui context, so any Draw() that reaches ImGui::Begin aborts the test
 *    process. Draw()/Update() are called under all three GameIds with the
 *    visibility CVar CLEARED, in both paired and unpaired worlds.
 *
 *    That matters more here than for the spoiler: this pane deliberately READS
 *    the active game (to draw the "Majora's Mask - suspended" state, ADR 0004
 *    §6), and reading the active game is one step away from reading that game's
 *    save. The tripwire is what keeps the distinction enforced rather than
 *    merely intended.
 *
 * 3. THE PRODUCTION ENTRY POINT IS HEADLESS-SAFE and populates the model.
 *    Combo_MMOptionsWindow_Init() must return cleanly with no window on the
 *    shared context AND must still have published MM's descriptor table — the
 *    table registration is deliberately NOT gated on the Gui existing, because
 *    a test or a headless run has every reason to read the options and none to
 *    draw them.
 *
 * 4. THE FROZEN-PROFILE WRITE GATE (#498/#564 V5). Once a creation event has
 *    stamped gComboCtx.mmProfileDigest, the two src/common option writers
 *    (Combo_MMOptionSetValue / Combo_MMOptionClear) must REJECT — the profile
 *    is world identity, and a post-creation edit would diverge the next MM
 *    arrival from the creation stamp. Locked at the writers rather than the
 *    widgets because the writers are the gate; the pane is just its face.
 *    Falsifiable: remove the Combo_MMProfileFrozen() check from either writer
 *    and its half goes red.
 *
 * Deliberately absent: any assertion about appearance. Whether the grouping
 * reads well, whether the disabled rows' reasons are legible beside their
 * widgets, and whether the three standing warnings land are operator
 * verification; no headless test can stand in for them.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++)
 * — it drives the C++-linkage ComboGui::RegisterComboMmOptionsWindow.
 *
 * Entry point is a RunHeadless bridge, not a TestResult body, for the same
 * reason the spoiler window's is: constructing a Ship::GuiWindow reads
 * ConsoleVariables off the Ship::Context singleton, so it needs the display-free
 * shared bring-up that lives (static) in test_runner.cpp.
 */

#include "../ComboMmOptionsWindow.h"
#include "../ComboSpoilerWindow.h"
#include "../combo_mm_options_view.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <memory>

#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>
#include <libultraship/bridge/consolevariablebridge.h>

#define CMOW_ASSERT(cond)                                                                                              \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            return TEST_FAIL;                                                                                          \
        }                                                                                                              \
    } while (0)

namespace {

// Inert stand-in, matching test_combo_spoiler_window.c's: empty cvar (no
// ConsoleVariables traffic in the ctor) and no-op overrides.
class MMOptionsStandinWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override {
    }
    void DrawElement() override {
    }
    void UpdateElement() override {
    }
};

const char* const kMMOptionsNeighbourNames[] = {
    "Check Tracker",    // SoH-owned
    "MM Check Tracker", // MM-owned
};

} // namespace

extern "C" int Combo_MMOptionsWindow_RunHeadless(void) {
    printf("[TEST] combo-mm-options-window: the common-owned MM options pane registers de-collided, publishes its "
           "table headlessly, and is inert under every active game (#497, ADR 0008)\n");

    // Production entry point must be headless-safe: with no window on the
    // shared context it returns without touching a Gui — while still having
    // published MM's descriptor table.
    Combo_MMOptionsWindow_Init();
    CMOW_ASSERT(Combo_MMOptionCount() > 0);
    CMOW_ASSERT(Combo_MMOptionAt(0) != NULL);
    CMOW_ASSERT(Combo_MMOptionAt(Combo_MMOptionCount()) == NULL);

    auto gui = std::make_shared<Ship::Gui>();

    std::shared_ptr<Ship::GuiWindow> neighbours[2];
    for (int i = 0; i < 2; i++) {
        neighbours[i] = std::make_shared<MMOptionsStandinWindow>("", kMMOptionsNeighbourNames[i]);
        gui->AddGuiWindow(neighbours[i]);
    }

    // The sibling common-owned window: both live in the same unprefixed name
    // space, so a collision between the two is the one this file is uniquely
    // placed to catch.
    ComboGui::RegisterComboSpoilerWindow(gui);
    auto spoiler = gui->GetGuiWindow(ComboGui::kComboSpoilerWindowName);
    CMOW_ASSERT(spoiler != nullptr);

    // Keep the pane shut for every Draw() below. The live-CVar early-out is the
    // only thing between Draw() and ImGui::Begin in a process with no ImGui
    // context, which is what makes the tripwire survivable.
    CVarClear(ComboGui::kComboMMOptionsVisibilityCVar);

    ComboGui::RegisterComboMmOptionsWindow(gui);
    auto window = gui->GetGuiWindow(ComboGui::kComboMMOptionsWindowName);
    CMOW_ASSERT(window != nullptr);
    CMOW_ASSERT(window != spoiler);

    // Idempotence: a second registration must not replace the instance.
    ComboGui::RegisterComboMmOptionsWindow(gui);
    CMOW_ASSERT(gui->GetGuiWindow(ComboGui::kComboMMOptionsWindowName) == window);

    // De-collision: neither game's window names were disturbed, the sibling
    // combo window still resolves to itself, and the pane took none of them.
    for (int i = 0; i < 2; i++) {
        CMOW_ASSERT(gui->GetGuiWindow(kMMOptionsNeighbourNames[i]) == neighbours[i]);
        CMOW_ASSERT(gui->GetGuiWindow(kMMOptionsNeighbourNames[i]) != window);
    }
    CMOW_ASSERT(gui->GetGuiWindow(ComboGui::kComboSpoilerWindowName) == spoiler);

    // The two common-owned windows must not share a visibility CVar either —
    // that would make one un-openable without the other.
    CMOW_ASSERT(strcmp(ComboGui::kComboMMOptionsVisibilityCVar, ComboGui::kComboSpoilerVisibilityCVar) != 0);

    // ---- Game-agnosticism tripwire ---------------------------------------
    const GameId prevGame = Context_GetCurrentGame();
    const GameId allGames[] = { GAME_OOT, GAME_MM, GAME_NONE };

    for (int paired = 0; paired <= 1; paired++) {
        ComboContext_Init();
        if (paired) {
            gComboCtx.sourceIsRando = true;
            gComboCtx.sharedRandoSeed = 0xC0FFEE97u;
            gComboCtx.sharedRandoSettingsHash = 0x5EED0497u;
            gComboCtx.mmProfileDigest = 0x4D4D0001u;
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

    // ---- Frozen-profile write gate (#498/#564 V5) -------------------------
    {
        // Any checkbox row will do: 0/1 are always legal values for it, so a
        // rejected write is distinguishable from a clamped one.
        const ComboMMOptionDesc* checkbox = NULL;
        for (int i = 0; i < Combo_MMOptionCount(); i++) {
            const ComboMMOptionDesc* d = Combo_MMOptionAt(i);
            if (d != NULL && d->widget == COMBO_MM_WIDGET_CHECKBOX) {
                checkbox = d;
                break;
            }
        }
        CMOW_ASSERT(checkbox != NULL);

        // Unfrozen (digest 0): the authoring window. Writes land.
        ComboContext_Init();
        CMOW_ASSERT(!Combo_MMProfileFrozen());
        Combo_MMOptionClear(checkbox);
        Combo_MMOptionSetValue(checkbox, 1);
        CMOW_ASSERT(Combo_MMOptionGetValue(checkbox) == 1);
        CMOW_ASSERT(Combo_MMOptionIsExplicit(checkbox));

        // Frozen (a creation stamped the identity): both writers reject, and
        // the CVar state is byte-for-byte what it was before the attempts.
        gComboCtx.mmProfileDigest = 0x4D4D0001u;
        CMOW_ASSERT(Combo_MMProfileFrozen());
        Combo_MMOptionSetValue(checkbox, 0);
        CMOW_ASSERT(Combo_MMOptionGetValue(checkbox) == 1);
        Combo_MMOptionClear(checkbox);
        CMOW_ASSERT(Combo_MMOptionIsExplicit(checkbox));
        CMOW_ASSERT(Combo_MMOptionGetValue(checkbox) == 1);

        // Unfrozen again (the pair's identity was dropped): authoring resumes.
        gComboCtx.mmProfileDigest = 0;
        CMOW_ASSERT(!Combo_MMProfileFrozen());
        Combo_MMOptionClear(checkbox);
        CMOW_ASSERT(!Combo_MMOptionIsExplicit(checkbox));
    }

    // Leave global state clean for any subsequent test.
    CVarClear(ComboGui::kComboMMOptionsVisibilityCVar);
    ComboContext_Init();

    printf("[TEST] PASS: the MM options pane registers de-collided and idempotently beside both games' windows and "
           "the combo spoiler, and its draw path is inert under GAME_OOT/GAME_MM/GAME_NONE\n");
    return TEST_PASS;
}
