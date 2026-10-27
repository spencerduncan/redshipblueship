/**
 * MM's In-Game Timer overlay and Time Splits list on the shared Gui (#1235, #1237, #1239; W6-TIMERS).
 *
 * Upstream 2S2H creates both windows in BenGui.cpp::SetupGuiElements, an excluded TU, so neither existed in the single
 * exe. TimersGuiSingleExe.cpp registers them beside MM's tracker windows (TrackersGuiSingleExe.cpp, ADR 0008), drawn
 * only while Majora's Mask is the running game. 2Ship's third window, the Time Splits settings popout, is not
 * registered: SoH's own Time Splits window hosts its body while MM runs (src/common/mm_time_splits_bridge.h), so the
 * player has one Time Splits surface, not two.
 *
 * Locked ROM-free by the mm-trackers-gui CTest, leg 8 (mm_trackers_gui_test.cpp).
 */
#ifndef TIMERS_GUI_SINGLE_EXE_H
#define TIMERS_GUI_SINGLE_EXE_H

#ifdef RSBS_SINGLE_EXECUTABLE
#ifdef __cplusplus

#include <memory>
#include <string>

namespace Ship {
class Gui;
}

namespace S2H {
namespace TimersGui {

// Gui registration names, "MM "-prefixed like the tracker windows' and distinct from every name SoH registers.
inline constexpr const char* kInGameTimerWindowName = "MM In-Game Timer";
inline constexpr const char* kTimeSplitsWindowName = "MM Time Splits";

// The ImGui window names the vendored Draw()s begin (DisplayOverlay.cpp, Timesplits.cpp DrawSplitsList).
inline constexpr const char* kInGameTimerImGuiName = "Overlay";
inline constexpr const char* kTimeSplitsImGuiName = "Timesplits";

/**
 * Register the In-Game Timer and the Time Splits list on @p gui, each drawn only while MM is the running game
 * (MM_TrackersGui_ShouldDraw). Idempotent. S2H::TrackersGui::RegisterWindows calls it, so MM_TrackersGui_Init (at
 * startup and at MM's init) registers these with the trackers.
 */
void RegisterWindows(std::shared_ptr<Ship::Gui> gui);

} // namespace TimersGui
} // namespace S2H

/** The splits file: the app directory's 2S2HTimeSplitData.json, or a test's path (MM_TimeSplits_SetDataPathForTest).
 *  TimeSplitsActions.cpp reads and writes it through this. */
std::string MM_TimeSplits_DataPath();

#endif // __cplusplus
#endif // RSBS_SINGLE_EXECUTABLE

#endif // TIMERS_GUI_SINGLE_EXE_H
