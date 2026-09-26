/**
 * @file soh_menu_mm_enhancements_test.cpp
 * @brief ROM-free, display-free lock for #682's PRESENTATION half: the curated
 *        MM enhancement page exists in the tier-4 Combo section, and every row
 *        on it is bound to the manifest key it claims, in the manifest's class.
 *
 * CTest row MenuMmEnhancementRows in CMake/SingleExecutable.cmake, dispatch
 * "menu-mm-enhancement-rows" in src/common/test_runner.cpp.
 *
 * WHY A SECOND ROW ALONGSIDE MenuComboSection AND MMEnhancementToggles. The three
 * measure three different things, and each failure this row catches is silent in
 * the other two:
 *
 *   - MenuComboSection owns the section's SHAPE and the extension point's
 *     MECHANICS, driven with a synthetic page. It would stay green if the
 *     production page never registered at all.
 *   - MMEnhancementToggles owns the MM-side LIVENESS EVIDENCE: that each key's
 *     provider linked, that its registrar runs, and that writing the key re-arms
 *     it. It would stay green if no widget anywhere wrote the key — which is
 *     precisely the state #682 was filed against, and #499's failure before it.
 *   - This row is the join: the page is registered, and each row's `.CVar` is the
 *     manifest key, so the widget the player clicks writes the key the provider
 *     reads. A typo'd literal is a control that flips a key nothing consults, and
 *     nothing about it is a compile error or a crash.
 *
 * WHAT IT ASSERTS, and why each one is a real failure mode:
 *
 *   1. THE PAGE IS NOT THERE. The registrar is a file-scope initializer in
 *      SohGui/SohMenuComboMmEnhancements.cpp. All three OoT archives are
 *      WHOLE_ARCHIVE'd (#341/#640) so it should not be elidable, but "should not"
 *      is what #516 and #640 both said. A missing page is a missing surface with
 *      no diagnostic at all.
 *
 *   2. A ROW IS BOUND TO THE WRONG KEY, or to none. `WIDGET_CVAR_CHECKBOX` with a
 *      null or misspelled `.CVar` draws and writes nothing the provider reads.
 *
 *   3. A ROW IS REGISTERED PAST THE COLUMN COUNT. The page declares one column
 *      and `Menu::DrawElement` iterates `columnCount` columns, so a row in a
 *      higher one is registered and never drawn.
 *
 *   4. THE PAGE IS EMPTY, or the pointer row is the only thing on it. #640: an
 *      empty multi-column page leaves `SetNextWindowPos` unconsumed and undocks
 *      libultraship's "Main Game" window.
 *
 *   5. A NON-LIVE ROW DRAWS AS LIVE. ADR 0004 section 5's whole point: "a toggle
 *      that does nothing is worse than no toggle" (#682). Every row the manifest
 *      classifies Partial or Dormant must come out of its PreFunc DISABLED, with
 *      its reason in the disabled tooltip. Every Live row must come out of a
 *      draw pass with NO presentation suffix on its name and not disabled.
 *
 *   6. THE DISABLED PATH IS NEVER EXERCISED. All five shipped rows are Live
 *      today, so leg 4 below installs SYNTHETIC Partial and Dormant rows through
 *      the same code path and asserts the presentation it produces. Without that,
 *      #682's non-live branch would be untested code discovered on the day
 *      somebody adds a dormant toggle — and what it renders is the one thing ADR
 *      0004 section 5 exists to get right.
 *
 *   7. (#693) A NON-BOOLEAN KEY GETS A CHECKBOX, or a slider whose range or
 *      default drifted from the manifest. MM's autosave interval is minutes; a
 *      checkbox over it writes 1 or 0 (a one-minute interval, or one that saves
 *      on every eligible frame), and a slider default that differs from the
 *      provider's read default makes "reset" and "never touched" disagree.
 *
 *   8. (#693) A GATED ROW IGNORES ITS GATE, or hides for good. The interval is
 *      moot while Autosave is off, so its row must be hidden then and visible,
 *      enabled and unrenamed when Autosave is on — driven through the real
 *      PreFunc with the gate key really written, both ways, and the gate key put
 *      back afterwards. And it sits AFTER the row that accounts for its gate, so
 *      the player reads "Autosave lives there" before the slider that tunes it.
 *
 * WHAT IT DOES NOT COVER. Whether an enabled toggle changes the game. That needs
 * a ROM, a play state and an operator; the ROM-free half of it is
 * MMEnhancementToggles' registry-content evidence, and the rest is a playtest.
 * Appearance is operator verification on the nightly.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "soh/SohGui/SohMenu.h"

#include "cvar_shared_keys.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include <climits>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace SohGui {
// The page's registrar and its registered sidebar name, both externally linked
// and declared in no header — the same shape AddCrossGamePointerWidgets uses.
// This row reaches the registrar THROUGH AddMenuCombo() rather than by calling
// it, so the real extension point is what is exercised; the name accessor exists
// so the test cannot drift from the registration by spelling the literal twice.
const char* MmEnhancementsPageName();
} // namespace SohGui

namespace {

int gFailures = 0;

#define MME_CHECK(cond, ...)                                               \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            printf("[TEST]       ");                                       \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
            gFailures++;                                                   \
        }                                                                  \
    } while (0)

class MmEnhancementMenuProbe final : public SohGui::SohMenu {
  public:
    MmEnhancementMenuProbe() : SohGui::SohMenu("", "MM Enhancements Probe") {
    }

    std::unordered_map<std::string, MainMenuEntry>& Entries() {
        return menuEntries;
    }
};

/** Every widget on `page`, flattened, with the column it was registered in. */
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

/** The row whose `.CVar` is exactly `cVar`, or NULL. */
FlatRow* FindRowByCVar(std::vector<FlatRow>& rows, const char* cVar) {
    for (FlatRow& row : rows) {
        if (row.info->cVar != nullptr && std::string(row.info->cVar) == cVar) {
            return &row;
        }
    }
    return nullptr;
}

/** The index in `rows` of the row whose `.CVar` is `cVar` or whose NAME is
 *  `name` (either may be NULL), or -1. Leg 5's ordering check. */
long IndexOfRow(std::vector<FlatRow>& rows, const char* cVar, const char* name) {
    for (std::size_t i = 0; i < rows.size(); i++) {
        WidgetInfo* info = rows[i].info;
        if (cVar != nullptr && info->cVar != nullptr && std::string(info->cVar) == cVar) {
            return (long)i;
        }
        if (name != nullptr && info->name == name) {
            return (long)i;
        }
    }
    return -1;
}

/** The row whose registered NAME is exactly `name`, or NULL. */
FlatRow* FindRowByName(std::vector<FlatRow>& rows, const std::string& name) {
    for (FlatRow& row : rows) {
        if (row.info->name == name) {
            return &row;
        }
    }
    return nullptr;
}

/**
 * Run a row's PreFunc the way MenuDrawItem does: ResetDisables() first, because
 * that is what makes a gate applied anywhere ELSE than a PreFunc useless, and
 * what forces ApplyPresentation to be idempotent in its base name.
 */
void DrawPass(WidgetInfo& info) {
    info.ResetDisables();
    if (info.preFunc != nullptr) {
        info.preFunc(info);
    }
}

// The presentation state lands in the row's OPTIONS, not on WidgetInfo: `disabled`
// and `disabledTooltip` are WidgetOptions members, and every options alternative
// derives from WidgetOptions, so the base pointer is where all of them agree.
// SohMenu::AddWidget always installs one, which is also why ResetDisables() can
// dereference it unconditionally.
const char* DisabledTooltipOf(WidgetInfo& info) {
    return info.options == nullptr ? nullptr : info.options->disabledTooltip;
}

bool IsDisabled(WidgetInfo& info) {
    return info.options != nullptr && info.options->disabled;
}

// ---- Synthetic rows for leg 4 ----------------------------------------------
// The manifest's four rows are all Live, so the non-live presentation path has no
// production data to exercise it. These two drive it through the SAME
// ApplyPresentation call the production PreFunc makes.
constexpr const char* kSyntheticPartialReason = "Partially available: its draw leg has no MM dispatch point (#438)";
constexpr const char* kSyntheticDormantReason = "Not yet available: its provider is elided from this build (#427)";

} // namespace

extern "C" int OoT_MenuMmEnhancementRows_RunHeadless(void) {
    printf("[TEST] menu-mm-enhancement-rows: the curated MM enhancement page, its rows' CVar bindings and their ADR "
           "0004 section 5 presentation (#682)\n");

    gFailures = 0;

    const char* pageName = SohGui::MmEnhancementsPageName();
    if (pageName == nullptr || pageName[0] == '\0') {
        printf("[TEST] FAIL(1): the MM enhancement page has no registered name\n");
        return 1;
    }

    // ---- Leg 1: the page exists, through the real extension point -----------
    // Registered by a file-scope initializer, so it is already in the registry;
    // asserting that here is leg 1's first half, and it is exact against the
    // elision class (#516/#640) that would drop the initializer silently.
    {
        bool registered = false;
        for (const SohGui::ComboSectionPage& page : SohGui::GetComboSectionPages()) {
            if (page.sidebarName == pageName) {
                registered = true;
                MME_CHECK(page.columnCount == 1,
                          "the MM enhancement page declares %u columns; it registers rows in the first one only, and "
                          "Menu::DrawElement iterates columnCount columns",
                          page.columnCount);
                MME_CHECK(page.registrar != nullptr, "the MM enhancement page registered a null registrar");
            }
        }
        if (!registered) {
            printf("[TEST] FAIL(1): no contributed Combo page named \"%s\" is registered -- "
                   "SohGui/SohMenuComboMmEnhancements.cpp's file-scope registrar did not run, which is exactly the "
                   "state #682 was filed against (MM's enhancement toggles reachable only from "
                   "shipofharkinian.json)\n",
                   pageName);
            return 1;
        }
    }

    MmEnhancementMenuProbe probe;
    probe.AddMenuCombo();

    auto& entries = probe.Entries();
    if (!entries.contains("Combo")) {
        printf("[TEST] FAIL(1): AddMenuCombo registered no \"Combo\" section at all\n");
        return 1;
    }
    auto& sidebars = entries.at("Combo").sidebars;
    if (!sidebars.contains(pageName)) {
        printf("[TEST] FAIL(1): AddMenuCombo created no sidebar for the registered page \"%s\"\n", pageName);
        return 1;
    }

    SidebarEntry& page = sidebars.at(pageName);
    std::vector<FlatRow> rows = FlattenRows(page);

    // #640, verbatim: an empty page undocks the "Main Game" window. And a page
    // holding ONLY the heading separator is the same thing with a label on it.
    MME_CHECK(rows.size() >= RSBS::kHostedMmEnhancementCount + 1,
              "the MM enhancement page holds %zu widgets; %zu manifest rows plus a heading is the minimum", rows.size(),
              RSBS::kHostedMmEnhancementCount + (std::size_t)1);
    for (FlatRow& row : rows) {
        MME_CHECK(row.column == (uint32_t)SECTION_COLUMN_1,
                  "row \"%s\" is registered in column %u; the page draws one column, so anything past the first is "
                  "registered and never drawn",
                  row.info->name.c_str(), row.column);
    }
    printf("[TEST] leg 1: the page is registered through the extension point, declares one column and holds %zu "
           "widgets\n",
           rows.size());

    // ---- Leg 2: every manifest key has a row, bound to that key -------------
    for (std::size_t i = 0; i < RSBS::kHostedMmEnhancementCount; i++) {
        const RSBS::HostedMmEnhancement& desc = RSBS::kHostedMmEnhancements[i];

        if (desc.hosting == RSBS::MmEnhancementHosting::HostedElsewhere) {
            // A pointer row, not a writer. Two assertions, both load-bearing:
            // the label is on the page (so the player can find out where the
            // control is), and NOTHING on this page binds the key (a second
            // writer over one key is the duplicate-host shape #655 removed).
            MME_CHECK(FindRowByName(rows, desc.label) != nullptr,
                      "no row named \"%s\" -- the key is hosted elsewhere in the menu and this page is supposed to "
                      "say so",
                      desc.label);
            MME_CHECK(FindRowByCVar(rows, desc.key) == nullptr,
                      "a row on this page binds \"%s\", which is already written by another row in the unified menu. "
                      "Two widgets over one key is the only way for two surfaces to disagree about it",
                      desc.key);
            continue;
        }

        FlatRow* row = FindRowByCVar(rows, desc.key);
        if (row == nullptr) {
            printf("[TEST] FAIL(2): no row on \"%s\" is bound to \"%s\" (manifest row %zu, \"%s\"). A widget whose "
                   "CVar drifts from its provider flips a key nothing consults -- #499's failure, and invisible\n",
                   pageName, desc.key, i, desc.label);
            gFailures++;
            continue;
        }
        MME_CHECK(row->info->name == desc.label ||
                      row->info->name == SohGui::SohMenu::StripPresentationSuffix(row->info->name),
                  "the row on \"%s\" is named \"%s\"", desc.key, row->info->name.c_str());
        if (desc.widget == RSBS::MmEnhancementWidget::SliderInt) {
            // #693: an integer key gets an integer slider, over the manifest's
            // range, with the manifest's default — which leg 6 of
            // MMEnhancementToggles ties to the provider's own read default.
            MME_CHECK(row->info->type == WIDGET_CVAR_SLIDER_INT,
                      "the row on \"%s\" is widget type %d, not WIDGET_CVAR_SLIDER_INT; the provider reads an integer "
                      "(minutes), and a checkbox over it writes 1 or 0",
                      desc.key, (int)row->info->type);
            // WidgetOptions is not polymorphic, so the downcast is static and
            // guarded by the widget type (SohMenu::AddWidget installs the
            // options alternative matching the type the row was added with).
            std::shared_ptr<UIWidgets::IntSliderOptions> slider =
                row->info->type == WIDGET_CVAR_SLIDER_INT && row->info->options != nullptr
                    ? std::static_pointer_cast<UIWidgets::IntSliderOptions>(row->info->options)
                    : nullptr;
            if (slider == nullptr) {
                printf("[TEST] FAIL(7): the slider on \"%s\" carries no IntSliderOptions\n", desc.key);
                gFailures++;
            } else {
                MME_CHECK(slider->min == desc.sliderMin && slider->max == desc.sliderMax,
                          "the slider on \"%s\" spans [%d, %d], the manifest says [%d, %d]", desc.key, slider->min,
                          slider->max, desc.sliderMin, desc.sliderMax);
                MME_CHECK(slider->defaultValue == desc.sliderDefault,
                          "the slider on \"%s\" defaults to %d, the manifest (and the provider's read default) says "
                          "%d",
                          desc.key, slider->defaultValue, desc.sliderDefault);
                MME_CHECK(slider->format != nullptr && std::string(slider->format) == desc.sliderFormat,
                          "the slider on \"%s\" formats with \"%s\", the manifest says \"%s\"", desc.key,
                          slider->format != nullptr ? slider->format : "(null)", desc.sliderFormat);
            }
        } else {
            MME_CHECK(row->info->type == WIDGET_CVAR_CHECKBOX,
                      "the row on \"%s\" is widget type %d, not WIDGET_CVAR_CHECKBOX; these keys are read with "
                      "CVarGetInteger as booleans",
                      desc.key, (int)row->info->type);
        }
    }
    printf("[TEST] leg 2: every manifest key is bound by exactly the row its hosting class calls for\n");

    // ---- Leg 3: a Live row draws live ---------------------------------------
    for (std::size_t i = 0; i < RSBS::kHostedMmEnhancementCount; i++) {
        const RSBS::HostedMmEnhancement& desc = RSBS::kHostedMmEnhancements[i];
        if (desc.hosting != RSBS::MmEnhancementHosting::OwnRow) {
            continue;
        }
        FlatRow* row = FindRowByCVar(rows, desc.key);
        if (row == nullptr) {
            continue; // already reported by leg 2
        }

        // Two passes, not one. ApplyPresentation is called from a PreFunc with
        // info.name as its own base, so a non-idempotent implementation
        // compounds the suffix — the defect #497's own lane found and fixed. One
        // pass cannot see it; two can.
        const std::string beforeName = row->info->name;
        DrawPass(*row->info);
        DrawPass(*row->info);

        if (desc.liveness == RSBS::MmEnhancementLiveness::Live) {
            MME_CHECK(!IsDisabled(*row->info),
                      "the row on \"%s\" is classified Live but came out of a draw pass DISABLED; a live control that "
                      "explains why it is unavailable looks broken and is not",
                      desc.key);
            MME_CHECK(row->info->name == beforeName,
                      "the row on \"%s\" is classified Live but its name changed from \"%s\" to \"%s\" across a draw "
                      "pass",
                      desc.key, beforeName.c_str(), row->info->name.c_str());
        } else {
            MME_CHECK(IsDisabled(*row->info),
                      "the row on \"%s\" is classified %s but came out of a draw pass ENABLED. ADR 0004 section 5's "
                      "whole point is that a toggle which does nothing is worse than no toggle",
                      desc.key, desc.liveness == RSBS::MmEnhancementLiveness::Partial ? "Partial" : "Dormant");
            const char* tip = DisabledTooltipOf(*row->info);
            MME_CHECK(tip != nullptr && std::string(tip) == desc.reason,
                      "the row on \"%s\" carries disabled tooltip \"%s\", expected the manifest reason \"%s\"",
                      desc.key, tip != nullptr ? tip : "(null)", desc.reason);
        }
    }
    printf("[TEST] leg 3: each own-row key's presentation matches its manifest liveness class, twice over\n");

    // ---- Leg 5 (#693): a gated row follows its gate, both ways ----------------
    // Numbered after leg 4 in the header's list of failure modes (7 and 8), but
    // run here, while `rows` still points into this probe's page.
    std::size_t gatedRows = 0;
    for (std::size_t i = 0; i < RSBS::kHostedMmEnhancementCount; i++) {
        const RSBS::HostedMmEnhancement& desc = RSBS::kHostedMmEnhancements[i];
        if (desc.shownWhileKey == nullptr) {
            continue;
        }
        gatedRows++;
        FlatRow* row = FindRowByCVar(rows, desc.key);
        if (row == nullptr) {
            continue; // already reported by leg 2
        }

        // Ordering: the row accounting for the gate (its own control, or the
        // pointer row's label) comes first on the page.
        const char* gateLabel = nullptr;
        for (std::size_t j = 0; j < RSBS::kHostedMmEnhancementCount; j++) {
            if (std::string(RSBS::kHostedMmEnhancements[j].key) == desc.shownWhileKey) {
                gateLabel = RSBS::kHostedMmEnhancements[j].label;
            }
        }
        const long gateAt = IndexOfRow(rows, desc.shownWhileKey, gateLabel);
        const long rowAt = IndexOfRow(rows, desc.key, nullptr);
        MME_CHECK(gateAt >= 0 && rowAt > gateAt,
                  "the row on \"%s\" is at page index %ld and the row accounting for its gate \"%s\" at %ld; the "
                  "gated row must come after it",
                  desc.key, rowAt, desc.shownWhileKey, gateAt);

        // INT32_MIN marks "absent": the restore below clears rather than
        // writing a 0 nobody set, the discipline MMEnhancementToggles leg 6
        // follows (PR #730 review).
        const int savedGate = CVarGetInteger(desc.shownWhileKey, INT32_MIN);
        const std::string beforeName = row->info->name;

        CVarSetInteger(desc.shownWhileKey, 0);
        DrawPass(*row->info);
        MME_CHECK(row->info->isHidden,
                  "with its gate \"%s\" OFF, the row on \"%s\" is drawn; the setting is moot while its parent feature "
                  "is off",
                  desc.shownWhileKey, desc.key);

        CVarSetInteger(desc.shownWhileKey, 1);
        DrawPass(*row->info);
        DrawPass(*row->info);
        MME_CHECK(!row->info->isHidden, "with its gate \"%s\" ON, the row on \"%s\" is still hidden",
                  desc.shownWhileKey, desc.key);
        if (desc.liveness == RSBS::MmEnhancementLiveness::Live) {
            MME_CHECK(!IsDisabled(*row->info), "with its gate \"%s\" ON, the Live row on \"%s\" is disabled",
                      desc.shownWhileKey, desc.key);
            MME_CHECK(row->info->name == beforeName,
                      "the gated Live row on \"%s\" was renamed from \"%s\" to \"%s\"; hiding is not a presentation "
                      "state and must not suffix the name",
                      desc.key, beforeName.c_str(), row->info->name.c_str());
        }

        CVarSetInteger(desc.shownWhileKey, 0);
        DrawPass(*row->info);
        MME_CHECK(row->info->isHidden,
                  "the row on \"%s\" stayed visible after its gate \"%s\" went back OFF; the gate is latched rather "
                  "than re-evaluated per draw",
                  desc.key, desc.shownWhileKey);

        // Back as found: the gate key (absent stays absent), and the widget's
        // isHidden re-derived from it by one more real draw, so the page is not
        // left hidden by this leg's last OFF pass.
        if (savedGate == INT32_MIN) {
            CVarClear(desc.shownWhileKey);
        } else {
            CVarSetInteger(desc.shownWhileKey, savedGate);
        }
        DrawPass(*row->info);
    }
    MME_CHECK(gatedRows >= 1,
              "no manifest row carries a gate; #693's interval row is gated on gEnhancements.Autosave, so this leg ran "
              "on nothing");
    printf("[TEST] leg 5: %zu gated row(s) hide with their gate off, show enabled and unrenamed with it on, re-hide "
           "when it goes off again, and sit after the row that accounts for the gate\n",
           gatedRows);

    // ---- Leg 4: the non-live path, driven with synthetic rows ---------------
    // The shipped manifest is all-Live, so without this leg #682's disabled
    // branch is code nobody has run. These drive ApplyPresentation exactly as the
    // production PreFunc does.
    {
        MmEnhancementMenuProbe synthProbe;
        WidgetPath synthPath = { "Combo", "Synthetic Liveness Page", SECTION_COLUMN_1 };
        synthProbe.AddMenuEntry("Combo", "gSettings.Menu.ComboSidebarSection");
        synthProbe.AddSidebarEntry("Combo", synthPath.sidebarName, 1);

        struct SynthCase {
            const char* name;
            const char* reason;
        };
        const SynthCase cases[] = { { "Synthetic Partial Row", kSyntheticPartialReason },
                                    { "Synthetic Dormant Row", kSyntheticDormantReason } };

        for (const SynthCase& c : cases) {
            const char* reason = c.reason;
            WidgetInfo& info = synthProbe.AddWidget(synthPath, c.name, WIDGET_CVAR_CHECKBOX)
                                   .CVar("gEnhancements.MenuLockSyntheticRow")
                                   .RaceDisable(false)
                                   .Options(UIWidgets::CheckboxOptions().Tooltip("synthetic"));
            info.PreFunc([reason](WidgetInfo& i) {
                SohGui::SohMenu::ApplyPresentation(i, i.name, SohGui::SOH_MENU_PRESENT_CAPABILITY, reason);
            });
        }

        SidebarEntry& synthPage = synthProbe.Entries().at("Combo").sidebars.at(synthPath.sidebarName);
        std::vector<FlatRow> synthRows = FlattenRows(synthPage);
        for (const SynthCase& c : cases) {
            FlatRow* row = FindRowByName(synthRows, c.name);
            if (row == nullptr) {
                printf("[TEST] FAIL(4): the synthetic row \"%s\" was not registered\n", c.name);
                gFailures++;
                continue;
            }
            DrawPass(*row->info);
            const std::string afterOne = row->info->name;
            DrawPass(*row->info);

            MME_CHECK(IsDisabled(*row->info), "the synthetic non-live row \"%s\" came out of a draw pass ENABLED",
                      c.name);
            const char* tip = DisabledTooltipOf(*row->info);
            MME_CHECK(tip != nullptr && std::string(tip) == c.reason,
                      "the synthetic non-live row \"%s\" carries disabled tooltip \"%s\", expected \"%s\"", c.name,
                      tip != nullptr ? tip : "(null)", c.reason);
            MME_CHECK(row->info->name != std::string(c.name),
                      "the synthetic non-live row's NAME is unchanged (\"%s\"). ADR 0004's rule is that a row a player "
                      "must read without hovering says its state in the name, not only in a tooltip",
                      row->info->name.c_str());
            MME_CHECK(row->info->name == afterOne,
                      "the synthetic non-live row's name grew across two draw passes: \"%s\" then \"%s\". "
                      "ResetDisables() does not clear `name`, so a PreFunc that appended rather than normalising "
                      "through StripPresentationSuffix compounds every frame",
                      afterOne.c_str(), row->info->name.c_str());
            MME_CHECK(SohGui::SohMenu::StripPresentationSuffix(row->info->name) == std::string(c.name),
                      "stripping the presentation suffix from \"%s\" does not give back \"%s\"",
                      row->info->name.c_str(), c.name);
        }
    }
    printf("[TEST] leg 4: a Partial and a Dormant row both render disabled, with their reason, and their names do not "
           "compound across frames\n");

    if (gFailures == 0) {
        printf("[TEST] menu-mm-enhancement-rows: PASS\n");
    } else {
        printf("[TEST] menu-mm-enhancement-rows: %d failure(s)\n", gFailures);
    }
    return gFailures;
}

#else

#include <cstdio>

/**
 * The contributed-page registry, the Combo section and the manifest's whole
 * point (MM's menu TU is excluded) exist only in the single-exe build. A
 * standalone SoH build has no MM half to configure, so the row reports pass
 * rather than failing to link. test_runner.cpp declares this unconditionally.
 */
extern "C" int OoT_MenuMmEnhancementRows_RunHeadless(void) {
    printf("[TEST] menu-mm-enhancement-rows: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
