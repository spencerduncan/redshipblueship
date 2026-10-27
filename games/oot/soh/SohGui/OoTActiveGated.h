/**
 * Active-game gate for OoT's save/play-state windows in the single executable (#797).
 *
 * gSaveContext is one shared buffer (src/common/unified_save.c), so while Majora's Mask is the running game any OoT
 * window that reads it through OoT's SaveContext layout shows MM's bytes, and any window that writes it writes MM's
 * live save. The Save Editor's Info tab does that on every drawn frame (the magic capacity/clamp and the health clamp
 * in DrawInfoTab), the Message Viewer's "Display Message" dereferences a null OoT_gPlayState, and Time Splits writes
 * the stats block when its last split is clicked.
 *
 * OoTActiveGated<B> is the OoT twin of MM's MMActiveGated (games/mm/2s2h/TrackersGuiSingleExe.cpp), without the CVar
 * re-sync (OoT's own menu opens and closes its windows through Show()/Hide()). It gates all three ways into a
 * window's body:
 *
 *  - Draw(): the per-frame Gui::DrawFloatingWindows path, both GuiWindow::Draw and the four overrides (the three
 *    trackers and Time Splits).
 *  - UpdateElement(): Gui's unconditional per-frame Update() (the Message Viewer's "Display Message" crash path).
 *  - DrawElement(): the Port Menu's embed path. SohGui/Menu.cpp calls window->DrawElement() through a GuiWindow pointer
 *    when a WIDGET_WINDOW_BUTTON row's window is hidden and the row embeds (the default), which bypasses Draw() and so
 *    bypasses visibility entirely. DrawElement() is public and virtual on Ship::GuiElement, so the override is reached
 *    through that pointer.
 *
 * A gated window is never Hide()n: it stays visible-but-dormant, so its visibility CVar is untouched and it reappears
 * with the same contents when OoT is the running game again.
 *
 * The predicate is OoT_Gui_ShouldDraw(): Context_GetCurrentGame() == GAME_OOT. GAME_NONE is excluded, matching
 * MM_TrackersGui_ShouldDraw. The production wrap sites are the eight in SohGui::SetupGuiElements; the OoTWindowsGate
 * row (games/oot/soh/oot_windows_gate_test.cpp plus a source scan in src/common/test_runner.cpp) locks the gate and
 * holds every window that function constructs to a gated or an exempt list.
 */
#ifndef OOT_ACTIVE_GATED_H
#define OOT_ACTIVE_GATED_H

#ifdef __cplusplus
#include <utility>
#endif

#ifdef RSBS_SINGLE_EXECUTABLE

#ifdef __cplusplus
extern "C" {
#endif

/** True iff Ocarina of Time is the running game (Context_GetCurrentGame() == GAME_OOT). Defined in SohGui.cpp. */
bool OoT_Gui_ShouldDraw(void);

#ifdef __cplusplus
}

template <typename BaseWindow> class OoTActiveGated final : public BaseWindow {
  public:
    template <typename... Args> explicit OoTActiveGated(Args&&... args) : BaseWindow(std::forward<Args>(args)...) {
    }

    void Draw() override {
        if (!OoT_Gui_ShouldDraw()) {
            return;
        }
        BaseWindow::Draw();
    }

    void DrawElement() override {
        if (!OoT_Gui_ShouldDraw()) {
            return;
        }
        BaseWindow::DrawElement();
    }

  protected:
    void UpdateElement() override {
        if (!OoT_Gui_ShouldDraw()) {
            return;
        }
        BaseWindow::UpdateElement();
    }
};

/**
 * OoTActiveGated<B> for an SoH window that also HOSTS a Majora's Mask body (W6-TIMERS, #1237: SoH's Time Splits window,
 * whose body is 2Ship's Time Splits popout while MM runs, so the player has one Time Splits surface, not two).
 *
 * Under OoT it is exactly OoTActiveGated<B>. While HostMm's @c active() says MM's body is live, Draw() runs B's Draw(),
 * which must only frame the window (B's title, size, visibility key and colours; TimeSplitWindow::Draw pushes its
 * background colour and calls GuiWindow::Draw), and DrawElement() draws MM's body through @c drawElement() instead of
 * B's, so B's body never reads OoT state under MM. The page embed reaches DrawElement() directly, so it hosts too.
 * UpdateElement() stays OoT-only. Before HostMm is called it gates exactly as OoTActiveGated<B> does.
 */
template <typename BaseWindow> class OoTGatedHostingMm final : public BaseWindow {
  public:
    template <typename... Args> explicit OoTGatedHostingMm(Args&&... args) : BaseWindow(std::forward<Args>(args)...) {
    }

    /** Host MM's body: @p active says whether it is live (MM running), @p drawElement draws it. */
    void HostMm(bool (*active)(void), void (*drawElement)(void)) {
        mMmActive = active;
        mMmDrawElement = drawElement;
    }

    void Draw() override {
        if (OoT_Gui_ShouldDraw() || MmHosted()) {
            BaseWindow::Draw();
        }
    }

    void DrawElement() override {
        if (OoT_Gui_ShouldDraw()) {
            BaseWindow::DrawElement();
        } else if (MmHosted()) {
            mMmDrawElement();
        }
    }

  protected:
    void UpdateElement() override {
        if (!OoT_Gui_ShouldDraw()) {
            return;
        }
        BaseWindow::UpdateElement();
    }

  private:
    bool MmHosted() const {
        return mMmActive != nullptr && mMmDrawElement != nullptr && mMmActive();
    }

    bool (*mMmActive)(void) = nullptr;
    void (*mMmDrawElement)(void) = nullptr;
};
#endif // __cplusplus

#elif defined(__cplusplus)

// Outside the single executable there is no second game, so the wrap sites construct the window itself.
template <typename BaseWindow> using OoTActiveGated = BaseWindow;
template <typename BaseWindow> using OoTGatedHostingMm = BaseWindow;

#endif // RSBS_SINGLE_EXECUTABLE

#endif // OOT_ACTIVE_GATED_H
