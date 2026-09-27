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
 * 5. THE combo_ui SEAM IS SoH's (UI parity M6). The pane draws through
 *    src/common/combo_ui.h; the shipped binary must have SoH's table installed
 *    (games/oot/soh/SohGui/ComboUiSoh.cpp's file-scope initializer), or the
 *    pane silently falls back to raw, unthemed ImGui. The disabled-tooltip
 *    composer must build SoH's shape exactly as Menu::MenuDrawItem does, the
 *    shown-tooltip rule must match UIWidgets' (a disabled widget shows its
 *    disabled tooltip), no player-facing reason may carry a tracker number or an
 *    ADR reference, every reason the pane prints is a Title Case fragment in
 *    SoH's disabledMap style (R-S2), and the pane's own draw TUs still name no gSettings.Menu.*
 *    key (ADR 0004 §4.1a(ii): a common-owned window is not a second menu shell).
 *
 * Deliberately absent: any assertion about appearance. That is judged from
 * pixels, by the UiSnapshot row's captures of this pane (docs/ui-style-guide.md
 * section 12), not by a headless test.
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
#include "../combo_mm_tricks_view.h"
#include "../combo_ui.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>
#include <libultraship/bridge/consolevariablebridge.h>

#define CMOW_ASSERT(cond)                                                  \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return TEST_FAIL;                                              \
        }                                                                  \
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

    // ---- The combo_ui seam (UI parity M6) ----------------------------------
    {
        // SoH's table, not the raw-ImGui fallback, is what the shipped binary
        // draws this pane with. Falsifiable: drop ComboUiSoh.cpp's initializer
        // (or the TU from the link) and this goes red.
        CMOW_ASSERT(ComboUi_IsInstalled());
        CMOW_ASSERT(ComboUi_Get() != NULL);

        // SoH's disabled shape (a), exactly as Menu::MenuDrawItem builds it from
        // its disabledMap: the head, a blank line, then "- <Reason>" per reason.
        CMOW_ASSERT(strcmp(ComboUi_DisabledTooltip("Already Decided", NULL),
                           "This setting is disabled because: \n\n- Already Decided") == 0);
        CMOW_ASSERT(strcmp(ComboUi_DisabledTooltip("Already Decided", "Retired"),
                           "This setting is disabled because: \n\n- Already Decided\n- Retired") == 0);
        CMOW_ASSERT(ComboUi_DisabledTooltip(NULL, "")[0] == '\0');
        // Stored once per composition, so the pointer a widget holds stays valid.
        CMOW_ASSERT(ComboUi_DisabledTooltip("Already Decided", NULL) ==
                    ComboUi_DisabledTooltip(NULL, "Already Decided"));

        // UIWidgets' rule: a disabled widget shows its disabled tooltip, an
        // enabled one its description.
        ComboUiWidgetOpts o;
        o.tooltip = "description";
        o.disabled = true;
        o.disabledTooltip = "reason";
        CMOW_ASSERT(strcmp(ComboUi_ShownTooltip(&o), "reason") == 0);
        o.disabled = false;
        CMOW_ASSERT(strcmp(ComboUi_ShownTooltip(&o), "description") == 0);
        o.tooltip = "";
        CMOW_ASSERT(ComboUi_ShownTooltip(&o) == NULL);

        // No player-facing text in either table carries a tracker number or an
        // ADR reference (docs/ui-style-guide.md R-N4, R-TT5): the pane prints
        // every reason in a disabled tooltip, where a player reads it.
        auto cites = [](const char* text) {
            if (text == NULL) {
                return false;
            }
            for (const char* c = text; *c != '\0'; c++) {
                if (c[0] == '#' && c[1] >= '0' && c[1] <= '9') {
                    return true;
                }
                if (strncmp(c, "ADR ", 4) == 0) {
                    return true;
                }
            }
            return false;
        };
        for (int i = 0; i < Combo_MMOptionCount(); i++) {
            const ComboMMOptionDesc* d = Combo_MMOptionAt(i);
            CMOW_ASSERT(d != NULL);
            if (cites(d->label) || cites(d->tooltip) || cites(d->disabledReason)) {
                printf("[TEST] FAIL: MM option %s cites a tracker or ADR in player-facing text\n", d->name);
                return TEST_FAIL;
            }
        }
        for (int i = 0; i < Combo_MMTrickCount(); i++) {
            const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
            CMOW_ASSERT(d != NULL);
            if (cites(d->disabledReason)) {
                printf("[TEST] FAIL: MM trick %s cites a tracker or ADR in its disabled reason\n", d->name);
                return TEST_FAIL;
            }
        }

        // Every reason the pane prints in a disabled tooltip is a fragment in
        // SoH's disabledMap style (docs/ui-style-guide.md R-S2: "Save Not
        // Loaded", "Debug Mode is Disabled", "Not Available on DirectX"), so a
        // frozen and retired row does not stack a Title Case fragment over a
        // sentence: it starts upper case, every word of four or more letters
        // is capitalised, and it carries no colon and no closing punctuation.
        // The reserved tricks' reasons (StaticData/Tricks.cpp) are the Tricks
        // section's and are migrated with it (UI parity M6 part 2).
        auto fragment = [](const char* text) {
            if (text == NULL || text[0] == '\0') {
                return true;
            }
            if (!(text[0] >= 'A' && text[0] <= 'Z')) {
                return false;
            }
            const size_t n = strlen(text);
            if (text[n - 1] == '.' || text[n - 1] == '!' || text[n - 1] == '?' || strchr(text, ':') != NULL) {
                return false;
            }
            for (const char* w = text; *w != '\0';) {
                while (*w == ' ') {
                    w++;
                }
                const char* e = w;
                while (*e != '\0' && *e != ' ') {
                    e++;
                }
                if (e - w >= 4 && *w >= 'a' && *w <= 'z') {
                    return false;
                }
                w = e;
            }
            return true;
        };
        CMOW_ASSERT(fragment("Already Decided"));
        for (int i = 0; i < Combo_MMOptionCount(); i++) {
            const ComboMMOptionDesc* d = Combo_MMOptionAt(i);
            if (!fragment(d->disabledReason)) {
                printf("[TEST] FAIL: MM option %s's disabled reason '%s' is not a Title Case fragment (R-S2)\n",
                       d->name, d->disabledReason);
                return TEST_FAIL;
            }
        }
        for (int i = 0; i < Combo_MMTrickCount(); i++) {
            const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
            if (!d->reserved && !fragment(d->disabledReason)) {
                printf("[TEST] FAIL: MM trick %s's disabled reason '%s' is not a Title Case fragment (R-S2)\n",
                       d->name, d->disabledReason);
                return TEST_FAIL;
            }
        }

#ifdef RSBS_SOURCE_DIR
        // The pane reads no menu-index key: its draw TUs name no gSettings.Menu.*
        // key (the theme reaches it through ComboUiSoh.cpp, from the live menu's
        // own cached index, not a CVar read here).
        for (const char* rel : { "src/common/ComboMmOptionsWindow.cpp", "src/common/combo_ui.cpp" }) {
            std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + rel, std::ios::binary);
            CMOW_ASSERT(in.good());
            const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            CMOW_ASSERT(!text.empty());
            CMOW_ASSERT(text.find("gSettings.Menu") == std::string::npos);
            CMOW_ASSERT(text.find("CVAR_SETTING(\"Menu") == std::string::npos);
        }
#endif
    }

    // Leave global state clean for any subsequent test.
    CVarClear(ComboGui::kComboMMOptionsVisibilityCVar);
    ComboContext_Init();

    printf("[TEST] PASS: the MM options pane registers de-collided and idempotently beside both games' windows and "
           "the combo spoiler, its draw path is inert under GAME_OOT/GAME_MM/GAME_NONE, and it draws through SoH's "
           "combo_ui table\n");
    return TEST_PASS;
}
