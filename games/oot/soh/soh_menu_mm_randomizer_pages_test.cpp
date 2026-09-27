/**
 * @file soh_menu_mm_randomizer_pages_test.cpp
 * @brief ROM-free, display-free lock for Combo > MM Randomizer and Combo > MM
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
 *   1. THE PAGES ARE NOT THERE, OR NOT TOGETHER. Both register from a file-scope
 *      initializer beside the Majora's Mask page's (SohMenuComboMmEnhancements.cpp),
 *      so the sidebar reads Majora's Mask, MM Randomizer, MM Tricks. The options
 *      page declares two columns (Randomizer > General's), the tricks page one
 *      (Randomizer > Tricks/Glitches').
 *
 *   2. AN OPTION IS LOST OR DOUBLED. The options page's interactive rows are
 *      EXACTLY MM's descriptor table: one row per descriptor, of the widget type
 *      the descriptor names, under the name the page derives from its label (the
 *      "##MMRando" id suffix on checkboxes and comboboxes, SoH's "Name: %d" on
 *      sliders), and nothing else interactive but Reset.
 *
 *   3. A ROW LANDS IN THE WRONG COLUMN, or in one the page never draws, or
 *      outside its group's header.
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
 *      is the model's headline.
 *
 * Appearance is UiSnapshot's (the pages' captures beside Randomizer > General
 * and Randomizer > Tricks/Glitches).
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "soh/SohGui/SohMenu.h"

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
    printf("[TEST] menu-mm-randomizer-pages: MM's randomizer options and tricks are Combo pages whose rows are the "
           "descriptor table, in the model's columns and states, writing only through the gated writers\n");
    gFailures = 0;
    const GameId prevGame = Context_GetCurrentGame();
    ComboContext_Init();

    MmRandomizerPagesProbe probe;
    probe.AddMenuCombo();
    auto& entries = probe.Entries();
    if (!entries.contains("Combo")) {
        printf("[TEST] FAIL(1): AddMenuCombo registered no Combo section\n");
        return 1;
    }
    MainMenuEntry& combo = entries.at("Combo");

    // ---- Leg 1: the pages, their column counts, their place -----------------
    if (!combo.sidebars.contains(COMBO_MM_OPTIONS_PAGE_NAME) || !combo.sidebars.contains(COMBO_MM_TRICKS_PAGE_NAME)) {
        printf("[TEST] FAIL(1): the Combo section has no \"%s\" and/or \"%s\" page: MM's randomizer options are "
               "unreachable from the menu\n",
               COMBO_MM_OPTIONS_PAGE_NAME, COMBO_MM_TRICKS_PAGE_NAME);
        return 1;
    }
    SidebarEntry& options = combo.sidebars.at(COMBO_MM_OPTIONS_PAGE_NAME);
    SidebarEntry& tricks = combo.sidebars.at(COMBO_MM_TRICKS_PAGE_NAME);
    MMRP_CHECK(options.columnCount == COMBO_MM_OPTIONS_PAGE_COLUMNS,
               "the options page declares %u columns, expected %d (Randomizer > General's)", options.columnCount,
               COMBO_MM_OPTIONS_PAGE_COLUMNS);
    MMRP_CHECK(tricks.columnCount == 1, "the tricks page declares %u columns, expected 1 (Tricks/Glitches')",
               tricks.columnCount);
    {
        long mm = -1, rando = -1, trick = -1;
        for (std::size_t i = 0; i < combo.sidebarOrder.size(); i++) {
            if (combo.sidebarOrder[i] == "Majora's Mask") {
                mm = (long)i;
            } else if (combo.sidebarOrder[i] == COMBO_MM_OPTIONS_PAGE_NAME) {
                rando = (long)i;
            } else if (combo.sidebarOrder[i] == COMBO_MM_TRICKS_PAGE_NAME) {
                trick = (long)i;
            }
        }
        MMRP_CHECK(mm >= 0 && rando == mm + 1 && trick == rando + 1,
                   "the sidebar order is Majora's Mask at %ld, MM Randomizer at %ld, MM Tricks at %ld; the MM pages "
                   "must sit together in that order",
                   mm, rando, trick);
    }
    printf("[TEST] leg 1: both pages are registered with their column counts, right under Majora's Mask\n");

    // ---- Leg 2: the row set equals the descriptor table ---------------------
    std::vector<FlatRow> rows = FlattenRows(options);
    const int count = Combo_MMOptionCount();
    MMRP_CHECK(count > 0, "no MM option table is registered after AddMenuCombo");
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
        MMRP_CHECK(interactive == count + 1,
                   "the options page holds %d interactive rows, expected the %d descriptors plus Reset: an option is "
                   "lost or doubled",
                   interactive, count);
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
    printf("[TEST] leg 2-3: the options page's rows are the %d descriptors, once each, in the model's columns under "
           "their groups' headers\n",
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
            } else if (d->widget == COMBO_MM_WIDGET_TIME && clock == nullptr) {
                clock = d;
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
        if (tricksNote != nullptr) {
            for (int frozen = 0; frozen <= 1; frozen++) {
                Freeze(frozen != 0);
                char expected[256];
                Combo_MMOptionsPage_TricksNote(expected, sizeof(expected));
                WidgetInfo t = *tricksNote->info;
                DrawPass(t);
                MMRP_CHECK(t.name == expected, "the tricks note reads \"%s\", the model \"%s\"", t.name.c_str(),
                           expected);
            }
        }
    }
    printf("[TEST] leg 7: the status, suspended and tricks notes are the model's sentences in every state\n");

    ComboContext_Init();
    Context_SetCurrentGame(prevGame);
    if (gFailures == 0) {
        printf("[TEST] PASS: Combo > MM Randomizer and Combo > MM Tricks carry MM's whole option table and trick list "
               "behind the same gates the pop-out window had\n");
    }
    return gFailures == 0 ? 0 : 1;
}

#endif // RSBS_SINGLE_EXECUTABLE
