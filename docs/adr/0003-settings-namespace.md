# ADR 0003: One shared CVar store, classified deliberately — settings are unified by default

- Status: **Accepted** (2026-07-23, #497 step 1)
- For: #34 (settings migration), gating #35; the settings decision in
  `docs/unified-surface-findings.md` §6.1
- Corrected on acceptance: §4.1's `gDeveloperTools.DebugSaveFileMode` row was
  **wrong** and is struck below; §5 gains the `TimeSavers` casing decision
  ADR 0004 asked this document to make.
- Measured against `origin/main` on 2026-07-21 by exhaustive grep of both
  trees (method and its one correction in Appendix A — the numbers below are
  reproducible, not quoted)

**No CVar is renamed by this PR.** This document decides *what* moves; the
rename list in §5 is the executable output, scheduled as #34.

## Settled inputs (maintainer decisions, 2026-07-21)

Three questions were decided by the maintainer during drafting, not inferred
by this document. They are recorded here as premises, and the analysis below
is built on top of them:

1. **Governing principle.** *"In general, this is conceptually one game. If
   something applying to both makes sense, it should."* Settings are
   **unified by default**; per-game keys are for things that genuinely differ.
2. **Mechanism.** *"One CVar both games read is fine."* Shared-intent settings
   are **one key in one store**, read directly by both ports. No UI fan-out
   layer, no per-game shadow keys, one source of truth. §3 records the
   trade-off this accepts.
3. **Convergence direction.** *"We unify around the OoT keys where we should."*
   Where both games mean the same thing under different key names, **MM moves
   to OoT's key.**

Decision 2 in particular reversed an earlier draft of this ADR, which had
recommended namespacing MM's entire keyspace under `gMM.`. That reversal and
its reasoning are recorded in §3.3, because the discarded option is the one a
future reader is most likely to re-propose.

## Context

### 1.1 One store, one file, two games

RedShipBlueShip runs both games in one process against **one** CVar store,
backed by **one** config file:

```
rsbs/src/main.cpp:336
    Ship::Context::CreateUninitializedInstance(
        RSBS_WINDOW_TITLE, "soh", "shipofharkinian.json");
```

The app short name (`"soh"`) and filename are pinned deliberately — the
comment above that call explains why: re-keying them per selected game would
split one user's saves and settings across two app-data folders the first time
they switched games. Upstream's own bootstraps
(`games/oot/soh/OTRGlobals.cpp:404`, `games/mm/2s2h/BenPort.cpp:194`) are both
bypassed in the single exe, so `2ship2harkinian.json` is **never opened** by
`redship`, and MM's sole config updater (`Ben::ConfigVersion1Updater`,
registered only in the excluded `BenPort.cpp:843`) never runs.

Every key either game reads or writes therefore resolves in the same flat
`CVars` map. Under the governing principle this is **the right architecture,
already in place** — the work is not to partition it but to make its contents
deliberate.

### 1.2 How keys are spelled — the asymmetry that prices everything

**OoT builds prefixes in CMake.** `CMake/soh-cvars.cmake` `set()`s fourteen
prefixes and pushes them through `add_compile_definitions` at
`CMakeLists.txt:10`; `games/oot/soh/cvar_prefixes.h` wraps them as
`CVAR_CHEAT(var)`, `CVAR_ENHANCEMENT(var)`, etc. A prefix-level rename on the
OoT side is a one-line build-system edit touching no source.

**MM builds prefixes nowhere.** `games/mm/` uses **zero** `CVAR_*` prefix
macros — verified, not assumed. All 424 distinct MM keys are raw literals at
the call site:

```
games/mm/src/code/z_bgcheck.c:1918
    if (CVarGetInteger("gCheats.NoClip", 0) && actor != NULL && ...
```

MM's TUs do inherit the `CVAR_PREFIX_*` defines (the `add_compile_definitions`
is top-level); they simply never use them.

Consequence for §5: every MM-side rename is a literal source edit, so the
convergence list must be **small and justified key by key**. It is: 9 sites in
tier 1.

### 1.3 The measured collision set — and a correction to the findings doc

`docs/unified-surface-findings.md` §1 lists five colliding keys
(`gCheats.{InfiniteHealth, InfiniteMagic, MoonJumpOnL, NoClip,
EasyFrameAdvance}`) and states the collision is defused *only* because MM's
registrars are link-elided.

**The measured set is 30 keys, not 5, and 5 of them are live in the shipped
binary today** — not defused by anything. Of the doc's own five, four are
genuinely inert and `NoClip` is live. The doc's *mechanism* is right; its
*scope* and its "all inert" conclusion are not.

(The doc's one exactly-correct number is "~65": `gSettings.*` appears exactly
65 times in `games/mm/2s2h/Enhancements/`. 125 across all of `2s2h/`, 131
tree-wide, spanning 41 distinct keys.)

A first pass of this measurement reported 36 collisions. Six were artifacts of
scraping `games/oot/soh/config/ConfigMigrators.h`, whose `from` fields are
**legacy** spellings OoT has already migrated away from. Excluding that file
gives 30. The error is worth recording because one of the six turned out to be
a real finding in disguise — see §2.3.

#### The five live-today collisions

Read from TUs unambiguously in the link — core decomp reached by ordinary
symbol reference every frame the game runs:

| Key | OoT reader | MM reader |
|---|---|---|
| `gCheats.NoClip` | `games/oot/src/code/z_bgcheck.c:1905` | `games/mm/src/code/z_bgcheck.c:1918` |
| `gDeveloperTools.DebugEnabled` | `z_debug.c`, `z_demo.c`, `z_camera.c`, `z_sram.c`, `game.c`, `graph.c`, `z_message_PAL.c`, `z_player.c`, `z_file_choose.c`, `z_kaleido_scope_PAL.c` | `graph.c:424`, `z_pause.c:37`, `z_player.c:13038`, `z_kaleido_scope_NES.c:534` |
| `gDeveloperTools.FrameAdvanceTick` | `z_frame_advance.c:21,25` | `z_pause.c:43,47` |
| `gEnhancements.Graphics.IncreaseActorDrawDistance` | `soh/ShipUtils.cpp:30` | `z_actor.c:3379,3470`, `z_obj_grass.c:81,421`, `z_en_wood02.c:106` |
| `gEnhancements.Graphics.ActorCullingAccountsForWidescreen` | `soh/ShipUtils.cpp:41` | `z_actor.c:3408,3471`, `z_obj_grass.c:85`, `z_en_wood02.c:107` |

`FrameAdvanceTick` is not merely read but **`CVarClear`ed** by both
(`z_frame_advance.c:25` / `z_pause.c:47`) — a shared write.

**Under the governing principle, all five of these are working correctly.**
They are the shape the project wants, arrived at by accident. The remaining 25
are dormant behind the `2ship_enh` / `2ship_rando_ui` plain-archive elision
(`games/mm/CMakeLists.txt:361,389`) and will behave the same way once those
flip to `WHOLE_ARCHIVE`.

#### The trace: what happens today when you toggle one

OoT's `SohMenu` is the only live menu in the build (findings §1). Ticking
*Enhancements → Cheats → No Clip* calls `SohMenuEnhancements.cpp:1747`'s
`WIDGET_CVAR_CHECKBOX` on `CVAR_CHEAT("NoClip")`, writing the string
`gCheats.NoClip` into the shared store. OoT's `z_bgcheck.c:1905` reads it and
skips player collision; **MM's `z_bgcheck.c:1918` reads the same string and
does the same thing.** One toggle, two games, today.

`gCheats.InfiniteHealth` is the inert counterpart and shows the elision
mechanism exactly: MM's reader is
`games/mm/2s2h/Enhancements/Cheats/Infinite.cpp:11`, a `COND_HOOK` inside
`2ship_enh`. Nothing references that TU's symbols, the linker discards it, the
registrar never runs. Flip `2ship_enh` to `WHOLE_ARCHIVE` — step 3 of the
enhancement chain in findings §4 — and it starts behaving like `NoClip` does
now, which is **the desired outcome**, not a regression.

### 1.4 The real defect class: divergence, in both directions

Since sharing is the goal, a same-spelled key is not a bug. The two actual
defect classes are:

- **(P) Accidental sharing with divergent semantics** — both games read one
  key but interpret the value differently. A single control cannot drive both
  correctly; these must be split.
- **(Q) Accidental non-sharing** — the same setting under two different key
  names, so the unified control the principle asks for does not exist. These
  must converge.

(Q) is the larger and more user-visible problem, and it is invisible if you
only look for collisions. The clearest case is audio volume: a user sets
master volume in OoT's menu, switches to MM, and MM plays at its untouched
default, because the two ports spell the key differently.

### 1.5 Per-game-only prefixes (out of scope, listed for completeness)

Disjoint today and staying disjoint — **no renames proposed for any of these**:

- **OoT only:** `gRandoSettings`, `gRandoEnhancements`, `gTrackers`,
  `gGeneral`, `gGameplayStats`, `gTimeDisplay`, `gRemote`, `gValueViewer`,
  `gAddTraps`
- **MM only:** `gRando`, `gModes`, `gFixes`, `gEventLog`, `gCollisionViewer`,
  `gHudEditor`, `gNotifications`, `gDisplayOverlay`, `gColors`, `gCosmetic`

### 1.6 The reservation nothing implements

`src/common/ComboMenuBar.cpp:13–35` declares a third scheme —
`gCore.*` / `gOoT.*` / `gMM.*` — with a `MakeCVar` helper. **Nothing reads or
writes any of it.** ComboMenuBar is dead code: compiled into `redship_common`,
never instantiated, no `SetMenuBar` call in the single-exe link. #320 was
closed premise-incorrect for exactly this reason and is not relitigated here.

That reservation is **rejected by this ADR.** It partitions by game, which is
the opposite of the governing principle. Its constants should be amended to
match what ships, or deleted with the rest of the dead file — they must not
survive as a third scheme contradicting the accepted one.

### 1.7 The constraint that prices the convergence list: upstream diffs

RSBS deliberately preserves verbatim-applying upstream diffs against both
Shipwright and 2Ship — the mechanism by which the project takes upstream fixes
at all. It has been defended at real engineering cost:

- **#422** redefined `COND_HOOK` / `COND_ID_HOOK` / `COND_VB_SHOULD` /
  `REGISTER_VB_SHOULD` at the bottom of MM's `GameInteractor.h` so **~650 hook
  sites migrated to `S2H::GameHooks` with zero call-site edits.**
- **#435** did the same for the five `GameInteractor_Should` names, rebinding
  355+ VB call sites, and says so explicitly: *"Call sites compile textually
  unchanged, upstream diffs still apply — the same technique as C0's
  registration-macro redirection."*

Twice the project chose **redirect at the boundary** over **rewrite at the
call site**. Convergence (§5) does the opposite: it edits MM literals. That is
acceptable **because the measured set is 9 sites**, not 900. §5.4 records the
threshold at which that judgement flips, and the escape hatch if it does.

## Decision

### 2.1 Unified by default; every shared key is a recorded judgement

One CVar store, one config file, keys shared unless a reason exists not to.
The deliverable is not a partition — it is a **classification**, in which every
key both games touch falls into exactly one of:

| Class | Meaning | Action |
|---|---|---|
| **(S)** | Shared intent, semantics equivalent | Keep shared. Document as deliberate. |
| **(P)** | Shared key, **divergent** semantics | Disambiguate — the real bug set. |
| **(Q)** | Shared intent, **different key names** | Converge: MM moves to OoT's key. |
| **(G)** | Genuinely per-game | Leave alone. No renames. |

### 2.2 Mechanism: one CVar, read directly by both

Per maintainer decision 2. A shared-intent setting is **one key in the shared
store**, read directly by both ports' code. No fan-out layer, no per-game
shadow keys, no alias indirection.

**The trade-off this accepts, stated plainly:** there is **no per-game override
under this model.** A setting toggled while playing one game silently applies
to the other, including in the frozen game the user is not currently looking
at. If a per-game override affordance is ever wanted, it is a genuine future
redesign — a shared key cannot grow one incrementally, because the single
storage cell has nowhere to put the second value. Recorded so a future reader
knows this was a chosen constraint and not an oversight.

The upside is that it needs no machinery whatsoever: the store already works
this way, and the five live collisions in §1.3 are proof it works in practice.

### 2.3 Cheats are shared intent — a guard against a future "fix"

Per maintainer decision 1. **`gCheats.{InfiniteHealth, InfiniteMagic,
MoonJumpOnL, NoClip, EasyFrameAdvance}` sharing one key across both games is
correct and deliberate.**

This is written down defensively. Every one of these keys reads, to a
newcomer, exactly like a namespacing bug — `docs/unified-surface-findings.md`
§1 called them "live collisions" and framed them as a hazard, and that framing
is the natural one to arrive at independently. It is wrong. Infinite health
means the same thing in both games; one toggle driving both is what "this is
conceptually one game" means in practice.

**Do not disambiguate these.** A PR that namespaces `gCheats.*` per game is
reverting an accepted decision, not fixing a defect. Same for the rest of
class (S) in §4.

### 2.4 Convergence direction: MM moves to OoT

Per maintainer decision 3, and consistent with the architecture: SoH is the
host port. It creates the `Ship::Context`, owns the config file
(`shipofharkinian.json`), and provides the only live menu — the one the
unified settings surface will extend. OoT's key is the incumbent in every
sense that matters, and it is also the only spelling any existing user config
can contain (§6.1).

**The cost is real and lands on the 2S2H side:** every converged key is a
literal edit in MM's tree, and each one is a line where a future upstream 2S2H
diff can conflict. §5 keeps that cost measurable by keeping the list short and
justifying each entry.

**"Where we should" is a scope limit.** Convergence applies only where a
setting is genuinely shared-intent *and* the games currently spell it
differently. It is not a licence to rename MM's per-game settings into OoT
style, and it does not touch class (P), which gets split rather than merged.

## Rationale

### 3.1 Why classification rather than partition

The collision count moves with every upstream sync, so a scheme that hard-codes
today's 30 answers decays immediately. But the same is true of a scheme that
hard-partitions by game: it would have "fixed" all five live collisions in
§1.3, every one of which is currently doing the right thing. A partition
optimises for the wrong failure — it prevents accidental sharing at the cost of
preventing *deliberate* sharing, which is the thing the project actually wants.

Classification keeps the default aligned with the principle and makes the
exceptions explicit and reviewable. §6.3's lock is what stops it from decaying.

### 3.2 Why not a UI fan-out layer

The alternative mechanism — one control writing two per-game keys — was
considered and is rejected, beyond the maintainer's decision, for a reason
worth recording: **it cannot work for this codebase.** Fan-out unifies only
what the *menu* writes, but most of these keys are read by decomp code that
never goes near a menu widget — `z_bgcheck.c` reads `gCheats.NoClip` directly.
Anything that writes the store outside the UI (the console `set` command, a
hand-edited JSON, MM's `PresetManager` import) bypasses the fan-out and
desyncs the two halves silently. Two storage cells for one user-facing setting
is a consistency problem with no natural repair point.

### 3.3 Why the earlier `gMM.` namespacing recommendation was wrong

An earlier draft of this ADR recommended redirecting MM's entire keyspace under
`gMM.` at the accessor boundary — a `MM_CVarKey()` rewrite in the style of
#422/#435, with a deny-by-default shared-tier allowlist. It is recorded here
because it is the option a future reader is most likely to re-derive, and the
reason it is wrong is not obvious from the code.

It was mechanically sound, cost zero call-site edits, and preserved upstream
diffs on both trees perfectly. **Its defaults were backwards.** Deny-by-default
means a newly-convergent upstream key is *separated* automatically and shared
only if someone notices and adds it to an allowlist — so the system's silent
failure mode is "these two settings drifted apart," which under "conceptually
one game" is the failure that actually hurts users. It also would have
namespaced the five working live collisions into two settings apiece, breaking
behaviour that is currently correct.

The lesson generalises: **on this project the safe default is shared, and the
thing that needs a deliberate act is separation.** Any future proposal in this
area should be checked against that first.

### 3.4 Why edit MM literals rather than redirect at the boundary

Convergence (§5) breaks the #422/#435 posture by editing call sites. That is
justified at this size and not at arbitrary size. Tier 1 is **9 literal edits**
— six in one audio TU, three in an already-excluded file. Building a redirect
layer to avoid nine edits would cost more in indirection than it saves in diff
friction, and would put a lookup on a hot read path to solve a problem that
does not exist yet. §5.4 states the threshold and keeps the redirect available.

## Classification of the 30 measured collisions

### 4.1 Class (S) — shared intent, keep shared (26 keys)

No action. Documented as deliberate; see the §2.3 guard.

| Keys | Why equivalent |
|---|---|
| `gCheats.{InfiniteHealth, InfiniteMagic, MoonJumpOnL, NoClip, EasyFrameAdvance}` | Identical meaning in both games. Maintainer decision. |
| `gDeveloperTools.{DebugEnabled, FrameAdvanceTick, LogLevel}` | Host/debug state. `FrameAdvanceTick` is a transient both games set and clear; only one game is active at a time, so no interleaving. |
| ~~`gDeveloperTools.DebugSaveFileMode`~~ | **STRUCK 2026-07-23 — this row was wrong; the key is class (P).** It said: *"Value spaces align… only the unwritten fallback differs. Acceptable, worth knowing."* The value spaces do align, but the key is never left unwritten: `games/mm/2s2h/DeveloperTools/DeveloperTools.cpp`'s `RegisterDebugMode()` does `CVarSetInteger(CVAR_SAVE_FILE_MODE_NAME, DEBUG_SAVE_INFO_NONE)` whenever debug mode is off — the default state — so MM's registrar destroys OoT's `1` on every launch. ADR 0004 §2d holds the record; the key sits in `kDisputedClassificationKeys` until MM's write is retired and its read default aligned. Dormant today only because `2s2h/DeveloperTools/` is excluded from the single-exe link. **Class (S) count is 25 here, not 26.** |
| `gEnhancements.Graphics.{IncreaseActorDrawDistance, ActorCullingAccountsForWidescreen}` | Same multiplier semantics, same default (`1`), same widescreen-culling boolean. |
| `gAudioEditor.{EnemyBGMDisable, LowHpAlarm, SeqNameNotification, SeqNameNotificationDuration}` | Same audio-editor behaviours. |
| `gSettings.{CursorVisibility, DisableChanges}` | Host UI state. |
| `gSettings.Menu.{Theme, Popout, PoppedWidth, PoppedHeight, PoppedPos.x, PoppedPos.y, SearchAutofocus, SidebarSearch}` | Menu **chrome** — window geometry, theme, search behaviour. Game-independent. |
| `gInputViewer.*` (~60 keys, one entry) | Shared by construction: `CVAR_INPUT_VIEWER(var) "gInputViewer." var` is defined identically at `games/oot/soh/Enhancements/controls/InputViewer.h:5` and `games/mm/2s2h/BenGui/InputViewer.h:5`. Same N64 controller, same overlay. |

### 4.2 Class (P) — shared key, divergent semantics: **the real bug set** (4 keys)

These are indices into **different menus**. Sharing them is not a unified
setting; it is one game writing a value the other cannot interpret.

| Key | Divergence |
|---|---|
| `gSettings.Menu.ActiveHeader` | A **string** naming the active header. OoT writes `"Settings"` (`FileSelectEnhancements.cpp:73`) and `"Randomizer"` (`SohGui.cpp:258`); MM's `BenGui/Menu.cpp:589` reads it against MM's own header set, which has no `"Randomizer"`. A value valid in one is meaningless in the other. |
| `gSettings.Menu.SettingsSidebarSection` | Section index/name into each menu's own sidebar list (`BenMenu.cpp:298` for MM's). The lists differ in length and content. |
| `gSettings.Menu.EnhancementsSidebarSection` | Same, MM's at `BenMenu.cpp:659`. OoT's Enhancements sections and MM's do not correspond. |
| `gSettings.Menu.DevToolsSidebarSection` | Same. |

**Recommended disposition:** these are dormant today (MM's menu is elided) and
their correct resolution is coupled to the menu end-state decision
(findings §6.2). If MM's menu ports as a second menu, they must be split
per-menu; if MM's entries are added into `SohMenu` instead, they become genuine
class (S) automatically because there is then only one menu. **Defer to the
menu decision, but do not let MM's menu un-elide before resolving it** — that
is the point at which these four start corrupting each other's navigation state.

Note the shape of the bug: it is not a *value* conflict but a *namespace*
conflict — the value is an index into a list that differs. This is the same
class as the #356 entrance-id and ADR 0002's untagged-`uint16_t` hazards: an
identifier from one game's space read in the other's.

### 4.3 Class (G) — per-game, unaffected

Everything in §1.5. No renames.

## The convergence list — class (Q), the executable output

For each: MM's current key → OoT's key, with the justification for calling it
shared-intent. **MM moves in every case** (§2.4).

### 5.0 Canonical family spelling: `TimeSavers`, capital S (decided 2026-07-23)

ADR 0004's open call 5 asked this document to state which casing wins. It is
`gEnhancements.TimeSavers.` — OoT's, capital `S`.

Measured rather than preferred: capital-S holds 148:1 in `games/oot`, five MM
keys have **already** converged onto it via #462
(`SkipCutscene.{Story,Intro,OnePoint,Entrances}`, `DisableTitleCard`), and the
single lowercase hit anywhere in `games/oot` is a migration row retiring one of
MM's spellings. MM's remaining lowercase family is 8 keys — `AutoBankDeposit`,
`DampeDiggingSkip`, `FasterSceneTransitions`, `GalleryTwofer`, `MarineLabHP`,
`PowderKegCertification`, `SkipBalladOfWindfish`, `SwampBoatSpeed` — plus the
`games/mm/2s2h/Enhancements/Timesavers/` directory. That is the cheaper side to
move, and the two spellings do not collide in a case-sensitive store, so nothing
is broken while the move is pending.

**Mechanics, when the rename pass runs (not in this ADR's PR):** add the 8 keys
to `kConvergedKeys` under a `ConfigVersion9Updater` group, add
`"gEnhancements.Timesavers."` to `kRetiredKeyPrefixes`, and **in the same commit**
delete `kParkedCasingPrefixMM` and its assertion in
`src/common/tests/test_cvar_classification.c`. That assertion requires MM to
still contain the lowercase prefix and goes red the moment the last key moves —
by design, so the parked hazard cannot be quietly half-resolved.

### 5.1 Tier 1 — recommended now: 9 sites, 8 renames

**Audio volume (6 renames, 6 sites).** All six MM sites are in linked decomp
(`games/mm/src/`), so this is live user-visible divergence today.

| MM current | → OoT | Note |
|---|---|---|
| `gSettings.Audio.MasterVolume` | `gSettings.Volume.Master` | |
| `gSettings.Audio.MainMusicVolume` | `gSettings.Volume.MainMusic` | |
| `gSettings.Audio.SubMusicVolume` | `gSettings.Volume.SubMusic` | |
| `gSettings.Audio.SoundEffectsVolume` | `gSettings.Volume.SFX` | |
| `gSettings.Audio.FanfareVolume` | `gSettings.Volume.Fanfare` | |
| `gSettings.Audio.AmbienceVolume` | `gSettings.Volume.Ambience` | **No OoT twin.** OoT has no ambience channel. Recommend adopting the OoT-style spelling anyway so the family is consistent and OoT can adopt the key if it ever gains the channel. |

Master volume is host state by any reading; this is the clearest instance of
the §1.4 (Q) defect in the codebase.

**Character colour (3 renames, 3 sites).** MM reads OoT's **pre-migration**
spelling:

| MM current | → OoT |
|---|---|
| `gCosmetics.Link_KokiriTunic.Value` | `gCosmetics.Link.KokiriTunic.Value` |
| `gCosmetics.Link_GoronTunic.Value` | `gCosmetics.Link.GoronTunic.Value` |
| `gCosmetics.Link_ZoraTunic.Value` | `gCosmetics.Link.ZoraTunic.Value` |

This is the finding hiding inside the §1.3 measurement error. OoT already
renamed `Link_GoronTunic.Value` → `Link.GoronTunic.Value` in its own migration
table (`ConfigMigrators.h:482`); MM was never updated and is **stranded on a
spelling OoT abandoned**. They read as a collision only if you scrape OoT's
legacy `from` fields. Today the user's OoT tunic colours simply do not reach
MM.

Shared-intent holds on the merits, not just the name: MM applies these to the
Deku/Goron/Zora **forms** (`BenPort.cpp:1846–1856`, defaulting to
`kokiriColor`/`goronColor`/`zoraColor`) where OoT applies them to the tunics.
Different garment, same user intent — "the Goron colour." All three sites are
in `BenPort.cpp`, which is **already excluded from the single-exe link**, so
this costs zero live-code churn and the smallest possible upstream-diff
surface.

**Tier 1 total: 8 renames across 9 literal sites.** One focused commit.

### 5.2 Tier 2 — recommended, but gated on the tracker/menu decision (15 renames)

MM's `gWindows.*` and OoT's `gOpenWindows.*` are the same concept — "is this
window open" — under different prefixes. Fifteen window names match exactly:

```
ActorViewer   CheckTracker          CollisionViewer  InputViewer          ItemTracker           Menu    ModalWindow   SaveEditor
AudioEditor   CheckTrackerSettings  HookDebugger     InputViewerSettings  ItemTrackerSettings   MessageViewer  Notifications
```

plus two spelling near-misses worth folding in: `gWindows.CosmeticEditor` →
`gOpenWindows.CosmeticsEditor`, and `gWindows.DLViewer` →
`gOpenWindows.DisplayListViewer`.

MM-only windows (`BenInputEditor`, `DisplayOverlay`, `EventLog`, `HudEditor`,
`Timesplits`, `Timesplits.Settings`) are class (G) — no rename.

**Why this is gated rather than recommended outright.** OoT's Item Tracker and
MM's Item Tracker are *different windows*. Converging the key means opening one
opens both. Under the governing principle that is arguably right — the user
wants their tracker open regardless of which game is in front. But findings §3
records that MM windows registered on the shared Gui **draw regardless of
active game**, and that no per-game show/hide precedent exists yet. Converging
these keys before that gating exists would make OoT's tracker toggle
summon MM's dormant tracker on top of it.

**Recommendation: adopt tier 2 as the target, sequenced after per-game window
gating lands** (findings §6.3). Not a blocker for #34; it should be tracked
against the tracker work.

### 5.3 Explicitly not converging

- Anything in §1.5 (class (G)).
- Class (P) — those are split or resolved by the menu decision, never merged.
- `gRando` (MM) vs `gRandoSettings` (OoT). Near-miss names, genuinely different
  content: OoT's rando settings are CVar-backed, MM's live in the save
  (`RANDO_SAVE_OPTIONS`) — findings §2. Bridging those is a storage-model
  problem, not a naming one.

### 5.4 The threshold that would flip this approach

Tier 1 + tier 2 is ~23 renames. If a future convergence set is large enough
that literal edits would meaningfully degrade 2S2H diff application — a rough
line is **when a single upstream-tracked TU takes more than a handful of edits,
or the set exceeds ~50 sites** — the right move is the #422/#435 recipe: a
key-alias redirect in an MM-owned header mapping MM's spelling onto OoT's, with
call sites untouched. That is a strictly better tool at scale and a strictly
worse one at nine sites. It is recorded here so the option is available without
re-deriving it.

## Migration and compatibility plan

### 6.1 What actually needs migrating: less than it looks

- **Class (S) keys: nothing changes.** Same spelling, same store, same
  behaviour. Zero migration.
- **Class (G): nothing changes.**
- **Class (Q): the converging keys are the whole migration**, and the set of
  values at risk is small, because MM has essentially never written one. MM's
  menu is elided; the only writer that could have populated an MM-spelled key
  is a user hand-editing JSON or importing a 2Ship preset.
- **Class (P): no migration** — resolution is deferred to the menu decision.

The migration must not *depend* on that reasoning, though. It runs
unconditionally and is a no-op when the legacy key is absent.

### 6.2 Reuse the existing versioned updater

SoH already ships exactly the mechanism ADR 0002's growth contract and the
`.redsave` version window would ask for, already wired into RSBS:

- `games/oot/soh/config/ConfigMigrators.h` — declarative
  `{MigrationAction::Rename, from, to}` tables
- `games/oot/soh/config/ConfigUpdaters.cpp` — per-version `Update(Config*)`
  applying `CVarCopy(from, to)` then `CVarClear(from)`
- `games/oot/soh/OTRGlobals.cpp:1726–1731` — six versions registered, running
  in RSBS today

The migration is therefore **`ConfigVersion7Updater` + a `version7Migrations`
table**, in the shape of the six before it. This inherits monotonic versioning,
forward-only application, run-once semantics, and copy-before-clear ordering
for free — zero-loss by construction, matching the precedent.

### 6.3 The merge rule when both keys exist

A config can hold both `gSettings.Volume.Master` (written by OoT's menu) and
`gSettings.Audio.MasterVolume` (hand-edited or preset-imported) with different
values. `CVarCopy` overwrites unconditionally, so the plain updater pattern
would let the MM-spelled value clobber the OoT one. That is the wrong winner.

**Rule: the OoT-spelled key wins. The MM-spelled value is adopted only if the
OoT key is absent.**

```
for each (mmKey -> ootKey) in version7Migrations:
    if config has mmKey:
        if config does NOT have ootKey:
            CVarCopy(mmKey, ootKey)      # adopt; nothing to conflict with
        # else: OoT's value stands
        CVarClear(mmKey)                 # legacy spelling retires either way
```

Deterministic, and correct on the merits: the OoT-spelled value is the one the
user could have set through a live menu, so it is the one they last saw take
effect. This needs a `MigrationAction` the existing enum lacks — a
`RenameIfAbsent` alongside `Rename`/`Remove` — which is a small, honest
addition to `ConfigMigrators.h`.

**Back up before the first RSBS-era rewrite.** Copy `shipofharkinian.json` to
`shipofharkinian.json.pre-rsbs-<version>.bak` once. Cheap, and the one thing
the upstream updaters do not do.

### 6.4 The 2Ship import (#34's surviving half)

Importing a real `2ship2harkinian.json` is still worth doing and is now
well-defined: read the legacy file once, map each key through the **same**
table `version7Migrations` uses, apply §6.3's merge rule, stamp a flag so it
never re-runs. One table, two consumers — the updater and the importer cannot
drift.

**Ordering hazard for #34's acceptance criteria:** legacy `2ship2harkinian.json`
files must be brought up to 2Ship's own config version **before** key-mapping,
or pre-v1 files import mis-keyed. `Ben::ConfigVersion1Updater` exists but is
registered only in the excluded `BenPort.cpp:843`, so it does not run today.
The importer must invoke it explicitly or replicate it.

### 6.5 The lock: make the classification a build-time invariant

Consistent with how this project locks its other structural invariants
(`check-registrar-elision.sh`, the #375 collision gate, the `mm-gi-shim` fold
tripwire, ADR 0002's static-assert pins), the classification needs a test that
fails when reality drifts from it — otherwise it decays on the first upstream
sync.

Proposed: a CTest that re-runs Appendix A's extraction and asserts the set of
cross-game shared keys equals a checked-in manifest carrying each key's class.
A newly-convergent upstream key is then a **red build naming the key**, forcing
an (S)/(P) judgement at the moment it appears rather than after it ships. The
manifest doubles as the documentation §2.3 asks for.

This is the piece that turns "unified by default" from a slogan into a policy.
Without it, class (P) keys enter silently and behave exactly like the four in
§4.2 — dormant, then quietly wrong.

### 6.6 Rollback

Renames are mechanically invertible (`CVarCopy` the other way) and the source
edits are 9 literals. Nothing in this plan writes a lossy transform. The one
irreversible step is `CVarClear` of a legacy key after a successful copy, which
the §6.3 ordering makes safe.

## Consequences for #34 and the menu lanes

**#34 shrinks substantially.** Its issue body specifies a `gCore`/`gOoT`/`gMM`
mapping table, `MigrateOoTSettings` / `MigrateMMSettings`, and a `redship.json`
first-run check. Under this ADR:

- **Drop** the whole OoT-side mapping. `gEnhancements.* → gOoT.Enhancements.*`,
  `gCheats.* → gOoT.Cheats.*`, `gGfx.* → gCore.Graphics.*` — none of it moves.
  Zero migration for the keyspace holding 100% of real user data.
- **Drop `redship.json`** and its first-run existence check. The config file
  stays `shipofharkinian.json`, per `main.cpp:326`.
- **Keep** the 2Ship import, re-scoped per §6.4.
- **Add** the 8 tier-1 renames (§5.1), `ConfigVersion7Updater` with
  `RenameIfAbsent` (§6.2–6.3), and the classification lock (§6.5).

Net: #34 becomes "rename 9 MM literals, add one config updater, write the
importer and the lock" — one focused PR rather than a two-sided keyspace
migration.

**For the menu lanes:**

- **Findings §6.2 (menu end-state) is no longer gated on this.** Settings
  unify by default, so adding MM entries to `SohMenu` needs no per-widget
  collision audit — a shared-intent widget just uses the shared key.
- **Findings §6.3 (trackers) is unaffected**, and tracker un-elision remains
  the cheapest next move. Tier-2 window-key convergence (§5.2) should be
  sequenced with it.
- **The `2ship_enh` `WHOLE_ARCHIVE` flip is safe** under this ADR, with one
  exception: it arms the four class-(P) menu keys in §4.2. Resolve those, or
  confirm MM's menu stays elided through the flip, before it lands.
- **ComboMenuBar's `gCore`/`gOoT`/`gMM` constants are rejected** (§1.6) and
  should be amended or deleted with the file.

## Appendix A — measurement method, and its correction

Reproducible on `origin/main`:

```bash
# MM: every key is a raw literal
grep -rhoE '"g[A-Za-z0-9_]+\.[A-Za-z0-9_.]*"' games/mm/ \
  --include=*.c --include=*.cpp --include=*.h --include=*.hpp \
  | sed 's/"//g' | sort -u                            # 424 distinct

# OoT: raw literals EXCLUDING ConfigMigrators.h, plus the 14 CVAR_* macro
# forms expanded through CMake/soh-cvars.cmake's prefixes
grep -rhoE '"g[A-Za-z0-9_]+\.[A-Za-z0-9_.]*"' games/oot/ \
  --include=*.c --include=*.cpp --include=*.h --include=*.hpp \
  --exclude=ConfigMigrators.h | sed 's/"//g' | sort -u > oot_lit.txt
# + per-macro expansion of CVAR_CHEAT(...), CVAR_ENHANCEMENT(...), etc.
cat oot_lit.txt oot_macro.txt | sort -u                # 1202 distinct

comm -12 oot_live.txt mm_keys.txt | grep -vE '\.h$'    # 30
```

**The correction that matters.** Including `games/oot/soh/config/
ConfigMigrators.h` inflates OoT's key set from 1202 to 2588 and the collision
count from 30 to 36, because that file's `from` fields are **legacy spellings
OoT has already migrated away from**. Six "collisions" were MM matching keys
OoT no longer uses. Excluding the file is correct — and the three tunic entries
among those six turned out to be real class-(Q) findings (§5.1), so the
artifact was worth chasing rather than discarding.

`.h` matches (`game_lifecycle.h`, `gfx.h`, `global.h`) are `#include`
artifacts and are excluded. `gInputViewer.` is the shared macro *prefix* —
`CVAR_INPUT_VIEWER(var) "gInputViewer." var`, defined identically in both trees
— so its ~60 keys collide by construction and count as one entry.

Live-vs-dormant classification is by reader location: `games/mm/src/**` is core
decomp reached by symbol reference (live); `games/mm/2s2h/Enhancements/**` and
the rando-UI split are `2ship_enh` / `2ship_rando_ui`, plain static archives per
`games/mm/CMakeLists.txt:361,389` (dormant until the `WHOLE_ARCHIVE` flip).

MM's name-taking CVar API surface, for sizing any future boundary-redirect work
(§5.4): `CVarGetInteger` 831, `CVarSetInteger` 135, `CVarGetFloat` 81,
`CVarClear` 50, `CVarGetColor` 32, `CVarSetFloat` 20, `CVarSetString` 17,
`CVarGetString` 13, and 14 more across `CVarSetColor` / `CVarGetColor24` /
`CVarClearBlock` / `CVarCopy` / `CVarRegisterInteger` / `CVarGets` —
**~1193 sites behind 14 function names.**

## Appendix B — the 30 collisions by class

**(S) Shared intent — keep shared (26).** Live today (5) marked ★:

```
gCheats.EasyFrameAdvance                    gSettings.CursorVisibility
gCheats.InfiniteHealth                      gSettings.DisableChanges
gCheats.InfiniteMagic                       gSettings.Menu.Theme
gCheats.MoonJumpOnL                         gSettings.Menu.Popout
gCheats.NoClip                          ★   gSettings.Menu.PoppedWidth
gDeveloperTools.DebugEnabled            ★   gSettings.Menu.PoppedHeight
gDeveloperTools.FrameAdvanceTick        ★   gSettings.Menu.PoppedPos.x
gDeveloperTools.LogLevel                    gSettings.Menu.PoppedPos.y
gDeveloperTools.DebugSaveFileMode           gSettings.Menu.SearchAutofocus
gEnhancements.Graphics.IncreaseActorDrawDistance            ★   gSettings.Menu.SidebarSearch
gEnhancements.Graphics.ActorCullingAccountsForWidescreen    ★   gInputViewer.* (~60 keys)
gAudioEditor.EnemyBGMDisable                gAudioEditor.SeqNameNotification
gAudioEditor.LowHpAlarm                     gAudioEditor.SeqNameNotificationDuration
```

**(P) Divergent semantics — disambiguate, deferred to the menu decision (4):**

```
gSettings.Menu.ActiveHeader
gSettings.Menu.SettingsSidebarSection
gSettings.Menu.EnhancementsSidebarSection
gSettings.Menu.DevToolsSidebarSection
```

**(Q) Convergence targets** are not collisions by definition — they are in §5.
