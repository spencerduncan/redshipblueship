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
 *    Image, HasImage and ToneText entries are installed and fall back (draw
 *    nothing, say no texture) without one.
 *
 * 7. THE ICONS (#458 U3), over the real rows of both production adapters:
 *    each game's own grid (OoT: SoH's main window, 66 icons in exactly 11
 *    lines of six, flowing on; MM: one table per group, Songs and Quest five
 *    wide), the texture and alpha each cell draws (SoH's _Faded textures, MM's
 *    own icon at 0.4), the songs' narrow notes, the counts each tracker prints,
 *    the per-section text fallback, the Shared section's icons from the active
 *    game, and the side-by-side fit at both capture profiles.
 *
 * 8. THE KEY SPACES DO NOT MEET (#458 U3's scan lock). Both games' icons sit in
 *    one Gui texture map, so an OoT key equal to an MM key would draw one
 *    game's icon for the other's. OoT keys its icons by texture NAME and MM by
 *    resource PATH: every key either adapter hands out is checked to be of its
 *    game's kind, and a scan of every OoT LoadGuiTexture call holds that OoT
 *    registers names (the one path-keyed call, the seed-hash icons that key a
 *    texture by its own OoT path, is named and held to that shape).
 *
 * Appearance is judged from the UiSnapshot captures (window/Combo Item Tracker
 * @no-data, @live and @snapshot beside SoH's window/Item Tracker); ImageOracle
 * there holds that the live and snapshot states draw icons.
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

#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
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

bool CitwIsPath(const char* key) {
    return key != nullptr && strchr(key, '/') != nullptr;
}

/** A Gui texture map holding OoT's icons only (the harness and OoT-first play before MM boots). */
bool CitwOoTLoaded(const char* key) {
    return key != nullptr && key[0] != '\0' && !CitwIsPath(key);
}

/** A Gui texture map holding both games' icons (after MM's first boot). */
bool CitwBothLoaded(const char* key) {
    return key != nullptr && key[0] != '\0';
}

bool CitwNoneLoaded(const char*) {
    return false;
}

const ComboItemRow* CitwFindRow(const ComboGui::ComboItemTrackerSection& section, const char* name) {
    for (const ComboItemRow& row : section.rows) {
        if (row.name != nullptr && strcmp(row.name, name) == 0) {
            return &row;
        }
    }
    return nullptr;
}

/**
 * Point 7's grids, picks and counts, over the real sections of the "OoT live,
 * MM snapshot" model read: OoT's live world, MM's snapshot world, the pool.
 */
int CitwLockIcons(const ComboGui::ComboItemTrackerSection& oot, const ComboGui::ComboItemTrackerSection& mm,
                  const ComboGui::ComboItemTrackerSection& shared, const ComboGui::ComboItemTrackerSection& ootSnap) {
    using namespace ComboGui;

    // ---- each game's own grid -----------------------------------------------
    CITW_ASSERT(oot.grid != nullptr && mm.grid != nullptr && shared.grid != nullptr);
    std::vector<ComboItemGridCell> cells;
    // OoT: DrawItemsInRows(mainWindowItems, 6) over SoH's 66 default rows: row
    // i at line i / 6, column i % 6, 11 lines, no group starting a line.
    ComboItemIconLayout(oot.rows, *oot.grid, cells);
    CITW_ASSERT(cells.size() == 66 && oot.rows.size() == 66);
    for (size_t i = 0; i < cells.size(); i++) {
        if (cells[i].line != (int)(i / 6) || cells[i].column != (int)(i % 6)) {
            printf("[TEST] FAIL: OoT icon %zu (%s) at line %d column %d, SoH draws it at line %d column %d\n", i,
                   oot.rows[i].name, cells[i].line, cells[i].column, (int)(i / 6), (int)(i % 6));
            return TEST_FAIL;
        }
    }
    CITW_ASSERT(oot.grid->cellPx == 36.0f && oot.grid->gapPx == 12.0f && oot.grid->countStyle == COMBO_ITEM_COUNT_SOH);
    // MM: one table per group (Inventory 16 and Masks 24 six wide, Songs 10
    // and Quest 10 five wide): lines 0-2, 3-6, 7-8, 9-10.
    ComboItemIconLayout(mm.rows, *mm.grid, cells);
    CITW_ASSERT(cells.size() == mm.rows.size() && cells.size() == 60);
    struct Expect {
        const char* group;
        int firstLine;
        int columns;
    };
    const Expect mmGroups[] = { { "Inventory", 0, 6 }, { "Masks", 3, 6 }, { "Songs", 7, 5 }, { "Quest", 9, 5 } };
    for (const Expect& e : mmGroups) {
        int index = 0;
        for (size_t i = 0; i < cells.size(); i++) {
            if (strcmp(mm.rows[i].group, e.group) != 0) {
                continue;
            }
            const int line = e.firstLine + index / e.columns;
            const int column = index % e.columns;
            if (cells[i].line != line || cells[i].column != column) {
                printf("[TEST] FAIL: MM %s icon %d (%s) at line %d column %d, MM's table puts it at line %d column "
                       "%d\n",
                       e.group, index, mm.rows[i].name, cells[i].line, cells[i].column, line, column);
                return TEST_FAIL;
            }
            index++;
        }
        CITW_ASSERT(index > 0);
    }
    CITW_ASSERT(cells.back().line == 10);
    CITW_ASSERT(mm.grid->cellPx == 46.0f && mm.grid->countStyle == COMBO_ITEM_COUNT_MM);
    // The pool: SoH's icons two a line.
    ComboItemIconLayout(shared.rows, *shared.grid, cells);
    CITW_ASSERT(!cells.empty() && cells.back().line == (int)(shared.rows.size() - 1) / 2);
    printf("[TEST] combo-item-tracker-window: grids: OoT %zu icons in 11 lines of 6 (SoH's main window), MM %zu in "
           "its 4 tables (11 lines), the pool %zu two a line\n",
           oot.rows.size(), mm.rows.size(), shared.rows.size());

    // ---- picks: the texture and alpha each cell draws ------------------------
    const ComboItemRow* hookshot = CitwFindRow(oot, "Hookshot"); // held in OoT's live world
    const ComboItemRow* bow = CitwFindRow(oot, "Fairy Bow");     // not held there
    CITW_ASSERT(hookshot != nullptr && hookshot->have && bow != nullptr && !bow->have);
    ComboItemIconPick pick = ComboItemPickIcon(*hookshot);
    CITW_ASSERT(pick.key != nullptr && strcmp(pick.key, "ITEM_HOOKSHOT") == 0 && pick.alpha == 1.0f);
    pick = ComboItemPickIcon(*bow);
    CITW_ASSERT(pick.key != nullptr && strcmp(pick.key, "ITEM_BOW_Faded") == 0 && pick.alpha == 1.0f);
    const ComboItemRow* ootSong = CitwFindRow(oot, "Song of Time");
    CITW_ASSERT(ootSong != nullptr && ootSong->iconAspect > 0.66f && ootSong->iconAspect < 0.67f); // iconSize / 1.5
    CITW_ASSERT(hookshot->iconAspect == 0.0f);
    const ComboItemRow* mmBow = CitwFindRow(mm, "Bow");             // held in MM's snapshot world
    const ComboItemRow* mmZora = CitwFindRow(mm, "Zora Mask");      // not held there
    const ComboItemRow* mmSong = CitwFindRow(mm, "Song of Healing"); // held
    CITW_ASSERT(mmBow != nullptr && mmBow->have && mmZora != nullptr && !mmZora->have && mmSong != nullptr);
    pick = ComboItemPickIcon(*mmBow);
    CITW_ASSERT(CitwIsPath(pick.key) && pick.alpha == 1.0f);
    pick = ComboItemPickIcon(*mmZora);
    CITW_ASSERT(CitwIsPath(pick.key) && pick.key == mmZora->iconKey && pick.alpha == 0.4f); // MM's 40%
    CITW_ASSERT(mmSong->iconAspect > 0.69f && mmSong->iconAspect < 0.70f); // 32 / 46

    // ---- counts ---------------------------------------------------------------
    ComboItemCount count;
    const ComboItemRow* ootSnapBow = CitwFindRow(ootSnap, "Fairy Bow");
    CITW_ASSERT(ootSnapBow != nullptr && ootSnapBow->have);
    CITW_ASSERT(ComboItemCountFor(*ootSnapBow, COMBO_ITEM_COUNT_SOH, count));
    CITW_ASSERT(count.amount == "35" && count.ceiling == "/40" && count.amountTone == COMBO_UI_TONE_WHITE &&
                count.ceilingTone == COMBO_UI_TONE_GREEN);
    ComboItemRow full = *ootSnapBow;
    full.count = 40;
    CITW_ASSERT(ComboItemCountFor(full, COMBO_ITEM_COUNT_SOH, count) && count.amountTone == COMBO_UI_TONE_GREEN);
    full.count = 0;
    CITW_ASSERT(ComboItemCountFor(full, COMBO_ITEM_COUNT_SOH, count) && count.amountTone == COMBO_UI_TONE_GRAY);
    CITW_ASSERT(!ComboItemCountFor(*bow, COMBO_ITEM_COUNT_SOH, count)); // faded, uncounted: no number
    CITW_ASSERT(!ComboItemCountFor(*hookshot, COMBO_ITEM_COUNT_SOH, count)); // held, uncounted
    const ComboItemRow* tokens = CitwFindRow(ootSnap, "Gold Skulltula Tokens");
    CITW_ASSERT(tokens != nullptr && ComboItemCountFor(*tokens, COMBO_ITEM_COUNT_SOH, count) &&
                count.amount == "17" && count.ceiling == "/100");
    CITW_ASSERT(ComboItemCountFor(*mmBow, COMBO_ITEM_COUNT_MM, count) && count.amount == "25" &&
                count.ceiling.empty()); // MM prints the ammo alone
    const ComboItemRow* rupees = CitwFindRow(shared, "Rupees");
    const ComboItemRow* arrows = CitwFindRow(shared, "Arrows");
    CITW_ASSERT(arrows != nullptr && ComboItemCountFor(*arrows, COMBO_ITEM_COUNT_SOH, count) &&
                count.amount == "33" && count.ceiling == "/40");
    if (rupees != nullptr && rupees->count > 0) {
        CITW_ASSERT(ComboItemCountFor(*rupees, COMBO_ITEM_COUNT_SOH, count) && count.ceiling.empty());
    }

    // ---- icons or text, per section --------------------------------------------
    CITW_ASSERT(ComboItemSectionDrawsIcons(oot.rows, CitwOoTLoaded));
    CITW_ASSERT(!ComboItemSectionDrawsIcons(mm.rows, CitwOoTLoaded)); // before MM's first boot: the text grid
    CITW_ASSERT(ComboItemSectionDrawsIcons(mm.rows, CitwBothLoaded));
    CITW_ASSERT(!ComboItemSectionDrawsIcons(oot.rows, CitwNoneLoaded));
    CITW_ASSERT(!ComboItemSectionDrawsIcons(oot.rows, nullptr));
    printf("[TEST] combo-item-tracker-window: picks, song notes, counts and the per-section text fallback PASS\n");
    return TEST_PASS;
}

/**
 * The floating overlay's fit (#458 U2 review; U3's icons): the three columns
 * stand side by side and every pixel of them scales together. Over the real
 * sections at the two capture profiles' room (832x600 and 1280x800, less the
 * overlay's 60 px position, its padding and the margin), with a 70 px header.
 */
int CitwLockFit(const ComboGui::ComboItemTrackerSection (&sections)[ComboGui::COMBO_ITEM_SECTION_COUNT]) {
    using namespace ComboGui;
    ComboItemTrackerMetrics m;
    m.textHeight = 20.0f;
    m.linePadding = 4.0f;
    m.columnGap = 16.0f;
    m.cellPadding = 8.0f;
    const float header = 70.0f;

    // Both games' icons loaded (play after MM's first boot), and MM as text (OoT-first play before it).
    for (int mmText = 0; mmText < 2; mmText++) {
        std::vector<ComboItemSectionBox> boxes;
        for (const ComboItemTrackerSection& section : sections) {
            CITW_ASSERT(!section.rows.empty());
            if (mmText && section.id == COMBO_ITEM_SECTION_MM) {
                std::vector<float> widths;
                for (const ComboItemRow& row : section.rows) {
                    widths.push_back(11.0f * (float)ComboItemRowText(row).size());
                }
                boxes.push_back(ComboItemTextBox(section, widths, m, header));
                continue;
            }
            std::vector<ComboItemGridCell> cells;
            ComboItemIconLayout(section.rows, *section.grid, cells);
            boxes.push_back(ComboItemIconBox(cells, *section.grid, m, header));
        }
        // OoT's column is SoH's own overlay grid: six 36 px icons 48 px apart,
        // 11 lines.
        CITW_ASSERT(boxes[0].width == 5.0f * 48.0f + 36.0f && boxes[0].height == 11.0f * 48.0f);

        const float rooms[2][2] = { { 748.0f, 520.0f }, { 1196.0f, 720.0f } };
        for (const auto& room : rooms) {
            const ComboItemTrackerFit fit = ComboItemTrackerFitBoxes(boxes, room[0], room[1]);
            float width = 0.0f;
            float height = 0.0f;
            for (const ComboItemSectionBox& b : boxes) {
                width += b.width * fit.scale + b.fixedWidth;
                const float h = b.height * fit.scale + b.fixedHeight;
                height = h > height ? h : height;
            }
            printf("[TEST] combo-item-tracker-window: fit in %.0fx%.0f (MM %s): scale %.3f, columns %.0fx%.0f\n",
                   room[0], room[1], mmText ? "as text" : "as icons", fit.scale, width, height);
            CITW_ASSERT(fit.scale >= kComboItemTrackerMinScale && fit.scale <= 1.0f);
            CITW_ASSERT(width <= room[0] + 0.01f && height <= room[1] + 0.01f);
            const float steps = fit.scale / kComboItemTrackerScaleStep;
            CITW_ASSERT(steps == (float)(int)steps);
            if (fit.scale < 1.0f) {
                // The largest whole step that fits: one more does not.
                const float bigger = fit.scale + kComboItemTrackerScaleStep;
                float w2 = 0.0f;
                float h2 = 0.0f;
                for (const ComboItemSectionBox& b : boxes) {
                    w2 += b.width * bigger + b.fixedWidth;
                    const float h = b.height * bigger + b.fixedHeight;
                    h2 = h > h2 ? h : h2;
                }
                CITW_ASSERT(w2 > room[0] || h2 > room[1]);
            }
            if (!mmText && room[1] == 720.0f) {
                CITW_ASSERT(fit.scale == 1.0f); // 1280x800 holds both games' grids at SoH's own size
            }
            // Hysteresis: the fit drawn last frame is kept as it is; a step
            // down is taken at once; a smaller scale last frame grows back.
            ComboItemTrackerFit same = ComboItemTrackerFitBoxes(boxes, room[0], room[1], &fit);
            CITW_ASSERT(same.scale == fit.scale);
            ComboItemTrackerFit tooBig;
            tooBig.scale = fit.scale + 4.0f * kComboItemTrackerScaleStep;
            CITW_ASSERT(ComboItemTrackerFitBoxes(boxes, room[0], room[1], &tooBig).scale == fit.scale);
            ComboItemTrackerFit smaller;
            smaller.scale = fit.scale - 2.0f * kComboItemTrackerScaleStep;
            if (smaller.scale >= kComboItemTrackerMinScale) {
                CITW_ASSERT(ComboItemTrackerFitBoxes(boxes, room[0], room[1], &smaller).scale == fit.scale);
            }
        }
        // No room at all: the floor, never below it.
        CITW_ASSERT(ComboItemTrackerFitBoxes(boxes, 100.0f, 100.0f).scale == kComboItemTrackerMinScale);
    }

    // A step up waits until it fits by half a step more. One box 100 px wide in
    // a room that fits it at 0.5 and a quarter step: 0.5; with 0.5 less a step
    // last frame it stays there; with room for three quarters more it steps up.
    {
        std::vector<ComboItemSectionBox> one(1);
        one[0].width = 100.0f;
        one[0].height = 10.0f;
        ComboItemTrackerFit lower;
        lower.scale = 0.5f - kComboItemTrackerScaleStep;
        const float quarter = 100.0f * (0.5f + 0.25f * kComboItemTrackerScaleStep);
        CITW_ASSERT(ComboItemTrackerFitBoxes(one, quarter, 10000.0f).scale == 0.5f);
        CITW_ASSERT(ComboItemTrackerFitBoxes(one, quarter, 10000.0f, &lower).scale == lower.scale);
        const float threeQuarters = 100.0f * (0.5f + 0.75f * kComboItemTrackerScaleStep);
        CITW_ASSERT(ComboItemTrackerFitBoxes(one, threeQuarters, 10000.0f, &lower).scale == 0.5f);
    }
    printf("[TEST] combo-item-tracker-window: the side-by-side fit PASS\n");
    return TEST_PASS;
}

/**
 * Point 8, the runtime half: every icon key either adapter hands out, for
 * every row of both authored worlds and of an empty save, and for every
 * shared kind at every tier, is of its own game's kind: OoT's a texture name
 * (never a path), MM's a resource path.
 */
int CitwLockKeySpaces(const ComboItemOps* oot, const ComboItemOps* mm) {
    int ootKeys = 0;
    int mmKeys = 0;
    std::vector<uint8_t> buf((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    std::vector<uint8_t> mmBuf((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    for (int variant = 0; variant < 2; variant++) {
        CITW_ASSERT(OoT_ItemAdapter_TestAuthorSave(buf.data(), buf.size(), variant) == 1);
        CITW_ASSERT(MM_ItemAdapter_TestAuthorSave(mmBuf.data(), mmBuf.size(), variant) == 1);
        ComboItemRow row;
        for (int i = 0; i < oot->count(); i++) {
            CITW_ASSERT(oot->rowAt(buf.data(), i, &row));
            for (const char* key : { row.iconKey, row.iconKeyFaded }) {
                if (key != nullptr) {
                    if (CitwIsPath(key)) {
                        printf("[TEST] FAIL: OoT row \"%s\" keys its icon by a path: %s\n", row.name, key);
                        return TEST_FAIL;
                    }
                    ootKeys++;
                }
            }
        }
        for (int i = 0; i < mm->count(); i++) {
            CITW_ASSERT(mm->rowAt(mmBuf.data(), i, &row));
            for (const char* key : { row.iconKey, row.iconKeyFaded }) {
                if (key != nullptr) {
                    if (!CitwIsPath(key)) {
                        printf("[TEST] FAIL: MM row \"%s\" keys its icon by a name: %s\n", row.name, key);
                        return TEST_FAIL;
                    }
                    mmKeys++;
                }
            }
        }
    }
    CITW_ASSERT(oot->sharedIcon != nullptr && mm->sharedIcon != nullptr);
    int ootShared = 0;
    int mmShared = 0;
    for (unsigned kind = 1; kind < RSBS_SHARED_RES_KIND_COUNT; kind++) {
        for (uint16_t tier = 0; tier <= 3; tier++) {
            ComboItemRow row = {};
            oot->sharedIcon((uint8_t)kind, tier, &row);
            CITW_ASSERT(row.iconKey == nullptr || !CitwIsPath(row.iconKey));
            CITW_ASSERT(row.iconKeyFaded == nullptr || !CitwIsPath(row.iconKeyFaded));
            ootShared += row.iconKey != nullptr ? 1 : 0;
            row = {};
            mm->sharedIcon((uint8_t)kind, tier, &row);
            CITW_ASSERT(row.iconKey == nullptr || CitwIsPath(row.iconKey));
            mmShared += row.iconKey != nullptr ? 1 : 0;
        }
    }
    // The rupees, the hearts and the bow's quiver each have an icon in both games.
    for (const ComboItemOps* ops : { oot, mm }) {
        for (uint8_t kind : { (uint8_t)RSBS_SHARED_RES_RUPEES, (uint8_t)RSBS_SHARED_RES_HEALTH_QUARTERS,
                              (uint8_t)RSBS_SHARED_RES_ARROW_COUNT }) {
            ComboItemRow row = {};
            ops->sharedIcon(kind, 2, &row);
            CITW_ASSERT(row.iconKey != nullptr && row.iconKeyFaded != nullptr);
        }
    }
    printf("[TEST] combo-item-tracker-window: key spaces: %d OoT row keys and %d shared are names, %d MM row keys and "
           "%d shared are paths\n",
           ootKeys, ootShared, mmKeys, mmShared);
    CITW_ASSERT(ootKeys > 100 && mmKeys > 100 && ootShared > 0 && mmShared > 0);
    return TEST_PASS;
}

/**
 * Point 8, the source half: every LoadGuiTexture call in OoT's tree keys its
 * texture by a NAME. A key is a name when it is a string literal with no '/'
 * or one of the name members SoH's tables carry (entry.second.name, .nameFaded;
 * the song tables' entry.name, entry.nameFaded; the time display's
 * load.first). One call is path-keyed today and is held to its shape: the
 * seed-hash icons (ImGuiUtils.cpp, `entry.tex, entry.tex`), which key a texture
 * by its own OoT path ("__OTR__textures/..."), a string no MM path equals.
 */
int CitwLockLoadGuiTextureScan(void) {
#ifdef RSBS_SOURCE_DIR
    namespace fs = std::filesystem;
    const fs::path root = fs::path(RSBS_SOURCE_DIR) / "games" / "oot" / "soh";
    const char* const kNameKeys[] = { "entry.second.name", "entry.second.nameFaded", "entry.name", "entry.nameFaded",
                                      "load.first.c_str()" };
    int calls = 0;
    int selfKeyed = 0;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec)) {
        const fs::path& file = it->path();
        const std::string ext = file.extension().string();
        if (!it->is_regular_file() || (ext != ".cpp" && ext != ".c" && ext != ".h" && ext != ".hpp")) {
            continue;
        }
        std::ifstream in(file, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const std::string call = "LoadGuiTexture(";
        for (size_t at = text.find(call); at != std::string::npos; at = text.find(call, at + 1)) {
            if (at > 0 && (isalnum((unsigned char)text[at - 1]) || text[at - 1] == '_')) {
                continue; // LoadGuiTextures, a declaration of another function
            }
            // A call, not a declaration or a comment: "->LoadGuiTexture(".
            if (at < 2 || text.compare(at - 2, 2, "->") != 0) {
                continue;
            }
            size_t a = at + call.size();
            const size_t comma = text.find(',', a);
            CITW_ASSERT(comma != std::string::npos);
            std::string key = text.substr(a, comma - a);
            const size_t comma2 = text.find_first_of(",)", comma + 1);
            std::string second = text.substr(comma + 1, comma2 - comma - 1);
            auto trim = [](std::string& v) {
                const size_t b = v.find_first_not_of(" \t\r\n");
                const size_t e = v.find_last_not_of(" \t\r\n");
                v = (b == std::string::npos) ? std::string() : v.substr(b, e - b + 1);
            };
            trim(key);
            trim(second);
            calls++;
            bool name = key.size() >= 2 && key.front() == '"' && key.back() == '"' && key.find('/') == std::string::npos;
            for (const char* member : kNameKeys) {
                name = name || key == member;
            }
            if (name) {
                continue;
            }
            const bool seedHash = key == "entry.tex" && second == "entry.tex" &&
                                  file.filename().string() == "ImGuiUtils.cpp";
            if (seedHash) {
                selfKeyed++;
                continue;
            }
            printf("[TEST] FAIL: %s: LoadGuiTexture keyed by \"%s\", not a texture name: OoT keys its icons by "
                   "name and MM by path, so an OoT path key could meet one of MM's\n",
                   file.string().c_str(), key.c_str());
            return TEST_FAIL;
        }
    }
    printf("[TEST] combo-item-tracker-window: OoT's %d LoadGuiTexture calls key by name (%d seed-hash call keyed by "
           "its own OoT path)\n",
           calls, selfKeyed);
    CITW_ASSERT(!ec && calls >= 20 && selfKeyed == 1);
#endif
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
    // Each section carries its own game's grid (#458 U3); the pool's icons are
    // the active game's own: OoT's tier-2 quiver under GAME_OOT.
    CITW_ASSERT(ootLiveSection.grid == oot->grid && mmSnap.grid == mm->grid && shared.grid == &kComboItemSharedGrid);
    const ComboItemRow* arrowsOoT = CitwFindRow(shared, "Arrows");
    CITW_ASSERT(arrowsOoT != nullptr && arrowsOoT->iconKey != nullptr &&
                strcmp(arrowsOoT->iconKey, "ITEM_QUIVER_40") == 0);
    // The fallback read the draw makes when the active game's icons are not
    // loaded: MM's own, a resource path.
    ComboItemTrackerSection sharedMM;
    ComboItemTrackerCollectSection(ComboGui::COMBO_ITEM_SECTION_SHARED, sharedMM, (int)GAME_MM);
    const ComboItemRow* arrowsMM = CitwFindRow(sharedMM, "Arrows");
    CITW_ASSERT(arrowsMM != nullptr && CitwIsPath(arrowsMM->iconKey) && arrowsMM->fadedAlpha == 0.4f);
    const ComboItemTrackerSection ootLiveCopy = ootLiveSection;
    const ComboItemTrackerSection mmSnapCopy = mmSnap;
    const ComboItemTrackerSection sharedCopy = shared;

    // MM played live: OoT is never live, its rows are its snapshot's.
    Context_SetCurrentGame(GAME_MM);
    CITW_ASSERT(CitwDriveSections(sections));
    const ComboItemRow* arrowsActiveMM = CitwFindRow(sections[ComboGui::COMBO_ITEM_SECTION_SHARED], "Arrows");
    CITW_ASSERT(arrowsActiveMM != nullptr && CitwIsPath(arrowsActiveMM->iconKey)); // MM active: MM's icons
    const ComboItemTrackerSection ootSnapCopy = sections[ComboGui::COMBO_ITEM_SECTION_OOT];
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

    // ---- 7. the icons and the fit, over the real rows -----------------------
    if (CitwLockIcons(ootLiveCopy, mmSnapCopy, sharedCopy, ootSnapCopy) != TEST_PASS) {
        return TEST_FAIL;
    }
    const ComboItemTrackerSection live[ComboGui::COMBO_ITEM_SECTION_COUNT] = { ootLiveCopy, mmSnapCopy, sharedCopy };
    if (CitwLockFit(live) != TEST_PASS) {
        return TEST_FAIL;
    }
    // ---- 8. the key spaces ----------------------------------------------------
    return CitwLockKeySpaces(oot, mm);
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
    CITW_ASSERT(ComboUi_Get()->HasImage != nullptr && ComboUi_Get()->ToneText != nullptr);
    CITW_ASSERT(!ComboUi_Get()->HasImage("ITEM_LONGSHOT") && !ComboUi_Get()->HasImage(nullptr));

    // ---- 8. OoT's LoadGuiTexture keys -------------------------------------------
    if (CitwLockLoadGuiTextureScan() != TEST_PASS) {
        return TEST_FAIL;
    }

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
        // Icons or text per section, each game's grid, the picked texture
        // through the seam, the count and the name as its tooltip (#458 U3).
        CITW_ASSERT(text.find("ComboItemSectionDrawsIcons(section.rows, Ui().HasImage)") != std::string::npos);
        CITW_ASSERT(text.find("ComboItemIconLayout(section.rows, *section.grid, plan.cells)") != std::string::npos);
        CITW_ASSERT(text.find("ComboItemPickIcon(row)") != std::string::npos);
        CITW_ASSERT(text.find("Ui().Image(pick.key, iconWidth, cell)") != std::string::npos);
        CITW_ASSERT(text.find("ComboItemCountFor(row, countStyle, count)") != std::string::npos);
        CITW_ASSERT(text.find("Ui().Tooltip(ComboItemRowText(row).c_str())") != std::string::npos);
        CITW_ASSERT(text.find("ComboItemGridLayout(section.rows, kComboItemTrackerColumns, cells)") != std::string::npos);
        // The floating overlay is fitted, and drawn at the fit's scale.
        CITW_ASSERT(text.find("fit = ComboItemTrackerFitBoxes(boxes, availWidth, availHeight, &previous)") !=
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
