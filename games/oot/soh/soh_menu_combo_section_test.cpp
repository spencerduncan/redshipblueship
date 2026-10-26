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
 *     Combo sidebars are deliberately NOT registered yet. A COLUMN of a shipped
 *     page is not empty either (#844): `Menu::DrawElement` splits the section
 *     by `columnCount`, so a declared column with no rows draws as an empty
 *     half and narrows the rows beside it.
 *
 *  4. A WINDOW BUTTON'S CVar OR NAME DRIFTS. `WIDGET_WINDOW_BUTTON` reads `.CVar`
 *     only for its open/close label and calls the window's own
 *     `ToggleVisibility` by `.WindowName` — so a mismatched pair produces a
 *     button that looks right and opens nothing. Leg 3 pins all three pairs,
 *     and `EmbedWindow(false)` with them, as SoH's own tracker toggles are. It
 *     also pins the page's SoH shape (UI parity M5, the shape of Randomizer >
 *     Item Tracker): each button sits directly under its own separator and
 *     carries SoH's "Toggles the <Window>." tooltip. And it pins what LEFT the
 *     page (#458 U6, ADR 0004's 2026-09-30 amendment): the unified Combo Tracker
 *     and Combo Item Tracker supersede MM's four tracker rows, so no button on
 *     the page names an MM tracker window or binds an MM `gWindows.*` key. The
 *     native MM windows stay registered as fallbacks; that their keys still
 *     open them is MMTrackersGui's lock, not this row's.
 *
 *  5. THE RANDOMIZER PAGES ARE NOT UNDER THE RANDOMIZER HEADER (#1101, operator
 *     ruling 2026-10-21, option C). MM Randomizer, MM Tricks and the cross-game
 *     rules are pages of SoH's Randomizer header, after Tricks/Glitches and
 *     before Plandomizer, in that order, the rules under the "Cross-Game" name
 *     the pointer page kept alive (sidebar selection persists BY DISPLAY-NAME
 *     STRING into `gSettings.Menu.RandomizerSidebarSection`, so a config holding
 *     it lands on the rules). Leg 4 drives AddComboRandomizerPages between
 *     stand-ins for Tricks/Glitches and Plandomizer and requires that order, the
 *     rules page's rows and no pointer row left behind.
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
 *  7. A RENAMED OR MOVED PAGE STRANDS THE SELECTION. The Windows page shipped as "Cross-Game
 *     Windows" and was renamed on 2026-09-27 because the old name was wider than
 *     the 200 px sidebar (the snapshot harness drew it as "ross-Game Window");
 *     the same day "MM Enhancements" became "Majora's Mask", because its 192 px
 *     label left the selected highlight no padding. Selection persists by
 *     display name in gSettings.Menu.ComboSidebarSection, so leg 6 drives
 *     AddMenuCombo over a config holding each old name and requires the new one
 *     afterwards (a registered page, so the carry table cannot name a page that
 *     does not exist), and leaves every other value alone. The three pages #1101
 *     moved to the Randomizer header are carried ACROSS headers: a config that
 *     had Combo open on one of them opens on the moved page under Randomizer,
 *     and no Combo selection is left naming a page the header no longer has.
 *
 *  8. THE UNIFIED TRACKERS' SETTINGS HAVE NO MENU WRITER. ADR 0004's 2026-09-30
 *     amendment puts the unified trackers in Combo > Windows and their settings
 *     on a Combo page. Leg 7 requires Combo > Tracker Settings to write every
 *     gCombo.Tracker.Items.* key in the manifest exactly once, with the
 *     overlay's own widget, default and range, to hide the floating-only rows
 *     for a windowed overlay, to draw its colour picker in the menu theme, and
 *     to embed SoH's Check Tracker Settings (whose colours the Combo Tracker
 *     draws with), and (#1131) to write each of the Combo Tracker's own
 *     gCombo.Tracker.Checks.* keys exactly once, a checkbox with the window's
 *     default, under the Check Tracker Settings separator.
 *
 * WHAT IS NOT COVERED. The six tier-4 rules' own behaviour — the staging buffers,
 * the freeze gate, the values-from-the-save rule — is `ComboSettingsRows`, which
 * followed the rows here. This row owns the section's SHAPE. Appearance is
 * operator verification on the nightly.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "soh/SohGui/SohMenu.h"

#include "ComboItemTrackerWindow.h" // leg 7: the overlay's window types and Icon Size range
#include "cvar_shared_keys.h"       // leg 7: RSBS::kComboKeys and the gCombo.Tracker.Items.* keys

#include <libultraship/bridge/consolevariablebridge.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

// AddMenuRandomizer() as a whole cannot run ROM-free (its five Rando::Settings
// option groups reach Option::AddWidget, which RUNS each option's callback at
// registration time and dereferences a null gRandoContext), so legs 4 and 6 drive
// our slice of it, SohGui::AddComboRandomizerPages (SohMenu.h), directly.
namespace SohGui {
// Leg 7 points THEME_COLOR (SohGui::GetMenuThemeColor) at the probe for one call.
extern std::shared_ptr<SohMenu> mSohMenu;
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
// The same seam with ComboPageHost::Randomizer (#1101): drawn under SoH's
// Randomizer header by AddComboRandomizerPages, never under Combo.
const char* kExtRandoPageName = "Contributed Randomizer Test Page";

// Leg 7. Restated as literals on purpose, as leg 3's are: the point is that the
// page and this lock agree on the spelling.
const char* kTrackerSettingsPage = "Tracker Settings";
const char* kItemTrackerKeyPrefix = "gCombo.Tracker.Items.";
// #1131: the Combo Tracker's own keys, and each one's default in the window
// (ComboTrackerWindow.h, ComboTrackerNavKeys).
const char* kCheckTrackerKeyPrefix = "gCombo.Tracker.Checks.";
const struct {
    const char* key;
    bool defaultValue;
} kCheckTrackerKeyDefaults[] = {
    { "gCombo.Tracker.Checks.AutoScrollToCurrentArea", true },
    { "gCombo.Tracker.Checks.OnlyShowCurrentArea", false },
};

} // namespace

extern "C" int OoT_MenuComboSection_RunHeadless(void) {
    printf("[TEST] menu-combo-section: the tier-4 Combo section, its pages, the interim host's pointer row and the "
           "contributed-page extension point (#497 step 6, #682)\n");

    gFailures = 0;

    // The registry is process-global, so start from a known state rather than
    // from whatever ran before in AllTests.
    while (SohGui::UnregisterComboSectionPage(kExtPageName)) {}
    while (SohGui::UnregisterComboSectionPage(kExtRandoPageName)) {}

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
    // rename's carried selection. Cross-Game Rules is no longer a Combo page
    // (#1101): it is Randomizer > Cross-Game, leg 4.
    const char* kShippedPages[] = { "Windows" };
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
        // #844: every column the page declares holds rows. Menu::DrawElement
        // divides the section by columnCount whether or not a column has rows,
        // so a declared column with none leaves an empty half beside the page
        // and squeezes every row into the other (Cross-Game Rules kept one
        // after #834 retired its item-class groups).
        for (uint32_t column = 0; column < page.columnCount; column++) {
            const std::size_t rowsInColumn =
                column < page.columnWidgets.size() ? page.columnWidgets.at(column).size() : 0;
            COMBO_CHECK(rowsInColumn > 0,
                        "Combo / %s declares %u columns but column %u holds no rows -- the page draws an empty "
                        "column and its rows take only part of the width (#844)",
                        pageName, page.columnCount, column + 1);
        }
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
    // The Combo header after #1101: exactly five pages, Windows first, and none
    // of the three that moved to the Randomizer header.
    {
        const std::vector<std::string> kComboPages = { "Windows", "Majora's Mask", "MM Mods", "Save Files",
                                                       "Tracker Settings" };
        const auto& order = combo.sidebarOrder;
        COMBO_CHECK(order.size() == kComboPages.size(), "the Combo header holds %zu pages, expected the %zu below",
                    order.size(), kComboPages.size());
        COMBO_CHECK(!order.empty() && order.front() == "Windows", "the Combo header's first page is '%s', not Windows",
                    order.empty() ? "(none)" : order.front().c_str());
        for (const std::string& name : kComboPages) {
            COMBO_CHECK(combo.sidebars.contains(name), "the Combo header has no \"%s\" page", name.c_str());
        }
        for (const char* moved : { "Cross-Game Rules", "Cross-Game", "MM Randomizer", "MM Tricks" }) {
            COMBO_CHECK(!combo.sidebars.contains(moved),
                        "the Combo header still holds \"%s\"; the randomizer pages are under the Randomizer header "
                        "(operator ruling 2026-10-21)",
                        moved);
        }
    }
    printf("[TEST] leg 1: the Combo header exists with its own sidebar CVar and exactly its five pages, Windows "
           "first, each holding widgets in every column it declares\n");

    // ---- Leg 2: no Combo page hosts the tier-4 rows ------------------------
    // Which rows, and how they behave, is ComboSettingsRows. What this leg owns is
    // that they are NOT under Combo any more (#1101): leg 4 finds them under
    // Randomizer > Cross-Game, so two hosts cannot both write the keys.
    for (const std::string& name : combo.sidebarOrder) {
        if (!combo.sidebars.contains(name)) {
            continue;
        }
        SidebarEntry& page = combo.sidebars.at(name);
        COMBO_CHECK(FindRow(page, "Combo Rules Status") == nullptr && FindRow(page, "Reset Combo Rules") == nullptr,
                    "Combo / %s still hosts the cross-game rules", name.c_str());
    }

    // ---- Leg 3: the windows page's CVar/name pairs and their embed flag ----
    struct WindowRow {
        const char* rowName;
        const char* cVar;
        const char* windowName;
        const char* separator; // the SEPARATOR_TEXT directly above the button
        const char* tooltip;
        bool raceDisable; // SoH's own toggle's flag: a spoiler tool is race-disabled, a tracker is not
    };
    // The authoritative constants are ComboGui::kComboSpoiler*/kComboTracker*/
    // kComboItemTracker* (src/common/ComboSpoilerWindow.h, ComboTrackerWindow.h,
    // ComboItemTrackerWindow.h). Restated as literals here on
    // purpose: the point is that the menu's spelling and the window's agree, and
    // sharing a constant would make them agree by construction instead.
    // The separators and tooltips are SoH's own tracker-page copy
    // (SohMenuRandomizer.cpp's Item Tracker page), restated here so a rewording
    // is a deliberate lock edit.
    const WindowRow kWindowRows[] = {
        { "Toggle Cross-Game Spoiler", "gCombo.Windows.Spoiler", "Cross-Game Spoiler", "Cross-Game Spoiler",
          "Toggles the Cross-Game Spoiler.", true },
        // Not race-disabled, as SoH's Toggle Check Tracker is not
        // (SohMenuRandomizer.cpp): ruling D10 cut the Crossings section, the
        // window's only part that named items on uncollected checks (#1128).
        { "Toggle Combo Tracker", "gCombo.Windows.Tracker", "Combo Tracker", "Combo Tracker",
          "Toggles the Combo Tracker.", false },
        // The unified Item Tracker overlay (#458 U2; ADR 0004's 2026-09-30
        // amendment: U2 added this row, U6 removed MM's four tracker rows).
        { "Toggle Combo Item Tracker", "gCombo.Windows.ItemTracker", "Combo Item Tracker", "Combo Item Tracker",
          "Toggles the Combo Item Tracker. Drag it via Tracker Settings > Enable Dragging.", false },
    };
    // MM's four tracker rows, retired by #458 U6: the Combo Tracker and the
    // Combo Item Tracker above cover both games. Their windows stay registered
    // and their gWindows.* keys still open them (MMTrackersGui), but no menu row
    // writes those keys any more. Restated as literals, like the rows above.
    struct RetiredRow {
        const char* rowName;
        const char* cVar;
        const char* windowName;
    };
    const RetiredRow kRetiredRows[] = {
        { "Toggle MM Item Tracker", "gWindows.ItemTracker", "MM Item Tracker" },
        { "Popout MM Item Tracker Settings", "gWindows.ItemTrackerSettings", "MM Item Tracker Settings" },
        { "Toggle MM Check Tracker", "gWindows.CheckTracker", "MM Check Tracker" },
        { "Popout MM Check Tracker Settings", "gWindows.CheckTrackerSettings", "MM Check Tracker Settings" },
    };
    if (!combo.sidebars.contains("Windows")) {
        printf("[TEST] FAIL(3): the Combo section has no \"Windows\" page, so none of its three window rows can be "
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
            COMBO_CHECK(row->raceDisable == expected.raceDisable,
                        "row '%s' has RaceDisable(%s), expected RaceDisable(%s)", expected.rowName,
                        row->raceDisable ? "true" : "false", expected.raceDisable ? "true" : "false");
            auto options = std::static_pointer_cast<UIWidgets::WindowButtonOptions>(row->options);
            COMBO_CHECK(options != nullptr && !options->embedWindow,
                        "row '%s' embeds its window; SoH's own tracker toggles pop their windows out, and the embed "
                        "path calls DrawElement() directly, past the window's own Draw",
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
        // Exactly the live-play tools (ADR 0004's 2026-09-27 host amendment):
        // MM's randomizer options are pages now, and a button opening a second
        // surface over the same keys would be a way for the two to disagree.
        // Every window button on the page must be one of the rows above.
        COMBO_CHECK(FindRow(windows, "Toggle MM Randomizer Options") == nullptr,
                    "the Windows page still offers \"Toggle MM Randomizer Options\"; MM's randomizer options are "
                    "the Combo > MM Randomizer and Combo > MM Tricks pages, not a window");
        // MM's tracker rows are gone by name, by window and by key: a renamed
        // row that still opens an MM tracker window is the same row.
        for (const RetiredRow& retired : kRetiredRows) {
            COMBO_CHECK(FindRow(windows, retired.rowName) == nullptr,
                        "the Windows page still offers \"%s\"; the unified Combo Tracker and Combo Item Tracker "
                        "supersede MM's tracker rows (#458 U6)",
                        retired.rowName);
            for (auto& column : windows.columnWidgets) {
                for (WidgetInfo& row : column) {
                    if (row.type != WIDGET_WINDOW_BUTTON) {
                        continue;
                    }
                    COMBO_CHECK(row.windowName == nullptr || std::string(row.windowName) != retired.windowName,
                                "row '%s' still opens MM's \"%s\" window from the Windows page (#458 U6)",
                                row.name.c_str(), retired.windowName);
                    COMBO_CHECK(row.cVar == nullptr || std::string(row.cVar) != retired.cVar,
                                "row '%s' still binds MM's '%s' key on the Windows page (#458 U6)", row.name.c_str(),
                                retired.cVar);
                }
            }
        }
        int windowButtons = 0;
        for (auto& column : windows.columnWidgets) {
            for (WidgetInfo& row : column) {
                if (row.type == WIDGET_WINDOW_BUTTON) {
                    windowButtons++;
                }
            }
        }
        COMBO_CHECK(windowButtons == (int)(sizeof(kWindowRows) / sizeof(kWindowRows[0])),
                    "the Windows page holds %d window buttons, expected exactly the %d live-play tools above",
                    windowButtons, (int)(sizeof(kWindowRows) / sizeof(kWindowRows[0])));
        printf("[TEST] leg 3: all %d cross-game window rows carry a matching CVar/WindowName pair, stay pop-out, and "
               "sit under their own separator with SoH's tooltip, and the page holds no other window button and none "
               "of MM's %d retired tracker rows\n",
               (int)(sizeof(kWindowRows) / sizeof(kWindowRows[0])),
               (int)(sizeof(kRetiredRows) / sizeof(kRetiredRows[0])));
    }

    // ---- Leg 4: our pages under SoH's Randomizer header (#1101) ------------
    // The operator's ruling of 2026-10-21 (option C): after Tricks/Glitches and
    // before Plandomizer, in the order MM Randomizer, MM Locations, MM Tricks,
    // Cross-Game (MM Locations since lane R3). "Cross-Game" is the
    // rules page under the name the pointer page kept alive, so a config holding
    // it in gSettings.Menu.RandomizerSidebarSection lands on the rules.
    // AddMenuRandomizer() as a whole cannot run ROM-free, so SoH's neighbours are
    // stand-ins registered the way it registers them.
    {
        ComboSectionMenuProbe rando;
        rando.AddMenuEntry("Randomizer", "gSettings.Menu.RandomizerSidebarSection");
        WidgetPath randoPath = { "Randomizer", "Tricks/Glitches", SECTION_COLUMN_1 };
        rando.AddSidebarEntry("Randomizer", randoPath.sidebarName, 1);
        rando.AddWidget(randoPath, "Tricks/Glitches", WIDGET_SEPARATOR_TEXT);
        // A deliberately WRONG incoming column and sidebar: every page pins its
        // own, because SoH's option groups leave path.column wherever their last
        // COLUMN container landed (option.cpp).
        randoPath.sidebarName = "Ignored";
        randoPath.column = SECTION_COLUMN_3;
        SohGui::AddComboRandomizerPages(rando, randoPath);
        randoPath.sidebarName = "Plandomizer";
        randoPath.column = SECTION_COLUMN_1;
        rando.AddSidebarEntry("Randomizer", randoPath.sidebarName, 1);
        rando.AddWidget(randoPath, "Plandomizer", WIDGET_SEPARATOR_TEXT);

        MainMenuEntry& randoEntry = rando.Entries().at("Randomizer");
        const std::vector<std::string> kExpectedOrder = { "Tricks/Glitches", "MM Randomizer", "MM Locations",
                                                          "MM Tricks",       "Cross-Game",    "Plandomizer" };
        std::string got;
        for (const std::string& name : randoEntry.sidebarOrder) {
            got += (got.empty() ? "" : ", ") + name;
        }
        COMBO_CHECK(randoEntry.sidebarOrder == kExpectedOrder, "the Randomizer sidebar reads [%s], expected [%s]",
                    got.c_str(), "Tricks/Glitches, MM Randomizer, MM Locations, MM Tricks, Cross-Game, Plandomizer");
        COMBO_CHECK(SohGui::ComboRandomizerPageNames() ==
                        std::vector<std::string>({ "MM Randomizer", "MM Locations", "MM Tricks", "Cross-Game" }),
                    "ComboRandomizerPageNames() does not name MM Randomizer, MM Locations, MM Tricks, Cross-Game in "
                    "that order");

        if (!randoEntry.sidebars.contains("Cross-Game")) {
            printf("[TEST] FAIL(4): Randomizer / Cross-Game is gone. Every config holding \"Cross-Game\" in "
                   "gSettings.Menu.RandomizerSidebarSection now selects a sidebar that does not exist\n");
            gFailures++;
        } else {
            SidebarEntry& page = randoEntry.sidebars.at("Cross-Game");
            // One column, as the rules page has been since #844.
            COMBO_CHECK(page.columnCount == 1, "Randomizer / Cross-Game declares %u columns, expected 1",
                        page.columnCount);
            COMBO_CHECK(FindRow(page, "Combo Rules Status") != nullptr,
                        "Randomizer / Cross-Game has no \"Combo Rules Status\" row: it is not the rules page");
            COMBO_CHECK(FindRow(page, "Reset Combo Rules") != nullptr,
                        "Randomizer / Cross-Game has no \"Reset Combo Rules\" row: it is not the rules page");
            COMBO_CHECK(RowCount(page) >= 6,
                        "Randomizer / Cross-Game holds %zu widgets; the tier-4 settings alone are a status line, two "
                        "group separators, two comboboxes and a button",
                        RowCount(page));
            COMBO_CHECK(FindRow(page, "Moved to Combo") == nullptr,
                        "Randomizer / Cross-Game still carries the pointer row \"Moved to Combo\"");
        }
        for (const std::string& name : randoEntry.sidebarOrder) {
            SidebarEntry& page = randoEntry.sidebars.at(name);
            COMBO_CHECK(RowCount(page) > 0, "Randomizer / %s is registered with no widgets -- #640's failure mode",
                        name.c_str());
            for (uint32_t column = 0; column < page.columnWidgets.size(); column++) {
                for (WidgetInfo& row : page.columnWidgets.at(column)) {
                    COMBO_CHECK(column < page.columnCount,
                                "Randomizer / %s row '%s' landed in column %u of a page that draws %u -- registered "
                                "and never drawn",
                                name.c_str(), row.name.c_str(), column, page.columnCount);
                }
            }
        }
        COMBO_CHECK(!randoEntry.sidebars.contains("Ignored"),
                    "AddComboRandomizerPages registered rows under the stale incoming sidebar name");
        printf("[TEST] leg 4: MM Randomizer, MM Locations, MM Tricks and Cross-Game (the rules) sit between "
               "Tricks/Glitches and "
               "Plandomizer under the Randomizer header, every row in a drawn column\n");
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
    std::size_t baselineComboPages = 0;
    for (const SohGui::ComboSectionPage& page : SohGui::GetComboSectionPages()) {
        if (page.host == SohGui::ComboPageHost::Combo) {
            baselineComboPages++;
        }
    }

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
        // shipped one out from under a player's persisted selection; a page
        // hosted under Randomizer is not created here at all.
        auto& order = extProbe.Entries().at("Combo").sidebarOrder;
        COMBO_CHECK(order.size() == baselineComboPages + 2,
                    "the Combo section has %zu sidebars in order, expected the shipped Windows plus the %zu "
                    "Combo-hosted pages already registered plus the one this leg added",
                    order.size(), baselineComboPages);
        if (!order.empty()) {
            COMBO_CHECK(order.front() == "Windows" && order.back() == kExtPageName,
                        "sidebar order is [%s, ..., %s]; the shipped page must come first and contributed ones last",
                        order.front().c_str(), order.back().c_str());
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

    // The Randomizer host (#1101): the same registry, drawn by
    // AddComboRandomizerPages after the pages already registered there and
    // before Cross-Game, and never by AddMenuCombo.
    SohGui::RegisterComboSectionPage(kExtRandoPageName, 1, ExtRegistrar, SohGui::ComboPageHost::Randomizer);
    {
        ComboSectionMenuProbe hostProbe;
        gExtRan = 0;
        hostProbe.AddMenuCombo();
        COMBO_CHECK(!hostProbe.Entries().at("Combo").sidebars.contains(kExtRandoPageName),
                    "a page registered for the Randomizer header was created under Combo");
        COMBO_CHECK(gExtRan == 0, "AddMenuCombo ran a Randomizer-hosted registrar");

        hostProbe.AddMenuEntry("Randomizer", "gSettings.Menu.RandomizerSidebarSection");
        WidgetPath hostPath = { "Randomizer", "Ignored", SECTION_COLUMN_2 };
        gExtSectionName.clear();
        gExtColumn = -1;
        SohGui::AddComboRandomizerPages(hostProbe, hostPath);
        COMBO_CHECK(gExtRan == 1, "the Randomizer-hosted registrar ran %d times, expected once", gExtRan);
        COMBO_CHECK(gExtSectionName == "Randomizer" && gExtColumn == (int)SECTION_COLUMN_1,
                    "the Randomizer-hosted registrar was handed section '%s' column %d, expected 'Randomizer' and the "
                    "pinned first column",
                    gExtSectionName.c_str(), gExtColumn);
        const auto& order = hostProbe.Entries().at("Randomizer").sidebarOrder;
        COMBO_CHECK(order.size() >= 2 && order.at(order.size() - 2) == kExtRandoPageName &&
                        order.back() == SohGui::kComboRulesRandomizerPage,
                    "the Randomizer-hosted page is not the last page before %s", SohGui::kComboRulesRandomizerPage);
        const std::vector<std::string> names = SohGui::ComboRandomizerPageNames();
        COMBO_CHECK(names == order,
                    "ComboRandomizerPageNames() disagrees with what AddComboRandomizerPages registered");
    }
    COMBO_CHECK(SohGui::UnregisterComboSectionPage(kExtRandoPageName),
                "UnregisterComboSectionPage did not remove the Randomizer-hosted page");

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
    printf("[TEST] leg 5: a contributed page is created after the shipped one with a pinned column, replaces on "
           "re-registration, is refused when invalid, is drawn only under the header it names, and unregisters "
           "cleanly\n");

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
            { "Save Files", "Save Files" }, // a page never renamed is left alone
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
    // The pages #1101 moved to the Randomizer header, carried ACROSS headers. A
    // config that had Combo open on one of them opens on the moved page under
    // Randomizer (Menu::DrawElement reads gSettings.Menu.ActiveHeader, then that
    // header's sidebar key); one whose open header was another leaves the
    // Randomizer selection alone. Either way no Combo selection is left naming
    // a page Combo no longer has. All three keys are restored afterwards.
    {
        const char* kComboKey = "gSettings.Menu.ComboSidebarSection";
        const char* kRandoKey = "gSettings.Menu.RandomizerSidebarSection";
        const char* kHeaderKey = "gSettings.Menu.ActiveHeader";
        const std::string savedCombo = CVarGetString(kComboKey, "");
        const std::string savedRando = CVarGetString(kRandoKey, "");
        const std::string savedHeader = CVarGetString(kHeaderKey, "");
        struct Move {
            const char* header;
            const char* comboBefore;
            const char* headerAfter;
            const char* randoAfter;
        };
        const Move kMoves[] = {
            { "Combo", "MM Randomizer", "Randomizer", "MM Randomizer" },
            { "Combo", "MM Tricks", "Randomizer", "MM Tricks" },
            { "Combo", "Cross-Game Rules", "Randomizer", "Cross-Game" },
            // Another header open: the Randomizer selection is the player's own.
            { "Settings", "MM Randomizer", "Settings", "Shuffles" },
        };
        const std::vector<std::string> randoPages = SohGui::ComboRandomizerPageNames();
        for (const Move& move : kMoves) {
            CVarSetString(kHeaderKey, move.header);
            CVarSetString(kComboKey, move.comboBefore);
            CVarSetString(kRandoKey, "Shuffles");
            ComboSectionMenuProbe moveProbe;
            moveProbe.AddMenuCombo();
            const std::string header = CVarGetString(kHeaderKey, "");
            const std::string rando = CVarGetString(kRandoKey, "");
            const std::string comboNow = CVarGetString(kComboKey, "");
            COMBO_CHECK(header == move.headerAfter && rando == move.randoAfter,
                        "a config on %s > %s opens on %s > %s after AddMenuCombo, expected %s > %s", move.header,
                        move.comboBefore, header.c_str(), rando.c_str(), move.headerAfter, move.randoAfter);
            COMBO_CHECK(std::string(move.randoAfter) == "Shuffles" ||
                            std::find(randoPages.begin(), randoPages.end(), rando) != randoPages.end(),
                        "the carried Randomizer selection \"%s\" is not a page AddComboRandomizerPages registers",
                        rando.c_str());
            COMBO_CHECK(comboNow.empty() || moveProbe.Entries().at("Combo").sidebars.contains(comboNow),
                        "the Combo selection still reads \"%s\", a page the Combo header no longer has",
                        comboNow.c_str());
        }
        auto restore = [](const char* key, const std::string& value) {
            if (value.empty()) {
                CVarClear(key);
            } else {
                CVarSetString(key, value.c_str());
            }
        };
        restore(kComboKey, savedCombo);
        restore(kRandoKey, savedRando);
        restore(kHeaderKey, savedHeader);
        printf("[TEST] leg 6: a config open on Combo > MM Randomizer, MM Tricks or Cross-Game Rules opens on the "
               "moved page under Randomizer, and no Combo selection is left stranded\n");
    }

    // ---- Leg 7: Combo > Tracker Settings writes the unified trackers' keys ----
    // ADR 0004's 2026-09-30 amendment: the unified trackers are live-play windows
    // (Combo > Windows) and their settings are a Combo page. Every
    // gCombo.Tracker.Items.* key in the manifest must have exactly one row here,
    // with the overlay's own default and range, or the console is a player's only
    // writer for it. The Combo Tracker draws its checks in SoH's Check Tracker
    // colours, so the page embeds SoH's own Check Tracker Settings.
    if (!combo.sidebars.contains(kTrackerSettingsPage)) {
        printf("[TEST] FAIL(7): the Combo section has no \"%s\" page, so the Combo Item Tracker's settings "
               "(gCombo.Tracker.Items.*) have no menu writer and the Combo Tracker's colours no Combo host\n",
               kTrackerSettingsPage);
        gFailures++;
    } else {
        SidebarEntry& page = combo.sidebars.at(kTrackerSettingsPage);
        COMBO_CHECK(page.columnCount == 1, "Combo / %s declares %u columns, expected 1 (SoH's tracker pages)",
                    kTrackerSettingsPage, page.columnCount);
        auto rowFor = [&](const char* key) -> WidgetInfo* {
            for (auto& column : page.columnWidgets) {
                for (WidgetInfo& row : column) {
                    if (row.cVar != nullptr && std::string(row.cVar) == key) {
                        return &row;
                    }
                }
            }
            return nullptr;
        };
        std::size_t itemKeys = 0;
        for (std::size_t k = 0; k < RSBS::kComboKeyCount; k++) {
            const char* key = RSBS::kComboKeys[k].key;
            if (!RSBS::ComboKeyHasPrefix(key, kItemTrackerKeyPrefix)) {
                continue;
            }
            itemKeys++;
            int rows = 0;
            for (auto& column : page.columnWidgets) {
                for (WidgetInfo& row : column) {
                    if (row.cVar != nullptr && std::string(row.cVar) == key) {
                        rows++;
                        const char* tip = row.options != nullptr ? row.options->tooltip : nullptr;
                        COMBO_CHECK(tip != nullptr && tip[0] != '\0', "row '%s' (%s) has no tooltip", row.name.c_str(),
                                    key);
                        COMBO_CHECK(!row.raceDisable, "row '%s' is race-disabled; the overlay's look is cosmetic",
                                    row.name.c_str());
                    }
                }
            }
            COMBO_CHECK(rows == 1, "Combo / %s holds %d rows writing %s, expected exactly one", kTrackerSettingsPage,
                        rows, key);
        }
        COMBO_CHECK(itemKeys == 9, "the manifest holds %zu %s* keys; this leg was written against nine", itemKeys,
                    kItemTrackerKeyPrefix);

        // Each row's widget, default and range are the overlay's
        // (ComboItemTrackerWindow.cpp reads every key with these defaults and
        // clamps), so the page never shows a value the overlay is not drawing.
        if (WidgetInfo* row = rowFor(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE)) {
            auto o = std::static_pointer_cast<UIWidgets::ComboboxOptions>(row->options);
            COMBO_CHECK(row->type == WIDGET_CVAR_COMBOBOX &&
                            o->defaultIndex == (uint32_t)ComboGui::COMBO_ITEM_TRACKER_FLOATING &&
                            o->comboMap.size() == 2 && o->comboMap.contains(ComboGui::COMBO_ITEM_TRACKER_FLOATING) &&
                            o->comboMap.contains(ComboGui::COMBO_ITEM_TRACKER_WINDOW),
                        "the Window Type row is not a combobox over Floating/Window defaulting to Floating");
        }
        const char* kBoolKeys[] = { RSBS_CVAR_COMBO_ITEMS_DRAGGABLE, RSBS_CVAR_COMBO_ITEMS_SHOW_ONLY_PAUSED,
                                    RSBS_CVAR_COMBO_ITEMS_SECTION_OOT, RSBS_CVAR_COMBO_ITEMS_SECTION_MM,
                                    RSBS_CVAR_COMBO_ITEMS_SECTION_SHARED };
        const bool kBoolDefaults[] = { false, false, true, true, true };
        for (std::size_t i = 0; i < sizeof(kBoolKeys) / sizeof(kBoolKeys[0]); i++) {
            if (WidgetInfo* row = rowFor(kBoolKeys[i])) {
                auto o = std::static_pointer_cast<UIWidgets::CheckboxOptions>(row->options);
                COMBO_CHECK(row->type == WIDGET_CVAR_CHECKBOX && o->defaultValue == kBoolDefaults[i],
                            "the %s row is not a checkbox defaulting to %s, the overlay's default", kBoolKeys[i],
                            kBoolDefaults[i] ? "on" : "off");
            }
        }
        if (WidgetInfo* row = rowFor(RSBS_CVAR_COMBO_ITEMS_ICON_SIZE)) {
            auto o = std::static_pointer_cast<UIWidgets::IntSliderOptions>(row->options);
            COMBO_CHECK(row->type == WIDGET_CVAR_SLIDER_INT && o->min == ComboGui::kComboItemTrackerIconSizeMin &&
                            o->max == ComboGui::kComboItemTrackerIconSizeMax &&
                            o->defaultValue == ComboGui::kComboItemTrackerIconSize,
                        "the Icon Size row is not an int slider over %d..%d defaulting to %d",
                        ComboGui::kComboItemTrackerIconSizeMin, ComboGui::kComboItemTrackerIconSizeMax,
                        ComboGui::kComboItemTrackerIconSize);
        }
        if (WidgetInfo* row = rowFor(RSBS_CVAR_COMBO_ITEMS_OPACITY)) {
            auto o = std::static_pointer_cast<UIWidgets::FloatSliderOptions>(row->options);
            COMBO_CHECK(row->type == WIDGET_CVAR_SLIDER_FLOAT && o->min == 0.1f && o->max == 1.0f &&
                            o->defaultValue == 1.0f && o->isPercentage,
                        "the Opacity row is not a percentage slider over 0.1..1 defaulting to 1, the overlay's clamp");
        }
        if (WidgetInfo* row = rowFor(RSBS_CVAR_COMBO_ITEMS_BG_COLOR)) {
            auto o = std::static_pointer_cast<UIWidgets::ColorPickerOptions>(row->options);
            COMBO_CHECK(row->type == WIDGET_CVAR_COLOR_PICKER && o->useAlpha && o->defaultValue.r == 0 &&
                            o->defaultValue.g == 0 && o->defaultValue.b == 0 && o->defaultValue.a == 0,
                        "the Background Color row is not an alpha colour picker defaulting to SoH's clear black");
            // Its Reset and Random buttons draw in the menu theme, as SoH's Item
            // Tracker Settings picker does. Menu::MenuDrawItem themes every
            // other interactive row but draws a picker in its options' own
            // colour (ColorPickerOptions' default is Gray), so only the row's
            // PreFunc can theme it. The PreFunc reads THEME_COLOR, which is
            // SohGui::mSohMenu's theme: point it at the probe for the call and
            // drive two themes, so a fixed colour cannot pass.
            COMBO_CHECK(row->preFunc != nullptr,
                        "the Background Color row has no PreFunc, so its Reset and Random buttons draw Gray, not in "
                        "the menu theme");
            if (row->preFunc != nullptr) {
                const char* kThemeKey = "gSettings.Menu.Theme";
                const int savedTheme = CVarGetInteger(kThemeKey, -1);
                std::shared_ptr<SohGui::SohMenu> savedMenu = SohGui::mSohMenu;
                SohGui::mSohMenu = std::shared_ptr<SohGui::SohMenu>(&probe, [](SohGui::SohMenu*) {});
                for (UIWidgets::Colors theme : { UIWidgets::Colors::Red, UIWidgets::Colors::Green }) {
                    CVarSetInteger(kThemeKey, static_cast<int32_t>(theme));
                    probe.UpdateElement();
                    o->color = UIWidgets::Colors::Gray;
                    row->ResetDisables();
                    row->preFunc(*row);
                    COMBO_CHECK(o->color == theme,
                                "the Background Color picker draws in colour %d under menu theme %d, so its Reset and "
                                "Random buttons do not follow the theme",
                                static_cast<int>(o->color), static_cast<int>(theme));
                }
                SohGui::mSohMenu = savedMenu;
                if (savedTheme < 0) {
                    CVarClear(kThemeKey);
                } else {
                    CVarSetInteger(kThemeKey, savedTheme);
                }
                probe.UpdateElement();
            }
        }

        // Dragging and Only While Paused apply to the floating overlay only, as
        // in SoH's Item Tracker Settings: hidden while the overlay is a window.
        const char* kFloatingOnly[] = { RSBS_CVAR_COMBO_ITEMS_DRAGGABLE, RSBS_CVAR_COMBO_ITEMS_SHOW_ONLY_PAUSED };
        const int savedType = CVarGetInteger(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE, -1);
        for (const char* key : kFloatingOnly) {
            WidgetInfo* row = rowFor(key);
            if (row == nullptr) {
                continue;
            }
            COMBO_CHECK(row->preFunc != nullptr, "the %s row has no PreFunc, so it shows for a windowed overlay", key);
            if (row->preFunc == nullptr) {
                continue;
            }
            CVarSetInteger(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE, ComboGui::COMBO_ITEM_TRACKER_WINDOW);
            row->ResetDisables();
            row->preFunc(*row);
            COMBO_CHECK(row->isHidden, "the %s row shows while the overlay is a window", key);
            CVarSetInteger(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE, ComboGui::COMBO_ITEM_TRACKER_FLOATING);
            row->ResetDisables();
            row->preFunc(*row);
            COMBO_CHECK(!row->isHidden, "the %s row is hidden while the overlay floats", key);
        }
        if (savedType < 0) {
            CVarClear(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE);
        } else {
            CVarSetInteger(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE, savedType);
        }

        // The Combo Tracker draws its checks in SoH's Check Tracker colours
        // (ComboTrackerWindow.cpp reads gTrackers.CheckTracker.*), so the page
        // hosts SoH's own Check Tracker Settings as SoH's Check Tracker page does:
        // embedded, under its pop-out row.
        WidgetInfo* check = FindRow(page, "Popout Check Tracker Settings");
        COMBO_CHECK(check != nullptr, "Combo / %s has no \"Popout Check Tracker Settings\" row", kTrackerSettingsPage);
        if (check != nullptr) {
            auto o = std::static_pointer_cast<UIWidgets::WindowButtonOptions>(check->options);
            COMBO_CHECK(check->type == WIDGET_WINDOW_BUTTON && check->cVar != nullptr &&
                            std::string(check->cVar) == "gOpenWindows.CheckTrackerSettings" &&
                            check->windowName != nullptr &&
                            std::string(check->windowName) == "Check Tracker Settings" && o->embedWindow,
                        "the Check Tracker Settings row is not SoH's embedded pop-out over "
                        "gOpenWindows.CheckTrackerSettings / \"Check Tracker Settings\"");
            // #1128: the Combo Tracker follows that window's hide options and
            // header toggles now, not its colors alone, and the tooltip says so.
            const std::string tip = (o != nullptr && o->tooltip != nullptr) ? o->tooltip : "";
            COMBO_CHECK(tip.find("no other setting") == std::string::npos &&
                            tip.find("Hide options") != std::string::npos,
                        "the Check Tracker Settings row's tooltip \"%s\" does not say the Combo Tracker follows its "
                        "Hide options",
                        tip.c_str());
        }

        // Move to Default Position (#962) is the one row that writes no key: it asks the
        // floating overlay to return to its default place (under the hearts),
        // and, like Enable Dragging, shows only while the overlay floats.
        WidgetInfo* resetPos = FindRow(page, "Move to Default Position##ComboItemTracker");
        COMBO_CHECK(resetPos != nullptr && resetPos->type == WIDGET_BUTTON && resetPos->callback != nullptr &&
                        resetPos->cVar == nullptr,
                    "Combo / %s has no Move to Default Position button with a Callback and no key",
                    kTrackerSettingsPage);
        if (resetPos != nullptr && resetPos->preFunc != nullptr) {
            const int savedResetType = CVarGetInteger(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE, -1);
            CVarSetInteger(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE, ComboGui::COMBO_ITEM_TRACKER_WINDOW);
            resetPos->ResetDisables();
            resetPos->preFunc(*resetPos);
            COMBO_CHECK(resetPos->isHidden, "the Move to Default Position row shows while the overlay is a window");
            CVarSetInteger(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE, ComboGui::COMBO_ITEM_TRACKER_FLOATING);
            resetPos->ResetDisables();
            resetPos->preFunc(*resetPos);
            COMBO_CHECK(!resetPos->isHidden, "the Move to Default Position row is hidden while the overlay floats");
            if (savedResetType < 0) {
                CVarClear(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE);
            } else {
                CVarSetInteger(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE, savedResetType);
            }
        } else {
            COMBO_CHECK(false, "the Move to Default Position row has no PreFunc, so it shows for a windowed overlay");
        }

        // #1131: the Combo Tracker's own keys (gCombo.Tracker.Checks.*, which SoH
        // has no twin for) each have exactly one row, a checkbox with the window's
        // default, under the Check Tracker Settings separator (until L5 #1132 moves
        // the block onto SoH's Check Tracker page).
        std::size_t checkKeys = 0;
        for (std::size_t k = 0; k < RSBS::kComboKeyCount; k++) {
            const char* key = RSBS::kComboKeys[k].key;
            if (!RSBS::ComboKeyHasPrefix(key, kCheckTrackerKeyPrefix)) {
                continue;
            }
            checkKeys++;
            int rows = 0;
            for (auto& column : page.columnWidgets) {
                const char* section = "";
                for (WidgetInfo& row : column) {
                    if (row.type == WIDGET_SEPARATOR_TEXT) {
                        section = row.name.c_str();
                        continue;
                    }
                    if (row.cVar == nullptr || std::string(row.cVar) != key) {
                        continue;
                    }
                    rows++;
                    bool known = false;
                    bool defaultValue = false;
                    for (const auto& d : kCheckTrackerKeyDefaults) {
                        if (std::string(d.key) == key) {
                            known = true;
                            defaultValue = d.defaultValue;
                        }
                    }
                    auto o = std::static_pointer_cast<UIWidgets::CheckboxOptions>(row.options);
                    COMBO_CHECK(known && row.type == WIDGET_CVAR_CHECKBOX && o != nullptr &&
                                    o->defaultValue == defaultValue,
                                "the %s row is not a checkbox defaulting to the Combo Tracker's default", key);
                    COMBO_CHECK(o != nullptr && o->tooltip != nullptr && o->tooltip[0] != '\0',
                                "row '%s' (%s) has no tooltip", row.name.c_str(), key);
                    COMBO_CHECK(!row.raceDisable, "row '%s' is race-disabled; the tracker's navigation is cosmetic",
                                row.name.c_str());
                    COMBO_CHECK(std::string(section) == "Check Tracker Settings",
                                "the %s row is under \"%s\", not the Check Tracker Settings separator", key, section);
                }
            }
            COMBO_CHECK(rows == 1, "Combo / %s holds %d rows writing %s, expected exactly one", kTrackerSettingsPage,
                        rows, key);
        }
        COMBO_CHECK(checkKeys == 2, "the manifest holds %zu %s* keys, expected two (#1131)", checkKeys,
                    kCheckTrackerKeyPrefix);

        // No other writer: every other interactive row writes one of the keys above.
        for (auto& column : page.columnWidgets) {
            for (WidgetInfo& row : column) {
                if (row.type == WIDGET_SEPARATOR_TEXT || row.type == WIDGET_TEXT || &row == check || &row == resetPos) {
                    continue;
                }
                COMBO_CHECK(row.cVar != nullptr && (RSBS::ComboKeyHasPrefix(row.cVar, kItemTrackerKeyPrefix) ||
                                                    RSBS::ComboKeyHasPrefix(row.cVar, kCheckTrackerKeyPrefix)),
                            "row '%s' on Combo / %s writes '%s', which is not one of the unified trackers' keys",
                            row.name.c_str(), kTrackerSettingsPage, row.cVar != nullptr ? row.cVar : "(null)");
            }
        }
        printf("[TEST] leg 7: Combo / %s writes each of the nine %s* keys once with the overlay's defaults and "
               "ranges, hides the floating-only rows for a windowed overlay, themes its colour picker, embeds "
               "SoH's Check Tracker Settings, and writes the two %s* keys under it (#1131)\n",
               kTrackerSettingsPage, kItemTrackerKeyPrefix, kCheckTrackerKeyPrefix);
    }

    if (gFailures == 0) {
        printf("[TEST] menu-combo-section: PASS\n");
    } else {
        printf("[TEST] menu-combo-section: %d failure(s)\n", gFailures);
    }
    return gFailures;
}

#endif // RSBS_SINGLE_EXECUTABLE
