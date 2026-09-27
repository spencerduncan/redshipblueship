/**
 * @file soh_menu_combo_section_test.cpp
 * @brief ROM-free, display-free lock for #497 step 6: the tier-4 **Combo**
 *        section of the one live menu (ADR 0004 §4's ninth top-level header),
 *        the pointer row left behind on the interim host, and the extension
 *        point lane G's MM enhancement rows hang off (#682).
 *
 * CTest row MenuComboSection in CMake/SingleExecutable.cmake, dispatch
 * "menu-combo-section" in src/common/test_runner.cpp.
 *
 * WHAT IT IS FOR. Registration is container work, so every way step 6 can be
 * wrong is silent — nothing here is a compile error and nothing is a crash:
 *
 *  1. THE SECTION IS NOT THERE, or a page is not. #497's own verification
 *     comments tracked step 6's absence by counting `AddMenuEntry` call sites,
 *     which is a source grep; this asserts the registered structure instead.
 *
 *  2. A ROW IS REGISTERED PAST THE COLUMN COUNT. `Menu::DrawElement` iterates
 *     `columnCount` columns, so a row in a higher one is registered and never
 *     drawn. That is #640's failure mode with the halves swapped, and it is
 *     invisible without a window.
 *
 *  3. A PAGE IS EMPTY. #640 itself: `Menu::DrawElement`'s unconditional
 *     `SetNextWindowPos` goes unconsumed on a widget-less page and undocks
 *     libultraship's "Main Game" window. Every registered Combo sidebar must
 *     hold at least one widget, which is also why ADR 0004 §4's other three
 *     Combo sidebars are deliberately NOT registered yet.
 *
 *  4. A WINDOW BUTTON'S CVar OR NAME DRIFTS. `WIDGET_WINDOW_BUTTON` reads `.CVar`
 *     only for its open/close label and calls the window's own
 *     `ToggleVisibility` by `.WindowName` — so a mismatched pair produces a
 *     button that looks right and opens nothing. Leg 3 pins all seven pairs, and
 *     `EmbedWindow(false)` with them: the embed path calls `DrawElement()`
 *     directly and bypasses MM's `MMActiveGated` wrapper, the only thing keeping
 *     MM tracker UI from drawing while OoT is the running game. It also pins the
 *     page's SoH shape (UI parity M5, the shape of Randomizer > Item Tracker):
 *     each button sits directly under its own separator and carries SoH's
 *     "Toggles the <Window>." / "Enables the separate <Window> Settings Window."
 *     tooltip, and every MM tracker row says it shows only while Majora's Mask is
 *     running.
 *
 *  5. THE PLAYER'S SIDEBAR SELECTION IS STRANDED. Sidebar selection persists BY
 *     DISPLAY-NAME STRING into `gSettings.Menu.RandomizerSidebarSection`, so
 *     deleting `Randomizer → Cross-Game` outright would land every config holding
 *     that string on a header with no such sidebar. Leg 4 asserts the page
 *     survives, is non-empty, and actually names where its rows went.
 *
 *  6. THE EXTENSION POINT DOES NOT WORK, or works only by accident. Lane G adds
 *     MM enhancement toggles here after this merges, so leg 5 drives
 *     `RegisterComboSectionPage` end to end: the page appears AFTER the shipped
 *     ones, its registrar is handed `sectionName == "Combo"` and a PINNED
 *     `path.column`, its `columnCount` is honoured, a duplicate name replaces
 *     rather than duplicates, an invalid registration is refused, and
 *     `UnregisterComboSectionPage` puts the registry back.
 *
 *     #682 LANDED and leg 5's counts are now BASELINE-RELATIVE. A production page
 *     ("MM Enhancements", SohGui/SohMenuComboMmEnhancements.cpp) registers from a
 *     file-scope initializer, so the registry is no longer empty when any test
 *     runs. Leg 5 measures the delta its own registration makes and asserts the
 *     return to the baseline it started from; the production page's own shape is
 *     MenuMmEnhancementRows' invariant, not this row's.
 *
 *  7. A RENAMED PAGE STRANDS THE SELECTION. The Windows page shipped as "Cross-Game
 *     Windows" and was renamed on 2026-09-27 because the old name was wider than
 *     the 200 px sidebar (the snapshot harness drew it as "ross-Game Window");
 *     the same day "MM Enhancements" became "Majora's Mask", because its 192 px
 *     label left the selected highlight no padding. Selection persists by
 *     display name in gSettings.Menu.ComboSidebarSection, so leg 6 drives
 *     AddMenuCombo over a config holding each old name and requires the new one
 *     afterwards (a registered page, so the carry table cannot name a page that
 *     does not exist), and leaves every other value alone.
 *
 * WHAT IS NOT COVERED. The six tier-4 rules' own behaviour — the staging buffers,
 * the freeze gate, the values-from-the-save rule — is `ComboSettingsRows`, which
 * followed the rows here. This row owns the section's SHAPE. Appearance is
 * operator verification on the nightly.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "soh/SohGui/SohMenu.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace SohGui {
// The interim host's surviving registrar, defined in SohGui/SohMenuRandomizer.cpp
// and declared in no header: AddMenuRandomizer() as a whole cannot run ROM-free
// (its five Rando::Settings option groups reach Option::AddWidget, which RUNS
// each option's callback at registration time and dereferences a null
// gRandoContext), so this row drives the page directly. Declared here the way the
// SohGui TUs already declare SohGui::mSohMenu.
void AddCrossGamePointerWidgets(SohMenu& menu, WidgetPath& path);
} // namespace SohGui

namespace {

int gFailures = 0;

#define COMBO_CHECK(cond, ...)                                             \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            printf("[TEST]       ");                                       \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
            gFailures++;                                                   \
        }                                                                  \
    } while (0)

class ComboSectionMenuProbe final : public SohGui::SohMenu {
  public:
    ComboSectionMenuProbe() : SohGui::SohMenu("", "Combo Section Probe") {
    }

    std::unordered_map<std::string, MainMenuEntry>& Entries() {
        return menuEntries;
    }
    std::vector<std::string>& Order() {
        return menuOrder;
    }
};

WidgetInfo* FindRow(SidebarEntry& page, const std::string& name, uint32_t* columnOut = nullptr) {
    for (uint32_t column = 0; column < page.columnWidgets.size(); column++) {
        for (WidgetInfo& row : page.columnWidgets.at(column)) {
            if (row.name == name) {
                if (columnOut != nullptr) {
                    *columnOut = column;
                }
                return &row;
            }
        }
    }
    return nullptr;
}

// The row registered directly before `row` in its column, or null when `row`
// opens the column. The Windows page's SoH shape is "separator, then its
// button", so this is how leg 3 reads which separator a button sits under.
const WidgetInfo* RowBefore(SidebarEntry& page, const WidgetInfo* row) {
    for (auto& column : page.columnWidgets) {
        for (std::size_t i = 0; i < column.size(); i++) {
            if (&column.at(i) == row) {
                return i == 0 ? nullptr : &column.at(i - 1);
            }
        }
    }
    return nullptr;
}

std::size_t RowCount(const SidebarEntry& page) {
    std::size_t n = 0;
    for (const auto& column : page.columnWidgets) {
        n += column.size();
    }
    return n;
}

// ---- The extension point's observer ----------------------------------------
// A registrar records what AddMenuCombo handed it, so the contract lane G will
// rely on is OBSERVED rather than read off the source.
int gExtRan = 0;
std::string gExtSectionName;
std::string gExtSidebarName;
int gExtColumn = -1;
const char* kExtRowName = "Contributed Page Row";

void ExtRegistrar(SohGui::SohMenu& menu, WidgetPath& path) {
    gExtRan++;
    gExtSectionName = path.sectionName;
    gExtSidebarName = path.sidebarName;
    gExtColumn = (int)path.column;
    menu.AddWidget(path, kExtRowName, WIDGET_SEPARATOR_TEXT);
}

int gExtReplacementRan = 0;

void ExtReplacementRegistrar(SohGui::SohMenu& menu, WidgetPath& path) {
    gExtReplacementRan++;
    menu.AddWidget(path, "Replacement Page Row", WIDGET_SEPARATOR_TEXT);
}

const char* kExtPageName = "Contributed Test Page";

} // namespace

extern "C" int OoT_MenuComboSection_RunHeadless(void) {
    printf("[TEST] menu-combo-section: the tier-4 Combo section, its pages, the interim host's pointer row and the "
           "contributed-page extension point (#497 step 6, #682)\n");

    gFailures = 0;

    // The registry is process-global, so start from a known state rather than
    // from whatever ran before in AllTests.
    while (SohGui::UnregisterComboSectionPage(kExtPageName)) {}

    // ---- Leg 1: the section, its header CVar, and its two shipped pages -----
    ComboSectionMenuProbe probe;
    probe.AddMenuCombo();

    auto& entries = probe.Entries();
    if (!entries.contains("Combo")) {
        printf("[TEST] FAIL(1): AddMenuCombo registered no \"Combo\" menu entry -- ADR 0004 §4's tier-4 section does "
               "not exist, which is the state #497 step 6 was filed against\n");
        return 1;
    }
    MainMenuEntry& combo = entries.at("Combo");
    COMBO_CHECK(combo.label == "Combo", "the tier-4 header is labelled '%s'", combo.label.c_str());
    COMBO_CHECK(combo.sidebarCvar != nullptr && std::string(combo.sidebarCvar) == "gSettings.Menu.ComboSidebarSection",
                "the Combo header's sidebar CVar is '%s'; MainMenuEntry::sidebarCvar is where the last-viewed sidebar "
                "persists, and a header sharing another's key would move both",
                combo.sidebarCvar != nullptr ? combo.sidebarCvar : "(null)");
    {
        bool inOrder = false;
        for (const std::string& name : probe.Order()) {
            if (name == "Combo") {
                inOrder = true;
            }
        }
        COMBO_CHECK(inOrder, "\"Combo\" is in menuEntries but not in menuOrder, so it would never be drawn");
    }

    // "Windows" shipped as "Cross-Game Windows" until 2026-09-27; leg 6 pins the
    // rename's carried selection.
    const char* kShippedPages[] = { "Cross-Game Rules", "Windows" };
    for (const char* pageName : kShippedPages) {
        if (!combo.sidebars.contains(pageName)) {
            printf("[TEST] FAIL(1): the Combo section has no \"%s\" sidebar page\n", pageName);
            gFailures++;
            continue;
        }
        SidebarEntry& page = combo.sidebars.at(pageName);
        COMBO_CHECK(page.columnCount >= 1, "Combo / %s declares %u columns", pageName, page.columnCount);
        // #640, verbatim: an empty multi-column page leaves Menu::DrawElement's
        // SetNextWindowPos unconsumed and undocks the "Main Game" window.
        COMBO_CHECK(RowCount(page) > 0,
                    "Combo / %s is registered with no widgets at all -- #640's failure mode, which "
                    "undocks libultraship's \"Main Game\" window rather than merely looking empty",
                    pageName);
        // Every row must be in a column the page actually draws.
        for (uint32_t column = 0; column < page.columnWidgets.size(); column++) {
            for (WidgetInfo& row : page.columnWidgets.at(column)) {
                COMBO_CHECK(column < page.columnCount,
                            "Combo / %s row '%s' landed in column %u of a page that draws %u -- registered and never "
                            "drawn",
                            pageName, row.name.c_str(), column, page.columnCount);
            }
        }
        // Shipped pages come FIRST, before any contributed one.
        bool found = false;
        for (const std::string& name : combo.sidebarOrder) {
            if (name == pageName) {
                found = true;
            }
        }
        COMBO_CHECK(found, "Combo / %s is in `sidebars` but not in `sidebarOrder`", pageName);
    }
    printf("[TEST] leg 1: the Combo header exists with its own sidebar CVar and both shipped pages hold widgets\n");

    // ---- Leg 2: the rules page hosts the tier-4 rows -----------------------
    // Which rows, and how they behave, is ComboSettingsRows. What this leg owns is
    // that they are HERE and not on the interim host -- the move itself.
    if (combo.sidebars.contains("Cross-Game Rules")) {
        SidebarEntry& rules = combo.sidebars.at("Cross-Game Rules");
        COMBO_CHECK(FindRow(rules, "Combo Rules Status") != nullptr,
                    "the Cross-Game Rules page has no \"Combo Rules Status\" row");
        COMBO_CHECK(FindRow(rules, "Reset Combo Rules") != nullptr,
                    "the Cross-Game Rules page has no \"Reset Combo Rules\" row");
        COMBO_CHECK(RowCount(rules) >= 20,
                    "the Cross-Game Rules page holds %zu widgets; the six tier-4 settings alone are a status line, a "
                    "combobox, two sliders, two bitset headers with six checkboxes each, a checkbox and a button",
                    RowCount(rules));
    }

    // ---- Leg 3: the windows page's CVar/name pairs and their embed flag ----
    struct WindowRow {
        const char* rowName;
        const char* cVar;
        const char* windowName;
        const char* separator; // the SEPARATOR_TEXT directly above the button
        const char* tooltip;
    };
    // The authoritative constants are ComboGui::kComboMMOptions*/kComboSpoiler*/
    // kComboTracker* (src/common/ComboMmOptionsWindow.h, ComboSpoilerWindow.h,
    // ComboTrackerWindow.h) and kCheckTracker*/kItemTracker*
    // (games/mm/2s2h/TrackersGuiSingleExe.h). Restated as literals here on
    // purpose: the point is that the menu's spelling and the window's agree, and
    // sharing a constant would make them agree by construction instead.
    // The separators and tooltips are SoH's own tracker-page copy
    // (SohMenuRandomizer.cpp's Item Tracker page), restated here so a rewording
    // is a deliberate lock edit.
    const WindowRow kWindowRows[] = {
        { "Toggle MM Randomizer Options", "gCombo.Windows.MMOptions", "Majora's Mask Randomizer Options",
          "MM Randomizer Options", "Toggles the Majora's Mask Randomizer Options." },
        { "Toggle Cross-Game Spoiler", "gCombo.Windows.Spoiler", "Cross-Game Spoiler", "Cross-Game Spoiler",
          "Toggles the Cross-Game Spoiler." },
        { "Toggle Combo Tracker", "gCombo.Windows.Tracker", "Combo Tracker", "Combo Tracker",
          "Toggles the Combo Tracker." },
        { "Toggle MM Item Tracker", "gWindows.ItemTracker", "MM Item Tracker", "MM Item Tracker",
          "Toggles the MM Item Tracker. Majora's Mask only." },
        { "Popout MM Item Tracker Settings", "gWindows.ItemTrackerSettings", "MM Item Tracker Settings",
          "MM Item Tracker Settings", "Enables the separate MM Item Tracker Settings Window. Majora's Mask only." },
        { "Toggle MM Check Tracker", "gWindows.CheckTracker", "MM Check Tracker", "MM Check Tracker",
          "Toggles the MM Check Tracker. Majora's Mask only." },
        { "Popout MM Check Tracker Settings", "gWindows.CheckTrackerSettings", "MM Check Tracker Settings",
          "MM Check Tracker Settings", "Enables the separate MM Check Tracker Settings Window. Majora's Mask only." },
    };
    if (!combo.sidebars.contains("Windows")) {
        printf("[TEST] FAIL(3): the Combo section has no \"Windows\" page, so none of its seven window rows can be "
               "checked\n");
        gFailures++;
    } else {
        SidebarEntry& windows = combo.sidebars.at("Windows");
        for (const WindowRow& expected : kWindowRows) {
            WidgetInfo* row = FindRow(windows, expected.rowName);
            COMBO_CHECK(row != nullptr,
                        "the Windows page has no \"%s\" row -- that window is reachable "
                        "only from the console again",
                        expected.rowName);
            if (row == nullptr) {
                continue;
            }
            COMBO_CHECK(row->type == WIDGET_WINDOW_BUTTON, "row '%s' has widget type %d, expected WIDGET_WINDOW_BUTTON",
                        expected.rowName, (int)row->type);
            COMBO_CHECK(
                row->cVar != nullptr && std::string(row->cVar) == expected.cVar,
                "row '%s' binds CVar '%s', expected '%s' -- WINDOW_BUTTON reads .CVar for its open/close label, "
                "so a mismatch is a button that reads wrong while working",
                expected.rowName, row->cVar != nullptr ? row->cVar : "(null)", expected.cVar);
            COMBO_CHECK(row->windowName != nullptr && std::string(row->windowName) == expected.windowName,
                        "row '%s' names window '%s', expected '%s' -- ToggleVisibility is looked up BY THIS NAME, so a "
                        "mismatch is a button that opens nothing",
                        expected.rowName, row->windowName != nullptr ? row->windowName : "(null)", expected.windowName);
            auto options = std::static_pointer_cast<UIWidgets::WindowButtonOptions>(row->options);
            COMBO_CHECK(options != nullptr && !options->embedWindow,
                        "row '%s' embeds its window; the embed path calls DrawElement() directly and bypasses MM's "
                        "MMActiveGated Draw wrapper, which is the only thing keeping MM tracker UI from drawing while "
                        "Ocarina of Time is the running game",
                        expected.rowName);
            // The SoH tracker-page shape: this button's own separator directly
            // above it, and SoH's tooltip voice.
            const WidgetInfo* above = RowBefore(windows, row);
            COMBO_CHECK(above != nullptr && above->type == WIDGET_SEPARATOR_TEXT && above->name == expected.separator,
                        "row '%s' sits under '%s', expected its own separator '%s' -- SoH's tracker pages give every "
                        "window a separator of its own (Randomizer > Item Tracker)",
                        expected.rowName, above != nullptr ? above->name.c_str() : "(nothing)", expected.separator);
            const char* tip = options != nullptr ? options->tooltip : nullptr;
            COMBO_CHECK(tip != nullptr && std::string(tip) == expected.tooltip,
                        "row '%s' has tooltip \"%s\", expected \"%s\"", expected.rowName,
                        tip != nullptr ? tip : "(null)", expected.tooltip);
            // One line, as SoH's own tracker tooltips are: UIWidgets::WrappedText
            // breaks at 80 characters, and the longer MM caveat this replaced
            // wrapped a lone "running." onto a second line.
            COMBO_CHECK(tip == nullptr || std::string(tip).size() <= 80,
                        "row '%s' has a %zu-character tooltip; UIWidgets::WrappedText wraps past 80, and SoH's tracker "
                        "tooltips are one line",
                        expected.rowName, tip != nullptr ? std::string(tip).size() : (std::size_t)0);
        }
        printf("[TEST] leg 3: all %d cross-game window rows carry a matching CVar/WindowName pair, stay pop-out, and "
               "sit under their own separator with SoH's tooltip\n",
               (int)(sizeof(kWindowRows) / sizeof(kWindowRows[0])));
    }

    // ---- Leg 4: the interim host survives, and points somewhere ------------
    // Not politeness: gSettings.Menu.RandomizerSidebarSection holds the selection
    // as a DISPLAY-NAME STRING, which is the measured reason ADR 0004's resolved
    // call 1 refused to promote Cheats out of Enhancements.
    {
        ComboSectionMenuProbe interim;
        interim.AddMenuEntry("Randomizer", "gSettings.Menu.RandomizerSidebarSection");
        // A deliberately WRONG incoming column: the registrar pins its own,
        // because five Rando::Settings option groups run immediately before it in
        // production and leave path.column wherever their last COLUMN container
        // landed.
        WidgetPath interimPath = { "Randomizer", "Ignored", SECTION_COLUMN_3 };
        SohGui::AddCrossGamePointerWidgets(interim, interimPath);

        auto& randoSidebars = interim.Entries().at("Randomizer").sidebars;
        if (!randoSidebars.contains("Cross-Game")) {
            printf("[TEST] FAIL(4): Randomizer / Cross-Game is gone. Every config holding \"Cross-Game\" in "
                   "gSettings.Menu.RandomizerSidebarSection now selects a sidebar that does not exist\n");
            gFailures++;
        } else {
            SidebarEntry& page = randoSidebars.at("Cross-Game");
            COMBO_CHECK(RowCount(page) > 0, "Randomizer / Cross-Game survives with no widgets -- #640's failure mode");
            // The MEASURE (UI parity M2): two declared columns with the rows in
            // the first, so the gray note wraps at Randomizer > General's column
            // width instead of running as one line across the whole page.
            COMBO_CHECK(page.columnCount == 2,
                        "Randomizer / Cross-Game declares %u columns, expected 2: with one, its gray note runs as a "
                        "single line across the page instead of wrapping like Randomizer > General's notes",
                        page.columnCount);
            for (std::size_t column = 1; column < page.columnWidgets.size(); column++) {
                COMBO_CHECK(page.columnWidgets.at(column).empty(),
                            "Randomizer / Cross-Game has %zu widget(s) in column %zu; the pointer rows belong in the "
                            "first",
                            page.columnWidgets.at(column).size(), column);
            }
            bool pointsAtCombo = false;
            for (auto& column : page.columnWidgets) {
                for (WidgetInfo& row : column) {
                    if (row.name.find("Combo") != std::string::npos) {
                        pointsAtCombo = true;
                    }
                }
            }
            COMBO_CHECK(pointsAtCombo,
                        "Randomizer / Cross-Game survives but no row on it names the Combo section, so a player whose "
                        "config lands them here has no clue where their cross-game settings went");
            // The rows really moved: none of the tier-4 controls may be left
            // behind, or the two hosts would both write the same six keys.
            COMBO_CHECK(FindRow(page, "Combo Rules Status") == nullptr,
                        "the tier-4 status row is still on the interim host as well as in the Combo section");
            COMBO_CHECK(FindRow(page, "Toggle MM Randomizer Options") == nullptr,
                        "the MM options window row is still on the interim host as well as in the Combo section");
            printf("[TEST] leg 4: Randomizer / Cross-Game survives with a pointer row and none of the moved "
                   "controls\n");
        }
    }

    // ---- Leg 5: the extension point (#682's seam) --------------------------
    // BASELINE, not zero, as of #682. When this row was written the registry was
    // empty in every process, so the assertions below could be absolute. It is no
    // longer: SohGui/SohMenuComboMmEnhancements.cpp registers the production "MM
    // Enhancements" page from a file-scope initializer, which by design runs
    // before any test does. So this leg measures the DELTA its own registration
    // makes instead. The baseline is still checked for the thing the original
    // assertion was really guarding against — a page LEAKED by an earlier row in
    // the shared AllTests process — by the unregister-loop above plus the
    // return-to-baseline check at the end of the leg. That the production page
    // itself is registered, with the right rows, is MenuMmEnhancementRows' job,
    // not this row's.
    const std::size_t baselinePages = SohGui::GetComboSectionPages().size();

    gExtRan = 0;
    gExtColumn = -1;
    gExtSectionName.clear();
    gExtSidebarName.clear();
    SohGui::RegisterComboSectionPage(kExtPageName, 2, ExtRegistrar);
    COMBO_CHECK(SohGui::GetComboSectionPages().size() == baselinePages + 1,
                "registering one page left %zu in the registry, expected the baseline %zu plus one",
                SohGui::GetComboSectionPages().size(), baselinePages);

    // Refusals: a page with no name, no registrar or no column would either
    // never draw or be #640 by construction.
    SohGui::RegisterComboSectionPage(nullptr, 1, ExtRegistrar);
    SohGui::RegisterComboSectionPage("", 1, ExtRegistrar);
    SohGui::RegisterComboSectionPage("No Registrar", 1, nullptr);
    SohGui::RegisterComboSectionPage("No Columns", 0, ExtRegistrar);
    COMBO_CHECK(SohGui::GetComboSectionPages().size() == baselinePages + 1,
                "RegisterComboSectionPage accepted an invalid page: the registry holds %zu, expected the baseline %zu "
                "plus one",
                SohGui::GetComboSectionPages().size(), baselinePages);

    {
        ComboSectionMenuProbe extProbe;
        extProbe.AddMenuCombo();
        COMBO_CHECK(gExtRan == 1, "the contributed registrar ran %d times, expected once per AddMenuCombo", gExtRan);
        COMBO_CHECK(gExtSectionName == "Combo", "the contributed registrar was handed section '%s', expected 'Combo'",
                    gExtSectionName.c_str());
        COMBO_CHECK(gExtSidebarName == kExtPageName, "the contributed registrar was handed sidebar '%s', expected '%s'",
                    gExtSidebarName.c_str(), kExtPageName);
        COMBO_CHECK(gExtColumn == (int)SECTION_COLUMN_1,
                    "the contributed registrar was handed column %d, not the pinned first one. A contributor that "
                    "inherited a stale column would register rows that are never drawn, and no compile catches it",
                    gExtColumn);

        auto& extSidebars = extProbe.Entries().at("Combo").sidebars;
        if (!extSidebars.contains(kExtPageName)) {
            printf("[TEST] FAIL(5): AddMenuCombo created no sidebar for the contributed page '%s'\n", kExtPageName);
            gFailures++;
        } else {
            SidebarEntry& extPage = extSidebars.at(kExtPageName);
            COMBO_CHECK(extPage.columnCount == 2, "the contributed page declares %u columns, expected the registered 2",
                        extPage.columnCount);
            COMBO_CHECK(FindRow(extPage, kExtRowName) != nullptr, "the contributed page holds no row");
        }
        // Contributed pages come LAST, so a contributor cannot reorder the
        // shipped ones out from under a player's persisted selection.
        auto& order = extProbe.Entries().at("Combo").sidebarOrder;
        COMBO_CHECK(order.size() >= baselinePages + 3,
                    "the Combo section has %zu sidebars in order, expected the two shipped plus the %zu already "
                    "registered plus the one this leg added",
                    order.size(), baselinePages);
        if (order.size() >= baselinePages + 3) {
            COMBO_CHECK(order.at(0) == "Cross-Game Rules" && order.at(1) == "Windows" && order.back() == kExtPageName,
                        "sidebar order is [%s, %s, ..., %s]; the shipped pages must come first and contributed ones "
                        "last",
                        order.at(0).c_str(), order.at(1).c_str(), order.back().c_str());
        }
    }

    // A second registration under the same name REPLACES rather than duplicating:
    // two registrars claiming one page is an ambiguity in which the loser simply
    // never draws.
    gExtReplacementRan = 0;
    SohGui::RegisterComboSectionPage(kExtPageName, 1, ExtReplacementRegistrar);
    COMBO_CHECK(SohGui::GetComboSectionPages().size() == baselinePages + 1,
                "a same-name re-registration duplicated the page (%zu entries, baseline %zu)",
                SohGui::GetComboSectionPages().size(), baselinePages);
    {
        ComboSectionMenuProbe replaceProbe;
        gExtRan = 0;
        replaceProbe.AddMenuCombo();
        COMBO_CHECK(gExtReplacementRan == 1, "the replacement registrar ran %d times, expected once",
                    gExtReplacementRan);
        COMBO_CHECK(gExtRan == 0, "the replaced registrar ran anyway");
    }

    COMBO_CHECK(SohGui::UnregisterComboSectionPage(kExtPageName), "UnregisterComboSectionPage did not remove the page");
    COMBO_CHECK(!SohGui::UnregisterComboSectionPage(kExtPageName),
                "UnregisterComboSectionPage removed a page twice, so the registry held a duplicate");
    // Back to the baseline, not to empty: this leg must leave the registry
    // exactly as it found it, which is the leak check the opening assertion used
    // to be.
    COMBO_CHECK(SohGui::GetComboSectionPages().size() == baselinePages,
                "the registry holds %zu contributed pages, expected the baseline %zu it started from",
                SohGui::GetComboSectionPages().size(), baselinePages);
    {
        ComboSectionMenuProbe cleanProbe;
        cleanProbe.AddMenuCombo();
        COMBO_CHECK(!cleanProbe.Entries().at("Combo").sidebars.contains(kExtPageName),
                    "an unregistered contributed page is still created");
    }
    printf("[TEST] leg 5: a contributed page is created after the shipped ones with a pinned column, replaces on "
           "re-registration, is refused when invalid, and unregisters cleanly\n");

    // ---- Leg 6: the renamed Windows page carries a persisted selection ------
    // Menu::DrawElement reads gSettings.Menu.ComboSidebarSection as a display
    // name and falls back to the first page when no sidebar has it, so without
    // the carry a config saved on the old name would silently land on Cross-Game
    // Rules. The key is restored to whatever it held, so AllTests order cannot
    // leak a selection into a later row.
    {
        const char* kSidebarKey = "gSettings.Menu.ComboSidebarSection";
        const std::string saved = CVarGetString(kSidebarKey, "");
        struct Carry {
            const char* before;
            const char* after;
        };
        const Carry kCarries[] = {
            { "Cross-Game Windows", "Windows" },    // the old name moves to the new one
            { "Windows", "Windows" },               // the new name stays
            { "MM Enhancements", "Majora's Mask" }, // the second rename, the same day
            { "Majora's Mask", "Majora's Mask" },
            { "Cross-Game Rules", "Cross-Game Rules" }, // a page never renamed is left alone
        };
        for (const Carry& carry : kCarries) {
            CVarSetString(kSidebarKey, carry.before);
            ComboSectionMenuProbe carryProbe;
            carryProbe.AddMenuCombo();
            const std::string now = CVarGetString(kSidebarKey, "");
            COMBO_CHECK(now == carry.after, "a config holding \"%s\" reads \"%s\" after AddMenuCombo, expected \"%s\"",
                        carry.before, now.c_str(), carry.after);
            COMBO_CHECK(carryProbe.Entries().at("Combo").sidebars.contains(now),
                        "the carried selection \"%s\" names no Combo page, so the menu would fall back to the first",
                        now.c_str());
        }
        if (saved.empty()) {
            CVarClear(kSidebarKey);
        } else {
            CVarSetString(kSidebarKey, saved.c_str());
        }
        printf("[TEST] leg 6: selections saved on \"Cross-Game Windows\" and \"MM Enhancements\" are carried to "
               "\"Windows\" and \"Majora's Mask\"; every other value is left alone\n");
    }

    if (gFailures == 0) {
        printf("[TEST] menu-combo-section: PASS\n");
    } else {
        printf("[TEST] menu-combo-section: %d failure(s)\n", gFailures);
    }
    return gFailures;
}

#endif // RSBS_SINGLE_EXECUTABLE
