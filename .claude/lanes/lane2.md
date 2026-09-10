You are Lane 2 of eight in the 2026-09-10 wave: **ADR 0011 increment 2 — the five tier-4 `gCombo.Rando.*` keys, the creation-time resolver that reads them, and the pane that renders ADR 0004 §6 state 4 after creation.**

- **Branch:** `claude/adr0011-inc2-combo-settings-pane`
- **Serves:** #498 via ADR 0011 (`docs/adr/0011-combo-level-settings.md`, Accepted 2026-08-05), "Increment 2 — the tier-4 keys and the pane" (`:675-683`). Increment 1 landed in PR #628 (the frozen 12-byte `ComboSettingsRecord` at `src/common/context.h:816`, `comboSettingsHash` at `:795`, `Combo_ForeignPairingRequested()` at `foreign_items.h:509`, compare-and-refuse). #628 also left eight review resolutions to fold into the ADR — do that in the same PR.

## Scope

1. The keys: `gCombo.Rando.Direction`, `.PoolSize.OoT`, `.PoolSize.MM`, `.ItemClass.OoT`, `.ItemClass.MM` — ADR 0003 naming, new tier-4 keys (never converged MM keys), each classified **identity** at introduction per ADR 0004 §6's scope note. Register them in `src/common/cvar_shared_keys.h` with the count asserts that file already carries.
2. The creation-time resolver reads them into the record in the freeze order of ADR 0011 §4.1, before OoT's `Fill()`; defaults must reproduce today's world exactly, so `SeedDeterminism` and `MMRandoGen` must not move.
3. The pane renders §6 **state 4** post-creation: read-only, reason string "already decided" (not the capability reason), values from `Combo_ComboSettingsSummary()`. Pre-creation it is editable. Enforcement stays on the writers.

## Owns

`src/common/foreign_items.h` / `.c`, `src/common/cvar_shared_keys.h`, the combo options view/window (new `src/common` view + `Combo*Window.cpp` pair, or an extension of `combo_mm_options_view.*` / `ComboMmOptionsWindow.cpp` — your call, recorded in the PR), the interim Cross-Game rows in `games/oot/soh/SohGui/SohMenuRandomizer.cpp:793-827`, and ADR 0011.

## Do not touch

`SohMenu.cpp` / `Menu.h` capability gating and the tier-4 Combo header are #497 steps 3/6 — **not** this lane; the Cross-Game sidebar remains the interim host. `context.h` needs no new carve (the record exists). ADR 0010 is read-only.

## Verify

Local ROM-staged build plus both ctest tiers. The golden-vector digest test from increment 1 must still pass with defaults; a re-pin means you changed a default, which this increment must not. `Fixes` nothing — #498 stays open for increments 3-4; reference it as `Refs #498`.
