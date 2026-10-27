/**
 * @file SohMenuHostedMmCamGyro.cpp
 * @brief Majora's Mask's Remote Bombchu Control, hosted on SoH's Enhancements > Items page, Explosives section
 *        (#1236, #1240), through the hosted-row seam (SohMenuHostedMm.h).
 *
 * 2Ship draws the row in its own menu (the excluded BenMenu.cpp), so in the single exe nothing wrote the key, and its
 * provider, Equipment/RemoteBombchu.cpp, was held out of the link by a fence in Camera/FreeLook.cpp. W6-CAM-GYRO links
 * it (games/mm/CMakeLists.txt) and lifts the fence. The row sits right after SoH's Prevent Bombchus Forcing
 * First-Person, its nearest relative; SoH has no twin, so it keeps 2Ship's label and its tooltip ends "Majora's Mask
 * only." The rest of the lane's rows (gyro aiming, the free-look pitch limits, the debug camera and the speed
 * modifiers) sit in SoH's Bindings window beside their relatives (SohInputEditorWindow.cpp, DrawMmCameraRows).
 */

#include "SohMenuHostedMm.h"

#include "soh/SohGui/UIWidgets.hpp"

#include "cvar_shared_keys.h" // src/common: the RSBS_CVAR_MM_* defines

namespace SohGui {

using namespace UIWidgets;

// Items > Explosives, right after SoH's Prevent Bombchus Forcing First-Person.
void AddMmRemoteBombchuRow(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Explosives", "Remote Bombchu Control", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PLAYER_ACTIONS_REMOTE_BOMBCHU)
        .Options(CheckboxOptions().Tooltip(
            "Steers a moving Bombchu with the control stick: B detonates it, and A hands control back to Link. "
            "Majora's Mask only."));
}

} // namespace SohGui
