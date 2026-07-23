# ADR 0008: Cross-game Gui windows register from `src/common` on the shared `Ship::Context` Gui, from a lifecycle entry point

- Status: **Accepted** (2026-07-22)
- For: #492 (Phase 3.1 tracker), Lane 5 — #496 step 5
- Depends on: the shared-Gui registration pattern PR #457 established for MM's
  tracker windows (`games/mm/2s2h/TrackersGuiSingleExe.cpp`)

This settles the seam for **every** future cross-game panel, not just #496's.
#458 (the combo tracker) needs the same answer and should not re-litigate it.

## Context

Both games register their own windows on one shared `Ship::Gui`: OoT does it
from `SohGui.cpp` during its boot, MM from `MM_TrackersGui_Init` via
`MM_Rando_Init`. That worked while every window belonged to a game.

#496 introduces the first window that belongs to **neither**. The cross-game
spoiler view reads only `gComboCtx` — it deliberately touches no
`gSaveContext`, which is what lets it render correctly under `GAME_OOT`,
`GAME_MM` and `GAME_NONE` alike. Hanging it off a game's boot would make a
game-neutral window's existence depend on which game booted:

- Register from **MM's** boot and the window does not exist until the player
  has crossed into MM at least once. The crossings it describes are placed
  MM-side, so this is the tempting choice — and it is exactly wrong, because
  the player who most needs the view is the one still in OoT wondering where
  their hookshot went.
- Register from **OoT's** boot and MM-only sessions never get it.
- Register from **both** and `Gui::AddGuiWindow` silently rejects the second
  attempt (SPDLOG_ERROR and return, `libultraship/src/ship/window/gui/Gui.cpp`),
  so the duplicate is invisible until someone reads the log.

`src/common` had no wired Gui element at all before this. `ComboMenuBar` is
compiled but nothing outside its own TU references it, so there was no
precedent to copy — only a gap.

## Decision

**1. Cross-game Gui windows are owned by `src/common` and registered from a
`src/common` entry point, not from either game's boot.**

A window whose data source is `gComboCtx` registers itself; a window whose data
source is a game's `gSaveContext` stays with that game. That is the ownership
test, and it is a test about the *data*, not about the pixels: MM's check
tracker reads MM's save through MM's layout and therefore stays MM's, even
though it is displayed in the same combo binary.

**2. The registration point is the shared `Ship::Context` Gui, under a
distinct, unprefixed-by-a-game name.** SoH owns the four unprefixed tracker
names and MM owns the `MM `-prefixed four; common-owned windows take names in
neither space. `AddGuiWindow`'s duplicate rejection is silent from the caller's
side, so name collisions are a registration-time bug that surfaces only as a
missing window — every registrar must keep its names disjoint.

**3. Registration is idempotent and tolerates being called before, after, or
instead of either game's boot.** The concrete guard is the #457 one: check
`GetGuiWindow(name) != nullptr` and return.

**4. Registration must no-op cleanly when the shared context has no window.**
Every ROM-free harness runs in that state, and it is the reason
`MM_TrackersGui_Init` already returns early on a null `GetWindow()`/`GetGui()`.

**5. Common-owned windows do not read `gSaveContext` at all.** This is what
makes them safe to draw under any active game, and it is why they need no
equivalent of MM's `MMActiveGated` wrapper. A future common-owned window that
*does* need per-game data must gate like MM's do rather than relax this.

## Consequences

- The cross-game spoiler view (#496) is reachable in an OoT-only session, which
  is the session where it matters most.
- #458's combo tracker inherits this seam rather than inventing a second one.
  Its slice that reads the *inactive game's frozen shadow* is still
  common-owned by rule 1; its per-game check adapters are not.
- `Ship::GuiWindow` latches its visibility CVar in the constructor and nothing
  re-syncs CVar → visibility per frame
  (`libultraship/src/ship/window/gui/GuiWindow.cpp`). A common-owned window
  therefore needs its own openability route, exactly as MM's trackers do —
  either a live CVar read in `Draw()` or an explicit sync. Registration alone
  does not make a window reachable, and #489 is the bug report proving it.
- The three-edit CTest registration rule applies: a common-owned window's
  headless lock (registration, idempotence, and `Draw()`/`Update()` under all
  three `GameId`s with no ImGui context) belongs in the display-free `redship`
  tier.

## Alternatives rejected

**A `SohMenu` `WIDGET_WINDOW_BUTTON` row as the registration seam.** That is
OoT's menu machinery (`games/oot/soh/SohGui/Menu.cpp`); routing a game-neutral
window through it re-creates the OoT-boot dependency this ADR exists to avoid,
and puts a common-owned concern in a game-owned file. It remains a fine way to
*open* such a window once registered — the two questions are separate.

**A registry both games push into at boot.** More machinery than the problem
has: with `AddGuiWindow` already keyed by name and already idempotent-checkable,
a second registry would be a second source of truth about what is registered.
