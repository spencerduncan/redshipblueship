# ADR 0004: Menu information architecture — one shell, four tiers, capability-gated MM entries

- Status: **Proposed** (2026-07-21)
- For: #392 (Phase 3.0 tracker), #34 (settings migration)
- Depends on:
  - **[ADR 0003](0003-settings-namespace.md)** (settings namespace) — owns CVar key naming and
    the rename/migration rule; this ADR consumes its decisions rather than restating them.
    **This ADR resolves ADR 0003 §4.2** — see §2d below
  - **#446** — the `Ship::Menu` / `WidgetInfo` ODR split; gates any MM menu-code port
  - **#438** — missing MM hook dispatch placements; gates real MM functionality
  - `docs/unified-surface-findings.md` — the investigation this plan acts on
- Companion data: **[`docs/enhancement-classification.md`](../enhancement-classification.md)** —
  the per-setting classification table. That document is normative for *which* controls are
  shared; this one is normative for *where they go and when they may appear enabled*.

## Context

Four asks — a unified options menu, MM randomizer settings, both-game trackers, MM
enhancements — all land on the same surface: the menu. `docs/unified-surface-findings.md`
established where we are:

- The **only live menu** is SoH's `SohMenu`, registered on the shared libultraship `Gui` at
  OoT init. Esc/F1 handling is game-agnostic LUS code, so **OoT's menu is what you get while
  MM is running**, and every section in it is OoT-only.
- `ComboMenuBar` is dead code (compiled, never instantiated, F1 is a no-op). #320 was closed
  premise-incorrect for exactly this reason.
- MM's shell (`BenMenuBar` + `BenMenu` + tracker windows) is entirely excluded; instantiation
  is stranded in `BenGui.cpp`, whose only caller is the excluded `BenPort.cpp`.
- Of ~195 compiled MM enhancement TUs, **~190 are link-elided**. Registration is not dispatch:
  ~650 MM hook sites register safely, but a hook type stays dormant until an `Execute` call is
  deliberately placed.

The shell question is settled: **extend the live `SohMenu`.** Do not port MM's BenMenu shell.
Do not revive `ComboMenuBar` — its one working piece, the `.redsave` file-select panel, moves
into the new Combo section rather than being rebuilt.

## Decision

### 1. The governing principle

> "In general, this is conceptually one game. If something applying to both makes sense, it should."

The **default is UNIFIED**. A setting that means the same thing in both games is ONE control
driving both. Per-game controls are for things that genuinely differ — and each such case must
say why, in the classification table, not in the menu code.

This inverts the obvious-but-wrong layout. The naive IA is "OoT section / MM section", which
is easy to build and wrong: it would give the player two text-speed sliders, two low-health
beep toggles, and two Infinite Health checkboxes for what they experience as one game.

### 2. Settled inputs from the maintainer

Recorded here so downstream work does not relitigate them.

**2a. Shared-intent mechanism is ONE shared CVar** read by both games. No UI fan-out layer to
two per-game keys. ADR 0003 §2.2 owns key naming; this ADR assumes a single key.

> **Accepted trade — no per-game override.** Under one shared CVar, "Infinite Health in MM but
> not OoT" is **not expressible**. This is a deliberate consequence of the governing principle,
> not a defect, and should not later be filed as a bug. If per-game overrides are ever wanted,
> that is a new decision requiring a fan-out layer, and it reopens this ADR.

**2b. Cheats are class (S) shared-intent.** One "Infinite Health" control drives both games.
The existing `gCheats.{InfiniteHealth, InfiniteMagic, MoonJumpOnL, NoClip, EasyFrameAdvance}`
key sharing is **deliberate and correct** — verified per-key in the classification table §3.2 —
and is not part of the divergence bug list.

**2c. Convergence direction is MM → OoT keys**, where a shared-intent setting currently has
different keys on each side. "Where we should" is a scope limit: only (S) rows converge; (O)
rows keep their own keys; (P) rows are disambiguated, never merged. The classification table
§5.3 supplies the 26-row rename list; ADR 0003 §5 owns the rename and migration rule.

### 2d. Relationship to ADR 0003, including where the two differ

ADR 0003 and this ADR were drafted in parallel from the same three maintainer decisions and
measured the collision set independently. They agree on the principle, the mechanism, the
direction, and — importantly — **both independently found the same two divergence bugs**
(the tunic desync and the `DebugSaveFileMode` defaults). Where they differ, this section is
the record; neither document silently overrides the other.

**This ADR resolves ADR 0003 §4.2.** That section classifies four keys —
`gSettings.Menu.ActiveHeader` and the three `…SidebarSection` keys — as class (P) bugs,
because a section index or header name written by one menu is meaningless to the other. It
explicitly defers disposition to the menu end-state decision, and states: *"if MM's entries
are added into `SohMenu` instead, they become genuine class (S) automatically because there
is then only one menu."*

**This ADR makes that choice: one shell, MM entries added into `SohMenu`.** ADR 0003's four
(P) keys therefore become class (S), and its warning — *"do not let MM's menu un-elide before
resolving it"* — is discharged, provided MM's menu is never revived as a second shell. That
proviso is now load-bearing for both documents, which is the second reason §3 tier 1 flags the
sidebar-name caveat.

**One live disagreement, deliberately preserved.** On `gDeveloperTools.DebugSaveFileMode`,
ADR 0003 §4.1 classifies (S) — value spaces align, only the unwritten fallback differs,
"acceptable, worth knowing". This document classifies it **(P)** and files it as
[#454](https://github.com/spencerduncan/redshipblueship/issues/454). The disagreement is real
and narrow: 0003 is right that once the key is written the games agree, and this ADR is
taking the more conservative line that two different defaults on one shared key is a
behaviour decision someone should make rather than inherit. **The rename pass is safe under
either reading** — the key name already matches, so nothing renames. Whoever closes #454
should update whichever document ends up wrong.

### 3. The four tiers

Every setting in the combined product falls into exactly one.

#### Tier 1 — port-level (graphics / audio / controls): **already shared today, by construction**

This is the reassuring part, and it is verified, not assumed:

- One SDL window, one GL context, one renderer, one audio device, one input stack. OoT creates
  the `Ship::Context`; MM reuses it. There is no second window.
- LUS's own settings CVars are defined **once** in `CMake/lus-cvars.cmake` and compiled into
  both games: `gSettings.VsyncEnabled`, `MSAAValue`, `InternalResolution`, `TextureFilter`,
  `LowResMode`, `SimulatedInputLag`, `ZFightingMode`, `ControlNav`, `EnableMultiViewports`,
  `SdlWindowedFullscreen`, `OverlayFont`, and the `Controllers` / `AdvancedResolution` prefixes.
  MM's menu references `CVAR_LOW_RES_MODE` and `CVAR_SIMULATED_INPUT_LAG` through the *same*
  macros OoT uses.
- The `CVAR_PREFIX_*` strings themselves come from `CMake/soh-cvars.cmake` via
  `add_compile_definitions` — a single global set. MM hardcodes byte-identical literals.

**Consequence: nothing needs to be done to unify tier 1.** Resolution, MSAA, texture filtering,
vsync, master volume, controller mapping are already one setting each. The menu should say so
by presenting them once, under Settings, with no per-game qualification.

The one caveat: `gSettings.Menu.{Settings,Enhancements,DevTools}SidebarSection` store the
selected sidebar **by display-name string**. That is correct under this ADR's single-shell
decision and would silently break under a two-shell design — one more reason to reject porting
BenMenu.

#### Tier 2 — shared-intent gameplay: ONE control, both games

The classification table's (S) class. 59 rows identified: 33 already read the same key, 26
need MM renamed onto the OoT key. Examples: low-health beep, enemy proximity music, actor draw
distance, widescreen culling, fast chests, autosave, D-pad equips, cutscene skips, the cheat
block.

#### Tier 3 — per-game

Two sub-classes, and the menu must not blur them:

- **(P) per-game parallel** — exists in both, semantics/units differ enough that one control
  would be wrong. 7 rows; each documented with a reason. Notably **text speed**: OoT is a 1–5×
  int slider, MM is a bool that additionally bundles OoT's separate `SkipText`. One control
  cannot express both.
- **(O) only-in-one** — MM cycle/mask/song/Bombers/minigames; OoT dungeon/trade/age/rando.
  ~146 MM-only and ~328 OoT-only in-scope keys.

#### Tier 4 — combo-only

New surface with no upstream counterpart: pairing status, `.redsave` slot management (the panel
inherited from `ComboMenuBar`), entrance links, F10 hot-swap behaviour, shared-seed / shared-hash
display.

### 4. Proposed section layout

Top-level sections on the extended `SohMenu`. Within Enhancements / Cheats / Cosmetics, the
ordering rule is **shared-intent entries first, then per-game subsections** — so the unified
controls are what a player sees by default and the per-game ones read as exceptions.

| Section | Sidebars | Notes |
|---|---|---|
| **Settings** | General, Audio, Graphics, Controls, Input Viewer, Notifications, Mod Menu | Tier 1. **Relabel to state these apply to both games.** No per-game split — there is nothing to split. |
| **Enhancements** | Quality of Life, Skips & Speed-ups, Graphics, Items, Fixes, Difficulty, Minigames, Extra Modes | Shared-intent entries first in each sidebar; then `— Ocarina of Time —` and `— Majora's Mask —` separators for tier-3 entries |
| **Cheats** | (promoted out of Enhancements) | Shared-intent block first (the 5 matched + 5 converged cheats), then per-game |
| **Cosmetics** | Cosmetics Editor, Audio Editor, HUD Editor | Mostly (O) per game; MM's 3 tunic keys converge — see classification §3.3 BUG 1 |
| **Randomizer** | OoT, MM, **Paired** | See §4.1 |
| **Trackers** | Item, Check, Entrance, Combo | Per findings §3, MM trackers are nearly free — blocked on registration surface + selective un-elision, not hook migration |
| **Combo** | Pairing status, Save slots, Entrance links, Hot-swap | Tier 4. Absorbs `ComboMenuBar`'s working `.redsave` file-select panel |
| **Dev Tools** | (existing) | Shared where already shared; MM's viewers gated per §5 |
| **Network** | (existing) | Out of scope for this ADR |

#### 4.1 Randomizer: OoT / MM / Paired

The storage models genuinely differ and the pane must bridge them, not draw two lists:

- **OoT** rando settings are CVar-backed (`gRandoSettings.*`, 232 keys), drawn by
  `SohMenuRandomizer.cpp` with a `Generate Randomizer` button.
- **MM** rando options are CVar-backed **at authoring time** (`gRando.Options.<RO_ID>`, 47
  options) and **snapshotted into the save at generation** — `Rando/Menu.cpp` copies each CVar
  into `RANDO_SAVE_OPTIONS`, and all gameplay code reads the save array, never the CVar. This
  refines the findings doc's "stored in the save": it is both, with the save as runtime truth.

Lane B's accepted recommendation on #392 stands: **keep the minimal SohMenu path for 3.0.** The
paired world is opted into by generating on the OoT side; MM's half derives from
`sharedRandoSeed` + `sharedRandoSettingsHash`. The cheapest honest increment is a **read-only
"Paired" summary + MM logic-mode picker** inside the existing OoT rando pane — no MM menu port
required.

#### 4.2 Shared-intent entries must be visibly marked (required, not cosmetic)

Because a shared-intent control toggled while playing OoT also changes MM — a game the player
cannot currently see — **every tier-2 entry must carry an explicit "applies to both games"
affordance.** Without it, a player who toggles Infinite Health in OoT and later finds it on in
MM will read correct behaviour as a bug.

This is a hard requirement on the widget, not a tooltip nicety. The minimum is a persistent
marker in the row itself (icon or badge), legible without hovering. A tooltip alone does not
satisfy it.

### 5. The capability-gating rule (non-negotiable)

**An MM entry may only appear enabled when all three hold:**

1. its **TU links** (it is not link-elided), **and**
2. its **registrar runs** (`S2H::ShipInit::InitAll()` reaches it), **and**
3. its **hook type has a dispatch placed** (an `Execute` call exists at MM's upstream dispatch
   point — see #438).

Anything not yet live **appears disabled, with a reason string** — never functional-looking.
Reuse the existing `disabledMap` mechanism, which already pairs a predicate with an explanation
("Disabling VSync not supported", "Save Not Loaded"). MM entries get entries of the same shape:
"Not yet available: MM dispatch not placed (#438)".

**Why this is non-negotiable.** A control that flips a CVar and changes nothing is the
vacuous-gate class in UI form: it reports success, satisfies a check, and does nothing. Of ~195
MM enhancement TUs, ~190 are currently elided — so a naive port of MM's 295 menu widgets would
produce a menu that is ~97% lies. #320's grayed-stub history is the precedent; the lesson is
that a disabled control with a reason is honest and a live control that does nothing is not.

Note the fan-out hazard: 25 TUs share `gEnhancements.Cutscenes.SkipMiscInteractions` and 28
share `…SkipStoryCutscenes`. Such a control is only as live as the *least* linked of its TUs,
so gating must be computed over the whole set, not a representative file.

### 6. Active-game affordance

Both games' settings are visible while one is suspended. The UI must communicate which apply
now:

1. **Tier 1 and tier 2 entries** apply to both games always — marked per §4.2.
2. **Tier 3 entries** apply to one game. The inactive game's subsection must be visually
   de-emphasised and labelled with its state (e.g. "Majora's Mask — suspended"), while
   remaining *readable and editable* (a player should be able to set MM options before
   switching).
3. **Editable-but-not-active is a third state**, distinct from both "live" and "disabled by
   capability gating" (§5). Collapsing it into "disabled" would wrongly imply the setting is
   broken; collapsing it into "live" would wrongly imply immediate effect. Three states, three
   presentations.

## Dependencies and sequencing

Ordered by what unblocks what. Nothing in this ADR is implementable before its gate clears.

| # | Gate | Blocks | Status |
|---|---|---|---|
| 1 | **[ADR 0003](0003-settings-namespace.md)** — settings namespace | All key naming; the 26-row rename list | Proposed; scheduled as #34 |
| 2 | **#446** — `Ship::Menu` / `WidgetInfo` ODR split | *Any* MM menu-code port | Agent landing now |
| 3 | **#438** — MM hook dispatch placements (~22) | Real MM functionality behind any MM entry | Open |
| 4 | `2ship_enh` migration (~56 raw sites) + WHOLE_ARCHIVE + guard flip | MM enhancements linking at all | Part of #427 |
| 5 | **#442** — `SavingEnhancements` OOB write | Should land first regardless; hours | Open, live hazard |

On **#446**: both ports define `class Ship::Menu` and global-scope `WidgetInfo` with divergent
layouts (OoT `std::map` vs MM `std::unordered_map`; OoT's `WidgetInfo` has a `raceDisable`
field MM's lacks) under **identical include guards**. MM's implementation is excluded today, so
a resurrected MM menu TU's `AddMenuEntry` would silently cross-bind to OoT's compiled
implementation against MM's layout — the #383/FlagTable class. Since this ADR extends SohMenu
rather than porting BenMenu, the exposure is smaller than it would otherwise be, but any reuse
of MM menu code at all must apply the #434 recipe first.

**Sequencing note:** the layout in §4 can be built incrementally *without* gates 3–4, because
capability gating (§5) makes an un-dispatched MM entry a legitimate disabled row. The IA can
land ahead of the functionality it will eventually expose. That is the main practical benefit
of adopting the gating rule up front rather than retrofitting it.

## What this ADR does not decide

Genuinely open, and deliberately left to the maintainer:

1. **Cheats section placement.** §4 promotes Cheats to a top-level section (it is currently an
   Enhancements sidebar in both games). Cosmetic; flag if unwanted.
2. **(P) row P2 — ISG.** OoT files `gCheats.EasyISG` as a **cheat**; MM files
   `gEnhancements.Restorations.TatlISG` as a **restoration**. Same underlying behaviour, opposite
   framing. Which section wins is a taxonomy call, not a technical one, and the rename pass must
   not merge these until it is made.
3. **(P) row P5 — `gDeveloperTools.DebugSaveFileMode` defaults.** The games share the key but
   disagree on the default (OoT 1 "Vanilla", MM 0 "Empty"). Reconciling requires picking one
   behaviour; it is not a rename. **ADR 0003 §4.1 reaches the opposite conclusion** and calls
   this acceptable — see §2d and #454. Deciding it settles which document is right.
4. **Group B rename confirmations.** Two rows need a semantic check before renaming:
   `InfiniteAmmo`/`InfiniteConsumables` (MM's scope may be wider) and
   `NoRestrictItems`/`UnrestrictedItems` (OoT drops age gating, MM drops form gating).
5. **`TimeSavers` vs `Timesavers` casing.** OoT capitalises the `S`, MM does not. One spelling
   must win; the namespace ADR should state which.

> Two questions that *were* open when this work started are now settled and recorded in §2:
> shared-intent uses one shared CVar (not a fan-out), and cheats are shared-intent. They are
> listed here only so the decision trail is visible.

## Consequences

**Good:**

- Tier 1 needs no work — the reassuring finding. A large fraction of what a player would expect
  to have to unify is already unified by construction.
- The IA can land before the MM functionality it will expose, because capability gating makes
  "not yet live" a first-class, honest state.
- The classification table turns the vague "unify the menu" ask into a 26-row mechanical rename
  plus 7 rows needing judgement.

**Costs, accepted:**

- **No per-game override** for shared-intent settings (§2a).
- `shipofharkinian.json` becomes **permanently mixed-convention**: flat keys for shared
  settings (OoT's style, per §2c) and category-nested keys for MM-only settings, which keep
  their names. A consequence of the scope limit, not an oversight.
- Renaming MM's keys breaks verbatim upstream diffs against 2Ship for every touched file. This
  is the cost the namespace ADR weighs; it is real and it is recurring.

**Risk:**

- The rename list is generated from the classification table. A misclassification there becomes
  a wrong rename here — which is why every (S) row was verified by reading both
  implementations, and why the 7 (P) rows are called out explicitly as do-not-merge.
