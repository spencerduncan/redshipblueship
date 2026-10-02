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
 * section exists, so this lock follows them: the rows are now
 * `Combo → Cross-Game Rules`, registered by `SohMenu::AddMenuCombo()`. The old
 * page survives with a pointer row, and `MenuComboSection` owns the section's
 * shape (both pages, the pointer row, the extension point); this row still owns
 * the seam between the model and the rows, which is unchanged by the move.
 *
 * HOW IT OBSERVES. It builds a real SohMenu headless and calls the REAL
 * production entry point, `AddMenuCombo()` — not a test-only registrar. That is
 * possible here and was not possible for `AddMenuRandomizer()` (see below):
 * nothing in the Combo section reaches `Rando::Settings`. It is safe with no
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
 * method is running.) `AddMenuCombo()` has no such dependency, which is a
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
// widgets under test are the real ones AddMenuCombo() built.
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

// A checkbox row carries the marker on its own line above the label, so the
// label fits beside the box at min-832x600 (SohMenuCombo.cpp,
// ComboRuleCheckboxRowName).
std::string CheckboxRowName(ComboSettingId id) {
    return std::string(Combo_ComboSettingSharedMarker()) + "\n" + Combo_ComboSettingLabel(id);
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

/** The same, for a checkbox row: the shared ocarina (#668) is a comboFlags BIT,
 *  so its staging buffer is a bool rather than an int32_t. -1 for "no buffer",
 *  which is never a legal staged value. */
int StagedBool(WidgetInfo& row) {
    bool* p = std::get<bool*>(row.valuePointer);
    return p != nullptr ? (*p ? 1 : 0) : -1;
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

    // The REAL production entry point, not a test-only registrar: AddMenuCombo()
    // creates the "Combo" header, both sidebars and every row, and nothing in it
    // reaches Rando::Settings. So the page layout under test is the one a player
    // gets, including which column each row lands in -- the scope the interim
    // host's lock had to give up.
    ComboRulesMenuProbe probe;
    probe.AddMenuCombo();

    // ---- Leg 1: the section and the three rows exist --------------------------
    auto& entries = probe.Entries();
    if (!entries.contains("Combo")) {
        printf("[TEST] FAIL(1): AddMenuCombo registered no \"Combo\" menu entry -- ADR 0004 §4's tier-4 section "
               "(#497 step 6) is gone or was renamed\n");
        return 1;
    }
    auto& sidebars = entries.at("Combo").sidebars;
    if (!sidebars.contains("Cross-Game Rules")) {
        printf("[TEST] FAIL(1): the \"Combo\" menu has no \"Cross-Game Rules\" sidebar page -- the host for the "
               "tier-4 combo rules (#509/#655, moved by #497 step 6) is gone or was renamed\n");
        return 1;
    }
    const SidebarEntry& page = sidebars.at("Cross-Game Rules");
    auto& columns = sidebars.at("Cross-Game Rules").columnWidgets;
    if (columns.empty()) {
        printf("[TEST] FAIL(1): the Cross-Game Rules page has no widget columns\n");
        return 1;
    }
    std::vector<PageRow> rows = FlattenPage(columns);

    // Leg 2 asserts a property of the WHOLE section (no pop-out for the three keys
    // survives anywhere in it), because the window rows moved to a sibling page:
    // checking only the rules page would make that assertion vacuous.
    std::vector<PageRow> sectionRows = rows;
    // "Windows" was "Cross-Game Windows" until 2026-09-27 (UI parity: the old
    // name was wider than the sidebar); MenuComboSection pins the rename.
    if (sidebars.contains("Windows")) {
        for (PageRow& windowRow : FlattenPage(sidebars.at("Windows").columnWidgets)) {
            sectionRows.push_back(windowRow);
        }
    } else {
        printf("[TEST] FAIL: the \"Combo\" menu has no \"Windows\" sidebar page; leg 2's no-pop-out "
               "assertion would be vacuous\n");
        gFailures++;
    }

    struct ExpectedRow {
        ComboSettingId id;
        WidgetType type;
        const char* suffix; // "": the bare name; ": %d": the slider's value format
    };
    // The direction is a combobox over the four pinned enumerators and the
    // shared ocarina (#668) a plain checkbox over the model's 0/1 space. No
    // pool-size sliders (#801) and no item-class groups (#834).
    const ExpectedRow kExpected[] = {
        { COMBO_SETTING_DIRECTION, WIDGET_COMBOBOX, "" },
        { COMBO_SETTING_SHARED_OCARINA, WIDGET_CHECKBOX, "" },
        // ADR 0010 D1's goal: a combobox over the five pinned RSBS_COMBO_GOAL_*
        // enumerators, the direction row's shape.
        { COMBO_SETTING_GOAL, WIDGET_COMBOBOX, "" },
    };

    // Captured by its REGISTERED name, before any PreFunc runs: the status row
    // rewrites its own name every frame, so this is the only moment it has a
    // stable one.
    WidgetInfo* statusRow = FindRow(rows, "Combo Rules Status");
    ROWS_CHECK(statusRow != nullptr, "the Cross-Game Rules page has no \"Combo Rules Status\" row -- ADR 0004 §6 "
                                     "state 4's reason has nowhere to be legible without hovering");

    WidgetInfo* settingRow[COMBO_SETTING_COUNT] = {};
    for (const ExpectedRow& expected : kExpected) {
        const std::string name =
            (expected.type == WIDGET_CHECKBOX ? CheckboxRowName(expected.id) : RowName(expected.id)) + expected.suffix;
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
    printf("[TEST] leg 1: all three tier-4 settings are rows in Combo / Cross-Game Rules, each marked '%s'\n",
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

    // ---- Leg 1c: the two "Max Items" rows are retired (#801) ----------------
    // Under the single bag no rule reads a pool size: how many items cross is an
    // outcome of the fill, not a setting, and moving either slider only re-seeded
    // the world (ADR 0011's 2026-09-27 note). Operator ruling 2026-09-30: the
    // rows go. Matched by LABEL TEXT rather than by id, so the check does not
    // depend on the ids the fix removes, and every slider counts, because those
    // two rows were the page's only sliders.
    {
        int sliders = 0;
        for (PageRow& pageRow : rows) {
            const std::string& name = pageRow.first->name;
            ROWS_CHECK(name.find("Max OoT Items") == std::string::npos &&
                           name.find("Max MM Items") == std::string::npos,
                       "the Cross-Game Rules page still offers the retired row '%s' (#801): it changes no rule and "
                       "only re-seeds the world",
                       name.c_str());
            sliders += pageRow.first->type == WIDGET_SLIDER_INT ? 1 : 0;
        }
        ROWS_CHECK(
            sliders == 0,
            "the Cross-Game Rules page has %d slider row(s); the only sliders were the retired pool sizes (#801)",
            sliders);
        printf("[TEST] leg 1c: no \"Max OoT Items\" / \"Max MM Items\" row and no slider on the page (#801)\n");
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
    ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_SHARED_OCARINA, 1) == 1,
               "the writer refused a pre-creation shared-ocarina flag");
    ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_GOAL, (int32_t)RSBS_COMBO_GOAL_BEAT_OOT) == 1,
               "the writer refused a pre-creation goal");

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
    ROWS_CHECK(StagedBool(*settingRow[COMBO_SETTING_SHARED_OCARINA]) == 1,
               "the shared-ocarina row staged %d, expected the authored 1",
               StagedBool(*settingRow[COMBO_SETTING_SHARED_OCARINA]));
    ROWS_CHECK(StagedInt(*settingRow[COMBO_SETTING_GOAL]) == (int32_t)RSBS_COMBO_GOAL_BEAT_OOT,
               "the goal row staged %d, expected the authored %d (Ganon)", StagedInt(*settingRow[COMBO_SETTING_GOAL]),
               (int)RSBS_COMBO_GOAL_BEAT_OOT);
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
    frozen.direction = (uint8_t)RSBS_COMBO_DIR_REVERSE; // authored: FORWARD
    frozen.comboFlags = 0;                              // authored: ON (#668)
    frozen.goal = (uint8_t)RSBS_COMBO_GOAL_BEAT_MM;     // authored: BEAT_OOT
    Combo_FreezeComboSettings(&frozen);
    ROWS_CHECK(Combo_ComboSettingsFrozen(), "Combo_FreezeComboSettings left the record unfrozen");
    ROWS_CHECK(Combo_ComboSettingReadOnlyReason() != nullptr, "the model reports no read-only reason once frozen");

    RunAllPreFuncs(rows);
    ExpectDecided(*settingRow[COMBO_SETTING_DIRECTION], "the direction row");
    ROWS_CHECK(StagedInt(*settingRow[COMBO_SETTING_DIRECTION]) == (int32_t)RSBS_COMBO_DIR_REVERSE,
               "the frozen direction row staged %d; it must show the SAVE's %d, not the CVar's %d",
               StagedInt(*settingRow[COMBO_SETTING_DIRECTION]), (int)RSBS_COMBO_DIR_REVERSE,
               (int)RSBS_COMBO_DIR_FORWARD);
    ExpectDecided(*settingRow[COMBO_SETTING_SHARED_OCARINA], "the shared-ocarina row");
    ROWS_CHECK(StagedBool(*settingRow[COMBO_SETTING_SHARED_OCARINA]) == 0,
               "the frozen shared-ocarina row staged %d; it must show the SAVE's OFF, not the CVar's ON -- the "
               "world was built without a shared ocarina and a row that showed otherwise would be lying about it",
               StagedBool(*settingRow[COMBO_SETTING_SHARED_OCARINA]));
    ExpectDecided(*settingRow[COMBO_SETTING_GOAL], "the goal row");
    ROWS_CHECK(StagedInt(*settingRow[COMBO_SETTING_GOAL]) == (int32_t)RSBS_COMBO_GOAL_BEAT_MM,
               "the frozen goal row staged %d; it must show the SAVE's %d (Majora), not the CVar's %d (Ganon) -- the "
               "world was proved against the frozen goal",
               StagedInt(*settingRow[COMBO_SETTING_GOAL]), (int)RSBS_COMBO_GOAL_BEAT_MM, (int)RSBS_COMBO_GOAL_BEAT_OOT);
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

        // The same, for the checkbox (#668). Not redundant with the row above:
        // a checkbox's Callback fires on a TOGGLE, so a row wired to flip the
        // model itself rather than to offer the staged value to the writer
        // would pass every other leg and still let a player turn a decided
        // world's ocarina rule on from the menu.
        WidgetInfo& ocarinaRow = *settingRow[COMBO_SETTING_SHARED_OCARINA];
        *std::get<bool*>(ocarinaRow.valuePointer) = true;
        ROWS_CHECK(ocarinaRow.callback != nullptr, "the shared-ocarina row has no Callback");
        if (ocarinaRow.callback != nullptr) {
            ocarinaRow.callback(ocarinaRow);
        }
        ROWS_CHECK(!Combo_ComboSharedOcarina(),
                   "a post-creation toggle changed the world's ocarina rule; the frozen record is the authority");
        RunPreFunc(ocarinaRow);
        ROWS_CHECK(StagedBool(ocarinaRow) == 0,
                   "after a refused toggle the shared-ocarina row staged %d; the next frame must restore the "
                   "save's OFF",
                   StagedBool(ocarinaRow));

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
        ROWS_CHECK(statusRow->name.find("apply to the next paired world") != std::string::npos &&
                       statusRow->name.find("Loading a paired file restores its own rules") != std::string::npos,
                   "the pre-creation status line does not say these author the next world and that a load restores "
                   "a file's own rules (#781): '%s'",
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
                       Combo_ComboSettingSet(COMBO_SETTING_SHARED_OCARINA, 1) == 1 &&
                       Combo_ComboSettingSet(COMBO_SETTING_GOAL, (int32_t)RSBS_COMBO_GOAL_BEAT_MM) == 1,
                   "the writer refused a pre-creation value leg 9 needs");
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
        printf("[TEST] leg 9: Reset queues one confirm; Cancel clears nothing, and its Reset button clears all three "
               "rules\n");
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
