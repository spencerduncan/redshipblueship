# Phase 3 roadmap — Basic Combo Randomizer

> Written 2026-07-18. State verified against `main`, milestone 4, and the open
> tracker on this date. Supersedes nothing; complements `.claude/worker-prompts.md`
> (which stays the *worker* task file — this is the *phase* plan).
>
> **AMENDED 2026-07-19 — read §0.1 before planning off this document.** Six
> claims below were checked against source and the built binary after a code
> review and ODR sweep. Four are wrong. The execution plan derived from this
> roadmap plus those corrections is `docs/phase3-execution-prompt.md`.

## 0.1 Corrections (2026-07-19) — these override the body below

Verified after Phase 2 merged (`c8c47fa6`). Each was checked against source or
against `build-cmake/redship.exe`, not inferred.

1. **MM's randomizer is not in the shipping binary.** §1's table implicitly
   treats MM's rando as present. `games/mm/2s2h/Rando/` is complete (pools,
   fill, logic tiers, 382-row item table, spoiler JSON) and is built as
   `2ship_rando` and linked — but **no undefined symbol in the binary
   references it**, so the linker discards every object. Every non-`Rando/`
   caller of `Rando::` is in an excluded TU: `BenPort.cpp` (CMakeLists `:202`),
   `BenGui/` (`:205`), `DeveloperTools/` (`:208`), `SaveManager/` (`:250`), or
   `Enhancements/**` (`:335` — `2ship_enh`/`2ship_rando` deliberately do *not*
   get `WHOLE_ARCHIVE`). Confirmed by string-probing the real exe: the Rando
   menu string is absent while a MotionBlur control string is present.
   **Consequence: #384 gates Lane C**, and the `rando` CTest label is three
   invocations of one *OoT* test — there is no MM-side rando coverage.

2. **The cross-game seed does not propagate in single-exe.** §1 says seed
   propagation and the `sourceIsRando` handshake are "wired both directions."
   Both cited sites are in the dead legacy path: `OTRGlobals.cpp:2866` is inside
   `OoT_FreezeState(ComboContext*)`, `BenPort.cpp:2167/:2222` inside
   `MM_InitFirstEntrySaveContext`/`MM_FreezeState` — and `BenPort.cpp` is
   excluded. `switch.cpp:171` states these are "only compiled in non-single-exe
   builds"; their sole caller `Context_ProcessSwitch` has no callers. The live
   path (`Combo_FreezeState` → `Context_FreezeState` → `FreezeState`) carries a
   `SaveContext` blob and **nothing else** — no `ComboContext`, so no seed, no
   `sourceIsRando`, no `sharedItems`, no `sharedFlags`. **Lane A must first make
   the `ComboContext` channel cross the switch at all.**

3. **`ComboContext` has no serialization headroom, and Lane A necessarily
   breaks it.** `save.cpp:31` sets `kComboSize = sizeof(ComboContext)`;
   `DeserializeHeader:158-160` refuses any mismatch and `Load()` returns false
   *silently*. §7's own mitigation (tag shared items with origin game) changes
   `sizeof(ComboContext)`, so **every existing `.redsave` stops loading the
   moment Lane A lands.** Fix before Lane A and before the `v0.1.0-prealpha`
   tag: reserved padding, convert the tier-1 check to the size-field-driven
   zero-extend the game tiers already use (`save.cpp:167-170`), bump
   `RSBS_SAVE_VERSION`.

4. **§7 is wrong that Fault A "may corrupt saves on exit."** The `.redsave`
   write path is sound — atomic temp-plus-rename (`save.cpp:110-133`), CRC32
   over payload, explicit refusal to write a half-empty file. The real exposure
   is the `OnExitGame` write not being *reached*: loss of latest state, not
   corruption. Different severity, different fix.

5. **#370 gates Lane A.** The `.redsave` write executes inside the mutex #370
   poisons — `SaveManager.cpp:1264` locks, `:1353` fires
   `ExecuteHooks<OnSaveFile>` (the handler at `:151` calls `RsbsSave_Save()` at
   `:164`), `:1355` unlocks. Coupling is via hook dispatch, not an include.
   Also: `saveMtx` is a plain `std::mutex` (`SaveManager.h:200`), so any Lane A
   code hanging off `OnSaveFile` that re-enters SaveManager self-deadlocks.

6. **Two more dead-plumbing fields and two missing gates.**
   `gComboCtx.saveSlot` (`context.h:103`) is dead exactly like
   `sharedItems`/`sharedFlags` — zero non-test references. And §3 item 9's
   **known-issues doc does not exist** (`docs/` has no `known-issues*`), so
   `v0.1.0-prealpha` cannot be tagged until it is written;
   `.claude/worker-prompts.md` is still stale in the exact way §0 item 1
   describes, and it is what agents read to orient.

Additionally, the #387 remedy is wrong as filed: measured, **31.42 of
build-linux's 32.7 minutes is compilation**, so the proposed "~5 minute
link-only job" cannot exist. The real lever is `generate-builds.yml:227`, where
ccache restores on feature branches but never saves. #387's *diagnosis* stands.

## 0. Tracker corrections (verified, apply before planning off these docs)

Three load-bearing claims in the current docs are stale:

1. **`.claude/worker-prompts.md` Rev 6/7 say "gh auth still absent."** It is
   present and working (`gh auth status` → logged in, scopes `repo`/`workflow`).
   The 22 commits it describes as "awaiting push" *are* pushed, and are open as
   **PR #368** ("Phase 2: first full OoT↔MM gameplay round trip", 74 files,
   +5367/−762). Nothing is stranded; it is waiting on CI.

2. **Epic #321 (pre-alpha v0.1.0) reads as ~0/8 but is ~6/8.** #315 and #316 are
   MERGED; #317, #318, #319, #320 are all CLOSED. The only real remaining gates
   are **#310** (manual real-ROM QA) and the known-issues doc. Pre-alpha is much
   closer than the epic body implies.

3. **Milestone 4 is named "Basic Combo Randomizer" but contains no combo
   randomizer work.** Its 17 closed issues are SOH shuffle ports (#235, #289–293),
   Phase-1 cleanup (#270–272), and a PR-rebase chain (#253–258). Its 2 open issues
   (#34 settings migration, #177 Windows CI time) are also not randomizer work.
   **The milestone's headline deliverable is 0% started.** It has been used as a
   catch-all bucket.

## 1. Where the cross-game randomizer actually stands

The foundation is *partially* real. Verified by source inspection:

| Capability | State | Evidence |
|---|---|---|
| Cross-game switch (OoT↔MM round trip) | **Works** | `int-gameplay-roundtrip` 3-cycle soak passes; PR #368 |
| Shared rando seed propagation | **Wired both directions** | `games/oot/soh/OTRGlobals.cpp:2756-2766`, `games/mm/2s2h/BenPort.cpp:2153-2226` |
| `sourceIsRando` handshake | **Wired both directions** | same |
| `gComboCtx.sharedItems[32]` | **Dead plumbing** | declared `src/common/context.h:102`; the *only* other references in the whole tree are in `src/common/tests/`. No game code reads or writes it. |
| `gComboCtx.sharedFlags[64]` | **Dead plumbing** | declared `src/common/context.h:101`; zero non-test references. |
| Cross-game entrance shuffle | **Not started** | `src/common/entrance.h` is a fixed 1:1 link table; header says "extensible for future entrance shuffling" |
| Cross-game item placement | **Not started** | no item-namespace bridge, no foreign-item give path |

**Read that table as: the pipe is laid and the seed flows through it, but nothing
is on either end of the item channel.** Phase 3's first job is producers and
consumers for `sharedItems`/`sharedFlags` — everything else in the phase depends
on it.

## 2. The real capacity constraint

Not engineering throughput — **validation bandwidth.**

Hosted CI is ROM-free by construction (`oot.o2r`/`mm.o2r` cannot exist there);
the `int-*` tier is `workflow_dispatch`-only and never runs on a PR. Exactly one
human with real ROMs can confirm gameplay behavior. `docs/ci-gameplay-repro-postmortem.md`
exists because an entire crash class shipped through fully green CI.

Planning consequences, which apply to every item below:

- Every Phase 3 feature ships with a **ROM-free test lock in the `redship` CTest
  label**, or it is not done. Not negotiable — it is the only automated tier.
- **Operator verdicts are the scarce resource.** Batch them. Do not design lanes
  that each need an independent hands-on session; design lanes that can be
  verified together in one sitting.
- Budget roughly **20% of the phase to correctness debt** carried out of Phase 2
  (§3), not 0%. Phase 2's debt is unusually load-bearing: two of the items are
  data-loss bugs.

## 3. NOW — close out Phase 2 (blocking Phase 3)

Nothing in Phase 3 should start before item 1 lands. Items 2–5 can run in
parallel with early Phase 3 design work.

| # | Item | Why it is here | Size |
|---|---|---|---|
| 1 | **Merge PR #368** — drive CI green, squash-merge | 22 commits of round-trip stabilization are not on `main`. Every other lane branches off stale code until this lands. **Hard blocker.** | S (CI-bound) |
| 2 | **#370** SaveManager `saveMtx` never unlocked + truncate-before-write | Permanent deadlock on every subsequent save/load; **strictly worse than the crash it replaced**. Data loss. | S |
| 3 | **#371** `sequenceMap`/`gSequenceMap` `malloc` → `calloc` | Uninitialized holes get dereferenced and `free()`d. Named candidate contributor to the tracked `0xC0000374` exit corruption (Fault A). One-word fix, outsized payoff. | S |
| 4 | **#364** F10 hot-swap never freezes state | Phase 2 turned this from an obvious title-screen boot into **silent progress rollback** — the worst failure mode, because the user does not notice. `critical`. | M |
| 5 | **Retire the stub-signature-drift class** | #372, #379's `MotionBlur_Override`, and the two dead FI stubs are the *same class* Phase 2 already eliminated twice (see `mm-stub-signature-drift` note, postmortem §8.3). The structural fix exists: a header-checked TU compiled against the real declaration, so drift becomes a compile error. **Highest leverage item in this list** — retires a recurring class instead of three instances. | M |
| 6 | **Vacuous-gate category** — #375, #376 | Same class as the already-fixed #366. A gate that cannot fail is worse than no gate: it reads as coverage. Treat as one category, not two bugs. | M |
| 7 | Remaining #381 correctness: #374, #373, #378, #377, #372 | Real repros, none blocking. Parallelizable. | S each |
| 8 | Hygiene: #369 (format allowlist), #379, #380 | #369 is highly parallelizable, one directory per PR. | S each |
| 9 | **#310 manual QA + known-issues doc → tag `v0.1.0-prealpha`** | The only remaining #321 gates. Closing this converts Phase 2 into a *shipped artifact*, which is what makes Phase 3 feedback possible. | M (operator-bound) |

**Phase 2 closure criterion:** PR #368 merged, #381's correctness block closed,
both vacuous gates able to fail, and `v0.1.0-prealpha` tagged from green `main`.

## 4. NEXT — Phase 3.0 MVP ("Basic Combo Randomizer")

### MVP definition

> **One seed produces a paired OoT+MM world in which at least one item class
> crosses games, the crossing survives a full round trip, and a spoiler log
> describes it.**

That is deliberately the smallest thing that earns the milestone's name. It is
*not* OoTMM parity.

### Lanes

**Lane A — Make `sharedItems`/`sharedFlags` real.** *(prerequisite for everything)*
Producers and consumers on both sides of the switch: OoT writes on suspend, MM
reads on resume, and back. Define the shared-item id namespace up front — OoT's
`RG_*` and MM's item ids are unrelated enumerations and *must not* be aliased by
raw integer (this is the same class of bug as the entrance-id leak fixed in #356).
Lock with a `redship`-label round-trip test asserting a written item survives
suspend→switch→resume→switch→resume.
Size: **M**. Risk: low. Fully ROM-free testable.

**Lane B — Unified seed → paired world.** One seed deterministically derives both
games' randomizer settings. Seed *propagation* already exists (§1); what is
missing is generation-side determinism and a settings surface that presents the
pair as one configuration rather than two. Lock with a same-seed-twice determinism
test.
Size: **M**. Risk: low-medium. Mostly ROM-free testable.

**Lane C — Cross-game item placement (narrowed).** The headline. Ship it
**one-directional and one item class** for the MVP — recommended: a small set of
OoT progression items placeable in MM checks. Needs a foreign-item give path, and
text/icon handling for an item the receiving game has no assets for (a generic
"foreign item" presentation is acceptable and is what makes the narrowing viable).
Size: **L**. Risk: **high** — this is the phase's big bet.

**Lane D — Logic (DEFERRED to 3.1, deliberately).** Cross-game logic — not
placing MM's Bow behind an OoT check that requires MM's Bow — is the hardest
problem in the phase and does not fit an MVP. **Ship 3.0 with free-form placement
plus a spoiler log**, which is a legitimate rando mode, and let the spoiler log
carry the burden logic would otherwise carry. Adding logic later does not
invalidate A/B/C.

### Sequencing

A → B in parallel with A's back half → C. D is out of scope.
Gate C on Lane A being merged and locked; C without A is unverifiable.

## 5. NEXT — Phase 3.1 "Two-Way Combo Randomizer"

Phase 3.0 met its contract and its milestone reads 17/17 closed. What it shipped
is **four hand-pinned OoT items** (`kForeignPoolV1`,
`games/oot/soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp:58-63`) placed
into MM checks, one direction, named correctly over a shared `RI_RUPEE_HUGE`
icon and model, hosts drawn at random from junk-holding shuffled checks with
shops excluded, no cross-game logic, no MM options surfaced, and every
`RO_SHUFFLE_*` / `RO_HINTS_*` off in the paired profile.

Tracker: **#492**. Contract:

> **Items cross in both directions, chosen by rule rather than by a hand-written
> array, presented with their own identity, and configurable from one menu.**

Logic stays out — Lane D is promoted to its own 3.2 tracker (#500). 3.1 keeps
the 3.0 bargain: free-form placement plus a spoiler log.

### Four corrections to the sequencing this section used to imply

Each was adversarially verified against the tree, and each refuted the
intuitive ordering. Plans written without them are wrong in ways that compile.

1. **The reverse direction is not a mirror of Lane C** (#493). Only the
   origin-tagged `SharedItem` carrier is direction-neutral. The placement table
   is hard-keyed `uint16_t mmCheckId` with static_asserted `.redsave` offsets;
   OoT generates *first* and stamps the pairing key only at the end of
   `Playthrough_Init`, after `Fill()` and after `SpoilerLog_Write()`; and OoT's
   actor collection path drops the `RandomizerCheck` before the give, so the
   foreign identity must ride inside the `GetItemEntry`. **XL, not L.**
2. **The combo tracker is not a substrate** under presentation, placement rules
   or hints. #458 is a read-only, staleness-labelled reader of the *inactive*
   game's frozen shadow and structurally cannot serve a live write-time
   placement pass. Those three run in the active game and already share
   `src/common/foreign_items.h`. Sequence #458 in parallel, not first.
3. **`RSBS_FOREIGN_PLACEMENT_CAP` is a trap, not a deadline** (#490). Raising it
   in place is *already* a silent `.redsave` format break at head, because
   `grantCursors` and `sharedItemOverflowCount` are carved after it and the
   offset static_asserts are expressed in terms of the cap, so they follow a
   bump instead of catching it. The comment at `src/common/context.h:161-167`
   still promises this is cheap. Defusing the comment is the urgent part; the
   capacity increase belongs with whatever grows the pool.
4. **#451 does not gate the menu theme** (#497). Its four contended keys have no
   compiled **MM-side** reader: MM's readers are `BenGui/BenMenu.cpp` (a TU in
   no CMake target) and `BenGui/Menu.cpp` (in the elided `2ship_rando_ui`),
   neither of which the `2ship_enh` WHOLE_ARCHIVE flip touches. OoT's SohMenu
   *does* read all four from the live link (`SohMenuSettings.cpp:127`,
   `SohMenuEnhancements.cpp:148`, `SohMenuDevTools.cpp:35`, `Menu.cpp:568`) —
   that is the shared half of the hazard, not its arming condition, which is a
   *second*, MM-side shell indexing the same key. MM's option *table*
   (`Rando::StaticData::Options`) is already in the link via WHOLE_ARCHIVE'd
   `2ship_rando`, so a SohMenu-hosted MM pane can be built today. **The settings
   theme is unblocked now.**

### Waves

- **Wave 0 — hardening**: #487 (owl-save readback zeroes the live MM
  SaveContext — a still-live P0-class defect with the same symptom as the
  moon-crash leg closed in PR #485), #488 (host predicate), #489 (MM trackers
  show no data), #490 (cap comment), #491 (vacuous arrival lock). Wave 0 blocks
  **operator playtest acceptance** of live-play-facing work; it does **not**
  block the ROM-free `gComboCtx` / `src/common` increments, which never read
  `saveType` and start immediately.
- **Wave 1 — two directions**: #493. Separable cheaper increment: wiring
  `MM_AwardSharedItem` (still the Lane-A1 logging stub) to a real give needs
  none of the OoT-side work.
- **Wave 2 — identity and breadth**, parallel with Wave 1: #494 (presentation),
  #495 (rule-defined pool), #496 (in-game spoiler view), #458 (combo tracker).
  Presentation and pool-scaling are **independent** — foreign pickups already
  name each item individually, so presentation is O(1) in pool size.
- **Wave 3 — configuration**, parallel throughout: #497 (menu IA + ADR 0004),
  #498 (combo-level settings), #499 (MM shuffles and hints on).

## 5a. LATER — beyond 3.1

Directional, not committed:

- **Cross-game logic + beatability check** — Lane D, promoted to #500;
  design: [ADR 0010](adr/0010-cross-game-logic-and-beatability.md) (Proposed)
- **Cross-game entrance shuffle** — `entrance.h` was designed for it; the
  1:1 link table generalizes to any-entrance→any-entrance. Needs 3.2's logic to
  be more than a spoiler-log mode
- **Netplay 1b / multiworld player slots** — ADR 0007 specifies the shape;
  #460 is 1a
- **`sharedFlags` semantics** — which world events are genuinely shared
  (this needs a design decision, not just plumbing)
- **#34 settings migration** — deferred here by design since Phase 2

## 6. Milestone hygiene (recommended)

- **Move #34 and #177 out of milestone 4.** Neither is combo-randomizer work.
  #177 is CI infrastructure; #34 is settings migration explicitly deferred.
  Leaving them there is what made the milestone read as "in progress" while its
  actual deliverable sat at zero.
- **File a Phase 3 tracker epic** with Lanes A–D as sub-issues, in the shape of
  #381 (which is a good tracker: categorized, with the *why* on each item).
- Keep `.claude/worker-prompts.md` as the worker task file and **rewrite it as
  Wave 4** once PR #368 merges — its own §"Replanning" section requires this, and
  it is the mechanism that stops the file going stale again.

## 7. Risks

| Risk | Impact | Mitigation |
|---|---|---|
| **Item-id namespace aliasing across games** | Repeat of the #356 entrance-leak crash class — an id from one game interpreted as an index in the other | Tag shared items with their origin game, exactly as the startup entrance was given game affinity in `7c6e7eec`. Decide this in Lane A, before any placement work. |
| **Validation bandwidth** (single operator, ROM-gated) | Phase 3 features ship unverified; postmortem repeats | Every feature gets a `redship`-label lock; batch operator verdicts |
| **Lane C scope creep toward OoTMM parity** | Phase never closes | The MVP definition in §4 is the contract; one direction, one item class |
| **Phase 2 debt compounding** | Correctness debt in the save path corrupts rando saves, which are longer-lived | §3 items 2–4 before Lane A starts |
| **Fault A (`0xC0000374` on exit) still open** | Every session ends dirty; may corrupt saves on exit | #371 is a named candidate; re-evaluate Fault A after it lands |
