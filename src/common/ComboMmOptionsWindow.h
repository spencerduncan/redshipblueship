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
 *  2. The paired profile freezes into the world's identity at the CREATION
 *     event (#498/#564: Playthrough_Init stamps gComboCtx.mmProfileDigest),
 *     and an arrival that no longer matches the stamp is refused. The chooser
 *     therefore has to be reachable **while OoT is active, before creation**.
 *     A common-owned window is reachable in every session state; that is the
 *     property being bought.
 *
 * FOUR PRESENTATION STATES, per ADR 0004 section 6 as amended by #564 V25:
 *
 *  - LIVE: the option's behaviour is dispatched. Editable, unmarked.
 *  - EDITABLE BUT NOT ACTIVE: MM is not the running game. Still editable while
 *    the profile is unfrozen, but the pane says so, because "editable" and "in
 *    effect right now" are different facts.
 *  - DISABLED BY CAPABILITY: the behaviour has no MM dispatch point (#438), so
 *    the row is drawn disabled WITH ITS REASON (in its disabled tooltip, SoH's
 *    shape). A control that flips a CVar and changes nothing is the vacuous gate
 *    in UI form; collapsing this into "editable" would produce exactly that.
 *  - FROZEN AT CREATION (#498/#564): a creation event stamped the profile into
 *    the paired world's identity (Combo_MMProfileFrozen). Every row read-only,
 *    with the reason stated pane-wide by the gray state note and per row by the
 *    disabled tooltip; the underlying writers reject on their own, so the
 *    greying is honest rather than the gate.
 *
 * Drawn through the `combo_ui` seam (combo_ui.h), so the shipped binary renders
 * it with SoH's own widgets (UI parity M6, docs/ui-style-guide.md section 10).
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

// The narrowest the player can resize the pane to. A trick row is the box, up
// to two tag chips (the widest pair, "Intermediate" and "OoT Items", ends about
// 330 px in) and then the name, which wraps in whatever is left; below this the
// longest names would break into a word per line. The snapshot harness's
// tricks-narrow state renders the pane at exactly this width.
inline constexpr float kComboMMOptionsMinWidth = 560.0f;

// The Reset confirm's title (a SohGui::RegisterPopup title, so also the key the
// popup is dismissed by).
inline constexpr const char* kComboMMOptionsResetTitle = "Reset MM Randomizer Options";

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

/**
 * What the pane's Reset button does: queue the confirm (combo_ui's Confirm,
 * SohGui::RegisterPopup in the shipped binary) whose Reset clears every MM
 * option and trick CVar. Queues only; nothing is cleared until the player
 * confirms. Public so the UI snapshot harness can capture the confirm a player
 * gets without clicking.
 */
void Combo_MMOptionsRequestReset(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_MM_OPTIONS_WINDOW_H
