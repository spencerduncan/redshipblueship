# ADR 0004: Menu information architecture — one shell, four tiers, capability-gated MM entries

- Status: **Accepted** (2026-07-23, #497 step 1); **§6 and §4.1a amended
  2026-07-30** under the one-game-semantics ruling; **one §4.1a consequence
  superseded 2026-07-31** by ADR 0010
- For: #392 (Phase 3.0 tracker), #34 (settings migration), #497, #499
- Amended on acceptance: §4.1 (scope and host of the MM randomizer pane — see
  §4.1a), §2d (the #454 disagreement, now ruled), and "What this ADR does not
  decide" (all five calls resolved).
- Amended **2026-07-30, #564** (the one-game ruling is recorded on
  [#500](https://github.com/spencerduncan/redshipblueship/issues/500#issuecomment-5126492334)):
  §6 gains a **fourth presentation state** — frozen-at-creation, read-only, with
  the reason — and its "a player should be able to set MM options before
  switching" clause is superseded; §4.1a's timing is restated as *reachable
  before the combo file is created*, which strengthens rather than disturbs its
  common-owned-window conclusion. The tier model, the capability-gating rule and
  the section layout are untouched.
- Superseded in part **2026-07-31**, by
  [ADR 0010](0010-cross-game-logic-and-beatability.md) (Accepted 2026-07-31,
  [PR #572](https://github.com/spencerduncan/redshipblueship/pull/572)):
  §4.1a's "no raised profile ships" consequence — the paired `RO_LOGIC` default
  is raised from Nearly No Logic to Glitchless by ADR 0010 increment 1, with
  its deterministic attempt ladder answering the dead-end-rate concern §4.1a
  recorded. The explicit-choice-overrides rule and every other §4.1a conclusion
  stand. Annotated here separately because PR #572's stated scope excluded
  amending this document.
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

**~~One live disagreement, deliberately preserved.~~ RULED (P) on acceptance, 2026-07-23.** On
`gDeveloperTools.DebugSaveFileMode`, ADR 0003 §4.1 classified (S) — value spaces align, only the
unwritten fallback differs, "acceptable, worth knowing" — and this document classified it **(P)**,
filed as [#454](https://github.com/spencerduncan/redshipblueship/issues/454).

**#454 never decided it.** It was auto-closed by the squash-merge of PR #456, a docs-only change
whose own text says #454 is the thing that will settle the question. The close is an artifact.

**This ADR's (P) reading is upheld, on evidence neither document had.** The argument ADR 0003 §4.1
rests on — "once written the shared value governs both; only the *unwritten* fallback differs" — is
false on its own terms, because MM does not merely read the key with a different default. It
*writes* one:

```c
// games/mm/2s2h/DeveloperTools/DeveloperTools.cpp, RegisterDebugMode()
if (!CVAR_DEBUG_MODE) {
    CVarSetInteger(CVAR_SAVE_FILE_MODE_NAME, DEBUG_SAVE_INFO_NONE);
```

MM's registrar unconditionally writes `0` into the shared cell whenever debug mode is off, which is
the default state — so the key is never left unwritten once MM's devtools link, and OoT's `1` is
destroyed on every launch. **ADR 0003 §4.1's row is therefore wrong and is corrected there.**

Dormant but armed: `games/mm/CMakeLists.txt` excludes `2s2h/DeveloperTools/` from the single-exe
build, so the clobber does not run today. It arms on the same un-elision work §5's gate 4
schedules. The rename pass remains safe under either reading — the key name already matches — so
this is a behaviour fix (retire MM's write, align MM's read default to OoT's `1`) owed by whichever
lane un-elides `DeveloperTools`, not a namespace change. `kDisputedClassificationKeys` keeps the key
until that fix lands, because the manifest must not claim (S) while the source still contains the
write that makes it (P).

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
| **Cheats** | ~~(promoted out of Enhancements)~~ **stays an Enhancements sidebar** | Superseded on acceptance — see resolved call 1. Shared-intent block first (the 5 matched + 5 converged cheats), then per-game |
| **Cosmetics** | Cosmetics Editor, Audio Editor, HUD Editor | Mostly (O) per game; MM's 3 tunic keys converge — see classification §3.3 BUG 1 |
| **Randomizer** | OoT, MM, **Paired** | See §4.1 and **§4.1a** — MM's half is a common-owned window, not a SohMenu sidebar, for the timing reason given there |
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

#### 4.1a Superseded on acceptance: the FULL option set, in a common-owned window

**Decided 2026-07-23 (#497's open scope question, maintainer call). §4.1's minimum above is
recorded as the 3.0 position and is superseded for 3.1.** Two changes:

**(i) Scope: the full option set, not a logic-mode picker.** All 47 `RandoOptionId`s are exposed.
The minimum was the right 3.0 increment when the alternative was porting BenMenu; it is the wrong
3.1 one, because #499 established that *nothing in the shipped link ever writes a
`gRando.Options.*` CVar* — so a logic-mode picker would leave 46 options still unreachable and
still silently defaulted, which is the same defect with one fewer instance.

**(ii) Host: a common-owned `Ship::Gui` window (ADR 0008), not a pane inside `SohMenu`.** This is
the one place this ADR's "one shell, extend SohMenu" rule does not apply, and the reason is
timing rather than taste. The paired MM profile is snapshotted when MM's cross-game arrival
dispatches `OnSaveInit`, and an existing MM save is never regenerated (#499). The chooser must
therefore be reachable **while OoT is the running game, before the switch** — which is exactly
the property ADR 0008 rule 1 buys by owning a window in `src/common` rather than hanging it off
either game's boot.

The "one shell" rule is unweakened by this. ADR 0008 already distinguishes *registering* a
game-neutral window from *opening* it, and explicitly leaves a `SohMenu` `WIDGET_WINDOW_BUTTON`
row as a fine way to do the opening. What §3's rule forbids is a **second menu shell**; a
common-owned window is not one, and in particular it reads no `gSettings.Menu.*` key, so #451's
arming condition is untouched (mechanized as the MM-side reader allowlist in
`src/common/tests/test_cvar_classification.c`).

**Consequence accepted: §4.2's shared-intent marker does not apply to this pane.** All 47 options
are tier-3 (O) MM-only keys; none is in `kSharedIntentKeys`, so there is no "applies to both
games" claim to mark. §6's presentation states DO apply, all four: live, editable-but-not-active
("Majora's Mask — suspended"), disabled-by-capability with a reason, and — implemented with the
#564-V5 freeze work (#498 phase 2 step 9) — frozen-at-creation, read-only: once a creation event
stamps `gComboCtx.mmProfileDigest`, the two `src/common` option writers reject and the pane
renders the frozen banner with every row disabled. The predicate is `Combo_MMProfileFrozen()`,
the `src/common` fact §6 prescribes.

**Residue, named rather than implied: state 4 is implemented as the gate and the presentation, not
yet as the frozen VALUES.** §6 also requires that where a frozen key's value is shown, it be the
value *from the save* rather than the CVar. This pane still renders live CVar reads. That is
correct-but-fragile today — the two `src/common` writers are the only in-app writers and they
reject while frozen, so the two agree — and it is genuinely unimplementable under the interim
hybrid: the hybrid persists only the digest, so there is no frozen profile in `src/common`-owned
storage to publish (§6's own "publishing the frozen profile into a `src/common` view is part of the
freeze work" is the ask, and #564's carve budget shows a 47-option Tier-1 record does not fit).
An out-of-band write the writers never see — a hand-edited config, libultraship's console
`cvar_set` — therefore makes the pane display a value the world was not built from while labelled
"frozen at creation". It is caught (the next arrival refuses on the digest) but it is displayed
wrongly until then. Closing this is part of #564 step 11's move to delivery option 1, where the
frozen profile lives in MM's Tier-3 `RANDO_SAVE_OPTIONS` and reaches the pane through the same
`src/common` accessor the digest already uses.

> **Amended 2026-07-30 (#564): the host conclusion survives and gets stronger; the deadline is
> restated.** "Reachable while OoT is the running game, before the switch" was the right host
> argument off the wrong deadline. Under one-game semantics the MM option profile freezes at the
> **creation event** — OoT's file-create — not at the crossing, so the requirement restates as
> **reachable before the combo file is created**: a strictly earlier window, and one in which OoT
> is equally the running game. A `SohMenu` pane satisfies neither deadline, so ADR 0008's
> common-owned window is confirmed rather than disturbed, and the precedent to copy for the gate
> is OoT's own Generate button (file-select only, no save loaded — `SohMenuRandomizer.cpp`).
> What changes is the pane's contract *after* that window closes: see §6 state 4. The shipped
> copy that teaches the retired deadline — "These apply to the NEXT Majora's Mask file… set them
> before you cross" (`ComboMmOptionsWindow.cpp:47-49`) — is superseded with it, and its
> replacement must not describe a renegotiation window that no longer exists.

**Consequence accepted: the options are a CHOICE, and the defaults do not change.** No raised
profile ships. Every `RO_SHUFFLE_*` and `RO_HINTS_*` row keeps its `RO_GENERIC_OFF` default, so
generation dead-end rates are unchanged for anyone who does not touch the pane — which is the
honest answer to #499's "measure dead-end rates before shipping a raised profile", rather than
measuring a profile nobody chose. The one previously-hardcoded value, the paired `RO_LOGIC` pin
to Nearly No Logic (#426), becomes a default that an explicit choice overrides.

> **Superseded on a single point (2026-07-31):** [ADR 0010](0010-cross-game-logic-and-beatability.md)
> — **Accepted** 2026-07-31, [PR #572](https://github.com/spencerduncan/redshipblueship/pull/572) —
> resolves the forward pointer previously recorded here. Its increment 1 raises this one default:
> the paired `RO_LOGIC` default becomes **Glitchless**, not Nearly No Logic, at its single
> resolution point (`Rando/Foreign.cpp:126-128`), with a deterministic re-roll attempt ladder
> answering the dead-end-rate concern recorded above rather than ignoring it (implementation in
> flight). This paragraph's "defaults do not change" and "no raised profile ships" are superseded
> on that single point only — every `RO_SHUFFLE_*` and `RO_HINTS_*` row keeps its
> `RO_GENERIC_OFF` default, and the explicit-choice-overrides rule is unchanged: an explicit
> player choice of a no-logic mode is still honored and recorded in the frozen identity.

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
   remaining *readable and editable* ~~(a player should be able to set MM options before
   switching)~~ **for as long as they are still authorable at all — which, for any key that
   is world identity, ends at the creation event. See state 4.**
3. **Editable-but-not-active is a third state**, distinct from both "live" and "disabled by
   capability gating" (§5). Collapsing it into "disabled" would wrongly imply the setting is
   broken; collapsing it into "live" would wrongly imply immediate effect.
4. **Frozen-at-creation is a fourth state (added 2026-07-30, #564): read-only, labelled with
   the reason and with the identity it is frozen to.** A world-identity key belonging to a
   world that already exists is not editable, not suspended, and not broken — it was decided,
   once, and the decision is part of the save. Rendering it live is the worst of the four
   errors available here: the control accepts input, reports success, and changes nothing
   about the world the player is in (§5's vacuous-gate class, in its most convincing form,
   because this control *used* to work).

Four states, four presentations. The distinctions are what each one denies:

| State | Applies now? | Editable? | What the presentation must deny |
|---|---|---|---|
| Live | yes | yes | — |
| Editable-but-not-active | on resume | yes | "this takes effect now" |
| Disabled by capability (§5) | no | no | "this works" |
| Frozen at creation (#564) | it already did | **no** | "this is still a choice" |

A frozen entry's reason string is not optional and is not the capability reason: a capability
gate says *not yet available*, a freeze says *already decided*, and a player who reads the wrong
one goes looking for a bug in the right one. Where a frozen key's value is shown at all, show the
value from the save rather than the CVar — after creation the two may legitimately differ, and
the save is the one the world was built from. **"From the save" names the authority, not the read
site.** The host of every one of these keys is a common-owned window, which may not read
`gSaveContext` at all (ADR 0008 rule 5, restated below), and [ADR 0009](0009-combo-settings-and-reverse-pool.md)
decision 1's 2026-07-30 amendment puts the frozen MM profile in MM's own SaveContext
(`RANDO_SAVE_OPTIONS`, Tier-3) — so a pane that rendered it directly would break the rule in the
same breath as obeying this one. The frozen value reaches the pane the way the digest already
does: through a `src/common` accessor over `src/common`-owned storage
(`Combo_MMProfileSummary`, `combo_mm_options_view.c:155`, reads `gComboCtx` and nothing else).
Publishing the frozen profile into a `src/common` view is therefore part of the freeze work, not
an afterthought of the presentation.

> **Why the fourth state exists (2026-07-30, ruling on #500, alignment plan #564).** *"This is
> one game from a semantic standpoint"*: the paired OoT+MM world has ONE identity, fixed at ONE
> creation event, and arrival-time divergence from it is corruption to detect and refuse rather
> than a choice to honour. §6 as written assumed the opposite — that a suspended game's settings
> stay negotiable until the player switches into it — which was true only because MM's half was
> generated late, at first crossing. Once identity freezes at creation, a live editor over
> world-identity keys is not a convenience; it is the mechanism by which a player silently
> desynchronises the two halves of their own save.
>
> **Scope.** State 4 applies to a key when a world built from it exists — that is, to tier-3 and
> tier-4 **world-identity** keys after creation. It does NOT apply to preference keys, which have
> no identity role and stay live forever (Autosave and RememberSaveLocation are the named
> examples; #539's missing MM driver is about those, not these). Classifying each key into one
> bucket or the other is owed by the same work that ships the freeze — an unclassified key
> defaults to identity, because guessing "preference" for an identity key is the failure this
> state exists to prevent.
>
> **Enforcement is not the widget.** A read-only rendering that leaves the underlying writers
> open is decorative: the pane is one caller of the `src/common` write choke points, and the gate
> belongs on those (#564 V5). The predicate must be a `src/common` fact — the creation-stamped
> `mmProfileDigest != 0` is what #564 prescribes — never a `gSaveContext` read, per ADR 0008 rule
> 5 and the pane's game-agnosticism tripwire.

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

## What this ADR left open — all five resolved on acceptance (2026-07-23, #497 step 1)

Each was measured against the tree rather than argued from the documents. The original wording is
kept so the decision trail stays legible.

1. **Cheats section placement.** *Was:* §4 promotes Cheats to a top-level section (currently an
   Enhancements sidebar in both games); cosmetic, flag if unwanted.
   **RESOLVED: do NOT promote. Cheats stays an Enhancements sidebar.** Both upstreams already
   agree on that placement (`SohMenuEnhancements.cpp` `AddSidebarEntry("Enhancements", "Cheats", 3)`;
   `BenMenu.cpp` the same), and sidebar selection persists **by display-name string** into
   `gSettings.Menu.EnhancementsSidebarSection`. Promotion is purely cosmetic yet strands every
   config holding `EnhancementsSidebarSection == "Cheats"` and mints a fifth menu-index key while
   #451 is still open on the four that exist. §4's table row is superseded by this.
2. **(P) row P2 — ISG.** *Was:* OoT files `gCheats.EasyISG` as a cheat, MM files
   `gEnhancements.Restorations.TatlISG` as a restoration; which section wins is a taxonomy call.
   **RESOLVED: keep both keys distinct; file ISG under Cheats, and surface MM's as its own row
   labelled "Tatl ISG (restoration)".** The behaviour matches but the *gate* does not — OoT's is an
   unconditional passive grant, MM's restores a behaviour the N64 game had. One merged control
   would silently switch a restoration on for players who asked for neither. Stays in
   `kMustStayDistinct`.
3. **(P) row P5 — `gDeveloperTools.DebugSaveFileMode` defaults.**
   **RESOLVED (P), against ADR 0003 §4.1 — see §2d for the evidence.** Not on the default
   divergence: on MM's `DeveloperTools.cpp` write into the shared cell, which falsifies 0003's
   "only the unwritten fallback differs" outright. The fix is a behaviour change owed by whichever
   lane un-elides `2s2h/DeveloperTools/`; until then the key stays in
   `kDisputedClassificationKeys`.
4. **Group B rename confirmations.** *Was:* `InfiniteAmmo`/`InfiniteConsumables` and
   `NoRestrictItems`/`UnrestrictedItems` need a semantic check before renaming.
   **RESOLVED: neither converges. Both stay in `kMustStayDistinct`, and the recorded reasons are
   sharpened** — the parked entries were directionally right and understated the divergence.
   - *Ammo/Consumables:* OoT's refill is unconditional; MM's gates every item on `INV_CONTENT`, has
     no slingshot row, uses `CUR_CAPACITY(UPG_BOMB_BAG)` where OoT hardcodes 50, and additionally
     covers **Magic Beans and Powder Kegs** — consumables that are not ammo. MM's scope is genuinely
     wider, confirming the suspicion this row was filed on.
   - *NoRestrictItems/UnrestrictedItems:* OoT zeroes the interface restrictions bitfield every
     frame **with a hardcoded Sun's Song carve-out**; MM is a single `VB_ITEM_BE_RESTRICTED` veto
     with none. Different mechanism, different scope, and the carve-out is the concrete thing a
     merge would lose.
5. **`TimeSavers` vs `Timesavers` casing.**
   **RESOLVED: `TimeSavers` (capital S) is canonical.** Counted in the tree: 148:1 in `games/oot`,
   and five MM keys have already converged onto it. MM's lowercase set is 8 keys, the cheaper side
   to move. ADR 0003 §5 now states this. Mechanically it is a `ConfigVersion9Updater` group plus a
   `kRetiredKeyPrefixes` entry — and the same commit must delete `kParkedCasingPrefixMM` and its
   assertion, which *requires* the lowercase prefix to still exist and goes red as the last key
   moves. Not done here: it is a rename pass in files this lane does not own.

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
- **Added 2026-07-30 (#564):** every world-identity key now needs a classification the
  enhancement table does not currently carry — identity versus preference (§6 state 4). An
  identity key misfiled as a preference stays editable after creation, which is the
  desynchronisation the freeze exists to prevent; the safe default for an unclassified key is
  therefore identity, and the cost of getting it wrong in that direction is a control that
  refuses input a session too early.
