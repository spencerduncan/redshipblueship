/**
 * @file soh_combo_settings_rows_test.cpp
 * @brief ROM-free, display-free lock for #655: the five tier-4 combo settings
 *        must exist as SohMenu ROWS in the interim Cross-Game section, carry ADR
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
 * HOW IT OBSERVES. It builds a real SohMenu headless and calls the real
 * registrar for the Cross-Game page. That is safe with no window and no ImGui
 * context because registration is container work — every widget's heavy lifting
 * lives in a lambda that only the draw path calls — and a `Ship::GuiWindow`
 * constructed
 * with an EMPTY visibility CVar touches the Ship::Context not at all
 * (GuiWindow.cpp guards the latch on `!mVisibilityConsoleVariable.empty()`).
 * The rows' PreFuncs and Callbacks are then called DIRECTLY, exactly as
 * Menu::MenuDrawItem would (ResetDisables() first, then preFunc), which is
 * possible only because they touch no ImGui: they read the src/common model,
 * write a staging buffer, and set `options->disabled`. Staged values are read
 * back through each widget's own `valuePointer`, so the test asserts on what
 * the widget would render rather than on file-static storage it cannot see.
 *
 * WHY IT REGISTERS THE PAGE AND NOT THE WHOLE MENU. `AddMenuRandomizer()` as a
 * whole cannot run ROM-free, and not for a shallow reason: its five
 * `Rando::Settings` option groups reach `Option::AddWidget`, which **runs each
 * option's callback at registration time**
 * (`Enhancements/randomizer/option.cpp:330`), and those callbacks dereference
 * `OTRGlobals::Instance->gRandoContext` — null in a display-free harness, an
 * access violation reading offset 0x80. (They also register through the GLOBAL
 * `SohGui::mSohMenu`, `option.cpp:453`, rather than through the menu whose
 * method is running.) So the Cross-Game page is its own externally linked
 * registrar, `SohGui::AddCrossGameWidgets`, and this row drives that. The scope
 * that costs: the ORDER of the sections inside `AddMenuRandomizer`, and anything
 * the option groups do to the `WidgetPath` before this page sees it, is not
 * covered here — which is exactly why the registrar pins `path.column` itself
 * instead of inheriting it.
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

#include <cstdio>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "combo_settings_view.h"
#include "context.h"
#include "foreign_items.h"

namespace SohGui {
// The Cross-Game page's registrar, defined in SohGui/SohMenuRandomizer.cpp and
// declared in no header (see the header comment above). Declared here the way
// the SohGui TUs already declare SohGui::mSohMenu.
void AddCrossGameWidgets(SohMenu& menu, WidgetPath& path);
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
// widgets under test are the real ones AddMenuRandomizer built.
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

// The six allocated RSBS_ITEMCLASS_* bits in bit order, restated here rather
// than shared with the menu file: the point is that both agree, and a shared
// array would make them agree by construction instead of by assertion.
const uint16_t kClassBits[6] = {
    (uint16_t)RSBS_ITEMCLASS_PROGRESSION,   (uint16_t)RSBS_ITEMCLASS_SONGS,          (uint16_t)RSBS_ITEMCLASS_MASKS,
    (uint16_t)RSBS_ITEMCLASS_DUNGEON_ITEMS, (uint16_t)RSBS_ITEMCLASS_DUNGEON_REWARD, (uint16_t)RSBS_ITEMCLASS_SIDEQUEST,
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

/**
 * Assert @p row is in ADR 0004 §6 state 4: disabled, with the reason string the
 * MODEL owns. The string identity matters as much as the disabling — a row that
 * greyed itself with a capability reason ("not yet available") would send a
 * player hunting for a missing feature instead of telling them the choice was
 * already made.
 */
void ExpectDecided(WidgetInfo& row, const char* what) {
    const char* reason = Combo_ComboSettingReadOnlyReason();
    ROWS_CHECK(row.options->disabled, "%s must be read-only once the record is frozen", what);
    ROWS_CHECK(reason != nullptr && row.options->disabledTooltip != nullptr &&
                   std::string(row.options->disabledTooltip) == std::string(reason != nullptr ? reason : ""),
               "%s must carry the model's reason (%s), not a capability reason; it carries '%s'", what,
               reason != nullptr ? reason : "(null)",
               row.options->disabledTooltip != nullptr ? row.options->disabledTooltip : "(null)");
}

} // namespace

extern "C" int OoT_ComboSettingsRows_RunHeadless(void) {
    printf("[TEST] combo-settings-rows: the five tier-4 combo settings are SohMenu rows in the Cross-Game section, "
           "marked per ADR 0004 §4.2, and read-only from the save once frozen (#655)\n");

    gFailures = 0;

    // A clean model before the menu is built: every PreFunc below reads it.
    ComboContext_Init();
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        Combo_ComboSettingClear((ComboSettingId)i);
    }

    // The page needs its section to exist first (AddSidebarEntry does
    // menuEntries.at), which is all AddMenuRandomizer does before reaching the
    // page. The WidgetPath is handed in with a DELIBERATELY WRONG column, so the
    // registrar's own pin is what puts the rows where they can be drawn -- if it
    // stopped pinning, leg 1's column check goes red instead of silently
    // depending on the caller.
    ComboRulesMenuProbe probe;
    probe.AddMenuEntry("Randomizer", "gSettings.Menu.RandomizerSidebarSection");
    WidgetPath path = { "Randomizer", "Cross-Game", SECTION_COLUMN_3 };
    SohGui::AddCrossGameWidgets(probe, path);

    // ---- Leg 1: the section and the five rows exist -------------------------
    auto& entries = probe.Entries();
    if (!entries.contains("Randomizer")) {
        printf("[TEST] FAIL(1): AddMenuRandomizer registered no \"Randomizer\" menu entry\n");
        return 1;
    }
    auto& sidebars = entries.at("Randomizer").sidebars;
    if (!sidebars.contains("Cross-Game")) {
        printf("[TEST] FAIL(1): the \"Randomizer\" menu has no \"Cross-Game\" sidebar page -- the interim host for "
               "the tier-4 combo rules (#509/#655) is gone or was renamed\n");
        return 1;
    }
    const SidebarEntry& page = sidebars.at("Cross-Game");
    auto& columns = sidebars.at("Cross-Game").columnWidgets;
    if (columns.empty()) {
        printf("[TEST] FAIL(1): the Cross-Game page has no widget columns\n");
        return 1;
    }
    std::vector<PageRow> rows = FlattenPage(columns);

    struct ExpectedRow {
        ComboSettingId id;
        WidgetType type;
        const char* suffix; // "": the bare name; ": %d": the slider's value format
    };
    // The direction is a combobox over the four pinned enumerators, each pool
    // size a slider over the pinned 1..CAP space, and each item-class bitset a
    // marked header over six checkboxes (a bitset is not expressible as either
    // of the other two).
    const ExpectedRow kExpected[] = {
        { COMBO_SETTING_DIRECTION, WIDGET_COMBOBOX, "" },
        { COMBO_SETTING_POOL_SIZE_OOT, WIDGET_SLIDER_INT, ": %d" },
        { COMBO_SETTING_POOL_SIZE_MM, WIDGET_SLIDER_INT, ": %d" },
        { COMBO_SETTING_ITEM_CLASS_OOT, WIDGET_SEPARATOR_TEXT, "" },
        { COMBO_SETTING_ITEM_CLASS_MM, WIDGET_SEPARATOR_TEXT, "" },
    };

    // Captured by its REGISTERED name, before any PreFunc runs: the status row
    // rewrites its own name every frame, so this is the only moment it has a
    // stable one.
    WidgetInfo* statusRow = FindRow(rows, "Combo Rules Status");
    ROWS_CHECK(statusRow != nullptr, "the Cross-Game page has no \"Combo Rules Status\" row -- ADR 0004 §6 state 4's "
                                     "reason has nowhere to be legible without hovering");

    WidgetInfo* settingRow[COMBO_SETTING_COUNT] = { nullptr, nullptr, nullptr, nullptr, nullptr };
    for (const ExpectedRow& expected : kExpected) {
        const std::string name = RowName(expected.id) + expected.suffix;
        uint32_t column = 0;
        WidgetInfo* row = FindRow(rows, name, &column);
        ROWS_CHECK(row != nullptr, "no Cross-Game row named '%s' -- the setting '%s' is unreachable in the menu",
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
    printf("[TEST] leg 1: all five tier-4 settings are rows in Randomizer / Cross-Game, each marked '%s'\n",
           Combo_ComboSettingSharedMarker());

    // ---- Leg 2: no row is its own writer, and no pop-out is offered ---------
    // The enforcement rule (ADR 0004 §6): the gate is on the src/common writers,
    // so no widget in this section may bind one of the five keys directly -- a
    // WIDGET_CVAR_* row would write the store itself and never reach the freeze
    // check.
    int classCheckboxes = 0;
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
        if (row.type == WIDGET_CHECKBOX) {
            classCheckboxes++;
        }
        ROWS_CHECK(!(row.type == WIDGET_WINDOW_BUTTON && row.windowName != nullptr &&
                     std::string(row.windowName) == "Combo Settings"),
                   "the Cross-Game page still offers a pop-out for the combo settings ('%s'); #655 replaced it with "
                   "the rows above",
                   row.name.c_str());
    }
    // Twelve: six allocated classes per direction. A count rather than a floor,
    // so an appended RSBS_ITEMCLASS_* bit without a row here is red.
    ROWS_CHECK(classCheckboxes == 12,
               "%d item-class checkboxes in the Cross-Game page, expected 12 (6 classes x 2 "
               "directions); an allocated RSBS_ITEMCLASS_* bit has no row",
               classCheckboxes);
    for (int which = 0; which < 2; which++) {
        const std::string prefix = (which == 0) ? "Ocarina of Time item class: " : "Majora's Mask item class: ";
        for (int bit = 0; bit < 6; bit++) {
            const std::string name = prefix + Combo_ForeignItemClassName(kClassBits[bit]);
            ROWS_CHECK(FindRow(rows, name) != nullptr, "no checkbox row named '%s'", name.c_str());
        }
    }
    printf("[TEST] leg 2: no row binds a gCombo.Rando.* key directly, %d class checkboxes present, no combo-settings "
           "pop-out row\n",
           classCheckboxes);

    // ---- Leg 3: pre-creation the rows are editable and show the resolver ----
    ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_DIRECTION, (int32_t)RSBS_COMBO_DIR_FORWARD) == 1,
               "the writer refused a pre-creation direction");
    ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_POOL_SIZE_OOT, 2) == 1, "the writer refused a pre-creation pool");
    ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_POOL_SIZE_MM, 3) == 1, "the writer refused a pre-creation pool");
    ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_ITEM_CLASS_OOT, (int32_t)RSBS_ITEMCLASS_SONGS) == 1,
               "the writer refused a pre-creation class mask");
    ROWS_CHECK(Combo_ComboSettingSet(COMBO_SETTING_ITEM_CLASS_MM, (int32_t)RSBS_ITEMCLASS_MASKS) == 1,
               "the writer refused a pre-creation class mask");

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
    ROWS_CHECK(StagedInt(*settingRow[COMBO_SETTING_POOL_SIZE_OOT]) == 2, "the OoT pool row staged %d, expected 2",
               StagedInt(*settingRow[COMBO_SETTING_POOL_SIZE_OOT]));
    ROWS_CHECK(StagedInt(*settingRow[COMBO_SETTING_POOL_SIZE_MM]) == 3, "the MM pool row staged %d, expected 3",
               StagedInt(*settingRow[COMBO_SETTING_POOL_SIZE_MM]));
    {
        // The OoT set is SONGS alone, so exactly one of its six boxes is ticked.
        int ticked = 0;
        for (int bit = 0; bit < 6; bit++) {
            WidgetInfo* box = FindRow(rows, std::string("Ocarina of Time item class: ") +
                                                Combo_ForeignItemClassName(kClassBits[bit]));
            if (box == nullptr) {
                continue;
            }
            const bool on = *std::get<bool*>(box->valuePointer);
            const bool expectOn = (kClassBits[bit] == (uint16_t)RSBS_ITEMCLASS_SONGS);
            ROWS_CHECK(on == expectOn, "OoT class '%s' staged %d, expected %d",
                       Combo_ForeignItemClassName(kClassBits[bit]), (int)on, (int)expectOn);
            ticked += on ? 1 : 0;
        }
        ROWS_CHECK(ticked == 1, "%d OoT class boxes ticked, expected 1", ticked);
    }
    printf("[TEST] leg 3: before the creation event every row is editable and stages the resolver's value\n");

    // ---- Leg 4: a pre-creation edit reaches the store through the model -----
    // The Callback is the row's ONLY write path. Editing the staging buffer the
    // way the widget would and firing it must land in the store; if a future row
    // were rewired to a CVar widget this leg would still pass, which is why leg
    // 2 exists as well.
    {
        WidgetInfo& poolRow = *settingRow[COMBO_SETTING_POOL_SIZE_OOT];
        *std::get<int32_t*>(poolRow.valuePointer) = 5;
        ROWS_CHECK(poolRow.callback != nullptr, "the OoT pool row has no Callback");
        if (poolRow.callback != nullptr) {
            poolRow.callback(poolRow);
        }
        int32_t stored = 0;
        ROWS_CHECK(Combo_ComboSettingReadStore(COMBO_SETTING_POOL_SIZE_OOT, &stored) && stored == 5,
                   "a pre-creation edit did not reach the store (read back %d)", stored);
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
    frozen.direction = (uint8_t)RSBS_COMBO_DIR_REVERSE;                                  // authored: FORWARD
    frozen.poolSizeOoT = 7;                                                              // authored: 5
    frozen.poolSizeMM = 4;                                                               // authored: 3
    frozen.itemClassOoT = (uint16_t)(RSBS_ITEMCLASS_PROGRESSION | RSBS_ITEMCLASS_MASKS); // authored: SONGS
    frozen.itemClassMM = 0;                                                              // authored: MASKS
    Combo_FreezeComboSettings(&frozen);
    ROWS_CHECK(Combo_ComboSettingsFrozen(), "Combo_FreezeComboSettings left the record unfrozen");
    ROWS_CHECK(Combo_ComboSettingReadOnlyReason() != nullptr, "the model reports no read-only reason once frozen");

    RunAllPreFuncs(rows);
    ExpectDecided(*settingRow[COMBO_SETTING_DIRECTION], "the direction row");
    ExpectDecided(*settingRow[COMBO_SETTING_POOL_SIZE_OOT], "the OoT pool row");
    ExpectDecided(*settingRow[COMBO_SETTING_POOL_SIZE_MM], "the MM pool row");
    ROWS_CHECK(StagedInt(*settingRow[COMBO_SETTING_DIRECTION]) == (int32_t)RSBS_COMBO_DIR_REVERSE,
               "the frozen direction row staged %d; it must show the SAVE's %d, not the CVar's %d",
               StagedInt(*settingRow[COMBO_SETTING_DIRECTION]), (int)RSBS_COMBO_DIR_REVERSE,
               (int)RSBS_COMBO_DIR_FORWARD);
    ROWS_CHECK(StagedInt(*settingRow[COMBO_SETTING_POOL_SIZE_OOT]) == 7,
               "the frozen OoT pool row staged %d; it must show the SAVE's 7, not the CVar's 5",
               StagedInt(*settingRow[COMBO_SETTING_POOL_SIZE_OOT]));
    ROWS_CHECK(StagedInt(*settingRow[COMBO_SETTING_POOL_SIZE_MM]) == 4,
               "the frozen MM pool row staged %d; it must show the SAVE's 4, not the CVar's 3",
               StagedInt(*settingRow[COMBO_SETTING_POOL_SIZE_MM]));
    for (int which = 0; which < 2; which++) {
        const std::string prefix = (which == 0) ? "Ocarina of Time item class: " : "Majora's Mask item class: ";
        const uint16_t mask = (which == 0) ? frozen.itemClassOoT : frozen.itemClassMM;
        for (int bit = 0; bit < 6; bit++) {
            WidgetInfo* box = FindRow(rows, prefix + Combo_ForeignItemClassName(kClassBits[bit]));
            if (box == nullptr) {
                continue;
            }
            ExpectDecided(*box, box->name.c_str());
            const bool on = *std::get<bool*>(box->valuePointer);
            ROWS_CHECK(on == ((mask & kClassBits[bit]) != 0),
                       "frozen class row '%s' staged %d; the SAVE's mask is %04X", box->name.c_str(), (int)on,
                       (unsigned)mask);
        }
    }
    // The empty-set note: MM's frozen mask is 0, OoT's is not, so exactly one of
    // the two "places nothing" lines is shown. A note that never hides would
    // report an empty pool for a world that has one.
    {
        WidgetInfo* ootNote = FindRow(rows, "No Ocarina of Time classes armed: this direction places nothing.");
        WidgetInfo* mmNote = FindRow(rows, "No Majora's Mask classes armed: this direction places nothing.");
        ROWS_CHECK(ootNote != nullptr && ootNote->isHidden, "the OoT empty-set note is shown for a non-empty mask");
        ROWS_CHECK(mmNote != nullptr && !mmNote->isHidden, "the MM empty-set note is hidden for an empty mask");
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
        ROWS_CHECK(statusRow->name.find("corrupt session state") != std::string::npos,
                   "a frozen record with no live pairing is not named as corrupt: '%s'", statusRow->name.c_str());

        // Unfrozen and unpaired: the ordinary pre-creation state.
        ComboContext_Init();
        RunPreFunc(*statusRow);
        ROWS_CHECK(statusRow->name.find("freeze into the paired world's identity") != std::string::npos,
                   "the pre-creation status line does not say these freeze at creation: '%s'", statusRow->name.c_str());
    }

    // Leave the process clean: this row writes the five tier-4 keys and freezes
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
