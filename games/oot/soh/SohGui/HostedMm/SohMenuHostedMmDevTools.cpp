/**
 * @file SohMenuHostedMmDevTools.cpp
 * @brief Majora's Mask's developer rows, hosted on SoH's Dev Tools > General through the hosted-row seam (#1238,
 *        #1240, lane W6-DEV).
 *
 * 2Ship draws these in its own Dev Tools menu (the excluded BenMenu.cpp), so in the single exe nothing wrote their keys
 * and their TUs were out of the link (games/mm/CMakeLists.txt now links DeveloperTools/DeveloperTools.cpp,
 * ObjectDependency.cpp and WarpPoint.cpp under its W6-DEV block). SoH's page calls each function below in one line:
 *  - Warping: "MM Better Debug Warp Screen" right after SoH's Better Debug Warp Screen. MM's map select is its own
 *    screen (DeveloperTools/BetterMapSelect.c, already linked) with its own key, default off.
 *  - Warping: MM's warp point right after SoH's Warp Points. SoH's widget stores OoT entrances and warps OoT's play
 *    state, so it cannot hold a Termina point; these rows set, warp to and clear 2Ship's single point through
 *    src/common/mm_dev_tools_bridge.h, and only while MM is in gameplay. 2Ship's Boot to Warp Point has no row: it is
 *    out of the single exe (WarpPoint.cpp says why).
 *  - the end of the first column: a "Majora's Mask" block with Prevent Actor Update / Draw / Init and Disable Object
 *    Dependency. SoH has no twin of any. Like 2Ship, they act only under Debug Mode (MM's RegisterDebugMode clears them
 *    when Debug Mode goes off), so the rows are disabled, with SoH's reason, while it is off.
 * SoH's own Frame Advance rows and Debug Save File Mode need no MM row: the first follow the running game
 * (SohMenu.cpp's RunningFrameAdvanceEnabled), and the second is one shared key MM now honours on its own new-save path.
 *
 * Every row keeps 2Ship's wording, in SoH's Title Case, and its tooltip ends "Majora's Mask only."
 */

#include "SohMenuHostedMm.h"

#include "soh/SohGui/UIWidgets.hpp"

#include "cvar_shared_keys.h"    // src/common: the RSBS_CVAR_MM_* defines
#include "mm_dev_tools_bridge.h" // src/common: MM's warp point

#include <string>

namespace SohGui {

using namespace UIWidgets;

namespace {

/** Disable a Debug-Mode-only MM row while Debug Mode is off, with SoH's own reason. */
void DisableWithoutDebugMode(WidgetInfo& info) {
    if (!CVarGetInteger(CVAR_DEVELOPER_TOOLS("DebugEnabled"), 0)) {
        info.activeDisables.push_back(DISABLE_FOR_DEBUG_MODE_OFF);
    }
}

} // namespace

// The end of Dev Tools > General's first column: MM's switches with no OoT twin.
void AddMmDevToolsBlock(SohMenu& menu, WidgetPath& path) {
    AddHostedMmBlock(menu, path);
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Prevent Actor Update", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PREVENT_ACTOR_UPDATE)
        .PreFunc(DisableWithoutDebugMode)
        .Options(CheckboxOptions().Tooltip("Prevents actors from updating. Needs Debug Mode. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Prevent Actor Draw", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PREVENT_ACTOR_DRAW)
        .PreFunc(DisableWithoutDebugMode)
        .Options(CheckboxOptions().Tooltip("Prevents actors from drawing. Needs Debug Mode. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Prevent Actor Init", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PREVENT_ACTOR_INIT)
        .PreFunc(DisableWithoutDebugMode)
        .Options(CheckboxOptions().Tooltip(
            "Prevents every actor but Link from initializing. Needs Debug Mode. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Disable Object Dependency", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_DISABLE_OBJECT_DEPENDENCY)
        .PreFunc(DisableWithoutDebugMode)
        .Options(CheckboxOptions().Tooltip(
            "Disables dependencies when loading objects. Needs Debug Mode. Majora's Mask only."));
}

// Warping, right after SoH's Better Debug Warp Screen.
void AddMmBetterMapSelectRow(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Warping", "MM Better Debug Warp Screen", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_BETTER_MAP_SELECT)
        .Options(CheckboxOptions().Tooltip(
            "Replaces the original map select with a translated, more user-friendly version. Majora's Mask only."));
}

// Warping, right after SoH's Warp Points.
void AddMmWarpPointRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Warping", "MM Set Warp Point", WIDGET_BUTTON)
        .Accessor("MM_DevTools_SetWarpPoint")
        .Options(
            ButtonOptions()
                .Size(Sizes::Inline)
                .Tooltip(
                    "Saves where Link stands as the warp point, replacing the one saved before. Majora's Mask only."))
        .PreFunc([](WidgetInfo& info) {
            if (!MM_DevTools_WarpPointAvailable()) {
                info.activeDisables.push_back(DISABLE_FOR_MM_NOT_IN_GAMEPLAY);
            }
        })
        .Callback([](WidgetInfo& info) { MM_DevTools_SetWarpPoint(); });
    AddHostedMmWidget(menu, path, "Warping", "MM Warp to Warp Point", WIDGET_BUTTON)
        .Accessor("MM_DevTools_WarpToWarpPoint")
        .Options(ButtonOptions().Size(Sizes::Inline).Tooltip("Warps Link to the saved warp point. Majora's Mask only."))
        .PreFunc([](WidgetInfo& info) {
            // The saved point's place, in the tooltip: the row's name stays its own (SoH never writes state into it).
            static std::string sTooltip;
            char where[96];
            sTooltip = MM_DevTools_DescribeWarpPoint(where, sizeof(where))
                           ? std::string("Warps Link to the saved warp point: ") + where + ". Majora's Mask only."
                           : std::string("Warps Link to the saved warp point. Majora's Mask only.");
            info.options->tooltip = sTooltip.c_str();
            if (!MM_DevTools_WarpPointSaved()) {
                info.activeDisables.push_back(DISABLE_FOR_NO_MM_WARP_POINT);
            }
            if (!MM_DevTools_WarpPointAvailable()) {
                info.activeDisables.push_back(DISABLE_FOR_MM_NOT_IN_GAMEPLAY);
            }
        })
        .Callback([](WidgetInfo& info) { MM_DevTools_WarpToWarpPoint(); });
    AddHostedMmWidget(menu, path, "Warping", "MM Clear Warp Point", WIDGET_BUTTON)
        .Accessor("MM_DevTools_ClearWarpPoint")
        .Options(ButtonOptions().Size(Sizes::Inline).Tooltip("Forgets the saved warp point. Majora's Mask only."))
        .PreFunc([](WidgetInfo& info) {
            if (!MM_DevTools_WarpPointSaved()) {
                info.activeDisables.push_back(DISABLE_FOR_NO_MM_WARP_POINT);
            }
        })
        .Callback([](WidgetInfo& info) { MM_DevTools_ClearWarpPoint(); })
        .SameLine(true);
}

} // namespace SohGui
