/**
 * @file ComboMmOptionsWindow.h
 * @brief The Majora's Mask randomizer options pane (#497 step 4, #499; ADR
 *        0004, ADR 0008).
 *
 * Renders src/common/combo_mm_options_view.h's model: MM's full randomizer
 * option set, grouped the way MM's own (link-elided) rando menu groups it, with
 * every row capability-gated per ADR 0004 section 5.
 *
 * WHY THIS EXISTS. Before it, MM's 47 options had no host — the one live menu
 * in the binary is OoT's SohMenu and all of its headers are OoT-only, MM's
 * BenMenu shell is in no CMake target, and MM's own rando pane is elided into
 * `2ship_rando_ui`. So the paired MM world generated on `StaticData::Options`
 * defaults, every shuffle and every hint off, and the player had no way to say
 * otherwise (#499).
 *
 * WHY A COMMON-OWNED WINDOW AND NOT A SohMenu PANE. Two reasons, and the second
 * is the load-bearing one:
 *
 *  1. ADR 0008 already settled that a panel belonging to neither game is owned
 *     by src/common and registered on the shared Ship::Context Gui. This pane
 *     reads `gComboCtx` and CVars — never either game's `gSaveContext` — so it
 *     satisfies ADR 0008 rule 5 and needs no active-game gating to be SAFE.
 *  2. The paired profile is snapshotted when MM's cross-game arrival dispatches
 *     `OnSaveInit`, and an existing MM save is never regenerated. The chooser
 *     therefore has to be reachable **while OoT is active, before the switch**
 *     (#499's timing constraint). A common-owned window is reachable in every
 *     session state; that is the property being bought.
 *
 * THREE PRESENTATION STATES, per ADR 0004 section 6 — and they are three, not
 * two:
 *
 *  - LIVE: the option's behaviour is dispatched. Editable, unmarked.
 *  - EDITABLE BUT NOT ACTIVE: MM is not the running game. Still fully editable
 *    (that is the whole point — you set these before you cross), but the pane
 *    says so, because "editable" and "in effect right now" are different facts.
 *  - DISABLED BY CAPABILITY: the behaviour has no MM dispatch point (#438), so
 *    the row is drawn disabled WITH ITS REASON. A control that flips a CVar and
 *    changes nothing is the vacuous gate in UI form; collapsing this into
 *    "editable" would produce exactly that.
 *
 * Openability: `Ship::GuiWindow` latches its visibility CVar in the ctor and
 * nothing re-syncs CVar -> visibility per frame, so `Draw()` reads the CVar live
 * the way the spoiler window and MM's check tracker do (#489 cause 1).
 *
 * Locked ROM-free by the ComboMMOptionsWindow CTest: registration, idempotence,
 * name de-collision, and Draw()/Update() under all three GameIds with no ImGui
 * context. Its APPEARANCE — grouping, readability of the disabled rows, whether
 * the warnings land — is operator verification; no headless test can assert on
 * pixels.
 */

#ifndef RSBS_COMMON_COMBO_MM_OPTIONS_WINDOW_H
#define RSBS_COMMON_COMBO_MM_OPTIONS_WINDOW_H

#ifdef __cplusplus

#include <memory>
#include <ship/window/gui/GuiWindow.h>

namespace Ship {
class Gui;
}

namespace ComboGui {

// Registration name on the shared Gui. Distinct from SoH's unprefixed window
// names and MM's "MM "-prefixed ones: Gui::AddGuiWindow rejects duplicates
// SILENTLY from the caller's side, so a collision here would present as a
// window that simply never appears (ADR 0008 rule 2).
inline constexpr const char* kComboMMOptionsWindowName = "Majora's Mask Randomizer Options";

// Visibility CVar, in the same "gCombo.Windows.*" space as the spoiler view's.
inline constexpr const char* kComboMMOptionsVisibilityCVar = "gCombo.Windows.MMOptions";

class ComboMmOptionsWindow final : public Ship::GuiWindow {
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
 * Register the options pane on `gui` under kComboMMOptionsWindowName.
 * Idempotent (the #457 GetGuiWindow guard); a null `gui` is a no-op.
 */
void RegisterComboMmOptionsWindow(std::shared_ptr<Ship::Gui> gui);

} // namespace ComboGui

extern "C" {
#endif // __cplusplus

/**
 * Production entry point. Registers the options pane on the shared
 * Ship::Context Gui. Safe no-op when the context has no window/Gui, which is
 * the state every ROM-free harness runs in.
 */
void Combo_MMOptionsWindow_Init(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_MM_OPTIONS_WINDOW_H
