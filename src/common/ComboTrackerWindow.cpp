/**
 * @file ComboTrackerWindow.cpp
 * @brief Renders the combo tracker model in-game (#458; ADR 0008).
 *
 * See ComboTrackerWindow.h for the contract. Every value drawn here comes
 * from combo_tracker_view.h and is read again every frame, so progress made
 * mid-session updates on the next frame. The file's only state is each game
 * pane's own (PaneState): its search text, which persists across frames, a
 * closed and reopened pane and file loads within one process, as SoH's
 * checkSearch does, and its Show Hidden Items and Only Show Available Checks
 * toggles (#1128, #1129, ruling D12: per pane), seeded from SoH's
 * gTrackers.CheckTracker.ShowHidden and .OnlyShowAvailable and never written
 * back, so the two panes toggle apart and no key is stored.
 *
 * It is drawn the way SoH draws its own tracker and editor panes
 * (docs/ui-style-guide.md section 10): the title bar's close button, themed
 * collapsing headers (UIWidgets::PushStyleCombobox(THEME_COLOR), as the Check
 * Tracker Settings pane's section headers are), gray notes for state, and
 * SoH's table shape (cell padding 8x8, horizontal and vertical borders, a
 * header row). The header row stays in ImGui's table-header gray, as in the
 * Check Tracker Settings pane: the theme's Header / HeaderHovered /
 * HeaderActive colours reach a table's header row only while it is hovered or
 * clicked. A check's status is a FontAwesome glyph, a
 * project convention (SoH's check tracker marks status by row colour instead),
 * and it is the ONE notation for it: the crossing tables (DrawCrossingList,
 * drawn by the Cross-Game Spoiler since ruling D10 cut them from this window)
 * lead each host check's name with the same glyph the Checks lists use.
 * src/common cannot include UIWidgets, so every styled element goes through the
 * combo_ui seam.
 */

#include "ComboTrackerWindow.h"

#include <cfloat>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/IconsFontAwesome4.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "ComboItemTrackerWindow.h" // RegisterComboItemTrackerWindow (#458 U2)
#include "combo_identity_display.h" // the paired seed as the player typed it (#952)
#include "combo_item_view.h"        // {OoT,MM}_ItemAdapter_Register (#458 U1)
#include "combo_tracker_view.h"
#include "combo_ui.h"
#include "context.h" // GameId

namespace ComboGui {

namespace {

const ComboUiTable& Ui() {
    return *ComboUi_Get();
}

/**
 * A check's status as a FontAwesome glyph (a project convention, style guide
 * section 10: SoH's check tracker colours the row instead, and uses glyphs only
 * on its skip/lock buttons): a ticked box when collected, a box with a minus
 * when the player skipped it, an empty box otherwise.
 */
const char* CheckGlyph(const ComboTrackerCheckRow& row) {
    if (row.obtained) {
        return ICON_FA_CHECK_SQUARE_O;
    }
    return row.skipped ? ICON_FA_MINUS_SQUARE_O : ICON_FA_SQUARE_O;
}

/** A crossing's found state in the Checks lists' notation (CheckGlyph). */
const char* FoundGlyph(uint8_t found) {
    if (found == COMBO_TRACKER_FOUND_YES) {
        return ICON_FA_CHECK_SQUARE_O;
    }
    return found == COMBO_TRACKER_FOUND_NO ? ICON_FA_SQUARE_O : ICON_FA_QUESTION_CIRCLE_O;
}

float ImGuiTextWidth(void*, const char* begin, const char* end) {
    return ImGui::CalcTextSize(begin, end).x;
}

float ImGuiTextHeight(void*, const char* text, float wrapWidth) {
    return ImGui::CalcTextSize(text, nullptr, false, wrapWidth).y;
}

const ComboTextMeasure kImGuiMeasure = { ImGuiTextWidth, ImGuiTextHeight, nullptr };

/**
 * Wrapped text with balanced lines (ComboBalancedWrapWidth): a long name breaks
 * into even lines instead of leaving its last word alone on one ("Stone Tower
 * Temple Entrance Small Crate" over "02"), and never inside a word. Text that
 * fits is drawn as is.
 */
void TextBalanced(const char* text) {
    const float wrap = ComboBalancedWrapWidth(text, ImGui::GetContentRegionAvail().x, kImGuiMeasure);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
}

// ---- SoH's Check Tracker colours (#458 U4) ----------------------------------
//
// The Checks lists colour each check, and each area header, the way SoH's Check
// Tracker does (randomizer_check_tracker.cpp, CheckTrackerWindow::DrawElement and
// DrawLocation): a main colour for the name and an extra colour for the text
// after it, per status, read from the player's own Check Tracker settings
// (CVAR_TRACKER_CHECK("<State>.MainColor" / ".ExtraColor"), which expands to the
// "gTrackers.CheckTracker." keys below; src/common has no cvar_prefixes.h) with
// SoH's defaults. Both games' lists use them: one window, one colour scheme.
// Each state's "<State>.Hide" key, SoH's Check Tracker Settings > Hide, is
// spelled out whole (#1128), so a search for the key finds its reader.

constexpr Color_RGBA8 kSohMainDefault = { 255, 255, 255, 255 }; // Color_Main_Default

struct SohStatusColourKey {
    const char* state;        // the CVar's state segment
    Color_RGBA8 extraDefault; // SoH's Color_<State>_Extra_Default
    const char* hideKey;      // SoH's "<State>.Hide" (default 0: shown)
};

// Indexed by ComboTrackerCheckStatus.
const SohStatusColourKey kSohStatusColours[COMBO_TRACKER_CHECK_STATUS_COUNT] = {
    { "Unchecked", { 255, 255, 255, 255 }, "gTrackers.CheckTracker.Unchecked.Hide" }, // COMBO_TRACKER_CHECK_UNCHECKED
    { "Seen", { 255, 255, 255, 255 }, "gTrackers.CheckTracker.Seen.Hide" },           // COMBO_TRACKER_CHECK_SEEN
    { "Scummed", { 0, 174, 239, 255 }, "gTrackers.CheckTracker.Scummed.Hide" },       // COMBO_TRACKER_CHECK_SCUMMED
    { "Skipped", { 160, 160, 160, 255 }, "gTrackers.CheckTracker.Skipped.Hide" },     // COMBO_TRACKER_CHECK_SKIPPED
    { "Collected", { 242, 101, 34, 255 }, "gTrackers.CheckTracker.Collected.Hide" },  // COMBO_TRACKER_CHECK_COLLECTED
    { "Saved", { 0, 185, 0, 255 }, "gTrackers.CheckTracker.Saved.Hide" },             // COMBO_TRACKER_CHECK_SAVED
};

/**
 * One game pane's own state (#1128): its search text, kept across frames like
 * SoH's checkSearch, and its Show Hidden Items and Only Show Available Checks
 * toggles (#1129; ruling D12: per pane). The toggles are window state, not
 * keys: each seeded once from SoH's own key (gTrackers.CheckTracker.ShowHidden,
 * .OnlyShowAvailable), so a player who left SoH's toggle on finds both panes
 * that way, and never written back, so the panes toggle apart, no new key needs
 * classifying, and the pane stores nothing (a stored value would persist into
 * every later run's config).
 */
struct PaneState {
    char search[128] = "";
    bool showHidden = false;
    bool onlyAvailable = false;
    bool seeded = false;
};

PaneState& Pane(uint8_t game) {
    static PaneState sPanes[3];
    PaneState& pane = sPanes[game < 3 ? game : 0];
    if (!pane.seeded) {
        pane.showHidden = CVarGetInteger("gTrackers.CheckTracker.ShowHidden", 0) != 0;
        pane.onlyAvailable = CVarGetInteger("gTrackers.CheckTracker.OnlyShowAvailable", 0) != 0;
        pane.seeded = true;
    }
    return pane;
}

struct TextColours {
    ImVec4 main;
    ImVec4 extra;
};

ImVec4 SohTrackerColour(const char* state, const char* which, Color_RGBA8 fallback) {
    char key[96];
    snprintf(key, sizeof(key), "gTrackers.CheckTracker.%s.%s.Value", state, which);
    const Color_RGBA8 c = CVarGetColor(key, fallback);
    return ImVec4(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
}

TextColours SohColours(const char* state, Color_RGBA8 extraDefault) {
    return { SohTrackerColour(state, "MainColor", kSohMainDefault),
             SohTrackerColour(state, "ExtraColor", extraDefault) };
}

/** Every colour one Checks list draws with, read once per frame. */
struct CheckListColours {
    TextColours status[COMBO_TRACKER_CHECK_STATUS_COUNT];
    TextColours areaComplete;
    TextColours areaIncomplete;
};

CheckListColours ReadCheckListColours() {
    CheckListColours c;
    for (int i = 0; i < (int)COMBO_TRACKER_CHECK_STATUS_COUNT; i++) {
        c.status[i] = SohColours(kSohStatusColours[i].state, kSohStatusColours[i].extraDefault);
    }
    c.areaComplete = SohColours("AreaComplete", kSohMainDefault);
    c.areaIncomplete = SohColours("AreaIncomplete", kSohMainDefault);
    return c;
}

/**
 * One check: its status glyph and name in the status's main colour, then, in its
 * extra colour, what SoH's tracker prints after a name: the found item, with the
 * other game's item marked " (MM)" / " (OoT)" as both native trackers mark a
 * crossing (#796), or "Skipped" for a skipped check that names none. The item
 * follows the name on its line when it fits there, and starts the next line under
 * the name when it does not.
 *
 * On a panel that takes skip writes (`skipColumn`: the LIVE panel, #458 U5) the
 * row starts with SoH's skip button (DrawLocation): a times to skip, a plus to
 * unskip, on a check of the seed not yet found, and an empty square of the same
 * size on every other row so the names stay in one column. A snapshot panel
 * draws neither.
 *
 * With Enable Available Checks on (`availableChecks`, #1129) SoH's reachability
 * glyph follows, as in DrawLocation: an open lock for a check reachable now, a
 * closed one otherwise, in the status's colour, and drawn clear on a found check
 * so the names stay in one column. A row whose availability is not known draws
 * no glyph.
 */
void DrawCheckRow(uint8_t game, const ComboTrackerCheckRow& row, const CheckListColours& colours, bool skipColumn,
                  bool availableChecks) {
    if (skipColumn) {
        char buttonId[24];
        snprintf(buttonId, sizeof(buttonId), "##skip%u", (unsigned)row.checkId);
        if (Combo_TrackerRowSkippable(game, &row)) {
            if (Ui().IconButton(buttonId, row.skipped ? ICON_FA_PLUS : ICON_FA_TIMES, nullptr)) {
                // The flag changes on the next read: the view is recomputed per frame.
                (void)Combo_TrackerSetSkipped(game, row.checkId, !row.skipped);
            }
        } else {
            (void)Ui().IconButton(buttonId, nullptr, nullptr); // the seam's empty square, never a button
        }
        ImGui::SameLine();
    }
    const TextColours& c = colours.status[row.status < (uint8_t)COMBO_TRACKER_CHECK_STATUS_COUNT ? row.status : 0];
    if (availableChecks) {
        if (const char* lockGlyph = ComboAvailabilityGlyph(row)) {
            ImVec4 clear = c.main; // SoH clears the glyph on a found check: the status colour at zero alpha
            clear.w = 0.0f;
            ImGui::PushStyleColor(ImGuiCol_Text, row.obtained ? clear : c.main);
            ImGui::TextUnformatted(lockGlyph);
            ImGui::PopStyleColor();
            ImGui::SameLine();
        }
    }
    char idLabel[24];
    const char* name = ComboCheckListName(row);
    if (name == nullptr) {
        // No name table loaded (e.g. OoT static data before OoT's first boot):
        // the game-local id is still an honest label.
        snprintf(idLabel, sizeof(idLabel), "Check 0x%04X", (unsigned)row.checkId);
        name = idLabel;
    }
    char extra[160] = "";
    const std::string item = ComboCheckRowItemText(game, row);
    if (!item.empty()) {
        snprintf(extra, sizeof(extra), "(%s)", item.c_str());
    } else if (row.skipped) {
        snprintf(extra, sizeof(extra), "(Skipped)");
    }

    ImGui::PushStyleColor(ImGuiCol_Text, c.main);
    ImGui::TextUnformatted(CheckGlyph(row));
    ImGui::SameLine();
    const float nameX = ImGui::GetCursorPosX();
    TextBalanced(name);
    ImGui::PopStyleColor();
    if (extra[0] == '\0') {
        return;
    }
    const bool nameWrapped = ImGui::GetItemRectSize().y > ImGui::GetTextLineHeight() + 0.5f;
    const float right = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x;
    const float limit = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    if (!nameWrapped && right + ImGui::CalcTextSize(extra).x <= limit) {
        ImGui::SameLine();
    } else {
        ImGui::SetCursorPosX(nameX);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, c.extra);
    TextBalanced(extra);
    ImGui::PopStyleColor();
}

/**
 * One game's Checks list (#458 U4), shaped like SoH's Check Tracker: the checks
 * under their areas, each area a tree node in SoH's complete or incomplete
 * colour with its "(checked / total)" beside it, open while it has checks left
 * (SoH's own default) unless Expand All / Collapse All was pressed this frame
 * (`expandAll` 1 / -1), and a padded separator after an open area. Which areas
 * and rows it draws is ComboCollectVisibleCheckAreas' decision (#1128): SoH's
 * hide keys, with the pane's own Show Hidden Items and, while Enable Available
 * Checks is on (`availableChecks`), its own Only Show Available Checks (#1129).
 */
void DrawCheckList(uint8_t game, const PaneState& pane, int expandAll, bool availableChecks) {
    const char* search = pane.search;
    ComboCheckHideOptions hide = ComboReadCheckHideOptions(pane.showHidden);
    hide.onlyAvailable = availableChecks && pane.onlyAvailable;
    std::vector<ComboTrackerAreaRows> areas;
    ComboCollectVisibleCheckAreas(game, search, hide, areas);
    if (areas.empty()) {
        // The note is for a game (or a search) with no checks. When only the
        // Hide options leave nothing, SoH draws an empty list, and so does this.
        std::vector<ComboTrackerAreaRows> unhidden;
        ComboCollectCheckAreas(game, search, unhidden);
        if (unhidden.empty()) {
            Ui().NoteText(search[0] != '\0' ? "No check matches the search." : "No checks.");
        }
        return;
    }

    const CheckListColours colours = ReadCheckListColours();
    // Asked once per frame: the panel takes skip writes only while it is LIVE (#458 U5).
    const bool skipColumn = Combo_TrackerSkipWritable(game);
    bool previousOpen = false;
    // SoH draws its whole area loop under FramePadding (4, 3)
    // (randomizer_check_tracker.cpp, DrawElement), which puts an area's tree
    // arrow level with its checks; the theme's larger padding would push the
    // header to the right of its own rows.
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f));
    for (const ComboTrackerAreaRows& area : areas) {
        if (previousOpen) {
            // SoH's UIWidgets::PaddedSeparator, through the seam's spacer.
            Ui().Spacer(0.0f);
            ImGui::Separator();
            Ui().Spacer(0.0f);
        }
        const bool complete = area.done == area.total;
        const TextColours& c = complete ? colours.areaComplete : colours.areaIncomplete;
        ImGui::PushID((int)area.key);
        ImGui::PushStyleColor(ImGuiCol_Text, c.main);
        if (expandAll != 0) {
            ImGui::SetNextItemOpen(expandAll > 0, ImGuiCond_Always);
        } else {
            ImGui::SetNextItemOpen(!complete, ImGuiCond_Once);
        }
        const bool open =
            ImGui::TreeNodeEx(area.name != nullptr ? area.name : "Other Checks", ImGuiTreeNodeFlags_NoTreePushOnOpen);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, c.extra);
        ImGui::TextUnformatted(ComboAreaTotalsText(area, availableChecks).c_str());
        ImGui::PopStyleColor();
        Ui().Tooltip(availableChecks && area.availabilityKnown ? "Available / Checked / Total" : "Checked / Total");
        if (open) {
            for (const ComboTrackerCheckRow& row : area.rows) {
                DrawCheckRow(game, row, colours, skipColumn, availableChecks);
            }
        }
        ImGui::PopID();
        previousOpen = open;
    }
    ImGui::PopStyleVar();
}

/**
 * One game's panel: SoH's Check Tracker header (#1128) and its Checks list,
 * always drawn as SoH's is. The header is SoH's, control for control, each
 * behind its own SoH visibility key: the pane's Show Hidden Items checkbox, the
 * Expand All / Collapse All buttons, the search box and the totals line, then a
 * separator under whichever of them drew. Fed ONLY by that game's adapter
 * through the view — the game argument is the origin tag, and nothing here
 * compares ids across panels (ADR 0002).
 */
void DrawGamePanel(uint8_t game, const char* title, const ComboTrackerIdentity& identity) {
    if (!ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    ImGui::PushID((int)game);

    ComboTrackerGameSummary summary;
    Combo_TrackerGameSummary(game, &summary);

    if (summary.freshness == COMBO_TRACKER_FRESH_UNAVAILABLE) {
        // No data is NOT "zero progress": an MM shadow that was never written
        // and a never-created OoT heap context land here, and an empty check
        // list under a 0/0 counter would misreport them. The header stands
        // alone, as SoH's trackers show nothing before a file exists (#927).
        ImGui::PopID();
        return;
    }

    if (!summary.hasWorld) {
        // One note, not two: how fresh the data is says nothing without a world.
        Ui().NoteText("Not a randomized world.");
        ImGui::PopID();
        return;
    }

    // No freshness caption (#927): the panel is labelled by its header.
    const char* caption = Combo_TrackerFreshnessLabel(game, summary.freshness);
    if (caption[0] != '\0') {
        Ui().NoteText(caption);
    }

    // ComboPanelShowsOwnSeed (#816): a paired file's pane prints the paired seed
    // on its top line, and its panels print no seed of their own.
    if (ComboPanelShowsOwnSeed(identity, summary)) {
        ImGui::Text("Seed: %u", (unsigned)summary.seed);
    }

    PaneState& pane = Pane(game);
    const ComboCheckHeaderOptions header = ComboReadCheckHeaderOptions();
    if (header.hiddenItemsToggle) {
        const ComboUiWidgetOpts tip = { "When active, this game's checks show even where your Check Tracker Settings "
                                        "hide them.",
                                        false, nullptr };
        (void)Ui().Checkbox("Show Hidden Items", &pane.showHidden, &tip);
    }
    // #1129: SoH's Only Show Available Checks, this pane's own (ruling D12), shown
    // as SoH shows it: only with Enable Available Checks on and its toggle visible.
    const bool availableToggle = header.availableChecks && header.availableChecksToggle;
    if (availableToggle) {
        const ComboUiWidgetOpts tip = { "When active, this game's unavailable checks will be hidden.", false, nullptr };
        (void)Ui().Checkbox("Only Show Available Checks", &pane.onlyAvailable, &tip);
    }
    int expandAll = 0;
    if (header.expandCollapseButtons) {
        // SoH's two half-width buttons.
        if (Ui().Button("Expand All", ImGui::GetContentRegionAvail().x / 2 - 6, nullptr)) {
            expandAll = 1;
        }
        ImGui::SameLine();
        if (Ui().Button("Collapse All", ImGui::GetContentRegionAvail().x - 6, nullptr)) {
            expandAll = -1;
        }
    }
    if (header.searchInput) {
        // Hidden, the box keeps its text and the list its filter, as SoH's does.
        Ui().SearchInput("##checkSearch", pane.search, (int)sizeof(pane.search));
    }
    if (header.checkTotals) {
        ImGui::TextUnformatted(ComboCheckTotalsText(summary, header.availableChecks).c_str());
        Ui().Tooltip(header.availableChecks && summary.availabilityKnown ? "Available / Checked / Total"
                                                                         : "Checked / Total");
    }
    if (header.hiddenItemsToggle || availableToggle || header.expandCollapseButtons || header.searchInput ||
        header.checkTotals) {
        ImGui::Separator();
    }

    DrawCheckList(game, pane, expandAll, header.availableChecks);

    ImGui::PopID();
}

} // namespace

float ComboWidestWordWidth(const char* text, const ComboTextMeasure& measure) {
    float widest = 0.0f;
    if (text == nullptr) {
        return widest;
    }
    const char* p = text;
    while (*p != '\0') {
        while (*p == ' ') {
            p++;
        }
        const char* end = p;
        while (*end != '\0' && *end != ' ') {
            end++;
        }
        if (end > p) {
            const float w = measure.width(measure.user, p, end);
            widest = w > widest ? w : widest;
        }
        p = end;
    }
    return widest;
}

float ComboBalancedWrapWidth(const char* text, float avail, const ComboTextMeasure& measure) {
    if (text == nullptr || avail <= 0.0f) {
        return avail;
    }
    if (measure.width(measure.user, text, text + strlen(text)) <= avail) {
        return avail; // fits on one line
    }
    // The search's floor is the widest word, not 1 px (#815): below it ImGui breaks
    // that word inside itself, and such a wrap can take no more lines than the
    // cell's full width does ("Progressiv" / "e Slingshot"), so a search for the
    // narrowest width with that many lines used to land on it. A word wider than
    // the whole cell cannot be kept whole here at all; the crossing table's Item
    // column is sized so that does not happen (ComboCrossingItemColumnWidth).
    const float widest = ComboWidestWordWidth(text, measure);
    if (widest >= avail) {
        return avail;
    }
    const float height = measure.height(measure.user, text, avail);
    float lo = widest;
    float hi = avail;
    for (int i = 0; i < 12 && hi - lo > 1.0f; i++) {
        const float mid = (lo + hi) * 0.5f;
        if (measure.height(measure.user, text, mid) <= height) {
            hi = mid;
        } else {
            lo = mid;
        }
    }
    return hi;
}

float ComboCrossingItemColumnWidth(float contentWidth, float widestItemWord) {
    const float share = contentWidth * 2.0f / 5.0f;
    const float cap = contentWidth * 3.0f / 5.0f;
    const float width = widestItemWord > share ? widestItemWord : share;
    return width < cap ? width : cap;
}

bool ComboPanelShowsOwnSeed(const ComboTrackerIdentity& identity, const ComboTrackerGameSummary& summary) {
    // A paired file's own seeds are not a second fact: OoT's is the paired seed
    // and MM's final seed is a hash of it and the options
    // (Rando::Foreign::MixPairedFinalSeedForAttempt), so printing either beside
    // the "Paired Seed" line reads as two seeds for one world (#816).
    (void)summary;
    return !identity.paired;
}

const char* ComboCheckListName(const ComboTrackerCheckRow& row) {
    // SoH's DrawLocation prints GetShortName() under the area header ("Kokiri
    // Sword Chest" under "Kokiri Forest"); MM's tracker prints the full name.
    return row.shortName != nullptr ? row.shortName : row.name;
}

std::string ComboCheckRowItemText(uint8_t game, const ComboTrackerCheckRow& row) {
    if (row.placedItemName == nullptr) {
        return std::string();
    }
    std::string text = row.placedItemName;
    if (row.placedItemGame != game) {
        text += (row.placedItemGame == (uint8_t)GAME_MM) ? " (MM)" : " (OoT)";
    }
    return text;
}

const char* ComboAvailabilityGlyph(const ComboTrackerCheckRow& row) {
    if (!row.availabilityKnown) {
        return nullptr;
    }
    // SoH's DrawLocation: `available ? ICON_FA_UNLOCK : ICON_FA_LOCK`.
    return row.available ? ICON_FA_UNLOCK : ICON_FA_LOCK;
}

namespace {

/**
 * ComboCollectCheckAreas, keeping (with `keepFiltered`) each area the search
 * empties, marked `filtered` and without rows, for ComboCheckAreaHidden to
 * decide on.
 */
void CollectCheckAreas(uint8_t game, const char* search, bool keepFiltered, std::vector<ComboTrackerAreaRows>& out) {
    out.clear();
    const ImGuiTextFilter filter(search != nullptr ? search : "");
    std::map<uint16_t, ComboTrackerAreaRows> byKey; // ascending key: each game's own tracker order
    std::string haystack;
    const int count = Combo_TrackerCheckCount(game);
    for (int i = 0; i < count; i++) {
        ComboTrackerCheckRow row;
        if (!Combo_TrackerCheckAt(game, i, &row)) {
            break;
        }
        if (!row.shuffled) {
            continue;
        }
        ComboTrackerAreaRows& area = byKey[row.areaKey];
        area.key = row.areaKey;
        if (area.name == nullptr) {
            area.name = row.areaName;
        }
        area.total++;
        if (row.obtained || row.skipped) {
            area.done++;
        }
        // #1129: like `total`, the whole area, whatever the search hides.
        if (row.availabilityKnown) {
            area.availabilityKnown = true;
            area.available += row.available ? 1 : 0;
        }
        if (filter.IsActive()) {
            // What SoH's ShouldShowCheck searches: the check's short and full
            // names, its area, and the item only once the row reveals it, spelled
            // as the row prints it (PlacedItemTrackerName's " (MM)" included).
            haystack = row.shortName != nullptr ? row.shortName : "";
            haystack += ' ';
            haystack += row.name != nullptr ? row.name : "";
            haystack += ' ';
            haystack += row.areaName != nullptr ? row.areaName : "";
            const std::string item = ComboCheckRowItemText(game, row);
            if (!item.empty()) {
                haystack += ' ';
                haystack += item;
            }
            if (!filter.PassFilter(haystack.c_str())) {
                continue;
            }
        }
        area.rows.push_back(row);
    }
    for (auto& [key, area] : byKey) {
        if (!area.rows.empty()) {
            out.push_back(std::move(area));
        } else if (keepFiltered) {
            // Only the search empties an area: every shuffled row is kept otherwise.
            area.filtered = true;
            out.push_back(std::move(area));
        }
    }
}

} // namespace

void ComboCollectCheckAreas(uint8_t game, const char* search, std::vector<ComboTrackerAreaRows>& out) {
    CollectCheckAreas(game, search, false, out);
}

ComboCheckHideOptions ComboReadCheckHideOptions(bool showHidden) {
    // SoH's keys and defaults (randomizer_check_tracker.cpp, DrawElement):
    // every Hide off, Filter Empty Areas on.
    ComboCheckHideOptions opts;
    for (int i = 0; i < (int)COMBO_TRACKER_CHECK_STATUS_COUNT; i++) {
        opts.hideStatus[i] = CVarGetInteger(kSohStatusColours[i].hideKey, 0) != 0;
    }
    opts.hideAreaComplete = CVarGetInteger("gTrackers.CheckTracker.AreaComplete.Hide", 0) != 0;
    opts.hideAreaIncomplete = CVarGetInteger("gTrackers.CheckTracker.AreaIncomplete.Hide", 0) != 0;
    opts.hideFilteredAreas = CVarGetInteger("gTrackers.CheckTracker.HideFilteredAreas", 1) != 0;
    opts.showHidden = showHidden;
    return opts;
}

bool ComboCheckRowHidden(const ComboTrackerCheckRow& row, const ComboCheckHideOptions& opts) {
    // #1129, SoH's DrawLocation: Only Show Available returns first, so Show Hidden
    // Items does not bring an unavailable check back; a row with no answer stays.
    if (opts.onlyAvailable && row.availabilityKnown && !row.available) {
        return true;
    }
    // SoH's DrawLocation: the row's status (already SoH's precedence: collected,
    // saved, skipped, seen, scummed, unchecked) picks the one Hide that applies.
    return !opts.showHidden && row.status < (uint8_t)COMBO_TRACKER_CHECK_STATUS_COUNT && opts.hideStatus[row.status];
}

bool ComboCheckAreaHidden(const ComboTrackerAreaRows& area, const ComboCheckHideOptions& opts) {
    // SoH's area loop: a filtered area goes whatever Show Hidden Items says; the
    // complete / incomplete hides yield to it.
    if (opts.hideFilteredAreas && area.filtered) {
        return true;
    }
    // #1129: an area with nothing available goes under Only Show Available, as
    // SoH's area loop has it, whatever Show Hidden Items says.
    if (opts.onlyAvailable && area.availabilityKnown && area.available == 0) {
        return true;
    }
    if (opts.showHidden) {
        return false;
    }
    const bool complete = area.done == area.total;
    return complete ? opts.hideAreaComplete : opts.hideAreaIncomplete;
}

void ComboCollectVisibleCheckAreas(uint8_t game, const char* search, const ComboCheckHideOptions& opts,
                                   std::vector<ComboTrackerAreaRows>& out) {
    std::vector<ComboTrackerAreaRows> all;
    CollectCheckAreas(game, search, true, all);
    out.clear();
    for (ComboTrackerAreaRows& area : all) {
        if (ComboCheckAreaHidden(area, opts)) {
            continue;
        }
        std::vector<ComboTrackerCheckRow> shown;
        for (const ComboTrackerCheckRow& row : area.rows) {
            if (!ComboCheckRowHidden(row, opts)) {
                shown.push_back(row);
            }
        }
        area.rows = std::move(shown);
        out.push_back(std::move(area));
    }
}

ComboCheckHeaderOptions ComboReadCheckHeaderOptions() {
    ComboCheckHeaderOptions header;
    header.hiddenItemsToggle = CVarGetInteger("gTrackers.CheckTracker.HiddenItemsToggleVisible", 1) != 0;
    header.expandCollapseButtons = CVarGetInteger("gTrackers.CheckTracker.ExpandCollapseButtonsVisible", 0) != 0;
    header.searchInput = CVarGetInteger("gTrackers.CheckTracker.SearchInputVisible", 1) != 0;
    header.checkTotals = CVarGetInteger("gTrackers.CheckTracker.CheckTotalsVisible", 1) != 0;
    // #1129: SoH's reachability keys (randomizer_check_tracker.cpp, DrawElement).
    header.availableChecks = CVarGetInteger("gTrackers.CheckTracker.EnableAvailableChecks", 0) != 0;
    header.availableChecksToggle = CVarGetInteger("gTrackers.CheckTracker.AvailableChecksToggleVisible", 1) != 0;
    return header;
}

std::string ComboCheckTotalsText(const ComboTrackerGameSummary& summary) {
    // Checked is collected or skipped (the view counts them apart), as SoH's
    // "Checked", MM's collected count and this pane's area headers count it.
    char text[64];
    snprintf(text, sizeof(text), "Checks: %d / %d", summary.obtained + summary.skipped, summary.shuffled);
    return text;
}

std::string ComboCheckTotalsText(const ComboTrackerGameSummary& summary, bool availableChecks) {
    if (!availableChecks || !summary.availabilityKnown) {
        return ComboCheckTotalsText(summary);
    }
    // SoH's DrawElement: "N Available / " in front of its own totals.
    char text[32];
    snprintf(text, sizeof(text), "%d Available / ", summary.available);
    return text + ComboCheckTotalsText(summary);
}

std::string ComboAreaTotalsText(const ComboTrackerAreaRows& area, bool availableChecks) {
    // SoH's area header: "(available / checked / total)" with Enable Available Checks.
    char text[48];
    if (availableChecks && area.availabilityKnown) {
        snprintf(text, sizeof(text), "(%d / %d / %d)", area.available, area.done, area.total);
    } else {
        snprintf(text, sizeof(text), "(%d / %d)", area.done, area.total);
    }
    return text;
}

ComboTrackerChromeKeys ComboReadTrackerChromeKeys() {
    return ComboTrackerChromeKeys();
}

uint32_t ComboTrackerComboButtonMask(int) {
    return 0;
}

ComboTrackerChrome ComboTrackerChromeFor(const ComboTrackerChromeKeys&, bool, uint32_t) {
    ComboTrackerChrome chrome;
    chrome.flags = ImGuiWindowFlags_NoFocusOnAppearing;
    return chrome;
}

/**
 * One direction's crossing table (declared in ComboTrackerWindow.h). The
 * direction is the accessor — the two directions are separate key spaces and
 * are never merged (ADR 0009). SoH's table shape (SohMenuRandomizer.cpp's
 * location tables; the Check Tracker Settings pane's table): cell padding 8x8,
 * horizontal and vertical borders, a header row. The header is a short Title
 * Case name (R-N1) with no count in it (R-N4); the gray note under it says how
 * many, which way, and how many of their checks were collected.
 */
void DrawCrossingList(uint8_t hostGame) {
    const bool inMM = hostGame == (uint8_t)GAME_MM;
    const char* title = inMM ? "In MM Checks" : "In OoT Checks";
    const char* itemGame = inMM ? "Ocarina of Time" : "Majora's Mask";
    const char* hostName = inMM ? "Majora's Mask" : "Ocarina of Time";

    ComboTrackerForeignProgress progress;
    Combo_TrackerForeignProgress(hostGame, &progress);
    const int count = progress.total;
    Ui().SeparatorText(title);
    char note[224];
    if (count == 0) {
        snprintf(note, sizeof(note), "No %s items were placed in %s checks.", itemGame, hostName);
        Ui().NoteText(note);
        return;
    }
    // The second sentence is the host game's own save: how many of these checks
    // it has collected. Not as of when (#927: freshness is no caption). With
    // nothing to read from the host game, no collected count is claimed at all.
    int len = snprintf(note, sizeof(note), "%d %s %s placed in %s checks.", count, itemGame,
                       count == 1 ? "item was" : "items were", hostName);
    if (progress.freshness != COMBO_TRACKER_FRESH_UNAVAILABLE && len > 0 && len < (int)sizeof(note)) {
        snprintf(note + len, sizeof(note) - (size_t)len, " %d collected.", progress.found);
    }
    Ui().NoteText(note);
    ImGui::PushID((int)hostGame);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 8.0f));
    // The Item column's width (#815): its share of the table, widened to the
    // widest word of any item name here so every word fits whole. The columns'
    // content is the table's width less both cells' padding and the three
    // vertical borders.
    float widestItemWord = 0.0f;
    for (int i = 0; i < count; i++) {
        ComboTrackerForeignRow row;
        if (Combo_TrackerForeignRowAt(hostGame, i, &row)) {
            const float w = ComboWidestWordWidth(row.itemName, kImGuiMeasure);
            widestItemWord = w > widestItemWord ? w : widestItemWord;
        }
    }
    const float columnsWidth = ImGui::GetContentRegionAvail().x - 4.0f * 8.0f - 3.0f;
    const float itemWidth = ComboCrossingItemColumnWidth(columnsWidth, widestItemWord);
    if (ImGui::BeginTable("##Crossings", 2, ImGuiTableFlags_BordersH | ImGuiTableFlags_BordersV)) {
        // Check names run longer than item names ("Stone Tower Temple ..."), so the
        // check column takes the larger share; both wrap in balanced lines rather
        // than clip, orphan a word or break one. There is no third column for the collected state: the
        // found state is the glyph leading the check's name, the one notation
        // the per-game Checks lists above use for the same fact.
        ImGui::TableSetupColumn("Check", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthFixed, itemWidth);
        ImGui::TableHeadersRow();
        for (int i = 0; i < count; i++) {
            ComboTrackerForeignRow row;
            if (!Combo_TrackerForeignRowAt(hostGame, i, &row)) {
                break;
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            // The glyph, then the name beside it; a wrapped name's later lines
            // start under its first word, not under the glyph.
            ImGui::TextUnformatted(FoundGlyph(row.found));
            ImGui::SameLine();
            if (row.hostCheckName != nullptr) {
                TextBalanced(row.hostCheckName);
            } else {
                // No name table for the host game (its adapter is not
                // registered): the game-local id is still an honest label.
                char id[24];
                snprintf(id, sizeof(id), "Check 0x%04X", (unsigned)row.hostCheckId);
                TextBalanced(id);
            }
            ImGui::TableNextColumn();
            // The bare display name, as SoH's own item tables print one; the
            // row's article is for a sentence, and a table cell is not one.
            TextBalanced(row.itemName);
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    ImGui::PopID();
}

void BeginComboPaneFit(ComboPaneFit& fit) {
    ImGui::SetNextWindowSize(ImVec2(kComboPaneWidth, kComboPaneHeight), ImGuiCond_FirstUseEver);
    if (fit.growTo > 0.0f && fit.width > 0.0f) {
        ImGui::SetNextWindowSize(ImVec2(fit.width, fit.growTo), ImGuiCond_Always);
        fit.growTo = 0.0f;
    }
    if (fit.contentHeight > 0.0f) {
        ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, fit.contentHeight));
    }
}

void EndComboPaneFit(ComboPaneFit& fit) {
    // Window-local cursor Y includes the title bar and the scroll, so after the
    // last item it is the height the whole pane needs, less the trailing item
    // spacing and plus the bottom padding.
    const ImGuiStyle& style = ImGui::GetStyle();
    const float content = ImGui::GetCursorPosY() - style.ItemSpacing.y + style.WindowPadding.y;
    const ImVec2 size = ImGui::GetWindowSize();
    const bool fitted = fit.contentHeight > 0.0f && size.y >= fit.contentHeight - 0.5f;
    if (fitted && content > fit.contentHeight + 0.5f && size.y < kComboPaneHeight) {
        fit.growTo = content < kComboPaneHeight ? content : kComboPaneHeight;
    }
    fit.contentHeight = content;
    fit.width = size.x;
    fit.height = size.y;
}

void ComboTrackerWindow::Draw() {
    // Read the visibility CVar LIVE rather than trusting the ctor-latched
    // IsVisible() (#489 cause 1; same pattern as ComboSpoilerWindow).
    if (!CVarGetInteger(kComboTrackerVisibilityCVar, 0)) {
        return;
    }

    // SoH's pane chrome (docs/ui-style-guide.md section 10): Ship::GuiWindow::Draw
    // passes its visibility to ImGui::Begin, so an SoH pane has a close button.
    // Closing clears the visibility CVar through SetVisibility, which also
    // schedules the save, as a closed SoH pane does.
    bool open = true;
    BeginComboPaneFit(mFit);
    if (ImGui::Begin(kComboTrackerWindowName, &open, ImGuiWindowFlags_NoFocusOnAppearing)) {
        DrawElement();
        EndComboPaneFit(mFit);
    }
    ImGui::End();
    if (!open) {
        SetVisibility(false);
    }
}

void ComboTrackerWindow::DrawElement() {
    // Identity header. Unlike the spoiler, the per-game panels are NOT gated
    // on pairing — a solo OoT rando session has progress worth showing — so
    // an unpaired world gets one honest line, not an early return.
    ComboTrackerIdentity identity;
    Combo_TrackerIdentity(&identity);
    if (identity.paired) {
        // "Paired Seed", not "Seed": each game panel below may print its own
        // seed, and the two must not read as the same line.
        // The seed string, which can be typed back in (#952), not its hash.
        char seed[256];
        Combo_IdentityDisplay_PairedSeed(identity.sharedRandoSeed, seed, sizeof(seed));
        ImGui::Text("Paired Seed: %s", seed);
    } else {
        Ui().NoteText("No paired world.");
    }
    Ui().Spacer(0.0f);

    Ui().PushTheme();
    DrawGamePanel((uint8_t)GAME_OOT, "Ocarina of Time", identity);
    DrawGamePanel((uint8_t)GAME_MM, "Majora's Mask", identity);
    // No crossing tables (ruling D10, #1128): they were a debug view, and they
    // named items on checks not yet collected. The Cross-Game Spoiler draws them.
    Ui().PopTheme();
}

void RegisterComboTrackerWindow(std::shared_ptr<Ship::Gui> gui) {
    if (gui == nullptr) {
        return;
    }
    // Idempotence, the #457 guard: the production entry point may be reached
    // from more than one bring-up path, and AddGuiWindow rejects duplicates
    // silently rather than loudly.
    if (gui->GetGuiWindow(kComboTrackerWindowName) != nullptr) {
        return;
    }

    gui->AddGuiWindow(std::make_shared<ComboTrackerWindow>(kComboTrackerVisibilityCVar, kComboTrackerWindowName));
}

} // namespace ComboGui

extern "C" void Combo_TrackerWindow_Init(void) {
    // Register both adapters first, unconditionally — even in the headless
    // case below — so the model is populated for tests that never construct a
    // Gui. Explicit calls, not
    // file-scope registrars: the MM half reads std::maps in other TUs whose
    // static init order is unspecified, and a call site cannot be link-elided
    // the way an unreferenced registrar can (#516's dead-registrar class).
    MM_TrackerAdapter_Register();
    OoT_TrackerAdapter_Register();
    // The unified item view's adapters (#458 U1a OoT, U1b MM), registered with
    // the check adapters so the view has them from the same bring-up.
    OoT_ItemAdapter_Register();
    MM_ItemAdapter_Register();

    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetWindow() == nullptr) {
        // ROM-free unit harness: shared subsystems without a window/Gui.
        return;
    }
    auto gui = ctx->GetWindow()->GetGui();
    if (gui == nullptr) {
        return;
    }
    ComboGui::RegisterComboTrackerWindow(gui);
    // The unified Item Tracker overlay (#458 U2), over the item adapters
    // registered above: registered from common code with its sibling, never
    // from either game's boot.
    ComboGui::RegisterComboItemTrackerWindow(gui);
}
