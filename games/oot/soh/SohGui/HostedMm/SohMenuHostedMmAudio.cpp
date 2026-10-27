/**
 * @file SohMenuHostedMmAudio.cpp
 * @brief Majora's Mask rows hosted on SoH's Settings > Audio page (#1235), through the hosted-row seam.
 *
 * Ambience Volume. MM has a fifth sequence player SoH does not (wind, birds, night sounds), and MM_AudioMgr_Init reads
 * its volume from RSBS_CVAR_VOLUME_AMBIENCE, the volume family's spelling (ADR 0003 section 5.1: integer percent,
 * default 100). 2Ship's own slider lives in the excluded BenMenu.cpp, so nothing in the single exe wrote the key. The
 * row is SoH's slider in SoH's shape, right after Sound Effects Volume: SohMenuSettings.cpp calls
 * AddMmAmbienceVolumeRow in one line. It has no OoT twin, so it keeps its plain label and says "Majora's Mask only."
 * Locked by MenuMmEnhancementRows legs 8 and 9.
 */

#include "SohMenuHostedMm.h"

#include "soh/SohGui/UIWidgets.hpp"

#include "cvar_shared_keys.h" // src/common: RSBS_CVAR_VOLUME_AMBIENCE
#ifdef RSBS_SINGLE_EXECUTABLE
#include "mm_volume_bridge.h" // src/common: the live apply to MM's sequence players
#endif

namespace SohGui {

void AddMmAmbienceVolumeRow(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "", "Ambience Volume: %d %%", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_VOLUME_AMBIENCE)
        .RaceDisable(false)
        .Options(UIWidgets::IntSliderOptions().Min(0).Max(100).DefaultValue(100).ShowButtons(true).Format("").Tooltip(
            "Adjusts the volume of ambient sounds such as wind and birdsong. Majora's Mask only."))
        // MM reads the key in MM_AudioMgr_Init only; this reaches its players while MM's audio is up.
        .Callback([](WidgetInfo& info) {
#ifdef RSBS_SINGLE_EXECUTABLE
            MM_Volume_ReapplyPortScales();
#endif
        });
}

} // namespace SohGui
