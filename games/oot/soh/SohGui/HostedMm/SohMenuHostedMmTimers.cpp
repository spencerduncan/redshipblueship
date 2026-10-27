/**
 * @file SohMenuHostedMmTimers.cpp
 * @brief Majora's Mask's In-Game Timer and Time Splits rows hosted on SoH's Enhancements pages (#1235, #1237, #1239),
 *        through the hosted-row seam (SohMenuHostedMm.h).
 *
 * MM read every key below, but its only widgets sat in the excluded BenMenu.cpp and TimesplitsSettings.cpp's popout,
 * and the windows they drive were never created (games/mm/2s2h/TimersGuiSingleExe.cpp now registers them). SoH's
 * pages call these functions in one line each (SohMenuEnhancements.cpp):
 *  - Timers: a "Majora's Mask" block after SoH's own timer rows, holding the In-Game Timer's mode, background and
 *    scale. SoH's Additional Timers read OoT's save only, so none is a twin.
 *  - Time Splits: a "Majora's Mask" block holding 2Ship's six List Options switches (settings, so page rows), ABOVE
 *    SoH's Popout Time Splits Window: while that window is not popped out the page embeds it, and its splits list
 *    fills the rest of the column, so a block below it would sit out of sight. The rest of 2Ship's popout, the list
 *    management and Split Entries tabs, is drawn inside SoH's own Time Splits window while MM runs
 *    (src/common/mm_time_splits_bridge.h), so that popout resolves to the one window and gets no row of its own.
 * Every row binds its RSBS_CVAR_MM_* define (cvar_shared_keys.h, W6-TIMERS slot) and says "Majora's Mask only."
 */

#include "SohMenuHostedMm.h"

#include "soh/SohGui/UIWidgets.hpp"

#include "cvar_shared_keys.h" // src/common: the RSBS_CVAR_MM_* keys
#ifdef RSBS_SINGLE_EXECUTABLE
#include "mm_time_splits_bridge.h" // src/common: MM_TimeSplits_RefreshSettings
#endif

namespace SohGui {

using namespace UIWidgets;

namespace {

// 2Ship's TimerDisplayOptions (Enhancements.h): TIMER_DISPLAY_NONE, _RTA, _IGT.
const std::map<int32_t, const char*> kMmInGameTimerModes = {
    { 0, "Off" },
    { 1, "Real-Time" },
    { 2, "In-Game Time" },
};

// 2Ship applies three of its switches to the splits list when they change (Timesplits.cpp UpdateSplitSettings).
void RefreshMmTimeSplits(WidgetInfo& info) {
#ifdef RSBS_SINGLE_EXECUTABLE
    MM_TimeSplits_RefreshSettings();
#endif
}

} // namespace

void AddMmInGameTimerRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmBlock(menu, path);
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "In-Game Timer", WIDGET_CVAR_COMBOBOX)
        .CVar(RSBS_CVAR_MM_DISPLAY_OVERLAY_MODE)
        .RaceDisable(false)
        .Options(ComboboxOptions()
                     .ComboMap(kMmInGameTimerModes)
                     .DefaultIndex(0)
                     .Tooltip("Shows a timer over the game: Real-Time counts from the moment you first entered "
                              "Majora's Mask on this file whether or not you play, In-Game Time counts only time spent "
                              "playing Majora's Mask. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Hide In-Game Timer Background", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_DISPLAY_OVERLAY_BACKGROUND)
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip("Hides the background of the In-Game Timer. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "In-Game Timer Scale: %.1fx", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(RSBS_CVAR_MM_DISPLAY_OVERLAY_SCALE)
        .RaceDisable(false)
        .Options(FloatSliderOptions().Min(1.0f).Max(5.0f).DefaultValue(1.0f).Format("%.1fx").Step(0.1f).Tooltip(
            "Sets the size of the In-Game Timer. Majora's Mask only."));
}

void AddMmTimeSplitsRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmBlock(menu, path);
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Enable Time Splits", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_TIME_SPLITS_ENABLE)
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "Turns on the Time Splits list and its splits; nothing splits while this is off. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Show Split Headers", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_TIME_SPLITS_SHOW_HEADERS)
        .RaceDisable(false)
        .Callback(RefreshMmTimeSplits)
        .Options(CheckboxOptions().Tooltip("Shows the column names in the Time Splits list. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Hide Split List Background", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_TIME_SPLITS_HIDE_BACKGROUND)
        .RaceDisable(false)
        .Callback(RefreshMmTimeSplits)
        .Options(CheckboxOptions().Tooltip("Hides the background of the Time Splits list. The background still shows "
                                           "where the list reaches past the game window. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Highlight Active Split", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_TIME_SPLITS_HIGHLIGHT)
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip("Highlights the split you are on in the Time Splits list. Majora's Mask "
                                           "only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Follow Active Split", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_TIME_SPLITS_FOLLOW)
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip("Keeps the split you are on in view in the Time Splits list. The list "
                                           "cannot be scrolled by hand while this is on. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Compare Splits", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_TIME_SPLITS_COMPARE)
        .RaceDisable(false)
        .Callback(RefreshMmTimeSplits)
        .Options(CheckboxOptions().Tooltip(
            "Compares your splits against a saved list, chosen in the Time Splits window, inside the Time Splits "
            "list. Majora's Mask only."));
}

} // namespace SohGui
