/**
 * @file soh_menu_combo_section_test.cpp
 * @brief ROM-free, display-free lock for the pages this project adds to the one
 *        live menu: the tier-4 **Combo** header #497 step 6 built and #1133
 *        retired (operator rulings D4 and D5, 2026-10-22), the hosts its pages
 *        moved to, the persisted-selection carry, and the extension points.
 *
 * CTest row MenuComboSection in CMake/SingleExecutable.cmake, dispatch
 * "menu-combo-section" in src/common/test_runner.cpp.
 *
 * WHAT IT IS FOR. Registration is container work, so every way a page can be
 * wrong is silent — nothing here is a compile error and nothing is a crash:
 *
 *  1. THE COMBO HEADER IS STILL THERE (#1133). Rulings D4 and D5 retire it:
 *     its Majora's Mask rows are hosted on the SoH pages that hold their twins
 *     (MenuMmEnhancementRows locks where), MM Mods is a Settings page beside
 *     SoH's, renamed OoT Mods (the operator's ruling of 2026-10-27), and Save
 *     Files a page of Dev Tools (leg 9 here). Leg 1 builds the
 *     ROM-free headers and AddMenuCombo, which now only carries a persisted
 *     selection, and requires no "Combo" entry anywhere.
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
 *  6. THE EXTENSION POINT DOES NOT WORK, or works only by accident. Leg 5
 *     drives `RegisterComboSectionPage` end to end under the three hosts SoH's
 *     headers give it (Randomizer, #1101; Dev Tools, #1133; Settings, the
 *     ruling of 2026-10-27): the page appears
 *     AFTER the ones already there, its registrar is handed the header's name
 *     and a PINNED `path.column`, its `columnCount` is honoured, a duplicate
 *     name replaces rather than duplicates, an invalid registration is refused,
 *     `UnregisterComboSectionPage` puts the registry back, and AddMenuCombo
 *     creates no page at all. Its counts are BASELINE-RELATIVE: production pages
 *     register from file-scope initializers before any test runs.
 *
 *  7. A RETIRED HEADER STRANDS THE SELECTION, or crashes the menu. Menu's
 *     DrawElement reads gSettings.Menu.ActiveHeader and calls
 *     menuEntries.at(headerIndex), and its only catch is bad_variant_access, so
 *     a config left on "Combo" would be an uncaught std::out_of_range. Leg 6
 *     drives AddMenuCombo, the carry's call site, over every page the header
 *     ever had, by every name it had: each reopens on its new host (Randomizer
 *     > Cross-Game, MM Randomizer, MM Tricks, Check Tracker, Item Tracker;
 *     Enhancements > Quality of Life, the page that got the Majora's Mask page's
 *     first rows; Settings > MM Mods; Dev Tools > Save Files), a page that each
 *     host's ROM-free builder really registers; an empty or unknown selection
 *     CLEARS the header key (clear-never-store) so the menu opens on its default;
 *     the Combo sidebar key is cleared in every case; and a config whose open
 *     header was another keeps every selection but loses the stale Combo key.
 *     SoH's Settings > Mod Menu is OoT Mods since 2026-10-27, and selection
 *     persists by display name, so a saved Mod Menu reopens on OoT Mods under
 *     any open header, writing nothing else. Every probe puts back each key it
 *     set.
 *
 *  8. THE ITEM TRACKER OVERLAY'S SETTINGS HAVE NO MENU WRITER, OR TWO. Operator
 *     ruling IT-D6 (#1141): one set of settings, SoH's. SoH's own Item Tracker
 *     Settings window writes the gTrackers.ItemTracker.* keys the overlay reads,
 *     and leg 7 requires SoH's Randomizer > Item Tracker page to carry, after
 *     SoH's rows, exactly one row for every gCombo.Tracker.Items.* key in the
 *     manifest (the overlay's Opacity and sections, MM's eight group keys), with
 *     the overlay's own widget, default and range, under SoH's separator or its
 *     own, Move to Default Position only while the overlay floats, no row writing
 *     a SoH key or a retired twin, and Combo > Tracker Settings gone.
 *
 *  9. SoH's OWN TRACKER WINDOWS ARE UNREGISTERED, OR DRAWN (#1132). Their
 *     InitElement registers the hooks and save sections the unified panes read,
 *     so they must stay on the Gui, never drawn and never opened by SoH's key.
 *     Leg 8 holds RetiredInSingleExe<> on a spy, then the two real windows on a
 *     bare Gui beside the unified ones: every hook registered once, no key
 *     latched, and "Check Tracker" / "Item Tracker" naming the unified windows.
 *
 * 10. A FORMER COMBO PAGE HAS NO HOST, or the wrong one (#1133, rulings D4 and
 *     D5, and the operator's ruling of 2026-10-27: "Have two rows, OoT Mods and
 *     MM Mods instead of just Mods"). Leg 9 builds SoH's own Settings and Dev
 *     Tools headers: Settings > OoT Mods reads SoH's Popout Mod Menu Window
 *     alone, and Settings > MM Mods, directly after it, the MM Mods page's rows
 *     (its state note, Rescan Mods Folder, MM Mod List) in their order with no
 *     heading, Rescan Mods Folder recorded by the hosted-row seam; Dev Tools
 *     ends with Save Files, after SoH's last page, its note, section and table
 *     in their order.
 *
 * WHAT IS NOT COVERED. The six tier-4 rules' own behaviour — the staging buffers,
 * the freeze gate, the values-from-the-save rule — is `ComboSettingsRows`, which
 * followed the rows here. This row owns the section's SHAPE. Appearance is
 * operator verification on the nightly.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "soh/SohGui/SohMenu.h"
// Leg 9: the hosted-row seam's registry and block heading (#1233).
#include "soh/SohGui/HostedMm/SohMenuHostedMm.h"
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

// Leg 8c: the Check Tracker's two settings statics IsVisibleInCheckTracker reads
// (randomizer_check_tracker.cpp test bridges).
extern "C" void RandoTest_CheckTrackerVisibilityStatics(int* alwaysShowGsOut, int* hideUnshuffledShopsOut);
extern "C" void RandoTest_CheckTrackerSetVisibilityStatics(int alwaysShowGs, int hideUnshuffledShops);

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
// A registrar records what the host's builder handed it, so the contract a
// contributor relies on is OBSERVED rather than read off the source.
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
// Randomizer header by AddComboRandomizerPages, never under Dev Tools.
const char* kExtRandoPageName = "Contributed Randomizer Test Page";
// The same seam with ComboPageHost::Settings (the operator's ruling of
// 2026-10-27, MM Mods): drawn under SoH's Settings header by
// AddComboSettingsPages, after SoH's own pages.
const char* kExtSettingsPageName = "Contributed Settings Test Page";

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

/** A string CVar's state (absent, or its value), put back when the guard leaves scope. */
struct StringKeyGuard {
    const char* key;
    bool existed;
    std::string value;
    explicit StringKeyGuard(const char* k) : key(k), existed(CVarExists(k)), value(CVarGetString(k, "")) {
    }
    ~StringKeyGuard() {
        if (existed) {
            CVarSetString(key, value.c_str());
        } else {
            CVarClear(key);
        }
    }
};

/** A string CVar's value, or "(absent)" when the key does not exist (a carry
 *  must CLEAR, never store an empty or placeholder value). */
std::string KeyState(const char* key) {
    return CVarExists(key) ? std::string(CVarGetString(key, "")) : std::string("(absent)");
}

} // namespace

extern "C" int OoT_MenuComboSection_RunHeadless(void) {
    printf("[TEST] menu-combo-section: no Combo header, the hosts its pages moved to, the persisted-selection carry, "
           "SoH's tracker pages and the extension points (#497 step 6, #1101, #1132, #1133)\n");

    gFailures = 0;

    // The registry is process-global, so start from a known state rather than
    // from whatever ran before in AllTests.
    while (SohGui::UnregisterComboSectionPage(kExtPageName)) {}
    while (SohGui::UnregisterComboSectionPage(kExtRandoPageName)) {}
    while (SohGui::UnregisterComboSectionPage(kExtSettingsPageName)) {}

    // ---- Leg 1: there is no Combo header (#1133) ------------------------------
    // Rulings D4 and D5 retire it. AddMenuCombo is still called from
    // AddMenuElements as the carry's call site (leg 6) and registers nothing.
    // Built here among SoH's ROM-free headers in AddMenuElements' order, no
    // header named "Combo" exists, none of its former pages is a page anywhere
    // (Save Files is Dev Tools' own, leg 9), and the page registry holds no
    // Majora's Mask or MM Mods page.
    {
        ComboSectionMenuProbe probe;
        probe.AddMenuSettings();
        probe.AddMenuEnhancements();
        probe.AddMenuCombo();
        probe.AddMenuDevTools();
        auto& entries = probe.Entries();
        COMBO_CHECK(!entries.contains("Combo"),
                    "a \"Combo\" header is still registered; rulings D4 and D5 (#1133) fold its pages into SoH's "
                    "headers, and a config left on it would select a header the menu no longer needs");
        COMBO_CHECK(std::find(probe.Order().begin(), probe.Order().end(), std::string("Combo")) == probe.Order().end(),
                    "\"Combo\" is still in menuOrder, so a header would be drawn for it");
        for (const auto& [header, entry] : entries) {
            for (const char* gone :
                 { "Majora's Mask", "MM Enhancements", "Windows", "Cross-Game Windows", "Tracker Settings" }) {
                COMBO_CHECK(!entry.sidebars.contains(gone),
                            "%s still holds a \"%s\" page; the former Combo pages' rows live on SoH's pages now",
                            header.c_str(), gone);
            }
            // The operator's ruling of 2026-10-27: MM Mods is a Settings page (leg 9), and only there.
            COMBO_CHECK(header == "Settings" || !entry.sidebars.contains("MM Mods"),
                        "%s holds an \"MM Mods\" page; it is a Settings page, beside OoT Mods", header.c_str());
        }
        COMBO_CHECK(entries.contains("Settings") && entries.at("Settings").sidebars.contains("MM Mods"),
                    "Settings holds no \"MM Mods\" page (the operator's ruling of 2026-10-27: \"Have two rows, OoT "
                    "Mods and MM Mods instead of just Mods\")");
        bool mmModsRegistered = false;
        for (const SohGui::ComboSectionPage& page : SohGui::GetComboSectionPages()) {
            COMBO_CHECK(page.sidebarName != "Majora's Mask",
                        "the page registry still holds \"%s\"; its rows are hosted on SoH's own pages (#1133)",
                        page.sidebarName.c_str());
            if (page.sidebarName == "MM Mods") {
                mmModsRegistered = true;
                COMBO_CHECK(page.host == SohGui::ComboPageHost::Settings,
                            "MM Mods is registered for another header than Settings (the ruling of 2026-10-27)");
            }
            if (page.sidebarName == "Save Files") {
                COMBO_CHECK(page.host == SohGui::ComboPageHost::DevTools,
                            "Save Files is registered for another header than Dev Tools (ruling D4)");
            }
        }
        COMBO_CHECK(mmModsRegistered, "the page registry holds no \"MM Mods\" page for the Settings header");
        printf("[TEST] leg 1: no Combo header, no former Combo page anywhere but MM Mods (Settings) and Save Files "
               "(Dev Tools), and AddMenuCombo registers nothing\n");
    }

    // ---- Leg 3: SoH's tracker pages are the unified windows' pages (#1132) ---
    // Ruling D1: SoH's Randomizer > Check Tracker and Item Tracker pages open the
    // unified windows, because those windows ARE "Check Tracker" and "Item
    // Tracker" over SoH's own window keys (ruling D7): WINDOW_BUTTON calls the
    // window it finds BY NAME, and reads .CVar for its label. The Check Tracker
    // rows that lived on Combo > Tracker Settings (#1131's two navigation keys)
    // and the Cross-Game Spoiler's toggle that lived on Combo > Windows follow
    // SoH's own rows on its Check Tracker page, through the contributed-rows
    // registry; the Item Tracker page takes the overlay's own rows after
    // SoH's the same way (lane IT-3, #1141).
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
        // The Item Tracker page (#1141, ruling IT-D6): SoH's four rows, then the
        // overlay's keys SoH's Item Tracker Settings window has no twin for.
        const std::vector<std::string> kItemPage = {
            "Item Tracker",
            "Toggle Item Tracker",
            "Item Tracker Settings",
            "Popout Item Tracker Settings",
            "Move to Default Position##ItemTracker",
            "Tracker Opacity",
            "Item Tracker Sections",
            "Ocarina of Time##ItemTracker",
            "Majora's Mask##ItemTracker",
            "Shared##ItemTracker",
            "Majora's Mask Groups",
            "Tokens##ItemTracker",
            "Stray Fairies##ItemTracker",
            "Dungeon##ItemTracker",
            "Boss Souls##ItemTracker",
            "Enemy Souls##ItemTracker",
            "Owl Statues##ItemTracker",
            "Time##ItemTracker",
            "Misc##ItemTracker",
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
               "Item Tracker page the overlay's own block, and a contribution reaches only its own page\n");
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

    // ---- Leg 5: the extension point, under both of SoH's headers it serves --
    // BASELINE-RELATIVE: production pages (MM Randomizer, MM Locations, MM
    // Tricks; Save Files) register from file-scope initializers before any test
    // runs, so this leg measures the delta its own registration makes and
    // asserts the return to the baseline it started from.
    const std::size_t baselinePages = SohGui::GetComboSectionPages().size();
    const std::size_t baselineDevToolsPages = SohGui::ComboDevToolsPageNames().size();

    gExtRan = 0;
    gExtColumn = -1;
    gExtSectionName.clear();
    gExtSidebarName.clear();
    SohGui::RegisterComboSectionPage(kExtPageName, 2, ExtRegistrar, SohGui::ComboPageHost::DevTools);
    COMBO_CHECK(SohGui::GetComboSectionPages().size() == baselinePages + 1,
                "registering one page left %zu in the registry, expected the baseline %zu plus one",
                SohGui::GetComboSectionPages().size(), baselinePages);

    // Refusals: a page with no name, no registrar or no column would either
    // never draw or be #640 by construction.
    SohGui::RegisterComboSectionPage(nullptr, 1, ExtRegistrar, SohGui::ComboPageHost::DevTools);
    SohGui::RegisterComboSectionPage("", 1, ExtRegistrar, SohGui::ComboPageHost::DevTools);
    SohGui::RegisterComboSectionPage("No Registrar", 1, nullptr, SohGui::ComboPageHost::DevTools);
    SohGui::RegisterComboSectionPage("No Columns", 0, ExtRegistrar, SohGui::ComboPageHost::DevTools);
    COMBO_CHECK(SohGui::GetComboSectionPages().size() == baselinePages + 1,
                "RegisterComboSectionPage accepted an invalid page: the registry holds %zu, expected the baseline %zu "
                "plus one",
                SohGui::GetComboSectionPages().size(), baselinePages);

    {
        // SoH's own Dev Tools builder draws the DevTools-hosted pages after its
        // last page (AddMenuDevTools calls AddComboDevToolsPages).
        ComboSectionMenuProbe extProbe;
        extProbe.AddMenuDevTools();
        COMBO_CHECK(gExtRan == 1,
                    "the DevTools-hosted registrar ran %d times under AddMenuDevTools, expected once: SoH's Dev Tools "
                    "builder does not draw the pages registered for it",
                    gExtRan);
        COMBO_CHECK(gExtSectionName == "Dev Tools",
                    "the DevTools-hosted registrar was handed section '%s', expected 'Dev Tools'",
                    gExtSectionName.c_str());
        COMBO_CHECK(gExtSidebarName == kExtPageName, "the contributed registrar was handed sidebar '%s', expected '%s'",
                    gExtSidebarName.c_str(), kExtPageName);
        COMBO_CHECK(gExtColumn == (int)SECTION_COLUMN_1,
                    "the contributed registrar was handed column %d, not the pinned first one. A contributor that "
                    "inherited a stale column would register rows that are never drawn, and no compile catches it",
                    gExtColumn);
        auto& devTools = extProbe.Entries().at("Dev Tools");
        if (!devTools.sidebars.contains(kExtPageName)) {
            printf("[TEST] FAIL(5): AddMenuDevTools created no sidebar for the contributed page '%s'\n", kExtPageName);
            gFailures++;
        } else {
            SidebarEntry& extPage = devTools.sidebars.at(kExtPageName);
            COMBO_CHECK(extPage.columnCount == 2, "the contributed page declares %u columns, expected the registered 2",
                        extPage.columnCount);
            COMBO_CHECK(FindRow(extPage, kExtRowName) != nullptr, "the contributed page holds no row");
        }
        // Contributed pages come after SoH's own and in registration order, so
        // a later contributor cannot reorder the earlier ones out from under a
        // player's persisted selection.
        const auto& order = devTools.sidebarOrder;
        COMBO_CHECK(!order.empty() && order.back() == kExtPageName,
                    "the Dev Tools sidebar ends with '%s'; a contributed page comes last",
                    order.empty() ? "(nothing)" : order.back().c_str());
        const std::vector<std::string> devToolsNames = SohGui::ComboDevToolsPageNames();
        COMBO_CHECK(devToolsNames.size() == baselineDevToolsPages + 1 && devToolsNames.back() == kExtPageName,
                    "ComboDevToolsPageNames() does not list the contributed page last");
    }

    // A second registration under the same name REPLACES rather than duplicating:
    // two registrars claiming one page is an ambiguity in which the loser simply
    // never draws. Driven through AddComboDevToolsPages directly, the slice
    // AddMenuDevTools calls, with a deliberately wrong incoming path.
    gExtReplacementRan = 0;
    SohGui::RegisterComboSectionPage(kExtPageName, 1, ExtReplacementRegistrar, SohGui::ComboPageHost::DevTools);
    COMBO_CHECK(SohGui::GetComboSectionPages().size() == baselinePages + 1,
                "a same-name re-registration duplicated the page (%zu entries, baseline %zu)",
                SohGui::GetComboSectionPages().size(), baselinePages);
    {
        ComboSectionMenuProbe replaceProbe;
        replaceProbe.AddMenuEntry("Dev Tools", "gSettings.Menu.DevToolsSidebarSection");
        WidgetPath replacePath = { "Ignored", "Ignored", SECTION_COLUMN_3 };
        gExtRan = 0;
        SohGui::AddComboDevToolsPages(replaceProbe, replacePath);
        COMBO_CHECK(gExtReplacementRan == 1, "the replacement registrar ran %d times, expected once",
                    gExtReplacementRan);
        COMBO_CHECK(gExtRan == 0, "the replaced registrar ran anyway");
        COMBO_CHECK(replaceProbe.Entries().at("Dev Tools").sidebars.contains(kExtPageName),
                    "AddComboDevToolsPages registered the page under the stale incoming header");
    }

    // AddMenuCombo is the carry's call site only: it creates no header and runs
    // no page registrar, whatever is registered.
    {
        ComboSectionMenuProbe comboProbe;
        gExtRan = 0;
        gExtReplacementRan = 0;
        comboProbe.AddMenuCombo();
        COMBO_CHECK(comboProbe.Entries().empty() && comboProbe.Order().empty(),
                    "AddMenuCombo registered %zu header(s); since #1133 it only carries a persisted selection",
                    comboProbe.Entries().size());
        COMBO_CHECK(gExtRan == 0 && gExtReplacementRan == 0, "AddMenuCombo ran a contributed page's registrar");
    }

    // The Randomizer host (#1101): the same registry, drawn by
    // AddComboRandomizerPages after the pages already registered there and
    // before Cross-Game, and never by AddMenuDevTools.
    SohGui::RegisterComboSectionPage(kExtRandoPageName, 1, ExtRegistrar, SohGui::ComboPageHost::Randomizer);
    {
        ComboSectionMenuProbe hostProbe;
        gExtRan = 0;
        hostProbe.AddMenuDevTools();
        COMBO_CHECK(!hostProbe.Entries().at("Dev Tools").sidebars.contains(kExtRandoPageName),
                    "a page registered for the Randomizer header was created under Dev Tools");
        COMBO_CHECK(gExtRan == 0, "AddMenuDevTools ran a Randomizer-hosted registrar");

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

    // The Settings host (the operator's ruling of 2026-10-27, MM Mods): the same
    // registry, drawn by SoH's own AddMenuSettings after its last page, and never
    // by AddMenuDevTools.
    const std::size_t baselineSettingsPages = SohGui::ComboSettingsPageNames().size();
    SohGui::RegisterComboSectionPage(kExtSettingsPageName, 1, ExtRegistrar, SohGui::ComboPageHost::Settings);
    {
        ComboSectionMenuProbe settingsProbe;
        gExtRan = 0;
        settingsProbe.AddMenuDevTools();
        COMBO_CHECK(!settingsProbe.Entries().at("Dev Tools").sidebars.contains(kExtSettingsPageName),
                    "a page registered for the Settings header was created under Dev Tools");
        COMBO_CHECK(gExtRan == 0, "AddMenuDevTools ran a Settings-hosted registrar");

        gExtSectionName.clear();
        gExtSidebarName.clear();
        gExtColumn = -1;
        settingsProbe.AddMenuSettings();
        COMBO_CHECK(gExtRan == 1,
                    "the Settings-hosted registrar ran %d times under AddMenuSettings, expected once: SoH's Settings "
                    "builder does not draw the pages registered for it",
                    gExtRan);
        COMBO_CHECK(gExtSectionName == "Settings" && gExtSidebarName == kExtSettingsPageName &&
                        gExtColumn == (int)SECTION_COLUMN_1,
                    "the Settings-hosted registrar was handed section '%s' sidebar '%s' column %d, expected "
                    "'Settings', '%s' and the pinned first column",
                    gExtSectionName.c_str(), gExtSidebarName.c_str(), gExtColumn, kExtSettingsPageName);
        const auto& order = settingsProbe.Entries().at("Settings").sidebarOrder;
        COMBO_CHECK(!order.empty() && order.back() == kExtSettingsPageName,
                    "the Settings sidebar ends with '%s'; a contributed page comes after SoH's own pages",
                    order.empty() ? "(nothing)" : order.back().c_str());
        const std::vector<std::string> settingsNames = SohGui::ComboSettingsPageNames();
        COMBO_CHECK(settingsNames.size() == baselineSettingsPages + 1 && settingsNames.back() == kExtSettingsPageName,
                    "ComboSettingsPageNames() does not list the contributed page last");
    }
    COMBO_CHECK(SohGui::UnregisterComboSectionPage(kExtSettingsPageName),
                "UnregisterComboSectionPage did not remove the Settings-hosted page");

    COMBO_CHECK(SohGui::UnregisterComboSectionPage(kExtPageName), "UnregisterComboSectionPage did not remove the page");
    COMBO_CHECK(!SohGui::UnregisterComboSectionPage(kExtPageName),
                "UnregisterComboSectionPage removed a page twice, so the registry held a duplicate");
    // Back to the baseline, not to empty: this leg must leave the registry
    // exactly as it found it.
    COMBO_CHECK(SohGui::GetComboSectionPages().size() == baselinePages,
                "the registry holds %zu contributed pages, expected the baseline %zu it started from",
                SohGui::GetComboSectionPages().size(), baselinePages);
    {
        ComboSectionMenuProbe cleanProbe;
        cleanProbe.AddMenuDevTools();
        COMBO_CHECK(!cleanProbe.Entries().at("Dev Tools").sidebars.contains(kExtPageName),
                    "an unregistered contributed page is still created");
    }
    printf("[TEST] leg 5: a contributed page is created after SoH's own pages with a pinned column under the header "
           "it names (Dev Tools, Randomizer, Settings), replaces on re-registration, is refused when invalid, is never "
           "created by AddMenuCombo, and unregisters cleanly\n");

    // ---- Leg 6: a selection on the retired header is carried, or cleared -----
    // Menu::DrawElement reads gSettings.Menu.ActiveHeader and then
    // menuEntries.at(header): with no Combo header, a config left on it is an
    // uncaught std::out_of_range (its only catch is bad_variant_access). The
    // carry runs from AddMenuCombo, before the first draw. Every page the header
    // ever had, under every name it had, reopens on its new host, a page that
    // host's own builder registers; an empty or unknown selection CLEARS the
    // header key (clear-never-store: the menu then opens on its default); the
    // Combo sidebar key is cleared in every case; no other key is written; and
    // every key this leg sets is put back.
    {
        const char* kHeaderKey = "gSettings.Menu.ActiveHeader";
        const char* kComboKey = "gSettings.Menu.ComboSidebarSection";
        const char* kRandoKey = "gSettings.Menu.RandomizerSidebarSection";
        const char* kEnhKey = "gSettings.Menu.EnhancementsSidebarSection";
        const char* kSettingsKey = "gSettings.Menu.SettingsSidebarSection";
        const char* kDevToolsKey = "gSettings.Menu.DevToolsSidebarSection";
        const char* const kKeys[] = { kHeaderKey, kComboKey, kRandoKey, kEnhKey, kSettingsKey, kDevToolsKey };
        StringKeyGuard headerGuard(kHeaderKey);
        StringKeyGuard comboGuard(kComboKey);
        StringKeyGuard randoGuard(kRandoKey);
        StringKeyGuard enhGuard(kEnhKey);
        StringKeyGuard settingsGuard(kSettingsKey);
        StringKeyGuard devToolsGuard(kDevToolsKey);
        auto clearAll = [&]() {
            for (const char* key : kKeys) {
                CVarClear(key);
            }
        };

        // Where a carry may land: the pages each host's ROM-free builder (or,
        // for Randomizer, the slices of it that run ROM-free) registers.
        ComboSectionMenuProbe hosts;
        hosts.AddMenuSettings();
        hosts.AddMenuEnhancements();
        hosts.AddMenuDevTools();
        AddSohTrackerPages(hosts);
        WidgetPath hostsRandoPath = { "Randomizer", "Tricks/Glitches", SECTION_COLUMN_1 };
        SohGui::AddComboRandomizerPages(hosts, hostsRandoPath);
        auto& hostEntries = hosts.Entries();

        struct Carry {
            const char* comboBefore;
            const char* headerAfter;
            const char* sidebarKey;
            const char* sidebarAfter;
        };
        const Carry kCarries[] = {
            // #1101: the randomizer pages.
            { "Cross-Game Rules", "Randomizer", kRandoKey, "Cross-Game" },
            { "MM Randomizer", "Randomizer", kRandoKey, "MM Randomizer" },
            { "MM Tricks", "Randomizer", kRandoKey, "MM Tricks" },
            // #1132: Windows (Cross-Game Windows until 2026-09-27).
            { "Windows", "Randomizer", kRandoKey, "Check Tracker" },
            { "Cross-Game Windows", "Randomizer", kRandoKey, "Check Tracker" },
            // #1141: Tracker Settings.
            { "Tracker Settings", "Randomizer", kRandoKey, "Item Tracker" },
            // #1133: the Majora's Mask page (MM Enhancements until 2026-09-27),
            // to the page that got its first rows; MM Mods (a Settings page of
            // its own since the operator's ruling of 2026-10-27); Save Files.
            { "Majora's Mask", "Enhancements", kEnhKey, "Quality of Life" },
            { "MM Enhancements", "Enhancements", kEnhKey, "Quality of Life" },
            { "MM Mods", "Settings", kSettingsKey, "MM Mods" },
            { "Save Files", "Dev Tools", kDevToolsKey, "Save Files" },
        };
        for (const Carry& c : kCarries) {
            clearAll();
            CVarSetString(kHeaderKey, "Combo");
            CVarSetString(kComboKey, c.comboBefore);
            ComboSectionMenuProbe carryProbe;
            carryProbe.AddMenuCombo();
            COMBO_CHECK(KeyState(kHeaderKey) == c.headerAfter && KeyState(c.sidebarKey) == c.sidebarAfter,
                        "a config on Combo > %s opens on %s > %s after AddMenuCombo, expected %s > %s", c.comboBefore,
                        KeyState(kHeaderKey).c_str(), KeyState(c.sidebarKey).c_str(), c.headerAfter, c.sidebarAfter);
            COMBO_CHECK(!CVarExists(kComboKey), "after carrying Combo > %s the Combo sidebar key still reads \"%s\"",
                        c.comboBefore, KeyState(kComboKey).c_str());
            for (const char* key : kKeys) {
                if (key != kHeaderKey && key != c.sidebarKey && key != kComboKey) {
                    COMBO_CHECK(!CVarExists(key), "carrying Combo > %s also wrote %s = \"%s\"", c.comboBefore, key,
                                KeyState(key).c_str());
                }
            }
            COMBO_CHECK(hostEntries.contains(c.headerAfter) &&
                            hostEntries.at(c.headerAfter).sidebars.contains(c.sidebarAfter),
                        "Combo > %s carries to %s > %s, which that header's builder does not register, so the menu "
                        "would fall back to the header's first page",
                        c.comboBefore, c.headerAfter, c.sidebarAfter);
        }

        // Absent, empty or unknown on the Combo header: clear, never store.
        const char* const kUnknown[] = { nullptr, "", "Pairing Status", "Not A Page" };
        for (const char* before : kUnknown) {
            clearAll();
            CVarSetString(kHeaderKey, "Combo");
            if (before != nullptr) {
                CVarSetString(kComboKey, before);
            }
            ComboSectionMenuProbe carryProbe;
            carryProbe.AddMenuCombo();
            for (const char* key : kKeys) {
                COMBO_CHECK(!CVarExists(key),
                            "a config on Combo with the selection \"%s\" leaves %s = \"%s\"; the header key must be "
                            "cleared so the menu opens on its default, and no key stored",
                            before == nullptr ? "(absent)" : before, key, KeyState(key).c_str());
            }
        }

        // Another header open: every selection stays the player's own; only the
        // stale Combo key goes. With no Combo key at all, nothing is minted.
        struct Other {
            const char* header;
            const char* comboBefore;
            const char* sidebarKey;
            const char* sidebar;
        };
        const Other kOthers[] = {
            { "Settings", "MM Mods", kSettingsKey, "General" },
            { "Enhancements", "Majora's Mask", kEnhKey, "Difficulty" },
            { "Randomizer", "Windows", kRandoKey, "Shuffles" },
            { "Dev Tools", "Save Files", kDevToolsKey, "General" },
            { "Settings", nullptr, kSettingsKey, "Audio" },
        };
        for (const Other& o : kOthers) {
            clearAll();
            CVarSetString(kHeaderKey, o.header);
            CVarSetString(o.sidebarKey, o.sidebar);
            if (o.comboBefore != nullptr) {
                CVarSetString(kComboKey, o.comboBefore);
            }
            ComboSectionMenuProbe otherProbe;
            otherProbe.AddMenuCombo();
            COMBO_CHECK(KeyState(kHeaderKey) == o.header && KeyState(o.sidebarKey) == o.sidebar,
                        "with %s > %s open and Combo's key on %s, AddMenuCombo moved the menu to %s > %s", o.header,
                        o.sidebar, o.comboBefore != nullptr ? o.comboBefore : "(absent)", KeyState(kHeaderKey).c_str(),
                        KeyState(o.sidebarKey).c_str());
            for (const char* key : kKeys) {
                if (key != kHeaderKey && key != o.sidebarKey) {
                    COMBO_CHECK(!CVarExists(key), "with %s open, AddMenuCombo left %s = \"%s\"", o.header, key,
                                KeyState(key).c_str());
                }
            }
        }
        // SoH's Settings > Mod Menu is OoT Mods since the operator's ruling of
        // 2026-10-27 ("Have two rows, OoT Mods and MM Mods instead of just
        // Mods"). Selection persists by display name, and Menu::DrawElement opens
        // a Settings selection it has no page for on the header's first page
        // (General), so a config saved on Mod Menu is renamed to OoT Mods,
        // whichever header is open and with no other key written or minted. MM
        // Mods is inserted after it, the last Settings page, so no saved Settings
        // selection moves. OoT Mods and MM Mods themselves stay as they are.
        struct Rename {
            const char* headerBefore; // nullptr: the key is absent
            const char* otherKey;     // another header's sidebar key this case sets, or nullptr
            const char* otherValue;
            const char* comboBefore; // the Combo key, or nullptr
            const char* settingsBefore;
            const char* headerAfter; // nullptr: still absent
            const char* settingsAfter;
        };
        const Rename kRenames[] = {
            { nullptr, nullptr, nullptr, nullptr, "Mod Menu", nullptr, "OoT Mods" },
            { "Settings", nullptr, nullptr, nullptr, "Mod Menu", "Settings", "OoT Mods" },
            { "Enhancements", kEnhKey, "Difficulty", nullptr, "Mod Menu", "Enhancements", "OoT Mods" },
            { "Combo", kEnhKey, nullptr, "Majora's Mask", "Mod Menu", "Enhancements", "OoT Mods" },
            { "Combo", nullptr, nullptr, "MM Mods", "Mod Menu", "Settings", "MM Mods" },
            { "Settings", nullptr, nullptr, nullptr, "OoT Mods", "Settings", "OoT Mods" },
            { "Settings", nullptr, nullptr, nullptr, "MM Mods", "Settings", "MM Mods" },
            { "Randomizer", kRandoKey, "Shuffles", nullptr, "MM Mods", "Randomizer", "MM Mods" },
        };
        for (const Rename& r : kRenames) {
            clearAll();
            if (r.headerBefore != nullptr) {
                CVarSetString(kHeaderKey, r.headerBefore);
            }
            if (r.otherKey != nullptr && r.otherValue != nullptr) {
                CVarSetString(r.otherKey, r.otherValue);
            }
            if (r.comboBefore != nullptr) {
                CVarSetString(kComboKey, r.comboBefore);
            }
            CVarSetString(kSettingsKey, r.settingsBefore);
            ComboSectionMenuProbe renameProbe;
            renameProbe.AddMenuCombo();
            const std::string headerAfter = r.headerAfter != nullptr ? r.headerAfter : "(absent)";
            COMBO_CHECK(KeyState(kHeaderKey) == headerAfter && KeyState(kSettingsKey) == r.settingsAfter,
                        "a config on %s with Settings > %s opens on header %s with Settings > %s after AddMenuCombo, "
                        "expected %s with Settings > %s",
                        r.headerBefore != nullptr ? r.headerBefore : "(absent)", r.settingsBefore,
                        KeyState(kHeaderKey).c_str(), KeyState(kSettingsKey).c_str(), headerAfter.c_str(),
                        r.settingsAfter);
            for (const char* key : kKeys) {
                if (key == kHeaderKey || key == kSettingsKey) {
                    continue;
                }
                if (key == r.otherKey) {
                    const std::string expected = r.otherValue != nullptr
                                                     ? r.otherValue
                                                     : (r.comboBefore != nullptr ? "Quality of Life" : "(absent)");
                    COMBO_CHECK(KeyState(key) == expected,
                                "with Settings > %s saved, AddMenuCombo left %s = \"%s\", expected \"%s\"",
                                r.settingsBefore, key, KeyState(key).c_str(), expected.c_str());
                    continue;
                }
                COMBO_CHECK(!CVarExists(key), "with Settings > %s saved, AddMenuCombo left %s = \"%s\"",
                            r.settingsBefore, key, KeyState(key).c_str());
            }
            COMBO_CHECK(hostEntries.at("Settings").sidebars.contains(r.settingsAfter),
                        "Settings > %s carries to Settings > %s, which AddMenuSettings does not register, so the menu "
                        "would open on General",
                        r.settingsBefore, r.settingsAfter);
        }
        COMBO_CHECK(!hostEntries.at("Settings").sidebars.contains("Mod Menu"),
                    "AddMenuSettings still registers a \"Mod Menu\" page; SoH's page is OoT Mods (the ruling of "
                    "2026-10-27)");

        clearAll(); // the guards then put back what was there before this leg
        printf("[TEST] leg 6: a config on any former Combo page reopens on its new host's page (%zu carries), an "
               "absent, empty or unknown one opens on the default with no key stored, another open header keeps its "
               "selection, the Combo key is cleared every time, and a saved Settings > Mod Menu reopens on OoT Mods "
               "(%zu cases)\n",
               sizeof(kCarries) / sizeof(kCarries[0]), sizeof(kRenames) / sizeof(kRenames[0]));
    }

    // ---- Leg 7: SoH's Randomizer > Item Tracker page writes the overlay's keys
    // Operator ruling IT-D6 (#1141): one set of settings, SoH's. SoH's Item
    // Tracker Settings window (the page's Popout) writes every gTrackers.ItemTracker.*
    // key the overlay reads; the overlay's keys SoH has no twin for (its
    // Opacity, its three sections, MM's eight group keys) are an appended block
    // on that page, through the contributed-rows registry, and Combo > Tracker
    // Settings is gone. Every gCombo.Tracker.Items.* key in the manifest has
    // exactly one row there, with the overlay's own default and range, or the
    // console is a player's only writer for it. (No header holds a Tracker
    // Settings page: leg 1.)
    {
        ComboSectionMenuProbe itemProbe;
        AddSohTrackerPages(itemProbe);
        SidebarEntry& page = itemProbe.Entries().at("Randomizer").sidebars.at("Item Tracker");
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
        // The separator each row sits under.
        auto sectionOf = [&](const WidgetInfo* target) -> std::string {
            for (auto& column : page.columnWidgets) {
                std::string section;
                for (WidgetInfo& row : column) {
                    if (row.type == WIDGET_SEPARATOR_TEXT) {
                        section = row.name;
                    }
                    if (&row == target) {
                        return section;
                    }
                }
            }
            return "";
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
                    if (row.cVar == nullptr || std::string(row.cVar) != key) {
                        continue;
                    }
                    rows++;
                    const char* tip = row.options != nullptr ? row.options->tooltip : nullptr;
                    COMBO_CHECK(tip != nullptr && tip[0] != '\0' && std::string(tip).find("Combo") == std::string::npos,
                                "row '%s' (%s) has no tooltip, or one that says Combo (ruling D7)", row.name.c_str(),
                                key);
                    COMBO_CHECK(!row.raceDisable, "row '%s' is race-disabled; the overlay's look is cosmetic",
                                row.name.c_str());
                }
            }
            COMBO_CHECK(rows == 1, "Randomizer / Item Tracker holds %d rows writing %s, expected exactly one", rows,
                        key);
        }
        COMBO_CHECK(itemKeys == 12,
                    "the manifest holds %zu %s* keys; this leg was written against twelve (Opacity, three sections, "
                    "eight MM groups)",
                    itemKeys, kItemTrackerKeyPrefix);

        // Each row's widget, default and range are the overlay's
        // (ComboReadItemTrackerKeys reads every key with these defaults and
        // clamps), so the page never shows a value the overlay is not drawing.
        if (WidgetInfo* row = rowFor(RSBS_CVAR_COMBO_ITEMS_OPACITY)) {
            auto o = std::static_pointer_cast<UIWidgets::FloatSliderOptions>(row->options);
            COMBO_CHECK(row->type == WIDGET_CVAR_SLIDER_FLOAT && o->min == 0.1f && o->max == 1.0f &&
                            o->defaultValue == 1.0f && o->isPercentage,
                        "the Opacity row is not a percentage slider over 0.1..1 defaulting to 1, the overlay's clamp");
            COMBO_CHECK(sectionOf(row) == "Item Tracker Settings",
                        "the Opacity row is under \"%s\", not SoH's Item Tracker Settings separator",
                        sectionOf(row).c_str());
        }
        const struct {
            const char* key;
            const char* section;
        } kChecks[] = {
            { "gCombo.Tracker.Items.Section.OoT", "Item Tracker Sections" },
            { "gCombo.Tracker.Items.Section.MM", "Item Tracker Sections" },
            { "gCombo.Tracker.Items.Section.Shared", "Item Tracker Sections" },
            { "gCombo.Tracker.Items.MM.Tokens", "Majora's Mask Groups" },
            { "gCombo.Tracker.Items.MM.StrayFairies", "Majora's Mask Groups" },
            { "gCombo.Tracker.Items.MM.Dungeon", "Majora's Mask Groups" },
            { "gCombo.Tracker.Items.MM.BossSouls", "Majora's Mask Groups" },
            { "gCombo.Tracker.Items.MM.EnemySouls", "Majora's Mask Groups" },
            { "gCombo.Tracker.Items.MM.OwlStatues", "Majora's Mask Groups" },
            { "gCombo.Tracker.Items.MM.Time", "Majora's Mask Groups" },
            { "gCombo.Tracker.Items.MM.Misc", "Majora's Mask Groups" },
        };
        for (const auto& c : kChecks) {
            WidgetInfo* row = rowFor(c.key);
            COMBO_CHECK(row != nullptr, "Randomizer / Item Tracker has no row writing %s", c.key);
            if (row == nullptr) {
                continue;
            }
            auto o = std::static_pointer_cast<UIWidgets::CheckboxOptions>(row->options);
            COMBO_CHECK(row->type == WIDGET_CVAR_CHECKBOX && o->defaultValue,
                        "the %s row is not a checkbox defaulting to shown, the overlay's default", c.key);
            COMBO_CHECK(sectionOf(row) == c.section, "the %s row is under \"%s\", expected \"%s\"", c.key,
                        sectionOf(row).c_str(), c.section);
        }

        // Move to Default Position (#962) is the one row that writes no key: it
        // asks the floating overlay to return to its default place (under the
        // hearts), and shows only while SoH's Window Type is Floating.
        WidgetInfo* resetPos = FindRow(page, "Move to Default Position##ItemTracker");
        COMBO_CHECK(resetPos != nullptr && resetPos->type == WIDGET_BUTTON && resetPos->callback != nullptr &&
                        resetPos->cVar == nullptr && !resetPos->raceDisable,
                    "Randomizer / Item Tracker has no Move to Default Position button with a Callback and no key");
        if (resetPos != nullptr && resetPos->preFunc != nullptr) {
            const char* kSohWindowType = "gTrackers.ItemTracker.WindowType";
            const int savedResetType = CVarGetInteger(kSohWindowType, -1);
            CVarSetInteger(kSohWindowType, ComboGui::COMBO_ITEM_TRACKER_WINDOW);
            resetPos->ResetDisables();
            resetPos->preFunc(*resetPos);
            COMBO_CHECK(resetPos->isHidden, "the Move to Default Position row shows while the overlay is a window");
            CVarSetInteger(kSohWindowType, ComboGui::COMBO_ITEM_TRACKER_FLOATING);
            resetPos->ResetDisables();
            resetPos->preFunc(*resetPos);
            COMBO_CHECK(!resetPos->isHidden, "the Move to Default Position row is hidden while the overlay floats");
            if (savedResetType < 0) {
                CVarClear(kSohWindowType);
            } else {
                CVarSetInteger(kSohWindowType, savedResetType);
            }
        } else {
            COMBO_CHECK(false, "the Move to Default Position row has no PreFunc, so it shows for a windowed overlay");
        }

        // No other writer, and nothing of SoH's: SoH's own settings window
        // writes every gTrackers.ItemTracker.* key, so no row of ours does, and
        // none writes a retired twin.
        for (auto& column : page.columnWidgets) {
            for (WidgetInfo& row : column) {
                if (row.type == WIDGET_SEPARATOR_TEXT || row.type == WIDGET_TEXT || &row == resetPos ||
                    row.type == WIDGET_WINDOW_BUTTON) {
                    continue;
                }
                COMBO_CHECK(row.cVar != nullptr && RSBS::ComboKeyHasPrefix(row.cVar, kItemTrackerKeyPrefix),
                            "row '%s' on Randomizer / Item Tracker writes '%s', which is not one of the Item Tracker "
                            "overlay's own keys",
                            row.name.c_str(), row.cVar != nullptr ? row.cVar : "(null)");
                for (std::size_t r = 0; r < RSBS::kComboRetiredKeyCount; r++) {
                    COMBO_CHECK(row.cVar == nullptr || std::string(row.cVar) != RSBS::kComboRetiredKeys[r].key,
                                "row '%s' writes the retired %s", row.name.c_str(), RSBS::kComboRetiredKeys[r].key);
                }
            }
        }
        printf("[TEST] leg 7: Randomizer / Item Tracker writes each of the twelve %s* keys once with the overlay's "
               "defaults and ranges, under SoH's separators and its own, Move to Default Position floats only, and "
               "Combo / %s is gone (#1141)\n",
               kItemTrackerKeyPrefix, kTrackerSettingsPage);
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

        // c. The unified OoT pane lists what CheckTracker::IsVisibleInCheckTracker
        //    lists, which reads two settings statics (Always Show Gold
        //    Skulltulas, Hide Unshuffled Shop Item Checks). SoH's DrawElement
        //    loaded them from their keys every frame; retired, it never runs, so
        //    the window's per-frame path (what Gui::Draw runs for every
        //    registered window: Update(), then Draw()) must load them itself,
        //    from the keys and never by flipping. Statics at their defaults,
        //    keys on, then keys off.
        {
            int gsBefore = 0;
            int shopsBefore = 0;
            RandoTest_CheckTrackerVisibilityStatics(&gsBefore, &shopsBefore);
            IntKeyGuard gsKey("gTrackers.CheckTracker.AlwaysShowGSLocs");
            IntKeyGuard shopsKey("gTrackers.CheckTracker.HideUnshuffledShopChecks");
            RandoTest_CheckTrackerSetVisibilityStatics(0, 0);
            CVarSetInteger("gTrackers.CheckTracker.AlwaysShowGSLocs", 1);
            CVarSetInteger("gTrackers.CheckTracker.HideUnshuffledShopChecks", 1);
            check->Update();
            check->Draw();
            int gsOn = -1;
            int shopsOn = -1;
            RandoTest_CheckTrackerVisibilityStatics(&gsOn, &shopsOn);
            CVarSetInteger("gTrackers.CheckTracker.AlwaysShowGSLocs", 0);
            CVarSetInteger("gTrackers.CheckTracker.HideUnshuffledShopChecks", 0);
            check->Update();
            check->Draw();
            int gsOff = -1;
            int shopsOff = -1;
            RandoTest_CheckTrackerVisibilityStatics(&gsOff, &shopsOff);
            RandoTest_CheckTrackerSetVisibilityStatics(gsBefore, shopsBefore);
            printf("[TEST] leg 8: one frame of the retired Check Tracker loaded Always Show Gold Skulltulas %d and "
                   "Hide Unshuffled Shop Item Checks %d with both keys on, %d and %d with both off\n",
                   gsOn, shopsOn, gsOff, shopsOff);
            COMBO_CHECK(gsOn == 1 && shopsOn == 1,
                        "with Always Show Gold Skulltulas and Hide Unshuffled Shop Item Checks on, a frame of the "
                        "retired Check Tracker left their statics off: the unified OoT pane ignores both settings "
                        "until the rows are toggled");
            COMBO_CHECK(gsOff == 0 && shopsOff == 0,
                        "with both keys off, a frame of the retired Check Tracker left a static on: the statics do "
                        "not follow their keys");
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

    // ---- Leg 9: OoT Mods, MM Mods and Save Files under SoH's headers (#1133) ----
    // The operator's ruling of 2026-10-27 ("Have two rows, OoT Mods and MM Mods
    // instead of just Mods"): Settings has two mod pages where it had one. OoT
    // Mods is SoH's Mod Menu page with only its sidebar label changed: its one
    // row, SoH's Popout Mod Menu Window, opening SoH's "Mod Menu" window over
    // SoH's key. MM Mods is a page of its own directly after it, holding the
    // MM Mods page's rows in their order and with their behaviour (the note's
    // PreFunc, Rescan's Callback, the list's CustomFunction) and no "Majora's
    // Mask" heading (the page's name says it); Rescan Mods Folder goes through
    // the hosted-row seam, bound to the src/common accessor it calls. Ruling D4:
    // Save Files is a Dev Tools page, after every page of SoH's, its rows in
    // their order.
    {
        ComboSectionMenuProbe hostProbe;
        hostProbe.AddMenuSettings();
        hostProbe.AddMenuDevTools();
        auto& hostEntries = hostProbe.Entries();
        auto& settingsPages = hostEntries.at("Settings").sidebars;
        const auto& settingsOrder = hostEntries.at("Settings").sidebarOrder;

        COMBO_CHECK(!settingsPages.contains("Mod Menu"),
                    "Settings still holds a \"Mod Menu\" page; SoH's page is labelled OoT Mods (the ruling of "
                    "2026-10-27)");
        const auto ootPos = std::find(settingsOrder.begin(), settingsOrder.end(), std::string("OoT Mods"));
        const auto mmPos = std::find(settingsOrder.begin(), settingsOrder.end(), std::string("MM Mods"));
        COMBO_CHECK(ootPos != settingsOrder.end() && mmPos != settingsOrder.end() && mmPos == ootPos + 1,
                    "Settings reads [%s]; MM Mods belongs directly after OoT Mods", Joined(settingsOrder).c_str());

        if (!settingsPages.contains("OoT Mods")) {
            printf("[TEST] FAIL(9): AddMenuSettings registered no Settings > OoT Mods page\n");
            gFailures++;
        } else {
            SidebarEntry& ootMods = settingsPages.at("OoT Mods");
            const std::vector<std::string> kOoTMods = { "Popout Mod Menu Window" };
            COMBO_CHECK(RowNames(ootMods) == kOoTMods && ootMods.columnCount == 1,
                        "Settings / OoT Mods reads [%s] in %u column(s), expected SoH's own [%s] in one",
                        Joined(RowNames(ootMods)).c_str(), ootMods.columnCount, Joined(kOoTMods).c_str());
            WidgetInfo* popout = FindRow(ootMods, "Popout Mod Menu Window");
            if (popout != nullptr) {
                COMBO_CHECK(popout->type == WIDGET_WINDOW_BUTTON && popout->cVar != nullptr &&
                                std::string(popout->cVar) == CVAR_WINDOW("ModMenu") &&
                                std::string(popout->windowName) == "Mod Menu" && popout->hideInSearch,
                            "Settings / OoT Mods' Popout Mod Menu Window is not SoH's row (a window button over %s "
                            "opening the \"Mod Menu\" window, hidden from search)",
                            CVAR_WINDOW("ModMenu"));
            }
        }

        if (!settingsPages.contains("MM Mods")) {
            printf("[TEST] FAIL(9): AddMenuSettings registered no Settings > MM Mods page\n");
            gFailures++;
        } else {
            SidebarEntry& mmMods = settingsPages.at("MM Mods");
            const std::vector<std::string> kMmMods = {
                "MM Mods Status",
                "Rescan Mods Folder",
                "MM Mod List",
            };
            const std::vector<std::string> modRows = RowNames(mmMods);
            COMBO_CHECK(modRows == kMmMods, "Settings / MM Mods reads [%s], expected [%s]", Joined(modRows).c_str(),
                        Joined(kMmMods).c_str());
            COMBO_CHECK(mmMods.columnCount == 1 && mmMods.columnWidgets.size() >= 1 &&
                            mmMods.columnWidgets.at(0).size() == RowCount(mmMods),
                        "Settings / MM Mods is not one column holding every row");
            for (const auto& column : mmMods.columnWidgets) {
                for (const WidgetInfo& row : column) {
                    COMBO_CHECK(row.type != WIDGET_SEPARATOR_TEXT,
                                "Settings / MM Mods carries the heading \"%s\"; a page named MM Mods needs no "
                                "\"Majora's Mask\" heading",
                                row.name.c_str());
                }
            }
            WidgetInfo* note = FindRow(mmMods, "MM Mods Status");
            if (note != nullptr) {
                auto text = std::static_pointer_cast<UIWidgets::TextOptions>(note->options);
                COMBO_CHECK(note->type == WIDGET_TEXT && note->preFunc != nullptr && note->hideInSearch &&
                                !note->raceDisable && text != nullptr && text->color == UIWidgets::Colors::Gray,
                            "the MM Mods state note is not the gray, unsearched, PreFunc-written text row it was");
            }
            WidgetInfo* rescan = FindRow(mmMods, "Rescan Mods Folder");
            if (rescan != nullptr) {
                COMBO_CHECK(rescan->type == WIDGET_BUTTON && rescan->callback != nullptr && rescan->cVar == nullptr &&
                                !rescan->raceDisable && rescan->options != nullptr &&
                                rescan->options->tooltip != nullptr && rescan->options->tooltip[0] != '\0',
                            "Rescan Mods Folder is not the race-free button with a Callback and a tooltip it was");
            }
            WidgetInfo* list = FindRow(mmMods, "MM Mod List");
            if (list != nullptr) {
                COMBO_CHECK(list->type == WIDGET_CUSTOM && list->customFunction != nullptr && !list->raceDisable,
                            "MM Mod List is not the race-free custom row that draws the list");
            }
            for (const char* hosted : { "Rescan Mods Folder" }) {
                const SohGui::HostedMmRow* entry = nullptr;
                for (const SohGui::HostedMmRow& h : SohGui::HostedMmRows()) {
                    if (h.header == "Settings" && h.sidebar == "MM Mods" && h.name == hosted) {
                        entry = &h;
                    }
                }
                COMBO_CHECK(entry != nullptr && entry->section.empty() && entry->key == nullptr &&
                                entry->accessor != nullptr,
                            "\"%s\" on Settings / MM Mods is not recorded by the hosted-row seam, under no heading, "
                            "with its accessor, so the UI snapshot neither captures nor lints it",
                            hosted);
            }
        }
        for (const SohGui::HostedMmRow& h : SohGui::HostedMmRows()) {
            COMBO_CHECK(!(h.header == "Settings" && (h.sidebar == "OoT Mods" || h.sidebar == "Mod Menu")),
                        "the hosted-row seam records \"%s\" on Settings / %s; SoH's mod page holds SoH's row only",
                        h.name.c_str(), h.sidebar.c_str());
        }

        const auto& devOrder = hostEntries.at("Dev Tools").sidebarOrder;
        const auto savePos = std::find(devOrder.begin(), devOrder.end(), std::string("Save Files"));
        const auto gfxPos = std::find(devOrder.begin(), devOrder.end(), std::string("Gfx Debugger"));
        COMBO_CHECK(savePos != devOrder.end() && gfxPos != devOrder.end() && savePos > gfxPos,
                    "Dev Tools reads [%s]; Save Files belongs after SoH's last page, Gfx Debugger (ruling D4)",
                    Joined(devOrder).c_str());
        if (hostEntries.at("Dev Tools").sidebars.contains("Save Files")) {
            SidebarEntry& saveFiles = hostEntries.at("Dev Tools").sidebars.at("Save Files");
            const std::vector<std::string> kSaveFiles = { "Save Files Status", "File Status", "Save File List" };
            COMBO_CHECK(RowNames(saveFiles) == kSaveFiles && saveFiles.columnCount == 1,
                        "Dev Tools / Save Files reads [%s] in %u column(s), expected [%s] in one",
                        Joined(RowNames(saveFiles)).c_str(), saveFiles.columnCount, Joined(kSaveFiles).c_str());
        }
        printf("[TEST] leg 9: Settings holds OoT Mods (SoH's row only) and then MM Mods (MM's mod list rows, no "
               "heading), and Dev Tools ends with Save Files\n");
    }

    if (gFailures == 0) {
        printf("[TEST] menu-combo-section: PASS\n");
    } else {
        printf("[TEST] menu-combo-section: %d failure(s)\n", gFailures);
    }
    return gFailures;
}

#endif // RSBS_SINGLE_EXECUTABLE
