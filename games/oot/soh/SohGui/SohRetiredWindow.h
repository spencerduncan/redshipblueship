/**
 * A SoH window the single executable replaces with one of its own (#1132, operator ruling D1 of 2026-10-22: "replace
 * the soh tracker").
 *
 * The unified Check Tracker and Item Tracker (src/common/ComboTrackerWindow.h, ComboItemTrackerWindow.h) take SoH's
 * place: SoH's Randomizer > Check Tracker and Item Tracker pages open them, under SoH's window names. SoH's own two
 * windows are NOT unregistered, because their InitElement is what registers the hooks and save sections the unified
 * panes read (the Check Tracker's shop SEEN / IDENTIFIED, collected and scummed statuses, dialogue identification and
 * the trackerData save section; the Item Tracker's itemTrackerData section and its frame hook). They stay on the Gui,
 * under the names below, and are never drawn.
 *
 * RetiredInSingleExe<B> is that "registered, never drawn" wrapper. Init() reaches B's InitElement as before, so every
 * hook is registered. Draw() and the menu's embed path, DrawElement(), return at once, except while the UI snapshot
 * harness captures SoH's own window as a reference (gSohRetiredWindowReferenceDraw). The window is constructed with
 * no visibility CVar, so SoH's window key, which opens the unified window now, can never show it either.
 *
 * Locked by MenuComboSection (the wrapper on a spy, and the two real windows on a bare Gui with their hooks counted)
 * and by OoTWindowsGate's source scan (SetupGuiElements constructs both through it).
 */
#ifndef SOH_RETIRED_WINDOW_H
#define SOH_RETIRED_WINDOW_H

#ifdef __cplusplus

#include <utility>

#ifdef RSBS_SINGLE_EXECUTABLE

/** The registration names SoH's two retired windows keep on the shared Gui. The unprefixed "Check Tracker" and
 *  "Item Tracker" belong to the unified windows, which SoH's own toggle rows open by name. */
inline constexpr const char* kSohRetiredCheckTrackerWindowName = "SoH Check Tracker";
inline constexpr const char* kSohRetiredItemTrackerWindowName = "SoH Item Tracker";

/** True only while the UI snapshot harness captures one of SoH's retired windows as a reference
 *  (soh_ui_snapshot.cpp); defined in SohGui.cpp. Nothing else sets it. */
extern bool gSohRetiredWindowReferenceDraw;

template <typename BaseWindow> class RetiredInSingleExe : public BaseWindow {
  public:
    template <typename... Args> explicit RetiredInSingleExe(Args&&... args) : BaseWindow(std::forward<Args>(args)...) {
    }

    void Draw() override {
        if (!gSohRetiredWindowReferenceDraw) {
            return;
        }
        BaseWindow::Draw();
    }

    // The Port Menu's embed path (SohGui/Menu.cpp calls DrawElement() through a GuiWindow pointer), which
    // bypasses Draw().
    void DrawElement() override {
        if (!gSohRetiredWindowReferenceDraw) {
            return;
        }
        BaseWindow::DrawElement();
    }
};

#else // RSBS_SINGLE_EXECUTABLE

// Outside the single executable there is no replacement, so the window is SoH's own.
inline constexpr const char* kSohRetiredCheckTrackerWindowName = "Check Tracker";
inline constexpr const char* kSohRetiredItemTrackerWindowName = "Item Tracker";
template <typename BaseWindow> using RetiredInSingleExe = BaseWindow;

#endif // RSBS_SINGLE_EXECUTABLE

#endif // __cplusplus

#endif // SOH_RETIRED_WINDOW_H
