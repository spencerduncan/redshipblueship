/**
 * @file ComboTrackerWindow.h
 * @brief The combo tracker window: both games' progress in one panel (#458;
 *        ADR 0008).
 *
 * Renders src/common/combo_tracker_view.h's model: the combo identity header,
 * one panel per game fed ONLY by that game's registered adapter, and the
 * cross-game crossings in both directions (the crossing store, plus the legacy
 * pinned tables; combo_tracker_view.h "Cross-game crossings"). Every panel is labelled with its
 * freshness — the inactive game's data is honest about being "As of the last
 * game switch or save" (MM's shadow, written at freeze/save) or "As of the last
 * game switch" (OoT's heap, stopped at suspend), never presented as live.
 *
 * Common-owned Gui window (ADR 0008), the ComboSpoilerWindow pattern: it
 * reads gComboCtx and the adapter surfaces and NOTHING else — no gSaveContext
 * through either game's layout — which is what makes it safe to draw under
 * GAME_OOT, GAME_MM and GAME_NONE alike with no MMActiveGated-style wrapper.
 *
 * Openability: `Ship::GuiWindow` latches its visibility CVar in the ctor and
 * nothing re-syncs CVar -> visibility per frame, so `Draw()` reads the CVar
 * live (#489 cause 1, same defect class as MM's check tracker).
 *
 * Locked ROM-free by the ComboTrackerWindow CTest: registration, idempotence,
 * name de-collision, and Draw()/Update() under all three GameIds with no
 * ImGui context. Its APPEARANCE is operator verification — no headless test
 * can assert on pixels.
 */

#ifndef RSBS_COMMON_COMBO_TRACKER_WINDOW_H
#define RSBS_COMMON_COMBO_TRACKER_WINDOW_H

#ifdef __cplusplus

#include <cstdint>
#include <memory>
#include <ship/window/gui/GuiWindow.h>

namespace Ship {
class Gui;
}

namespace ComboGui {

// Registration name on the shared Gui. Distinct from SoH's unprefixed tracker
// names, MM's "MM "-prefixed ones, and the other common-owned windows:
// Gui::AddGuiWindow rejects duplicates SILENTLY from the caller's side, so a
// collision here would present as a window that simply never appears.
inline constexpr const char* kComboTrackerWindowName = "Combo Tracker";

// Visibility CVar in the "gCombo.*" namespace (neither OoT's "gOpenWindows.*"
// nor MM's "gWindows.*", so no store collision).
inline constexpr const char* kComboTrackerVisibilityCVar = "gCombo.Windows.Tracker";

class ComboTrackerWindow final : public Ship::GuiWindow {
  public:
    using Ship::GuiWindow::GuiWindow;

    void Draw() override;

  protected:
    void DrawElement() override;
    void InitElement() override {
    }
    void UpdateElement() override {
    }
};

/**
 * Register the tracker window on `gui` under kComboTrackerWindowName.
 * Idempotent (the #457 GetGuiWindow guard); a null `gui` is a no-op.
 */
void RegisterComboTrackerWindow(std::shared_ptr<Ship::Gui> gui);

/**
 * One direction's crossings, drawn the one way both panes draw them (#755): a
 * short section header ("In MM Checks" / "In OoT Checks"), a gray note saying
 * how many of the other game's items the host game holds and how many of their
 * checks the host game's save has collected, and SoH's table shape with the
 * host check's name, the item's name and whether the check was collected. The
 * Combo Tracker and the Cross-Game Spoiler both call this, so the two panes
 * cannot list a world's crossings differently. Must be called inside an ImGui
 * window, between the seam's PushTheme/PopTheme or not.
 */
void DrawCrossingList(uint8_t hostGame);

} // namespace ComboGui

extern "C" {
#endif // __cplusplus

/**
 * Production entry point. Registers both games' tracker adapters (so the
 * model is populated even for tests that never construct a Gui), then the
 * window on the shared Ship::Context Gui. Safe no-op past the adapter step
 * when the context has no window/Gui — the state every ROM-free harness runs
 * in.
 */
void Combo_TrackerWindow_Init(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_TRACKER_WINDOW_H
