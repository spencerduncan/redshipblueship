/**
 * @file ComboTrackerWindow.h
 * @brief The combo tracker window: both games' progress in one panel (#458;
 *        ADR 0008).
 *
 * Renders src/common/combo_tracker_view.h's model: the combo identity header,
 * one panel per game fed ONLY by that game's registered adapter, and the
 * cross-game crossings in both directions (the crossing store, plus the legacy
 * pinned tables; combo_tracker_view.h "Cross-game crossings"). Every panel is labelled with its
 * freshness — the inactive game's data is honest about being "As of the last
 * game switch or save" (MM's shadow, written at freeze/save) or "As of the last
 * game switch" (OoT's heap, stopped at suspend), never presented as live.
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

// Registration name on the shared Gui. Distinct from SoH's unprefixed tracker
// names, MM's "MM "-prefixed ones, and the other common-owned windows:
// Gui::AddGuiWindow rejects duplicates SILENTLY from the caller's side, so a
// collision here would present as a window that simply never appears.
inline constexpr const char* kComboTrackerWindowName = "Combo Tracker";

// Visibility CVar in the "gCombo.*" namespace (neither OoT's "gOpenWindows.*"
// nor MM's "gWindows.*", so no store collision).
inline constexpr const char* kComboTrackerVisibilityCVar = "gCombo.Windows.Tracker";

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
 * Combo Tracker and the Cross-Game Spoiler both call this, so the two panes
 * cannot list a world's crossings differently. Must be called inside an ImGui
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
 * ShouldShowCheck matches PlacedItemTrackerName.
 */
std::string ComboCheckRowItemText(uint8_t game, const ComboTrackerCheckRow& row);

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

} // namespace ComboGui

extern "C" {
#endif // __cplusplus

/**
 * Production entry point. Registers both games' tracker adapters and both
 * games' item adapters (#458 U1) (so the models are populated even for tests that
 * never construct a Gui), then the
 * window on the shared Ship::Context Gui. Safe no-op past the adapter step
 * when the context has no window/Gui — the state every ROM-free harness runs
 * in.
 */
void Combo_TrackerWindow_Init(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_TRACKER_WINDOW_H
