# Unified surface: menu, rando settings, trackers, enhancements, netplay

> Investigation dated 2026-07-21, verified against `origin/main@365f6dad` by seven
> parallel read-only researchers. Anchors are symbols-first; line numbers drift.
> This is a **findings + options** document, not a committed plan — the decision
> points it names are listed at the end.

## TL;DR

Every one of these four asks — unified options menu, MM randomizer settings, both-game
trackers, MM enhancements — is blocked on the *same two things*, and neither is the
thing the docs implied:

1. **A registration surface for MM's UI.** MM's menu/tracker windows are compiled but
   **link-elided**; their instantiation lives only in the excluded `BenGui.cpp`.
2. **Dispatch, not registration.** ~650 MM hook sites now *register* safely through
   `S2H::GameHooks` (macro redirection, zero call-site edits), but a hook type stays
   **dormant until an `Execute` call is deliberately placed** at MM's upstream dispatch
   point. Registration ≠ dispatch is the single most load-bearing fact in this document.

   > **#439 is the worked example, and it has a second edge nothing above predicted.**
   > `OnSaveInit` *was* dispatched — from `MM_Sram_InitSave`, MM's file-select
   > new-file flow — and MMRandoGen proved that chain green for months. But a
   > cross-game switch never touches file select: it cold-starts the gamestate
   > chain, and `TitleSetup_SetupTitleScreen` authors the save with
   > `MM_Sram_InitNewSave()` + `OnSaveLoad`. So the paired MM world never
   > generated in real play. The generalization: **a dispatch point placed on
   > one entry path is not coverage of the feature** — enumerate every path that
   > reaches the state the hook keys off, and put the `Execute` at the
   > *convergence* point (for MM arrivals that is
   > `MM_Play_ConsumeStartupEntrance`, the one place after every boot-chain wipe
   > and before the save is interpreted).
   >
   > The same issue produced the mirror-image bug for `COND_HOOK` types: the
   > boot chain's `OnSaveLoad` dispatch **disarms** every `IS_RANDO` hook
   > (it runs against a vanilla bootstrap file), so a hook can be correctly
   > registered, correctly dispatched, and still be *unregistered* by an
   > earlier dispatch on the same path. Any `COND_*` condition read off save
   > state needs a re-dispatch once the final save is known.

The ODR/linker prep is **already paid for** (#415 shim + poison guard, #422 rando
reachability, #434 S2H UIWidgets). What remains is surface work, and it decomposes far
better than expected: **trackers are nearly free; the settings menu is the expensive
one; netplay is much closer than "nothing exists."**

## 1. Menu architecture — where we actually are

- The **only live menu** is SoH's `SohMenu` ("Port Menu"), registered on the shared
  libultraship `Gui` at OoT init. Esc/F1 handling is game-agnostic LUS code, so **OoT's
  menu is what you get while MM is running**, and every section in it — including the
  entire Randomizer header — is OoT-only.
- **`ComboMenuBar` is dead code**: compiled into `redship_common`, never instantiated,
  no `SetMenuBar` call exists in the single-exe link (so F1 is a no-op today). It
  reserves a *third* CVar scheme (`gCore.` / `gOoT.` / `gMM.`) that nothing reads or
  writes. Everything except its working `.redsave` file-select panel is a
  `TextDisabled` placeholder. **#320 was closed premise-incorrect** for exactly this
  reason — do not reopen it.
- MM's shell (`BenMenuBar` + `BenMenu` + its tracker windows) is entirely excluded;
  window instantiation is stranded in `BenGui.cpp`, whose only caller is the excluded
  `BenPort.cpp`.
- **Across a switch nothing touches the Gui**: suspend/resume is audio+graph only. An
  open SohMenu and OoT's tracker windows persist over MM and go *dormant* (not wrong)
  because every tracker gates on `IsSaveLoaded`.
- **CVar store is shared, and collides.** One `shipofharkinian.json`, overlapping
  upstream prefixes, with exact key collisions — `gCheats.{InfiniteHealth, InfiniteMagic,
  MoonJumpOnL, NoClip, EasyFrameAdvance}` are used by *both* games. This is defused
  **only** because MM's CVar-reading registrars are currently elided. Un-eliding without
  a namespace policy arms one toggle to drive both games. This is what open issue **#34**
  is really about, and it gates any honest settings unification.

### A newly-identified ODR landmine (tracked nowhere before this doc)

Both ports define `class Ship::Menu` and global-scope `WidgetInfo` with **divergent
layouts** (OoT `std::map` vs MM `std::unordered_map`; OoT's `WidgetInfo` has a
`raceDisable` field MM's lacks) under **identical include guards**. MM's implementation
is excluded, so a resurrected MM menu TU's `AddMenuEntry` would **silently cross-bind to
OoT's compiled implementation** against MM's layout — the exact #383/FlagTable class we
just spent a day retiring. **Any MM menu port must apply the #434 recipe (S2H-namespace
`Ship::Menu` + `MenuTypes`) first.**

## 2. Randomizer settings — the OoT/MM asymmetry

OoT's rando settings are **CVar-backed** and drawn by a 749-line `SohMenuRandomizer.cpp`
with a `Generate Randomizer` button. MM's rando options are **stored in the save**
(`RANDO_SAVE_OPTIONS`, applied at file creation from a static options table), drawn by
the elided `Rando/Menu.cpp`.

A unified pane must bridge *storage models*, not just draw two lists. Lane B's accepted
recommendation on #392 stands: **keep the minimal SohMenu path for 3.0**; the paired
world is opted into by generating on the OoT side, and MM's half derives from
`sharedRandoSeed` + `sharedRandoSettingsHash`.

Cheapest honest increment: a **read-only "paired world" summary + MM logic-mode picker**
inside the existing OoT rando pane — no MM menu port required.

## 3. Trackers — the surprise: nearly free

Contrary to what #427's headline counts imply, **MM tracker files carry zero of the 20
raw `RegisterGameHook` + ~36 event-enqueue sites**; their 6 `COND_*` sites already
redirect to `S2H::GameHooks`. The ODR prep landed in #422/#434. MM trackers are blocked
**purely on a registration surface + selective un-elision** — *not* on hook migration
and *not* on the guard flip.

MM tracker windows are plain `Ship::GuiWindow` subclasses, and the shared-Gui
`AddGuiWindow` precedent exists on both sides. **The BenMenu shell can be bypassed
entirely.**

For genuinely *cross-game* tracking, the data situation is:

| Source | Availability | Granularity |
|---|---|---|
| `gComboCtx` (`sharedItemsTagged`, `foreignPlacements`, seed/hash) | live, accessors exist (incl. `Combo_GetForeignItemName`) | live |
| MM check completion (`RANDO_SAVE_CHECKS`) | travels **inside** the MM shadow blob (in-save POD) | last freeze/save |
| OoT check status | heap `Rando::Context` + JSON only — **not** in the SaveContext blob | needs an accessor |

So a combo tracker needs **per-game adapters by design** (ADR 0002 forbids untagged
cross-game ids; `RsbsGameMetaDesc`'s offset-descriptor pattern is the in-tree precedent).
Note `OOT_SAVE_CONTEXT_SIZE` has only ~1 KB slack — an OoT check exporter must go the
accessor route, not the blob route.

**Two gotchas for any tracker work:** unified `gSaveContext` storage means an un-elided
MM tracker reading it while OoT is active reads OoT bytes through MM's struct layout —
explicit current-game gating is mandatory. And MM windows registered on the shared Gui
will draw regardless of active game; no per-game show/hide precedent exists yet
(`BenGui::Destroy` calls `RemoveAllGuiWindows`, which would nuke SoH's windows if reused
naively).

## 4. Enhancements — mostly dark, and one live hazard

Of ~195 compiled MM enhancement TUs, **~190 are link-elided**. Genuinely linked today:
`FrameInterpolation`, `MotionBlur`, `PauseOwlWarp`, `SavingEnhancements`,
`SkipGiantsChamber`, `AudioCollection`.

- **Motion blur works right now** (CVar-driven, called from `z_play.c`) — there is
  simply no UI to set it.
- **Cheats, Restorations, and the 58-file Cutscenes category are dead** (elided).
- The **execution path is already game-agnostic and correct**: `MM_Game_Init` →
  `MM_Rando_Init` → `S2H::ShipInit::InitAll()` runs *every* surviving MM registrar,
  enhancements included. There is no "pulled but never initialized" class — **elision is
  the only bottleneck.**
- The elision report's "26 present" for `2ship_enh` is a **measurement artifact**: the
  script matches `_GLOBAL__sub_I_<file>` by name, and 11 MM TU basenames have
  identically-named WHOLE_ARCHIVE'd OoT twins, so MM's elision is invisible. Worth
  fixing in the script.
- **LIVE HAZARD → filed as #442**: `SavingEnhancements.cpp` raw-registers on the shared
  4-byte GameInteractor and *is* in the link with its registrar executing — an
  out-of-bounds write reachable today whenever `RememberSaveLocation` is set and a save
  loads. Pull this fix forward ahead of the full #427 migration.

Turning enhancements on properly is a four-step chain: migrate `2ship_enh`'s raw sites
(~56) → **place ~22 missing `Execute` dispatch points** (the real size driver) →
WHOLE_ARCHIVE + promote the elision gate to required → menu surface + CVar-collision
policy.

## 5. Netplay — much closer than "nothing exists"

**The complete upstream Anchor co-op client is already vendored** in our SoH tree
(`games/oot/soh/Network/Anchor`, 25+ files, 22 packet types, hook-driven), compiled but
**inert because `BUILD_REMOTE_CONTROL` is never declared as an option**. SDL2_net is
already staged in vcpkg/apt/Docker. Anchor merged into SoH mainline 2025-11-14; the
server is a generic Go room-relay ("client software handles all game state"), actively
maintained — but **carries no license**, so self-hosting is community-normal while
bundling is legally gray. The protocol (null-delimited JSON/TCP) is trivial to
reimplement cleanly.

An **MM Anchor client exists** as an open, active alpha PR into 2S2H mainline — written
against MM's *raw* GameInteractor surface, i.e. exactly what our shim contract replaces,
so porting it is the same class of work as the `2ship_enh` migration.

**The most interesting finding:** a network item grant maps **1:1 onto the `SharedItem`
machinery we already shipped** — origin-tagged struct, stage/record/redeem with a
single-use `REDEEMED` bit, serialized in every `.redsave`. A remote grant is structurally
identical to an in-process cross-game grant: it plugs in as **one more producer call site
feeding `Combo_RecordSharedItem`**, and dedup, persistence, and arrival-time redemption
come free.

Combo-specific wrinkles for any Anchor-style sync: OoT hooks stop firing on
`Game_Suspend` while the receive thread survives the switch (unbounded queue growth;
`PLAYER_UPDATE` is handled *on* the network thread against frozen OoT structures);
`UPDATE_TEAM_STATE` serializes only `gSaveContext`, so `gComboCtx`/MM state are invisible
to peers; and our `clientVersion` string is still stock `Copper Bravo (9.1.1)`, meaning
a combo client could join public stock rooms and exchange state despite divergent
semantics — **a one-line fix worth doing before anyone flips the flag.**

### Sizing

| Increment | Scope | Size | Validation |
|---|---|---|---|
| 0 — light up OoT-only Anchor | declare the option, RSBS version string, suspend/resume gating | days | operator-only; switch-while-connected is an unaudited crash surface |
| **1 — multiworld-lite (recommended first step)** | `SharedItem` grants over the wire into `Combo_RecordSharedItem` | ~1–2 weeks | **largely ROM-free lockable with a loopback harness** |
| 2 — full dual-game co-op | port the MM client onto `S2H::GameHooks`, combo lifecycle gating, room semantics, protocol slot for `gComboCtx` | weeks | heavily operator-gated |

## 6. Decisions this needs from the maintainer

1. **Settings namespace (#34 ADR).** `gCore`/`gOoT`/`gMM` re-namespace vs keep
   `shipofharkinian.json` as-is. Every menu option depends on or dodges this, and the
   trade-off is real: re-namespacing breaks verbatim upstream diffs on both games.
2. **Menu end-state.** Extend SohMenu (cheap, settled-by-default) vs port BenMenu as a
   swappable second menu (`Gui::SetMenu` is single-slot — swap semantics undecided) vs
   revive ComboMenuBar (relitigates a settled decision; XL).
3. **Tracker scope.** Cross-game slice inside OoT's existing trackers (small) vs
   un-eliding MM's native trackers onto the shared Gui (small-medium, needs the
   `OnSceneInit` dispatch + name de-collision + per-game gating) vs a true combo tracker
   in `src/common` (large, needs per-game adapters).
4. **Netplay appetite.** Whether increment 1 (multiworld-lite on the existing redemption
   machinery) is worth scheduling in 3.1, given it is the only increment that is mostly
   CI-lockable.

## Recommended sequencing (if the answer is "all of it, eventually")

Cheapest-first, each independently valuable:

1. **#442** (live OOB write) — pull forward, hours.
2. **Tracker un-elision via shared-Gui bypass** — small, unblocked *today*, no menu
   dependency, no guard-flip dependency.
3. **`#34` ADR** — a decision, not code; unblocks everything menu-shaped.
4. **`S2H::Menu`/`MenuTypes` namespace split** — the #434 recipe, before any menu port.
5. **`2ship_enh` migration + dispatch placement + guard flip** — the gateway to
   enhancements *and* the MM netplay client.
6. **Netplay increment 1** on the `SharedItem` machinery.
