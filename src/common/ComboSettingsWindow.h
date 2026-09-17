/**
 * @file ComboSettingsWindow.h
 * @brief The combo settings pane: the five tier-4 `gCombo.Rando.*` keys
 *        (ADR 0011 increment 2, #498; ADR 0003; ADR 0004 §6; ADR 0008).
 *
 * Renders src/common/combo_settings_view.h's model — direction, per-direction
 * pool sizes, per-direction item classes — and, once a creation event has
 * frozen them, the values FROM THE SAVE through Combo_ComboSettingsSummary.
 *
 * SUPERSEDED AS THE LIVE SURFACE (2026-09-16, #655). Operator direction after
 * the 2026-09-11 nightly: "For the combo settings they should be built into the
 * menu itself like all the other combo settings instead of being pop out
 * panes." The five settings are SohMenu rows now, in the interim Cross-Game
 * section of games/oot/soh/SohGui/SohMenuRandomizer.cpp, and the
 * WIDGET_WINDOW_BUTTON row that used to open this pane is gone — nothing in the
 * menu writes kComboSettingsVisibilityCVar, so this window no longer appears.
 *
 * It is KEPT, unshown, rather than deleted, for reasons that are about the
 * repository and not about taste:
 *
 *  - Two of its three registration callers are outside #655's file scope
 *    (ComboMmOptionsWindow.cpp registers it as the tier-4 twin of the MM options
 *    pane; rsbs/src/main.cpp calls it directly), and deleting it would edit a
 *    sibling pane's bring-up to no behavioural end.
 *  - kComboSettingsVisibilityCVar is a classified entry in cvar_shared_keys.h's
 *    tier-4 manifest; removing the window would orphan a manifest row in a file
 *    whose classification lock is deliberately hard to edit.
 *  - Its headless lock (the ComboSettingsWindow CTest) is the assertion that a
 *    common-owned pane over these keys stays game-agnostic under all three
 *    GameIds, which is ADR 0008 rule 5's tripwire and is worth keeping green.
 *
 * Reason 3 below — the enforcement argument — is the one that did NOT survive
 * contact: a SohMenu row does not have to be its own writer. The rows use the
 * POINTER-based widget types (WIDGET_COMBOBOX / WIDGET_SLIDER_INT /
 * WIDGET_CHECKBOX) over a staging buffer, refresh that buffer from the model in
 * their PreFunc and offer every edit to Combo_ComboSettingSet in their
 * Callback, so src/common's writers are still the only choke point and still
 * the only gate. Only a WIDGET_CVAR_* row would have had the defect that
 * argument describes.
 *
 * The original argument, recorded as written, for the two reasons that do still
 * hold (a common-owned window reads gComboCtx and never gSaveContext, and it
 * does not hang off either game's boot):
 *
 * WHY A COMMON-OWNED WINDOW AND NOT ROWS IN THE INTERIM Cross-Game HOST. Three
 * reasons, each from a different ADR, and each sufficient on its own:
 *
 *  1. ADR 0004 §6 state 4 says it outright: "the host of every one of these
 *     keys is a common-owned window, which may not read gSaveContext at all
 *     (ADR 0008 rule 5)". These are world-identity keys; that sentence is
 *     about them.
 *  2. ADR 0008 rule 1's ownership test is about the DATA: a window whose
 *     source is gComboCtx is owned by src/common. This pane reads gComboCtx
 *     (through Combo_ComboSettingsSummary) and CVars, and never either game's
 *     gSaveContext, so it is game-neutral and safe under every GameId. Rows
 *     inside SohMenuRandomizer.cpp would put tier-4 authoring in an OoT-owned
 *     file and hang its existence off OoT's boot.
 *  3. ADR 0004 §6's enforcement rule: "the pane is one caller of the
 *     src/common write choke points, and the gate belongs on those". A
 *     SohMenu widget IS its own writer — UIWidgets writes the CVar directly —
 *     so there would be no choke point for Combo_ComboSettingsFrozen() to
 *     gate. This window writes only through Combo_ComboSettingSet /
 *     Combo_ComboSettingClear, which refuse on their own.
 *
 * The interim Cross-Game host keeps what ADR 0008 explicitly leaves it: a
 * WIDGET_WINDOW_BUTTON row that OPENS this window. Registering and opening are
 * separate questions, and only the first is settled by rule 1.
 * (#655 removed that row. Registration is untouched, so the window is still
 * openable from the console — `set gCombo.Windows.ComboSettings 1` — and its
 * lock still drives Draw(); nothing in the UI opens it.)
 *
 * THE PRESENTATION STATES (ADR 0004 §6):
 *
 *  - PRE-CREATION: editable. Every write goes through the src/common writers.
 *    The pane says what these are — rules that freeze into the next paired
 *    world's identity — so "editable" is not mistaken for "in effect now".
 *  - FROZEN AT CREATION (state 4): read-only, with the reason string
 *    "already decided" — NOT the capability reason, which would send a player
 *    looking for a missing feature — and the values read from the save
 *    (Combo_ComboSettingsSummary), never from the CVar, because after creation
 *    the two may legitimately differ and the save is what the world was built
 *    from. The greying is honest rather than the gate: the writers reject on
 *    their own.
 *  - The pane deliberately has no "editable but not active" state: tier-4
 *    keys apply at creation, not at a game's resume, so that state has no
 *    meaning for them.
 *
 * Openability: `Ship::GuiWindow` latches its visibility CVar in the ctor and
 * nothing re-syncs CVar -> visibility per frame, so `Draw()` reads the CVar
 * live the way the sibling common-owned windows do (#489 cause 1).
 *
 * Locked ROM-free by the ComboSettingsWindow CTest: registration, idempotence,
 * name de-collision against both games' windows and the sibling combo
 * windows, and Draw()/Update() under all three GameIds — unpaired, paired
 * and frozen — with no ImGui context. Its APPEARANCE is operator
 * verification; no headless test can assert on pixels.
 */

#ifndef RSBS_COMMON_COMBO_SETTINGS_WINDOW_H
#define RSBS_COMMON_COMBO_SETTINGS_WINDOW_H

#include "cvar_shared_keys.h" // RSBS_CVAR_COMBO_WINDOW_COMBO_SETTINGS

#ifdef __cplusplus

#include <memory>
#include <ship/window/gui/GuiWindow.h>

namespace Ship {
class Gui;
}

namespace ComboGui {

// Registration name on the shared Gui. Distinct from SoH's unprefixed window
// names, MM's "MM "-prefixed ones, and the three sibling common-owned windows:
// Gui::AddGuiWindow rejects duplicates SILENTLY from the caller's side, so a
// collision here would present as a window that simply never appears (ADR
// 0008 rule 2).
inline constexpr const char* kComboSettingsWindowName = "Combo Settings";

// Visibility CVar, in the same "gCombo.Windows.*" space as the sibling
// windows'. A PREFERENCE key (no identity role) — classified as such in
// cvar_shared_keys.h's tier-4 manifest.
inline constexpr const char* kComboSettingsVisibilityCVar = RSBS_CVAR_COMBO_WINDOW_COMBO_SETTINGS;

class ComboSettingsWindow final : public Ship::GuiWindow {
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
 * Register the pane on `gui` under kComboSettingsWindowName. Idempotent (the
 * #457 GetGuiWindow guard); a null `gui` is a no-op.
 */
void RegisterComboSettingsWindow(std::shared_ptr<Ship::Gui> gui);

} // namespace ComboGui

extern "C" {
#endif // __cplusplus

/**
 * Production entry point. Registers the pane on the shared Ship::Context Gui.
 * Safe no-op when the context has no window/Gui, which is the state every
 * ROM-free harness runs in. Idempotent, so it may be reached from more than
 * one bring-up path.
 */
void Combo_ComboSettingsWindow_Init(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_SETTINGS_WINDOW_H
