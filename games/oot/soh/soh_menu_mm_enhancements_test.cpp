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
 *   3. A ROW IS REGISTERED PAST THE COLUMN COUNT, or in the wrong column. The
 *      page declares three columns (UI parity M3: the column measure of its
 *      reference, Enhancements > Quality of Life) and `Menu::DrawElement`
 *      iterates `columnCount` columns, so a row in a higher one is registered and
 *      never drawn. The toggles fill the first column; the first pointer row
 *      opens the second and the rows after it stay there, so a pointer and the
 *      rows gated on it are read together. The third is empty, for the measure.
 *
 *   9. A TOOLTIP LEAVES SoH's VOICE (docs/ui-style-guide.md R-TT2, R-TT4,
 *      R-TT6). Every own-row tooltip opens with a present-tense verb, is at most
 *      two sentences, and ends with the "Majora's Mask only." caveat. The
 *      runtime lint counts none of that, so leg 6 does.
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
 *  10. (#747) A NON-LIVE ROW NAMES NO TRACKER, or its state is legible only on
 *      hover. Every manifest row carries `issue` nonzero exactly when it is not
 *      Live (the capability registry's `{gate, issue}` split: the number is
 *      recorded, never drawn), and every row the page draws DISABLED sits in a
 *      group that SHOWS one gray note -- the part of the state a player reads
 *      without hovering and a race lockout cannot replace. Every shipped row is
 *      Live, so leg 7 drives both through the page's own builder with a
 *      synthetic table built from leg 4's reasons, and holds the shipped page to
 *      "no note shown" (nothing visible changes while every row is live). The
 *      note sits directly above the group's rows (after a pointer row's
 *      sentence), and a table that lists a pointer FIRST draws no note under the
 *      then-empty heading (7d).
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
#include <cstdarg>
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
// #747: the page's body over a manifest table (the shipped page passes
// RSBS::kHostedMmEnhancements), and its group note's sentence. Leg 7 builds a
// page from a SYNTHETIC table through the same function the shipped page uses.
void AddMmEnhancementRows(SohMenu& menu, WidgetPath& path, const RSBS::HostedMmEnhancement* rows, std::size_t count);
const char* MmEnhancementsGroupNoteText();
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
// ApplyPresentation call the production PreFunc makes. Their reasons are player
// text in SoH's disabled-reason style (ADR 0004's 2026-09-27 amendment): short
// Title Case fragments with no tracker number, because this leg locks the
// tooltip EXACTLY and a number here would lock a tracker into the pixels.
constexpr const char* kSyntheticPartialReason = "Draw Leg Not Wired in Majora's Mask";
constexpr const char* kSyntheticDormantReason = "Provider Not in This Build";

/** Does @p text print a tracker number ("#" then a digit)? SoH's disabled
 *  reasons never do (ADR 0004's 2026-09-27 amendment). */
bool PrintsIssueNumber(const char* text) {
    for (const char* p = text; p != nullptr && *p != '\0'; p++) {
        if (p[0] == '#' && p[1] >= '0' && p[1] <= '9') {
            return true;
        }
    }
    return false;
}

/** SoH's disabled tooltip around @p reason, exactly as MenuDrawItem builds one. */
std::string SohDisabledShape(const char* reason) {
    return std::string("This setting is disabled because: \n\n- ") + reason;
}

// ---- Leg 7 (#747): a synthetic manifest with non-live rows -----------------
// Static storage, because the page's PreFuncs keep pointers into the table for
// the life of the menu. Two groups, the shipped page's shape: the heading group
// holds a Live and a Dormant checkbox; the pointer row opens the second, which
// holds a Partial slider gated on the pointer's key, so the note there must
// follow the gate (a hidden row draws nothing to hover). The reasons are leg 4's.
constexpr const char* kSynthLiveKey = "gEnhancements.MenuLockSynthetic747.Live";
constexpr const char* kSynthDormantKey = "gEnhancements.MenuLockSynthetic747.Dormant";
constexpr const char* kSynthParentKey = "gEnhancements.MenuLockSynthetic747.Parent";
constexpr const char* kSynthPartialKey = "gEnhancements.MenuLockSynthetic747.Partial";
constexpr uint32_t kSynthIssue = 747;

constexpr RSBS::HostedMmEnhancement kSyntheticManifest[] = {
    { kSynthLiveKey, "Synthetic Live Row", "Toggles a synthetic live setting. Majora's Mask only.", "synthetic",
      nullptr, "", RSBS::MmEnhancementHosting::OwnRow, RSBS::MmEnhancementLiveness::Live, "" },
    { kSynthDormantKey, "Synthetic Dormant Row", "Toggles a synthetic dormant setting. Majora's Mask only.",
      "synthetic", nullptr, "", RSBS::MmEnhancementHosting::OwnRow, RSBS::MmEnhancementLiveness::Dormant,
      kSyntheticDormantReason, RSBS::MmEnhancementWidget::Checkbox, 0, 0, 0, nullptr, nullptr, kSynthIssue },
    { kSynthParentKey, "Synthetic Parent", "Synthetic Parent is elsewhere in the menu.", "synthetic", nullptr, "",
      RSBS::MmEnhancementHosting::HostedElsewhere, RSBS::MmEnhancementLiveness::Live, "" },
    { kSynthPartialKey, "Synthetic Partial Row: %d", "Sets a synthetic partial value. Majora's Mask only.", "synthetic",
      nullptr, "", RSBS::MmEnhancementHosting::OwnRow, RSBS::MmEnhancementLiveness::Partial, kSyntheticPartialReason,
      RSBS::MmEnhancementWidget::SliderInt, 1, 10, 5, "%d", kSynthParentKey, kSynthIssue },
};
constexpr std::size_t kSyntheticManifestCount = sizeof(kSyntheticManifest) / sizeof(kSyntheticManifest[0]);

// Leg 7d: a pointer listed FIRST, so the heading group is empty and the only
// non-live row sits in the pointer's group.
constexpr const char* kSynthFirstParentKey = "gEnhancements.MenuLockSynthetic747.FirstParent";
constexpr const char* kSynthFirstPartialKey = "gEnhancements.MenuLockSynthetic747.FirstPartial";
constexpr RSBS::HostedMmEnhancement kPointerFirstManifest[] = {
    { kSynthFirstParentKey, "Synthetic First Parent", "Synthetic First Parent is elsewhere in the menu.", "synthetic",
      nullptr, "", RSBS::MmEnhancementHosting::HostedElsewhere, RSBS::MmEnhancementLiveness::Live, "" },
    { kSynthFirstPartialKey, "Synthetic First Partial Row: %d", "Sets a synthetic partial value. Majora's Mask only.",
      "synthetic", nullptr, "", RSBS::MmEnhancementHosting::OwnRow, RSBS::MmEnhancementLiveness::Partial,
      kSyntheticPartialReason, RSBS::MmEnhancementWidget::SliderInt, 1, 10, 5, "%d", kSynthFirstParentKey,
      kSynthIssue },
};
constexpr std::size_t kPointerFirstManifestCount = sizeof(kPointerFirstManifest) / sizeof(kPointerFirstManifest[0]);
static_assert(RSBS::HostedMmEnhancementRowsAreHonest(kPointerFirstManifest, kPointerFirstManifestCount),
              "leg 7d's pointer-first table must itself pass the manifest's honesty rule");

/** What CheckGroupNotes saw on one page. */
struct GroupNoteCensus {
    std::size_t disabledRows = 0; ///< interactive rows drawn disabled
    std::size_t notesShown = 0;   ///< group notes drawn (not hidden)
    std::size_t violations = 0;   ///< breaches of #747's rule, reported or not
};

/** One breach of #747's rule: counted always, printed as a FAIL only when the
 *  caller is checking a page that must pass (the negative control counts a
 *  page that must NOT pass, silently). */
void NoteViolation(GroupNoteCensus& census, bool report, const char* fmt, ...) {
    census.violations++;
    if (!report) {
        return;
    }
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("[TEST] FAIL(10): %s\n", buf);
    gFailures++;
}

/**
 * #747's rule over a built page, as MenuDrawItem would draw it right now: every
 * interactive row drawn DISABLED sits in a group -- the rows after one
 * SEPARATOR_TEXT, within one column -- that shows exactly one group note,
 * directly above the group's rows (after every other text row of the group,
 * such as a pointer row's sentence, and before its first interactive row: style
 * guide R-S3's "one gray note row above the group"), gray, .RaceDisable(false)
 * and .HideInSearch(true); and a shown note sits over a group with a disabled row
 * in it (a note over nothing disabled is the same lie the other way, and what a
 * note under an empty heading is). Every row gets one draw pass first, so each
 * PreFunc has run.
 */
GroupNoteCensus CheckGroupNotes(SidebarEntry& page, const char* where, bool report) {
    GroupNoteCensus census;
    const std::string note = SohGui::MmEnhancementsGroupNoteText();
    for (uint32_t column = 0; column < page.columnWidgets.size(); column++) {
        auto& widgets = page.columnWidgets.at(column);
        for (WidgetInfo& row : widgets) {
            DrawPass(row);
        }
        std::string group = "(before any separator)";
        std::size_t interactiveSeen = 0;
        std::size_t notes = 0;
        std::size_t disabled = 0;
        std::string firstDisabled;
        auto closeGroup = [&]() {
            if (disabled != 0 && notes != 1) {
                NoteViolation(census, report,
                              "%s: group \"%s\" draws %zu row(s) disabled (first \"%s\") and shows %zu group "
                              "note(s); a disabled row's state must be legible without hovering, in exactly one gray "
                              "note",
                              where, group.c_str(), disabled, firstDisabled.c_str(), notes);
            }
            if (notes != 0 && disabled == 0) {
                NoteViolation(census, report,
                              "%s: group \"%s\" shows a group note over no disabled row; the note says \"Hover "
                              "one\" over nothing",
                              where, group.c_str());
            }
            census.disabledRows += disabled;
            census.notesShown += notes;
        };
        for (std::size_t at = 0; at < widgets.size(); at++) {
            WidgetInfo& row = widgets.at(at);
            if (row.type == WIDGET_SEPARATOR_TEXT) {
                closeGroup();
                group = row.name;
                interactiveSeen = 0;
                notes = 0;
                disabled = 0;
                firstDisabled.clear();
                continue;
            }
            if (!row.isHidden && row.type == WIDGET_TEXT && row.name == note) {
                notes++;
                // Directly above the group's rows: nothing interactive before it,
                // and the next registered row is one of the rows it describes
                // (not a pointer's sentence, another note, a separator or the
                // column's end).
                const bool hasNext = at + 1 < widgets.size();
                const bool nextIsRow = hasNext && widgets.at(at + 1).type != WIDGET_TEXT &&
                                       widgets.at(at + 1).type != WIDGET_SEPARATOR_TEXT;
                if (interactiveSeen != 0 || !nextIsRow) {
                    const std::string next = hasNext ? "\"" + widgets.at(at + 1).name + "\"" : "the end of the column";
                    NoteViolation(census, report,
                                  "%s: group \"%s\"'s note follows %zu of its rows and is followed by %s; it belongs "
                                  "directly above the group's rows (style guide R-S3), after any pointer sentence",
                                  where, group.c_str(), interactiveSeen, next.c_str());
                }
                if (row.raceDisable || !row.hideInSearch) {
                    NoteViolation(census, report,
                                  "%s: group \"%s\"'s note has RaceDisable %d and HideInSearch %d; a gray note is "
                                  ".RaceDisable(false) and .HideInSearch(true) (R-S3a)",
                                  where, group.c_str(), (int)row.raceDisable, (int)row.hideInSearch);
                }
                auto text =
                    row.options != nullptr ? std::static_pointer_cast<UIWidgets::TextOptions>(row.options) : nullptr;
                if (text == nullptr || text->color != UIWidgets::Colors::Gray) {
                    NoteViolation(census, report, "%s: group \"%s\"'s note is not drawn Gray", where, group.c_str());
                }
            } else if (!row.isHidden && row.type != WIDGET_TEXT && IsDisabled(row)) {
                disabled++;
                if (firstDisabled.empty()) {
                    firstDisabled = row.name;
                }
            }
            if (row.type != WIDGET_TEXT) {
                interactiveSeen++;
            }
        }
        closeGroup();
    }
    return census;
}

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
                MME_CHECK(page.columnCount == 3,
                          "the MM enhancement page declares %u columns, expected 3: Quality of Life's count, so its "
                          "rows have that page's width (the toggles in the first, the Autosave group in the second)",
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
    // Which column each row belongs in, from the manifest: the first pointer
    // row's label opens the second column, and nothing before it may be there.
    const char* firstPointerLabel = nullptr;
    for (std::size_t i = 0; i < RSBS::kHostedMmEnhancementCount && firstPointerLabel == nullptr; i++) {
        if (RSBS::kHostedMmEnhancements[i].hosting == RSBS::MmEnhancementHosting::HostedElsewhere) {
            firstPointerLabel = RSBS::kHostedMmEnhancements[i].label;
        }
    }
    bool inSecondGroup = false;
    std::size_t perColumn[2] = { 0, 0 };
    for (FlatRow& row : rows) {
        if (firstPointerLabel != nullptr && row.info->type == WIDGET_SEPARATOR_TEXT &&
            row.info->name == firstPointerLabel) {
            inSecondGroup = true;
        }
        const uint32_t expected = inSecondGroup ? (uint32_t)SECTION_COLUMN_2 : (uint32_t)SECTION_COLUMN_1;
        MME_CHECK(row.column == expected,
                  "row \"%s\" is registered in column %u, expected %u: the toggles fill the first column and the "
                  "first pointer row opens the second (a row past the page's two columns is never drawn)",
                  row.info->name.c_str(), row.column, expected);
        if (row.column < 2) {
            perColumn[row.column]++;
        }
    }
    MME_CHECK(perColumn[0] > 0 && perColumn[1] > 0,
              "the page's columns hold %zu and %zu widgets; an empty column in a multi-column page is #640's failure "
              "mode",
              perColumn[0], perColumn[1]);
    printf("[TEST] leg 1: the page is registered through the extension point, declares three columns and holds %zu "
           "widgets (%zu toggles side, %zu Autosave side)\n",
           rows.size(), perColumn[0], perColumn[1]);

    // ---- Leg 6: every own-row tooltip is in SoH's voice -----------------------
    // R-TT2 (present tense, verb first: SoH's "Makes...", "Allows...",
    // "Toggles..." -- a third-person verb, so the first word ends in 's'),
    // R-TT4 (one or two sentences, the trailing caveat included) and R-TT6 (the
    // caveat trails). A pointer row's text is a gray note, not a tooltip, and
    // is not held to this.
    {
        const std::string kCaveat = " Majora's Mask only.";
        for (std::size_t i = 0; i < RSBS::kHostedMmEnhancementCount; i++) {
            const RSBS::HostedMmEnhancement& e = RSBS::kHostedMmEnhancements[i];
            if (e.hosting != RSBS::MmEnhancementHosting::OwnRow) {
                continue;
            }
            const std::string tip = e.tooltip != nullptr ? e.tooltip : "";
            const std::string firstWord = tip.substr(0, tip.find(' '));
            MME_CHECK(!firstWord.empty() && firstWord.back() == 's',
                      "\"%s\"'s tooltip opens with \"%s\", not a present-tense verb (R-TT2: \"Makes...\", "
                      "\"Toggles...\")",
                      e.key, firstWord.c_str());
            std::size_t sentences = 0;
            for (std::size_t at = tip.find(". "); at != std::string::npos; at = tip.find(". ", at + 2)) {
                sentences++;
            }
            if (!tip.empty() && tip.back() == '.') {
                sentences++;
            }
            MME_CHECK(sentences >= 1 && sentences <= 2,
                      "\"%s\"'s tooltip is %zu sentences; SoH's are one or two, the trailing caveat included (R-TT4)",
                      e.key, sentences);
            MME_CHECK(
                tip.size() > kCaveat.size() && tip.compare(tip.size() - kCaveat.size(), kCaveat.size(), kCaveat) == 0,
                "\"%s\"'s tooltip does not end with \"%s\" (R-TT6: the caveat trails)", e.key, kCaveat.c_str() + 1);
        }
        printf("[TEST] leg 6: every own-row tooltip opens with a verb, is at most two sentences and ends with the "
               "Majora's Mask caveat\n");
    }

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
        // Every manifest reason, whatever its hosting: it is drawn after "- " in
        // SoH's disabled tooltip, so it is player text and prints no tracker
        // number (ADR 0004's 2026-09-27 amendment). The manifest has no separate
        // issue field yet (#747); until it does, a non-live row names its
        // tracker in a source comment beside the entry, never in the string.
        MME_CHECK(!PrintsIssueNumber(desc.reason),
                  "the manifest reason for \"%s\" prints a tracker number (\"%s\"); it is drawn in the disabled "
                  "tooltip, and no SoH disabled reason carries one",
                  desc.key, desc.reason != nullptr ? desc.reason : "(null)");
        // #747: the tracker lives in the row's own field instead, nonzero
        // exactly when the row is not Live. BELT AND BRACES, not a lock: the
        // same rule is a static_assert on this very table
        // (HostedMmEnhancementsAreHonest() in cvar_shared_keys.h), so a
        // shipped row that broke it would fail the build before this ran, and
        // this check can never be seen red. The rule's observed red halves are
        // leg 7a's, over synthetic tables.
        MME_CHECK((desc.issue != 0) == (desc.liveness != RSBS::MmEnhancementLiveness::Live),
                  "the manifest row \"%s\" is %s with issue %u; a non-live row records the issue that retires it, "
                  "and a Live row records none",
                  desc.key, desc.liveness == RSBS::MmEnhancementLiveness::Live ? "Live" : "not Live",
                  (unsigned)desc.issue);
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
            MME_CHECK(tip != nullptr && std::string(tip) == SohDisabledShape(desc.reason),
                      "the row on \"%s\" carries disabled tooltip \"%s\", expected SoH's disabled shape around the "
                      "manifest reason \"%s\"",
                      desc.key, tip != nullptr ? tip : "(null)", desc.reason);
            MME_CHECK(row->info->name == beforeName,
                      "the row on \"%s\" is classified non-live and its name changed from \"%s\" to \"%s\"; the state "
                      "belongs in the tooltip and the group's gray note",
                      desc.key, beforeName.c_str(), row->info->name.c_str());
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
            // SoH's disabled row (ADR 0004's 2026-09-27 amendment): MenuDrawItem's
            // own tooltip shape around the reason, and the row's own name.
            const std::string wantTip = SohDisabledShape(c.reason);
            MME_CHECK(tip != nullptr && std::string(tip) == wantTip,
                      "the synthetic non-live row \"%s\" carries disabled tooltip \"%s\", expected \"%s\"", c.name,
                      tip != nullptr ? tip : "(null)", wantTip.c_str());
            MME_CHECK(row->info->name == std::string(c.name) && afterOne == std::string(c.name),
                      "the synthetic non-live row's NAME changed (\"%s\", then \"%s\"). SoH never writes a state or an "
                      "explanation into an interactive row's name; the state belongs in the tooltip and the group's "
                      "gray note",
                      afterOne.c_str(), row->info->name.c_str());
        }
    }
    printf("[TEST] leg 4: a Partial and a Dormant row both render disabled, with SoH's disabled tooltip around their "
           "reason, and keep their names\n");

    // ---- Leg 7 (#747): the issue field and the group note ----------------------
    {
        // 7a. The honesty rule over a table, both halves. The synthetic table is
        // honest; a copy whose non-live row drops its issue is not, and neither
        // is one whose Live row carries an issue. Evaluated at run time through
        // the same constexpr function the shipped table's static_assert uses.
        MME_CHECK(RSBS::HostedMmEnhancementRowsAreHonest(kSyntheticManifest, kSyntheticManifestCount),
                  "the synthetic manifest (non-live rows WITH an issue) is refused by the honesty rule");
        RSBS::HostedMmEnhancement noIssue[kSyntheticManifestCount];
        RSBS::HostedMmEnhancement liveWithIssue[kSyntheticManifestCount];
        for (std::size_t i = 0; i < kSyntheticManifestCount; i++) {
            noIssue[i] = kSyntheticManifest[i];
            liveWithIssue[i] = kSyntheticManifest[i];
        }
        noIssue[1].issue = 0;         // Dormant, no issue
        liveWithIssue[0].issue = 747; // Live, with an issue
        MME_CHECK(!RSBS::HostedMmEnhancementRowsAreHonest(noIssue, kSyntheticManifestCount),
                  "the honesty rule accepts a Dormant row with issue 0; a reason nobody can trace is a reason nobody "
                  "retires");
        MME_CHECK(!RSBS::HostedMmEnhancementRowsAreHonest(liveWithIssue, kSyntheticManifestCount),
                  "the honesty rule accepts a Live row that records an issue");
        printf("[TEST] leg 7a: the honesty rule requires an issue exactly on the non-live rows, both directions\n");

        // 7-control. The check's own red half, observed on every run: a group
        // whose row is drawn disabled with NO note under its separator -- what
        // the page drew before #747 the day a row went non-live -- must be
        // counted as a breach. Built the way leg 4's synthetic rows are (the
        // presentation call, no builder), so no note is registered.
        {
            MmEnhancementMenuProbe controlProbe;
            WidgetPath controlPath = { "Combo", "Synthetic Noteless Page", SECTION_COLUMN_1 };
            controlProbe.AddMenuEntry("Combo", "gSettings.Menu.ComboSidebarSection");
            controlProbe.AddSidebarEntry("Combo", controlPath.sidebarName, 1);
            controlProbe.AddWidget(controlPath, "Noteless Group", WIDGET_SEPARATOR_TEXT);
            controlProbe.AddWidget(controlPath, "Synthetic Noteless Row", WIDGET_CVAR_CHECKBOX)
                .CVar("gEnhancements.MenuLockSyntheticRow")
                .RaceDisable(false)
                .Options(UIWidgets::CheckboxOptions().Tooltip("synthetic"))
                .PreFunc([](WidgetInfo& i) {
                    SohGui::SohMenu::ApplyPresentation(i, i.name, SohGui::SOH_MENU_PRESENT_CAPABILITY,
                                                       kSyntheticDormantReason);
                });
            SidebarEntry& controlPage = controlProbe.Entries().at("Combo").sidebars.at(controlPath.sidebarName);
            GroupNoteCensus control = CheckGroupNotes(controlPage, "noteless control page", false);
            MME_CHECK(control.disabledRows == 1 && control.violations == 1,
                      "the noteless control page counted %zu disabled row(s) and %zu breach(es), expected 1 and 1: "
                      "the group-note check cannot see a disabled row with no note, so its green proves nothing",
                      control.disabledRows, control.violations);
            printf("[TEST] leg 7-control: the group-note check counts a disabled row with no note as a breach (red "
                   "half: %zu breach)\n",
                   control.violations);
        }

        // 7b. The shipped page: every row Live, so no row is drawn disabled and
        // no group note is shown -- the page looks as it did before #747.
        {
            SidebarEntry& shipped = sidebars.at(pageName);
            GroupNoteCensus census = CheckGroupNotes(shipped, pageName, true);
            MME_CHECK(census.disabledRows == 0 && census.notesShown == 0,
                      "the shipped page draws %zu disabled row(s) and %zu group note(s); every manifest row is Live, "
                      "so it draws neither",
                      census.disabledRows, census.notesShown);
            printf("[TEST] leg 7b: the shipped page draws no disabled row and no group note\n");
        }

        // 7c. A page built from the synthetic table through the page's own
        // builder. The gate key is set explicitly both ways and put back.
        MmEnhancementMenuProbe noteProbe;
        WidgetPath notePath = { "Combo", "Synthetic Group Note Page", SECTION_COLUMN_1 };
        noteProbe.AddMenuEntry("Combo", "gSettings.Menu.ComboSidebarSection");
        noteProbe.AddSidebarEntry("Combo", notePath.sidebarName, 3);
        SohGui::AddMmEnhancementRows(noteProbe, notePath, kSyntheticManifest, kSyntheticManifestCount);
        SidebarEntry& notePage = noteProbe.Entries().at("Combo").sidebars.at(notePath.sidebarName);

        const int savedGate = CVarGetInteger(kSynthParentKey, INT32_MIN);

        // Gate OFF: the Partial slider is hidden, so only the heading group has a
        // disabled row, and only its note shows.
        CVarSetInteger(kSynthParentKey, 0);
        GroupNoteCensus off = CheckGroupNotes(notePage, "synthetic page, gate off", true);
        MME_CHECK(off.disabledRows == 1 && off.notesShown == 1,
                  "gate off: %zu disabled row(s) and %zu note(s) shown, expected 1 and 1 (the Dormant row and its "
                  "group's note; the gated Partial row is hidden and its group's note with it)",
                  off.disabledRows, off.notesShown);

        // Gate ON: the Partial slider draws disabled, and its group's note shows.
        CVarSetInteger(kSynthParentKey, 1);
        GroupNoteCensus on = CheckGroupNotes(notePage, "synthetic page, gate on", true);
        MME_CHECK(on.disabledRows == 2 && on.notesShown == 2,
                  "gate on: %zu disabled row(s) and %zu note(s) shown, expected 2 and 2 (one per group)",
                  on.disabledRows, on.notesShown);

        // The disabled rows are the builder's, not leg 4's hand-made ones: SoH's
        // disabled shape around the manifest reason, and the row's own name.
        std::vector<FlatRow> noteRows = FlattenRows(notePage);
        for (const RSBS::HostedMmEnhancement& e : kSyntheticManifest) {
            if (e.hosting != RSBS::MmEnhancementHosting::OwnRow || e.liveness == RSBS::MmEnhancementLiveness::Live) {
                continue;
            }
            FlatRow* row = FindRowByCVar(noteRows, e.key);
            if (row == nullptr) {
                printf("[TEST] FAIL(10): the builder drew no row for the synthetic \"%s\"\n", e.key);
                gFailures++;
                continue;
            }
            const char* tip = DisabledTooltipOf(*row->info);
            MME_CHECK(IsDisabled(*row->info) && tip != nullptr && std::string(tip) == SohDisabledShape(e.reason),
                      "the builder's non-live row on \"%s\" is %s with tooltip \"%s\"", e.key,
                      IsDisabled(*row->info) ? "disabled" : "ENABLED", tip != nullptr ? tip : "(null)");
            MME_CHECK(row->info->name == e.label, "the builder's non-live row on \"%s\" was renamed to \"%s\"", e.key,
                      row->info->name.c_str());
        }

        if (savedGate == INT32_MIN) {
            CVarClear(kSynthParentKey);
        } else {
            CVarSetInteger(kSynthParentKey, savedGate);
        }
        printf("[TEST] leg 7c: a page built from a synthetic non-live table shows one gray note per group with a "
               "disabled row (%zu with the gate off, %zu with it on), directly above the group's rows, and none over "
               "a group with nothing disabled\n",
               off.notesShown, on.notesShown);

        // 7d. A table that lists a POINTER FIRST. Nothing forbids that order
        // (neither the honesty rule nor leg 5), and it leaves the heading group
        // empty: the pointer opens the second column with its own group. The
        // heading must then draw no note -- one there would sit under an empty
        // heading over nothing disabled, while the pointer's group, in the other
        // column, carries the real one.
        {
            MmEnhancementMenuProbe firstProbe;
            WidgetPath firstPath = { "Combo", "Synthetic Pointer First Page", SECTION_COLUMN_1 };
            firstProbe.AddMenuEntry("Combo", "gSettings.Menu.ComboSidebarSection");
            firstProbe.AddSidebarEntry("Combo", firstPath.sidebarName, 3);
            SohGui::AddMmEnhancementRows(firstProbe, firstPath, kPointerFirstManifest, kPointerFirstManifestCount);
            SidebarEntry& firstPage = firstProbe.Entries().at("Combo").sidebars.at(firstPath.sidebarName);

            const int savedFirstGate = CVarGetInteger(kSynthFirstParentKey, INT32_MIN);
            CVarSetInteger(kSynthFirstParentKey, 1);
            GroupNoteCensus first = CheckGroupNotes(firstPage, "pointer-first page, gate on", true);
            MME_CHECK(first.disabledRows == 1 && first.notesShown == 1 && first.violations == 0,
                      "pointer-first page: %zu disabled row(s), %zu note(s) shown and %zu breach(es), expected 1, 1 "
                      "and 0 (the pointer group's note only; none under the empty heading)",
                      first.disabledRows, first.notesShown, first.violations);
            std::size_t headingNotes = 0;
            for (const WidgetInfo& row : firstPage.columnWidgets.at(0)) {
                if (!row.isHidden && row.type == WIDGET_TEXT && row.name == SohGui::MmEnhancementsGroupNoteText()) {
                    headingNotes++;
                }
            }
            MME_CHECK(headingNotes == 0,
                      "pointer-first page: the empty heading column shows %zu group note(s); its group holds no row",
                      headingNotes);
            if (savedFirstGate == INT32_MIN) {
                CVarClear(kSynthFirstParentKey);
            } else {
                CVarSetInteger(kSynthFirstParentKey, savedFirstGate);
            }
            printf("[TEST] leg 7d: a table listing a pointer first draws its note in the pointer's group only (%zu "
                   "note, none under the empty heading)\n",
                   first.notesShown);
        }
    }

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
