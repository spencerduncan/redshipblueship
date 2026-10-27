/**
 * @file soh_menu_mm_randomizer_pages_test.cpp
 * @brief ROM-free, display-free lock for Randomizer > MM Randomizer and Randomizer > MM
 *        Tricks: MM's randomizer options and tricks as menu pages instead of a
 *        pop-out window (ADR 0004's 2026-09-27 host amendment; #497, #578).
 *
 * CTest row MenuMmRandomizerPages in CMake/SingleExecutable.cmake, dispatch
 * "menu-mm-randomizer-pages" in src/common/test_runner.cpp.
 *
 * The model's decisions (columns, states, notes) are ComboMMOptionsPage's; the
 * descriptor tables' honesty is MMRandoOptions' and MMTrickTable's. This row is
 * the join: that the rows a player sees ARE those decisions, and that every
 * edit still goes through the gated writers. Every failure it catches is silent
 * otherwise, because a registered row with the wrong binding still draws.
 *
 *   1. THE PAGES ARE NOT THERE, OR NOT TOGETHER, OR UNDER THE WRONG HEADER. Both
 *      register from a file-scope initializer beside the Majora's Mask page's
 *      (SohMenuComboMmEnhancements.cpp) with ComboPageHost::Randomizer, so they
 *      are pages of SoH's Randomizer header (#1101, operator ruling 2026-10-21):
 *      AddComboRandomizerPages draws them as MM Randomizer, MM Locations, MM
 *      Tricks, then the rules as Cross-Game, and no other header holds any of
 *      them. The options page declares two columns (Randomizer > General's),
 *      the tricks and locations pages one each (Randomizer > Tricks/Glitches'
 *      and Locations').
 *
 *   2. AN OPTION IS LOST OR DOUBLED. The options page's interactive rows are
 *      EXACTLY MM's descriptor table: one row per descriptor, of the widget type
 *      the descriptor names, under the name the page derives from its label (the
 *      "##MMRando" id suffix on checkboxes and comboboxes, SoH's "Name: %d" on
 *      sliders), plus the seven trap-type rows (#1101 lane R4: Freeze, Blast,
 *      Shock, Jinx, Wallet, Like Like and Time Traps, each a "##MMRando"
 *      checkbox over its gRando.Traps.* key, spelled here, directly after Trap
 *      Count in Trap Count's column, as MM's own Items tab drew its "Toggle Trap
 *      Types"), and nothing else interactive but Reset.
 *
 *   3. A ROW LANDS IN THE WRONG COLUMN, or in one the page never draws, or
 *      outside its group's header. The headers are SoH's Randomizer headers in
 *      SoH's order (#1101, option C), spelled here independently of the model:
 *      Logic, Win Condition, Area Access, Shuffle Items down the left column;
 *      Shuffle Shops & Merchants, Additional Items, Traps, Hints, Starting
 *      Items down the right.
 *
 *   4. A ROW SHOWS THE WRONG STATE. Unpaired: a capability-blocked row is
 *      disabled with SoH's disabled tooltip around the table's own reason, and
 *      every other row is live. Frozen: every row and Reset are disabled with
 *      "Already Decided". The name stays the row's own in every state, across
 *      two draw passes (ApplyPresentation runs from a PreFunc with its own name
 *      as the base, so a non-idempotent call would compound).
 *
 *   5. A ROW WRITES AROUND THE GATE. Each row's PreFunc refreshes its staging
 *      value from the model and its Callback offers the edit to the writer: an
 *      unfrozen edit lands, a frozen one is refused and the next draw restores
 *      the model's value. Driven on a checkbox, a combobox, a slider and the
 *      clock row, whose in-slider text is the value as HH:MM.
 *
 *   6. AN UNKNOWN COMBO VALUE THROWS. UIWidgets::Combobox previews with
 *      std::map::at, which throws on a key it does not hold, and MenuDrawItem
 *      catches only bad_variant_access. A value the table has no label for is
 *      taught as "Unknown (N)" and withdrawn once the value is known again.
 *
 *   7. THE NOTES DISAGREE WITH THE MODEL. The status note is the model's
 *      sentence in each state; the suspended note shows exactly while Majora's
 *      Mask is not running and the profile is editable; the tricks page's note
 *      is the model's headline. Unpaired, both pages open with the one status
 *      sentence every randomizer page of ours shares (#1101 decision 7).
 *
 *   8. THE RACE LOCKOUT DIFFERS FROM SOH'S RANDOMIZER PAGES. SoH's race lockout
 *      (CVAR_SETTING("DisableChanges"), Menu.cpp's MenuDrawItem) disables every
 *      row whose WidgetInfo::raceDisable is set, and SoH's Randomizer > General
 *      rows keep that default. So does every interactive row here (each option
 *      row and Reset): under a lockout MM's options lock exactly as OoT's do.
 *      Every gray note opts out (RaceDisable(false)), so the status note still
 *      says the world is frozen while a lockout replaces a row's "Already
 *      Decided" tooltip with its own. The pop-out window these pages replaced
 *      ignored the lockout; the change is deliberate (ADR 0004's 2026-09-27
 *      host amendment). The trick list applies the lockout itself, as SoH's
 *      DrawTricksMenu does.
 *
 *   9. A TRAP-TYPE ROW WRITES AROUND THE GATE (#1101 lane R4). Each of the seven
 *      rows refreshes from the model and offers its edit to the gated trap-type
 *      writer: an unfrozen edit lands in its own gRando.Traps.* key and no
 *      other, a frozen one is refused and the next draw restores the model's
 *      value, and Reset clears all seven with the options and tricks.
 *
 *  10. MM LOCATIONS IS NOT SOH'S LOCATIONS IN SHAPE (#1101 lane R3). One column,
 *      the gray status note (the model's sentence unpaired, frozen and under
 *      Vanilla logic, legible under a race lockout), a section header, and ONE
 *      custom row, the Included / Forced Junk table, as SoH's Locations page is
 *      one custom row.
 *
 * What the trick list DRAWS (every trick once, in its column, with its reason)
 * is UiSnapshot's trick census: it needs an ImGui frame, which this row has not.
 *
 * Appearance is UiSnapshot's (the pages' captures beside Randomizer > General
 * and Randomizer > Tricks/Glitches).
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "soh/SohGui/SohMenu.h"

#include "combo_mm_locations_view.h"
#include "combo_mm_options_page.h"
#include "context.h"
#include "foreign_items.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int gFailures = 0;

#define MMRP_CHECK(cond, ...)                                              \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            printf("[TEST]       ");                                       \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
            gFailures++;                                                   \
        }                                                                  \
    } while (0)

class MmRandomizerPagesProbe final : public SohGui::SohMenu {
  public:
    MmRandomizerPagesProbe() : SohGui::SohMenu("", "MM Randomizer Pages Probe") {
    }

    std::unordered_map<std::string, MainMenuEntry>& Entries() {
        return menuEntries;
    }
};

struct FlatRow {
    WidgetInfo* info;
    uint32_t column;
};

std::vector<FlatRow> FlattenRows(SidebarEntry& page) {
    std::vector<FlatRow> rows;
    for (uint32_t column = 0; column < page.columnWidgets.size(); column++) {
        for (WidgetInfo& row : page.columnWidgets.at(column)) {
            rows.push_back(FlatRow{ &row, column });
        }
    }
    return rows;
}

void DrawPass(WidgetInfo& info) {
    info.ResetDisables();
    if (info.preFunc != nullptr) {
        info.preFunc(info);
    }
}

bool IsDisabled(WidgetInfo& info) {
    return info.options != nullptr && info.options->disabled;
}

std::string TooltipShown(WidgetInfo& info) {
    if (info.options == nullptr) {
        return std::string();
    }
    const char* t = info.options->disabled ? info.options->disabledTooltip : info.options->tooltip;
    return t != nullptr ? t : "";
}

std::string SohDisabledShape(const char* reason) {
    return std::string("This setting is disabled because: \n\n- ") + reason;
}

/** The row name the page gives @p desc, spelled independently of the page TU. */
std::string ExpectedRowName(const ComboMMOptionDesc* desc) {
    switch ((ComboMMOptionWidget)desc->widget) {
        case COMBO_MM_WIDGET_CHECKBOX:
        case COMBO_MM_WIDGET_COMBO:
            return std::string(desc->label) + "##MMRando";
        case COMBO_MM_WIDGET_SLIDER:
            return std::string(desc->label) + ": %d";
        case COMBO_MM_WIDGET_TIME:
        default:
            return desc->label;
    }
}

WidgetType ExpectedType(const ComboMMOptionDesc* desc) {
    switch ((ComboMMOptionWidget)desc->widget) {
        case COMBO_MM_WIDGET_CHECKBOX:
            return WIDGET_CHECKBOX;
        case COMBO_MM_WIDGET_COMBO:
            return WIDGET_COMBOBOX;
        default:
            return WIDGET_SLIDER_INT;
    }
}

bool IsInteractiveRow(const WidgetInfo& info) {
    return info.type == WIDGET_CHECKBOX || info.type == WIDGET_COMBOBOX || info.type == WIDGET_SLIDER_INT ||
           info.type == WIDGET_BUTTON || info.type == WIDGET_CVAR_CHECKBOX || info.type == WIDGET_CVAR_COMBOBOX ||
           info.type == WIDGET_CVAR_SLIDER_INT;
}

FlatRow* FindByName(std::vector<FlatRow>& rows, const std::string& name) {
    for (FlatRow& row : rows) {
        if (row.info->name == name) {
            return &row;
        }
    }
    return nullptr;
}

/** The seven trap-type rows (#1101 lane R4), spelled independently of the table:
 *  MM's own labels (Rando/Menu.cpp's "Toggle Trap Types") and the keys Traps.cpp
 *  freezes, in MM's TrapTypes order. */
struct TrapRow {
    const char* label;
    const char* cvar;
};
const TrapRow kTrapRows[] = {
    { "Freeze Traps", "gRando.Traps.Freeze" }, { "Blast Traps", "gRando.Traps.Blast" },
    { "Shock Traps", "gRando.Traps.Shock" },   { "Jinx Traps", "gRando.Traps.Jinx" },
    { "Wallet Traps", "gRando.Traps.Wallet" }, { "Like Like Traps", "gRando.Traps.Enemy" },
    { "Time Traps", "gRando.Traps.Time" },
};
constexpr int kTrapRowCount = (int)(sizeof(kTrapRows) / sizeof(kTrapRows[0]));

void Freeze(bool frozen) {
    ComboContext_Init();
    if (frozen) {
        gComboCtx.sourceIsRando = true;
        gComboCtx.sharedRandoSeed = 0xC0FFEE09u;
        gComboCtx.sharedRandoSettingsHash = 0x5EED0009u;
        gComboCtx.mmProfileDigest = 0x4D4D0009u;
    }
}

} // namespace

extern "C" int OoT_MenuMmRandomizerPages_RunHeadless(void) {
    printf(
        "[TEST] menu-mm-randomizer-pages: MM's randomizer options and tricks are Randomizer pages whose rows are the "
        "descriptor table, in the model's columns and states, writing only through the gated writers\n");
    gFailures = 0;
    const GameId prevGame = Context_GetCurrentGame();
    ComboContext_Init();

    // Our slice of the Randomizer header, as production builds it
    // (AddMenuRandomizer() as a whole cannot run ROM-free), then our Dev Tools
    // pages: no other header holds a page of ours since #1133 retired Combo.
    MmRandomizerPagesProbe probe;
    probe.AddMenuEntry("Randomizer", "gSettings.Menu.RandomizerSidebarSection");
    WidgetPath randoPath = { "Randomizer", "Tricks/Glitches", SECTION_COLUMN_1 };
    SohGui::AddComboRandomizerPages(probe, randoPath);
    probe.AddMenuEntry("Dev Tools", "gSettings.Menu.DevToolsSidebarSection");
    WidgetPath devToolsPath = { "Dev Tools", "General", SECTION_COLUMN_1 };
    SohGui::AddComboDevToolsPages(probe, devToolsPath);
    auto& entries = probe.Entries();
    if (!entries.contains("Dev Tools") || !entries.contains("Randomizer")) {
        printf("[TEST] FAIL(1): the probe has no Dev Tools and/or Randomizer section\n");
        return 1;
    }
    MainMenuEntry& devTools = entries.at("Dev Tools");
    MainMenuEntry& rando = entries.at("Randomizer");

    // ---- Leg 1: the pages, their column counts, their header and place ------
    if (!rando.sidebars.contains(COMBO_MM_OPTIONS_PAGE_NAME) || !rando.sidebars.contains(COMBO_MM_TRICKS_PAGE_NAME)) {
        printf("[TEST] FAIL(1): the Randomizer header has no \"%s\" and/or \"%s\" page: MM's randomizer options "
               "are not where the one randomizer menu puts them (operator ruling 2026-10-21)\n",
               COMBO_MM_OPTIONS_PAGE_NAME, COMBO_MM_TRICKS_PAGE_NAME);
        return 1;
    }
    MMRP_CHECK(!devTools.sidebars.contains(COMBO_MM_OPTIONS_PAGE_NAME) &&
                   !devTools.sidebars.contains(COMBO_MM_TRICKS_PAGE_NAME),
               "the Dev Tools header holds \"%s\" or \"%s\" as well", COMBO_MM_OPTIONS_PAGE_NAME,
               COMBO_MM_TRICKS_PAGE_NAME);
    SidebarEntry& options = rando.sidebars.at(COMBO_MM_OPTIONS_PAGE_NAME);
    SidebarEntry& tricks = rando.sidebars.at(COMBO_MM_TRICKS_PAGE_NAME);
    MMRP_CHECK(options.columnCount == COMBO_MM_OPTIONS_PAGE_COLUMNS,
               "the options page declares %u columns, expected %d (Randomizer > General's)", options.columnCount,
               COMBO_MM_OPTIONS_PAGE_COLUMNS);
    MMRP_CHECK(tricks.columnCount == 1, "the tricks page declares %u columns, expected 1 (Tricks/Glitches')",
               tricks.columnCount);
    {
        // The ruling's order: MM Randomizer, MM Locations, MM Tricks,
        // Cross-Game, the four pages AddComboRandomizerPages registers.
        const std::vector<std::string>& order = rando.sidebarOrder;
        long options_at = -1, locations_at = -1, tricks_at = -1, rules_at = -1;
        for (std::size_t i = 0; i < order.size(); i++) {
            if (order[i] == COMBO_MM_OPTIONS_PAGE_NAME) {
                options_at = (long)i;
            } else if (order[i] == COMBO_MM_LOCATIONS_PAGE_NAME) {
                locations_at = (long)i;
            } else if (order[i] == COMBO_MM_TRICKS_PAGE_NAME) {
                tricks_at = (long)i;
            } else if (order[i] == "Cross-Game") {
                rules_at = (long)i;
            }
        }
        MMRP_CHECK(options_at >= 0 && locations_at == options_at + 1 && tricks_at == locations_at + 1 &&
                       rules_at == tricks_at + 1 && rules_at == (long)order.size() - 1,
                   "the Randomizer sidebar has MM Randomizer at %ld, MM Locations at %ld, MM Tricks at %ld, "
                   "Cross-Game at %ld of %zu; the ruling's order is MM Randomizer, MM Locations, MM Tricks, Cross-Game",
                   options_at, locations_at, tricks_at, rules_at, order.size());
    }
    printf("[TEST] leg 1: the pages are registered with their column counts under the Randomizer header, in the "
           "order MM Randomizer, MM Locations, MM Tricks, Cross-Game\n");

    // ---- Leg 2: the row set equals the descriptor table ---------------------
    std::vector<FlatRow> rows = FlattenRows(options);
    const int count = Combo_MMOptionCount();
    MMRP_CHECK(count > 0, "no MM option table is registered after AddComboRandomizerPages");
    {
        int interactive = 0;
        int resets = 0;
        for (FlatRow& row : rows) {
            if (IsInteractiveRow(*row.info)) {
                interactive++;
                MMRP_CHECK(row.info->type != WIDGET_CVAR_CHECKBOX && row.info->type != WIDGET_CVAR_COMBOBOX &&
                               row.info->type != WIDGET_CVAR_SLIDER_INT,
                           "row '%s' is a WIDGET_CVAR_* row, which writes its CVar itself, beside the freeze gate",
                           row.info->name.c_str());
            }
            if (row.info->type == WIDGET_BUTTON) {
                resets++;
                MMRP_CHECK(row.info->name == COMBO_MM_OPTIONS_RESET_TITLE, "unexpected button '%s'",
                           row.info->name.c_str());
            }
            MMRP_CHECK(row.column < options.columnCount,
                       "row '%s' landed in column %u of a page that draws %u: registered and never drawn",
                       row.info->name.c_str(), row.column, options.columnCount);
        }
        MMRP_CHECK(resets == 1, "the options page holds %d Reset buttons, expected 1", resets);
        MMRP_CHECK(interactive == count + kTrapRowCount + 1,
                   "the options page holds %d interactive rows, expected the %d descriptors, the %d trap types and "
                   "Reset: an option is lost or doubled",
                   interactive, count, kTrapRowCount);
        // The seven trap-type rows (#1101 lane R4): the model's table is those
        // seven, in MM's order, and each is one checkbox after Trap Count.
        MMRP_CHECK(Combo_MMTrapTypeCount() == kTrapRowCount, "the trap-type table holds %d rows, expected %d",
                   Combo_MMTrapTypeCount(), kTrapRowCount);
        for (int t = 0; t < kTrapRowCount; t++) {
            const ComboMMTrapTypeDesc* d = Combo_MMTrapTypeAt(t);
            MMRP_CHECK(d != nullptr && d->type == (uint8_t)t && std::string(d->label) == kTrapRows[t].label &&
                           std::string(d->cvar) == kTrapRows[t].cvar,
                       "trap type %d is not %s over %s", t, kTrapRows[t].label, kTrapRows[t].cvar);
            const std::string name = std::string(kTrapRows[t].label) + "##MMRando";
            int matches = 0;
            FlatRow* found = nullptr;
            for (FlatRow& row : rows) {
                if (row.info->name == name) {
                    matches++;
                    found = &row;
                }
            }
            MMRP_CHECK(matches == 1, "the trap type %s has %d rows named '%s', expected exactly 1", kTrapRows[t].label,
                       matches, name.c_str());
            if (found == nullptr) {
                continue;
            }
            MMRP_CHECK(found->info->type == WIDGET_CHECKBOX, "the trap row '%s' has widget type %d, expected %d",
                       name.c_str(), (int)found->info->type, (int)WIDGET_CHECKBOX);
            // Directly after Trap Count, in MM's order, in Trap Count's column
            // and so under its header (MM's own Items tab drew the "Toggle Trap
            // Types" there). Trap Count is named here, not read from the page.
            const WidgetInfo* anchor = nullptr;
            unsigned anchorColumn = 0;
            for (FlatRow& row : rows) {
                if (row.info->name == "Trap Count: %d") {
                    anchor = row.info;
                    anchorColumn = row.column;
                }
            }
            MMRP_CHECK(anchor != nullptr, "the options page has no Trap Count row for the trap types to follow");
            if (anchor == nullptr) {
                continue;
            }
            MMRP_CHECK(found->column == anchorColumn, "the trap row '%s' sits in column %u, Trap Count in column %u",
                       name.c_str(), found->column, anchorColumn);
            const std::vector<WidgetInfo>& column = options.columnWidgets.at(anchorColumn);
            size_t at = column.size();
            for (size_t w = 0; w < column.size(); w++) {
                if (&column[w] == anchor) {
                    at = w;
                }
            }
            const size_t want = at + 1 + (size_t)t;
            MMRP_CHECK(
                want < column.size() && &column[want] == found->info,
                "the trap row '%s' is not row %d after Trap Count: the trap types do not follow it in MM's order",
                name.c_str(), t + 1);
        }
        for (int i = 0; i < count; i++) {
            const ComboMMOptionDesc* desc = Combo_MMOptionAt(i);
            const std::string name = ExpectedRowName(desc);
            int matches = 0;
            FlatRow* found = nullptr;
            for (FlatRow& row : rows) {
                if (row.info->name == name) {
                    matches++;
                    found = &row;
                }
            }
            MMRP_CHECK(matches == 1, "option %s has %d rows named '%s', expected exactly 1", desc->name, matches,
                       name.c_str());
            if (found == nullptr) {
                continue;
            }
            MMRP_CHECK(found->info->type == ExpectedType(desc), "option %s's row has widget type %d, expected %d",
                       desc->name, (int)found->info->type, (int)ExpectedType(desc));
            // ---- Leg 3: the model's column, under its group's header ------
            MMRP_CHECK((int)found->column == Combo_MMOptionsPage_GroupColumn(desc->group),
                       "option %s sits in column %u, the model puts its group in column %d", desc->name, found->column,
                       Combo_MMOptionsPage_GroupColumn(desc->group));
            const WidgetInfo* header = nullptr;
            for (WidgetInfo& row : options.columnWidgets.at(found->column)) {
                if (row.type == WIDGET_SEPARATOR_TEXT) {
                    header = &row;
                }
                if (&row == found->info) {
                    break;
                }
            }
            MMRP_CHECK(header != nullptr && header->name == Combo_MMOptionGroupName(desc->group),
                       "option %s sits under the header '%s', expected its group's '%s'", desc->name,
                       header != nullptr ? header->name.c_str() : "(none)", Combo_MMOptionGroupName(desc->group));
        }
    }
    // ---- Leg 3 (cont.): the headers are SoH's, in SoH's order --------------
    {
        const std::vector<std::vector<std::string>> kHeaders = {
            { "Logic", "Win Condition", "Area Access", "Shuffle Items" },
            { "Shuffle Shops & Merchants", "Additional Items", "Traps", "Hints", "Starting Items" },
        };
        for (uint32_t column = 0; column < kHeaders.size(); column++) {
            std::vector<std::string> seen;
            if (column < options.columnWidgets.size()) {
                for (WidgetInfo& row : options.columnWidgets.at(column)) {
                    if (row.type == WIDGET_SEPARATOR_TEXT) {
                        seen.push_back(row.name);
                    }
                }
            }
            std::string got;
            for (const std::string& h : seen) {
                got += (got.empty() ? "" : ", ") + h;
            }
            MMRP_CHECK(seen == kHeaders.at(column),
                       "column %u of the options page reads the headers [%s], not SoH's vocabulary in SoH's order",
                       column + 1, got.c_str());
        }
    }
    printf("[TEST] leg 2-3: the options page's rows are the %d descriptors, once each, in the model's columns under "
           "their groups' headers, which are SoH's in SoH's order\n",
           count);

    // ---- Leg 4: the states -----------------------------------------------------
    FlatRow* reset = FindByName(rows, COMBO_MM_OPTIONS_RESET_TITLE);
    for (int frozen = 0; frozen <= 1; frozen++) {
        Freeze(frozen != 0);
        for (int i = 0; i < count; i++) {
            const ComboMMOptionDesc* desc = Combo_MMOptionAt(i);
            FlatRow* row = FindByName(rows, ExpectedRowName(desc));
            if (row == nullptr) {
                continue;
            }
            const std::string registered = row->info->name;
            DrawPass(*row->info);
            DrawPass(*row->info);
            MMRP_CHECK(row->info->name == registered, "option %s's row was renamed to '%s' by its PreFunc", desc->name,
                       row->info->name.c_str());
            const bool blocked =
                desc->liveness == COMBO_MM_LIVENESS_PARTIAL || desc->liveness == COMBO_MM_LIVENESS_DORMANT;
            if (frozen) {
                MMRP_CHECK(IsDisabled(*row->info) &&
                               TooltipShown(*row->info) == SohDisabledShape(COMBO_MM_OPTIONS_FROZEN_REASON),
                           "frozen: option %s is %s with tooltip \"%s\"", desc->name,
                           IsDisabled(*row->info) ? "disabled" : "LIVE", TooltipShown(*row->info).c_str());
            } else if (blocked) {
                MMRP_CHECK(IsDisabled(*row->info) && TooltipShown(*row->info) == SohDisabledShape(desc->disabledReason),
                           "blocked option %s is %s with tooltip \"%s\"", desc->name,
                           IsDisabled(*row->info) ? "disabled" : "LIVE", TooltipShown(*row->info).c_str());
            } else {
                MMRP_CHECK(!IsDisabled(*row->info), "live option %s is drawn disabled", desc->name);
            }
        }
        // A trap row is live until the profile is stamped, then "Already Decided".
        for (int t = 0; t < kTrapRowCount; t++) {
            FlatRow* row = FindByName(rows, std::string(kTrapRows[t].label) + "##MMRando");
            if (row == nullptr) {
                continue;
            }
            const std::string registered = row->info->name;
            DrawPass(*row->info);
            DrawPass(*row->info);
            MMRP_CHECK(row->info->name == registered, "the trap row %s was renamed to '%s' by its PreFunc",
                       kTrapRows[t].label, row->info->name.c_str());
            if (frozen) {
                MMRP_CHECK(IsDisabled(*row->info) &&
                               TooltipShown(*row->info) == SohDisabledShape(COMBO_MM_OPTIONS_FROZEN_REASON),
                           "frozen: the trap row %s is %s with tooltip \"%s\"", kTrapRows[t].label,
                           IsDisabled(*row->info) ? "disabled" : "LIVE", TooltipShown(*row->info).c_str());
            } else {
                MMRP_CHECK(!IsDisabled(*row->info), "the trap row %s is drawn disabled unpaired", kTrapRows[t].label);
            }
        }
        if (reset != nullptr) {
            DrawPass(*reset->info);
            MMRP_CHECK(IsDisabled(*reset->info) == (frozen != 0), "Reset is %s while the profile is %s",
                       IsDisabled(*reset->info) ? "disabled" : "live", frozen ? "frozen" : "editable");
        }
    }
    Freeze(false);
    printf("[TEST] leg 4: every row takes its table's state unpaired and \"Already Decided\" once frozen, keeping "
           "its name\n");

    // ---- Leg 5: staging values and the gated writers ---------------------------
    {
        const ComboMMOptionDesc* checkbox = nullptr;
        const ComboMMOptionDesc* comboDesc = nullptr;
        const ComboMMOptionDesc* slider = nullptr;
        const ComboMMOptionDesc* clock = nullptr;
        for (int i = 0; i < count; i++) {
            const ComboMMOptionDesc* d = Combo_MMOptionAt(i);
            // The one clock row, Final Hours Start Time, is retired (#1229) but
            // still drawn: its HH:MM text is checked whatever its liveness.
            if (d->widget == COMBO_MM_WIDGET_TIME && clock == nullptr) {
                clock = d;
            }
            const bool live = d->liveness == COMBO_MM_LIVENESS_LIVE || d->liveness == COMBO_MM_LIVENESS_GENERATION_ONLY;
            if (!live) {
                continue;
            }
            if (d->widget == COMBO_MM_WIDGET_CHECKBOX && checkbox == nullptr) {
                checkbox = d;
            } else if (d->widget == COMBO_MM_WIDGET_COMBO && comboDesc == nullptr) {
                comboDesc = d;
            } else if (d->widget == COMBO_MM_WIDGET_SLIDER && slider == nullptr) {
                slider = d;
            }
        }
        MMRP_CHECK(checkbox && comboDesc && slider && clock, "the table lacks a live row of some widget kind");
        const bool haveRows =
            checkbox && comboDesc && slider && clock && FindByName(rows, ExpectedRowName(checkbox)) != nullptr &&
            FindByName(rows, ExpectedRowName(comboDesc)) != nullptr &&
            FindByName(rows, ExpectedRowName(slider)) != nullptr && FindByName(rows, ExpectedRowName(clock)) != nullptr;
        if (haveRows) {
            // Checkbox: refresh, edit, refuse while frozen.
            WidgetInfo& cb = *FindByName(rows, ExpectedRowName(checkbox))->info;
            bool* cbValue = std::get<bool*>(cb.valuePointer);
            Combo_MMOptionClear(checkbox);
            DrawPass(cb);
            MMRP_CHECK(*cbValue == (checkbox->defaultValue != 0), "the checkbox's staging value is not the model's");
            *cbValue = !*cbValue;
            cb.callback(cb);
            MMRP_CHECK(Combo_MMOptionGetValue(checkbox) == (*cbValue ? 1 : 0), "an unfrozen checkbox edit was lost");
            const int32_t before = Combo_MMOptionGetValue(checkbox);
            Freeze(true);
            DrawPass(cb);
            *cbValue = !*cbValue;
            cb.callback(cb);
            MMRP_CHECK(Combo_MMOptionGetValue(checkbox) == before, "a FROZEN checkbox edit reached the CVar");
            DrawPass(cb);
            MMRP_CHECK(*cbValue == (before != 0), "the next draw did not restore the frozen value");
            Freeze(false);
            Combo_MMOptionClear(checkbox);

            // Combobox: the last label, through the callback.
            WidgetInfo& co = *FindByName(rows, ExpectedRowName(comboDesc))->info;
            int32_t* coValue = std::get<int32_t*>(co.valuePointer);
            DrawPass(co);
            *coValue = (int32_t)comboDesc->valueCount - 1;
            co.callback(co);
            MMRP_CHECK(Combo_MMOptionGetValue(comboDesc) == (int32_t)comboDesc->valueCount - 1,
                       "a combobox edit did not reach the writer");
            // ---- Leg 6: an unknown value is taught, then withdrawn ------------
            auto comboOptions = std::static_pointer_cast<UIWidgets::ComboboxOptions>(co.options);
            CVarSetInteger(comboDesc->cvar, 99);
            DrawPass(co);
            MMRP_CHECK(comboOptions->comboMap.contains(99) &&
                           std::string(comboOptions->comboMap.at(99)) == "Unknown (99)",
                       "an unknown combo value was not taught as \"Unknown (99)\"; UIWidgets' map.at() would throw");
            Combo_MMOptionClear(comboDesc);
            DrawPass(co);
            MMRP_CHECK(!comboOptions->comboMap.contains(99), "the taught unknown value was not withdrawn");
            MMRP_CHECK((int)comboOptions->comboMap.size() == (int)comboDesc->valueCount,
                       "the combo map holds %zu values, the table %u", comboOptions->comboMap.size(),
                       (unsigned)comboDesc->valueCount);

            // Slider: the writer clamps, the row shows the clamp.
            WidgetInfo& sl = *FindByName(rows, ExpectedRowName(slider))->info;
            int32_t* slValue = std::get<int32_t*>(sl.valuePointer);
            DrawPass(sl);
            *slValue = slider->maxValue + 50;
            sl.callback(sl);
            DrawPass(sl);
            MMRP_CHECK(*slValue == slider->maxValue, "the slider shows %d after an over-range edit, expected %d",
                       (int)*slValue, (int)slider->maxValue);
            Combo_MMOptionClear(slider);

            // The clock row: HH:MM is the slider's text.
            WidgetInfo& tm = *FindByName(rows, ExpectedRowName(clock))->info;
            int32_t* tmValue = std::get<int32_t*>(tm.valuePointer);
            DrawPass(tm);
            *tmValue = 5 * 60 + 9;
            tm.callback(tm);
            DrawPass(tm);
            auto tmOptions = std::static_pointer_cast<UIWidgets::IntSliderOptions>(tm.options);
            MMRP_CHECK(tmOptions->format != nullptr && std::string(tmOptions->format) == "05:09",
                       "the clock row shows \"%s\", expected \"05:09\"", tmOptions->format ? tmOptions->format : "");
            Combo_MMOptionClear(clock);
        }
    }
    printf("[TEST] leg 5-6: rows refresh from the model, edit through the writers, are refused while frozen, and "
           "show an unknown combo value without throwing\n");

    // ---- Leg 7: the notes --------------------------------------------------------
    {
        // The one status sentence (#1101 decision 7), spelled independently.
        const std::string kNextWorldNote =
            "These apply to the next world you generate. Loading a paired file shows its own.";
        FlatRow* status = FindByName(rows, "MM Randomizer Status");
        FlatRow* suspended = FindByName(rows, "MM Randomizer Suspended");
        MMRP_CHECK(status != nullptr && suspended != nullptr, "the options page lacks its state notes");
        if (status != nullptr && suspended != nullptr) {
            MMRP_CHECK(status->column == 0 && suspended->column == 0, "the state notes are not in column 1");
            for (int frozen = 0; frozen <= 1; frozen++) {
                for (GameId game : { GAME_OOT, GAME_MM }) {
                    Freeze(frozen != 0);
                    Context_SetCurrentGame(game);
                    WidgetInfo s = *status->info;
                    DrawPass(s);
                    MMRP_CHECK(s.name == Combo_MMOptionsPage_StatusNote(), "the status note reads \"%s\"",
                               s.name.c_str());
                    if (!frozen) {
                        MMRP_CHECK(s.name == kNextWorldNote,
                                   "unpaired, the status note reads \"%s\", not the shared sentence \"%s\"",
                                   s.name.c_str(), kNextWorldNote.c_str());
                    }
                    WidgetInfo n = *suspended->info;
                    DrawPass(n);
                    const bool shown = !n.isHidden;
                    MMRP_CHECK(shown == (!frozen && game != GAME_MM), "the suspended note is %s (frozen %d, game %d)",
                               shown ? "shown" : "hidden", frozen, (int)game);
                    if (shown) {
                        MMRP_CHECK(n.name == Combo_MMOptionsPage_SuspendedNote(), "the suspended note reads \"%s\"",
                                   n.name.c_str());
                    }
                }
            }
        }
        std::vector<FlatRow> trickRows = FlattenRows(tricks);
        int custom = 0;
        for (FlatRow& row : trickRows) {
            custom += row.info->type == WIDGET_CUSTOM && row.info->customFunction != nullptr ? 1 : 0;
        }
        MMRP_CHECK(custom == 1, "the tricks page holds %d trick lists, expected 1", custom);
        MMRP_CHECK(FindByName(trickRows, "Majora's Mask Tricks") != nullptr, "the tricks page has no section header");
        FlatRow* tricksNote = FindByName(trickRows, "MM Tricks Status");
        MMRP_CHECK(tricksNote != nullptr, "the tricks page has no note");
        // The trick headline is a note row of its own, under the status note: the
        // shared sentence is two sentences already, a note's whole budget.
        FlatRow* tricksHeadline = FindByName(trickRows, "MM Tricks Headline");
        MMRP_CHECK(tricksHeadline != nullptr, "the tricks page has no headline row under its status note");
        if (tricksNote != nullptr) {
            // The headline, spelled independently of the model.
            char kHeadline[128];
            snprintf(kHeadline, sizeof(kHeadline), "%d of %d tricks are supported by the randomizer logic so far.",
                     Combo_MMOptionsPage_SettableTrickCount(), Combo_MMTrickCount());
            for (int frozen = 0; frozen <= 1; frozen++) {
                Freeze(frozen != 0);
                char expected[256];
                Combo_MMOptionsPage_TricksNote(expected, sizeof(expected));
                WidgetInfo t = *tricksNote->info;
                DrawPass(t);
                MMRP_CHECK(t.name == expected, "the tricks note reads \"%s\", the model \"%s\"", t.name.c_str(),
                           expected);
                if (!frozen) {
                    MMRP_CHECK(t.name == kNextWorldNote,
                               "unpaired, the tricks note reads \"%s\", not the shared sentence \"%s\"", t.name.c_str(),
                               kNextWorldNote.c_str());
                }
                if (tricksHeadline != nullptr) {
                    WidgetInfo h = *tricksHeadline->info;
                    DrawPass(h);
                    // Unpaired, the honest headline; frozen, nothing is settable,
                    // so the freeze sentence alone speaks (as before #1101).
                    MMRP_CHECK(h.isHidden == (frozen != 0), "the trick headline is %s (frozen %d)",
                               h.isHidden ? "hidden" : "shown", frozen);
                    if (!frozen) {
                        MMRP_CHECK(h.name == kHeadline, "the trick headline reads \"%s\", expected \"%s\"",
                                   h.name.c_str(), kHeadline);
                    }
                }
            }
        }
        // UiSnapshot's R6, held here ROM-free: every note on both pages is at
        // most two sentences and 200 characters, in every state it is drawn in.
        auto sentences = [](const std::string& text) {
            int n = 0;
            for (size_t i = 0; i < text.size(); i++) {
                if ((text[i] == '.' || text[i] == '!' || text[i] == '?') &&
                    (i + 1 == text.size() || text[i + 1] == ' ')) {
                    n++;
                }
            }
            return n;
        };
        for (int frozen = 0; frozen <= 1; frozen++) {
            for (GameId game : { GAME_OOT, GAME_MM }) {
                Freeze(frozen != 0);
                Context_SetCurrentGame(game);
                for (SidebarEntry* page : { &options, &tricks }) {
                    for (FlatRow& row : FlattenRows(*page)) {
                        if (row.info->type != WIDGET_TEXT) {
                            continue;
                        }
                        WidgetInfo n = *row.info;
                        DrawPass(n);
                        if (n.isHidden) {
                            continue;
                        }
                        MMRP_CHECK(sentences(n.name) <= 2 && n.name.size() <= 200,
                                   "the note \"%s\" is %d sentence(s) and %zu characters (frozen %d, game %d); a "
                                   "note is at most two sentences and 200 characters",
                                   n.name.c_str(), sentences(n.name), n.name.size(), frozen, (int)game);
                    }
                }
            }
        }
    }
    printf("[TEST] leg 7: the status, suspended and tricks notes are the model's sentences in every state\n");

    // ---- Leg 8: the race lockout, as SoH's Randomizer pages have it ------------
    {
        int locked = 0;
        for (SidebarEntry* page : { &options, &tricks }) {
            for (FlatRow& row : FlattenRows(*page)) {
                if (IsInteractiveRow(*row.info)) {
                    MMRP_CHECK(row.info->raceDisable,
                               "the row \"%s\" ignores SoH's race lockout; SoH's Randomizer > General rows obey it",
                               row.info->name.c_str());
                    locked += row.info->raceDisable ? 1 : 0;
                } else if (row.info->type == WIDGET_TEXT) {
                    MMRP_CHECK(!row.info->raceDisable,
                               "the note \"%s\" is disabled by the race lockout; a note is not a setting",
                               row.info->name.c_str());
                }
            }
        }
        MMRP_CHECK(locked == count + kTrapRowCount + 1,
                   "%d rows obey the race lockout, expected every option row, trap row and Reset (%d)", locked,
                   count + kTrapRowCount + 1);
    }
    printf("[TEST] leg 8: every option row, trap row and Reset obeys SoH's race lockout; every note stays legible\n");

    // ---- Leg 9: the trap-type rows write through the gated writer (#1101 R4) ---
    {
        Freeze(false);
        auto trapKeysOn = []() {
            int on = 0;
            for (int u = 0; u < kTrapRowCount; u++) {
                on |= CVarGetInteger(kTrapRows[u].cvar, 0) != 0 ? (1 << u) : 0;
            }
            return on;
        };
        for (int u = 0; u < kTrapRowCount; u++) {
            CVarClear(kTrapRows[u].cvar);
        }
        int landed = 0;
        for (int t = 0; t < kTrapRowCount; t++) {
            FlatRow* row = FindByName(rows, std::string(kTrapRows[t].label) + "##MMRando");
            if (row == nullptr || row->info->type != WIDGET_CHECKBOX || row->info->callback == nullptr) {
                continue;
            }
            WidgetInfo& cb = *row->info;
            bool* value = std::get<bool*>(cb.valuePointer);
            DrawPass(cb);
            MMRP_CHECK(!*value, "the trap row %s starts on with every key clear", kTrapRows[t].label);
            *value = true;
            cb.callback(cb);
            MMRP_CHECK(trapKeysOn() == (1 << t), "turning on %s left the trap keys at %02X, expected %02X (%s alone)",
                       kTrapRows[t].label, (unsigned)trapKeysOn(), (unsigned)(1 << t), kTrapRows[t].cvar);
            landed += trapKeysOn() == (1 << t) ? 1 : 0;
            // Frozen: the edit is refused and the next draw restores the model.
            Freeze(true);
            DrawPass(cb);
            *value = false;
            cb.callback(cb);
            MMRP_CHECK(CVarGetInteger(kTrapRows[t].cvar, 0) == 1, "a FROZEN edit of %s reached %s", kTrapRows[t].label,
                       kTrapRows[t].cvar);
            DrawPass(cb);
            MMRP_CHECK(*value, "the next draw did not restore %s's frozen value", kTrapRows[t].label);
            Freeze(false);
            DrawPass(cb);
            *value = false;
            cb.callback(cb);
            MMRP_CHECK(trapKeysOn() == 0, "turning %s off again left the trap keys at %02X", kTrapRows[t].label,
                       (unsigned)trapKeysOn());
        }
        MMRP_CHECK(landed == kTrapRowCount, "%d of the %d trap rows wrote their own key", landed, kTrapRowCount);
        // Reset clears the trap types with the options and tricks.
        for (int u = 0; u < kTrapRowCount; u++) {
            CVarSetInteger(kTrapRows[u].cvar, 1);
        }
        Combo_MMOptionsPage_ResetAll();
        MMRP_CHECK(trapKeysOn() == 0, "Reset MM Randomizer left the trap keys at %02X", (unsigned)trapKeysOn());
        for (int u = 0; u < kTrapRowCount; u++) {
            CVarClear(kTrapRows[u].cvar);
        }
    }
    printf("[TEST] leg 9: each trap-type row writes its own key through the gated writer, is refused while frozen, "
           "and Reset clears all seven\n");

    // ---- Leg 10: Randomizer > MM Locations (#1101 lane R3) -------------------
    if (!rando.sidebars.contains(COMBO_MM_LOCATIONS_PAGE_NAME)) {
        MMRP_CHECK(false, "the Randomizer header has no \"%s\" page: MM's excluded checks have no surface",
                   COMBO_MM_LOCATIONS_PAGE_NAME);
    } else {
        SidebarEntry& locations = rando.sidebars.at(COMBO_MM_LOCATIONS_PAGE_NAME);
        MMRP_CHECK(!devTools.sidebars.contains(COMBO_MM_LOCATIONS_PAGE_NAME),
                   "the Dev Tools header holds \"%s\" as well", COMBO_MM_LOCATIONS_PAGE_NAME);
        MMRP_CHECK(locations.columnCount == 1, "the locations page declares %u columns, expected 1 (Locations')",
                   locations.columnCount);
        std::vector<FlatRow> locationRows = FlattenRows(locations);
        int custom = 0;
        int interactive = 0;
        for (FlatRow& row : locationRows) {
            MMRP_CHECK(row.column == 0, "the locations page row \"%s\" sits in column %u of a one-column page",
                       row.info->name.c_str(), row.column);
            custom += row.info->type == WIDGET_CUSTOM && row.info->customFunction != nullptr ? 1 : 0;
            interactive += IsInteractiveRow(*row.info) ? 1 : 0;
        }
        MMRP_CHECK(custom == 1, "the locations page holds %d tables, expected 1 (SoH's Locations is one custom row)",
                   custom);
        MMRP_CHECK(interactive == 0,
                   "the locations page holds %d interactive rows outside its table; SoH's Locations holds none",
                   interactive);
        FlatRow* note = FindByName(locationRows, "MM Locations Status");
        MMRP_CHECK(note != nullptr, "the locations page has no status note");
        if (note != nullptr) {
            MMRP_CHECK(!note->info->raceDisable, "the locations note is disabled by the race lockout");
            for (int frozen = 0; frozen <= 1; frozen++) {
                Freeze(frozen != 0);
                const std::string expected = Combo_MMLocationsPage_StatusNote();
                WidgetInfo n = *note->info;
                DrawPass(n);
                MMRP_CHECK(n.name == expected, "the locations note reads \"%s\", the model \"%s\"", n.name.c_str(),
                           expected.c_str());
                MMRP_CHECK(frozen != 0 || n.name == COMBO_RANDO_PAGES_NEXT_WORLD_NOTE,
                           "unpaired, the locations note \"%s\" is not the shared sentence", n.name.c_str());
            }
            Freeze(false);
        }
    }
    printf("[TEST] leg 10: MM Locations is one column: the model's note, a header and the one table\n");

    ComboContext_Init();
    Context_SetCurrentGame(prevGame);
    if (gFailures == 0) {
        printf("[TEST] PASS: Randomizer > MM Randomizer and Randomizer > MM Tricks carry MM's whole option table "
               "and trick list behind the same gates the pop-out window had\n");
    }
    return gFailures == 0 ? 0 : 1;
}

#endif // RSBS_SINGLE_EXECUTABLE
