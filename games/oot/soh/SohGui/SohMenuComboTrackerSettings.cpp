/**
 * @file SohMenuComboTrackerSettings.cpp
 * @brief Combo > Tracker Settings, the Item Tracker overlay's settings (#458 U7),
 *        a CONTRIBUTED page in the tier-4 Combo section; and the Check
 *        Tracker's rows on SoH's own Randomizer > Check Tracker page (#1132).
 *
 * THE ITEM TRACKER'S PAGE. The Item Tracker overlay (src/common/
 * ComboItemTrackerWindow.cpp) reads nine gCombo.Tracker.Items.* keys
 * (cvar_shared_keys.h), and this page is their writer. Its rows are SoH's own
 * Item Tracker Settings (randomizer_item_tracker.cpp,
 * ItemTrackerSettingsWindow::DrawElement and RegisterItemTrackerWidgets) for the
 * keys the overlay has: Background Color (SoH's clear-black alpha picker), Window
 * Type (Floating / Window), Enable Dragging and Only Enable While Paused
 * (floating only, hidden for a window as SoH hides them) and, after them, Move to
 * Default Position (floating only; #962: the overlay starts under the hearts, and
 * this returns a dragged overlay there; not named Reset, which the parity lint
 * reserves for confirmed destructive buttons), the Icon Size slider (SoH's
 * 25..128 px, default 36), then the overlay's two keys SoH's overlay has no row
 * for: its Opacity (the overlay's 0.1..1 clamp) and which of its three sections
 * it draws. Every default and range is the overlay's own
 * (ComboItemTrackerWindow.h), so the page never shows a value the overlay is not
 * drawing. TRANSITIONAL (#1132, operator ruling IT-D6): the unified Item Tracker
 * replaces SoH's and opens from SoH's Randomizer > Item Tracker page, and lane
 * IT-3 (#1141) retires the five rows that have SoH twins and appends the rest to
 * that page; until then they are here, and this is the Combo header's only
 * tracker page.
 *
 * THE CHECK TRACKER'S ROWS (#1132, operator ruling D1: "replace the soh
 * tracker"). The unified Check Tracker is SoH's Check Tracker in the single exe,
 * so its settings are SoH's page: SoH's own rows there (its toggle, which opens
 * the unified window by name, and its embedded Check Tracker Settings, whose keys
 * the window reads), then, through SohGui::RegisterSohPageRows, the window's two
 * keys of its own (#1131: Auto Scroll To Current Area and Only Show Current Area,
 * gCombo.Tracker.Checks.*, with MM's tracker's defaults) under SoH's Check Tracker
 * Settings separator, and the Cross-Game Spoiler's toggle under its own
 * separator (it was on Combo > Windows, which #1132 retired; it reads the
 * crossings ruling D10 cut from the tracker). SoH's window is not copied or
 * edited (rule 0).
 *
 * Locked by MenuComboSection leg 7 (every gCombo.Tracker.Items.* key written
 * exactly once with the overlay's widget, default and range, the floating-only
 * rows hidden for a window, the picker in the menu theme, and nothing of the
 * Check Tracker's) and leg 3 (SoH's Check Tracker page's rows and their order,
 * each gCombo.Tracker.Checks.* key once under SoH's separator, the Spoiler's
 * toggle), and drawn by UiSnapshot (Combo > Tracker Settings beside SoH's
 * Randomizer > Item Tracker, states "" and "window"; Randomizer > Check Tracker).
 */

#include "SohMenu.h"
#include "soh/OTRGlobals.h"
#include "soh/SohGui/SohGui.hpp"

// src/common only (ADR 0002): the overlay's keys, window types and ranges, and
// the Cross-Game Spoiler's registered name and visibility key.
#include "ComboItemTrackerWindow.h"
#include "ComboSpoilerWindow.h"
#include "cvar_shared_keys.h"

#include <map>

namespace SohGui {

using namespace UIWidgets;

namespace {

/** The sidebar label. Selection persists by display name, so this is the one
 *  spelling; UiSnapshot's reference table (soh_ui_snapshot.cpp, kCompare) and
 *  MenuComboSection leg 7 repeat it and fail when no registered Combo page
 *  carries it. */
constexpr const char* kTrackerSettingsPage = "Tracker Settings";

/** SoH's Window Type values (randomizer_entrance_tracker.h, windowType). */
const std::map<int32_t, const char*> kComboItemTrackerWindowTypes = {
    { ComboGui::COMBO_ITEM_TRACKER_FLOATING, "Floating" },
    { ComboGui::COMBO_ITEM_TRACKER_WINDOW, "Window" },
};

/** SoH's Item Tracker Settings draws Enable Dragging and Only Enable While
 *  Paused only while its overlay floats; these rows follow the same rule. */
void HideUnlessFloating(WidgetInfo& info) {
    info.isHidden = CVarGetInteger(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE, ComboGui::COMBO_ITEM_TRACKER_FLOATING) !=
                    ComboGui::COMBO_ITEM_TRACKER_FLOATING;
}

/** MenuDrawItem gives every other interactive row the menu's theme colour, but
 *  draws a colour picker in its options' own colour (no SoH page has one), so
 *  its Reset and Random buttons would be gray where SoH's are themed. SoH's
 *  Item Tracker Settings sets THEME_COLOR on its picker; this does the same,
 *  read every frame so a theme change follows at once. */
void ThemeColorPicker(WidgetInfo& info) {
    std::static_pointer_cast<ColorPickerOptions>(info.options)->color = THEME_COLOR;
}

} // namespace

/**
 * The contributed page's registrar. One column, like SoH's tracker pages: the
 * overlay's general settings, then its sections. Every row is cosmetic, so none
 * is race-disabled (SoH's tracker rows are not).
 */
void AddTrackerSettingsWidgets(SohMenu& menu, WidgetPath& path) {
    path.column = SECTION_COLUMN_1;

    menu.AddWidget(path, "Item Tracker Settings", WIDGET_SEPARATOR_TEXT);
    menu.AddWidget(path, "Background Color##ComboItemTracker", WIDGET_CVAR_COLOR_PICKER)
        .CVar(RSBS_CVAR_COMBO_ITEMS_BG_COLOR)
        .RaceDisable(false)
        .PreFunc(ThemeColorPicker)
        .Options(ColorPickerOptions()
                     .DefaultValue({ 0, 0, 0, 0 })
                     .UseAlpha()
                     .ShowReset()
                     .ShowRandom()
                     .Tooltip("Sets the background color of the Item Tracker."));
    menu.AddWidget(path, "Window Type##ComboItemTracker", WIDGET_CVAR_COMBOBOX)
        .CVar(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE)
        .RaceDisable(false)
        .Options(ComboboxOptions()
                     .DefaultIndex(ComboGui::COMBO_ITEM_TRACKER_FLOATING)
                     .ComponentAlignment(ComponentAlignments::Right)
                     .LabelPosition(LabelPositions::Far)
                     .ComboMap(kComboItemTrackerWindowTypes)
                     .Tooltip("Chooses how the Item Tracker is shown.\n\n"
                              "Floating: Over the game, with no title bar.\n"
                              "Window: In a window of its own, which you move by its title bar."));
    menu.AddWidget(path, "Enable Dragging##ComboItemTracker", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_COMBO_ITEMS_DRAGGABLE)
        .RaceDisable(false)
        .PreFunc(HideUnlessFloating)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Allows the floating Item Tracker to be dragged to a new place. It starts below the hearts at the "
            "left of the game: turn this on, then drag it where you want it."));
    menu.AddWidget(path, "Move to Default Position##ComboItemTracker", WIDGET_BUTTON)
        .RaceDisable(false)
        .PreFunc(HideUnlessFloating)
        .Callback([](WidgetInfo& info) { ComboGui::ComboItemTrackerResetPosition(); })
        .Options(ButtonOptions()
                     .Size(Sizes::Inline)
                     .Tooltip("Puts the floating Item Tracker back where it starts, below the hearts at the left "
                              "of the game."));
    menu.AddWidget(path, "Only Enable While Paused##ComboItemTracker", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_COMBO_ITEMS_SHOW_ONLY_PAUSED)
        .RaceDisable(false)
        .PreFunc(HideUnlessFloating)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Shows the floating Item Tracker only while the game is paused."));
    menu.AddWidget(path, "Icon Size: %dpx", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_COMBO_ITEMS_ICON_SIZE)
        .RaceDisable(false)
        .Options(IntSliderOptions()
                     .Min(ComboGui::kComboItemTrackerIconSizeMin)
                     .Max(ComboGui::kComboItemTrackerIconSizeMax)
                     .DefaultValue(ComboGui::kComboItemTrackerIconSize)
                     .Format("%dpx")
                     .Tooltip("Sets the size of the icons in the Item Tracker."));
    menu.AddWidget(path, "Tracker Opacity", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(RSBS_CVAR_COMBO_ITEMS_OPACITY)
        .RaceDisable(false)
        // IsPercentage() first: it resets the range to 0..1, and the overlay
        // clamps at 0.1 (ComboItemTrackerWindow.cpp, OverlayOpacity).
        .Options(FloatSliderOptions().IsPercentage().Min(0.1f).Max(1.0f).DefaultValue(1.0f).Tooltip(
            "Sets the opacity of the whole Item Tracker, its icons included."));

    menu.AddWidget(path, "Item Tracker Sections", WIDGET_SEPARATOR_TEXT);
    menu.AddWidget(path, "Ocarina of Time##ComboItemTracker", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_COMBO_ITEMS_SECTION_OOT)
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Shows the items you hold in Ocarina of Time in the Item Tracker, as of your last game switch or "
            "save while you play Majora's Mask."));
    menu.AddWidget(path, "Majora's Mask##ComboItemTracker", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_COMBO_ITEMS_SECTION_MM)
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Shows the items you hold in Majora's Mask in the Item Tracker, as of your last game switch or save "
            "while you play Ocarina of Time."));
    menu.AddWidget(path, "Shared##ComboItemTracker", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_COMBO_ITEMS_SECTION_SHARED)
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Shows the items and resources the two games share in the Item Tracker, as of your last game "
            "switch or save."));
}

/**
 * The Check Tracker's rows on SoH's Randomizer > Check Tracker page (#1132),
 * after SoH's own (the page's path, first column pinned, handed over by
 * AddContributedSohPageRows): they continue SoH's Check Tracker Settings group,
 * then the Cross-Game Spoiler gets a separator of its own, as SoH's tracker
 * pages give every window one.
 */
void AddCheckTrackerPageRows(SohMenu& menu, WidgetPath& path) {
    // #1131: the window's two keys of its own, in SoH's checkbox shape with the
    // window's defaults (ComboTrackerWindow.h, ComboTrackerNavKeys).
    menu.AddWidget(path, "Auto Scroll To Current Area##CheckTracker", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_COMBO_CHECKS_AUTO_SCROLL)
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Scrolls the Check Tracker to the area you are in each time you enter a new one, in the game you are "
            "playing."));
    menu.AddWidget(path, "Only Show Current Area##CheckTracker", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_COMBO_CHECKS_ONLY_CURRENT_AREA)
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Shows only the checks in the area you are in, in both games' lists of the Check Tracker. Each list also "
            "has its own Only Show Current Area toggle."));
    // The Cross-Game Spoiler, from Combo > Windows: it reveals the paired seed's
    // crossings, so it is race-disabled like a spoiler tool. WINDOW_BUTTON reads
    // .CVar only for its label and opens the window it finds by .WindowName, so
    // both come from the constants the window is built with.
    menu.AddWidget(path, "Cross-Game Spoiler", WIDGET_SEPARATOR_TEXT);
    menu.AddWidget(path, "Toggle Cross-Game Spoiler", WIDGET_WINDOW_BUTTON)
        .CVar(ComboGui::kComboSpoilerVisibilityCVar)
        .RaceDisable(true)
        .WindowName(ComboGui::kComboSpoilerWindowName)
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Toggles the Cross-Game Spoiler.").EmbedWindow(false));
}

/** File-scope, so it runs before AddMenuElements() (SohMenu.h's contract); this
 *  TU is under games/oot/soh, whose archives are WHOLE_ARCHIVE'd, so the
 *  registrar is not elided. The page always holds its rows, so it is never
 *  #640's empty page. */
static RegisterComboSectionPage_t sTrackerSettingsPage(kTrackerSettingsPage, 1, AddTrackerSettingsWidgets);
/** The same for the Check Tracker's rows (#1132): SoH's page always holds its own
 *  rows, so a contribution can never make it #640's empty page. */
static RegisterSohPageRows_t sCheckTrackerRows("Randomizer", "Check Tracker", AddCheckTrackerPageRows);

} // namespace SohGui
