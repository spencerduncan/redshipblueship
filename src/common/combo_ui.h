/**
 * @file combo_ui.h
 * @brief The `combo_ui` seam: SoH's widget look for src/common panes, without
 *        an SoH header (UI parity M6; docs/ui-style-guide.md section 10).
 *
 * THE PROBLEM. Our common-owned panes (the MM Randomizer Options pane first,
 * a menu page since 2026-09-27 whose trick rows still draw their chips and
 * names through this seam; the Combo Tracker and the Cross-Game Spoiler next)
 * live in src/common, which
 * ADR 0002 keeps free of every game header, and which cannot include
 * `UIWidgets.hpp` anyway: that header includes "soh/ShipUtils.h", and
 * redship_common has games/oot/soh on its include path but not games/oot. So
 * they drew raw ImGui, which is what made them look unlike every SoH page beside
 * them (raw checkboxes, unthemed frames, hand-picked colours, collapsing
 * headers, inline disabled reasons).
 *
 * THE SEAM. A C function table, the shape src/common already uses for every
 * game-to-common hand-off (the MM option and trick descriptor tables, the
 * foreign-item pools): src/common says WHAT it draws, one game's TU says HOW.
 * `games/oot/soh/SohGui/ComboUiSoh.cpp` implements each entry with SoH's own
 * helpers (UIWidgets + THEME_COLOR + SohGui::RegisterPopup) and installs the
 * table from a file-scope initializer, the RegisterComboSectionPage_t precedent
 * (a TU under games/oot/soh, which is WHOLE_ARCHIVE'd, so the initializer cannot
 * be elided). The shipped binary therefore always draws through SoH's helpers;
 * the ComboMMOptionsPage lock asserts the table is installed.
 *
 * FALLBACK. ComboUi_Get() never returns NULL: with nothing installed it returns
 * a raw-ImGui table (combo_ui.cpp), so a link without the OoT TU still draws a
 * usable, if unthemed, pane. Nothing here touches ImGui until an entry is
 * CALLED, and the display-free locks never call one (their panes stay shut), so
 * whether a table is installed is invisible to them.
 *
 * THE RECT RECORDER. Every widget entry reports the rectangle it drew and the
 * tooltip a hover would show, to one optional recorder. Production never sets
 * one. The UI snapshot harness does, to find a pane row by its label and hover
 * it (a menu page gets the same from a row's postFunc; a pane has no rows to wrap).
 *
 * Every entry takes its strings as borrowed pointers, valid for the call only.
 */

#ifndef RSBS_COMMON_COMBO_UI_H
#define RSBS_COMMON_COMBO_UI_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The SoH palette entries a surface may name (UIWidgets::Colors, in words).
 * THEME is the player's menu theme colour; the rest are fixed palette entries,
 * for the semantic uses the style guide allows (gray notes, orange warnings) and
 * for trick-difficulty chips (SoH's tricks.cpp: Novice green, Intermediate
 * orange, Advanced blue, Expert red, Extreme purple, Experimental light blue,
 * Glitch white).
 */
typedef enum {
    COMBO_UI_TONE_THEME,
    COMBO_UI_TONE_GRAY,
    COMBO_UI_TONE_ORANGE,
    COMBO_UI_TONE_GREEN,
    COMBO_UI_TONE_BLUE,
    COMBO_UI_TONE_LIGHT_BLUE,
    COMBO_UI_TONE_RED,
    COMBO_UI_TONE_PURPLE,
    COMBO_UI_TONE_WHITE,
    COMBO_UI_TONE_COUNT
} ComboUiTone;

/**
 * Per-widget options: SoH's UIWidgets::WidgetOptions in C. A NULL options
 * pointer means all defaults (enabled, no tooltip).
 *
 * A disabled widget shows `disabledTooltip` on hover (SoH's own rule: the reason
 * a row is disabled replaces its description); an enabled one shows `tooltip`.
 * Write `disabledTooltip` in SoH's disabled shape, which ComboUi_DisabledTooltip
 * composes.
 */
typedef struct {
    const char* tooltip;
    bool disabled;
    const char* disabledTooltip;
} ComboUiWidgetOpts;

/** Button widths: SoH's `Sizes::Fill`, and the 250 px primary-action width
 *  (SohMenuRandomizer.cpp's preset and generate buttons). */
#define COMBO_UI_WIDTH_FILL (-1.0f)
#define COMBO_UI_WIDTH_PRIMARY 250.0f

typedef struct {
    /** Checkbox, label right of the box. True when toggled this frame. */
    bool (*Checkbox)(const char* label, bool* value, const ComboUiWidgetOpts* opts);
    /**
     * Combobox, label above, box as wide as the longest value. An `*index`
     * outside [0, count) is shown as "Unknown (N)" rather than clamped, so a
     * value a later build wrote is never displayed as one it is not. True when
     * the player picked a value this frame.
     */
    bool (*Combobox)(const char* label, int32_t* index, const char* const* values, int count,
                     const ComboUiWidgetOpts* opts);
    /**
     * Integer slider with SoH's -/+ buttons, label above, full width. `format`
     * is the in-slider value text (printf'd with the value; NULL means "%d").
     * True when the value changed this frame.
     */
    bool (*SliderInt)(const char* label, int32_t* value, int32_t min, int32_t max, const char* format,
                      const ComboUiWidgetOpts* opts);
    /** Themed button; `width` is COMBO_UI_WIDTH_FILL or a pixel width. True when clicked. */
    bool (*Button)(const char* label, float width, const ComboUiWidgetOpts* opts);
    /**
     * SoH's check-tracker search box (randomizer_check_tracker.cpp): a themed
     * text field across the line, editing the NUL-terminated `buf` (`bufSize`
     * bytes), a "Search..." placeholder while it is empty, and an eraser button
     * beside it that clears it. `id` scopes its ImGui ids. True when the text
     * changed this frame (typed or erased). It takes no options (SoH's box has
     * no tooltip and is never disabled); it reports the field's rect under `id`
     * and the eraser's under "<id>##eraser".
     */
    bool (*SearchInput)(const char* id, char* buf, int bufSize);
    /** A section header (ImGui::SeparatorText), SoH's WIDGET_SEPARATOR_TEXT. */
    void (*SeparatorText)(const char* text);
    /** A gray note, wrapped: SoH's `TextOptions().Color(Colors::Gray)` TEXT row. */
    void (*NoteText)(const char* text);
    /** An orange warning line, wrapped: SoH's warning colour (Colors::Orange). */
    void (*WarningText)(const char* text);
    /** Tooltip for the item drawn last, when hovered (UIWidgets::Tooltip: wrapped at 80 characters). */
    void (*Tooltip)(const char* text);
    /**
     * A small inert chip in `tone`, on the current line (SoH's trick tag chips,
     * which SoH draws disabled). `opts` is the row's options: when
     * `opts->disabled` the chip dims once more, as the row's control and name
     * do, so a disabled row reads disabled end to end (ImGui does not dim a
     * nested BeginDisabled again). NULL is a live row.
     */
    void (*TagChip)(const char* label, ComboUiTone tone, const ComboUiWidgetOpts* opts);
    /**
     * Queue a confirm modal (SohGui::RegisterPopup): `confirmLabel` runs
     * `onConfirm(user)`, `cancelLabel` closes it. The call only queues; the modal
     * draws on a later frame and `onConfirm` runs from that frame.
     */
    void (*Confirm)(const char* title, const char* message, const char* confirmLabel, const char* cancelLabel,
                    void (*onConfirm)(void* user), void* user);
    /**
     * Theme collapsing headers (and the header, tree-node and selectable
     * highlight) until PopTheme, as SoH's tracker panes do
     * (UIWidgets::PushStyleCombobox: rounded, 10x6 padding, theme colour at half alpha).
     */
    void (*PushTheme)(void);
    void (*PopTheme)(void);
    /** Vertical space, SoH's UIWidgets::Spacer (0 is one item spacing). */
    void (*Spacer)(float height);
    /**
     * The name cell of a composite row, on the current line after the row's other
     * items: SoH's trick-list row is a control, the tag chips, then the name
     * (DrawTricksMenu's ImGui::Text plus UIWidgets::Tooltip). Wrapped at the
     * pane's edge with a hanging indent under the name, dimmed when
     * `opts->disabled`, and showing the tooltip a widget with the same options
     * would (ComboUi_ShownTooltip), so the whole row explains itself on hover.
     * Reports its rectangle under `text` to the rect recorder.
     */
    void (*RowText)(const char* text, const ComboUiWidgetOpts* opts);
    /**
     * A square icon button one frame high, in the theme colour, on the current
     * line: SoH's check-tracker skip button (randomizer_check_tracker.cpp,
     * DrawLocation: UIWidgets::StateButton with ICON_FA_TIMES / ICON_FA_PLUS at
     * GetFrameHeight()). `id` is its ImGui id and the label it reports its rect
     * under; `icon` is the glyph drawn. True when clicked. A NULL `icon` draws
     * DrawLocation's placeholder instead, an empty square of the same size (its
     * ImGui::Dummy on a row with no button), so the rows' names stay in one
     * column; it reports nothing and returns false.
     */
    bool (*IconButton)(const char* id, const char* icon, const ComboUiWidgetOpts* opts);
    /**
     * An icon from the Gui's texture map, `width` x `height` pixels, on the
     * current line: SoH's item-tracker icon (randomizer_item_tracker.cpp, an
     * ImGui::Image of Gui::GetTextureByName). `textureKey` is the opaque key an
     * item adapter produced (ComboItemRow.iconKey). False, drawing nothing, when
     * the key is NULL or no texture is loaded under it (MM's icons before MM's
     * first boot): the caller then draws the item's name as text (#458 U2; the
     * unified overlay draws text in U2 and icons from U3).
     */
    bool (*Image)(const char* textureKey, float width, float height);
    /**
     * Whether Image would draw `textureKey` (a texture is loaded under it),
     * drawing nothing: the unified overlay decides per section whether its
     * cells are icons or text before it lays them out (#458 U3).
     */
    bool (*HasImage)(const char* textureKey);
    /**
     * One unwrapped line of `text` in the SoH palette colour `tone`, at the
     * cursor: SoH's item-tracker counts (randomizer_item_tracker.cpp,
     * DrawItemCount: the amount in white, green or gray, the ceiling in green),
     * #458 U3.
     */
    void (*ToneText)(const char* text, ComboUiTone tone);
} ComboUiTable;

/** Install the table the panes draw through. NULL uninstalls (back to the fallback). */
void ComboUi_Install(const ComboUiTable* table);

/** True when a table was installed (in the shipped binary: SoH's). */
bool ComboUi_IsInstalled(void);

/** The installed table, or the raw-ImGui fallback. Never NULL. */
const ComboUiTable* ComboUi_Get(void);

/**
 * SoH's disabled tooltip, shape (a) of docs/ui-style-guide.md R-S2:
 * "This setting is disabled because: \n" then "\n- <reason>" per reason (the
 * text Menu::MenuDrawItem builds from its disabledMap). NULL and empty reasons
 * are skipped; with none left the result is "". The returned pointer is owned
 * here and stays valid for the process's life (one stored copy per distinct
 * composition, so the set is bounded by the reasons the panes can show).
 */
const char* ComboUi_DisabledTooltip(const char* reason1, const char* reason2);

/** The text a hover on a widget with these options shows: its disabled tooltip or its tooltip. */
const char* ComboUi_ShownTooltip(const ComboUiWidgetOpts* opts);

/**
 * The rect recorder. `label` is the widget's label as passed to the entry,
 * `tooltip` the text a hover would show now (NULL when none), and the rectangle
 * is in ImGui screen coordinates. At most one recorder; NULL removes it.
 */
typedef void (*ComboUiRectRecorder)(void* user, const char* label, const char* tooltip, float minX, float minY,
                                    float maxX, float maxY);
void ComboUi_SetRectRecorder(ComboUiRectRecorder recorder, void* user);

/** Called by a table implementation after each widget; forwards to the recorder, if any. */
void ComboUi_NotifyRect(const char* label, const char* tooltip, float minX, float minY, float maxX, float maxY);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_UI_H
