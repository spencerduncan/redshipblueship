/**
 * @file ComboItemTrackerWindow.h
 * @brief The unified Item Tracker overlay: both games' items in one overlay
 *        (#458 U2; ADR 0008, ADR 0004's 2026-09-30 amendment).
 *
 * One overlay covers both games (the operator's ruling on #458): the active
 * game's items live, the other game's from its frozen snapshot, and the shared
 * resource pool, each section labelled with how fresh it is. Every value comes
 * from src/common/combo_item_view.h and is read again every frame; the overlay
 * names no game layout and reads no gSaveContext, which is what makes it safe
 * to draw under GAME_OOT, GAME_MM and GAME_NONE alike (ADR 0008 rule 5).
 *
 * CHROME. SoH's own Item Tracker overlay (randomizer_item_tracker.cpp,
 * BeginFloatingWindows / EndFloatingWindows), read from the combo's own keys
 * (gCombo.Tracker.Items.*, cvar_shared_keys.h): floating by default (no title
 * bar, input-less and fixed unless Draggable, on the main viewport), or a normal
 * auto-sized window; SoH's clear background colour, transparent border and
 * rounding 4; ShowOnlyPaused hides the floating overlay until the active game's
 * pause menu opens. Like SoH's overlay it has no close button: the Combo >
 * Windows row toggles it.
 *
 * ROWS. Text in this slice (icons are #458 U3): each item's name, with its count
 * and ceiling when it has them ("Fairy Bow 35/40"), dimmed while not held (the
 * text twin of SoH's faded icon), in a grid where each item group starts a new
 * line and every section shares one set of column widths. The window type uses
 * kComboItemTrackerColumns columns at full size. The floating overlay cannot
 * scroll, so it is fitted to the game window every frame
 * (ComboItemTrackerFitGrid): up to SoH's six per line, and its text scaled
 * down when even that would run past the edge.
 *
 * Openability: Draw() reads the visibility CVar live, as the Combo Tracker does
 * (#489 cause 1).
 *
 * Locked ROM-free by the ComboItemTrackerWindow CTest (registration, idempotence,
 * name de-collision, Draw()/Update() under all three GameIds with no ImGui
 * context, the ShowOnlyPaused gate, the model reads the draw makes, the chrome
 * and layout decisions below, and a source scan). Its appearance is judged from
 * the UiSnapshot captures (window/Combo Item Tracker beside SoH's Item Tracker).
 */

#ifndef RSBS_COMMON_COMBO_ITEM_TRACKER_WINDOW_H
#define RSBS_COMMON_COMBO_ITEM_TRACKER_WINDOW_H

#ifdef __cplusplus

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <ship/window/gui/GuiWindow.h>

#include "combo_item_view.h"
#include "cvar_shared_keys.h" // RSBS_CVAR_COMBO_WINDOW_ITEM_TRACKER, RSBS_CVAR_COMBO_ITEMS_*

namespace Ship {
class Gui;
}

namespace ComboGui {

// Registration name on the shared Gui, and the ImGui window's name. Distinct
// from SoH's "Item Tracker", MM's "MM Item Tracker" (ImGui id "Item Tracker
// (MM)") and the other common-owned windows: Gui::AddGuiWindow rejects
// duplicates silently, and a shared ImGui id would merge two overlays.
inline constexpr const char* kComboItemTrackerWindowName = "Combo Item Tracker";

// Visibility key, classified by #458 U0 (cvar_shared_keys.h).
inline constexpr const char* kComboItemTrackerVisibilityCVar = RSBS_CVAR_COMBO_WINDOW_ITEM_TRACKER;

/** RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE's values: SoH's TRACKER_WINDOW_FLOATING / _WINDOW. */
enum ComboItemTrackerWindowType : int {
    COMBO_ITEM_TRACKER_FLOATING = 0,
    COMBO_ITEM_TRACKER_WINDOW = 1,
};

/** The overlay's sections, in draw order. */
enum ComboItemTrackerSectionId : int {
    COMBO_ITEM_SECTION_OOT = 0,
    COMBO_ITEM_SECTION_MM = 1,
    COMBO_ITEM_SECTION_SHARED = 2,
    COMBO_ITEM_SECTION_COUNT = 3,
};

/** Text rows per grid line in the window type, and the floating grid's preferred width. */
inline constexpr int kComboItemTrackerColumns = 3;

/** The floating grid's widest line: SoH's DrawItemsInRows puts six icons on a line. */
inline constexpr int kComboItemTrackerMaxColumns = 6;

/**
 * The smallest text scale the floating overlay shrinks to so that it fits the
 * game window. Both games and the pool in text need about 0.45 at the 832x600
 * contract profile (the lock prints the figure), so the floor sits below that.
 */
inline constexpr float kComboItemTrackerMinScale = 0.4f;

/** The fitted scale moves in whole steps of this size, so the overlay does not shimmer frame to frame. */
inline constexpr float kComboItemTrackerScaleStep = 1.0f / 32.0f;

class ComboItemTrackerWindow final : public Ship::GuiWindow {
  public:
    using Ship::GuiWindow::GuiWindow;

    void Draw() override;

  protected:
    void DrawElement() override;
    void InitElement() override {
    }
    void UpdateElement() override {
    }
};

/**
 * Register the overlay on `gui` under kComboItemTrackerWindowName. Idempotent
 * (the #457 GetGuiWindow guard); a null `gui` is a no-op. Called from
 * Combo_TrackerWindow_Init, so the overlay registers with the Combo Tracker.
 */
void RegisterComboItemTrackerWindow(std::shared_ptr<Ship::Gui> gui);

// ============================================================================
// The overlay's decisions, as pure functions the draw code calls, so the
// ROM-free lock can hold them without an ImGui context.
// ============================================================================

/**
 * Whether an open overlay draws this frame, SoH's rule
 * (ItemTrackerWindow::DrawElement): a window-type overlay always does; a
 * floating one does unless ShowOnlyPaused is on and the active game is not
 * paused.
 */
bool ComboItemTrackerShows(int windowType, bool showOnlyPaused, bool paused);

/**
 * The ImGuiWindowFlags BeginFloatingWindows gives the overlay: auto-sized, no
 * focus on appearing, not resizable; floating adds no docking, nav, title bar,
 * mouse scroll or scrollbar, and, unless draggable, no inputs and no move.
 */
int ComboItemTrackerWindowFlags(int windowType, bool draggable);

/** A row as the overlay prints it: the name, then " count/max" when the row has
 *  a ceiling, else " count" when it has a count, else nothing more. A row
 *  neither held nor counted prints its name alone ("Heart Containers", not
 *  "Heart Containers 0/8"), as SoH's faded icon carries no number. */
std::string ComboItemRowText(const ComboItemRow& row);

/** One section as the overlay draws it. */
struct ComboItemTrackerSection {
    int id = COMBO_ITEM_SECTION_OOT;
    const char* title = "";
    uint8_t freshness = COMBO_TRACKER_FRESH_UNAVAILABLE;
    std::string note; // the freshness label as a sentence ("Updated live.")
    std::vector<ComboItemRow> rows;
};

/**
 * Section `section`'s title, freshness, note and rows, read through the item
 * view exactly as the draw reads them: no rows while its data is UNAVAILABLE.
 */
void ComboItemTrackerCollectSection(int section, ComboItemTrackerSection& out);

/** Whether the player's Section.* key shows `section` (default yes). */
bool ComboItemTrackerSectionShown(int section);

/** Where a row sits in its section's grid. */
struct ComboItemGridCell {
    int line;
    int column;
};

/**
 * The grid: rows fill `columns` columns left to right, and a row whose group
 * differs from the previous row's starts a new line, as each of SoH's item
 * sections starts its own row of icons.
 */
void ComboItemGridLayout(const std::vector<ComboItemRow>& rows, int columns, std::vector<ComboItemGridCell>& out);

/** How many grid lines `rows` take in `columns` columns (ComboItemGridLayout's last line + 1; 0 for no rows). */
int ComboItemGridLines(const std::vector<ComboItemRow>& rows, int columns);

/**
 * The grid's column widths, shared by every section so the columns line up
 * down the whole overlay the way SoH's fixed-pitch icon grid does: column c is
 * as wide as the widest row any section puts in column c. `widths[i][r]` is
 * row r of sections[i]'s text width at scale 1.
 */
std::vector<float> ComboItemGridColumnWidths(const std::vector<ComboItemTrackerSection>& sections,
                                             const std::vector<std::vector<float>>& widths, int columns);

/** What the fit is measured against, all in pixels at text scale 1. */
struct ComboItemTrackerMetrics {
    float textHeight = 0.0f;  // a line of text (ImGui::GetFontSize at scale 1); scales
    float linePadding = 0.0f; // a grid line's height beyond its text (ItemSpacing.y); never scales
    float columnGap = 0.0f;   // between two columns; never scales
    float overhead = 0.0f;    // every section's header, note and spacer, as measured last frame
    float availWidth = 0.0f;  // the room the game window leaves the overlay's contents
    float availHeight = 0.0f;
};

/** The grid the overlay draws: columns per line, the text scale, and the shared column widths at scale 1. */
struct ComboItemTrackerFit {
    int columns = kComboItemTrackerColumns;
    float scale = 1.0f;
    std::vector<float> columnWidths;
};

/**
 * The floating overlay's fit (#458 U2 review): the floating overlay has no
 * scrollbar and takes no input, so whatever runs past the game window's edge
 * could never be seen. Of 1..kComboItemTrackerMaxColumns columns per line, the
 * one that lets the text stay largest while every section fits inside
 * availWidth x availHeight; the scale is at most 1 (a grid that fits is never
 * enlarged), rounded down to a whole kComboItemTrackerScaleStep, and at least
 * kComboItemTrackerMinScale. Ties go to the column count nearest
 * kComboItemTrackerColumns, then to the wider grid. With `previous` (the fit
 * drawn last frame) the choice has hysteresis: the previous column count stays
 * unless another beats it by half a step, and the scale steps up only once it
 * fits by half a step more (it steps down at once). SoH scales its own
 * overlays' text the same way (SetWindowFontScale in randomizer_item_tracker.cpp
 * and MM's ItemTracker.cpp).
 */
ComboItemTrackerFit ComboItemTrackerFitGrid(const std::vector<ComboItemTrackerSection>& sections,
                                            const std::vector<std::vector<float>>& widths,
                                            const ComboItemTrackerMetrics& metrics,
                                            const ComboItemTrackerFit* previous = nullptr);

} // namespace ComboGui

#endif // __cplusplus

#endif // RSBS_COMMON_COMBO_ITEM_TRACKER_WINDOW_H
