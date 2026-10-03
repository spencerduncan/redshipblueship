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
 * ICONS (#458 U3). Each game's section is that game's own tracker: its icons
 * (OoT's keyed by texture name, MM's by resource path, both straight from the
 * adapters), laid out as that tracker lays them out (the adapter's
 * ComboItemGridStyle: SoH's six-wide main window flowing on, 11 lines of 36 px
 * icons; MM's one table per group, 46 px cells), faded the way that tracker
 * fades (SoH's _Faded textures; MM's 40% alpha), with that tracker's count on
 * the icon, and the row's text as the icon's tooltip. The Shared section draws
 * the active game's own icons for the pool, two a line. The sections stand side
 * by side, as three columns of one table, so the overlay is as tall as one
 * game's grid. The gCombo.Tracker.Items.IconSize key sizes every grid.
 *
 * TEXT FALLBACK. A section none of whose icons is loaded (MM's load only once
 * MM has booted) is U2's text grid: each item's name, with its count and
 * ceiling when it has them ("Fairy Bow 35/40"), dimmed while not held, in
 * kComboItemTrackerColumns columns, each group on a new line. In a section of
 * icons, a single cell whose texture is missing draws its name in the cell.
 * "No data." is a note with no rows.
 *
 * FIT. The floating overlay cannot scroll, so it is fitted to the game window
 * every frame (ComboItemTrackerFitBoxes): every icon, gap and line of text
 * scales down together when the columns would run past the edge.
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
#include "combo_ui.h" // ComboUiTone
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

/** Text rows per grid line in a text-fallback section. */
inline constexpr int kComboItemTrackerColumns = 3;

/** The IconSize key's default and range: SoH's IconSize default (36) and slider bounds. */
inline constexpr int kComboItemTrackerIconSize = 36;
inline constexpr int kComboItemTrackerIconSizeMin = 25;
inline constexpr int kComboItemTrackerIconSizeMax = 128;

/** A count's text height at scale 1: SoH's item-tracker counts are its 16 px mono font. */
inline constexpr float kComboItemTrackerCountPx = 16.0f;

/**
 * The smallest scale the floating overlay shrinks to so that it fits the game
 * window. Below it the bottom-edge clamp is the backstop.
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
    const ComboItemGridStyle* grid = &kComboItemSohGrid; // the game's own tracker grid (#458 U3)
};

/**
 * Section `section`'s title, freshness, note, rows and grid, read through the
 * item view exactly as the draw reads them: no rows while its data is
 * UNAVAILABLE. `sharedIconGame` (GAME_OOT or GAME_MM) draws the Shared rows
 * with that game's icons instead of the active game's.
 */
void ComboItemTrackerCollectSection(int section, ComboItemTrackerSection& out, int sharedIconGame = -1);

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

// ============================================================================
// Icons (#458 U3)
// ============================================================================

/**
 * The icon grid: rows fill the style's columns left to right (a group's own
 * count when the style names one); with `groupLines` each group starts a new
 * line (MM's one table per group), without it the rows flow on (SoH's main
 * window: its 66 rows are exactly 11 lines of six).
 */
void ComboItemIconLayout(const std::vector<ComboItemRow>& rows, const ComboItemGridStyle& grid,
                         std::vector<ComboItemGridCell>& out);

/** The texture a cell draws and the alpha it draws it at. */
struct ComboItemIconPick {
    const char* key = nullptr;
    float alpha = 1.0f;
};

/**
 * Held: iconKey, opaque. Not held: iconKeyFaded, at fadedAlpha when the row
 * names one (MM draws its own icon at 0.4) and opaque otherwise (SoH's _Faded
 * textures are faded already).
 */
ComboItemIconPick ComboItemPickIcon(const ComboItemRow& row);

/** A count as the icon shows it: the amount, then the ceiling, each in its SoH palette tone. */
struct ComboItemCount {
    std::string amount;
    std::string ceiling;
    ComboUiTone amountTone = COMBO_UI_TONE_WHITE;
    ComboUiTone ceilingTone = COMBO_UI_TONE_GREEN;
};

/**
 * The count drawn on a row's icon, or false for none. A row neither held nor
 * counted has none (a faded icon carries no number). COMBO_ITEM_COUNT_SOH is
 * SoH's DrawItemCount with the ammo shown ("35" then "/40"): the amount green
 * once it reaches the ceiling, gray at 0, white otherwise, the ceiling green;
 * a row with no ceiling shows its amount alone in white. COMBO_ITEM_COUNT_MM
 * is MM's DrawItemCounts: the amount alone, in white, while it is above 0.
 */
bool ComboItemCountFor(const ComboItemRow& row, uint8_t countStyle, ComboItemCount& out);

/**
 * Whether a section draws icons: some row's picked texture is loaded
 * (`hasImage`, the seam's HasImage). Otherwise it is U2's text grid, the
 * fallback while a game's icons are not loaded (MM's before MM's first boot).
 */
bool ComboItemSectionDrawsIcons(const std::vector<ComboItemRow>& rows, bool (*hasImage)(const char*));

// ============================================================================
// The fit
// ============================================================================

/**
 * A section's column of the overlay, in pixels at scale 1: `width` and
 * `height` scale with the fit (icons, gaps, text), `fixedWidth` and
 * `fixedHeight` do not (the table's cell padding, the header and note as
 * measured last frame, the text lines' spacing).
 */
struct ComboItemSectionBox {
    float width = 0.0f;
    float height = 0.0f;
    float fixedWidth = 0.0f;
    float fixedHeight = 0.0f;
};

/** What a box is measured with: the icon size and, for text, the font's metrics, at scale 1. */
struct ComboItemTrackerMetrics {
    float iconSize = (float)kComboItemTrackerIconSize; // the IconSize key
    float textHeight = 0.0f;                           // a line of text (ImGui::GetFontSize at scale 1)
    float linePadding = 0.0f;                          // a text grid line's height beyond its text (ItemSpacing.y)
    float columnGap = 0.0f;                            // between two text columns
    float cellPadding = 0.0f;                          // a table column's padding, both sides together
};

/** The factor a grid style's pixel sizes (given at IconSize 36) take at `iconSize`. */
float ComboItemIconUnit(float iconSize);

/** An icon section's box: its cells' extent plus one gap below the last line (for SoH's counts). */
ComboItemSectionBox ComboItemIconBox(const std::vector<ComboItemGridCell>& cells, const ComboItemGridStyle& grid,
                                     const ComboItemTrackerMetrics& metrics, float header);

/** A text-fallback section's box: U2's text grid in kComboItemTrackerColumns columns. `widths[r]` is row r's
 *  text width at scale 1. */
ComboItemSectionBox ComboItemTextBox(const ComboItemTrackerSection& section, const std::vector<float>& widths,
                                     const ComboItemTrackerMetrics& metrics, float header);

/** The scale the overlay draws at. */
struct ComboItemTrackerFit {
    float scale = 1.0f;
};

/**
 * The floating overlay's fit (#458 U2 review; U3 for icons): the floating
 * overlay has no scrollbar and takes no input, so whatever runs past the game
 * window's edge could never be seen. The boxes stand side by side: the largest
 * scale, at most 1 (a grid that fits is never enlarged), at which their widths
 * together and each one's height fit availWidth x availHeight, rounded down to
 * a whole kComboItemTrackerScaleStep, and at least kComboItemTrackerMinScale.
 * With `previous` (the fit drawn last frame) it has hysteresis: the scale steps
 * up only once it fits by half a step more (it steps down at once). SoH scales
 * its own overlays the same way (SetWindowFontScale in randomizer_item_tracker.cpp,
 * MM's ItemTracker.cpp Scale).
 */
ComboItemTrackerFit ComboItemTrackerFitBoxes(const std::vector<ComboItemSectionBox>& boxes, float availWidth,
                                             float availHeight, const ComboItemTrackerFit* previous = nullptr);

} // namespace ComboGui

#endif // __cplusplus

#endif // RSBS_COMMON_COMBO_ITEM_TRACKER_WINDOW_H
