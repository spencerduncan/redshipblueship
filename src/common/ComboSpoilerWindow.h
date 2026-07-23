/**
 * @file ComboSpoilerWindow.h
 * @brief The in-game cross-game spoiler panel (#496 steps 3-4; ADR 0008).
 *
 * Renders src/common/combo_spoiler_view.h's model: which MM check hosts which
 * OoT item, and whether it has been collected yet. Before this, the paired
 * world's spoiler existed only as `randomizer-mm/RSBSPAIR<masterSeed>.json`
 * and the operator had to be told an absolute path to read their own seed.
 *
 * This is the first common-owned Gui window (ADR 0008). It reads `gComboCtx`
 * and NOTHING else — no `gSaveContext` through either game's layout — which is
 * what makes it safe to draw under GAME_OOT, GAME_MM and GAME_NONE alike, and
 * why it needs no equivalent of MM's `MMActiveGated` wrapper.
 *
 * Openability: `Ship::GuiWindow` latches its visibility CVar in the ctor and
 * nothing re-syncs CVar -> visibility per frame, so `Draw()` reads the CVar
 * live the way MM's check tracker does (#489 cause 1, same defect class).
 *
 * Locked ROM-free by the ComboSpoilerWindow CTest: registration, idempotence,
 * name de-collision, and Draw()/Update() under all three GameIds with no ImGui
 * context. Its APPEARANCE is operator verification — no headless test can
 * assert on pixels.
 */

#ifndef RSBS_COMMON_COMBO_SPOILER_WINDOW_H
#define RSBS_COMMON_COMBO_SPOILER_WINDOW_H

#ifdef __cplusplus

#include <memory>
#include <ship/window/gui/GuiWindow.h>

namespace Ship {
class Gui;
}

namespace ComboGui {

// Registration name on the shared Gui. Distinct from SoH's unprefixed tracker
// names and MM's "MM "-prefixed ones: Gui::AddGuiWindow rejects duplicates
// SILENTLY from the caller's side, so a collision here would present as a
// window that simply never appears (ADR 0008).
inline constexpr const char* kComboSpoilerWindowName = "Cross-Game Spoiler";

// Visibility CVar. "gCombo.*" is neither OoT's "gOpenWindows.*" nor MM's
// "gWindows.*", so there is no store collision.
inline constexpr const char* kComboSpoilerVisibilityCVar = "gCombo.Windows.Spoiler";

class ComboSpoilerWindow final : public Ship::GuiWindow {
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
 * Register the spoiler window on `gui` under kComboSpoilerWindowName.
 * Idempotent (the #457 GetGuiWindow guard); a null `gui` is a no-op.
 */
void RegisterComboSpoilerWindow(std::shared_ptr<Ship::Gui> gui);

} // namespace ComboGui

extern "C" {
#endif // __cplusplus

/**
 * Production entry point. Registers the spoiler window on the shared
 * Ship::Context Gui. Safe no-op when the context has no window/Gui, which is
 * the state every ROM-free harness runs in.
 */
void Combo_SpoilerWindow_Init(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_SPOILER_WINDOW_H
