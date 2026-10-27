/**
 * @file ComboTrackerWindow.h
 * @brief The combo tracker window: both games' progress in one panel (#458;
 *        ADR 0008).
 *
 * Renders src/common/combo_tracker_view.h's model: the combo identity header
 * and one panel per game fed ONLY by that game's registered adapter, each
 * drawn as SoH's Check Tracker is and governed by SoH's Check Tracker Settings
 * keys (#1128), its window chrome included (#1130: Window Type, Enable
 * Dragging, Only Enable While Paused, Display Mode and its combo buttons,
 * Background Color, Font Size). The cross-game crossings are not drawn here (ruling D10): the
 * Cross-Game Spoiler draws them, through DrawCrossingList below. Every panel is labelled by its
 * game's header alone (#927): the inactive game's data is its snapshot (MM's
 * shadow, written at freeze/save; OoT's heap, stopped at suspend), and the
 * panel takes no live write from it (skips are written only while LIVE).
 *
 * Common-owned Gui window (ADR 0008), the ComboSpoilerWindow pattern: it
 * reads gComboCtx and the adapter surfaces and NOTHING else — no gSaveContext
 * through either game's layout — which is what makes it safe to draw under
 * GAME_OOT, GAME_MM and GAME_NONE alike with no MMActiveGated-style wrapper.
 *
 * Openability: `Ship::GuiWindow` latches its visibility CVar in the ctor and
 * nothing re-syncs CVar -> visibility per frame, so `Draw()` reads the CVar
 * live (#489 cause 1, same defect class as MM's check tracker).
 *
 * Locked ROM-free by the ComboTrackerWindow CTest: registration, idempotence,
 * name de-collision, and Draw()/Update() under all three GameIds with no
 * ImGui context. Its APPEARANCE is operator verification — no headless test
 * can assert on pixels.
 */

#ifndef RSBS_COMMON_COMBO_TRACKER_WINDOW_H
#define RSBS_COMMON_COMBO_TRACKER_WINDOW_H

#ifdef __cplusplus

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <ship/window/gui/GuiWindow.h>

#include "combo_tracker_view.h"

namespace Ship {
class Gui;
}

namespace ComboGui {

// Registration name on the shared Gui, and the title (operator ruling D7 of
// 2026-10-22: "just drop the combo"). It is SoH's own window's name: the unified
// window replaces SoH's in the single exe (ruling D1, #1132), so SoH's
// Randomizer > Check Tracker > Toggle Check Tracker, a WINDOW_BUTTON that looks
// its window up BY NAME, opens this one. SoH's own window stays registered under
// "SoH Check Tracker" (games/oot/soh/SohGui/SohRetiredWindow.h), never drawn;
// MM's are "MM "-prefixed. Gui::AddGuiWindow rejects duplicates SILENTLY from
// the caller's side, so a collision here would present as a window that simply
// never appears. Until #1132 the window was "Combo Tracker"; ImGui keys a
// window's saved position by its title, so it takes SoH's own Check Tracker's
// saved place, if the player ever moved that one.
inline constexpr const char* kComboTrackerWindowName = "Check Tracker";

// Visibility CVar: SoH's own Check Tracker key, CVAR_WINDOW("CheckTracker")
// (CMake/soh-cvars.cmake's prefix), which SoH's toggle row reads for its label
// (#1132). Never MM's "gWindows.CheckTracker", which opens MM's own tracker.
// gCombo.Windows.Tracker (cvar_shared_keys.h) held it until #1132.
inline constexpr const char* kComboTrackerVisibilityCVar = "gOpenWindows.CheckTracker";

/**
 * Both panes' size (#755 follow-up): first use opens them at kComboPaneWidth x
 * kComboPaneHeight, and a pane is never taller than what it draws, so a short
 * state (unpaired, or paired with no crossings) is not a tall empty box under a
 * few lines; a long one scrolls, as SoH's panes do. The player can still
 * resize it, up to its content's height. When the content grows while the pane
 * is fitted to it (a file with crossings loaded under an open pane), the pane
 * grows with it, up to kComboPaneHeight.
 */
inline constexpr float kComboPaneWidth = 480.0f;
inline constexpr float kComboPaneHeight = 520.0f;

struct ComboPaneFit {
    float contentHeight = 0.0f; // the last frame's content height, title bar included; 0 = not measured yet
    float width = 0.0f;         // the last frame's window size
    float height = 0.0f;
    float growTo = 0.0f; // nonzero: the height to grow to on the next frame
};

/** Before ImGui::Begin: the first-use size, the content cap, and any growth. */
void BeginComboPaneFit(ComboPaneFit& fit);
/** Inside the window, after its last item: measure what was drawn. */
void EndComboPaneFit(ComboPaneFit& fit);

class ComboTrackerWindow final : public Ship::GuiWindow {
  public:
    using Ship::GuiWindow::GuiWindow;

    void Draw() override;

  protected:
    void DrawElement() override;
    void InitElement() override {
    }
    void UpdateElement() override {
    }

  private:
    ComboPaneFit mFit;
};

/**
 * Register the tracker window on `gui` under kComboTrackerWindowName.
 * Idempotent (the #457 GetGuiWindow guard); a null `gui` is a no-op.
 */
void RegisterComboTrackerWindow(std::shared_ptr<Ship::Gui> gui);

/**
 * One direction's crossings, drawn the one way both panes draw them (#755): a
 * short section header ("In MM Checks" / "In OoT Checks"), a gray note saying
 * how many of the other game's items the host game holds and how many of their
 * checks the host game's save has collected, and SoH's table shape with two
 * columns: the host check's name, led by the same status glyph the per-game
 * Checks lists use (collected, open, or a question mark when the host game has
 * nothing to read), and the item's name. The
 * Cross-Game Spoiler calls this (the Combo Tracker did until ruling D10 cut its
 * Crossings section, #1128). Must be called inside an ImGui
 * window, between the seam's PushTheme/PopTheme or not.
 */
void DrawCrossingList(uint8_t hostGame);

// ============================================================================
// The window's layout and grouping decisions, as pure functions (#458 U4,
// #815, #816), so the ROM-free ComboTrackerWindow lock can hold them without
// an ImGui context. The draw code calls exactly these.
// ============================================================================

/**
 * How text is measured: production passes ImGui::CalcTextSize, the lock a fake
 * font that wraps the way ImGui does (a word wider than the wrap width is broken
 * inside itself).
 */
struct ComboTextMeasure {
    float (*width)(void* user, const char* begin, const char* end); // one line, unwrapped
    float (*height)(void* user, const char* text, float wrapWidth); // wrapped at wrapWidth
    void* user;
};

/** The width of the widest space-separated word of `text` (0 for NULL or ""). */
float ComboWidestWordWidth(const char* text, const ComboTextMeasure& measure);

/**
 * The wrap width a table cell draws `text` at, given `avail` pixels: the
 * narrowest width that takes no more lines than `avail` does (a long name breaks
 * into even lines rather than leaving its last word alone), but never narrower
 * than the text's widest word, so no word is broken inside itself
 * ("Progressiv" / "e Slingshot", #815). Text that fits is drawn at `avail`.
 */
float ComboBalancedWrapWidth(const char* text, float avail, const ComboTextMeasure& measure);

/**
 * The crossing table's Item column width for a table `contentWidth` wide (the
 * columns' content, padding excluded): its two-fifths share, widened to the
 * widest word of any item name in the table (`widestItemWord`) so the column can
 * hold every word whole (#815), but never past three fifths, so the Check column
 * keeps the larger share.
 */
float ComboCrossingItemColumnWidth(float contentWidth, float widestItemWord);

/**
 * Whether a game panel prints its own "Seed:" line (#816). The pane's top line
 * prints the paired seed on a paired file, and a game's own final seed there is
 * not a second fact a player needs (MM's is a hash of the paired seed, OoT's is
 * the paired seed), so a paired file's panels print none; an unpaired world's
 * panel prints its game's seed, since nothing else on the pane does.
 */
bool ComboPanelShowsOwnSeed(const ComboTrackerIdentity& identity, const ComboTrackerGameSummary& summary);

/**
 * The name a Checks-list row prints under its area header (#458 U4): the
 * check's short name when its game's own tracker prints one there (OoT, SoH's
 * DrawLocation), else its full name. NULL when the row has neither (no name
 * table loaded): the caller prints the game-local id then.
 */
const char* ComboCheckListName(const ComboTrackerCheckRow& row);

/**
 * The item a Checks-list row names, spelled as the game's own check tracker
 * spells it (#796): the placed item's name, with " (MM)" / " (OoT)" when the
 * item is the other game's (a crossing host). "" when the row names no item.
 * The row draws it in parentheses, and the search matches it, as SoH's
 * ShouldShowCheck matches PlacedItemTrackerName. A row with a price (#1166)
 * follows the name with " - <price>", or prints the price alone when it names
 * no item, as SoH's DrawLocation does.
 */
std::string ComboCheckRowItemText(uint8_t game, const ComboTrackerCheckRow& row);

/**
 * What the search box matches a Checks-list row against (#1132): what SoH's
 * ShouldShowCheck matches, the check's short and full names and its area's
 * name, then the item the row reveals (ComboCheckRowItemText's name and game
 * suffix) when it reveals one.
 */
std::string ComboCheckRowSearchText(uint8_t game, const ComboTrackerCheckRow& row);

/**
 * The reachability glyph a row draws before its check (#1129): SoH's
 * DrawLocation glyph, ICON_FA_UNLOCK for an available check and ICON_FA_LOCK
 * for one that is not, or NULL when the row's availability is not known (no
 * glyph at all). The draw shows it only while Enable Available Checks is on, and
 * clear on a found check as SoH does, so the names stay in one column.
 */
const char* ComboAvailabilityGlyph(const ComboTrackerCheckRow& row);

/**
 * The tooltip a Checks-list row shows on hover (#1161): the check's logic as its
 * game's own check tracker shows it (Combo_TrackerCheckLogic), while `showLogic`
 * (SoH's Show Logic, gTrackers.CheckTracker.ShowLogic, one key for both panes) is
 * on. "" when it is off, when the game has no condition for the check, or when
 * the condition is "true" (SoH draws no tooltip then).
 */
std::string ComboCheckLogicTooltip(uint8_t game, const ComboTrackerCheckRow& row, bool showLogic);

/**
 * One area of a game panel's Checks list (#458 U4): the area's game-local key
 * and name, its shuffled-check count and how many of them are done (collected
 * or skipped: SoH's and MM's trackers both count a skipped check as checked),
 * and the rows the search leaves, in table order. `total` and `done` count the
 * whole area, whatever the search hides, as SoH's area totals do.
 */
struct ComboTrackerAreaRows {
    uint16_t key = 0;
    const char* name = nullptr; // NULL when the game's adapter names no area
    int total = 0;
    int done = 0;
    bool filtered = false; // the search is active and leaves none of the area's rows (#1128)
    // #1129: the area's rows reachable now, counted like `total` (the whole
    // area), and whether any row's availability is known.
    int available = 0;
    bool availabilityKnown = false;
    std::vector<ComboTrackerCheckRow> rows;
};

/**
 * `game`'s shuffled checks grouped by area, areas in ascending key order (each
 * game's own tracker order), keeping only the rows `search` matches (ImGui's
 * text-filter syntax, SoH's search box: comma-separated terms, a leading '-'
 * excludes). A row is matched on its check name, its area name and the item it
 * names when its status reveals one, never on an item it does not reveal. An
 * area the search leaves empty is omitted. `search` NULL or "" keeps every row.
 */
void ComboCollectCheckAreas(uint8_t game, const char* search, std::vector<ComboTrackerAreaRows>& out);

/**
 * What a Checks list hides (#1128), read from SoH's Check Tracker Settings keys
 * (gTrackers.CheckTracker.*) with SoH's defaults, everything shown: each
 * status's "<State>.Hide", "AreaComplete.Hide" and "AreaIncomplete.Hide", and
 * "HideFilteredAreas" (SoH's "Filter Empty Areas", on by default). `showHidden`
 * is the pane's own Show Hidden Items toggle (ruling D12: per pane), which
 * overrides every hide but the filtered-area one, as SoH's does.
 */
struct ComboCheckHideOptions {
    bool hideStatus[COMBO_TRACKER_CHECK_STATUS_COUNT] = {};
    bool hideAreaComplete = false;
    bool hideAreaIncomplete = false;
    bool hideFilteredAreas = true;
    bool showHidden = false;
    // #1129: the pane's own Only Show Available Checks (ruling D12: per pane),
    // in force only while Enable Available Checks is on. It leaves out every
    // row known not to be available, whatever Show Hidden Items says, and
    // every area with none available (SoH's DrawLocation and area loop); a row
    // whose availability is not known is never left out.
    bool onlyAvailable = false;
    // #1131: the pane's own Only Show Current Area (ruling D12: per pane),
    // with the area its game's player is in (Combo_TrackerCurrentArea). It
    // leaves out every row and area outside `currentArea`, whatever Show Hidden
    // Items says; with no area known (`currentAreaKnown` false) nothing goes.
    bool onlyCurrentArea = false;
    bool currentAreaKnown = false;
    uint16_t currentArea = 0;
};

/** The hide keys' values now, with `showHidden` as the pane's toggle. */
ComboCheckHideOptions ComboReadCheckHideOptions(bool showHidden);

/** Whether a Checks list leaves out `row` (SoH's DrawLocation: its status's Hide, unless Show Hidden Items;
 *  #1129's Only Show Available and #1131's Only Show Current Area first, whatever Show Hidden Items says). */
bool ComboCheckRowHidden(const ComboTrackerCheckRow& row, const ComboCheckHideOptions& opts);

/**
 * Whether a Checks list leaves out `area`'s header (SoH's DrawElement area
 * loop): an area the search emptied while Filter Empty Areas is on, or, unless
 * Show Hidden Items, a complete area under AreaComplete.Hide or an incomplete
 * one under AreaIncomplete.Hide. An area whose rows are all hidden by status
 * keeps its header, as SoH's does. Under Only Show Available (#1129) an area with
 * none available goes, and under Only Show Current Area (#1131) every area but
 * the current one, whatever Show Hidden Items says.
 */
bool ComboCheckAreaHidden(const ComboTrackerAreaRows& area, const ComboCheckHideOptions& opts);

/**
 * Exactly what a Checks list draws (#1128): ComboCollectCheckAreas' areas, every
 * area the search emptied kept (with `filtered` set and no rows) unless
 * ComboCheckAreaHidden leaves it out, and each kept area's rows less those
 * ComboCheckRowHidden leaves out. Area totals still count the whole area.
 */
void ComboCollectVisibleCheckAreas(uint8_t game, const char* search, const ComboCheckHideOptions& opts,
                                   std::vector<ComboTrackerAreaRows>& out);

/**
 * Which of SoH's Check Tracker header controls a pane draws (#1128), from SoH's
 * keys with SoH's defaults: the Show Hidden Items checkbox
 * ("HiddenItemsToggleVisible", on), the Expand All / Collapse All buttons
 * ("ExpandCollapseButtonsVisible", off), the search box ("SearchInputVisible",
 * on) and the totals line ("CheckTotalsVisible", on).
 */
struct ComboCheckHeaderOptions {
    bool hiddenItemsToggle = true;
    bool expandCollapseButtons = false;
    bool searchInput = true;
    bool checkTotals = true;
    // #1129: SoH's Enable Available Checks ("EnableAvailableChecks", off): the
    // glyphs, the Available counts and the toggle below show only while it is
    // on; and SoH's Available Checks Toggle ("AvailableChecksToggleVisible", on):
    // the pane's Only Show Available Checks checkbox.
    bool availableChecks = false;
    bool availableChecksToggle = true;
    // #1161: SoH's Show Logic ("ShowLogic", off): each row's logic tooltip.
    bool showLogic = false;
};

ComboCheckHeaderOptions ComboReadCheckHeaderOptions();

/**
 * Whether a pane draws its header at all, and so the separator under it (#1132):
 * SoH's DrawElement draws that separator only while one of its header controls
 * is visible ("headerPresent").
 */
bool ComboCheckHeaderPresent(const ComboCheckHeaderOptions& header);

// ---- Navigation (#1131, lane L4) --------------------------------------------
//
// The Combo Tracker's first keys of its own, since SoH has neither setting
// (its window always scrolls, and has no current-area filter; MM's tracker has
// both, gRando.CheckTracker.ScrollToCurrentScene and .ShowCurrentScene):
// gCombo.Tracker.Checks.AutoScrollToCurrentArea (on) and
// gCombo.Tracker.Checks.OnlyShowCurrentArea (off), one each for both panes
// (ruling D3: one set of settings). The second only seeds each pane's own
// toggle (ruling D12), as SoH's ShowHidden seeds Show Hidden Items.

struct ComboTrackerNavKeys {
    bool autoScroll = true;       // AutoScrollToCurrentArea: MM's Auto Scroll To Current Scene default
    bool onlyCurrentArea = false; // OnlyShowCurrentArea: MM's Only Show Current Scene default
};

/** The two navigation keys' values now, their defaults for a key not set. */
ComboTrackerNavKeys ComboReadTrackerNavKeys();

/**
 * One pane's scroll to its current area (SoH's doAreaScroll, MM's
 * sScrollToTargetScene): a scroll is owed once per area change, only on the
 * ACTIVE game's pane (the inactive pane's area is a snapshot and never
 * scrolls it), and paid at that area's header the first frame it is drawn.
 */
struct ComboAreaScroll {
    bool seen = false;    // an area has been noted
    uint16_t area = 0;    // the last area noted
    bool pending = false; // a scroll is owed
    uint16_t target = 0;  // to this area
};

/**
 * Note the pane's current area this frame: `areaKnown` false notes nothing (a
 * transition's gap does not count as a change); the first area, or one that
 * differs from the last noted, is a change, which owes a scroll when
 * `activePane` and `autoScroll`, and otherwise drops any owed one. The same
 * area again changes nothing. @return whether a scroll is now owed.
 */
bool ComboAreaScrollNote(ComboAreaScroll& scroll, bool activePane, bool autoScroll, bool areaKnown, uint16_t area);

/**
 * At an area's header: true exactly once for the owed scroll's area (the
 * caller then scrolls there, SoH's ImGui::SetScrollHereY(0.0f)), clearing it;
 * false for any other area and when nothing is owed.
 */
bool ComboAreaScrollTake(ComboAreaScroll& scroll, uint16_t areaKey);

/**
 * One pane's Only Show Current Area toggle (ruling D12), seeded from the key:
 * whenever the key's value differs from the one it last seeded from, the
 * toggle takes it (so the settings row reaches both panes); between changes,
 * the pane's checkbox flips its own toggle alone and nothing is written back.
 */
struct ComboPaneAreaToggle {
    bool onlyCurrentArea = false;
    int seededFrom = -1; // the key's value at the last seed; -1 never seeded
};

void ComboPaneAreaToggleSeed(ComboPaneAreaToggle& toggle, bool keyValue);

/**
 * Put a pane's Only Show Current Area into `opts`: `onlyCurrentArea` (the
 * pane's toggle) and `game`'s current area (Combo_TrackerCurrentArea).
 */
void ComboApplyCurrentAreaFilter(ComboCheckHideOptions& opts, uint8_t game, bool onlyCurrentArea);

/**
 * A game panel's totals line (#1128): its checked count over its shuffled
 * count, where checked is collected or skipped, as SoH's Checked and MM's
 * collected count are, and as the panel's own area headers count.
 */
std::string ComboCheckTotalsText(const ComboTrackerGameSummary& summary);

/**
 * The totals line with SoH's Available count in front (#1129): "N Available / "
 * then ComboCheckTotalsText's line, as SoH prints "N Available / " before its
 * own totals, when `availableChecks` (Enable Available Checks) is on and the
 * summary's availability is known; ComboCheckTotalsText's line otherwise.
 */
std::string ComboCheckTotalsText(const ComboTrackerGameSummary& summary, bool availableChecks);

/**
 * An area header's totals (#1129): SoH's "(available / checked / total)" when
 * `availableChecks` is on and the area's availability is known, else
 * "(checked / total)".
 */
std::string ComboAreaTotalsText(const ComboTrackerAreaRows& area, bool availableChecks);

// ---- The window's chrome (#1130, lane L1b) ---------------------------------
//
// SoH's Check Tracker chrome keys, read under SoH's names with SoH's defaults
// (randomizer_check_tracker.cpp, CheckTrackerWindow::DrawElement, and
// Trackers::BeginFloatWindows): the Window Type, Enable Dragging, Only Enable
// While Paused, Display Mode with its two combo buttons, the background colour
// and the font size. The values are SoH's TrackerWindowType, TrackerDisplayType
// and TrackerComboButton (randomizerTypes.h), which src/common cannot include.

inline constexpr int kComboTrackerWindowFloating = 0;     // TRACKER_WINDOW_FLOATING
inline constexpr int kComboTrackerWindowWindow = 1;       // TRACKER_WINDOW_WINDOW, SoH's default
inline constexpr int kComboTrackerDisplayAlways = 0;      // TRACKER_DISPLAY_ALWAYS, SoH's default
inline constexpr int kComboTrackerDisplayComboButton = 1; // TRACKER_DISPLAY_COMBO_BUTTON
inline constexpr int kComboTrackerComboButtonL = 6;       // TRACKER_COMBO_BUTTON_L, the hold's Combo Button 1 default
inline constexpr int kComboTrackerComboButtonR = 8;       // TRACKER_COMBO_BUTTON_R, the hold's Combo Button 2 default
inline constexpr int kComboTrackerComboButtonCount = 14;  // A .. D-Right

struct ComboTrackerChromeKeys {
    int windowType = kComboTrackerWindowWindow;
    bool draggable = true;
    bool showOnlyPaused = false;
    int displayType = kComboTrackerDisplayAlways;
    int comboButton1 = kComboTrackerComboButtonL;
    int comboButton2 = kComboTrackerComboButtonR;
    uint8_t bg[4] = { 0, 0, 0, 255 }; // BgColor.Value, RGBA: SoH's Color_Bg_Default, opaque black
    float fontSize = 1.0f;
};

/** The chrome keys' values now (gTrackers.CheckTracker.*), SoH's default for any key not set. */
ComboTrackerChromeKeys ComboReadTrackerChromeKeys();

/**
 * The pad bit a Combo Button value names: SoH's `buttons` table
 * (randomizer_entrance_tracker.h), BTN_A .. BTN_DRIGHT in TrackerComboButton
 * order. 0 for a value outside it, which no held pad matches.
 */
uint32_t ComboTrackerComboButtonMask(int button);

/** The first pad's held buttons, where SoH's trackers read them for their combo-button hold; 0 with no pad
 *  (#1141: the Item Tracker's hold reads the same). */
uint32_t ComboTrackerHeldPadButtons();

/**
 * What the window does this frame. `draws` false: it draws nothing (and keeps
 * its visibility). `flags` are the ImGuiWindowFlags it begins with;
 * `closeButton` is whether it passes ImGui::Begin an open flag (the title bar's
 * close button); `mainViewport` pins it to the game window's viewport.
 */
struct ComboTrackerChrome {
    bool draws = true;
    int flags = 0;
    bool closeButton = true;
    bool mainViewport = false;
};

/**
 * SoH's chrome decision (DrawElement's floating gates and BeginFloatWindows'
 * flags) for `keys`, the active game's pause state `paused` and the first
 * pad's held buttons `padButtons`. The Window type always draws, as a titled,
 * dockable pane with a close button. The Floating type is an overlay on the
 * main viewport with no title bar, no docking, no nav and no close button
 * (SoH's overlays and the Combo Item Tracker have none), input-less and fixed
 * unless Draggable; it draws only while paused under Only Enable While Paused,
 * and only while both combo buttons are held under Combo Button Hold. Both
 * types keep the pane's own scrolling, since the pane scrolls as a whole where
 * SoH's scrolls an inner table that its NoScrollbar / NoScrollWithMouse do not
 * reach, and both keep no focus on appearing.
 */
ComboTrackerChrome ComboTrackerChromeFor(const ComboTrackerChromeKeys& keys, bool paused, uint32_t padButtons);

} // namespace ComboGui

extern "C" {
#endif // __cplusplus

/**
 * Production entry point. Registers both games' tracker adapters and both
 * games' item adapters (#458 U1) (so the models are populated even for tests that
 * never construct a Gui), then the
 * window and the unified Item Tracker overlay (#458 U2, ComboItemTrackerWindow.h)
 * on the shared Ship::Context Gui. Safe no-op past the adapter step
 * when the context has no window/Gui — the state every ROM-free harness runs
 * in.
 */
void Combo_TrackerWindow_Init(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_TRACKER_WINDOW_H
