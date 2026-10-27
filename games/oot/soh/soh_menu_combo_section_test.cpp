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
 *  4. SoH'S TRACKER TOGGLES OPEN SoH's WINDOWS (#1132, operator rulings D1 and
 *     D7 of 2026-10-22). `WIDGET_WINDOW_BUTTON` reads `.CVar` only for its
 *     open/close label and calls the window's own `ToggleVisibility` by
 *     `.WindowName`, so SoH's Randomizer > Check Tracker and Item Tracker
 *     toggles open the unified windows only because those ARE "Check Tracker"
 *     and "Item Tracker" over SoH's own window keys. Leg 3 drives SoH's two
 *     pages row for row through the contributed-rows registry and pins that,
 *     the Check Tracker page's rows of ours (#1131's two navigation keys and
 *     the Cross-Game Spoiler's toggle, which Combo > Windows held until this
 *     lane retired it) and their order, and that neither page opens an MM
 *     tracker window or binds an MM `gWindows.*` key (#458 U6).
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
 *  7. A RENAMED OR MOVED PAGE STRANDS THE SELECTION. "MM Enhancements" became
 *     "Majora's Mask" on 2026-09-27, because its 192 px label left the selected
 *     highlight no padding. Selection persists by display name in
 *     gSettings.Menu.ComboSidebarSection, so leg 6 drives AddMenuCombo over a
 *     config holding the old name and requires the new one afterwards (a
 *     registered page, so the carry table cannot name a page that does not
 *     exist), and leaves every other value alone. The pages that left the
 *     header are carried ACROSS headers: the three #1101 moved to Randomizer,
 *     and Windows (#1132; "Cross-Game Windows" until 2026-09-27), whose
 *     trackers open from Randomizer > Check Tracker. A config that had Combo
 *     open on one of them opens on that page under Randomizer, and no Combo
 *     selection is left naming a page the header no longer has.
 *
 *  8. THE ITEM TRACKER OVERLAY'S SETTINGS HAVE NO MENU WRITER. Leg 7 requires
 *     Combo > Tracker Settings to write every gCombo.Tracker.Items.* key in the
 *     manifest exactly once, with the overlay's own widget, default and range,
 *     to hide the floating-only rows for a windowed overlay and to draw its
 *     colour picker in the menu theme, and (#1132) to hold nothing of the Check
 *     Tracker's any more: a transitional page until lane IT-3 (#1141) moves the
 *     overlay's rows onto SoH's Item Tracker page.
 *
 *  9. SoH's OWN TRACKER WINDOWS ARE UNREGISTERED, OR DRAWN (#1132). Their
 *     InitElement registers the hooks and save sections the unified panes read,
 *     so they must stay on the Gui, never drawn and never opened by SoH's key.
 *     Leg 8 holds RetiredInSingleExe<> on a spy, then the two real windows on a
 *     bare Gui beside the unified ones: every hook registered once, no key
 *     latched, and "Check Tracker" / "Item Tracker" naming the unified windows.
 *
 * WHAT IS NOT COVERED. The six tier-4 rules' own behaviour — the staging buffers,
 * the freeze gate, the values-from-the-save rule — is `ComboSettingsRows`, which
 * followed the rows here. This row owns the section's SHAPE. Appearance is
 * operator verification on the nightly.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "soh/SohGui/SohMenu.h"
// Leg 8: SoH's two tracker windows as SetupGuiElements constructs them, and the
// hooks and save sections their InitElement registers.
#include "soh/SohGui/OoTActiveGated.h"
#include "soh/SohGui/SohRetiredWindow.h"
#include "soh/Enhancements/randomizer/randomizer_check_tracker.h"
#include "soh/Enhancements/randomizer/randomizer_item_tracker.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/SaveManager.h"

#include "ComboItemTrackerWindow.h" // legs 3, 7, 8: the overlay's name, key, window types and Icon Size range
#include "ComboTrackerWindow.h"     // legs 3 and 8: the Check Tracker's name and key
#include "context.h"                // leg 8: the running game OoTActiveGated<> reads
#include "cvar_shared_keys.h"       // legs 3 and 7: RSBS::kComboKeys and the gCombo.Tracker.* keys

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <memory>
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
// opens the column. SoH's tracker pages' shape is "separator, then its
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

// ---- Leg 3: SoH's own tracker pages, as AddMenuRandomizer registers them ----
// AddMenuRandomizer() as a whole cannot run ROM-free, so these are its two
// tracker pages row for row (SohMenuRandomizer.cpp: the Item Tracker and Check
// Tracker pages, whose literals ComboTrackerWindow's #1132 leg pins in source),
// each ending in the same AddContributedSohPageRows call. CVAR_WINDOW(x) is
// "gOpenWindows." x (CMake/soh-cvars.cmake, pinned there too).
void AddSohTrackerPage(SohGui::SohMenu& menu, const char* window, const char* windowKey, const char* settingsWindow,
                       const char* settingsKey) {
    const std::string w = window;
    WidgetPath path = { "Randomizer", w, SECTION_COLUMN_1 };
    menu.AddSidebarEntry("Randomizer", path.sidebarName, 1);
    menu.AddWidget(path, w, WIDGET_SEPARATOR_TEXT);
    menu.AddWidget(path, "Toggle " + w, WIDGET_WINDOW_BUTTON)
        .CVar(windowKey)
        .RaceDisable(false)
        .WindowName(window)
        .HideInSearch(true)
        .Options(UIWidgets::WindowButtonOptions().Tooltip("Toggles the tracker.").EmbedWindow(false));
    menu.AddWidget(path, w + " Settings", WIDGET_SEPARATOR_TEXT);
    menu.AddWidget(path, "Popout " + w + " Settings", WIDGET_WINDOW_BUTTON)
        .CVar(settingsKey)
        .RaceDisable(false)
        .WindowName(settingsWindow)
        .HideInSearch(true)
        .Options(UIWidgets::WindowButtonOptions().Tooltip("Enables the separate settings window."));
    // A deliberately WRONG column: the contributions pin their own.
    path.column = SECTION_COLUMN_3;
    SohGui::AddContributedSohPageRows(menu, path);
}

void AddSohTrackerPages(SohGui::SohMenu& menu) {
    menu.AddMenuEntry("Randomizer", "gSettings.Menu.RandomizerSidebarSection");
    AddSohTrackerPage(menu, "Item Tracker", "gOpenWindows.ItemTracker", "Item Tracker Settings",
                      "gOpenWindows.ItemTrackerSettings");
    AddSohTrackerPage(menu, "Check Tracker", "gOpenWindows.CheckTracker", "Check Tracker Settings",
                      "gOpenWindows.CheckTrackerSettings");
}

/** The page's rows in registration order, by name. */
std::vector<std::string> RowNames(SidebarEntry& page) {
    std::vector<std::string> names;
    for (auto& column : page.columnWidgets) {
        for (WidgetInfo& row : column) {
            names.push_back(row.name);
        }
    }
    return names;
}

std::string Joined(const std::vector<std::string>& names) {
    std::string out;
    for (const std::string& name : names) {
        out += (out.empty() ? "" : ", ") + name;
    }
    return out;
}

// The SoH-page extension point's observer (leg 3), as leg 5's is the Combo one's.
int gSohRowsRan = 0;
std::string gSohRowsSection;
std::string gSohRowsSidebar;
int gSohRowsColumn = -1;
const char* kSohRowsRowName = "Contributed SoH Page Row";

void SohRowsRegistrar(SohGui::SohMenu& menu, WidgetPath& path) {
    gSohRowsRan++;
    gSohRowsSection = path.sectionName;
    gSohRowsSidebar = path.sidebarName;
    gSohRowsColumn = (int)path.column;
    menu.AddWidget(path, kSohRowsRowName, WIDGET_SEPARATOR_TEXT);
}

// ---- Leg 8: SoH's two tracker windows, registered and never drawn -----------

/** A window that counts what reaches it, under RetiredInSingleExe<>. */
class RetireSpyWindow : public Ship::GuiWindow {
  public:
    using Ship::GuiWindow::GuiWindow;
    int inits = 0;
    int draws = 0;
    int elements = 0;

    void Draw() override {
        draws++;
    }
    void DrawElement() override {
        elements++;
    }

  protected:
    void InitElement() override {
        inits++;
    }
    void UpdateElement() override {
    }
};

template <typename H> std::vector<HOOK_ID> HookIds() {
    std::vector<HOOK_ID> ids;
    for (auto& [id, fn] : GameInteractor::RegisteredGameHooks<H>::functions) {
        ids.push_back(id);
    }
    return ids;
}

template <typename H> std::vector<HOOK_ID> NewHookIds(const std::vector<HOOK_ID>& before) {
    std::vector<HOOK_ID> added;
    for (HOOK_ID id : HookIds<H>()) {
        if (std::find(before.begin(), before.end(), id) == before.end()) {
            added.push_back(id);
        }
    }
    return added;
}

/** One hook type's ids at construction (the base), at Mark(), and what was added since. */
struct HookWatchBase {
    virtual ~HookWatchBase() = default;
    virtual void Mark() = 0;
    virtual void Drop() = 0;
};

template <typename H> struct HookWatch final : HookWatchBase {
    std::vector<HOOK_ID> base = HookIds<H>();
    std::vector<HOOK_ID> mark = base;
    void Mark() override {
        mark = HookIds<H>();
    }
    /** How many hooks of this type were registered after Mark(). */
    std::size_t Added() const {
        return NewHookIds<H>(mark).size();
    }
    /** Removes every hook of this type registered since construction. */
    void Drop() override {
        for (HOOK_ID id : NewHookIds<H>(base)) {
            GameInteractor::RegisteredGameHooks<H>::functions.erase(id);
            GameInteractor::RegisteredGameHooks<H>::hookData.erase(id);
        }
    }
};

/** An integer CVar's value, put back when the guard leaves scope. */
struct IntKeyGuard {
    const char* key;
    bool existed;
    int32_t value;
    explicit IntKeyGuard(const char* k) : key(k), existed(CVarExists(k)), value(CVarGetInteger(k, 0)) {
    }
    ~IntKeyGuard() {
        if (existed) {
            CVarSetInteger(key, value);
        } else {
            CVarClear(key);
        }
    }
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

    // ---- Leg 1: the section, its header CVar, and its pages -----------------
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

    // Every Combo page is a contributed one since #1132 retired Windows (the
    // unified trackers open from SoH's own tracker pages, leg 3). Each holds
    // widgets in every column it declares and draws every row it registers.
    for (const std::string& pageName : combo.sidebarOrder) {
        if (!combo.sidebars.contains(pageName)) {
            printf("[TEST] FAIL(1): \"%s\" is in the Combo header's sidebarOrder but not in its sidebars\n",
                   pageName.c_str());
            gFailures++;
            continue;
        }
        SidebarEntry& page = combo.sidebars.at(pageName);
        COMBO_CHECK(page.columnCount >= 1, "Combo / %s declares %u columns", pageName.c_str(), page.columnCount);
        // #640, verbatim: an empty multi-column page leaves Menu::DrawElement's
        // SetNextWindowPos unconsumed and undocks the "Main Game" window.
        COMBO_CHECK(RowCount(page) > 0,
                    "Combo / %s is registered with no widgets at all -- #640's failure mode, which "
                    "undocks libultraship's \"Main Game\" window rather than merely looking empty",
                    pageName.c_str());
        // #844: every column the page declares holds rows. Menu::DrawElement
        // divides the section by columnCount whether or not a column has rows,
        // so a declared column with none leaves an empty half beside the page
        // and squeezes every row into the other.
        for (uint32_t column = 0; column < page.columnCount; column++) {
            const std::size_t rowsInColumn =
                column < page.columnWidgets.size() ? page.columnWidgets.at(column).size() : 0;
            COMBO_CHECK(rowsInColumn > 0,
                        "Combo / %s declares %u columns but column %u holds no rows -- the page draws an empty "
                        "column and its rows take only part of the width (#844)",
                        pageName.c_str(), page.columnCount, column + 1);
        }
        // Every row must be in a column the page actually draws.
        for (uint32_t column = 0; column < page.columnWidgets.size(); column++) {
            for (WidgetInfo& row : page.columnWidgets.at(column)) {
                COMBO_CHECK(column < page.columnCount,
                            "Combo / %s row '%s' landed in column %u of a page that draws %u -- registered and never "
                            "drawn",
                            pageName.c_str(), row.name.c_str(), column, page.columnCount);
            }
        }
    }
    // The Combo header after #1132: exactly its four contributed pages, in
    // whatever order their file-scope registrars ran; no Windows page (ruling D1:
    // SoH's Randomizer > Check Tracker and Item Tracker open the unified
    // windows), and none of the three pages #1101 moved to the Randomizer header.
    {
        const std::vector<std::string> kComboPages = { "Majora's Mask", "MM Mods", "Save Files", "Tracker Settings" };
        const auto& order = combo.sidebarOrder;
        std::string got;
        for (const std::string& name : order) {
            got += (got.empty() ? "" : ", ") + name;
        }
        COMBO_CHECK(order.size() == kComboPages.size(), "the Combo header holds [%s], expected the %zu pages below",
                    got.c_str(), kComboPages.size());
        for (const std::string& name : kComboPages) {
            COMBO_CHECK(combo.sidebars.contains(name), "the Combo header has no \"%s\" page", name.c_str());
        }
        for (const char* gone : { "Windows", "Cross-Game Windows" }) {
            COMBO_CHECK(!combo.sidebars.contains(gone),
                        "the Combo header still holds \"%s\"; the unified trackers open from SoH's Randomizer > "
                        "Check Tracker and Item Tracker pages (#1132, ruling D1)",
                        gone);
        }
        for (const char* moved : { "Cross-Game Rules", "Cross-Game", "MM Randomizer", "MM Tricks" }) {
            COMBO_CHECK(!combo.sidebars.contains(moved),
                        "the Combo header still holds \"%s\"; the randomizer pages are under the Randomizer header "
                        "(operator ruling 2026-10-21)",
                        moved);
        }
    }
    printf("[TEST] leg 1: the Combo header exists with its own sidebar CVar and exactly its four contributed pages, "
           "no Windows page, each holding widgets in every column it declares\n");

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

    // ---- Leg 3: SoH's tracker pages are the unified windows' pages (#1132) ---
    // Ruling D1: SoH's Randomizer > Check Tracker and Item Tracker pages open the
    // unified windows, because those windows ARE "Check Tracker" and "Item
    // Tracker" over SoH's own window keys (ruling D7): WINDOW_BUTTON calls the
    // window it finds BY NAME, and reads .CVar for its label. The Check Tracker
    // rows that lived on Combo > Tracker Settings (#1131's two navigation keys)
    // and the Cross-Game Spoiler's toggle that lived on Combo > Windows follow
    // SoH's own rows on its Check Tracker page, through the contributed-rows
    // registry; the Item Tracker page takes none yet (lane IT-3, #1141, appends
    // the overlay's rows there).
    {
        struct Toggle {
            const char* page;
            const char* rowName;
            const char* comboWindow;
            const char* comboKey;
        };
        const Toggle kToggles[] = {
            { "Check Tracker", "Toggle Check Tracker", ComboGui::kComboTrackerWindowName,
              ComboGui::kComboTrackerVisibilityCVar },
            { "Item Tracker", "Toggle Item Tracker", ComboGui::kComboItemTrackerWindowName,
              ComboGui::kComboItemTrackerVisibilityCVar },
        };
        ComboSectionMenuProbe soh;
        AddSohTrackerPages(soh);
        auto& randoPages = soh.Entries().at("Randomizer").sidebars;
        for (const Toggle& t : kToggles) {
            WidgetInfo* row = randoPages.contains(t.page) ? FindRow(randoPages.at(t.page), t.rowName) : nullptr;
            COMBO_CHECK(row != nullptr, "the Randomizer / %s stand-in has no \"%s\" row", t.page, t.rowName);
            if (row == nullptr) {
                continue;
            }
            COMBO_CHECK(row->windowName != nullptr && std::string(row->windowName) == t.comboWindow,
                        "SoH's \"%s\" opens the window \"%s\", but the unified one is \"%s\": the button would open "
                        "SoH's own window, not ours (#1132, ruling D1)",
                        t.rowName, row->windowName != nullptr ? row->windowName : "(null)", t.comboWindow);
            COMBO_CHECK(row->cVar != nullptr && std::string(row->cVar) == t.comboKey,
                        "SoH's \"%s\" labels itself from \"%s\", but the unified window's key is \"%s\": the button "
                        "would read Open while the window is shut",
                        t.rowName, row->cVar != nullptr ? row->cVar : "(null)", t.comboKey);
        }

        // The Check Tracker page: SoH's four rows, then ours, in this order.
        const std::vector<std::string> kCheckPage = {
            "Check Tracker",
            "Toggle Check Tracker",
            "Check Tracker Settings",
            "Popout Check Tracker Settings",
            "Auto Scroll To Current Area##CheckTracker",
            "Only Show Current Area##CheckTracker",
            "Cross-Game Spoiler",
            "Toggle Cross-Game Spoiler",
        };
        // The Item Tracker page: SoH's four rows, nothing of ours yet (IT-3).
        const std::vector<std::string> kItemPage = {
            "Item Tracker",
            "Toggle Item Tracker",
            "Item Tracker Settings",
            "Popout Item Tracker Settings",
        };
        const std::vector<std::string> checkRows = RowNames(randoPages.at("Check Tracker"));
        const std::vector<std::string> itemRows = RowNames(randoPages.at("Item Tracker"));
        COMBO_CHECK(checkRows == kCheckPage, "Randomizer / Check Tracker reads [%s], expected [%s]",
                    Joined(checkRows).c_str(), Joined(kCheckPage).c_str());
        COMBO_CHECK(itemRows == kItemPage, "Randomizer / Item Tracker reads [%s], expected [%s]",
                    Joined(itemRows).c_str(), Joined(kItemPage).c_str());
        SidebarEntry& checkPage = randoPages.at("Check Tracker");
        for (uint32_t column = 1; column < checkPage.columnWidgets.size(); column++) {
            COMBO_CHECK(checkPage.columnWidgets.at(column).empty(),
                        "Randomizer / Check Tracker holds rows in column %u of a one-column page -- registered and "
                        "never drawn",
                        column + 1);
        }

        // #1131's two keys: each written once on the Check Tracker page, a
        // checkbox with the window's default and a tooltip, under SoH's Check
        // Tracker Settings separator, and not race-disabled.
        std::size_t checkKeys = 0;
        for (std::size_t k = 0; k < RSBS::kComboKeyCount; k++) {
            const char* key = RSBS::kComboKeys[k].key;
            if (!RSBS::ComboKeyHasPrefix(key, kCheckTrackerKeyPrefix)) {
                continue;
            }
            checkKeys++;
            int rows = 0;
            for (auto& column : checkPage.columnWidgets) {
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
                                "the %s row is not a checkbox defaulting to the Check Tracker's default", key);
                    COMBO_CHECK(o != nullptr && o->tooltip != nullptr && o->tooltip[0] != '\0' &&
                                    std::string(o->tooltip).find("Combo") == std::string::npos,
                                "row '%s' (%s) has no tooltip, or one that still says Combo (ruling D7)",
                                row.name.c_str(), key);
                    COMBO_CHECK(!row.raceDisable, "row '%s' is race-disabled; the tracker's navigation is cosmetic",
                                row.name.c_str());
                    COMBO_CHECK(std::string(section) == "Check Tracker Settings",
                                "the %s row is under \"%s\", not SoH's Check Tracker Settings separator", key, section);
                }
            }
            COMBO_CHECK(rows == 1, "Randomizer / Check Tracker holds %d rows writing %s, expected exactly one", rows,
                        key);
        }
        COMBO_CHECK(checkKeys == 2, "the manifest holds %zu %s* keys, expected two (#1131)", checkKeys,
                    kCheckTrackerKeyPrefix);

        // The Cross-Game Spoiler's toggle, moved from Combo > Windows: under its
        // own separator, race-disabled as a spoiler tool is, popped out, with
        // SoH's tracker-toggle tooltip and the window's constants restated.
        WidgetInfo* spoiler = FindRow(checkPage, "Toggle Cross-Game Spoiler");
        COMBO_CHECK(spoiler != nullptr, "Randomizer / Check Tracker has no \"Toggle Cross-Game Spoiler\" row");
        if (spoiler != nullptr) {
            auto o = std::static_pointer_cast<UIWidgets::WindowButtonOptions>(spoiler->options);
            const WidgetInfo* above = RowBefore(checkPage, spoiler);
            COMBO_CHECK(spoiler->type == WIDGET_WINDOW_BUTTON && spoiler->cVar != nullptr &&
                            std::string(spoiler->cVar) == "gCombo.Windows.Spoiler" && spoiler->windowName != nullptr &&
                            std::string(spoiler->windowName) == "Cross-Game Spoiler" && spoiler->raceDisable &&
                            o != nullptr && !o->embedWindow && o->tooltip != nullptr &&
                            std::string(o->tooltip) == "Toggles the Cross-Game Spoiler.",
                        "the Toggle Cross-Game Spoiler row is not a race-disabled pop-out over gCombo.Windows.Spoiler "
                        "/ \"Cross-Game Spoiler\" with SoH's tooltip");
            COMBO_CHECK(above != nullptr && above->type == WIDGET_SEPARATOR_TEXT && above->name == "Cross-Game Spoiler",
                        "the Toggle Cross-Game Spoiler row does not sit under its own separator");
        }

        // MM's four tracker rows stay retired (#458 U6): neither page opens an
        // MM tracker window or binds an MM gWindows.* key.
        const char* const kMmWindows[] = { "MM Item Tracker", "MM Item Tracker Settings", "MM Check Tracker",
                                           "MM Check Tracker Settings" };
        for (const char* pageName : { "Check Tracker", "Item Tracker" }) {
            for (auto& column : randoPages.at(pageName).columnWidgets) {
                for (WidgetInfo& row : column) {
                    if (row.type != WIDGET_WINDOW_BUTTON) {
                        continue;
                    }
                    for (const char* mm : kMmWindows) {
                        COMBO_CHECK(row.windowName == nullptr || std::string(row.windowName) != mm,
                                    "row '%s' on Randomizer / %s opens MM's \"%s\" window (#458 U6)", row.name.c_str(),
                                    pageName, mm);
                    }
                    COMBO_CHECK(row.cVar == nullptr || std::string(row.cVar).rfind("gWindows.", 0) != 0,
                                "row '%s' on Randomizer / %s binds MM's '%s' key (#458 U6)", row.name.c_str(), pageName,
                                row.cVar);
                }
            }
        }

        // The registry itself: a contribution reaches only the page it names,
        // with that page's path and the first column pinned, after SoH's rows;
        // an invalid one is refused; unregistering puts the registry back.
        SohGui::RegisterSohPageRows("Randomizer", "Item Tracker", SohRowsRegistrar);
        SohGui::RegisterSohPageRows(nullptr, "Item Tracker", SohRowsRegistrar);
        SohGui::RegisterSohPageRows("Randomizer", "", SohRowsRegistrar);
        SohGui::RegisterSohPageRows("Randomizer", "Item Tracker", nullptr);
        gSohRowsRan = 0;
        gSohRowsColumn = -1;
        {
            ComboSectionMenuProbe ext;
            AddSohTrackerPages(ext);
            auto& extPages = ext.Entries().at("Randomizer").sidebars;
            const std::vector<std::string> itemNow = RowNames(extPages.at("Item Tracker"));
            COMBO_CHECK(gSohRowsRan == 1, "the contributed SoH-page registrar ran %d times, expected once",
                        gSohRowsRan);
            COMBO_CHECK(gSohRowsSection == "Randomizer" && gSohRowsSidebar == "Item Tracker" &&
                            gSohRowsColumn == (int)SECTION_COLUMN_1,
                        "the contributed registrar was handed %s / %s column %d, expected Randomizer / Item Tracker "
                        "and the pinned first column",
                        gSohRowsSection.c_str(), gSohRowsSidebar.c_str(), gSohRowsColumn);
            COMBO_CHECK(!itemNow.empty() && itemNow.back() == kSohRowsRowName,
                        "the contributed row is not the last row of Randomizer / Item Tracker: [%s]",
                        Joined(itemNow).c_str());
            COMBO_CHECK(FindRow(extPages.at("Check Tracker"), kSohRowsRowName) == nullptr,
                        "a row contributed to the Item Tracker page landed on the Check Tracker page");
        }
        COMBO_CHECK(SohGui::UnregisterSohPageRows(SohRowsRegistrar) == 1,
                    "UnregisterSohPageRows did not remove exactly the one valid contribution, so an invalid one was "
                    "accepted or the valid one was lost");
        COMBO_CHECK(SohGui::UnregisterSohPageRows(SohRowsRegistrar) == 0,
                    "UnregisterSohPageRows removed a contribution twice");
        printf("[TEST] leg 3: SoH's Toggle Check Tracker and Toggle Item Tracker open the unified windows by name and "
               "key, the Check Tracker page carries #1131's two rows and the Cross-Game Spoiler after SoH's own, the "
               "Item Tracker page nothing yet, and a contribution reaches only its own page\n");
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
        // Contributed pages come in registration order, so a later contributor
        // cannot reorder the earlier ones out from under a player's persisted
        // selection; a page hosted under Randomizer is not created here at all.
        // The Combo header ships no page of its own since #1132 retired Windows.
        auto& order = extProbe.Entries().at("Combo").sidebarOrder;
        COMBO_CHECK(order.size() == baselineComboPages + 1,
                    "the Combo section has %zu sidebars in order, expected the %zu Combo-hosted pages already "
                    "registered plus the one this leg added",
                    order.size(), baselineComboPages);
        if (!order.empty()) {
            COMBO_CHECK(order.back() == kExtPageName, "sidebar order ends with '%s'; a contributed page comes last",
                        order.back().c_str());
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
    printf("[TEST] leg 5: a contributed page is created after the earlier ones with a pinned column, replaces on "
           "re-registration, is refused when invalid, is drawn only under the header it names, and unregisters "
           "cleanly\n");

    // ---- Leg 6: a renamed or moved page carries a persisted selection -------
    // Menu::DrawElement reads gSettings.Menu.ComboSidebarSection as a display
    // name and falls back to the first page when no sidebar has it, so without
    // the carry a config saved on an old name would silently land elsewhere.
    // The key is restored to whatever it held, so AllTests order cannot leak a
    // selection into a later row.
    {
        const char* kSidebarKey = "gSettings.Menu.ComboSidebarSection";
        const std::string saved = CVarGetString(kSidebarKey, "");
        struct Carry {
            const char* before;
            const char* after;
        };
        const Carry kCarries[] = {
            { "MM Enhancements", "Majora's Mask" }, // the rename of 2026-09-27
            { "Majora's Mask", "Majora's Mask" },
            { "Save Files", "Save Files" }, // a page never renamed is left alone
            // Combo > Tracker Settings keeps the Item Tracker overlay's rows until
            // lane IT-3 (#1141) retires it, so its selection stays put.
            { "Tracker Settings", "Tracker Settings" },
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
        printf("[TEST] leg 6: a selection saved on \"MM Enhancements\" is carried to \"Majora's Mask\"; every other "
               "Combo page's value is left alone\n");
    }
    // The pages that left the Combo header, carried ACROSS headers: the three
    // #1101 moved to the Randomizer header, and Windows (#1132, which shipped as
    // "Cross-Game Windows" until 2026-09-27), whose trackers open from SoH's
    // Randomizer > Check Tracker now. A config that had Combo open on one of them
    // opens on that page under Randomizer (Menu::DrawElement reads
    // gSettings.Menu.ActiveHeader, then that header's sidebar key); one whose
    // open header was another leaves the Randomizer selection alone. Either way
    // no Combo selection is left naming a page Combo no longer has. All three
    // keys are restored afterwards.
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
            { "Combo", "Windows", "Randomizer", "Check Tracker" },
            { "Combo", "Cross-Game Windows", "Randomizer", "Check Tracker" },
            // Another header open: the Randomizer selection is the player's own.
            { "Settings", "MM Randomizer", "Settings", "Shuffles" },
            { "Settings", "Windows", "Settings", "Shuffles" },
        };
        // Where a carry may land: a page AddComboRandomizerPages registers, or
        // one of SoH's own tracker pages (AddMenuRandomizer registers them).
        std::vector<std::string> randoPages = SohGui::ComboRandomizerPageNames();
        randoPages.push_back("Check Tracker");
        randoPages.push_back("Item Tracker");
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
                        "the carried Randomizer selection \"%s\" is not a Randomizer page", rando.c_str());
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
        printf("[TEST] leg 6: a config open on Combo > MM Randomizer, MM Tricks, Cross-Game Rules or Windows opens on "
               "the moved page under Randomizer, and no Combo selection is left stranded\n");
    }

    // ---- Leg 7: Combo > Tracker Settings writes the Item Tracker overlay's keys
    // ADR 0004's 2026-09-30 amendment put the unified trackers' settings on a
    // Combo page; since #1132 only the Item Tracker overlay's are left there, a
    // transitional state until lane IT-3 (#1141). Every gCombo.Tracker.Items.*
    // key in the manifest must have exactly one row here, with the overlay's own
    // default and range, or the console is a player's only writer for it.
    if (!combo.sidebars.contains(kTrackerSettingsPage)) {
        printf("[TEST] FAIL(7): the Combo section has no \"%s\" page, so the Item Tracker overlay's settings "
               "(gCombo.Tracker.Items.*) have no menu writer\n",
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

        // The Check Tracker's rows left with #1132 (ruling D1): SoH's own Check
        // Tracker page hosts its settings and #1131's two rows (leg 3), so this
        // page neither embeds SoH's Check Tracker Settings nor writes a
        // gCombo.Tracker.Checks.* key. It keeps only the Item Tracker overlay's
        // rows until lane IT-3 (#1141) moves them onto SoH's Item Tracker page.
        COMBO_CHECK(FindRow(page, "Popout Check Tracker Settings") == nullptr &&
                        FindRow(page, "Check Tracker Settings") == nullptr,
                    "Combo / %s still hosts the Check Tracker Settings block; it lives on SoH's Randomizer > Check "
                    "Tracker page since #1132",
                    kTrackerSettingsPage);

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

        // No other writer: every other interactive row writes one of the keys above.
        for (auto& column : page.columnWidgets) {
            for (WidgetInfo& row : column) {
                if (row.type == WIDGET_SEPARATOR_TEXT || row.type == WIDGET_TEXT || &row == resetPos) {
                    continue;
                }
                COMBO_CHECK(row.cVar != nullptr && RSBS::ComboKeyHasPrefix(row.cVar, kItemTrackerKeyPrefix),
                            "row '%s' on Combo / %s writes '%s', which is not one of the Item Tracker overlay's keys",
                            row.name.c_str(), kTrackerSettingsPage, row.cVar != nullptr ? row.cVar : "(null)");
            }
        }
        printf("[TEST] leg 7: Combo / %s writes each of the nine %s* keys once with the overlay's defaults and "
               "ranges, hides the floating-only rows for a windowed overlay, themes its colour picker, and holds "
               "nothing of the Check Tracker's (#1132)\n",
               kTrackerSettingsPage, kItemTrackerKeyPrefix);
    }

    // ---- Leg 8: SoH's two tracker windows stay registered, never drawn -------
    // Ruling D1 replaces them; it must not unregister them, because their
    // InitElement registers the hooks and save sections the unified panes read
    // (randomizer_check_tracker.cpp CheckTrackerWindow::InitElement,
    // randomizer_item_tracker.cpp ItemTrackerWindow::InitElement).
    // SetupGuiElements constructs both through RetiredInSingleExe<> (the
    // OoTWindowsGate row's source scan); this leg holds what that does.
    {
        const GameId prevGame = Context_GetCurrentGame();
        Context_SetCurrentGame(GAME_OOT); // OoTActiveGated<> passes everything through under OoT

        // a. The wrapper, on a spy: Init still reaches InitElement; Draw and the
        //    menu's embed path (DrawElement) never reach the window, even shown.
        auto spy = std::make_shared<OoTActiveGated<RetiredInSingleExe<RetireSpyWindow>>>("", "Retire Spy");
        spy->Init();
        spy->Show();
        spy->Draw();
        spy->DrawElement();
        printf("[TEST] leg 8: a retired spy saw %d init(s), %d draw(s), %d embed draw(s)\n", spy->inits, spy->draws,
               spy->elements);
        COMBO_CHECK(spy->inits == 1, "Init() did not reach the retired window's InitElement, so its hooks would not "
                                     "be registered");
        COMBO_CHECK(spy->draws == 0 && spy->elements == 0,
                    "a retired window was drawn (%d Draw, %d DrawElement): SoH's own tracker would open beside the "
                    "unified one",
                    spy->draws, spy->elements);

        // b. The two real windows, constructed as SetupGuiElements constructs
        //    them, on a bare Gui beside the unified ones. Their SoH keys are on,
        //    so a window that latched them would be shown.
        IntKeyGuard checkKey("gOpenWindows.CheckTracker");
        IntKeyGuard itemKey("gOpenWindows.ItemTracker");
        CVarSetInteger("gOpenWindows.CheckTracker", 1);
        CVarSetInteger("gOpenWindows.ItemTracker", 1);
        if (GameInteractor::Instance == nullptr) {
            GameInteractor::Instance = new GameInteractor(); // kept alive, like every other *::Instance singleton
        }
        // Every hook from here on is dropped at the end, the fresh SaveManager's
        // and the windows', so no later row dispatches into a tracker or a save
        // path with no play state.
        HookWatch<GameInteractor::OnLoadGame> loadGame;
        HookWatch<GameInteractor::OnExitGame> exitGame;
        HookWatch<GameInteractor::OnItemReceive> itemReceive;
        HookWatch<GameInteractor::OnTransitionEnd> transitionEnd;
        HookWatch<GameInteractor::OnShopSlotChange> shopSlot;
        HookWatch<GameInteractor::OnSceneFlagSet> sceneFlag;
        HookWatch<GameInteractor::OnFlagSet> flagSet;
        HookWatch<GameInteractor::OnDialogMessage> dialog;
        HookWatch<GameInteractor::OnGameFrameUpdate> frame;
        HookWatch<GameInteractor::OnDeleteFile> deleteFile;
        HookWatch<GameInteractor::OnFileChooseMain> fileChoose;
        HookWatch<GameInteractor::OnLoadFile> loadFile;
        HookWatch<GameInteractor::OnSaveFile> saveFile;
        // A fresh SaveManager takes the windows' save sections (leaked on
        // purpose, as oot_file_select_refusal_test.cpp's is).
        SaveManager* const saveManagerBefore = SaveManager::Instance;
        SaveManager::Instance = new SaveManager();
        for (HookWatchBase* w :
             std::initializer_list<HookWatchBase*>{ &loadGame, &exitGame, &itemReceive, &transitionEnd, &shopSlot,
                                                    &sceneFlag, &flagSet, &dialog, &frame }) {
            w->Mark();
        }

        auto gui = std::make_shared<Ship::Gui>();
        std::shared_ptr<Ship::GuiWindow> check =
            std::make_shared<OoTActiveGated<RetiredInSingleExe<CheckTracker::CheckTrackerWindow>>>(
                "", kSohRetiredCheckTrackerWindowName, ImVec2(400, 540));
        std::shared_ptr<Ship::GuiWindow> item = std::make_shared<OoTActiveGated<RetiredInSingleExe<ItemTrackerWindow>>>(
            "", kSohRetiredItemTrackerWindowName, ImVec2(350, 600));
        gui->AddGuiWindow(check); // AddGuiWindow runs Init(), as SetupGuiElements' does
        gui->AddGuiWindow(item);
        ComboGui::RegisterComboTrackerWindow(gui);
        ComboGui::RegisterComboItemTrackerWindow(gui);

        printf("[TEST] leg 8: the retired windows registered OnLoadGame %zu, OnExitGame %zu, OnItemReceive %zu, "
               "OnTransitionEnd %zu, OnShopSlotChange %zu, OnSceneFlagSet %zu, OnFlagSet %zu, OnDialogMessage %zu, "
               "OnGameFrameUpdate %zu\n",
               loadGame.Added(), exitGame.Added(), itemReceive.Added(), transitionEnd.Added(), shopSlot.Added(),
               sceneFlag.Added(), flagSet.Added(), dialog.Added(), frame.Added());
        COMBO_CHECK(loadGame.Added() == 1 && exitGame.Added() == 1 && itemReceive.Added() == 1 &&
                        transitionEnd.Added() == 1 && shopSlot.Added() == 1 && sceneFlag.Added() == 1 &&
                        flagSet.Added() == 1 && dialog.Added() == 1,
                    "the retired Check Tracker did not register each of its eight hooks once: shop SEEN/IDENTIFIED, "
                    "collected and dialogue identification would stop reaching the unified pane");
        COMBO_CHECK(frame.Added() == 1, "the retired Item Tracker did not register its frame hook once");

        COMBO_CHECK(!check->IsVisible() && !item->IsVisible(),
                    "a retired SoH tracker window is visible because its SoH key is on: it latched the key that "
                    "opens the unified window");
        COMBO_CHECK(gui->GetGuiWindow(kSohRetiredCheckTrackerWindowName) == check &&
                        gui->GetGuiWindow(kSohRetiredItemTrackerWindowName) == item,
                    "a retired SoH tracker window is not registered under its retired name");
        // What SoH's toggle rows find by name (leg 3 holds the names).
        COMBO_CHECK(std::dynamic_pointer_cast<ComboGui::ComboTrackerWindow>(gui->GetGuiWindow("Check Tracker")) !=
                        nullptr,
                    "the window named \"Check Tracker\" is not the unified Check Tracker, so SoH's Toggle Check "
                    "Tracker opens SoH's own");
        COMBO_CHECK(std::dynamic_pointer_cast<ComboGui::ComboItemTrackerWindow>(gui->GetGuiWindow("Item Tracker")) !=
                        nullptr,
                    "the window named \"Item Tracker\" is not the unified Item Tracker, so SoH's Toggle Item Tracker "
                    "opens SoH's own");
        // Shown anyway, neither draws (nor touches ImGui, which this process has
        // no frame for). Only once they are retired: SoH's own Draw would.
        if (std::string(kSohRetiredCheckTrackerWindowName) != "Check Tracker") {
            for (const auto& retired : { check, item }) {
                retired->Show();
                retired->Draw();
                retired->DrawElement();
                retired->Hide();
            }
        }

        for (HookWatchBase* w : std::initializer_list<HookWatchBase*>{
                 &loadGame, &exitGame, &itemReceive, &transitionEnd, &shopSlot, &sceneFlag, &flagSet, &dialog, &frame,
                 &deleteFile, &fileChoose, &loadFile, &saveFile }) {
            w->Drop();
        }
        SaveManager::Instance = saveManagerBefore;
        Context_SetCurrentGame(prevGame);
        printf("[TEST] leg 8: SoH's Check Tracker and Item Tracker stay registered under their retired names with "
               "their hooks, never latch their keys and never draw; \"Check Tracker\" and \"Item Tracker\" are the "
               "unified windows\n");
    }

    if (gFailures == 0) {
        printf("[TEST] menu-combo-section: PASS\n");
    } else {
        printf("[TEST] menu-combo-section: %d failure(s)\n", gFailures);
    }
    return gFailures;
}

#endif // RSBS_SINGLE_EXECUTABLE
