/**
 * @file test_combo_item_tracker_window.c
 * @brief ROM-free lock for the unified Item Tracker overlay (#458 U2; ADR 0008).
 *
 * The item view and both adapters have their own lock (test_combo_item_view.c).
 * This one covers the overlay that draws them, in the shape of
 * test_combo_tracker_window.c:
 *
 * 1. REGISTRATION + IDEMPOTENCE + NAME DE-COLLISION. The overlay lands on a bare
 *    Ship::Gui under kComboItemTrackerWindowName, a second registration is a
 *    no-op, and stand-ins holding SoH's "Item Tracker", MM's "MM Item Tracker"
 *    and the Combo Tracker's name are undisturbed.
 *
 * 2. GAME-AGNOSTICISM, the hard tripwire. This harness has NO ImGui context, so
 *    any Draw() that reaches ImGui::Begin aborts the process. Draw()/Update()
 *    run under GAME_OOT, GAME_MM and GAME_NONE, with no data and with both
 *    games' shadows authored, with the visibility key CLEARED.
 *
 * 3. THE SHOWONLYPAUSED GATE. With the visibility key SET, the overlay floating
 *    and ShowOnlyPaused on, an unpaused game must keep Draw() off ImGui: under
 *    every GameId the process survives, so the gate (not visibility) is what
 *    stopped it. Only the ACTIVE game's pause probe is asked (a counted test
 *    probe), and none under GAME_NONE. Turning ShowOnlyPaused off, or the pause
 *    probe answering true, would reach ImGui and abort, so those halves are
 *    held by the pure ComboItemTrackerShows decision in point 5 instead.
 *
 * 4. THE MODEL READS THE DRAW MAKES. The open draw path is
 *    ComboItemTrackerCollectSection per shown section, so the lock drives
 *    exactly that over: no data (every section "No data." with no rows); OoT
 *    live from a test live source with MM's shadow and the shared pool
 *    authored (OoT "Updated live." with the live world's rows, MM from its
 *    snapshot with its label, the pool "As of the last switch or save."); MM
 *    active (OoT is never live, its rows are its snapshot's). Row text is
 *    asserted as printed ("Bow 25/40"), held and not held.
 *
 * 5. THE CHROME AND GRID DECISIONS, as the pure functions the draw calls:
 *    BeginFloatingWindows' flags for both window types and both Draggable
 *    values, SoH's show rule, the row text, and a grid where each group starts a
 *    new line. Section keys default to shown.
 *
 * 6. DRAWN THROUGH THE SEAM, as far as a source scan holds it, and the seam's
 *    new Image entry is installed and falls back (draws nothing) without a
 *    texture.
 *
 * Appearance is judged from the UiSnapshot captures (window/Combo Item Tracker
 * @no-data, @live and @snapshot beside SoH's window/Item Tracker).
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++);
 * it drives the C++-linkage ComboGui functions.
 */

#include "../ComboItemTrackerWindow.h"
#include "../ComboTrackerWindow.h" // Combo_TrackerWindow_Init: the production registration
#include "../combo_item_view.h"
#include "../combo_ui.h"
#include "../context.h"
#include "../game.h"
#include "../shared_resources.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" int OoT_ItemAdapter_TestAuthorSave(void* buf, size_t size, int variant);
extern "C" int MM_ItemAdapter_TestAuthorSave(void* buf, size_t size, int variant);

#define CITW_ASSERT(cond)                                                                                              \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            return TEST_FAIL;                                                                                          \
        }                                                                                                              \
    } while (0)

namespace {

class ItemTrackerStandinWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override {
    }
    void DrawElement() override {
    }
    void UpdateElement() override {
    }
};

const char* const kItemTrackerNeighbourNames[] = {
    "Item Tracker",    // SoH-owned
    "MM Item Tracker", // MM-owned
    "Combo Tracker",   // the sibling common-owned window
};
constexpr int kItemTrackerNeighbourCount = 3;

// ---- test live sources and pause probes ------------------------------------
const void* sCitwOoTLive = nullptr;
const void* sCitwMMLive = nullptr;
int sCitwOoTPausedCalls = 0;
int sCitwMMPausedCalls = 0;

const void* CitwOoTLive(void) {
    return sCitwOoTLive;
}
const void* CitwMMLive(void) {
    return sCitwMMLive;
}
bool CitwOoTPaused(void) {
    sCitwOoTPausedCalls++;
    return false;
}
bool CitwMMPaused(void) {
    sCitwMMPausedCalls++;
    return false;
}

const char* const kCitwOverlayKeys[] = {
    RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE,   RSBS_CVAR_COMBO_ITEMS_DRAGGABLE,    RSBS_CVAR_COMBO_ITEMS_SHOW_ONLY_PAUSED,
    RSBS_CVAR_COMBO_ITEMS_SECTION_OOT,   RSBS_CVAR_COMBO_ITEMS_SECTION_MM,   RSBS_CVAR_COMBO_ITEMS_SECTION_SHARED,
    ComboGui::kComboItemTrackerVisibilityCVar,
};

void CitwClearOverlayKeys(void) {
    for (const char* key : kCitwOverlayKeys) {
        CVarClear(key);
    }
}

bool CitwSectionHasText(const ComboGui::ComboItemTrackerSection& section, const char* text, bool have) {
    for (const ComboItemRow& row : section.rows) {
        if (ComboGui::ComboItemRowText(row) == text) {
            return row.have == have;
        }
    }
    return false;
}

/**
 * The reads DrawElement makes, for every section, with the contract checks a
 * crash would not catch: a note that is a sentence, no rows without data, every
 * row labelled and carrying its section's freshness.
 */
bool CitwDriveSections(ComboGui::ComboItemTrackerSection out[ComboGui::COMBO_ITEM_SECTION_COUNT]) {
    for (int s = 0; s < ComboGui::COMBO_ITEM_SECTION_COUNT; s++) {
        ComboGui::ComboItemTrackerCollectSection(s, out[s]);
        const ComboGui::ComboItemTrackerSection& section = out[s];
        if (section.id != s || section.title == nullptr || section.title[0] == '\0') {
            return false;
        }
        if (section.note.size() < 2 || section.note.back() != '.') {
            return false;
        }
        if (section.freshness == COMBO_TRACKER_FRESH_UNAVAILABLE &&
            (!section.rows.empty() || section.note != "No data.")) {
            return false; // "no data" is not an empty inventory
        }
        for (const ComboItemRow& row : section.rows) {
            if (row.group == nullptr || row.name == nullptr || row.freshness != section.freshness) {
                return false;
            }
        }
    }
    return true;
}

int CitwLockDecisions(void) {
    using namespace ComboGui;

    // BeginFloatingWindows' flags (randomizer_item_tracker.cpp).
    const int base = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoResize;
    const int floating = base | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoTitleBar |
                         ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar;
    const int fixedInputs = ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
    CITW_ASSERT(ComboItemTrackerWindowFlags(ComboGui::COMBO_ITEM_TRACKER_FLOATING, false) == (floating | fixedInputs));
    CITW_ASSERT(ComboItemTrackerWindowFlags(ComboGui::COMBO_ITEM_TRACKER_FLOATING, true) == floating);
    CITW_ASSERT(ComboItemTrackerWindowFlags(ComboGui::COMBO_ITEM_TRACKER_WINDOW, false) == base);
    CITW_ASSERT(ComboItemTrackerWindowFlags(ComboGui::COMBO_ITEM_TRACKER_WINDOW, true) == base);

    // SoH's show rule: a window always; floating unless only-paused and unpaused.
    CITW_ASSERT(ComboItemTrackerShows(ComboGui::COMBO_ITEM_TRACKER_FLOATING, false, false));
    CITW_ASSERT(!ComboItemTrackerShows(ComboGui::COMBO_ITEM_TRACKER_FLOATING, true, false));
    CITW_ASSERT(ComboItemTrackerShows(ComboGui::COMBO_ITEM_TRACKER_FLOATING, true, true));
    CITW_ASSERT(ComboItemTrackerShows(ComboGui::COMBO_ITEM_TRACKER_WINDOW, true, false));

    // Row text.
    ComboItemRow row = {};
    row.group = "Inventory";
    row.name = "Fairy Bow";
    CITW_ASSERT(ComboItemRowText(row) == "Fairy Bow");
    row.max = 8; // neither held nor counted: the name alone, whatever the ceiling
    CITW_ASSERT(ComboItemRowText(row) == "Fairy Bow");
    row.count = 35; // counted though not held (a pool of pieces): the amount shows
    row.max = 40;
    CITW_ASSERT(ComboItemRowText(row) == "Fairy Bow 35/40");
    row.have = true;
    CITW_ASSERT(ComboItemRowText(row) == "Fairy Bow 35/40");
    row.max = 0;
    CITW_ASSERT(ComboItemRowText(row) == "Fairy Bow 35");
    row.count = 0;
    row.max = 3;
    CITW_ASSERT(ComboItemRowText(row) == "Fairy Bow 0/3"); // held at tier 0 (the shared wallet)
    row.max = 0;
    CITW_ASSERT(ComboItemRowText(row) == "Fairy Bow");

    // The grid: three columns, a new group starts a new line.
    std::vector<ComboItemRow> rows;
    const char* groups[] = { "A", "A", "A", "A", "B", "C", "C" };
    for (const char* g : groups) {
        ComboItemRow r = {};
        r.group = g;
        r.name = g;
        rows.push_back(r);
    }
    std::vector<ComboItemGridCell> cells;
    ComboItemGridLayout(rows, 3, cells);
    const ComboItemGridCell want[] = { { 0, 0 }, { 0, 1 }, { 0, 2 }, { 1, 0 }, { 2, 0 }, { 3, 0 }, { 3, 1 } };
    CITW_ASSERT(cells.size() == rows.size());
    for (size_t i = 0; i < cells.size(); i++) {
        if (cells[i].line != want[i].line || cells[i].column != want[i].column) {
            printf("[TEST] FAIL: grid cell %zu is line %d column %d, expected line %d column %d\n", i, cells[i].line,
                   cells[i].column, want[i].line, want[i].column);
            return TEST_FAIL;
        }
    }
    ComboItemGridLayout(rows, 0, cells); // a nonsense column count is one column
    CITW_ASSERT(cells.size() == rows.size() && cells.back().line == 6 && cells.back().column == 0);

    // Section keys: shown by default, hidden when cleared to 0.
    for (int s = 0; s < ComboGui::COMBO_ITEM_SECTION_COUNT; s++) {
        CITW_ASSERT(ComboItemTrackerSectionShown(s));
    }
    CVarSetInteger(RSBS_CVAR_COMBO_ITEMS_SECTION_MM, 0);
    CITW_ASSERT(!ComboItemTrackerSectionShown(ComboGui::COMBO_ITEM_SECTION_MM));
    CITW_ASSERT(ComboItemTrackerSectionShown(ComboGui::COMBO_ITEM_SECTION_OOT));
    CVarClear(RSBS_CVAR_COMBO_ITEMS_SECTION_MM);
    CITW_ASSERT(!ComboItemTrackerSectionShown(ComboGui::COMBO_ITEM_SECTION_COUNT));
    printf("[TEST] combo-item-tracker-window: chrome flags, show rule, row text, grid and section keys PASS\n");
    return TEST_PASS;
}

/** The grid's height and width as the fit models them (DrawElement draws exactly this grid). */
float CitwFitHeight(const std::vector<ComboGui::ComboItemTrackerSection>& sections, const ComboGui::ComboItemTrackerFit& fit,
                    const ComboGui::ComboItemTrackerMetrics& m) {
    int lines = 0;
    for (const ComboGui::ComboItemTrackerSection& section : sections) {
        lines += ComboGui::ComboItemGridLines(section.rows, fit.columns);
    }
    return m.overhead + (float)lines * (m.textHeight * fit.scale + m.linePadding);
}

float CitwFitWidth(const ComboGui::ComboItemTrackerFit& fit, const ComboGui::ComboItemTrackerMetrics& m) {
    float w = (float)(fit.columns - 1) * m.columnGap;
    for (float c : fit.columnWidths) {
        w += c * fit.scale;
    }
    return w;
}

/**
 * The floating overlay's fit (the #458 U2 review: at 832x600 the three-column
 * grid ran 824 px past the game window's bottom edge, where a player can never
 * scroll). Over the real rows, with widths of 11 px a character and the
 * profile metrics the UiSnapshot captures measure (20 px text, 4 px spacing).
 */
int CitwLockFit(const std::vector<ComboGui::ComboItemTrackerSection>& sections) {
    using namespace ComboGui;
    std::vector<std::vector<float>> widths(sections.size());
    for (size_t s = 0; s < sections.size(); s++) {
        CITW_ASSERT(!sections[s].rows.empty());
        for (const ComboItemRow& row : sections[s].rows) {
            widths[s].push_back(11.0f * (float)ComboItemRowText(row).size());
        }
    }

    // Shared column widths: column c is the widest row ANY section puts there.
    {
        std::vector<ComboItemTrackerSection> two(2);
        const float a[] = { 10.0f, 50.0f, 20.0f };
        const float b[] = { 40.0f, 5.0f, 30.0f };
        std::vector<std::vector<float>> w(2);
        for (int i = 0; i < 3; i++) {
            ComboItemRow r = {};
            r.group = "g";
            r.name = "x";
            two[0].rows.push_back(r);
            two[1].rows.push_back(r);
            w[0].push_back(a[i]);
            w[1].push_back(b[i]);
        }
        const std::vector<float> cols = ComboItemGridColumnWidths(two, w, 3);
        CITW_ASSERT(cols.size() == 3 && cols[0] == 40.0f && cols[1] == 50.0f && cols[2] == 30.0f);
        const std::vector<float> one = ComboItemGridColumnWidths(two, w, 1);
        CITW_ASSERT(one.size() == 1 && one[0] == 50.0f);
    }

    ComboItemTrackerMetrics m;
    m.textHeight = 20.0f;
    m.linePadding = 4.0f;
    m.columnGap = 16.0f;
    m.overhead = 200.0f;

    // Room to spare: the window type's three columns at full size.
    m.availWidth = 10000.0f;
    m.availHeight = 10000.0f;
    ComboItemTrackerFit fit = ComboItemTrackerFitGrid(sections, widths, m);
    CITW_ASSERT(fit.columns == kComboItemTrackerColumns && fit.scale == 1.0f);
    CITW_ASSERT(fit.columnWidths == ComboItemGridColumnWidths(sections, widths, kComboItemTrackerColumns));

    // The two capture profiles' room (832x600 and 1280x800, less the overlay's
    // 60 px position, its padding and the margin): three columns at full size
    // do not fit (the defect), the fit does, inside both edges.
    const float rooms[2][2] = { { 748.0f, 520.0f }, { 1196.0f, 720.0f } };
    for (const auto& room : rooms) {
        m.availWidth = room[0];
        m.availHeight = room[1];
        ComboItemTrackerFit three;
        three.columns = kComboItemTrackerColumns;
        three.columnWidths = ComboItemGridColumnWidths(sections, widths, three.columns);
        CITW_ASSERT(CitwFitHeight(sections, three, m) > m.availHeight);
        fit = ComboItemTrackerFitGrid(sections, widths, m);
        const float h = CitwFitHeight(sections, fit, m);
        const float w = CitwFitWidth(fit, m);
        printf("[TEST] combo-item-tracker-window: fit in %.0fx%.0f: %d columns at scale %.2f, grid %.0fx%.0f "
               "(three columns at full size: %.0f px tall)\n",
               room[0], room[1], fit.columns, fit.scale, w, h, CitwFitHeight(sections, three, m));
        CITW_ASSERT(fit.scale > kComboItemTrackerMinScale && fit.scale < 1.0f);
        CITW_ASSERT(fit.columns > kComboItemTrackerColumns && fit.columns <= kComboItemTrackerMaxColumns);
        CITW_ASSERT(h <= m.availHeight + 0.01f);
        CITW_ASSERT(w <= m.availWidth + 0.01f);
        // Whole steps, and the largest whole step that fits: one more does not.
        const float steps = fit.scale / kComboItemTrackerScaleStep;
        CITW_ASSERT(steps == (float)(int)steps);
        ComboItemTrackerFit bigger = fit;
        bigger.scale += kComboItemTrackerScaleStep;
        CITW_ASSERT(CitwFitHeight(sections, bigger, m) > m.availHeight || CitwFitWidth(bigger, m) > m.availWidth);

        // Hysteresis. The fit drawn last frame is kept as it is.
        ComboItemTrackerFit again = ComboItemTrackerFitGrid(sections, widths, m, &fit);
        CITW_ASSERT(again.columns == fit.columns && again.scale == fit.scale);
        // A grid two steps smaller last frame grows back to the fit.
        ComboItemTrackerFit smaller = fit;
        smaller.scale -= 2.0f * kComboItemTrackerScaleStep;
        again = ComboItemTrackerFitGrid(sections, widths, m, &smaller);
        CITW_ASSERT(again.columns == fit.columns && again.scale == fit.scale);
        // A step down is taken at once: a grid too large last frame shrinks to fit.
        again = ComboItemTrackerFitGrid(sections, widths, m, &bigger);
        CITW_ASSERT(again.columns == fit.columns && again.scale == fit.scale);
        // A column count that no longer fits at all is dropped.
        ComboItemTrackerFit narrow;
        narrow.columns = 1;
        narrow.scale = 1.0f;
        again = ComboItemTrackerFitGrid(sections, widths, m, &narrow);
        CITW_ASSERT(again.columns == fit.columns && again.scale == fit.scale);
    }

    // A step up waits until it fits by half a step more. One 100 px row in a
    // room that fits it at 0.5 and a quarter step: the fit is 0.5; with 0.5
    // less a step last frame it stays there (the quarter step is inside the
    // hysteresis), and with the room for three quarters more it steps up.
    {
        std::vector<ComboItemTrackerSection> one(1);
        ComboItemRow r = {};
        r.group = "g";
        r.name = "x";
        one[0].rows.push_back(r);
        const std::vector<std::vector<float>> w = { { 100.0f } };
        ComboItemTrackerMetrics flat;
        flat.textHeight = 20.0f;
        flat.availHeight = 10000.0f;
        flat.availWidth = 100.0f * (0.5f + 0.25f * kComboItemTrackerScaleStep);
        ComboItemTrackerFit lower;
        lower.columns = kComboItemTrackerColumns;
        lower.scale = 0.5f - kComboItemTrackerScaleStep;
        CITW_ASSERT(ComboItemTrackerFitGrid(one, w, flat).scale == 0.5f);
        CITW_ASSERT(ComboItemTrackerFitGrid(one, w, flat, &lower).scale == lower.scale);
        flat.availWidth = 100.0f * (0.5f + 0.75f * kComboItemTrackerScaleStep);
        CITW_ASSERT(ComboItemTrackerFitGrid(one, w, flat, &lower).scale == 0.5f);
    }

    // No room at all: the floor, never below it.
    m.availWidth = 100.0f;
    m.availHeight = 100.0f;
    fit = ComboItemTrackerFitGrid(sections, widths, m);
    CITW_ASSERT(fit.scale == kComboItemTrackerMinScale);
    printf("[TEST] combo-item-tracker-window: shared column widths and the floating fit PASS\n");
    return TEST_PASS;
}

/** Points 2-4 over the registered production adapters `oot` / `mm`. */
int CitwLockOverlay(std::shared_ptr<Ship::GuiWindow> window, const ComboItemOps* oot, const ComboItemOps* mm) {
    using namespace ComboGui;
    const GameId allGames[] = { GAME_OOT, GAME_MM, GAME_NONE };

    std::vector<uint8_t> ootZeros((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    std::vector<uint8_t> mmZeros((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    std::vector<uint8_t> ootShadow((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    std::vector<uint8_t> ootLive((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    std::vector<uint8_t> mmShadow((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    std::vector<uint8_t> mmLive((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    CITW_ASSERT(OoT_ItemAdapter_TestAuthorSave(ootShadow.data(), ootShadow.size(), 0) == 1);
    CITW_ASSERT(OoT_ItemAdapter_TestAuthorSave(ootLive.data(), ootLive.size(), 1) == 1);
    CITW_ASSERT(MM_ItemAdapter_TestAuthorSave(mmShadow.data(), mmShadow.size(), 0) == 1);
    CITW_ASSERT(MM_ItemAdapter_TestAuthorSave(mmLive.data(), mmLive.size(), 1) == 1);

    // Test vtables: the production adapters with a test live source and a
    // counted pause probe.
    static ComboItemOps sOoT;
    static ComboItemOps sMM;
    sOoT = *oot;
    sOoT.liveSave = CitwOoTLive;
    sOoT.paused = CitwOoTPaused;
    sMM = *mm;
    sMM.liveSave = CitwMMLive;
    sMM.paused = CitwMMPaused;
    Combo_Item_RegisterOps((uint8_t)GAME_OOT, &sOoT);
    Combo_Item_RegisterOps((uint8_t)GAME_MM, &sMM);

    ComboItemTrackerSection sections[ComboGui::COMBO_ITEM_SECTION_COUNT];

    // ---- 2. inert while shut, no data and authored -----------------------
    for (int authored = 0; authored < 2; authored++) {
        ComboContext_Init();
        Combo_ResetSharedResourceWatermarks();
        Context_UpdateShadowCopy(GAME_OOT, authored ? ootShadow.data() : ootZeros.data(), ootZeros.size());
        Context_UpdateShadowCopy(GAME_MM, authored ? mmShadow.data() : mmZeros.data(), mmZeros.size());
        sCitwOoTLive = authored ? ootLive.data() : nullptr;
        sCitwMMLive = authored ? mmLive.data() : nullptr;
        CVarClear(kComboItemTrackerVisibilityCVar);
        for (GameId game : allGames) {
            Context_SetCurrentGame(game);
            // Surviving IS the assertion: an ungated path reaches ImGui::Begin
            // with no ImGui context and aborts the process.
            window->Draw();
            window->Update();
            CITW_ASSERT(CitwDriveSections(sections));
            if (!authored) {
                for (const ComboItemTrackerSection& section : sections) {
                    CITW_ASSERT(section.freshness == COMBO_TRACKER_FRESH_UNAVAILABLE);
                }
            }
        }
    }

    // ---- 3. the ShowOnlyPaused gate --------------------------------------
    // Visible, floating, only-paused, and no game paused: Draw() must stop
    // before ImGui under every GameId, and ask only the active game's probe.
    CVarSetInteger(kComboItemTrackerVisibilityCVar, 1);
    CVarSetInteger(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE, ComboGui::COMBO_ITEM_TRACKER_FLOATING);
    CVarSetInteger(RSBS_CVAR_COMBO_ITEMS_SHOW_ONLY_PAUSED, 1);
    for (GameId game : allGames) {
        Context_SetCurrentGame(game);
        sCitwOoTPausedCalls = 0;
        sCitwMMPausedCalls = 0;
        window->Draw();
        window->Update();
        printf("[TEST] combo-item-tracker-window: visible, only-paused, unpaused under %s: drew nothing; pause "
               "probes asked OoT=%d MM=%d\n",
               game == GAME_OOT ? "GAME_OOT" : (game == GAME_MM ? "GAME_MM" : "GAME_NONE"), sCitwOoTPausedCalls,
               sCitwMMPausedCalls);
        CITW_ASSERT(sCitwOoTPausedCalls == (game == GAME_OOT ? 1 : 0));
        CITW_ASSERT(sCitwMMPausedCalls == (game == GAME_MM ? 1 : 0));
        CITW_ASSERT(!Combo_ItemActiveGamePaused());
    }
    // The production probes answer "not paused" with no play state loaded.
    Combo_Item_RegisterOps((uint8_t)GAME_OOT, oot);
    Combo_Item_RegisterOps((uint8_t)GAME_MM, mm);
    CITW_ASSERT(oot->paused != nullptr && mm->paused != nullptr);
    CITW_ASSERT(!oot->paused() && !mm->paused());
    for (GameId game : allGames) {
        Context_SetCurrentGame(game);
        window->Draw();
        CITW_ASSERT(!Combo_ItemActiveGamePaused());
    }
    Combo_Item_RegisterOps((uint8_t)GAME_OOT, &sOoT);
    Combo_Item_RegisterOps((uint8_t)GAME_MM, &sMM);
    CitwClearOverlayKeys();

    // ---- 4. the model reads, with data -----------------------------------
    // OoT played live; MM and the pool from their snapshots.
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_QUIVER_TIER, 2);
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_ARROW_COUNT, 33);
    Context_UpdateShadowCopy(GAME_OOT, ootShadow.data(), ootShadow.size());
    Context_UpdateShadowCopy(GAME_MM, mmShadow.data(), mmShadow.size());
    sCitwOoTLive = ootLive.data();
    sCitwMMLive = mmLive.data();
    Context_SetCurrentGame(GAME_OOT);
    CITW_ASSERT(CitwDriveSections(sections));
    const ComboItemTrackerSection& ootLiveSection = sections[ComboGui::COMBO_ITEM_SECTION_OOT];
    const ComboItemTrackerSection& mmSnap = sections[ComboGui::COMBO_ITEM_SECTION_MM];
    const ComboItemTrackerSection& shared = sections[ComboGui::COMBO_ITEM_SECTION_SHARED];
    printf("[TEST] combo-item-tracker-window: under GAME_OOT: \"%s\" %zu rows \"%s\"; \"%s\" %zu rows \"%s\"; \"%s\" "
           "%zu rows \"%s\"\n",
           ootLiveSection.title, ootLiveSection.rows.size(), ootLiveSection.note.c_str(), mmSnap.title,
           mmSnap.rows.size(), mmSnap.note.c_str(), shared.title, shared.rows.size(), shared.note.c_str());
    CITW_ASSERT(strcmp(ootLiveSection.title, "Ocarina of Time") == 0);
    CITW_ASSERT(ootLiveSection.freshness == COMBO_TRACKER_FRESH_LIVE && ootLiveSection.note == "Updated live.");
    CITW_ASSERT((int)ootLiveSection.rows.size() == oot->count());
    CITW_ASSERT(CitwSectionHasText(ootLiveSection, "Hookshot", true));     // the live world (variant 1)
    CITW_ASSERT(CitwSectionHasText(ootLiveSection, "Song of Time", true));  // the live world
    CITW_ASSERT(CitwSectionHasText(ootLiveSection, "Fairy Bow", false));    // not held: no count printed
    CITW_ASSERT(!CitwSectionHasText(ootLiveSection, "Longshot", true));     // the shadow's, not shown
    CITW_ASSERT(strcmp(mmSnap.title, "Majora's Mask") == 0);
    CITW_ASSERT(mmSnap.freshness == COMBO_TRACKER_FRESH_STALE);
    CITW_ASSERT(mmSnap.note ==
                std::string(Combo_ItemFreshnessLabel((uint8_t)GAME_MM, COMBO_TRACKER_FRESH_STALE)) + ".");
    CITW_ASSERT((int)mmSnap.rows.size() == mm->count());
    CITW_ASSERT(CitwSectionHasText(mmSnap, "Bow 25/40", true));
    CITW_ASSERT(CitwSectionHasText(mmSnap, "Magic Bean 7/20", true));
    CITW_ASSERT(CitwSectionHasText(mmSnap, "Zora Mask", false)); // the live world's, not the snapshot's
    CITW_ASSERT(strcmp(shared.title, "Shared") == 0);
    CITW_ASSERT(shared.freshness == COMBO_TRACKER_FRESH_STALE && shared.note == "As of the last switch or save.");
    CITW_ASSERT(CitwSectionHasText(shared, "Arrows 33/40", true));

    // MM played live: OoT is never live, its rows are its snapshot's.
    Context_SetCurrentGame(GAME_MM);
    CITW_ASSERT(CitwDriveSections(sections));
    CITW_ASSERT(sections[ComboGui::COMBO_ITEM_SECTION_OOT].freshness == COMBO_TRACKER_FRESH_STALE);
    CITW_ASSERT(sections[ComboGui::COMBO_ITEM_SECTION_OOT].note == "As of the last game switch or save.");
    CITW_ASSERT(CitwSectionHasText(sections[ComboGui::COMBO_ITEM_SECTION_OOT], "Longshot", true));
    CITW_ASSERT(CitwSectionHasText(sections[ComboGui::COMBO_ITEM_SECTION_OOT], "Fairy Bow 35/40", true));
    CITW_ASSERT(sections[ComboGui::COMBO_ITEM_SECTION_MM].freshness == COMBO_TRACKER_FRESH_LIVE);
    CITW_ASSERT(CitwSectionHasText(sections[ComboGui::COMBO_ITEM_SECTION_MM], "Zora Mask", true));

    // No game: both snapshots.
    Context_SetCurrentGame(GAME_NONE);
    CITW_ASSERT(CitwDriveSections(sections));
    CITW_ASSERT(sections[ComboGui::COMBO_ITEM_SECTION_OOT].freshness == COMBO_TRACKER_FRESH_STALE);
    CITW_ASSERT(sections[ComboGui::COMBO_ITEM_SECTION_MM].freshness == COMBO_TRACKER_FRESH_STALE);
    printf("[TEST] combo-item-tracker-window: model reads PASS (OoT live / MM live / no game)\n");

    // ---- 5b. the fit, over the real rows ----------------------------------
    std::vector<ComboItemTrackerSection> shown(sections, sections + ComboGui::COMBO_ITEM_SECTION_COUNT);
    return CitwLockFit(shown);
}

} // namespace

extern "C" int Combo_ItemTrackerWindow_RunHeadless(void) {
    printf("[TEST] combo-item-tracker-window: the unified Item Tracker overlay registers de-collided, is inert under "
           "every game and while unpaused, and draws the item view (#458 U2)\n");

    // The production bring-up registers both item adapters (headless: no Gui).
    Combo_TrackerWindow_Init();
    const ComboItemOps* oot = Combo_Item_GetOps((uint8_t)GAME_OOT);
    const ComboItemOps* mm = Combo_Item_GetOps((uint8_t)GAME_MM);
    CITW_ASSERT(oot != nullptr && mm != nullptr);

    // ---- snapshot what this row touches -----------------------------------
    const GameId prevGame = Context_GetCurrentGame();
    std::vector<uint8_t> ootBackup((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    if (const uint8_t* p = (const uint8_t*)Context_GetOoTSaveContext()) {
        memcpy(ootBackup.data(), p, ootBackup.size());
    }
    std::vector<uint8_t> mmBackup((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    if (const uint8_t* p = (const uint8_t*)Context_GetMMSaveContext()) {
        memcpy(mmBackup.data(), p, mmBackup.size());
    }
    static ComboContext sCtxBackup;
    sCtxBackup = gComboCtx;
    CitwClearOverlayKeys();

    // ---- 1. registration --------------------------------------------------
    auto gui = std::make_shared<Ship::Gui>();
    std::shared_ptr<Ship::GuiWindow> neighbours[kItemTrackerNeighbourCount];
    for (int i = 0; i < kItemTrackerNeighbourCount; i++) {
        neighbours[i] = std::make_shared<ItemTrackerStandinWindow>("", kItemTrackerNeighbourNames[i]);
        gui->AddGuiWindow(neighbours[i]);
    }
    ComboGui::RegisterComboItemTrackerWindow(gui);
    auto window = gui->GetGuiWindow(ComboGui::kComboItemTrackerWindowName);
    CITW_ASSERT(window != nullptr);
    ComboGui::RegisterComboItemTrackerWindow(gui);
    CITW_ASSERT(gui->GetGuiWindow(ComboGui::kComboItemTrackerWindowName) == window);
    for (int i = 0; i < kItemTrackerNeighbourCount; i++) {
        CITW_ASSERT(gui->GetGuiWindow(kItemTrackerNeighbourNames[i]) == neighbours[i]);
        CITW_ASSERT(neighbours[i] != window);
    }
    ComboGui::RegisterComboItemTrackerWindow(nullptr); // a null Gui is a no-op

    // ---- 2-4 ---------------------------------------------------------------
    const int overlay = CitwLockOverlay(window, oot, mm);

    // ---- put state back ----------------------------------------------------
    sCitwOoTLive = nullptr;
    sCitwMMLive = nullptr;
    Combo_Item_RegisterOps((uint8_t)GAME_OOT, oot);
    Combo_Item_RegisterOps((uint8_t)GAME_MM, mm);
    Context_UpdateShadowCopy(GAME_OOT, ootBackup.data(), ootBackup.size());
    Context_UpdateShadowCopy(GAME_MM, mmBackup.data(), mmBackup.size());
    gComboCtx = sCtxBackup;
    Combo_ResetSharedResourceWatermarks();
    Context_SetCurrentGame(prevGame);
    CitwClearOverlayKeys();
    if (overlay != TEST_PASS) {
        return TEST_FAIL;
    }

    // ---- 5. decisions --------------------------------------------------------
    if (CitwLockDecisions() != TEST_PASS) {
        return TEST_FAIL;
    }

    // ---- 6. the seam -----------------------------------------------------------
    // The Image entry is installed (SoH's table) and, with no texture under a
    // key (no Gui in this harness), draws nothing and says so, so a caller
    // falls back to the row's text.
    CITW_ASSERT(ComboUi_IsInstalled());
    CITW_ASSERT(ComboUi_Get()->Image != nullptr);
    CITW_ASSERT(!ComboUi_Get()->Image("ITEM_LONGSHOT", 32.0f, 32.0f));
    CITW_ASSERT(!ComboUi_Get()->Image(nullptr, 32.0f, 32.0f));

#ifdef RSBS_SOURCE_DIR
    {
        auto slurp = [](const char* rel) {
            std::ifstream in(std::string(RSBS_SOURCE_DIR) + rel, std::ios::binary);
            return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        };
        const std::string text = slurp("/src/common/ComboItemTrackerWindow.cpp");
        CITW_ASSERT(!text.empty());
        // The draw reads exactly what point 4 drove, and lays out what point 5 holds.
        CITW_ASSERT(text.find("ComboItemTrackerCollectSection(s, sections.back())") != std::string::npos);
        CITW_ASSERT(text.find("ComboItemGridLayout(section.rows, fit.columns, cells)") != std::string::npos);
        // The floating overlay is fitted, and drawn at the fit's scale.
        CITW_ASSERT(text.find("fit = ComboItemTrackerFitGrid(sections, widths, metrics, &previous)") !=
                    std::string::npos);
        CITW_ASSERT(text.find("ImGui::SetWindowFontScale(fit.scale)") != std::string::npos);
        CITW_ASSERT(text.find("ComboItemRowText(row)") != std::string::npos);
        CITW_ASSERT(text.find("ComboItemTrackerWindowFlags(windowType, draggable)") != std::string::npos);
        CITW_ASSERT(text.find("ComboItemTrackerShows(windowType, showOnlyPaused,") != std::string::npos);
        // Headers and notes through the seam; a row not held dimmed, not coloured.
        CITW_ASSERT(text.find("Ui().SeparatorText(") != std::string::npos);
        CITW_ASSERT(text.find("Ui().NoteText(") != std::string::npos);
        CITW_ASSERT(text.find("ImGui::BeginDisabled(!row.have)") != std::string::npos);
        CITW_ASSERT(text.find("ImGui::TextDisabled") == std::string::npos);
        CITW_ASSERT(text.find("PushFont") == std::string::npos);
        CITW_ASSERT(text.find("SetTooltip") == std::string::npos);
        // SoH's overlay chrome: no close button, the visibility key read live.
        CITW_ASSERT(text.find("ImGui::Begin(kComboItemTrackerWindowName, nullptr, flags)") != std::string::npos);
        CITW_ASSERT(text.find("CVarGetInteger(kComboItemTrackerVisibilityCVar, 0)") != std::string::npos);
        // Registered by the production bring-up, beside the Combo Tracker.
        const std::string init = slurp("/src/common/ComboTrackerWindow.cpp");
        CITW_ASSERT(init.find("ComboGui::RegisterComboItemTrackerWindow(gui);") != std::string::npos);
    }
#endif

    printf("[TEST] PASS: the Item Tracker overlay registers de-collided and idempotently, is inert under "
           "GAME_OOT/GAME_MM/GAME_NONE and while unpaused, and draws the item view's sections\n");
    return TEST_PASS;
}
