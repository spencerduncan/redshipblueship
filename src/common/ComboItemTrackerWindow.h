/**
 * @file ComboItemTrackerWindow.h
 * @brief The unified Item Tracker overlay: both games' items in one overlay
 *        (#458 U2; ADR 0008, ADR 0004's 2026-09-30 amendment).
 *
 * One overlay covers both games (the operator's ruling on #458): the active
 * game's items live, the other game's from its frozen snapshot, and the shared
 * resource pool, each section labelled by its header alone, as SoH's trackers
 * label theirs: no caption says how fresh it is (#927). Every value comes
 * from src/common/combo_item_view.h and is read again every frame; the overlay
 * names no game layout and reads no gSaveContext, which is what makes it safe
 * to draw under GAME_OOT, GAME_MM and GAME_NONE alike (ADR 0008 rule 5).
 *
 * CHROME. SoH's own Item Tracker overlay (randomizer_item_tracker.cpp,
 * BeginFloatingWindows / EndFloatingWindows), read from SoH's own Item Tracker
 * Settings keys (#1141, operator ruling IT-D6: one set of settings, SoH's;
 * ComboReadItemTrackerKeys): floating by default (no title bar, input-less and
 * fixed unless Enable Dragging, on the main viewport), or a normal auto-sized
 * window; SoH's background colour (clear by default), transparent border and
 * rounding 4; Only Enable While Paused hides the floating overlay until the
 * active game's pause menu opens, and Display Mode Combo Button Hold until both
 * of its combo buttons are held. Like SoH's overlay it has no close button:
 * SoH's Randomizer > Item Tracker > Toggle Item Tracker toggles it.
 *
 * POSITION (#962). The floating overlay first appears at the left edge, just
 * below the HUD's hearts and magic bar (ComboItemTrackerDefaultPos), not on
 * them; a dragged position is kept (ImGui's saved window settings, as SoH's
 * trackers keep theirs) and Randomizer > Item Tracker > Move to Default
 * Position returns it, also from an old saved position on the hearts.
 *
 * ICONS (#458 U3). Each game's section is that game's own tracker: its icons
 * (OoT's keyed by texture name, MM's by resource path, both straight from the
 * adapters), laid out as that tracker lays them out (the adapter's
 * ComboItemGridStyle: SoH's six-wide main window flowing on, 11 lines of 36 px
 * icons; MM's one table per group, 46 px cells), faded the way that tracker
 * fades (SoH's _Faded textures; MM's 40% alpha), with that tracker's count on
 * the icon (SoH's in its tracker's own mono font), and the item's name as the
 * icon's tooltip. The Shared section draws the active game's own icons for the
 * pool, four a line, tinted and numbered as that game draws them. The sections
 * stand side by side, as three columns of one table, each at least as wide as
 * its ruled header, so the overlay is as tall as one game's grid. SoH's Icon
 * size sizes every grid. A group the player hid is left out (#1141): MM's under
 * its own key (Combo_ItemGroupVisibilityKey), OoT's under SoH's
 * DisplayType.<Section>.
 *
 * NOT HONOURED YET (#1141 reads them; their lanes consume them): SoH's Icon
 * margins and Text size, its Ammo/Capacity, Align count to left side, Key
 * Count and Triforce Piece count modes and Hookshot identifier (lane IT-4,
 * #1143), its Maps and compasses (lane IT-2, #1142), and the Separate display
 * type (the overlay is one window: Separate shows a section in it).
 *
 * TEXT FALLBACK. A section none of whose icons is loaded (MM's load only once
 * MM has booted) is U2's text grid: each item's name, with its count and
 * ceiling when it has them ("Fairy Bow 35/40"), dimmed while not held, in
 * kComboItemTrackerColumns columns, each group on a new line. In a section of
 * icons, a single cell whose texture is missing draws its name in the cell.
 * A section with no data is its header with no rows (#927).
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
 * the UiSnapshot captures (window/Item Tracker beside SoH's own, window/SoH Item Tracker).
 */

#ifndef RSBS_COMMON_COMBO_ITEM_TRACKER_WINDOW_H
#define RSBS_COMMON_COMBO_ITEM_TRACKER_WINDOW_H

#ifdef __cplusplus

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>
#include <ship/window/gui/GuiWindow.h>

#include "combo_item_view.h"
#include "combo_ui.h"         // ComboUiTone
#include "cvar_shared_keys.h" // the overlay's own keys (RSBS_CVAR_COMBO_ITEMS_*), RSBS::kComboRetiredKeys

namespace Ship {
class Gui;
}

namespace ComboGui {

// Registration name on the shared Gui, and the ImGui window's name: SoH's own
// window's (rulings D1 and D7, #1132), so SoH's Randomizer > Item Tracker >
// Toggle Item Tracker opens this overlay by name. SoH's own overlay stays
// registered as "SoH Item Tracker", never drawn (SohRetiredWindow.h); MM's is
// "MM Item Tracker" (ImGui id "Item Tracker (MM)"). Gui::AddGuiWindow rejects
// duplicates silently, and a shared ImGui id would merge two overlays.
inline constexpr const char* kComboItemTrackerWindowName = "Item Tracker";

// Visibility key: SoH's own CVAR_WINDOW("ItemTracker") (#1132), never MM's
// gWindows.ItemTracker. RSBS_CVAR_COMBO_WINDOW_ITEM_TRACKER held it until then.
inline constexpr const char* kComboItemTrackerVisibilityCVar = "gOpenWindows.ItemTracker";

/** SoH's Window Type values (gTrackers.ItemTracker.WindowType): TRACKER_WINDOW_FLOATING / _WINDOW. */
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

/** A count's text height at scale 1 when SoH's own tracker font is not loaded (its mono font is 16 px). */
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

// ---- SoH's Item Tracker Settings keys (#1141, lane IT-3; operator ruling IT-D6) ----
//
// One set of settings, SoH's: the overlay reads SoH's gTrackers.ItemTracker.*
// keys, under SoH's names with SoH's defaults (randomizer_item_tracker.cpp:
// ItemTrackerWindow::DrawElement, BeginFloatingWindows, DrawItemsInRows,
// ItemTrackerSettingsWindow::DrawElement and RegisterItemTrackerWidgets), and
// SoH's own Item Tracker Settings window (Randomizer > Item Tracker > Popout
// Item Tracker Settings) writes them. ComboReadItemTrackerKeys is the only read
// of them, and of the overlay's own keys SoH has no twin for (its Opacity and
// which of its sections it draws). The values are SoH's enums
// (randomizerTypes.h, randomizer_item_tracker.cpp), which src/common cannot
// include.

inline constexpr int kComboItemTrackerDisplayAlways = 0;       // TRACKER_DISPLAY_ALWAYS, SoH's default
inline constexpr int kComboItemTrackerDisplayComboButton = 1;  // TRACKER_DISPLAY_COMBO_BUTTON
inline constexpr int kComboItemTrackerComboButtonL = 6;        // TRACKER_COMBO_BUTTON_L, Combo Button 1's default
inline constexpr int kComboItemTrackerComboButtonR = 8;        // TRACKER_COMBO_BUTTON_R, Combo Button 2's default
inline constexpr int kComboItemTrackerSectionHidden = 0;       // SECTION_DISPLAY_HIDDEN
inline constexpr int kComboItemTrackerSectionMainWindow = 1;   // SECTION_DISPLAY_MAIN_WINDOW
inline constexpr int kComboItemTrackerIconSpacing = 12;        // IconSpacing's default (Icon margins)
inline constexpr int kComboItemTrackerTextSize = 13;           // TextSize's default
inline constexpr int kComboItemTrackerCountCapacityOnly = 1;   // ITEM_TRACKER_NUMBER_CURRENT_CAPACITY_ONLY
inline constexpr int kComboItemTrackerKeysCollectedMax = 0;    // KEYS_COLLECTED_MAX
inline constexpr int kComboItemTrackerTriforceRequiredMax = 1; // TRIFORCE_PIECE_COLLECTED_REQUIRED_MAX
// The rest of SoH's count modes and its Icon margins / Text size slider bounds (#1143, lane IT-4).
inline constexpr int kComboItemTrackerCountNone = 0;               // ITEM_TRACKER_NUMBER_NONE
inline constexpr int kComboItemTrackerCountAmmoOnly = 2;           // ITEM_TRACKER_NUMBER_CURRENT_AMMO_ONLY
inline constexpr int kComboItemTrackerCountCapacity = 3;           // ITEM_TRACKER_NUMBER_CAPACITY
inline constexpr int kComboItemTrackerCountAmmo = 4;               // ITEM_TRACKER_NUMBER_AMMO
inline constexpr int kComboItemTrackerKeysCurrentCollectedMax = 1; // KEYS_CURRENT_COLLECTED_MAX
inline constexpr int kComboItemTrackerKeysCurrentMax = 2;          // KEYS_CURRENT_MAX
inline constexpr int kComboItemTrackerTriforceRequired = 0;        // TRIFORCE_PIECE_COLLECTED_REQUIRED
inline constexpr int kComboItemTrackerIconSpacingMin = -5;         // Icon margins' slider bounds
inline constexpr int kComboItemTrackerIconSpacingMax = 50;
inline constexpr int kComboItemTrackerTextSizeMin = 1; // Text size's slider bounds
inline constexpr int kComboItemTrackerTextSizeMax = 30;

/** SoH's main-window sections the OoT section's groups are, in SoH's order; each has a
 *  DisplayType.<Section> key (Hidden, Main Window, Separate), default Main Window. */
inline constexpr int kComboItemTrackerSohSectionCount = 5;
extern const char* const kComboItemTrackerSohSectionGroups[kComboItemTrackerSohSectionCount];

/** The overlay's settings, as read this frame. */
struct ComboItemTrackerKeys {
    // SoH's General settings.
    int windowType = COMBO_ITEM_TRACKER_FLOATING;                   // WindowType
    bool draggable = false;                                         // Draggable (Enable Dragging), floating only
    bool showOnlyPaused = false;                                    // ShowOnlyPaused (Only Enable While Paused)
    int displayMode = kComboItemTrackerDisplayAlways;               // DisplayType.Main (Display Mode), floating only
    int comboButton1 = kComboItemTrackerComboButtonL;               // ComboButton1, under Combo Button Hold
    int comboButton2 = kComboItemTrackerComboButtonR;               // ComboButton2
    uint8_t bg[4] = { 0, 0, 0, 0 };                                 // BgColor.Value, RGBA: SoH's clear black
    int iconSize = kComboItemTrackerIconSize;                       // IconSize, held to 25..128
    int iconSpacing = kComboItemTrackerIconSpacing;                 // IconSpacing: not honoured yet
    int textSize = kComboItemTrackerTextSize;                       // TextSize: not honoured yet
    int itemCountType = kComboItemTrackerCountCapacityOnly;         // ItemCountType: lane IT-4 (#1143)
    bool itemCountAlignLeft = false;                                // ItemCountAlignLeft: lane IT-4
    int keyCounts = kComboItemTrackerKeysCollectedMax;              // KeyCounts: lane IT-4
    int triforcePieceCounts = kComboItemTrackerTriforceRequiredMax; // TriforcePieceCounts: lane IT-4
    bool hookshotIdentifier = false;                                // HookshotIdentifier: lane IT-4
    bool dungeonItemsDisplayMaps = true;                            // DungeonItems.DisplayMaps: lane IT-2 (#1142)
    int sectionDisplay[kComboItemTrackerSohSectionCount] = { 1, 1, 1, 1, 1 }; // DisplayType.<Section>
    // The overlay's own (gCombo.Tracker.Items.*), which SoH's overlay has no row for.
    float opacity = 1.0f;                                               // Opacity, held to 0.1..1
    bool sectionShown[COMBO_ITEM_SECTION_COUNT] = { true, true, true }; // Section.OoT / .MM / .Shared
};

/** The keys' values now, SoH's default (or the overlay's) for any key not set. */
ComboItemTrackerKeys ComboReadItemTrackerKeys();

/**
 * Whether an OoT row of group `group` shows under SoH's DisplayType.<Section>
 * keys: hidden when its section is Hidden, shown on Main Window and Separate
 * (the overlay is one window). A group that is none of SoH's sections shows.
 */
bool ComboItemTrackerSohGroupShown(const ComboItemTrackerKeys& keys, const char* group);

/** What the overlay does this frame: draw or not, the flags it begins with, and whether it is pinned to the
 *  game window's viewport. */
struct ComboItemTrackerChrome {
    bool draws = true;
    int flags = 0;
    bool mainViewport = false;
};

/**
 * SoH's decision for its Item Tracker window (ItemTrackerWindow::DrawElement's
 * gate and BeginFloatingWindows' flags) over `keys`, the active game's pause
 * state `paused` and the first pad's held buttons `padButtons`: the Window
 * type always draws, as an auto-sized pane; the Floating type is an overlay on
 * the main viewport (ComboItemTrackerWindowFlags), drawn only while paused under
 * Only Enable While Paused, and only while both combo buttons are held under
 * Combo Button Hold.
 */
ComboItemTrackerChrome ComboItemTrackerChromeFor(const ComboItemTrackerKeys& keys, bool paused, uint32_t padButtons);

/**
 * The one-shot carry of the overlay's retired keys (RSBS::kComboRetiredKeys):
 * a value saved under a retired gCombo.Tracker.Items.* twin moves into SoH's
 * key, overwriting it (it is what the overlay drew), and the retired key is
 * cleared, so a second call carries nothing. Returns how many it carried.
 */
int ComboItemTrackerCarryRetiredKeys();

/** A rectangle in game-view pixels (the decisions below keep ImGui out of the lock). */
struct ComboItemTrackerRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

/**
 * The HUD's top-left band in a game view of `viewW` x `viewH` pixels at
 * (`viewX`, `viewY`): the heart rows and the magic bar under them, which both
 * games draw in the same place. The HUD is laid out on a 320x240 grid scaled to
 * the view's height (hearts: first centre at (30, 26), 10 apart, ten to a row,
 * two rows for 20 hearts, z_lifemeter.c; magic bar: top at y 42 at its lowest,
 * 16 tall, so its bottom is y 58, z_construct.c and z_parameter.c; the rupee
 * counter and key count sit at the bottom left, (26, 206)), so the band is 136
 * grid units wide and 58 tall, from the view's left edge. Wider than the hearts
 * themselves: it only needs to be a rect the overlay stays out of.
 *
 * Covered: a 4:3 or wider view, however far it is pillarboxed (the hearts shift
 * right, never down, so the vertical separation holds). NOT covered: a view
 * narrower than 4:3 that letterboxes the game top and bottom, where the grid's
 * unit is the view's width, not its height, and the hearts may sit lower than the
 * band; nor a HUD the player moved with the cosmetic HUD editor.
 */
ComboItemTrackerRect ComboItemTrackerHudBand(float viewX, float viewY, float viewW, float viewH);

/**
 * Where the floating overlay first appears (#962): at the left edge, just
 * below the HUD band (ComboItemTrackerHudBand), in the free stretch above the
 * rupee counter. The old default was ImGui's own (60, 60) for a new window,
 * which is on the hearts at every size. Applied only when the window has no
 * saved position (ImGuiCond_FirstUseEver) or the player presses Move to
 * Default Position; a position the player dragged to is kept, as SoH's trackers keep
 * theirs (ImGui's saved window settings).
 */
void ComboItemTrackerDefaultPos(float viewX, float viewY, float viewW, float viewH, float* outX, float* outY);

/**
 * Asks the floating overlay to go back to ComboItemTrackerDefaultPos on its
 * next draw (Randomizer > Item Tracker > Move to Default Position). Takes effect
 * once; draws nothing itself.
 */
void ComboItemTrackerResetPosition();

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
    std::string note; // the caption under the header: none since #927 (Combo_ItemFreshnessLabel)
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

/** Where a cell's icon is drawn, as an offset in the cell and a size. */
struct ComboItemIconRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

/**
 * An icon of aspect `aspect` (ComboItemRow.iconAspect) in a cell of edge
 * `cell`: a square for 0; a narrow one (a song's note) the cell's height and
 * centred across it; a wide one (MM's 46x24 owl face) held to the cell's width
 * and centred down it, so it never runs into the next cell (#1149).
 */
ComboItemIconRect ComboItemIconFit(float cell, float aspect);

/**
 * Where a row's label (ComboItemRow.label) starts: centred over the cell, its
 * top 13 px (at `scale`) above the icon's, as SoH's DrawItem draws its short
 * names (randomizer_item_tracker.cpp: p.y - (iconSize + 13) from the line
 * under the icon). `cellX`, `cellY` are the cell's top left, `textWidth` the
 * label's width.
 */
void ComboItemLabelPos(float cellX, float cellY, float cell, float textWidth, float scale, float* outX, float* outY);

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

/** A count as the icon shows it: the amount (with its slash), then the ceiling, each in its SoH palette tone. */
struct ComboItemCount {
    std::string amount;
    std::string ceiling;
    ComboUiTone amountTone = COMBO_UI_TONE_WHITE;
    ComboUiTone ceilingTone = COMBO_UI_TONE_GREEN;
    bool alignLeft = false; // SoH's Align count to left side, in a mode that offers it (#1143)
};

/**
 * The count drawn on a row's icon, or false for none. A row whose iconNumber
 * is below 0 has none (its icon shows the tier); one above 0 shows that number
 * alone, in white (a wallet's capacity). Otherwise a row neither held nor
 * counted has none (a faded icon carries no number). COMBO_ITEM_COUNT_SOH is
 * SoH's DrawItemCount with the ammo shown ("35/" then "40"): the amount and
 * its slash in the row's accent once the amount reaches the ceiling, gray at
 * 0, white otherwise, the ceiling in the accent (green, or red for the
 * skulltula tokens); a row with no ceiling shows its amount alone in white.
 * COMBO_ITEM_COUNT_MM is MM's DrawItemCounts: the amount alone, in white,
 * while it is above 0.
 */
bool ComboItemCountFor(const ComboItemRow& row, uint8_t countStyle, ComboItemCount& out);

/**
 * The count drawn on a row's icon under SoH's count settings in `keys` (#1143,
 * lane IT-4): SoH's DrawItemCount, for both games' rows (MM's section keeps
 * MM's place for the number, ComboItemCountPos).
 */
bool ComboItemCountFor(const ComboItemRow& row, const ComboItemTrackerKeys& keys, ComboItemCount& out);

/** The scale SoH's Text size gives a count (DrawItemCount: SetWindowFontScale(textSize / 13)). */
float ComboItemCountTextScale(int textSize);

/**
 * Where a count `width` x `height` wide starts in a cell at (`cellX`, `cellY`)
 * of edge `cell`, drawn at `scale` (#1143).
 */
void ComboItemCountPos(uint8_t countStyle, bool alignLeft, float cellX, float cellY, float cell, float width,
                       float height, float scale, float* outX, float* outY);

/** The letter SoH's Hookshot Identifier draws on a row's icon, or nullptr (#1143). */
const char* ComboItemIdentifierFor(const ComboItemRow& row, const ComboItemTrackerKeys& keys);

/**
 * Where the identifier letter, `textWidth` wide, starts in a cell at (`cellX`,
 * `cellY`) of edge `cell`; `unit` is the icon's size over 36 at the draw's
 * scale, the letter's own scale (#1143).
 */
void ComboItemIdentifierPos(float cellX, float cellY, float cell, float textWidth, float unit, float* outX,
                            float* outY);

/** A grid's cell edge, column pitch and line pitch at scale 1. */
struct ComboItemGridMetrics {
    float cell = 0.0f;
    float pitch = 0.0f;
    float linePitch = 0.0f;
};

/** Grid style `grid`'s metrics at SoH's Icon size `iconSize` and Icon margins `iconSpacing` (#1143). */
ComboItemGridMetrics ComboItemGridMetricsFor(const ComboItemGridStyle& grid, float iconSize, float iconSpacing);

/**
 * Whether a section draws icons: some row's picked texture is loaded
 * (`hasImage`, the seam's HasImage). Otherwise it is U2's text grid, the
 * fallback while a game's icons are not loaded and cannot be (ComboItemTrackerLoadIcons).
 */
bool ComboItemSectionDrawsIcons(const std::vector<ComboItemRow>& rows, bool (*hasImage)(const char*));

/**
 * Which keys a section's load already tried, for one stretch of the session. A
 * key that could not be loaded is not asked again until the epoch changes: the
 * archives a game can reach change only at a game switch, so the overlay passes
 * the active game as the epoch and a miss costs one attempt per switch, not one
 * per frame.
 */
struct ComboItemIconLoadMemo {
    int epoch = -1;
    std::unordered_set<std::string> missed;
};

/**
 * Load the textures a section's rows name that are not loaded yet (#963). A
 * game's icon textures are normally registered by that game's own init, and
 * Majora's Mask's runs at its first boot, so before it the overlay's MM column
 * had no icons to draw. A row key that is a resource path (ComboItemKeyIsPath)
 * is loaded by that path through `loadImage` (the shared resource manager's
 * archives), independent of any game's init; a texture name is a game's own
 * and is left alone. `hasImage` says what is loaded, `loadImage` loads one key
 * and says whether it is now loaded. Returns the number loaded by this call. A
 * key that fails stays a text cell, and is not retried within `epoch`.
 */
int ComboItemTrackerLoadIcons(const std::vector<ComboItemRow>& rows, bool (*hasImage)(const char*),
                              bool (*loadImage)(const char*), int epoch, ComboItemIconLoadMemo& memo);

/** Is `key` a resource path ("__OTR__..."), the kind MM's icons are keyed by, and OoT's are not? */
bool ComboItemKeyIsPath(const char* key);

/**
 * The production loader: load the Gui texture named `key` from the resource of
 * that path in the mounted archives, if one is there and parses as a texture.
 * Safe with no window, no Gui or no archive (false). True when the texture is
 * loaded under `key` afterwards, including when it already was.
 */
bool ComboItemLoadIconByPath(const char* key);

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
    float iconSize = (float)kComboItemTrackerIconSize;       // the IconSize key
    float iconSpacing = (float)kComboItemTrackerIconSpacing; // the IconSpacing key (#1143)
    float textHeight = 0.0f;                                 // a line of text (ImGui::GetFontSize at scale 1)
    float linePadding = 0.0f; // a text grid line's height beyond its text (ItemSpacing.y)
    float columnGap = 0.0f;   // between two text columns
    float cellPadding = 0.0f; // a table column's padding, both sides together
};

/** The gap between two lines of a grid style's cells: its lineGapPx, or its gapPx when it names none. */
float ComboItemGridLineGap(const ComboItemGridStyle& grid);

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
