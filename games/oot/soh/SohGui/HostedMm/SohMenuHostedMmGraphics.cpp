/**
 * @file SohMenuHostedMmGraphics.cpp
 * @brief Majora's Mask's clock, motion blur and scene-geometry rows hosted on SoH's Enhancements > Graphics page
 *        through the hosted-row seam (#1237, #1240, lane W6-GFX).
 *
 * MM read every key below, but its only widgets sat in the excluded BenMenu.cpp, so nothing in the single exe wrote
 * them, and the two clock-style TUs (Graphics/3DSClock.cpp, Graphics/TextBasedClock.cpp) were not in the link
 * (games/mm/CMakeLists.txt, W6-GFX). None has an SoH twin, so each sits in a "Majora's Mask" block. SoH's Graphics page
 * calls each function below in one line (SohMenuEnhancements.cpp):
 *  - the UI column: Clock Type and 24 Hours Clock, after SoH's own UI rows;
 *  - the Misc. column: the motion blur and scene-geometry rows, after SoH's Draw Distance rows.
 * Motion blur. Motion Blur Mode picks Dynamic (MM's own scenes turn the blur on and off), Always Off or Always On.
 * Dynamic Motion Blur and its strength are not keys: they are MM's R_MOTION_BLUR_ENABLED and R_MOTION_BLUR_ALPHA
 * registers, reached through src/common/mm_graphics_bridge.h. They mean something only while Majora's Mask runs a play
 * state, so they draw disabled with a reason, and a gray note says why, at any other time (ADR 0004's CAPABILITY
 * state, #497; MenuCapabilityGating's leg 8). Rows that act only in one mode are disabled, with SoH's "Requires ..."
 * tooltip, in the others, rather than hidden as in 2Ship, so every row stays on the page to be found and hovered.
 * Scene geometry. MM applies these patches once, in its asset phase; a change reapplies them through the bridge (at
 * once while MM runs, otherwise at MM's next resume), as 2Ship's menu callback did.
 * Authentic Logo (gEnhancements.Graphics.AuthenticLogo) has no row on purpose: the console logo it changes draws only
 * on a cold `--game mm` boot (z_title.c skips it on every crossing into MM), and unpaired play is not a supported mode.
 */

#include "SohMenuHostedMm.h"

#include "soh/SohGui/UIWidgets.hpp"

#include <libultraship/bridge/consolevariablebridge.h>

#include "cvar_shared_keys.h"   // src/common: the RSBS_CVAR_MM_GRAPHICS_* keys
#include "mm_graphics_bridge.h" // src/common: MM's motion blur registers and the patch reapply

namespace SohGui {

using namespace UIWidgets;

namespace {

// Static storage: the combobox keeps a pointer to its map. 2Ship's ClockTypeOptions order (Enhancements.h).
const std::map<int32_t, const char*> kClockTypes = {
    { 0, "Original" },
    { 1, "MM3D Style" },
    { 2, "Text Only" },
};

// MotionBlur.cpp's order: MotionBlur_Override reads 1 as always off and 2 as always on; 0 leaves MM's registers be.
constexpr int32_t kMotionBlurDynamic = 0;
constexpr int32_t kMotionBlurAlwaysOff = 1;
constexpr int32_t kMotionBlurAlwaysOn = 2;
const std::map<int32_t, const char*> kMotionBlurModes = {
    { kMotionBlurDynamic, "Dynamic" },
    { kMotionBlurAlwaysOff, "Always Off" },
    { kMotionBlurAlwaysOn, "Always On" },
};

constexpr const char* kMmNotRunning = "Majora's Mask Not Running";
constexpr const char* kDynamicNote = "Dynamic motion blur can be changed only while Majora's Mask is running.";

// The two register rows' values, refreshed from MM's registers by their PreFuncs while MM runs.
bool sDynamicEnabled = false;
int32_t sDynamicAlpha = 180;

int32_t MotionBlurMode() {
    return CVarGetInteger(RSBS_CVAR_MM_GRAPHICS_MOTION_BLUR_MODE, kMotionBlurDynamic);
}

/** SoH's in-page dependency idiom ("Disable Fixed Camera"): disabled, with a "Requires ..." tooltip, unless @p met. */
void RequireOrEnable(WidgetInfo& info, bool met, const char* requirement) {
    info.options->disabled = !met;
    info.options->disabledTooltip = met ? "" : requirement;
}

/** A register row: Dynamic mode first, then MM running (the CAPABILITY state), and only then is it live. Returns
 *  whether the row is live, so the caller can refresh its value from MM's register. */
bool GateRegisterRow(WidgetInfo& info) {
    if (MotionBlurMode() != kMotionBlurDynamic) {
        RequireOrEnable(info, false, "Requires Motion Blur Mode to be Dynamic.");
        return false;
    }
    if (!MM_MotionBlur_RegistersLive()) {
        SohMenu::ApplyPresentation(info, info.name, SOH_MENU_PRESENT_CAPABILITY, kMmNotRunning);
        return false;
    }
    SohMenu::ApplyPresentation(info, info.name, SOH_MENU_PRESENT_LIVE, nullptr);
    return true;
}

} // namespace

/** SoH's Fix Out of Bounds Textures row (Enhancements > Fixes): the key both games share also chooses MM's patches. */
void ReapplyMmAuthenticGfxPatches() {
    (void)MM_GfxPatches_Reapply();
}

/** Enhancements > Graphics, the UI column, after SoH's own UI rows. */
void AddMmClockRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmBlock(menu, path);
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Clock Type", WIDGET_CVAR_COMBOBOX)
        .CVar(RSBS_CVAR_MM_GRAPHICS_CLOCK_TYPE)
        .RaceDisable(false)
        .Options(ComboboxOptions()
                     .ComboMap(kClockTypes)
                     .DefaultIndex(0)
                     .Tooltip("Swaps the three-day clock for the Majora's Mask 3D clock or a text-only clock. "
                              "Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "24 Hours Clock", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_GRAPHICS_24_HOURS_CLOCK)
        .RaceDisable(false)
        .Options(CheckboxOptions().Tooltip(
            "Shows the time on a 24-hour clock instead of a 12-hour one. Majora's Mask only."));
}

/** Enhancements > Graphics, the Misc. column, after SoH's Draw Distance rows. */
void AddMmGraphicsBlockRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmBlock(menu, path);
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Motion Blur Mode", WIDGET_CVAR_COMBOBOX)
        .CVar(RSBS_CVAR_MM_GRAPHICS_MOTION_BLUR_MODE)
        .RaceDisable(false)
        .Options(ComboboxOptions()
                     .ComboMap(kMotionBlurModes)
                     .DefaultIndex(kMotionBlurDynamic)
                     .Tooltip("Chooses when the screen blurs with motion: Dynamic leaves it to the game's own scenes "
                              "and cutscenes, or keep it always off or always on. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Interpolate Motion Blur", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_GRAPHICS_MOTION_BLUR_INTERPOLATE)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            RequireOrEnable(info, MotionBlurMode() != kMotionBlurAlwaysOff,
                            "Requires Motion Blur Mode to be Dynamic or Always On.");
        })
        .Options(CheckboxOptions().Tooltip("Captures the motion blur on interpolated frames as well as game frames, "
                                           "which weakens the blur but smooths its trails. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Motion Blur Strength", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_MM_GRAPHICS_MOTION_BLUR_STRENGTH)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            RequireOrEnable(info, MotionBlurMode() == kMotionBlurAlwaysOn,
                            "Requires Motion Blur Mode to be Always On.");
        })
        .Options(IntSliderOptions().Min(0).Max(255).DefaultValue(180).Tooltip(
            "Sets how strong the motion blur is when Motion Blur Mode is Always On. Majora's Mask only."));
    // The gray note over the two register rows: shown while they are disabled because MM is not running.
    menu.AddWidget(path, "Majora's Mask Motion Blur Note", WIDGET_TEXT)
        .RaceDisable(false)
        .HideInSearch(true)
        .PreFunc([](WidgetInfo& note) {
            const bool waiting = MotionBlurMode() == kMotionBlurDynamic && !MM_MotionBlur_RegistersLive();
            SohMenu::ApplyPresentationNote(note, waiting ? SOH_MENU_PRESENT_CAPABILITY : SOH_MENU_PRESENT_LIVE,
                                           kDynamicNote);
        })
        .Options(TextOptions().Color(Colors::Gray));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Dynamic Motion Blur", WIDGET_CHECKBOX)
        .Accessor("MM_MotionBlur_SetDynamicEnabled")
        .ValuePointer(&sDynamicEnabled)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            if (GateRegisterRow(info)) {
                sDynamicEnabled = MM_MotionBlur_GetDynamicEnabled() != 0;
            }
        })
        .Callback([](WidgetInfo& info) { MM_MotionBlur_SetDynamicEnabled(sDynamicEnabled ? 1 : 0); })
        .Options(CheckboxOptions().Tooltip("Turns the blur on or off in the current scene; the game's own scenes and "
                                           "cutscenes turn it on and off as well. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Dynamic Motion Blur Strength", WIDGET_SLIDER_INT)
        .Accessor("MM_MotionBlur_SetDynamicAlpha")
        .ValuePointer(&sDynamicAlpha)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            if (!GateRegisterRow(info)) {
                return;
            }
            sDynamicAlpha = MM_MotionBlur_GetDynamicAlpha();
            RequireOrEnable(info, MM_MotionBlur_GetDynamicEnabled() != 0,
                            "Requires Dynamic Motion Blur to be enabled.");
        })
        .Callback([](WidgetInfo& info) { MM_MotionBlur_SetDynamicAlpha(sDynamicAlpha); })
        .Options(IntSliderOptions().Min(0).Max(255).DefaultValue(180).Tooltip(
            "Sets how strong the blur is in the current scene; the game's own scenes and cutscenes set it as well. "
            "Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Fix Scene Geometry Seams", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_GRAPHICS_FIX_GEOMETRY_SEAMS)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) { (void)MM_GfxPatches_Reapply(); })
        .Options(CheckboxOptions().Tooltip(
            "Closes a visible seam in the scene geometry of South Clock Town's ramp. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Disable Scene Geometry Distance Check", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_GRAPHICS_NO_GEOMETRY_DISTANCE_CHECK)
        .RaceDisable(false)
        .Callback([](WidgetInfo& info) { (void)MM_GfxPatches_Reapply(); })
        .Options(CheckboxOptions().Tooltip(
            "Draws some scene geometry, such as Clock Town seen from Termina Field, no matter how far away it is from "
            "the player. "
            "This may have unintended side effects. Majora's Mask only."));
}

} // namespace SohGui
