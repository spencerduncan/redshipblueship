/**
 * @file mm_time_splits_bridge.h
 * @brief How SoH's Time Splits window and page reach Majora's Mask's Time Splits (#1237, #1239; W6-TIMERS).
 *
 * THE ONE SURFACE. 2Ship keeps its splits tools in a settings popout (gWindows.Timesplits.Settings: the List Options
 * and Split Entries tabs) beside the titleless splits list. SoH keeps its own in its Time Splits window, which its
 * Enhancements > Time Splits page embeds. Rather than a second "Time Splits" window, SoH's window hosts MM's popout
 * body while MM is the running game (OoTGatedHostingMm in games/oot/soh/SohGui/OoTActiveGated.h), and the six List
 * Options switches are rows on SoH's page (SohMenuHostedMmTimers.cpp). The splits list itself is MM's own window,
 * registered beside MM's trackers (games/mm/2s2h/TimersGuiSingleExe.cpp).
 *
 * MM exports these C functions from TimersGuiSingleExe.cpp; the OoT side calls them through this header and never
 * includes an MM header (MM's and SoH's Time Splits headers both declare an unscoped SplitStatus enum).
 */

#ifndef RSBS_COMMON_MM_TIME_SPLITS_BRIDGE_H
#define RSBS_COMMON_MM_TIME_SPLITS_BRIDGE_H

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Whether SoH's Time Splits window shows MM's body: true while Majora's Mask is the running game. */
bool MM_TimeSplits_HostedActive(void);

/**
 * Draw MM's Time Splits popout body (List Options: compare list, list management and the current splits; Split
 * Entries) into the current ImGui window. Retrieves the saved lists from the splits file before its first draw. A no-op
 * unless MM_TimeSplits_HostedActive().
 */
void MM_TimeSplits_DrawHostedElement(void);

/**
 * Re-apply MM's Show Headers, Hide Background and Compare Splits switches to the splits list, as 2Ship's own rows do
 * when one changes (Timesplits.cpp UpdateSplitSettings). The page rows call it from their callbacks.
 */
void MM_TimeSplits_RefreshSettings(void);

/**
 * Point the splits file at @p path instead of the app directory's 2S2HTimeSplitData.json (null restores it), so a test
 * row or the UI harness reads and writes its own scratch file.
 */
void MM_TimeSplits_SetDataPathForTest(const char* path);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_MM_TIME_SPLITS_BRIDGE_H
