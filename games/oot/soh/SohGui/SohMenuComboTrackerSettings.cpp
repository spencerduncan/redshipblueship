/**
 * @file SohMenuComboTrackerSettings.cpp
 * @brief Combo > Tracker Settings: the unified trackers' settings (#458 U7), a
 *        CONTRIBUTED page in the tier-4 Combo section.
 *
 * WHAT IT IS FOR. The Combo Item Tracker overlay (src/common/
 * ComboItemTrackerWindow.cpp) reads nine gCombo.Tracker.Items.* keys
 * (cvar_shared_keys.h), and until this page nothing but the console wrote them.
 * ADR 0004's 2026-09-30 amendment puts the unified trackers in Combo > Windows
 * (live-play tools) and their settings on a Combo page, SoH's split (settings
 * are pages, tools are windows: docs/ui-style-guide.md section 10).
 *
 * THE ROWS are SoH's own Item Tracker Settings (randomizer_item_tracker.cpp,
 * ItemTrackerSettingsWindow::DrawElement and RegisterItemTrackerWidgets) for
 * the keys the overlay has: Background Color (SoH's clear-black alpha picker),
 * Window Type (Floating / Window), Enable Dragging and Only Enable While Paused
 * (floating only, hidden for a window as SoH hides them), the Icon Size slider
 * (SoH's 25..128 px, default 36), then the overlay's two keys SoH's overlay has
 * no row for: its Opacity (the overlay's 0.1..1 clamp) and which of its three
 * sections it draws. Every default and range is the overlay's own
 * (ComboItemTrackerWindow.h), so the page never shows a value the overlay is not
 * drawing.
 *
 * THE CHECK TRACKER. The Combo Tracker window has no keys of its own: it draws
 * its checks in the colours of SoH's Check Tracker Settings
 * (gTrackers.CheckTracker.*, ComboTrackerWindow.cpp). So the page hosts SoH's
 * own Check Tracker Settings exactly as SoH's Randomizer > Check Tracker page
 * does: embedded, under its "Popout Check Tracker Settings" row, with one gray
 * note saying which of its settings the Combo Tracker follows. SoH's window is
 * not copied or edited (rule 0).
 *
 * Locked by MenuComboSection leg 7 (every gCombo.Tracker.Items.* key written
 * exactly once with the overlay's widget, default and range, the floating-only
 * rows hidden for a window, the embedded Check Tracker Settings row, and no
 * other key) and drawn by UiSnapshot (Combo > Tracker Settings beside SoH's
 * Randomizer > Item Tracker, states "" and "window").
 */

#include "SohMenu.h"
#include "soh/OTRGlobals.h"
#include "soh/SohGui/SohGui.hpp"

// src/common only (ADR 0002): the overlay's keys, window types and ranges.
#include "ComboItemTrackerWindow.h"
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
 * overlay's general settings, its sections, then SoH's Check Tracker Settings.
 * Every row is cosmetic, so none is race-disabled (SoH's tracker rows are not).
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
                     .Tooltip("Sets the background color of the Combo Item Tracker."));
    menu.AddWidget(path, "Window Type##ComboItemTracker", WIDGET_CVAR_COMBOBOX)
        .CVar(RSBS_CVAR_COMBO_ITEMS_WINDOW_TYPE)
        .RaceDisable(false)
        .Options(ComboboxOptions()
                     .DefaultIndex(ComboGui::COMBO_ITEM_TRACKER_FLOATING)
                     .ComponentAlignment(ComponentAlignments::Right)
                     .LabelPosition(LabelPositions::Far)
                     .ComboMap(kComboItemTrackerWindowTypes)
                     .Tooltip("Chooses how the Combo Item Tracker is shown.\n\n"
                              "Floating: Over the game, with no title bar.\n"
                              "Window: In a window of its own, with a title bar."));
    menu.AddWidget(path, "Enable Dragging##ComboItemTracker", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_COMBO_ITEMS_DRAGGABLE)
        .RaceDisable(false)
        .PreFunc(HideUnlessFloating)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Allows the floating Combo Item Tracker to be dragged to a new place."));
    menu.AddWidget(path, "Only Enable While Paused##ComboItemTracker", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_COMBO_ITEMS_SHOW_ONLY_PAUSED)
        .RaceDisable(false)
        .PreFunc(HideUnlessFloating)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Shows the floating Combo Item Tracker only while the game is paused."));
    menu.AddWidget(path, "Icon Size: %dpx", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_COMBO_ITEMS_ICON_SIZE)
        .RaceDisable(false)
        .Options(IntSliderOptions()
                     .Min(ComboGui::kComboItemTrackerIconSizeMin)
                     .Max(ComboGui::kComboItemTrackerIconSizeMax)
                     .DefaultValue(ComboGui::kComboItemTrackerIconSize)
                     .Format("%dpx")
                     .Tooltip("Sets the size of the icons in the Combo Item Tracker."));
    menu.AddWidget(path, "Tracker Opacity", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(RSBS_CVAR_COMBO_ITEMS_OPACITY)
        .RaceDisable(false)
        // IsPercentage() first: it resets the range to 0..1, and the overlay
        // clamps at 0.1 (ComboItemTrackerWindow.cpp, OverlayOpacity).
        .Options(FloatSliderOptions().IsPercentage().Min(0.1f).Max(1.0f).DefaultValue(1.0f).Tooltip(
            "Sets the opacity of the whole Combo Item Tracker, its icons included."));

    menu.AddWidget(path, "Item Tracker Sections", WIDGET_SEPARATOR_TEXT);
    menu.AddWidget(path, "Ocarina of Time##ComboItemTracker", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_COMBO_ITEMS_SECTION_OOT)
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Shows the items you hold in Ocarina of Time in the Combo Item Tracker."));
    menu.AddWidget(path, "Majora's Mask##ComboItemTracker", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_COMBO_ITEMS_SECTION_MM)
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Shows the items you hold in Majora's Mask in the Combo Item Tracker."));
    menu.AddWidget(path, "Shared##ComboItemTracker", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_COMBO_ITEMS_SECTION_SHARED)
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Shows the rupees, hearts and ammo the two games share in the Combo Item Tracker."));

    menu.AddWidget(path, "Check Tracker Settings", WIDGET_SEPARATOR_TEXT);
    // A TEXT row draws its name: the one sentence SoH's window cannot say.
    menu.AddWidget(path, "The Combo Tracker draws its checks in the Check Tracker's colors.", WIDGET_TEXT)
        .RaceDisable(false)
        .HideInSearch(true)
        .Options(TextOptions().Color(Colors::Gray));
    menu.AddWidget(path, "Popout Check Tracker Settings", WIDGET_WINDOW_BUTTON)
        .CVar(CVAR_WINDOW("CheckTrackerSettings"))
        .RaceDisable(false)
        .WindowName("Check Tracker Settings")
        .HideInSearch(true)
        .Options(WindowButtonOptions().Tooltip("Enables the separate Check Tracker Settings Window."));
}

/** File-scope, so it runs before AddMenuElements() (SohMenu.h's contract); this
 *  TU is under games/oot/soh, whose archives are WHOLE_ARCHIVE'd, so the
 *  registrar is not elided. The page always holds its rows, so it is never
 *  #640's empty page. */
static RegisterComboSectionPage_t sTrackerSettingsPage(kTrackerSettingsPage, 1, AddTrackerSettingsWidgets);

} // namespace SohGui
