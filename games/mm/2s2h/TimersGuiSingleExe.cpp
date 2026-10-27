/**
 * MM's In-Game Timer overlay and Time Splits on the shared Gui (#1235, #1237, #1239; W6-TIMERS).
 *
 * Upstream 2S2H creates its three timer windows in BenGui.cpp::SetupGuiElements, an excluded TU (the same gap
 * TrackersGuiSingleExe.cpp closes for the trackers, ADR 0008):
 *
 *  - "Display Overlay" (DisplayOverlay.cpp), the In-Game Timer. Registered here as "MM In-Game Timer". 2Ship shows it
 *    through gWindows.DisplayOverlay, which only its Display combo's callback (BenMenu.cpp, excluded) wrote; here it
 *    has no visibility key at all, and the In-Game Timer row (gDisplayOverlay.Mode) alone decides, because
 *    DisplayOverlayWindow::Draw already returns while the mode is Off.
 *  - "Time Splits Window" (Timesplits.cpp), the titleless splits list. Registered here as "MM Time Splits" under
 *    2Ship's gWindows.Timesplits; its Draw() ignores that key and draws while Enable Time Splits is on, as upstream.
 *  - "Time Splits Settings Window" (TimesplitsSettings.cpp), the popout. NOT registered: its body is drawn inside
 *    SoH's own Time Splits window while MM runs (MM_TimeSplits_DrawHostedElement below, through
 *    src/common/mm_time_splits_bridge.h), so the player has one Time Splits surface, not two, and its six switches
 *    are rows on SoH's Enhancements > Time Splits page.
 *
 * Every window is drawn only while Majora's Mask is the running game: gSaveContext is one shared buffer, so an MM
 * window drawn under OoT would read OoT's bytes through MM's layout. Locked by mm-trackers-gui leg 8.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "TimersGuiSingleExe.h"
#include "TrackersGuiSingleExe.h" // MM_TrackersGui_ShouldDraw

#include <memory>
#include <string>
#include <utility>

#include <ship/Context.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>

#include <libultraship/bridge/consolevariablebridge.h>

#include "2s2h/Enhancements/Trackers/DisplayOverlay.h"
#include "2s2h/Enhancements/Trackers/TimeSplits/Timesplits.h"
#include "2s2h/Enhancements/Trackers/TimeSplits/TimesplitsSettings.h"

#include "mm_time_splits_bridge.h" // src/common: the C surface SoH's window and page call

// TimesplitsSettings.cpp's list selection (Timesplits.h declares only comparedIndex).
extern uint32_t selectedIndex;

namespace {

/**
 * Active-game gate for the timer windows: every way into the body (the per-frame Draw(), Gui's per-frame Update(),
 * and DrawElement() for a caller that embeds) runs only while MM is the running game. Unlike the trackers' wrapper it
 * neither re-syncs a visibility key nor adds a Close menu: neither window is opened through a key (see the file
 * comment), and each closes through its own row.
 */
template <typename BaseWindow> class MMTimerGated final : public BaseWindow {
  public:
    template <typename... Args> explicit MMTimerGated(Args&&... args) : BaseWindow(std::forward<Args>(args)...) {
    }

    void Draw() override {
        if (!MM_TrackersGui_ShouldDraw()) {
            return;
        }
        BaseWindow::Draw();
    }

    void DrawElement() override {
        if (!MM_TrackersGui_ShouldDraw()) {
            return;
        }
        BaseWindow::DrawElement();
    }

  protected:
    void UpdateElement() override {
        if (!MM_TrackersGui_ShouldDraw()) {
            return;
        }
        BaseWindow::UpdateElement();
    }
};

std::string sDataPathOverride;

// Whether the saved list names have been read for the hosted body. A test path resets it.
bool sListsRetrieved = false;

void EnsureSavedListsRetrieved() {
    if (!sListsRetrieved || savedLists.empty()) {
        // Upstream reads the names once, from the excluded boot path (TimeSplitsActions.cpp RegisterTimesplits). The
        // List Options tab indexes savedLists[0], so the read happens before the first draw; RETRIEVE always leaves
        // at least the "Create a List First" entry.
        SplitSaveFileAction(SPLIT_RETRIEVE, "");
        sListsRetrieved = true;
    }
    if (selectedIndex >= savedLists.size()) {
        selectedIndex = 0;
    }
    if (comparedIndex >= savedLists.size()) {
        comparedIndex = 0;
    }
}

} // namespace

namespace S2H {
namespace TimersGui {

void RegisterWindows(std::shared_ptr<Ship::Gui> gui) {
    if (gui == nullptr || gui->GetGuiWindow(kInGameTimerWindowName) != nullptr) {
        return;
    }
    // Visible from construction with no key (GuiWindow's (key, isVisible, name) ctor): the mode row is the switch.
    gui->AddGuiWindow(std::make_shared<MMTimerGated<DisplayOverlayWindow>>("", true, kInGameTimerWindowName));
    gui->AddGuiWindow(std::make_shared<MMTimerGated<TimesplitsWindow>>("gWindows.Timesplits", kTimeSplitsWindowName));
}

} // namespace TimersGui
} // namespace S2H

std::string MM_TimeSplits_DataPath() {
    if (!sDataPathOverride.empty()) {
        return sDataPathOverride;
    }
    return Ship::Context::GetPathRelativeToAppDirectory("2S2HTimeSplitData.json");
}

extern "C" bool MM_TimeSplits_HostedActive(void) {
    return MM_TrackersGui_ShouldDraw();
}

extern "C" void MM_TimeSplits_DrawHostedElement(void) {
    if (!MM_TimeSplits_HostedActive()) {
        return;
    }
    EnsureSavedListsRetrieved();
    // The popout's body, drawn into SoH's window (or its page embed). Never registered on the Gui: an empty key keeps
    // its ctor away from the config, and its InitElement is empty.
    static TimesplitsSettingsWindow sBody("", "MM Time Splits Settings");
    sBody.DrawElement();
}

extern "C" void MM_TimeSplits_RefreshSettings(void) {
    UpdateSplitSettings(SPLIT_HEADERS);
    UpdateSplitSettings(SPLIT_OPACITY);
    if (CVarGetInteger("gSettings.TimeSplits.Compare", 0) != 0) {
        EnsureSavedListsRetrieved();
        UpdateSplitSettings(SPLIT_COMPARE);
    }
}

extern "C" void MM_TimeSplits_SetDataPathForTest(const char* path) {
    sDataPathOverride = path != nullptr ? path : "";
    sListsRetrieved = false;
}

#endif // RSBS_SINGLE_EXECUTABLE
