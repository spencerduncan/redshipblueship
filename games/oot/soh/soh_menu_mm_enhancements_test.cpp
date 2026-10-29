/**
 * @file soh_menu_mm_enhancements_test.cpp
 * @brief ROM-free, display-free lock for #682's PRESENTATION half: every curated
 *        MM enhancement row is on the Ship of Harkinian page that holds its twin
 *        (#1133, ruling D5), bound to the manifest key it claims, in the
 *        manifest's class; and the MM rows other lanes host on SoH's pages.
 *
 * CTest row MenuMmEnhancementRows in CMake/SingleExecutable.cmake, dispatch
 * "menu-mm-enhancement-rows" in src/common/test_runner.cpp.
 *
 * WHY A SECOND ROW ALONGSIDE MenuComboSection AND MMEnhancementToggles. The three
 * measure three different things, and each failure this row catches is silent in
 * the other two:
 *
 *   - MenuComboSection owns the menu's SHAPE (no Combo header, the hosts the
 *     former Combo pages moved to, the carry) and the extension points.
 *   - MMEnhancementToggles owns the MM-side LIVENESS EVIDENCE: that each key's
 *     provider linked, that its registrar runs, and that writing the key re-arms
 *     it. It would stay green if no widget anywhere wrote the key — which is
 *     precisely the state #682 was filed against, and #499's failure before it.
 *   - This row is the join: each row is on its SoH page, and its `.CVar` is the
 *     manifest key, so the widget the player clicks writes the key the provider
 *     reads. A typo'd literal is a control that flips a key nothing consults, and
 *     nothing about it is a compile error or a crash.
 *
 * WHAT IT ASSERTS, and why each one is a real failure mode:
 *
 *   1. A ROW HAS NO HOST, or the wrong one (#1133). The Combo header's Majora's
 *      Mask page retired; each of its rows is hosted on SoH's own Enhancements
 *      pages through the hosted-row seam (a one-line call in SoH's builder, the
 *      #1037 shape), directly after its twin or nearest relative, or in a
 *      "Majora's Mask" block for the rows with none. Leg 1 builds SoH's own
 *      AddMenuEnhancements and holds every curated row to its page, separator
 *      and neighbour, recorded by the seam (so the UI snapshot captures it); and
 *      the converged Autosave key to ONE row, SoH's, which says it applies to
 *      both games (#1119's form) -- the retired page's pointer to it is gone.
 *
 *   2. A ROW IS BOUND TO THE WRONG KEY, or to none. `WIDGET_CVAR_CHECKBOX` with a
 *      null or misspelled `.CVar` draws and writes nothing the provider reads.
 *
 *   9. A TOOLTIP LEAVES SoH's VOICE (docs/ui-style-guide.md R-TT2, R-TT4,
 *      R-TT6). Every own-row tooltip opens with a present-tense verb, is at most
 *      two sentences, and ends with the "Majora's Mask only." caveat. The
 *      runtime lint counts none of that, so leg 6 does.
 *
 *   5. A NON-LIVE ROW DRAWS AS LIVE. ADR 0004 section 5's whole point: "a toggle
 *      that does nothing is worse than no toggle" (#682). Every row the manifest
 *      classifies Partial or Dormant must come out of its PreFunc DISABLED, with
 *      its reason in the disabled tooltip. Every Live row must come out of a
 *      draw pass with NO presentation suffix on its name and not disabled.
 *
 *   6. THE DISABLED PATH IS NEVER EXERCISED. All shipped rows are Live today, so
 *      leg 4 installs SYNTHETIC Partial and Dormant rows through the same code
 *      path and asserts the presentation it produces.
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
 *      back afterwards. And it sits AFTER the row that writes its gate, on the
 *      same page.
 *
 *  10. (#747) A NON-LIVE ROW NAMES NO TRACKER, or its state is legible only on
 *      hover. Every manifest row carries `issue` nonzero exactly when it is not
 *      Live, and every row drawn DISABLED sits in a group that SHOWS one gray
 *      note. Every shipped row is Live, so leg 7 drives both through the block
 *      builder with a synthetic table built from leg 4's reasons, and holds the
 *      shipped rows to "none disabled, no note" (SoH's pages look as SoH ships
 *      them, plus the rows). A pointer row (a key whose control is SoH's own)
 *      draws nothing.
 *
 *  11. (#1235) MM's AMBIENCE VOLUME HAS NO CONTROL. Leg 8 builds SoH's own
 *      Settings page and requires the ambience slider right after Sound Effects
 *      Volume, in SoH's slider shape, bound to the shared key.
 *
 *  12. (#1233) A HOSTED ROW IS NOT WHERE THE SEAM RECORDED IT. Leg 9 walks the
 *      hosted-row registry over SoH's Settings, Enhancements and Dev Tools.
 *
 * WHAT IT DOES NOT COVER. Whether an enabled toggle changes the game. That needs
 * a ROM, a play state and an operator; the ROM-free half of it is
 * MMEnhancementToggles' registry-content evidence, and the rest is a playtest.
 * Appearance is operator verification on the nightly.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "soh/SohGui/SohMenu.h"
#include "soh/SohGui/HostedMm/SohMenuHostedMm.h"

#include "cvar_shared_keys.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace SohGui {
// #747: the curated rows' block builder over a manifest table (the shipped block on Quality of Life is drawn
// from the same row and note helpers, through the hosted-row seam), and its group note's sentence. Leg 7 builds a
// block from a SYNTHETIC table. Both externally linked and declared in no header, as AddComboRulesWidgets is.
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

/** Every widget on `page`, flattened, with the column it was registered in, its page and the SEPARATOR_TEXT
 *  above it in its column ("" when none). */
struct FlatRow {
    WidgetInfo* info;
    uint32_t column;
    std::string sidebar;
    std::string section;
};

std::vector<FlatRow> FlattenRows(SidebarEntry& page, const std::string& sidebar = std::string()) {
    std::vector<FlatRow> rows;
    for (uint32_t column = 0; column < page.columnWidgets.size(); column++) {
        std::string section;
        for (WidgetInfo& row : page.columnWidgets.at(column)) {
            if (row.type == WIDGET_SEPARATOR_TEXT) {
                section = row.name;
            }
            rows.push_back(FlatRow{ &row, column, sidebar, section });
        }
    }
    return rows;
}

/** Every widget under `header`, page by page in sidebar order. */
std::vector<FlatRow> FlattenHeader(MainMenuEntry& header) {
    std::vector<FlatRow> rows;
    for (const std::string& sidebar : header.sidebarOrder) {
        std::vector<FlatRow> page = FlattenRows(header.sidebars.at(sidebar), sidebar);
        rows.insert(rows.end(), page.begin(), page.end());
    }
    return rows;
}

/** The row registered before `row` in its page and column, skipping TEXT rows (a gray note is not a neighbour), or
 *  NULL when `row` opens its column. */
const WidgetInfo* PreviousInteractive(const std::vector<FlatRow>& rows, const FlatRow* row) {
    const WidgetInfo* previous = nullptr;
    for (const FlatRow& r : rows) {
        if (&r == row) {
            return previous;
        }
        if (r.sidebar == row->sidebar && r.column == row->column && r.info->type != WIDGET_TEXT) {
            previous = r.info;
        }
    }
    return nullptr;
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

// ---- Leg 9 (#1233): one hosted-row registry entry against a built menu -------
/** The visible label: no "##id" suffix, no printf tail, no marker (soh_ui_snapshot.cpp R4). */
std::string HostedVisibleLabel(const std::string& name) {
    std::string s = name;
    const std::size_t hid = s.find("##");
    if (hid != std::string::npos) {
        s = s.substr(0, hid);
    }
    const std::string marker = std::string(SohGui::SohMenu::SharedIntentMarker()) + " ";
    if (s.rfind(marker, 0) == 0) {
        s = s.substr(marker.size());
    }
    const std::size_t pct = s.find('%');
    if (pct != std::string::npos) {
        s = s.substr(0, pct);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == ':')) {
        s.pop_back();
    }
    return s;
}

/** R4 Title Case over a visible label, parenthesised words included (#1276). */
bool IsHostedTitleCase(const std::string& label) {
    static const std::set<std::string> kSmall = { "a",    "an", "and", "as", "at",  "by", "for", "from", "in",
                                                  "into", "of", "on",  "or", "the", "to", "vs",  "with" };
    std::istringstream in(label);
    std::string word;
    bool first = true;
    while (in >> word) {
        std::string bare;
        for (char ch : word) {
            if (std::isalpha((unsigned char)ch)) {
                bare += ch;
            }
        }
        if (bare.empty()) {
            first = false;
            continue;
        }
        std::string lower = bare;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) { return std::tolower(ch); });
        if (!first && kSmall.contains(lower)) {
            first = false;
            continue;
        }
        const std::size_t alpha = word.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
        if (alpha != std::string::npos && std::islower((unsigned char)word[alpha])) {
            return false;
        }
        first = false;
    }
    return true;
}

/**
 * Where @p h says its row is, is it there? On its header and sidebar, under its section (the nearest SEPARATOR_TEXT
 * above it in its column, "" when none), Title Case (R4, #1276), and bound to its key (the row's .CVar) or to its
 * accessor (no CVar at all). Returns "" when it is, else the reason. Shared by the registry walk and its control, so
 * the control exercises the very check the walk relies on.
 */
std::string HostedRowVerdict(std::unordered_map<std::string, MainMenuEntry>& entries, const SohGui::HostedMmRow& h) {
    const std::string where = h.header + " > " + h.sidebar;
    if (!entries.contains(h.header) || !entries.at(h.header).sidebars.contains(h.sidebar)) {
        return where + " was not built by this leg's probe (Settings, Enhancements and Dev Tools, SoH's builders that "
                       "run ROM-free); a row hosted under another header needs that header's builder added to leg 9";
    }
    SidebarEntry& page = entries.at(h.header).sidebars.at(h.sidebar);
    for (std::vector<WidgetInfo>& column : page.columnWidgets) {
        std::string section;
        for (WidgetInfo& row : column) {
            if (row.type == WIDGET_SEPARATOR_TEXT) {
                section = row.name;
            }
            if (row.name != h.name || row.type == WIDGET_SEPARATOR_TEXT) {
                continue;
            }
            if (section != h.section) {
                return "\"" + h.name + "\" on " + where + " sits under \"" + section + "\", registered under \"" +
                       h.section + "\"";
            }
            if (!IsHostedTitleCase(HostedVisibleLabel(row.name))) {
                return "\"" + h.name + "\" on " + where + " is not Title Case (R4)";
            }
            if (h.key != nullptr) {
                if (row.cVar == nullptr || std::string(row.cVar) != h.key) {
                    return "\"" + h.name + "\" on " + where + " is bound to \"" +
                           std::string(row.cVar != nullptr ? row.cVar : "(no CVar)") + "\", registered to \"" + h.key +
                           "\"";
                }
            } else if (h.accessor != nullptr) {
                if (row.cVar != nullptr) {
                    return "\"" + h.name + "\" on " + where + " binds the accessor " + h.accessor +
                           " but also writes the CVar \"" + row.cVar + "\"";
                }
            } else {
                return "\"" + h.name + "\" on " + where +
                       " was registered with no binding: chain .CVar(key) or .Accessor(name) on the seam's handle";
            }
            return std::string();
        }
    }
    return "no row named \"" + h.name + "\" on " + where;
}

/**
 * #1276: in soh_ui_snapshot.cpp, window/Audio Editor captures both "" (Audio Options) and "mm-pane" (Majora's Mask
 * music replacement tabs, which scroll the outer Audio Editor window to mm-pane@scroll1). Finish() checks p.expectText
 * on every scroll slice, where the top-of-window tab bar ("Audio Options") has scrolled out of InnerClipRect on
 * mm-pane@scroll1; only the window title ("Audio Editor") stays on screen in every scroll view, while per-tab strings
 * belong in stateText with stateContrast. Returns "" when the spec obeys this, else the reason.
 */
std::string AudioEditorSnapshotSpecVerdict(const std::string& snapshotSource) {
    const std::string marker = "p.id = \"window/Audio Editor\";";
    const std::size_t begin = snapshotSource.find(marker);
    if (begin == std::string::npos) {
        return "soh_ui_snapshot.cpp has no window/Audio Editor PageSpec";
    }
    const std::size_t end = snapshotSource.find("pages.push_back(p);", begin);
    if (end == std::string::npos) {
        return "window/Audio Editor PageSpec in soh_ui_snapshot.cpp has no pages.push_back(p)";
    }
    const std::string block = snapshotSource.substr(begin, end - begin);
    if (block.find("p.expectText = { \"Audio Options\" };") != std::string::npos) {
        return "window/Audio Editor's expectText is { \"Audio Options\" }, which scrolls out of view on "
               "mm-pane@scroll1; expect the window title { \"Audio Editor\" } instead";
    }
    if (block.find("p.expectText = { \"Audio Editor\" };") == std::string::npos) {
        return "window/Audio Editor's expectText is not { \"Audio Editor\" } (its window title)";
    }
    if (block.find("p.stateContrast") == std::string::npos) {
        return "window/Audio Editor has no p.stateContrast between \"\" and \"mm-pane\"";
    }
    return std::string();
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
    printf("[TEST] menu-mm-enhancement-rows: the curated MM enhancement rows on SoH's own pages, their CVar bindings "
           "and their ADR 0004 section 5 presentation (#682, #1133)\n");

    gFailures = 0;

    // ---- Leg 1 (#1133): every curated row is on its SoH page, beside its twin --
    // Operator ruling D5 (2026-10-22): the Combo header's Majora's Mask page
    // retired, and each of its rows is hosted on the SoH page that holds its twin
    // or nearest relative, through the hosted-row seam, by a one-line call in
    // SoH's own builder. Built here through SoH's own AddMenuEnhancements, so the
    // pages are SoH's pages. The placement table is the PR's table: a row beside
    // a twin sits directly after it, in its column and under its separator; a row
    // with no twin sits in the "Majora's Mask" block on Quality of Life, after
    // Time of Day's SoH rows and before Pause Menu.
    for (const SohGui::ComboSectionPage& page : SohGui::GetComboSectionPages()) {
        MME_CHECK(page.sidebarName != "Majora's Mask" && page.sidebarName != "MM Enhancements",
                  "a page named \"%s\" is still registered; its rows are hosted on SoH's own pages (#1133)",
                  page.sidebarName.c_str());
    }

    MmEnhancementMenuProbe probe;
    probe.AddMenuEnhancements();
    auto& entries = probe.Entries();
    if (!entries.contains("Enhancements")) {
        printf("[TEST] FAIL(1): AddMenuEnhancements registered no Enhancements header\n");
        return 1;
    }
    MainMenuEntry& enhancements = entries.at("Enhancements");
    std::vector<FlatRow> rows = FlattenHeader(enhancements);

    struct CuratedRowHost {
        const char* key;
        const char* sidebar;
        const char* section;
        const char* after; ///< the interactive row directly before it in its column, or NULL: in its block
    };
    const CuratedRowHost kPlacements[] = {
        { RSBS_CVAR_MM_AUTOSAVE_INTERVAL, "Quality of Life", "Saving", "Autosave" },
        { RSBS_CVAR_MM_KALEIDO_GAME_OVER, "Quality of Life", SohGui::kHostedMmBlockLabel, nullptr },
        { RSBS_CVAR_MM_BETTER_SONG_OF_DOUBLE_TIME, "Quality of Life", SohGui::kHostedMmBlockLabel, "Game Over Prompt" },
        { RSBS_CVAR_MM_SKIP_SOT_CUTSCENES, "Skips & Speed-ups", "Cutscenes", "Skip Song Cutscenes" },
    };
    std::size_t ownRows = 0;
    for (std::size_t i = 0; i < RSBS::kHostedMmEnhancementCount; i++) {
        if (RSBS::kHostedMmEnhancements[i].hosting == RSBS::MmEnhancementHosting::OwnRow) {
            ownRows++;
        }
    }
    MME_CHECK(ownRows == sizeof(kPlacements) / sizeof(kPlacements[0]),
              "the manifest has %zu own rows and the placement table %zu; a curated row with no placement has no "
              "host",
              ownRows, sizeof(kPlacements) / sizeof(kPlacements[0]));
    for (const CuratedRowHost& want : kPlacements) {
        int bound = 0;
        for (const FlatRow& r : rows) {
            bound += r.info->cVar != nullptr && std::string(r.info->cVar) == want.key ? 1 : 0;
        }
        FlatRow* row = FindRowByCVar(rows, want.key);
        MME_CHECK(bound == 1, "%d rows under Enhancements bind %s, expected exactly one", bound, want.key);
        if (row == nullptr) {
            continue;
        }
        MME_CHECK(row->sidebar == want.sidebar && row->section == want.section,
                  "the %s row is on Enhancements > %s under \"%s\", expected Enhancements > %s under \"%s\"", want.key,
                  row->sidebar.c_str(), row->section.c_str(), want.sidebar, want.section);
        const WidgetInfo* before = PreviousInteractive(rows, row);
        if (want.after != nullptr) {
            MME_CHECK(before != nullptr && before->name == want.after,
                      "the %s row follows \"%s\" in its column, expected SoH's \"%s\" directly before it", want.key,
                      before != nullptr ? before->name.c_str() : "(the column's start)", want.after);
        } else {
            // In its block: right under the block's heading, or after another row the hosted-row seam put in the
            // same block (#1245's Time Cycle rows open the Quality of Life block), never after an SoH row.
            const bool afterBlockRow =
                before != nullptr && SohGui::IsHostedMmRow("Enhancements", want.sidebar, before->name);
            MME_CHECK(before != nullptr && (before->type == WIDGET_SEPARATOR_TEXT || afterBlockRow),
                      "the %s row follows \"%s\"; it belongs in its block, after the heading or another row of it",
                      want.key, before != nullptr ? before->name.c_str() : "(the column's start)");
        }
        bool recorded = false;
        for (const SohGui::HostedMmRow& h : SohGui::HostedMmRows()) {
            recorded = recorded || (h.key != nullptr && std::string(h.key) == want.key && h.header == "Enhancements" &&
                                    h.sidebar == want.sidebar && h.section == want.section);
        }
        MME_CHECK(recorded,
                  "the %s row is not recorded by the hosted-row seam on Enhancements > %s under \"%s\", so the UI "
                  "snapshot neither captures nor lints it",
                  want.key, want.sidebar, want.section);
    }
    // The block's place: Quality of Life's first column, after Time of Day's
    // SoH rows, before Pause Menu. The block is the "Majora's Mask" heading that
    // opens Game Over Prompt's group: since #1247 the column holds a second
    // block of that name under Saving (Pause Menu Save), so the first heading of
    // the name is not this one.
    {
        long block = -1;
        long shops = -1;
        long pause = -1;
        long lastHeading = -1;
        for (std::size_t i = 0; i < rows.size(); i++) {
            const FlatRow& r = rows[i];
            if (r.sidebar != "Quality of Life" || r.column != SECTION_COLUMN_1) {
                continue;
            }
            if (r.info->type == WIDGET_SEPARATOR_TEXT) {
                lastHeading = (long)i;
            }
            if (r.info->cVar != nullptr && std::string(r.info->cVar) == RSBS_CVAR_MM_KALEIDO_GAME_OVER &&
                lastHeading >= 0 && rows[(std::size_t)lastHeading].info->name == SohGui::kHostedMmBlockLabel) {
                block = lastHeading;
            } else if (r.info->name == "Shops and Games Always Open") {
                shops = (long)i;
            } else if (r.info->type == WIDGET_SEPARATOR_TEXT && r.info->name == "Pause Menu") {
                pause = (long)i;
            }
        }
        MME_CHECK(block >= 0 && shops >= 0 && pause >= 0 && shops < block && block < pause,
                  "Quality of Life's first column has the Majora's Mask block at %ld, Shops and Games Always Open at "
                  "%ld and Pause Menu at %ld; the block belongs after Time of Day's rows and before Pause Menu",
                  block, shops, pause);
    }
    // The converged key (#1119's form): one row, both games. SoH's Autosave on
    // Quality of Life > Saving is the only writer of gEnhancements.Autosave,
    // and its tooltip says it applies to both games.
    for (std::size_t i = 0; i < RSBS::kHostedMmEnhancementCount; i++) {
        const RSBS::HostedMmEnhancement& desc = RSBS::kHostedMmEnhancements[i];
        if (desc.hosting != RSBS::MmEnhancementHosting::HostedElsewhere) {
            continue;
        }
        int bound = 0;
        for (const FlatRow& r : rows) {
            bound += r.info->cVar != nullptr && std::string(r.info->cVar) == desc.key ? 1 : 0;
        }
        FlatRow* row = FindRowByCVar(rows, desc.key);
        MME_CHECK(bound == 1 && row != nullptr && row->info->name == desc.label &&
                      !SohGui::IsHostedMmRow("Enhancements", row->sidebar, row->info->name),
                  "%d rows under Enhancements bind the converged %s; expected exactly SoH's own \"%s\", the one row "
                  "both games read",
                  bound, desc.key, desc.label);
        if (row != nullptr) {
            const std::string tip = row->info->options != nullptr && row->info->options->tooltip != nullptr
                                        ? row->info->options->tooltip
                                        : "";
            MME_CHECK(tip.find("both games") != std::string::npos,
                      "SoH's \"%s\" row writes the key both games read, but its tooltip does not say so: \"%s\"",
                      desc.label, tip.c_str());
        }
        MME_CHECK(FindRowByName(rows, desc.tooltip) == nullptr,
                  "the retired page's pointer sentence for %s is still drawn", desc.key);
    }
    printf("[TEST] leg 1: every curated row sits on its SoH page beside its twin or in the Majora's Mask block, "
           "through the hosted-row seam, and the converged Autosave is SoH's one row\n");

    // ---- Leg 6: every own-row tooltip is in SoH's voice -----------------------
    // R-TT2 (present tense, verb first: SoH's "Makes...", "Allows...",
    // "Toggles..." -- a third-person verb, so the first word ends in 's'),
    // R-TT4 (one or two sentences, the trailing caveat included) and R-TT6 (the
    // caveat trails). A pointer row draws nothing and is not held to this.
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

    // ---- Leg 2: every manifest key has its row, bound to that key -----------
    for (std::size_t i = 0; i < RSBS::kHostedMmEnhancementCount; i++) {
        const RSBS::HostedMmEnhancement& desc = RSBS::kHostedMmEnhancements[i];
        if (desc.hosting == RSBS::MmEnhancementHosting::HostedElsewhere) {
            continue; // SoH's own row, leg 1
        }
        FlatRow* row = FindRowByCVar(rows, desc.key);
        if (row == nullptr) {
            printf("[TEST] FAIL(2): no row under Enhancements is bound to \"%s\" (manifest row %zu, \"%s\"). A widget "
                   "whose CVar drifts from its provider flips a key nothing consults -- #499's failure, and "
                   "invisible\n",
                   desc.key, i, desc.label);
            gFailures++;
            continue;
        }
        MME_CHECK(row->info->name == desc.label, "the row on \"%s\" is named \"%s\", the manifest says \"%s\"",
                  desc.key, row->info->name.c_str(), desc.label);
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
        const char* tip = row->info->options != nullptr ? row->info->options->tooltip : nullptr;
        MME_CHECK(tip != nullptr && std::string(tip) == desc.tooltip,
                  "the row on \"%s\" carries tooltip \"%s\", the manifest says \"%s\"", desc.key,
                  tip != nullptr ? tip : "(null)", desc.tooltip);
        MME_CHECK(!row->info->raceDisable, "the row on \"%s\" is race-disabled; these are preferences", desc.key);
    }
    printf("[TEST] leg 2: every own manifest key is bound by exactly the row its widget class calls for, with the "
           "manifest's label and tooltip\n");

    // ---- Leg 3: a Live row draws live ---------------------------------------
    for (std::size_t i = 0; i < RSBS::kHostedMmEnhancementCount; i++) {
        const RSBS::HostedMmEnhancement& desc = RSBS::kHostedMmEnhancements[i];
        // Every manifest reason, whatever its hosting: it is drawn after "- " in
        // SoH's disabled tooltip, so it is player text and prints no tracker
        // number (ADR 0004's 2026-09-27 amendment).
        MME_CHECK(!PrintsIssueNumber(desc.reason),
                  "the manifest reason for \"%s\" prints a tracker number (\"%s\"); it is drawn in the disabled "
                  "tooltip, and no SoH disabled reason carries one",
                  desc.key, desc.reason != nullptr ? desc.reason : "(null)");
        // #747: the tracker lives in the row's own field instead, nonzero
        // exactly when the row is not Live. BELT AND BRACES, not a lock: the
        // same rule is a static_assert on this very table
        // (HostedMmEnhancementsAreHonest() in cvar_shared_keys.h). The rule's
        // observed red halves are leg 7a's, over synthetic tables.
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

        // Ordering: the row writing the gate comes first, on the same page.
        FlatRow* gate = FindRowByCVar(rows, desc.shownWhileKey);
        const long gateAt = IndexOfRow(rows, desc.shownWhileKey, nullptr);
        const long rowAt = IndexOfRow(rows, desc.key, nullptr);
        MME_CHECK(gate != nullptr && gate->sidebar == row->sidebar && gateAt >= 0 && rowAt > gateAt,
                  "the row on \"%s\" is at index %ld and the row writing its gate \"%s\" at %ld; the gated row must "
                  "come after it on the same page",
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
        // isHidden re-derived from it by one more real draw.
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
           "when it goes off again, and sit after the row that writes the gate\n",
           gatedRows);

    // ---- Leg 4: the non-live path, driven with synthetic rows ---------------
    // The shipped manifest is all-Live, so without this leg #682's disabled
    // branch is code nobody has run. These drive ApplyPresentation exactly as the
    // production PreFunc does.
    {
        MmEnhancementMenuProbe synthProbe;
        WidgetPath synthPath = { "Synthetic", "Synthetic Liveness Page", SECTION_COLUMN_1 };
        synthProbe.AddMenuEntry("Synthetic", "gSettings.Menu.SyntheticSidebarSection");
        synthProbe.AddSidebarEntry("Synthetic", synthPath.sidebarName, 1);

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

        SidebarEntry& synthPage = synthProbe.Entries().at("Synthetic").sidebars.at(synthPath.sidebarName);
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
        // whose row is drawn disabled with NO note under its separator must be
        // counted as a breach. Built the way leg 4's synthetic rows are (the
        // presentation call, no builder), so no note is registered.
        {
            MmEnhancementMenuProbe controlProbe;
            WidgetPath controlPath = { "Synthetic", "Synthetic Noteless Page", SECTION_COLUMN_1 };
            controlProbe.AddMenuEntry("Synthetic", "gSettings.Menu.SyntheticSidebarSection");
            controlProbe.AddSidebarEntry("Synthetic", controlPath.sidebarName, 1);
            controlProbe.AddWidget(controlPath, "Noteless Group", WIDGET_SEPARATOR_TEXT);
            controlProbe.AddWidget(controlPath, "Synthetic Noteless Row", WIDGET_CVAR_CHECKBOX)
                .CVar("gEnhancements.MenuLockSyntheticRow")
                .RaceDisable(false)
                .Options(UIWidgets::CheckboxOptions().Tooltip("synthetic"))
                .PreFunc([](WidgetInfo& i) {
                    SohGui::SohMenu::ApplyPresentation(i, i.name, SohGui::SOH_MENU_PRESENT_CAPABILITY,
                                                       kSyntheticDormantReason);
                });
            SidebarEntry& controlPage = controlProbe.Entries().at("Synthetic").sidebars.at(controlPath.sidebarName);
            GroupNoteCensus control = CheckGroupNotes(controlPage, "noteless control page", false);
            MME_CHECK(control.disabledRows == 1 && control.violations == 1,
                      "the noteless control page counted %zu disabled row(s) and %zu breach(es), expected 1 and 1: "
                      "the group-note check cannot see a disabled row with no note, so its green proves nothing",
                      control.disabledRows, control.violations);
            printf("[TEST] leg 7-control: the group-note check counts a disabled row with no note as a breach (red "
                   "half: %zu breach)\n",
                   control.violations);
        }

        // 7b. The shipped rows: every one Live, so none is drawn disabled and no
        // group note is registered on their pages -- SoH's pages look as SoH
        // ships them, plus the rows.
        {
            std::size_t disabled = 0;
            for (std::size_t i = 0; i < RSBS::kHostedMmEnhancementCount; i++) {
                const RSBS::HostedMmEnhancement& desc = RSBS::kHostedMmEnhancements[i];
                FlatRow* row =
                    desc.hosting == RSBS::MmEnhancementHosting::OwnRow ? FindRowByCVar(rows, desc.key) : nullptr;
                if (row != nullptr) {
                    DrawPass(*row->info);
                    disabled += IsDisabled(*row->info) ? 1 : 0;
                }
            }
            const bool noteRegistered = FindRowByName(rows, SohGui::MmEnhancementsGroupNoteText()) != nullptr ||
                                        FindRowByName(rows, "Majora's Mask Group Note") != nullptr;
            MME_CHECK(disabled == 0 && !noteRegistered,
                      "the shipped rows draw %zu disabled row(s) and the Enhancements header %s a group note; every "
                      "manifest row is Live, so neither",
                      disabled, noteRegistered ? "registers" : "does not register");
            printf("[TEST] leg 7b: the shipped rows draw none disabled and their pages carry no group note\n");
        }

        // 7c. A block built from the synthetic table through the builder the
        // shipped block shares its row and note code with. The pointer row
        // draws nothing (SoH's own row is the control for its key), so the
        // block is ONE group: its note shows while any of its rows is drawn
        // disabled. The gate key is set explicitly both ways and put back.
        MmEnhancementMenuProbe noteProbe;
        WidgetPath notePath = { "Synthetic", "Synthetic Group Note Page", SECTION_COLUMN_1 };
        noteProbe.AddMenuEntry("Synthetic", "gSettings.Menu.SyntheticSidebarSection");
        noteProbe.AddSidebarEntry("Synthetic", notePath.sidebarName, 1);
        SohGui::AddMmEnhancementRows(noteProbe, notePath, kSyntheticManifest, kSyntheticManifestCount);
        SidebarEntry& notePage = noteProbe.Entries().at("Synthetic").sidebars.at(notePath.sidebarName);
        std::vector<FlatRow> noteRows = FlattenRows(notePage);
        MME_CHECK(FindRowByName(noteRows, "Synthetic Parent") == nullptr &&
                      FindRowByCVar(noteRows, kSynthParentKey) == nullptr,
                  "the builder drew the pointer row \"Synthetic Parent\" or bound its key; SoH's own row is the one "
                  "control for a key hosted elsewhere");
        MME_CHECK(!noteRows.empty() && noteRows.front().info->type == WIDGET_SEPARATOR_TEXT &&
                      noteRows.front().info->name == SohGui::kHostedMmBlockLabel,
                  "the builder's rows do not open with the Majora's Mask block heading");

        const int savedGate = CVarGetInteger(kSynthParentKey, INT32_MIN);

        // Gate OFF: the Partial slider is hidden, so only the Dormant row is
        // drawn disabled, and the block's one note shows.
        CVarSetInteger(kSynthParentKey, 0);
        GroupNoteCensus off = CheckGroupNotes(notePage, "synthetic block, gate off", true);
        MME_CHECK(off.disabledRows == 1 && off.notesShown == 1,
                  "gate off: %zu disabled row(s) and %zu note(s) shown, expected 1 and 1 (the Dormant row, and the "
                  "block's note; the gated Partial row is hidden)",
                  off.disabledRows, off.notesShown);

        // Gate ON: the Partial slider draws disabled too; still one note.
        CVarSetInteger(kSynthParentKey, 1);
        GroupNoteCensus on = CheckGroupNotes(notePage, "synthetic block, gate on", true);
        MME_CHECK(on.disabledRows == 2 && on.notesShown == 1,
                  "gate on: %zu disabled row(s) and %zu note(s) shown, expected 2 and 1 (one block, one note)",
                  on.disabledRows, on.notesShown);

        // The disabled rows are the builder's, not leg 4's hand-made ones: SoH's
        // disabled shape around the manifest reason, and the row's own name.
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
        printf("[TEST] leg 7c: a block built from a synthetic non-live table shows one gray note while a row of it "
               "is drawn disabled (%zu with the gate off, %zu with it on), directly above its rows, and draws no "
               "pointer row\n",
               off.notesShown, on.notesShown);

        // 7d. A table that lists a POINTER FIRST: the pointer draws nothing, so
        // the block's note sits directly under its heading, above the one row,
        // and only while that row draws disabled.
        {
            MmEnhancementMenuProbe firstProbe;
            WidgetPath firstPath = { "Synthetic", "Synthetic Pointer First Page", SECTION_COLUMN_1 };
            firstProbe.AddMenuEntry("Synthetic", "gSettings.Menu.SyntheticSidebarSection");
            firstProbe.AddSidebarEntry("Synthetic", firstPath.sidebarName, 1);
            SohGui::AddMmEnhancementRows(firstProbe, firstPath, kPointerFirstManifest, kPointerFirstManifestCount);
            SidebarEntry& firstPage = firstProbe.Entries().at("Synthetic").sidebars.at(firstPath.sidebarName);

            const int savedFirstGate = CVarGetInteger(kSynthFirstParentKey, INT32_MIN);
            CVarSetInteger(kSynthFirstParentKey, 0);
            GroupNoteCensus firstOff = CheckGroupNotes(firstPage, "pointer-first block, gate off", true);
            CVarSetInteger(kSynthFirstParentKey, 1);
            GroupNoteCensus firstOn = CheckGroupNotes(firstPage, "pointer-first block, gate on", true);
            MME_CHECK(firstOff.disabledRows == 0 && firstOff.notesShown == 0 && firstOn.disabledRows == 1 &&
                          firstOn.notesShown == 1 && firstOn.violations == 0,
                      "pointer-first block: gate off %zu disabled / %zu note(s), gate on %zu / %zu with %zu "
                      "breach(es); expected 0 / 0, then 1 / 1 and none",
                      firstOff.disabledRows, firstOff.notesShown, firstOn.disabledRows, firstOn.notesShown,
                      firstOn.violations);
            std::vector<FlatRow> firstRows = FlattenRows(firstPage);
            MME_CHECK(FindRowByName(firstRows, "Synthetic First Parent") == nullptr,
                      "pointer-first block: the pointer row is drawn");
            if (savedFirstGate == INT32_MIN) {
                CVarClear(kSynthFirstParentKey);
            } else {
                CVarSetInteger(kSynthFirstParentKey, savedFirstGate);
            }
            printf("[TEST] leg 7d: a table listing a pointer first draws no pointer and shows its block's note only "
                   "while the gated row draws disabled\n");
        }
    }

    // ---- Leg 8 (#1235): Settings > Audio holds MM's ambience slider -----------
    // MM's fifth sequence player (wind, birds, night sounds) has no OoT twin, so nothing in the single exe wrote its
    // key: MM_AudioMgr_Init reads RSBS_CVAR_VOLUME_AMBIENCE and only shipofharkinian.json could set it. The row is
    // SoH's slider in SoH's shape (0-100 percent, default 100, the family's spelling), right after SoH's Sound Effects
    // Volume and in its column, built here through SoH's own AddMenuSettings so the page is SoH's page.
    {
        MmEnhancementMenuProbe settingsProbe;
        settingsProbe.AddMenuSettings();
        auto& settingsEntries = settingsProbe.Entries();
        if (!settingsEntries.contains("Settings") || !settingsEntries.at("Settings").sidebars.contains("Audio")) {
            printf("[TEST] FAIL(8): AddMenuSettings registered no Settings > Audio page\n");
            gFailures++;
        } else {
            std::vector<FlatRow> audioRows = FlattenRows(settingsEntries.at("Settings").sidebars.at("Audio"));
            const long sfx = IndexOfRow(audioRows, RSBS_CVAR_VOLUME_SFX, nullptr);
            const long ambience = IndexOfRow(audioRows, RSBS_CVAR_VOLUME_AMBIENCE, nullptr);
            MME_CHECK(sfx >= 0, "Settings > Audio has no Sound Effects Volume row bound to %s", RSBS_CVAR_VOLUME_SFX);
            MME_CHECK(ambience >= 0,
                      "Settings > Audio has no row bound to %s: MM's ambience volume has no control in the single "
                      "exe, so only a hand-edited config can set it",
                      RSBS_CVAR_VOLUME_AMBIENCE);
            if (sfx >= 0 && ambience >= 0) {
                WidgetInfo& row = *audioRows[(size_t)ambience].info;
                MME_CHECK(ambience == sfx + 1 && audioRows[(size_t)ambience].column == audioRows[(size_t)sfx].column,
                          "the ambience row is row %ld (column %u); it belongs right after Sound Effects Volume (row "
                          "%ld, column %u)",
                          ambience, audioRows[(size_t)ambience].column, sfx, audioRows[(size_t)sfx].column);
                MME_CHECK(row.type == WIDGET_CVAR_SLIDER_INT,
                          "the ambience row is widget type %d, not SoH's int slider", (int)row.type);
                if (row.type == WIDGET_CVAR_SLIDER_INT && row.options != nullptr) {
                    const auto opts = std::static_pointer_cast<UIWidgets::IntSliderOptions>(row.options);
                    MME_CHECK(opts->min == 0 && opts->max == 100 && opts->defaultValue == 100,
                              "the ambience slider is %d-%d with default %d; the volume family is 0-100 percent with "
                              "default 100 (MM_AudioMgr_Init's read default)",
                              opts->min, opts->max, opts->defaultValue);
                }
                const std::string tip = row.options != nullptr && row.options->tooltip != nullptr
                                            ? std::string(row.options->tooltip)
                                            : std::string();
                const std::string caveat = " Majora's Mask only.";
                MME_CHECK(tip.size() > caveat.size() &&
                              tip.compare(tip.size() - caveat.size(), caveat.size(), caveat) == 0,
                          "the ambience row's tooltip \"%s\" does not end with the \"Majora's Mask only.\" caveat",
                          tip.c_str());
            }
            printf("[TEST] leg 8: Settings > Audio holds MM's ambience slider right after Sound Effects Volume (row "
                   "%ld after row %ld)\n",
                   ambience, sfx);
        }
    }

    // ---- Leg 9 (#1233): every hosted MM row is where the registry says --------
    // The hosted-row seam (SohGui/HostedMm/SohMenuHostedMm.h) records each MM row it adds to one of SoH's pages. This
    // probe builds SoH's own Settings, Enhancements and Dev Tools pages, and every registered row must be found on its
    // page, under its section, bound to its key or accessor. A page lane adds registry entries, never a leg here. The
    // control first: the same verdict must refuse the damage row with a wrong section and with a wrong key.
    {
        MmEnhancementMenuProbe hostProbe;
        hostProbe.AddMenuSettings();
        hostProbe.AddMenuEnhancements();
        hostProbe.AddMenuDevTools();
        auto& hostEntries = hostProbe.Entries();

        const SohGui::HostedMmRow* damage = nullptr;
        for (const SohGui::HostedMmRow& h : SohGui::HostedMmRows()) {
            if (h.key != nullptr && std::string(h.key) == RSBS_CVAR_MM_DAMAGE_MULTIPLIER) {
                damage = &h;
            }
        }
        MME_CHECK(damage != nullptr && damage->header == "Enhancements" && damage->sidebar == "Difficulty" &&
                      damage->section == "Health",
                  "the hosted-row registry holds no Enhancements > Difficulty > Health row bound to %s after SoH's "
                  "Enhancements page was built: #1037's row no longer goes through the seam, so nothing captures, "
                  "hovers or lints it",
                  RSBS_CVAR_MM_DAMAGE_MULTIPLIER);
        MME_CHECK(!IsHostedTitleCase(HostedVisibleLabel("Tatl ISG (restoration)")) &&
                      IsHostedTitleCase(HostedVisibleLabel("Tatl ISG (Restoration)")),
                  "leg 9's R4 Title Case check does not refuse a lowercase parenthesised word (#1276)");
        if (damage != nullptr) {
            SohGui::HostedMmRow wrongSection = *damage;
            wrongSection.section = "Not The Health Section";
            SohGui::HostedMmRow wrongKey = *damage;
            wrongKey.key = RSBS_CVAR_VOLUME_SFX;
            MME_CHECK(!HostedRowVerdict(hostEntries, wrongSection).empty() &&
                          !HostedRowVerdict(hostEntries, wrongKey).empty(),
                      "leg 9's verdict accepts the damage row under a wrong section or bound to a wrong key, so its "
                      "green over the registry would prove nothing");
        }

        const SohGui::HostedMmRow* tatlIsg = nullptr;
        for (const SohGui::HostedMmRow& h : SohGui::HostedMmRows()) {
            if (h.key != nullptr && std::string(h.key) == RSBS_CVAR_MM_RESTORATIONS_TATL_ISG) {
                tatlIsg = &h;
            }
        }
        MME_CHECK(tatlIsg != nullptr && tatlIsg->header == "Enhancements" && tatlIsg->sidebar == "Fixes" &&
                      tatlIsg->section == "Glitch Restorations" && tatlIsg->name == "Tatl ISG (Restoration)",
                  "the hosted-row registry holds no \"Tatl ISG (Restoration)\" row on Enhancements > Fixes > Glitch "
                  "Restorations bound to %s (#1276: it reads \"%s\")",
                  RSBS_CVAR_MM_RESTORATIONS_TATL_ISG, tatlIsg != nullptr ? tatlIsg->name.c_str() : "(none)");

        // The operator's ruling of 2026-10-27: MM's mod rows are a Settings page of their own, MM Mods, after SoH's
        // OoT Mods, and the page's name says whose they are, so Rescan Mods Folder sits under no heading.
        const SohGui::HostedMmRow* rescan = nullptr;
        for (const SohGui::HostedMmRow& h : SohGui::HostedMmRows()) {
            if (h.name == "Rescan Mods Folder") {
                MME_CHECK(rescan == nullptr, "the hosted-row registry holds Rescan Mods Folder twice (%s > %s too)",
                          h.header.c_str(), h.sidebar.c_str());
                rescan = &h;
            }
        }
        MME_CHECK(rescan != nullptr && rescan->header == "Settings" && rescan->sidebar == "MM Mods" &&
                      rescan->section.empty() && rescan->accessor != nullptr &&
                      std::string(rescan->accessor) == "Combo_MMModSet_Scan",
                  "the hosted-row registry holds no Rescan Mods Folder on Settings > MM Mods under no heading bound to "
                  "Combo_MMModSet_Scan (it reads %s > %s under \"%s\")",
                  rescan != nullptr ? rescan->header.c_str() : "(none)",
                  rescan != nullptr ? rescan->sidebar.c_str() : "(none)",
                  rescan != nullptr ? rescan->section.c_str() : "");

        std::size_t checked = 0;
        for (const SohGui::HostedMmRow& h : SohGui::HostedMmRows()) {
            const std::string why = HostedRowVerdict(hostEntries, h);
            MME_CHECK(why.empty(), "hosted MM row: %s", why.c_str());
            checked++;
        }
        MME_CHECK(!AudioEditorSnapshotSpecVerdict("p.id = \"window/Audio Editor\";\n"
                                                  "p.expectText = { \"Audio Options\" };\n"
                                                  "pages.push_back(p);")
                       .empty(),
                  "leg 9's Audio Editor snapshot spec verdict accepts p.expectText = { \"Audio Options\" } (#1276)");
        {
            const std::filesystem::path snapshotPath =
                std::filesystem::path(__FILE__).parent_path() / "soh_ui_snapshot.cpp";
            std::ifstream snapshotFile(snapshotPath, std::ios::binary);
            if (snapshotFile.is_open()) {
                const std::string snapshotText((std::istreambuf_iterator<char>(snapshotFile)),
                                               std::istreambuf_iterator<char>());
                const std::string specWhy = AudioEditorSnapshotSpecVerdict(snapshotText);
                MME_CHECK(specWhy.empty(), "ui-snapshot window/Audio Editor spec (#1276): %s", specWhy.c_str());
            }
        }
        printf("[TEST] leg 9: %zu hosted MM row(s) found on their SoH pages, under their sections, bound as "
               "registered (the verdict refuses a wrong section and a wrong key)\n",
               checked);
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
 * The hosted rows and the manifest's whole point (MM's menu TU is excluded)
 * exist only in the single-exe build. A
 * standalone SoH build has no MM half to configure, so the row reports pass
 * rather than failing to link. test_runner.cpp declares this unconditionally.
 */
extern "C" int OoT_MenuMmEnhancementRows_RunHeadless(void) {
    printf("[TEST] menu-mm-enhancement-rows: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
