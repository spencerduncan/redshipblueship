/**
 * @file soh_combo_settings_rows_test.cpp
 * @brief ROM-free, display-free lock for #655 (and #668): the six tier-4 combo
 *        settings must exist as SohMenu ROWS in the tier-4 Combo section,
 *        carry ADR
 *        0004 §4.2's persistent marker, and behave the way ADR 0004 §6 state 4
 *        requires — editable before the creation event, read-only afterwards
 *        with the MODEL's reason and the values from the SAVE.
 *
 * CTest label "redship", row ComboSettingsRows in CMake/SingleExecutable.cmake,
 * dispatch "combo-settings-rows" in src/common/test_runner.cpp.
 *
 * WHY THIS ROW EXISTS. PR #652 rendered the five `gCombo.Rando.*` keys in a
 * common-owned pop-out pane and locked it with ComboSettingsWindow. Operator
 * direction on #655 moved the presentation into the menu itself, and a pane's
 * lock says nothing about a row. The model's own lock
 * (combo-settings-authoring) still owns the resolver, the pinned value spaces
 * and the writers' refusal; this one owns the seam between that model and the
 * menu, which is where a presentation move can silently go wrong in three ways:
 *
 *   1. THE ROWS ARE NOT THERE, or not where the issue says. AddMenuRandomizer
 *      registers into a container, so a row that was never added is not a
 *      compile error and not a crash — it is a settings page that is missing a
 *      setting, which is exactly the report behind #655's predecessor #509.
 *
 *   2. A ROW IS ITS OWN WRITER. `WIDGET_CVAR_COMBOBOX` and friends call
 *      CVarSetInteger themselves (UIWidgets::CVarCombobox), which would put a
 *      second, ungated writer beside src/common's and make the freeze gate
 *      decorative — ADR 0004 §6's "the gate belongs on the write choke points,
 *      not the widget", inverted. Leg 2 asserts NO row in the section binds one
 *      of the five keys as its `cVar`, and legs 4/5 drive the rows' Callbacks
 *      to show that the one write path is Combo_ComboSettingSet and that it
 *      refuses once frozen.
 *
 *   3. A FROZEN ROW SHOWS THE CVAR. §6 state 4 requires the value shown after
 *      creation to come FROM THE SAVE, because the two may legitimately differ
 *      and the save is what the world was built from. Leg 5 freezes a record
 *      that differs from the authored CVars IN EVERY FIELD and asserts every
 *      row stages the record's value, not the store's — a test that froze the
 *      same values it authored would pass with the accessor wired to the wrong
 *      source, which is the vacuity trap this file is shaped to avoid.
 *
 * WHERE THE ROWS LIVE NOW (#497 step 6). They were registered by
 * `SohGui::AddCrossGameWidgets` into `Randomizer → Cross-Game`, a page that said
 * in-tree it was the interim host until the tier-4 Combo section existed. That
 * section exists, so this lock followed them to `Combo → Cross-Game Rules`,
 * registered by `SohMenu::AddMenuCombo()` (which registers no header since
 * #1133 retired it). #1101 (operator ruling 2026-10-21,
 * option C) moved them back under SoH's Randomizer header as
 * `Randomizer → Cross-Game`, the one randomizer menu for both games, registered
 * by `SohGui::AddComboRandomizerPages()` (our slice of `AddMenuRandomizer()`),
 * and this lock follows them again. `MenuComboSection` owns the headers' shape
 * (which pages, in which order, the persisted selection); this row still owns
 * the seam between the model and the rows, which no move changed.
 *
 * HOW IT OBSERVES. It builds a real SohMenu headless and calls the REAL
 * production entry points, `AddComboRandomizerPages()` and
 * `AddComboDevToolsPages()` — not test-only registrars. That is possible here
 * and was not possible for `AddMenuRandomizer()` as a whole (see below): nothing
 * in our slices reaches `Rando::Settings`. It is safe with no
 * window and no ImGui context because registration is container work — every
 * widget's heavy lifting lives in a lambda that only the draw path calls — and a
 * `Ship::GuiWindow` constructed
 * with an EMPTY visibility CVar touches the Ship::Context not at all
 * (GuiWindow.cpp guards the latch on `!mVisibilityConsoleVariable.empty()`).
 * The rows' PreFuncs and Callbacks are then called DIRECTLY, exactly as
 * Menu::MenuDrawItem would (ResetDisables() first, then preFunc), which is
 * possible only because they touch no ImGui: they read the src/common model,
 * write a staging buffer, and set `options->disabled`. Staged values are read
 * back through each widget's own `valuePointer`, so the test asserts on what
 * the widget would render rather than on file-static storage it cannot see.
 *
 * WHY IT REGISTERS ONE SECTION AND NOT THE WHOLE MENU. `AddMenuRandomizer()` — the
 * rows' host before #497 step 6 — cannot run ROM-free, and not for a shallow
 * reason: its five `Rando::Settings` option groups reach `Option::AddWidget`,
 * which **runs each option's callback at registration time**
 * (`Enhancements/randomizer/option.cpp:330`), and those callbacks dereference
 * `OTRGlobals::Instance->gRandoContext` — null in a display-free harness, an
 * access violation reading offset 0x80. (They also register through the GLOBAL
 * `SohGui::mSohMenu`, `option.cpp:453`, rather than through the menu whose
 * method is running.) Our slices have no such dependency, which is a
 * consequence of the move worth recording: the rows are now reachable from a
 * headless harness through the same call production makes, so the ORDER of the
 * pages and the `WidgetPath` each one receives ARE covered, where under the
 * interim host they were not.
 *
 * WHY IT SEARCHES EVERY COLUMN AND CHECKS THE COLUMN INDEX ANYWAY.
 * `Menu::DrawElement` draws only `columnCount` columns, so a row in a higher
 * column is registered and invisible — the #640 failure mode with the halves
 * swapped. The rows are located by name across every column, and the column each
 * landed in is asserted to be one the page actually draws, so the pin is
 * observed rather than trusted.
 *
 * WHAT IS NOT COVERED. Appearance. No headless test can assert that a row is
 * legible, ordered sensibly, or that the marker reads well — that is operator
 * verification on the nightly. What is asserted is that the marker is PRESENT
 * in the row's name (a tooltip would not satisfy §4.2) and that the state the
 * row is in matches the state the writers are in.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "soh/SohGui/SohMenu.h"

#include <cctype>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "combo_settings_view.h"
#include "context.h"
#include "foreign_items.h"

// The Reset confirm's registrar (SohMenuCombo.cpp). Declared here rather than
// in a header, so no production header grows a test-only entry point. SoH's
// popup machinery runs a button only from an ImGui frame, so this lock swaps in
// a recorder to reach the popup's Reset action.
namespace SohGui {
extern void (*gComboRulePopupRegistrar)(std::string, std::string, std::string, std::string, std::function<void()>,
                                        std::function<void()>);
} // namespace SohGui

namespace {

int gFailures = 0;

#define ROWS_CHECK(cond, ...)                                              \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            printf("[TEST]       ");                                       \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
            gFailures++;                                                   \
        }                                                                  \
    } while (0)

// The menu keeps its pages in a protected member of Ship::Menu, and there is no
// public reader — deliberately, since nothing in production needs one. A derived
// probe is the least invasive way in: no production header changes, and the
// widgets under test are the real ones our production slices built.
class ComboRulesMenuProbe final : public SohGui::SohMenu {
  public:
    // An EMPTY visibility CVar: GuiWindow's ctor then never reaches
    // Ship::Context, so the probe is constructible in a process with no window
    // and (if it came to it) no console variables either.
    ComboRulesMenuProbe() : SohGui::SohMenu("", "Combo Rules Row Probe") {
    }

    std::unordered_map<std::string, MainMenuEntry>& Entries() {
        return menuEntries;
    }
};

/** The row name the menu builds for @p id: §4.2's marker, then the model's label. */
std::string RowName(ComboSettingId id) {
    return std::string(Combo_ComboSettingSharedMarker()) + " " + Combo_ComboSettingLabel(id);
}

// Every widget of one sidebar page, flattened across its columns, each paired
// with the column it landed in. Pointers into the page's own vectors, which is
// safe because nothing is registered after the flatten.
using PageRow = std::pair<WidgetInfo*, uint32_t>;

std::vector<PageRow> FlattenPage(std::vector<std::vector<WidgetInfo>>& columns) {
    std::vector<PageRow> flat;
    for (uint32_t column = 0; column < columns.size(); column++) {
        for (WidgetInfo& row : columns.at(column)) {
            flat.emplace_back(&row, column);
        }
    }
    return flat;
}

WidgetInfo* FindRow(std::vector<PageRow>& rows, const std::string& name, uint32_t* columnOut = nullptr) {
    for (PageRow& row : rows) {
        if (row.first->name == name) {
            if (columnOut != nullptr) {
                *columnOut = row.second;
            }
            return row.first;
        }
    }
    return nullptr;
}

/** Drive one row the way Menu::MenuDrawItem does, minus the drawing. */
void RunPreFunc(WidgetInfo& row) {
    if (row.preFunc == nullptr) {
        return;
    }
    row.ResetDisables();
    row.preFunc(row);
}

void RunAllPreFuncs(std::vector<PageRow>& rows) {
    for (PageRow& row : rows) {
        RunPreFunc(*row.first);
    }
}

int32_t StagedInt(WidgetInfo& row) {
    int32_t* p = std::get<int32_t*>(row.valuePointer);
    return p != nullptr ? *p : -1;
}

/** The same, for a checkbox row: the Shared Kokiri Sword (#886) is a comboFlags
 *  BIT, so its staging buffer is a bool rather than an int32_t. -1 for "no
 *  buffer", which is never a legal staged value. */
int StagedBool(WidgetInfo& row) {
    bool* p = std::get<bool*>(row.valuePointer);
    return p != nullptr ? (*p ? 1 : 0) : -1;
}

/** #1065: the Shared Lens of Truth row, found by its NAME TEXT (the marker and
 *  the rule's label) so the lock compiles, and goes red, on a tree that has no
 *  such row yet; and its rule bit (RSBS_XITEM_SHARED_LENS). */
const char* const kLensLabel = "Shared Lens of Truth";
const uint32_t kLensBit = 0x00200000u;
std::string LensRowName() {
    return std::string(Combo_ComboSettingSharedMarker()) + " " + kLensLabel;
}

/**
 * The disabled tooltip a frozen row must show: MenuDrawItem's disabled shape
 * ("This setting is disabled because: \n" then "\n- Reason", Menu.cpp) around
 * the MODEL's reason in SoH's Title Case ("already decided" -> "Already
 * Decided"). Built here from Combo_ComboSettingReadOnlyReason, so a row whose
 * tooltip carries words the model does not own is red.
 */
std::string ExpectedDecidedTooltip() {
    const char* reason = Combo_ComboSettingReadOnlyReason();
    std::string text = "This setting is disabled because: \n\n- ";
    bool wordStart = true;
    for (const char* c = (reason != nullptr) ? reason : ""; *c != '\0'; c++) {
        text += wordStart ? (char)std::toupper((unsigned char)*c) : *c;
        wordStart = (*c == ' ');
    }
    return text;
}

/**
 * Assert @p row is in ADR 0004 §6 state 4: disabled, with the reason the MODEL
 * owns. The reason's identity matters as much as the disabling — a row that
 * greyed itself with a capability reason ("not yet available") would send a
 * player hunting for a missing feature instead of telling them the choice was
 * already made. Compared EXACTLY against the model's reason in SoH's disabled
 * shape: a containment check passed a hard-coded tooltip that added
 * renderer-only words ("When This World Was Created") the corrupt state
 * contradicts.
 */
void ExpectDecided(WidgetInfo& row, const char* what) {
    const char* reason = Combo_ComboSettingReadOnlyReason();
    const std::string tip = row.options->disabledTooltip != nullptr ? row.options->disabledTooltip : "";
    const std::string want = ExpectedDecidedTooltip();
    ROWS_CHECK(row.options->disabled, "%s must be read-only once the record is frozen", what);
    ROWS_CHECK(reason != nullptr && tip == want,
               "%s must carry exactly the model's reason (%s) in SoH's disabled shape, not a capability reason or "
               "renderer-only words; it carries '%s', expected '%s'",
               what, reason != nullptr ? reason : "(null)", tip.c_str(), want.c_str());
}

/** One popup the Reset row queued through gComboRulePopupRegistrar. */
struct RecordedPopup {
    std::string title;
    std::string button1;
    std::string button2;
    std::function<void()> onButton1;
    std::function<void()> onButton2;
};
std::vector<RecordedPopup> gRecordedPopups;

void RecordPopup(std::string title, std::string message, std::string button1, std::string button2,
                 std::function<void()> onButton1, std::function<void()> onButton2) {
    (void)message;
    gRecordedPopups.push_back(RecordedPopup{ std::move(title), std::move(button1), std::move(button2),
                                             std::move(onButton1), std::move(onButton2) });
}

} // namespace

extern "C" int OoT_ComboSettingsRows_RunHeadless(void) {
    printf("[TEST] combo-settings-rows: the three tier-4 combo settings are SohMenu rows in the Combo section's "
           "Cross-Game Rules page, marked per ADR 0004 §4.2, and read-only from the save once frozen (#655, #668, "
           "#497 step 6)\n");

    gFailures = 0;

    // A clean model before the menu is built: every PreFunc below reads it.
    ComboContext_Init();
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        Combo_ComboSettingClear((ComboSettingId)i);
    }

    // The REAL production entry points, not test-only registrars:
    // AddComboRandomizerPages() is the slice of AddMenuRandomizer() that creates
    // our Randomizer pages (the rules among them), and AddComboDevToolsPages()
    // the slice of AddMenuDevTools() that creates our Dev Tools pages (Save
    // Files since #1133, which retired the Combo header); neither reaches
    // Rando::Settings. So the page layout under test is the one a player gets,
    // including which column each row lands in.
    ComboRulesMenuProbe probe;
    probe.AddMenuEntry("Randomizer", "gSettings.Menu.RandomizerSidebarSection");
    WidgetPath randoPath = { "Randomizer", "Tricks/Glitches", SECTION_COLUMN_1 };
    SohGui::AddComboRandomizerPages(probe, randoPath);
    probe.AddMenuEntry("Dev Tools", "gSettings.Menu.DevToolsSidebarSection");
    WidgetPath devToolsPath = { "Dev Tools", "General", SECTION_COLUMN_1 };
    SohGui::AddComboDevToolsPages(probe, devToolsPath);

    // ---- Leg 1: the section and the two rows exist ----------------------------
    auto& entries = probe.Entries();
    if (!entries.contains("Randomizer") || !entries.contains("Dev Tools")) {
        printf("[TEST] FAIL(1): the probe has no \"Randomizer\" and/or \"Dev Tools\" menu entry\n");
        return 1;
    }
    auto& randoSidebars = entries.at("Randomizer").sidebars;
    if (!randoSidebars.contains("Cross-Game") || randoSidebars.at("Cross-Game").columnWidgets.empty() ||
        randoSidebars.at("Cross-Game").columnWidgets.at(0).empty()) {
        printf("[TEST] FAIL(1): the \"Randomizer\" menu has no \"Cross-Game\" page with rows -- the host for the "
               "tier-4 combo rules under the one randomizer menu (operator ruling 2026-10-21)\n");
        return 1;
    }
    const SidebarEntry& page = randoSidebars.at("Cross-Game");
    auto& columns = randoSidebars.at("Cross-Game").columnWidgets;
    if (columns.empty()) {
        printf("[TEST] FAIL(1): the Cross-Game Rules page has no widget columns\n");
        return 1;
    }
    std::vector<PageRow> rows = FlattenPage(columns);

    // Leg 2 asserts a property of the WHOLE section (no pop-out for the three keys
    // survives anywhere in it), because the window rows moved to a sibling page:
    // checking only the rules page would make that assertion vacuous.
    std::vector<PageRow> sectionRows = rows;
    // Every other page of ours as well, under both of SoH's headers we add pages
    // to: the window rows lived on Combo > Windows until #1132 retired it (the
    // windows open from SoH's tracker pages now), the Combo header retired with
    // #1133, and no page of ours may grow a pop-out for the rules' keys either.
    for (const char* header : { "Randomizer", "Dev Tools" }) {
        for (auto& [sidebarName, sidebar] : entries.at(header).sidebars) {
            if (std::string(header) == "Randomizer" && sidebarName == "Cross-Game") {
                continue; // the rules page, already in `rows`
            }
            for (PageRow& otherRow : FlattenPage(sidebar.columnWidgets)) {
                sectionRows.push_back(otherRow);
            }
        }
    }
    if (sectionRows.size() <= rows.size()) {
        printf("[TEST] FAIL: no other page of ours has rows; leg 2's no-pop-out assertion would cover only the "
               "rules page\n");
        gFailures++;
    }

    struct ExpectedRow {
        ComboSettingId id;
        WidgetType type;
        const char* suffix; // "": the bare name; ": %d": the slider's value format
    };
    // The direction is a combobox over the four pinned enumerators and the
    // Shared Kokiri Sword (#886) a plain checkbox over the model's 0/1 space.
    // No pool-size sliders (#801), no item-class groups (#834) and no Shared
    // Ocarina checkbox (#879).
    const ExpectedRow kExpected[] = {
        { COMBO_SETTING_DIRECTION, WIDGET_COMBOBOX, "" },
        { COMBO_SETTING_SHARED_KOKIRI_SWORD, WIDGET_CHECKBOX, "" },
        // #1068's Shared Shields: a checkbox over a bit of the item rules record.
        { COMBO_SETTING_SHARED_SHIELDS, WIDGET_CHECKBOX, "" },
        // #1065: the Shared Lens of Truth, the sword's shape over an item rule.
        { COMBO_SETTING_SHARED_LENS, WIDGET_CHECKBOX, "" },
        // #1066: the two shared songs, the lens's shape.
        { COMBO_SETTING_SHARED_SONG_EPONA, WIDGET_CHECKBOX, "" },
        { COMBO_SETTING_SHARED_SONG_STORMS, WIDGET_CHECKBOX, "" },
        // #1067's Shared Ocarina Buttons: the same shape.
        { COMBO_SETTING_SHARED_OCARINA_BUTTONS, WIDGET_CHECKBOX, "" },
        // #1369: the Shared Swim, the lens's shape (shuffle-gated, ships on).
        { COMBO_SETTING_SHARED_SWIM, WIDGET_CHECKBOX, "" },
        // #1069: the Shared Song of Time, the lens's shape.
        { COMBO_SETTING_SHARED_SONG_TIME, WIDGET_CHECKBOX, "" },
        // #968: the three shared arrows, the lens's shape.
        { COMBO_SETTING_SHARED_ARROW_FIRE, WIDGET_CHECKBOX, "" },
        { COMBO_SETTING_SHARED_ARROW_ICE, WIDGET_CHECKBOX, "" },
        { COMBO_SETTING_SHARED_ARROW_LIGHT, WIDGET_CHECKBOX, "" },
        // #970's Shared Bottles: the same shape.
        { COMBO_SETTING_SHARED_BOTTLES, WIDGET_CHECKBOX, "" },
        // #978: the five shared masks, the lens's shape.
        { COMBO_SETTING_SHARED_MASK_GORON, WIDGET_CHECKBOX, "" },
        { COMBO_SETTING_SHARED_MASK_ZORA, WIDGET_CHECKBOX, "" },
        { COMBO_SETTING_SHARED_MASK_BUNNY, WIDGET_CHECKBOX, "" },
        { COMBO_SETTING_SHARED_MASK_KEATON, WIDGET_CHECKBOX, "" },
        { COMBO_SETTING_SHARED_MASK_TRUTH, WIDGET_CHECKBOX, "" },
        // #979: the Blast Mask in OoT and its shared variant, the lens's shape, and
        // the Blast Mask Cooldown, a combobox over the six pinned cooldowns.
        { COMBO_SETTING_BLAST_MASK_OOT, WIDGET_CHECKBOX, "" },
        { COMBO_SETTING_SHARED_MASK_BLAST, WIDGET_CHECKBOX, "" },
        { COMBO_SETTING_BLAST_MASK_COOLDOWN, WIDGET_COMBOBOX, "" },
        // #979: the Stone Mask in OoT and its shared variant, the Blast Mask's shape.
        { COMBO_SETTING_STONE_MASK_OOT, WIDGET_CHECKBOX, "" },
        { COMBO_SETTING_SHARED_MASK_STONE, WIDGET_CHECKBOX, "" },
        // ADR 0010 D1's goal: a combobox over the five pinned RSBS_COMBO_GOAL_*
        // enumerators, the direction row's shape.
        { COMBO_SETTING_GOAL, WIDGET_COMBOBOX, "" },
        // #1367: [Both Games] Logic, a combobox over the three pinned
        // RSBS_COMBO_RUNG_* enumerators, the goal row's shape.
        { COMBO_SETTING_LOGIC, WIDGET_COMBOBOX, "" },
        // #1389: OoTMM's triforcePieces and triforceGoal, SoH's "Name: %d"
        // slider shape (its own Triforce Hunt sliders'), under the goal.
        { COMBO_SETTING_TRIFORCE_PIECES, WIDGET_SLIDER_INT, ": %d" },
        { COMBO_SETTING_TRIFORCE_GOAL, WIDGET_SLIDER_INT, ": %d" },
    };

    // Captured by its REGISTERED name, before any PreFunc runs: the status row
    // rewrites its own name every frame, so this is the only moment it has a
    // stable one.
    WidgetInfo* statusRow = FindRow(rows, "Combo Rules Status");
    ROWS_CHECK(statusRow != nullptr, "the Cross-Game Rules page has no \"Combo Rules Status\" row -- ADR 0004 §6 "
                                     "state 4's reason has nowhere to be legible without hovering");

    WidgetInfo* settingRow[COMBO_SETTING_COUNT] = {};
    for (const ExpectedRow& expected : kExpected) {
        const std::string name = RowName(expected.id) + expected.suffix;
        uint32_t column = 0;
        WidgetInfo* row = FindRow(rows, name, &column);
        ROWS_CHECK(row != nullptr, "no Cross-Game Rules row named '%s' -- the setting '%s' is unreachable in the menu",
                   name.c_str(), Combo_ComboSettingLabel(expected.id));
        if (row == nullptr) {
            continue;
        }
        settingRow[expected.id] = row;
        // Registered past the page's column count is registered and never drawn:
        // Menu::DrawElement iterates columnCount columns, not every column the
        // preceding option groups happened to leave behind.
        ROWS_CHECK(column < page.columnCount,
                   "row '%s' landed in column %u of a page that draws %u -- it would be registered and invisible",
                   name.c_str(), column, page.columnCount);
        ROWS_CHECK(row->type == expected.type, "row '%s' has widget type %d, expected %d", name.c_str(), (int)row->type,
                   (int)expected.type);
        // §4.2 is a requirement on the ROW, legible without hovering: the marker
        // must be in the name and not merely in the tooltip.
        ROWS_CHECK(name.rfind(Combo_ComboSettingSharedMarker(), 0) == 0,
                   "row '%s' does not lead with the shared-intent marker '%s'", name.c_str(),
                   Combo_ComboSettingSharedMarker());
    }
    if (gFailures != 0) {
        printf("[TEST] combo-settings-rows: %d failure(s) in leg 1; the later legs need the rows\n", gFailures);
        return gFailures;
    }
    printf("[TEST] leg 1: all three tier-4 settings are rows in Randomizer / Cross-Game, each marked '%s'\n",
           Combo_ComboSettingSharedMarker());

    // ---- Leg 1b: the goal row offers exactly OoTMM's goals, in its words ----
    // The value space is the pinned RSBS_COMBO_GOAL_* table (1..5) and each label
    // is OoTMM's own name for the value (packages/core/src/settings/data.ts:
    // any, ganon, majora, both, triforce; its triforce3 is not offered). A
    // sixth entry would be a goal this
    // build has no evaluator for; a missing one, a value the player cannot pick.
    // Its tooltip names every value as a "Value: effect" line (ui-style-guide
    // R-N6), so a label with no explanation is red too.
    {
        WidgetInfo* goalRow = settingRow[COMBO_SETTING_GOAL];
        auto goalOptions = std::static_pointer_cast<UIWidgets::ComboboxOptions>(goalRow->options);
        const struct {
            uint8_t value;
            const char* label;
        } kGoalLabels[] = {
            { (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH, "Ganon & Majora" },
            { (uint8_t)RSBS_COMBO_GOAL_BEAT_EITHER, "Any Final Boss" },
            { (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT, "Triforce Hunt" },
            { (uint8_t)RSBS_COMBO_GOAL_BEAT_OOT, "Ganon" },
            { (uint8_t)RSBS_COMBO_GOAL_BEAT_MM, "Majora" },
        };
        ROWS_CHECK(goalOptions->comboMap.size() == 5, "the goal row offers %zu values, expected the five pinned goals",
                   goalOptions->comboMap.size());
        const std::string tooltip = goalOptions->tooltip != nullptr ? goalOptions->tooltip : "";
        for (const auto& g : kGoalLabels) {
            ROWS_CHECK(Combo_ComboSettingValueValid(COMBO_SETTING_GOAL, (int32_t)g.value),
                       "goal %u is offered by the row but refused by the model's value space", (unsigned)g.value);
            ROWS_CHECK(goalOptions->comboMap.contains((int32_t)g.value) &&
                           std::string(goalOptions->comboMap.at((int32_t)g.value)) == g.label,
                       "goal %u is not labelled '%s' (OoTMM's name for it)", (unsigned)g.value, g.label);
            ROWS_CHECK(tooltip.find(std::string("\n") + g.label + ": ") != std::string::npos,
                       "the goal row's tooltip has no '%s: effect' line", g.label);
        }
        // ADR 0010 section 1.2: a goal that leaves a half without proof is
        // "documented at the setting". Each such line names what may be
        // unfinishable, and the tooltip does not promise that meeting the goal
        // ends the game (OoTMM's does; this build plays each game's own ending).
        const struct {
            const char* label;
            const char* warns;
        } kGoalWarnings[] = {
            { "Any Final Boss", "The other game may be unfinishable." },
            { "Ganon", "Majora's Mask may be unfinishable." },
            { "Majora", "Ocarina of Time may be unfinishable." },
        };
        for (const auto& w : kGoalWarnings) {
            const size_t at = tooltip.find(std::string("\n") + w.label + ": ");
            const size_t end = at == std::string::npos ? std::string::npos : tooltip.find('\n', at + 1);
            const std::string line = at == std::string::npos ? "" : tooltip.substr(at + 1, end - at - 1);
            ROWS_CHECK(line.find(w.warns) != std::string::npos,
                       "the goal row's '%s' line does not say '%s' (ADR 0010 section 1.2: documented at the setting)",
                       w.label, w.warns);
        }
        ROWS_CHECK(tooltip.find("The paired game ends when the goal is met") != std::string::npos,
                   "the goal row's tooltip must say that the paired game ends when the goal is met (#762)");
        ROWS_CHECK(Combo_ComboSettingDefault(COMBO_SETTING_GOAL) == (int32_t)RSBS_COMBO_GOAL_BEAT_BOTH,
                   "the goal's shipped default is %d, expected beat-both (OoTMM's default is 'both')",
                   (int)Combo_ComboSettingDefault(COMBO_SETTING_GOAL));
        printf("[TEST] leg 1b: the goal row offers the five pinned goals in OoTMM's words, default Ganon & Majora\n");
    }

    // ---- Leg 1b2: the logic row offers exactly OoTMM's three, in its words ----
    // #1367 (operator ruling 2026-11-02): OoTMM's `logic` values on the pinned
    // rungs (All Locations 3, Beatable Only 2, No Logic 1), each with a "Value:
    // effect" tooltip line, the No Logic one saying the seed may be unbeatable,
    // and the default All Locations, OoTMM's (#1367 R5; Beatable Only before it).
    {
        WidgetInfo* logicRow = settingRow[COMBO_SETTING_LOGIC];
        auto logicOptions = std::static_pointer_cast<UIWidgets::ComboboxOptions>(logicRow->options);
        const struct {
            uint8_t value;
            const char* label;
        } kLogicLabels[] = {
            { (uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE, "All Locations" },
            { (uint8_t)RSBS_COMBO_RUNG_BEATABLE, "Beatable Only" },
            { (uint8_t)RSBS_COMBO_RUNG_NONE, "No Logic" },
        };
        ROWS_CHECK(logicOptions->comboMap.size() == 3,
                   "the logic row offers %zu values, expected OoTMM's three (All Locations, Beatable Only, No Logic)",
                   logicOptions->comboMap.size());
        const std::string tooltip = logicOptions->tooltip != nullptr ? logicOptions->tooltip : "";
        for (const auto& l : kLogicLabels) {
            ROWS_CHECK(Combo_ComboSettingValueValid(COMBO_SETTING_LOGIC, (int32_t)l.value),
                       "rung %u is offered by the row but refused by the model's value space", (unsigned)l.value);
            ROWS_CHECK(logicOptions->comboMap.contains((int32_t)l.value) &&
                           std::string(logicOptions->comboMap.at((int32_t)l.value)) == l.label,
                       "rung %u is not labelled '%s' (OoTMM's name for it)", (unsigned)l.value, l.label);
            ROWS_CHECK(tooltip.find(std::string("\n") + l.label + ": ") != std::string::npos,
                       "the logic row's tooltip has no '%s: effect' line", l.label);
        }
        ROWS_CHECK(tooltip.find("No Logic: Items are placed without any check. The seed may be unbeatable.") !=
                       std::string::npos,
                   "the logic row's No Logic line does not say the seed may be unbeatable");
        ROWS_CHECK(tooltip.find("All Locations: The goal can be met, and every location in both games can be "
                                "reached.") != std::string::npos,
                   "the logic row's All Locations line does not say every location in both games is reached");
        ROWS_CHECK(Combo_ComboSettingDefault(COMBO_SETTING_LOGIC) == (int32_t)RSBS_COMBO_RUNG_ALL_REACHABLE,
                   "the logic's shipped default is %d, expected All Locations (OoTMM's default, #1367 R5)",
                   (int)Combo_ComboSettingDefault(COMBO_SETTING_LOGIC));
        printf("[TEST] leg 1b2: the logic row offers OoTMM's three values in its words, default All Locations\n");
    }

    // ---- Leg 1c: the two "Max Items" rows are retired (#801) ----------------
    // Under the single bag no rule reads a pool size: how many items cross is an
    // outcome of the fill, not a setting, and moving either slider only re-seeded
    // the world (ADR 0011's 2026-09-27 note). Operator ruling 2026-09-30: the
    // rows go. Matched by LABEL TEXT rather than by id, so the check does not
    // depend on the ids the fix removes. Every slider counts: since #1389 the
    // page's only sliders are the Triforce Hunt's two, under Win Condition.
    {
        int sliders = 0;
        int triforceSliders = 0;
        std::string section;
        for (PageRow& pageRow : rows) {
            const std::string& name = pageRow.first->name;
            if (pageRow.first->type == WIDGET_SEPARATOR_TEXT) {
                section = name;
            }
            ROWS_CHECK(name.find("Max OoT Items") == std::string::npos &&
                           name.find("Max MM Items") == std::string::npos,
                       "the Cross-Game Rules page still offers the retired row '%s' (#801): it changes no rule and "
                       "only re-seeds the world",
                       name.c_str());
            if (pageRow.first->type != WIDGET_SLIDER_INT) {
                continue;
            }
            sliders++;
            const bool triforce = name == RowName(COMBO_SETTING_TRIFORCE_PIECES) + ": %d" ||
                                  name == RowName(COMBO_SETTING_TRIFORCE_GOAL) + ": %d";
            triforceSliders += triforce ? 1 : 0;
            ROWS_CHECK(triforce && section == "Win Condition",
                       "the slider '%s' under '%s' is not one of the Triforce Hunt's two under Win Condition (#1389)",
                       name.c_str(), section.c_str());
        }
        ROWS_CHECK(sliders == 2 && triforceSliders == 2,
                   "the Cross-Game Rules page has %d slider row(s), %d of them the Triforce Hunt's; expected exactly "
                   "[Both Games] Triforce Pieces and Triforce Goal (#1389) after #801 retired the pool sizes",
                   sliders, triforceSliders);
        printf("[TEST] leg 1c: no \"Max OoT Items\" / \"Max MM Items\" row; the page's sliders are the Triforce "
               "Hunt's two (#801, #1389)\n");
    }

    // ---- Leg 1c2: the Triforce Hunt rows (#1389) ------------------------------
    // OoTMM's triforcePieces / triforceGoal: defaults 30 and 20, 1..255 (OoT's
    // 8-bit counter, OoTMM allows 999), shown only under the Triforce Hunt goal,
    // the goal slider capped at the pieces, and the goal's tooltip no longer says
    // a paired world cannot be created with it.
    {
        WidgetInfo* piecesRow = settingRow[COMBO_SETTING_TRIFORCE_PIECES];
        WidgetInfo* goalCountRow = settingRow[COMBO_SETTING_TRIFORCE_GOAL];
        ROWS_CHECK(Combo_ComboSettingDefault(COMBO_SETTING_TRIFORCE_PIECES) == 30 &&
                       Combo_ComboSettingDefault(COMBO_SETTING_TRIFORCE_GOAL) == 20,
                   "the Triforce Hunt rows' defaults are %d and %d, expected OoTMM's 30 and 20",
                   (int)Combo_ComboSettingDefault(COMBO_SETTING_TRIFORCE_PIECES),
                   (int)Combo_ComboSettingDefault(COMBO_SETTING_TRIFORCE_GOAL));
        ROWS_CHECK(Combo_ComboSettingValueValid(COMBO_SETTING_TRIFORCE_PIECES, 255) &&
                       !Combo_ComboSettingValueValid(COMBO_SETTING_TRIFORCE_PIECES, 256) &&
                       !Combo_ComboSettingValueValid(COMBO_SETTING_TRIFORCE_PIECES, 0) &&
                       Combo_ComboSettingValueValid(COMBO_SETTING_TRIFORCE_GOAL, 1) &&
                       !Combo_ComboSettingValueValid(COMBO_SETTING_TRIFORCE_GOAL, 0),
                   "the Triforce Hunt rows' value space is not 1..255");
        auto goalTooltipRow =
            std::static_pointer_cast<UIWidgets::ComboboxOptions>(settingRow[COMBO_SETTING_GOAL]->options);
        const std::string goalTooltip = goalTooltipRow->tooltip != nullptr ? goalTooltipRow->tooltip : "";
        ROWS_CHECK(goalTooltip.find("cannot be created") == std::string::npos,
                   "the goal row's tooltip still says a paired world cannot be created with Triforce Hunt (#1389)");
        // Hidden under the shipped goal, shown under the hunt.
        piecesRow->preFunc(*piecesRow);
        ROWS_CHECK(piecesRow->isHidden, "the Triforce Pieces row is shown under the Ganon & Majora goal");
        ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_GOAL, (int32_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_TRIFORCE_PIECES, 12) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_TRIFORCE_GOAL, 40) == 1,
                   "the writers refused a pre-creation Triforce Hunt");
        piecesRow->preFunc(*piecesRow);
        goalCountRow->preFunc(*goalCountRow);
        auto goalCountOptions = std::static_pointer_cast<UIWidgets::IntSliderOptions>(goalCountRow->options);
        ROWS_CHECK(!piecesRow->isHidden && !goalCountRow->isHidden,
                   "the Triforce Hunt rows are hidden under the Triforce Hunt goal");
        ROWS_CHECK(StagedInt(*piecesRow) == 12 && StagedInt(*goalCountRow) == 12 && goalCountOptions->max == 12,
                   "the rows show %d of %d (slider max %d); expected the goal clamped to the 12 pieces, as OoTMM's",
                   StagedInt(*goalCountRow), StagedInt(*piecesRow), (int)goalCountOptions->max);
        Combo_ComboSettingClear(COMBO_SETTING_TRIFORCE_GOAL);
        Combo_ComboSettingClear(COMBO_SETTING_TRIFORCE_PIECES);
        Combo_ComboSettingClear(COMBO_SETTING_GOAL);
        piecesRow->preFunc(*piecesRow);
        goalCountRow->preFunc(*goalCountRow);
        printf("[TEST] leg 1c2: the Triforce Hunt rows default to 30 / 20 over 1..255, show only under the hunt "
               "goal, and cap the goal at the pieces (#1389)\n");
    }

    // ---- Leg 1d: the "OoT Classes" / "MM Classes" rows are retired (#834) ---
    // Under the single bag every bag row is progression by construction, so no
    // rule reads the other class bits and the PROGRESSION bit only repeats the
    // direction; ticking a box only re-seeded the world. Operator ruling
    // 2026-10-01: the rows go, with their twelve checkboxes and the two "No ...
    // items will cross." notes. Matched by NAME TEXT, as leg 1c is, so the check
    // does not depend on the ids the fix removes.
    {
        static const char* const kRetiredParts[] = { "OoT Classes", "MM Classes", "##OoTClass", "##MMClass",
                                                     "items will cross" };
        int classRows = 0;
        for (PageRow& pageRow : rows) {
            const std::string& name = pageRow.first->name;
            bool retired = false;
            for (const char* part : kRetiredParts) {
                retired = retired || name.find(part) != std::string::npos;
            }
            ROWS_CHECK(!retired,
                       "the Cross-Game Rules page still offers the retired item-class row '%s' (#834): it changes "
                       "no rule and only re-seeds the world",
                       name.c_str());
            classRows += retired ? 1 : 0;
        }
        printf("[TEST] leg 1d: %d \"OoT Classes\" / \"MM Classes\" row(s) on the page, expected none (#834)\n",
               classRows);
    }

    // ---- Leg 1e: the "Shared Ocarina" row is retired (#879) -----------------
    // MM starts every paired world holding its Ocarina of Time, so the rule's
    // only effect was to carry that free instrument back to Hyrule. Operator
    // ruling 2026-10-09: the row, its key and its effect go. Matched by NAME
    // TEXT, as legs 1c and 1d are, so the check does not depend on the id the
    // fix removes. The page's sixteen checkboxes are #886's Shared Kokiri Sword,
    // #1068's Shared Shields, #1065's Shared Lens of Truth, #1066's two shared
    // songs, #1067's Shared Ocarina Buttons, #1069's Shared Song of Time,
    // #968's three shared arrows, #970's Shared Bottles and #978's five shared
    // masks. #1067's "Shared Ocarina Buttons" is a different rule
    // (the five ocarina buttons, not the instrument), so its name is not the
    // retired row's: the match excludes it explicitly.
    {
        int ocarinaRows = 0;
        int checkboxes = 0;
        for (PageRow& pageRow : rows) {
            const std::string& name = pageRow.first->name;
            const bool retired = name.find("Shared Ocarina") != std::string::npos &&
                                 name.find("Shared Ocarina Buttons") == std::string::npos;
            ROWS_CHECK(!retired,
                       "the Cross-Game Rules page still offers the retired row '%s' (#879): MM already starts with "
                       "its ocarina, so the rule only hands OoT a free one",
                       name.c_str());
            ocarinaRows += retired ? 1 : 0;
            checkboxes += pageRow.first->type == WIDGET_CHECKBOX ? 1 : 0;
        }
        ROWS_CHECK(checkboxes == 21,
                   "the Cross-Game Rules page has %d checkbox row(s); the twenty-one expected are the Shared Kokiri "
                   "Sword (#886), the Shared Shields (#1068), the Shared Lens of Truth (#1065), Shared Epona's Song "
                   "and Shared Song of Storms (#1066), the Shared Ocarina Buttons (#1067), the Shared Song of Time "
                   "(#1069), Shared Fire, Ice and Light Arrows (#968), the Shared Bottles (#970), the Shared Goron, "
                   "Zora, Keaton and Truth masks and Bunny Hood (#978), the Shared Swim (#1369) and the Blast and "
                   "Stone Masks (OoT) and their shared variants (#979), after #879 retired the Shared Ocarina",
                   checkboxes);
        printf("[TEST] leg 1e: %d \"Shared Ocarina\" row(s) and %d checkbox(es) on the page, expected 0 and 21 "
               "(#879, #886, #1068, #1065, #1066, #1067, #1069, #968, #970, #978, #1369, #979)\n",
               ocarinaRows, checkboxes);
    }

    // ---- Leg 1f: OoTMM's sections, in OoTMM's order (#1101, option C) -------
    // The page reads as OoTMM's Settings do: "Win Condition" over the Goal,
    // "Item Crossing" over the Crossing Direction, then OoTMM's own section name,
    // "Shared Items", over every Shared <Item> rule, and the rules in the order
    // OoTMM's settings list them (packages/core/src/settings/data.ts). The ranks
    // are by LABEL, spelled here, and cover the rules in flight too: a name the
    // page does not hold yet is skipped, so each rule lands in its place as it
    // merges and this leg pins it there.
    {
        static const char* const kOotmmSharedOrder[] = {
            "Shared Fire Arrows",
            "Shared Ice Arrows",
            "Shared Light Arrows",
            "Shared Epona's Song",
            "Shared Song of Storms",
            "Shared Song of Time",
            "Shared Lens of Truth",
            "Shared Goron Mask",
            "Shared Zora Mask",
            "Shared Bunny Hood",
            "Shared Keaton Mask",
            "Shared Mask of Truth",
            "Shared Kokiri Sword",
            "Shared Shields",
            "Shared Ocarina Buttons",
            "Shared Bottles",
            // #1369: OoTMM's sharedScales follows its button and bottle rules.
            "Shared Swim",
            // #979: OoTMM's next section, Cross-Game Masks, in its data.ts order: the
            // masks added to OoT (blastMaskOot, stoneMaskOot; kamaroMaskOot follows
            // when P4 adds it), then their shared variants in the same order
            // (sharedMaskBlast, sharedMaskStone, then sharedMaskKamaro).
            "Blast Mask (OoT)",
            "Stone Mask (OoT)",
            "Shared Blast Mask",
            "Shared Stone Mask",
        };
        // The first rank of OoTMM's Cross-Game Masks section (#979); every rank
        // before it is a Shared Items rule.
        const int kFirstCrossGameMaskRank = 17;
        const int ranks = (int)(sizeof(kOotmmSharedOrder) / sizeof(kOotmmSharedOrder[0]));
        const std::string marker = std::string(Combo_ComboSettingSharedMarker()) + " ";
        auto rankOf = [&](const std::string& name) {
            for (int r = 0; r < ranks; r++) {
                if (name == marker + kOotmmSharedOrder[r]) {
                    return r;
                }
            }
            return -1;
        };
        std::vector<std::string> separators;
        std::string section;
        int lastRank = -1;
        std::string lastName;
        int sharedRows = 0;
        for (PageRow& pageRow : rows) {
            const WidgetInfo& row = *pageRow.first;
            if (row.type == WIDGET_SEPARATOR_TEXT) {
                separators.push_back(row.name);
                section = row.name;
                continue;
            }
            if (row.name == RowName(COMBO_SETTING_GOAL)) {
                ROWS_CHECK(section == "Win Condition", "the Goal row sits under '%s', expected \"Win Condition\"",
                           section.c_str());
            } else if (row.name == RowName(COMBO_SETTING_LOGIC)) {
                ROWS_CHECK(section == "Logic", "the Logic row sits under '%s', expected \"Logic\" (#1367)",
                           section.c_str());
            } else if (row.name == RowName(COMBO_SETTING_DIRECTION)) {
                ROWS_CHECK(section == "Item Crossing",
                           "the Crossing Direction row sits under '%s', expected \"Item Crossing\"", section.c_str());
            } else if (row.type == WIDGET_CHECKBOX) {
                sharedRows++;
                const int rank = rankOf(row.name);
                const char* const wantSection = rank >= kFirstCrossGameMaskRank ? "Cross-Game Masks" : "Shared Items";
                ROWS_CHECK(section == wantSection, "the rule '%s' sits under '%s', expected \"%s\"", row.name.c_str(),
                           section.c_str(), wantSection);
                ROWS_CHECK(rank >= 0, "the rule '%s' has no place in OoTMM's order; add it to this leg's table",
                           row.name.c_str());
                ROWS_CHECK(rank > lastRank, "the rule '%s' comes after '%s'; OoTMM lists it first", row.name.c_str(),
                           lastName.c_str());
                if (rank > lastRank) {
                    lastRank = rank;
                    lastName = row.name;
                }
            }
        }
        const std::vector<std::string> kSections = { "Win Condition", "Logic", "Item Crossing", "Shared Items",
                                                     "Cross-Game Masks" };
        std::string got;
        for (const std::string& s : separators) {
            got += (got.empty() ? "" : ", ") + s;
        }
        ROWS_CHECK(separators == kSections,
                   "the page's sections read [%s]; expected Win Condition, Logic, Item Crossing, Shared Items, "
                   "Cross-Game Masks",
                   got.c_str());
        printf("[TEST] leg 1f: the page's sections are [%s] and its %d rule checkbox(es) sit under Shared Items and "
               "Cross-Game Masks in OoTMM's order\n",
               got.c_str(), sharedRows);
    }

    // ---- Leg 2: no row is its own writer, and no pop-out is offered ---------
    // The enforcement rule (ADR 0004 §6): the gate is on the src/common writers,
    // so no widget in this section may bind one of the three keys directly -- a
    // WIDGET_CVAR_* row would write the store itself and never reach the freeze
    // check.
    for (PageRow& pageRow : rows) {
        WidgetInfo& row = *pageRow.first;
        if (row.cVar != nullptr) {
            for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
                ROWS_CHECK(std::string(row.cVar) != std::string(Combo_ComboSettingKey((ComboSettingId)i)),
                           "row '%s' binds the tier-4 key '%s' directly; a CVar-typed widget is its own writer and "
                           "bypasses Combo_ComboSettingSet's freeze gate",
                           row.name.c_str(), row.cVar);
            }
        }
    }
    // Over the WHOLE section, not just the rules page: the window buttons live on
    // the sibling Windows page since #497 step 6, so a check confined
    // to the rules page could never see the row it is looking for.
    for (PageRow& pageRow : sectionRows) {
        WidgetInfo& row = *pageRow.first;
        ROWS_CHECK(!(row.type == WIDGET_WINDOW_BUTTON && row.windowName != nullptr &&
                     std::string(row.windowName) == "Combo Settings"),
                   "the Combo section still offers a pop-out for the combo settings ('%s'); #655 replaced it with "
                   "the rows above",
                   row.name.c_str());
    }
    printf("[TEST] leg 2: no row binds a gCombo.Rando.* key directly, no combo-settings pop-out row\n");

    // ---- Leg 3: pre-creation the rows are editable and show the resolver ----
    ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_FORWARD) == 1,
               "the writer refused a pre-creation direction");
    ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_GOAL, (int32_t)RSBS_COMBO_GOAL_BEAT_OOT) == 1,
               "the writer refused a pre-creation goal");
    ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_LOGIC, (int32_t)RSBS_COMBO_RUNG_ALL_REACHABLE) == 1,
               "the writer refused a pre-creation [Both Games] Logic (#1367)");
    ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_SHARED_KOKIRI_SWORD, 1) == 1,
               "the writer refused a pre-creation Shared Kokiri Sword");
    ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_SHARED_SHIELDS, 1) == 1,
               "the writer refused a pre-creation Shared Shields");
    ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_SHARED_OCARINA_BUTTONS, 1) == 1,
               "the writer refused a pre-creation Shared Ocarina Buttons");
    ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_SHARED_BOTTLES, 1) == 1,
               "the writer refused a pre-creation Shared Bottles");
    // #979: the Shared Blast Mask's row is greyed while the Blast Mask (OoT) is off
    // (OoTMM's cond), with the extension's reason, and shows off, the value the
    // resolver freezes without the extension, even with its own key authored on.
    // The key is cleared after, so the later legs see the shared variant unauthored.
    if (settingRow[COMBO_SETTING_SHARED_MASK_BLAST] != nullptr) {
        WidgetInfo& sharedBlastRow = *settingRow[COMBO_SETTING_SHARED_MASK_BLAST];
        ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_SHARED_MASK_BLAST, 1) == 1,
                   "the writer refused a pre-creation Shared Blast Mask");
        RunPreFunc(sharedBlastRow);
        const std::string tip =
            sharedBlastRow.options->disabledTooltip != nullptr ? sharedBlastRow.options->disabledTooltip : "";
        ROWS_CHECK(sharedBlastRow.options->disabled && tip.find("Blast Mask (OoT) Is Off") != std::string::npos,
                   "the Shared Blast Mask row must be greyed with the reason \"Blast Mask (OoT) Is Off\" while its "
                   "extension is off (disabled=%d, tooltip '%s')",
                   sharedBlastRow.options->disabled ? 1 : 0, tip.c_str());
        ROWS_CHECK(StagedBool(sharedBlastRow) == 0,
                   "the Shared Blast Mask row staged %d with the Blast Mask (OoT) off; the resolver freezes it off, "
                   "so the row must show off",
                   StagedBool(sharedBlastRow));
        Combo_ComboSettingClear(COMBO_SETTING_SHARED_MASK_BLAST);
        printf("[TEST] leg 3a: with the Blast Mask (OoT) off the Shared Blast Mask row is greyed with the "
               "extension's reason and shows off (#979)\n");
    }
    // #979: the Shared Stone Mask's row, the same way, with its own extension's
    // reason.
    if (settingRow[COMBO_SETTING_SHARED_MASK_STONE] != nullptr) {
        WidgetInfo& sharedStoneRow = *settingRow[COMBO_SETTING_SHARED_MASK_STONE];
        ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_SHARED_MASK_STONE, 1) == 1,
                   "the writer refused a pre-creation Shared Stone Mask");
        RunPreFunc(sharedStoneRow);
        const std::string tip =
            sharedStoneRow.options->disabledTooltip != nullptr ? sharedStoneRow.options->disabledTooltip : "";
        ROWS_CHECK(sharedStoneRow.options->disabled && tip.find("Stone Mask (OoT) Is Off") != std::string::npos,
                   "the Shared Stone Mask row must be greyed with the reason \"Stone Mask (OoT) Is Off\" while its "
                   "extension is off (disabled=%d, tooltip '%s')",
                   sharedStoneRow.options->disabled ? 1 : 0, tip.c_str());
        ROWS_CHECK(StagedBool(sharedStoneRow) == 0,
                   "the Shared Stone Mask row staged %d with the Stone Mask (OoT) off; the resolver freezes it off, "
                   "so the row must show off",
                   StagedBool(sharedStoneRow));
        Combo_ComboSettingClear(COMBO_SETTING_SHARED_MASK_STONE);
        printf("[TEST] leg 3a: with the Stone Mask (OoT) off the Shared Stone Mask row is greyed with the "
               "extension's reason and shows off (#979)\n");
    }
    // Then the extensions are authored ON for every row to be editable.
    ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_BLAST_MASK_OOT, 1) == 1,
               "the writer refused a pre-creation Blast Mask (OoT)");
    ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_STONE_MASK_OOT, 1) == 1,
               "the writer refused a pre-creation Stone Mask (OoT)");
    // #1065: the lens is authored ON through its own row's Callback (the row's
    // only write path), so the PreFunc below must stage what the writer kept.
    WidgetInfo* lensRow = FindRow(rows, LensRowName());
    ROWS_CHECK(lensRow != nullptr && lensRow->type == WIDGET_CHECKBOX && lensRow->callback != nullptr,
               "no Cross-Game Rules checkbox named '%s' with a Callback (#1065)", LensRowName().c_str());
    if (lensRow != nullptr && (lensRow->type != WIDGET_CHECKBOX || lensRow->callback == nullptr)) {
        lensRow = nullptr;
    }
    if (lensRow != nullptr) {
        *std::get<bool*>(lensRow->valuePointer) = true;
        lensRow->callback(*lensRow);
    }

    RunAllPreFuncs(rows);
    ROWS_CHECK(Combo_ComboSettingReadOnlyReason() == nullptr, "the model reports a read-only reason before creation");
    for (const ExpectedRow& expected : kExpected) {
        WidgetInfo* row = settingRow[expected.id];
        if (row != nullptr) {
            ROWS_CHECK(!row->options->disabled, "row for '%s' is disabled before the creation event",
                       Combo_ComboSettingLabel(expected.id));
        }
    }
    ROWS_CHECK(StagedInt(*settingRow[COMBO_SETTING_DIRECTION]) == (int32_t)RSBS_COMBO_DIR_FORWARD,
               "the direction row staged %d, expected the authored %d", StagedInt(*settingRow[COMBO_SETTING_DIRECTION]),
               (int)RSBS_COMBO_DIR_FORWARD);
    ROWS_CHECK(StagedInt(*settingRow[COMBO_SETTING_GOAL]) == (int32_t)RSBS_COMBO_GOAL_BEAT_OOT,
               "the goal row staged %d, expected the authored %d (Ganon)", StagedInt(*settingRow[COMBO_SETTING_GOAL]),
               (int)RSBS_COMBO_GOAL_BEAT_OOT);
    ROWS_CHECK(StagedInt(*settingRow[COMBO_SETTING_LOGIC]) == (int32_t)RSBS_COMBO_RUNG_ALL_REACHABLE,
               "the logic row staged %d, expected the authored %d (All Locations)",
               StagedInt(*settingRow[COMBO_SETTING_LOGIC]), (int)RSBS_COMBO_RUNG_ALL_REACHABLE);
    ROWS_CHECK(StagedBool(*settingRow[COMBO_SETTING_SHARED_KOKIRI_SWORD]) == 1,
               "the Shared Kokiri Sword row staged %d, expected the authored 1",
               StagedBool(*settingRow[COMBO_SETTING_SHARED_KOKIRI_SWORD]));
    ROWS_CHECK(StagedBool(*settingRow[COMBO_SETTING_SHARED_SHIELDS]) == 1,
               "the Shared Shields row staged %d, expected the authored 1",
               StagedBool(*settingRow[COMBO_SETTING_SHARED_SHIELDS]));
    ROWS_CHECK(StagedBool(*settingRow[COMBO_SETTING_SHARED_OCARINA_BUTTONS]) == 1,
               "the Shared Ocarina Buttons row staged %d, expected the authored 1",
               StagedBool(*settingRow[COMBO_SETTING_SHARED_OCARINA_BUTTONS]));
    ROWS_CHECK(StagedBool(*settingRow[COMBO_SETTING_SHARED_BOTTLES]) == 1,
               "the Shared Bottles row staged %d, expected the authored 1",
               StagedBool(*settingRow[COMBO_SETTING_SHARED_BOTTLES]));
    // #1369: the Shared Swim is never written here, and still stages ON: the one
    // rule that ships on shows its key's default before a world exists, though the
    // resolver never sets its shuffle-gated bit.
    ROWS_CHECK(StagedBool(*settingRow[COMBO_SETTING_SHARED_SWIM]) == 1,
               "the Shared Swim row staged %d before creation, expected the shipped ON (#1369)",
               StagedBool(*settingRow[COMBO_SETTING_SHARED_SWIM]));
    if (lensRow != nullptr) {
        ROWS_CHECK(!lensRow->options->disabled, "the Shared Lens of Truth row is disabled before the creation event");
        ROWS_CHECK(StagedBool(*lensRow) == 1,
                   "the Shared Lens of Truth row staged %d after its own Callback authored ON; the edit did not reach "
                   "the resolver",
                   StagedBool(*lensRow));
    }
    printf("[TEST] leg 3: before the creation event every row is editable and stages the resolver's value\n");

    // ---- Leg 4: a pre-creation edit reaches the store through the model -----
    // The Callback is the row's ONLY write path. Editing the staging buffer the
    // way the widget would and firing it must land in the store; if a future row
    // were rewired to a CVar widget this leg would still pass, which is why leg
    // 2 exists as well. Driven on the direction row (the pool-size row it used
    // was retired by #801), there and back, so the later legs still see the
    // authored FORWARD.
    {
        WidgetInfo& dirRow = *settingRow[COMBO_SETTING_DIRECTION];
        ROWS_CHECK(dirRow.callback != nullptr, "the direction row has no Callback");
        const int32_t kThere[2] = { (int32_t)RSBS_COMBO_DIR_REVERSE, (int32_t)RSBS_COMBO_DIR_FORWARD };
        for (const int32_t value : kThere) {
            *std::get<int32_t*>(dirRow.valuePointer) = value;
            if (dirRow.callback != nullptr) {
                dirRow.callback(dirRow);
            }
            int32_t stored = 0;
            ROWS_CHECK(Combo_ComboSettingReadStore(COMBO_SETTING_DIRECTION, &stored) && stored == value,
                       "a pre-creation edit to %d did not reach the store (read back %d)", (int)value, stored);
        }
    }

    // ---- Leg 5: frozen -> read-only, with the values FROM THE SAVE ----------
    // A PAIRED world first. Combo_ComboSettingsSummary refuses to present
    // gComboCtx's zeros as rules for a world that does not exist
    // (foreign_items.c:570), so a frozen-but-unpaired record reads as an absent
    // one -- which is the right answer, and is what leg 8 checks the status line
    // says out loud. Here the pairing is real so the record is the record.
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0xC0FFEE55u;
    gComboCtx.sharedRandoSettingsHash = 0x5EED0055u;
    gComboCtx.mmProfileDigest = 0x4D4D0055u;

    // The frozen record differs from the authored CVars in EVERY field, so a row
    // that showed the CVar would be red rather than accidentally right.
    ComboSettingsRecord frozen;
    Combo_ComboSettingsDefaults(&frozen);
    frozen.direction = (uint8_t)RSBS_COMBO_DIR_REVERSE;   // authored: FORWARD
    frozen.goal = (uint8_t)RSBS_COMBO_GOAL_BEAT_MM;       // authored: BEAT_OOT
    frozen.logicRung = (uint8_t)RSBS_COMBO_RUNG_BEATABLE; // authored: ALL_REACHABLE (a world of the old default)
    frozen.comboFlags = 0u;                               // authored: Shared Kokiri Sword ON (#886)
    Combo_FreezeComboSettings(&frozen);
    ROWS_CHECK(Combo_ComboSettingsFrozen(), "Combo_FreezeComboSettings left the record unfrozen");
    ROWS_CHECK(Combo_ComboSettingReadOnlyReason() != nullptr, "the model reports no read-only reason once frozen");

    RunAllPreFuncs(rows);
    ExpectDecided(*settingRow[COMBO_SETTING_DIRECTION], "the direction row");
    ROWS_CHECK(StagedInt(*settingRow[COMBO_SETTING_DIRECTION]) == (int32_t)RSBS_COMBO_DIR_REVERSE,
               "the frozen direction row staged %d; it must show the SAVE's %d, not the CVar's %d",
               StagedInt(*settingRow[COMBO_SETTING_DIRECTION]), (int)RSBS_COMBO_DIR_REVERSE,
               (int)RSBS_COMBO_DIR_FORWARD);
    ExpectDecided(*settingRow[COMBO_SETTING_GOAL], "the goal row");
    ROWS_CHECK(StagedInt(*settingRow[COMBO_SETTING_GOAL]) == (int32_t)RSBS_COMBO_GOAL_BEAT_MM,
               "the frozen goal row staged %d; it must show the SAVE's %d (Majora), not the CVar's %d (Ganon) -- the "
               "world was proved against the frozen goal",
               StagedInt(*settingRow[COMBO_SETTING_GOAL]), (int)RSBS_COMBO_GOAL_BEAT_MM, (int)RSBS_COMBO_GOAL_BEAT_OOT);
    // #1367: the frozen record's rung (Beatable Only, the default before #1367 R5)
    // against the authored All Locations key.
    ExpectDecided(*settingRow[COMBO_SETTING_LOGIC], "the logic row");
    ROWS_CHECK(StagedInt(*settingRow[COMBO_SETTING_LOGIC]) == (int32_t)RSBS_COMBO_RUNG_BEATABLE,
               "the frozen logic row staged %d; it must show the SAVE's %d (Beatable Only), not the CVar's %d",
               StagedInt(*settingRow[COMBO_SETTING_LOGIC]), (int)RSBS_COMBO_RUNG_BEATABLE,
               (int)RSBS_COMBO_RUNG_ALL_REACHABLE);
    ExpectDecided(*settingRow[COMBO_SETTING_SHARED_KOKIRI_SWORD], "the Shared Kokiri Sword row");
    ROWS_CHECK(StagedBool(*settingRow[COMBO_SETTING_SHARED_KOKIRI_SWORD]) == 0,
               "the frozen Shared Kokiri Sword row staged %d; it must show the SAVE's OFF, not the CVar's ON -- the "
               "world was built with two swords and a row that showed otherwise would be lying about it",
               StagedBool(*settingRow[COMBO_SETTING_SHARED_KOKIRI_SWORD]));
    // #1068: the frozen ITEM RULES record (every rule off here) against the
    // authored ON key.
    ExpectDecided(*settingRow[COMBO_SETTING_SHARED_SHIELDS], "the Shared Shields row");
    ROWS_CHECK(StagedBool(*settingRow[COMBO_SETTING_SHARED_SHIELDS]) == 0,
               "the frozen Shared Shields row staged %d; it must show the SAVE's OFF, not the CVar's ON",
               StagedBool(*settingRow[COMBO_SETTING_SHARED_SHIELDS]));
    ExpectDecided(*settingRow[COMBO_SETTING_SHARED_OCARINA_BUTTONS], "the Shared Ocarina Buttons row");
    ROWS_CHECK(StagedBool(*settingRow[COMBO_SETTING_SHARED_OCARINA_BUTTONS]) == 0,
               "the frozen Shared Ocarina Buttons row staged %d; it must show the SAVE's OFF, not the CVar's ON",
               StagedBool(*settingRow[COMBO_SETTING_SHARED_OCARINA_BUTTONS]));
    ExpectDecided(*settingRow[COMBO_SETTING_SHARED_BOTTLES], "the Shared Bottles row");
    ROWS_CHECK(StagedBool(*settingRow[COMBO_SETTING_SHARED_BOTTLES]) == 0,
               "the frozen Shared Bottles row staged %d; it must show the SAVE's OFF, not the CVar's ON",
               StagedBool(*settingRow[COMBO_SETTING_SHARED_BOTTLES]));
    // #1369: a world nobody shuffled swim in froze the Shared Swim bit OFF; the row
    // shows that, not the key's shipped ON.
    ExpectDecided(*settingRow[COMBO_SETTING_SHARED_SWIM], "the Shared Swim row");
    ROWS_CHECK(StagedBool(*settingRow[COMBO_SETTING_SHARED_SWIM]) == 0,
               "the frozen Shared Swim row staged %d; it must show the SAVE's OFF, not the key's ON",
               StagedBool(*settingRow[COMBO_SETTING_SHARED_SWIM]));
    // #979: the Blast Mask's three rows show the frozen item rules (every rule off,
    // the Default cooldown), not the authored Blast Mask (OoT) ON; the Shared Blast
    // Mask row carries the decided reason, not its extension's.
    ExpectDecided(*settingRow[COMBO_SETTING_BLAST_MASK_OOT], "the Blast Mask (OoT) row");
    ROWS_CHECK(StagedBool(*settingRow[COMBO_SETTING_BLAST_MASK_OOT]) == 0,
               "the frozen Blast Mask (OoT) row staged %d; it must show the SAVE's OFF, not the CVar's ON",
               StagedBool(*settingRow[COMBO_SETTING_BLAST_MASK_OOT]));
    ExpectDecided(*settingRow[COMBO_SETTING_SHARED_MASK_BLAST], "the Shared Blast Mask row");
    ROWS_CHECK(StagedBool(*settingRow[COMBO_SETTING_SHARED_MASK_BLAST]) == 0,
               "the frozen Shared Blast Mask row staged %d; it must show the SAVE's OFF",
               StagedBool(*settingRow[COMBO_SETTING_SHARED_MASK_BLAST]));
    ExpectDecided(*settingRow[COMBO_SETTING_BLAST_MASK_COOLDOWN], "the Blast Mask Cooldown row");
    ROWS_CHECK(StagedInt(*settingRow[COMBO_SETTING_BLAST_MASK_COOLDOWN]) == (int32_t)RSBS_BLAST_COOLDOWN_DEFAULT,
               "the frozen Blast Mask Cooldown row staged %d; it must show the SAVE's %d (Default)",
               StagedInt(*settingRow[COMBO_SETTING_BLAST_MASK_COOLDOWN]), (int)RSBS_BLAST_COOLDOWN_DEFAULT);
    // #979: the Stone Mask's two rows, the same way: the frozen OFF, not the
    // authored Stone Mask (OoT) ON, and the decided reason on the shared one.
    ExpectDecided(*settingRow[COMBO_SETTING_STONE_MASK_OOT], "the Stone Mask (OoT) row");
    ROWS_CHECK(StagedBool(*settingRow[COMBO_SETTING_STONE_MASK_OOT]) == 0,
               "the frozen Stone Mask (OoT) row staged %d; it must show the SAVE's OFF, not the CVar's ON",
               StagedBool(*settingRow[COMBO_SETTING_STONE_MASK_OOT]));
    ExpectDecided(*settingRow[COMBO_SETTING_SHARED_MASK_STONE], "the Shared Stone Mask row");
    ROWS_CHECK(StagedBool(*settingRow[COMBO_SETTING_SHARED_MASK_STONE]) == 0,
               "the frozen Shared Stone Mask row staged %d; it must show the SAVE's OFF",
               StagedBool(*settingRow[COMBO_SETTING_SHARED_MASK_STONE]));
    // #1065: the frozen ITEM RULES record (every rule off: the freeze above
    // passed none) against the authored ON key.
    if (lensRow != nullptr) {
        ExpectDecided(*lensRow, "the Shared Lens of Truth row");
        ROWS_CHECK(StagedBool(*lensRow) == 0,
                   "the frozen Shared Lens of Truth row staged %d; it must show the SAVE's OFF, not the CVar's ON",
                   StagedBool(*lensRow));
    }
    printf("[TEST] leg 5: once frozen every row is read-only with the model's reason and shows the save's record\n");

    // ---- Leg 6: a frozen row's Callback cannot move the store ---------------
    // The whole point of ADR 0004 §6's enforcement rule. The greying above is
    // presentation; this is the gate. Then the next PreFunc must put the staging
    // buffer back, so a refused write cannot leave the row displaying a value no
    // world was built from.
    {
        WidgetInfo& dirRow = *settingRow[COMBO_SETTING_DIRECTION];
        *std::get<int32_t*>(dirRow.valuePointer) = (int32_t)RSBS_COMBO_DIR_OFF;
        if (dirRow.callback != nullptr) {
            dirRow.callback(dirRow);
        }
        int32_t stored = 0;
        ROWS_CHECK(Combo_ComboSettingReadStore(COMBO_SETTING_DIRECTION, &stored) &&
                       stored == (int32_t)RSBS_COMBO_DIR_FORWARD,
                   "a post-creation edit reached the store (read back %d, expected the authored %d untouched)", stored,
                   (int)RSBS_COMBO_DIR_FORWARD);
        RunPreFunc(dirRow);
        ROWS_CHECK(StagedInt(dirRow) == (int32_t)RSBS_COMBO_DIR_REVERSE,
                   "after a refused write the direction row staged %d; the next frame must restore the save's %d",
                   StagedInt(dirRow), (int)RSBS_COMBO_DIR_REVERSE);

        // And the goal: a decided world's goal is what its creation PROVED, so
        // an edit that reached the store would make the next arrival diverge
        // from the stamp (and be refused), and one that reached the record would
        // name a goal nothing proved.
        WidgetInfo& goalRow = *settingRow[COMBO_SETTING_GOAL];
        *std::get<int32_t*>(goalRow.valuePointer) = (int32_t)RSBS_COMBO_GOAL_BEAT_EITHER;
        ROWS_CHECK(goalRow.callback != nullptr, "the goal row has no Callback");
        if (goalRow.callback != nullptr) {
            goalRow.callback(goalRow);
        }
        int32_t storedGoal = 0;
        ROWS_CHECK(Combo_ComboSettingReadStore(COMBO_SETTING_GOAL, &storedGoal) &&
                       storedGoal == (int32_t)RSBS_COMBO_GOAL_BEAT_OOT,
                   "a post-creation goal edit reached the store (read back %d, expected the authored %d untouched)",
                   storedGoal, (int)RSBS_COMBO_GOAL_BEAT_OOT);
        ROWS_CHECK(gComboCtx.comboSettings.goal == (uint8_t)RSBS_COMBO_GOAL_BEAT_MM,
                   "a post-creation goal edit moved the frozen record's goal to %u",
                   (unsigned)gComboCtx.comboSettings.goal);
        RunPreFunc(goalRow);
        ROWS_CHECK(StagedInt(goalRow) == (int32_t)RSBS_COMBO_GOAL_BEAT_MM,
                   "after a refused write the goal row staged %d; the next frame must restore the save's %d",
                   StagedInt(goalRow), (int)RSBS_COMBO_GOAL_BEAT_MM);

        // The same, for the checkbox (#886). Not redundant with the rows above:
        // a checkbox's Callback fires on a TOGGLE, so a row wired to flip the
        // model itself rather than to offer the staged value to the writer
        // would pass every other leg and still let a player turn a decided
        // world's sword rule on from the menu.
        WidgetInfo& swordRow = *settingRow[COMBO_SETTING_SHARED_KOKIRI_SWORD];
        *std::get<bool*>(swordRow.valuePointer) = true;
        ROWS_CHECK(swordRow.callback != nullptr, "the Shared Kokiri Sword row has no Callback");
        if (swordRow.callback != nullptr) {
            swordRow.callback(swordRow);
        }
        ROWS_CHECK(!Combo_ComboSharedKokiriSword(),
                   "a post-creation toggle changed the world's sword rule; the frozen record is the authority");
        RunPreFunc(swordRow);
        ROWS_CHECK(StagedBool(swordRow) == 0,
                   "after a refused toggle the Shared Kokiri Sword row staged %d; the next frame must restore the "
                   "save's OFF",
                   StagedBool(swordRow));

        // And the lens (#1065): a toggle on a decided world's row must move
        // neither the frozen item rules nor, after the next PreFunc, the row.
        if (lensRow != nullptr) {
            *std::get<bool*>(lensRow->valuePointer) = true;
            lensRow->callback(*lensRow);
            ROWS_CHECK(!Combo_ComboCrossItemRule(kLensBit),
                       "a post-creation toggle changed the world's lens rule; the frozen record is the authority");
            RunPreFunc(*lensRow);
            ROWS_CHECK(StagedBool(*lensRow) == 0,
                       "after a refused toggle the Shared Lens of Truth row staged %d; the next frame must restore "
                       "the save's OFF",
                       StagedBool(*lensRow));
        }
    }

    // ---- Leg 7: an unknown direction cannot take the process down ----------
    // UIWidgets::Combobox previews with comboMap.at(*value) and std::map::at
    // THROWS, while Menu::MenuDrawItem catches only std::bad_variant_access. A
    // frozen record may carry a direction this build does not know, because
    // RSBS_COMBO_DIR_* is append-only .redsave format. The row must teach its
    // own map that value -- naming it rather than clamping it to a rule the
    // world was not built from -- and withdraw the entry once the shown value is
    // a pinned one again.
    {
        WidgetInfo& dirRow = *settingRow[COMBO_SETTING_DIRECTION];
        const int32_t future = 9; // no such enumerator in this build
        ComboSettingsRecord fromTheFuture = frozen;
        fromTheFuture.direction = (uint8_t)future;
        Combo_FreezeComboSettings(&fromTheFuture);
        RunPreFunc(dirRow);
        auto options = std::static_pointer_cast<UIWidgets::ComboboxOptions>(dirRow.options);
        ROWS_CHECK(options->comboMap.contains(future),
                   "the direction row did not teach its combo map the unknown value %d; Combobox's comboMap.at() "
                   "would throw out of a draw path that catches only bad_variant_access",
                   future);
        ROWS_CHECK(StagedInt(dirRow) == future,
                   "the direction row clamped the unknown value %d to %d instead of "
                   "showing what the save holds",
                   future, StagedInt(dirRow));

        Combo_FreezeComboSettings(&frozen);
        RunPreFunc(dirRow);
        ROWS_CHECK(!options->comboMap.contains(future),
                   "the direction row kept the taught entry for %d after the shown value became a pinned enumerator",
                   future);
        ROWS_CHECK(options->comboMap.size() == 4,
                   "the direction combo map holds %zu entries, expected the four "
                   "pinned RSBS_COMBO_DIR_* enumerators",
                   options->comboMap.size());

        // The goal row has the same hazard: RSBS_COMBO_GOAL_* is append-only
        // .redsave format too, and a legacy record's goal 0 means "unset".
        WidgetInfo& goalRow = *settingRow[COMBO_SETTING_GOAL];
        auto goalOptions = std::static_pointer_cast<UIWidgets::ComboboxOptions>(goalRow.options);
        for (const int32_t unknownGoal : { (int32_t)9, (int32_t)0 }) {
            ComboSettingsRecord futureGoal = frozen;
            futureGoal.goal = (uint8_t)unknownGoal;
            Combo_FreezeComboSettings(&futureGoal);
            RunPreFunc(goalRow);
            ROWS_CHECK(goalOptions->comboMap.contains(unknownGoal),
                       "the goal row did not teach its combo map the unknown goal %d; comboMap.at() would throw",
                       unknownGoal);
            ROWS_CHECK(StagedInt(goalRow) == unknownGoal,
                       "the goal row clamped the unknown goal %d to %d instead of showing what the save holds",
                       unknownGoal, StagedInt(goalRow));
            ROWS_CHECK(goalOptions->comboMap.size() == 6,
                       "the goal combo map holds %zu entries while showing unknown goal %d; expected the five pinned "
                       "goals plus that one (a taught entry must be withdrawn before the next is taught)",
                       goalOptions->comboMap.size(), unknownGoal);
        }
        Combo_FreezeComboSettings(&frozen);
        RunPreFunc(goalRow);
        ROWS_CHECK(goalOptions->comboMap.size() == 5 && !goalOptions->comboMap.contains(0),
                   "the goal combo map holds %zu entries after the shown goal became a pinned one, expected five",
                   goalOptions->comboMap.size());
    }

    // ---- Leg 8: the status line names each state, without hovering ----------
    // ADR 0004 §6 state 4 wants the reason and the identity LABELLED. A disabled
    // widget's tooltip is not a label (and under a race lockout MenuDrawItem
    // overwrites it), so the status row carries the state in text. Three states,
    // three different things it must say — and the frozen-but-unpaired one is the
    // state in which the record reads as absent and the rows below show zeros,
    // which a player must not read as their world's rules.
    if (statusRow != nullptr) {
        RunPreFunc(*statusRow);
        ROWS_CHECK(statusRow->name.find("Already decided") != std::string::npos,
                   "the frozen status line does not state the reason: '%s'", statusRow->name.c_str());
        char fingerprint[16];
        snprintf(fingerprint, sizeof(fingerprint), "%08X", (unsigned)gComboCtx.comboSettingsHash);
        ROWS_CHECK(statusRow->name.find(fingerprint) != std::string::npos,
                   "the frozen status line does not name the identity (%s) it is frozen to: '%s'", fingerprint,
                   statusRow->name.c_str());

        // Frozen, pairing dropped: a state no created combo file may be in.
        gComboCtx.sourceIsRando = false;
        gComboCtx.sharedRandoSettingsHash = 0;
        RunPreFunc(*statusRow);
        ROWS_CHECK(statusRow->name.find("Session state is corrupt") != std::string::npos,
                   "a frozen record with no live pairing is not named as corrupt: '%s'", statusRow->name.c_str());

        // Paired but not frozen: a legacy pre-carve pair (ADR 0011 decision 4.2).
        // Its first crossing freezes the SHIPPED DEFAULTS and then compares that
        // record against the live CVars (foreign_items.c,
        // Combo_FreezeLegacyComboSettings then Combo_ComboSettingsDivergence), so
        // a rule edited before that crossing is REFUSED there. The note must say
        // to keep the defaults until then; "changes apply to the next world"
        // alone told the player an edit was harmless to this one.
        ComboContext_Init();
        gComboCtx.sourceIsRando = true;
        gComboCtx.sharedRandoSeed = 0xC0FFEE55u;
        gComboCtx.sharedRandoSettingsHash = 0x5EED0055u;
        gComboCtx.mmProfileDigest = 0x4D4D0055u;
        RunPreFunc(*statusRow);
        ROWS_CHECK(statusRow->name.find("predates these rules") != std::string::npos &&
                       statusRow->name.find("Keep them at the defaults until you have crossed into it") !=
                           std::string::npos,
                   "the paired-legacy status line does not tell the player to keep the defaults until the first "
                   "crossing (an edit before it is refused at that crossing): '%s'",
                   statusRow->name.c_str());

        // Unfrozen and unpaired: the ordinary pre-creation state.
        ComboContext_Init();
        RunPreFunc(*statusRow);
        // The one status sentence every randomizer page of ours shares (#1101
        // decision 7): these author the next world, a load shows its own (#781).
        ROWS_CHECK(statusRow->name ==
                       "These apply to the next world you generate. Loading a paired file shows its own.",
                   "the pre-creation status line is not the shared sentence that these author the next world and "
                   "that a load shows a file's own rules (#781): '%s'",
                   statusRow->name.c_str());
    }

    // ---- Leg 9: Reset asks first, and its Reset button clears every rule ------
    // The row's Callback only queues a confirm (SoH's Clear Config shape); the
    // action is the popup's Reset button. The snapshot harness proves the popup
    // is queued and drawn, but dismisses it without running a button, so this
    // leg is the only place the ACTION is checked. Cancel must clear nothing.
    {
        ComboContext_Init();
        ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_FORWARD) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_GOAL, (int32_t)RSBS_COMBO_GOAL_BEAT_MM) == 1 &&
                       // #1389: the Triforce Hunt's two keys, cleared by this leg's Reset.
                       Combo_ComboSettingSet(COMBO_SETTING_TRIFORCE_PIECES, 40) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_TRIFORCE_GOAL, 25) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_KOKIRI_SWORD, 1) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_SHIELDS, 1) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_LENS, 1) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_SONG_EPONA, 1) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_SONG_STORMS, 1) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_OCARINA_BUTTONS, 1) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_SONG_TIME, 1) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_ARROW_FIRE, 1) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_ARROW_ICE, 1) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_ARROW_LIGHT, 1) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_BOTTLES, 1) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_MASK_GORON, 1) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_MASK_ZORA, 1) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_MASK_BUNNY, 1) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_MASK_KEATON, 1) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_MASK_TRUTH, 1) == 1 &&
                       // Shared Swim ships ON, so only its non-default value proves Reset clears it.
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_SWIM, 0) == 1 &&
                       // #979: the Blast Mask's three rows, the cooldown at a non-default value.
                       Combo_ComboSettingSet(COMBO_SETTING_BLAST_MASK_OOT, 1) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_MASK_BLAST, 1) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_BLAST_MASK_COOLDOWN, (int32_t)RSBS_BLAST_COOLDOWN_SHORT) ==
                           1 &&
                       // #979: the Stone Mask's two rows.
                       Combo_ComboSettingSet(COMBO_SETTING_STONE_MASK_OOT, 1) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_MASK_STONE, 1) == 1,
                   "the writer refused a pre-creation value leg 9 needs");
        // #1169 XW2: the fourteen warp-song keys have no row until XW12, but the
        // Reset button clears every key, theirs included.
        for (int i = (int)COMBO_SETTING_SONG_SOARING_OOT; i <= (int)COMBO_SETTING_SHARED_SONG_PRELUDE; i++) {
            ROWS_CHECK(Combo_ComboSettingSet((ComboSettingId)i, 1) == 1,
                       "the writer refused the pre-creation value leg 9 needs for '%s'",
                       Combo_ComboSettingLabel((ComboSettingId)i));
        }
        for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
            ROWS_CHECK(Combo_ComboSettingIsExplicit((ComboSettingId)i),
                       "leg 9 set '%s' but the store holds no explicit value, so the reset check would be vacuous",
                       Combo_ComboSettingLabel((ComboSettingId)i));
        }

        WidgetInfo* resetRow = FindRow(rows, "Reset Combo Rules");
        ROWS_CHECK(resetRow != nullptr && resetRow->type == WIDGET_BUTTON && resetRow->callback != nullptr,
                   "the Cross-Game Rules page has no \"Reset Combo Rules\" button with a Callback");
        auto savedRegistrar = SohGui::gComboRulePopupRegistrar;
        SohGui::gComboRulePopupRegistrar = RecordPopup;
        gRecordedPopups.clear();
        if (resetRow != nullptr && resetRow->callback != nullptr) {
            RunPreFunc(*resetRow);
            ROWS_CHECK(!resetRow->options->disabled, "the Reset row is disabled before the creation event");
            resetRow->callback(*resetRow);
        }
        SohGui::gComboRulePopupRegistrar = savedRegistrar;
        for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
            ROWS_CHECK(Combo_ComboSettingIsExplicit((ComboSettingId)i),
                       "the Reset row cleared '%s' before any confirm was accepted",
                       Combo_ComboSettingLabel((ComboSettingId)i));
        }
        ROWS_CHECK(gRecordedPopups.size() == 1, "the Reset row queued %zu popups, expected one confirm",
                   gRecordedPopups.size());
        if (gRecordedPopups.size() == 1) {
            RecordedPopup& popup = gRecordedPopups.at(0);
            ROWS_CHECK(popup.title == "Reset Combo Rules" && popup.button1 == "Reset" && popup.button2 == "Cancel",
                       "the Reset confirm is '%s' with buttons '%s' / '%s', expected 'Reset Combo Rules' with "
                       "'Reset' / 'Cancel'",
                       popup.title.c_str(), popup.button1.c_str(), popup.button2.c_str());
            if (popup.onButton2 != nullptr) {
                popup.onButton2();
            }
            for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
                ROWS_CHECK(Combo_ComboSettingIsExplicit((ComboSettingId)i), "Cancel cleared '%s'",
                           Combo_ComboSettingLabel((ComboSettingId)i));
            }
            ROWS_CHECK(popup.onButton1 != nullptr, "the Reset confirm's Reset button has no action");
            if (popup.onButton1 != nullptr) {
                popup.onButton1();
            }
            for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
                ROWS_CHECK(!Combo_ComboSettingIsExplicit((ComboSettingId)i),
                           "the Reset confirm's Reset button left '%s' explicitly set",
                           Combo_ComboSettingLabel((ComboSettingId)i));
            }
        }
        gRecordedPopups.clear();
        printf("[TEST] leg 9: Reset queues one confirm; Cancel clears nothing, and its Reset button clears all "
               "three rules\n");
    }

    // Leave the process clean: this row writes the three tier-4 keys and freezes
    // gComboCtx, and AllTests runs every dispatch entry in ONE process.
    ComboContext_Init();
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        Combo_ComboSettingClear((ComboSettingId)i);
    }

    if (gFailures == 0) {
        printf("[TEST] combo-settings-rows: PASS\n");
    } else {
        printf("[TEST] combo-settings-rows: %d failure(s)\n", gFailures);
    }
    return gFailures;
}

#endif // RSBS_SINGLE_EXECUTABLE
