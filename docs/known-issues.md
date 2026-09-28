# Known issues

**Applies to:** `main` at `d6c8f270` (2026-09-28, PR [#776](https://github.com/spencerduncan/redshipblueship/pull/776): the single-bag switch of PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743)
plus the 19 PRs merged after it) and the GitHub Actions builds cut from it; the `v0.1.1-prealpha` tag
(2026-07-03) is older than everything in the first section.
**Last updated:** 2026-09-28 (the playtest refresh). **Playtesting this build?** Start with
[`playtest-2026-09.md`](playtest-2026-09.md).

RedShipBlueShip is **pre-alpha**. It boots Ocarina of Time and Majora's Mask from
one executable, round-trips between them through the Happy Mask Shop ↔ Clock Tower
portal, and generates a paired randomizer world in which items cross both ways.
This document is the honest list of what is broken or missing, grouped by what it
does to you as a player rather than by issue number. Every entry links its tracking
issue; entries that have been fixed since the last revision are struck through and
name the pull request, so old reports can be matched to the fix.

If you hit something not on this list, please file it. If you hit something on
this list, the issue link is the place to add detail.

---

## Read this before you play

### Cross-game randomization: what ships now, and what does not

The headline feature is real, and since 2026-09-27 it has cross-game logic. Phase 3.1
(tracker [#492](https://github.com/spencerduncan/redshipblueship/issues/492)) shipped; Phase 3.2 (cross-game *logic*, [#500](https://github.com/spencerduncan/redshipblueship/issues/500)) has
**increment 2** (generation once, at file creation; PR [#680](https://github.com/spencerduncan/redshipblueship/pull/680)) and now
**increment 3** in production: **the single-bag fill** (PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743), epic
[#645](https://github.com/spencerduncan/redshipblueship/issues/645)). One fill at the creation event places one bag of items across both
games' checks and proves the frozen goal over both games' logic at once. An item that
crosses leaves its home pool. The pieces it composes landed in waves 4-6: the coordinator
and both solver exports (PRs [#701](https://github.com/spencerduncan/redshipblueship/pull/701), [#714](https://github.com/spencerduncan/redshipblueship/pull/714), [#715](https://github.com/spencerduncan/redshipblueship/pull/715), [#717](https://github.com/spencerduncan/redshipblueship/pull/717)),
multiplicity and the surplus / filler / trap rules ([#728](https://github.com/spencerduncan/redshipblueship/pull/728)), the item
classification table ([#725](https://github.com/spencerduncan/redshipblueship/pull/725)), the monotonicity check ([#734](https://github.com/spencerduncan/redshipblueship/pull/734)), durable
crossings and the one spoiler ([#736](https://github.com/spencerduncan/redshipblueship/pull/736)), bag composition ([#738](https://github.com/spencerduncan/redshipblueship/pull/738)), one shared
triforce count ([#740](https://github.com/spencerduncan/redshipblueship/pull/740)) and the shared-quantity trim ([#744](https://github.com/spencerduncan/redshipblueship/pull/744)).

**What ships at `d6c8f270`:**

- **One seed, one paired world, one bag, items crossing in both directions.** Generating an
  OoT randomizer seed also generates the paired Majora's Mask world. OoT items are
  placed on MM checks and MM items on OoT checks, and **a crossed item is no longer also
  in its home pool** (PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743)). Crossings land only on chests (OoT: non-shop
  `EN_BOX` chests outside the treasure-chest game; MM: its Tier-A chests), at most 64 per
  side. Which items may cross is decided by a rule-defined item class (PR
  [#631](https://github.com/spencerduncan/redshipblueship/pull/631)) and the frozen direction.
- **Hearts and capacity upgrades are counted once for the pair.** Health is one shared
  bar. On the shipped profile the two pools hold 76 Pieces of Heart, 15 Heart Containers
  and 2 Double Defense; the bag keeps 44, 6 and 1 (pieces and containers OoT 14 + 4, MM 30 + 2;
  the Double Defense from either game), and the bar stops at 20 hearts (PR [#744](https://github.com/spencerduncan/redshipblueship/pull/744)). That is 42 trimmed heart and
  double-defense rows. Capacity families whose top tier is the same in both games (magic,
  quiver, bomb bag, and the wallet when OoT has no tycoon's wallet) are trimmed to the
  shared maximum the same way; with them, 51 rows (OoT 22, MM 29) become filler in their
  home game. Families whose top tiers differ keep every copy and cost a known dead pickup:
  the hookshot (OoT 2, MM 1) always, and the ocarina when the frozen rules arm it. Stick
  and nut capacity exist only in OoT and are unchanged.
- **Both worlds are generated together, once, at file creation.** Freeze, the single-bag
  fill, each game's own pass over its leftover checks (traps and junk stay in their home
  game), one spoiler with a `combo` section, one atomic identity publish, and the MM shadow
  armed last (PRs [#680](https://github.com/spencerduncan/redshipblueship/pull/680), [#743](https://github.com/spencerduncan/redshipblueship/pull/743)).
  Arrival in MM no longer generates anything: it hydrates the frozen shadow or
  refuses. A generation failure fails file creation itself, at file select, with
  no partial identity and no silent vanilla Termina fallback. A paired world's OoT
  spoiler loaded on its own at file select is refused with a toast.
- **Foreign items have their own identity.** An OoT item in Termina shows its real
  name and a coloured pickup toast; an MM item in Hyrule is presented as a native
  pickup (PRs [#524](https://github.com/spencerduncan/redshipblueship/pull/524),
  [#551](https://github.com/spencerduncan/redshipblueship/pull/551)).
- **The combo is one game, frozen at file creation.** MM's randomizer profile and
  the combo settings (direction, pool sizes, item classes) are frozen into the
  world's identity when the OoT file is created (ADR 0009, ADR 0011; PRs
  [#570](https://github.com/spencerduncan/redshipblueship/pull/570),
  [#628](https://github.com/spencerduncan/redshipblueship/pull/628)). Changing MM
  options or combo settings afterwards does not change the world — the divergence
  is **refused** when you next cross, and the save slot is marked refused rather
  than silently overwritten (PR
  [#568](https://github.com/spencerduncan/redshipblueship/pull/568)). Set MM's
  options *before* creating the file: `Combo → MM Randomizer` and `Combo → MM
  Tricks` (menu pages since 2026-09-27; before that a pop-out window opened from
  `Combo → Windows`, and before PR
  [#745](https://github.com/spencerduncan/redshipblueship/pull/745) from
  Randomizer → Cross-Game).
- **The pair is generated with logic set to Glitchless** by default, and the fill proves
  the frozen goal (`beat-both`, Ganon & Majora, by default) over both games' logic.
  It runs behind a deterministic attempt ladder with a host-calibrated budget (~30 s
  floor, ~90 s ceiling; PRs [#680](https://github.com/spencerduncan/redshipblueship/pull/680), [#743](https://github.com/spencerduncan/redshipblueship/pull/743)). On the development
  workstation 30 real creations took 5.9-13.7 s, each on its first attempt. **A progress
  overlay paints during file creation** ("Placing item n of N in both worlds"; PRs
  [#707](https://github.com/spencerduncan/redshipblueship/pull/707), [#749](https://github.com/spencerduncan/redshipblueship/pull/749), [#582](https://github.com/spencerduncan/redshipblueship/issues/582)): the window stays responsive and menu
  input is suppressed while it paints.
- **The Happy Mask Shop door is never shuffled.** It is the OoT ↔ MM crossing, so
  OoT's own entrance randomizer now leaves both directions of it out of every
  shuffle pool; under interior (or any) entrance shuffle the door you walk
  through is still the door to Termina (PR [#691](https://github.com/spencerduncan/redshipblueship/pull/691), [#661](https://github.com/spencerduncan/redshipblueship/issues/661)).
- **Paired MM worlds can enable individual Majora's Mask logic tricks.** MM now
  has a per-trick vocabulary (86 trick keys, frozen into the world's identity
  like OoT's). **37** of them are bound to a real logic edge, 20 are reserved for an
  Ocarina of Time item and 29 are recorded as not bindable (PRs
  [#686](https://github.com/spencerduncan/redshipblueship/pull/686), [#696](https://github.com/spencerduncan/redshipblueship/pull/696), [#703](https://github.com/spencerduncan/redshipblueship/pull/703), [#713](https://github.com/spencerduncan/redshipblueship/pull/713), [#729](https://github.com/spencerduncan/redshipblueship/pull/729), [#763](https://github.com/spencerduncan/redshipblueship/pull/763); [#578](https://github.com/spencerduncan/redshipblueship/issues/578)). Every trick is off by default.
  **One of them is Deku-Stick combat** (PR [#729](https://github.com/spencerduncan/redshipblueship/pull/729), [#719](https://github.com/spencerduncan/redshipblueship/issues/719)). MM's logic used to assume you
  could fight 17 kinds of enemy with a Deku Stick, even though the matching trick is off by default.
  Those enemies now need `Deku Stick Fighting` enabled, or another weapon. Using the stick as a fire
  source is unchanged. None of the pinned test worlds moved. A
  trick whose edge is not bound draws disabled-with-reason on Combo → MM
  Tricks rather than enabled-and-inert ([#697](https://github.com/spencerduncan/redshipblueship/issues/697) closed with PR [#763](https://github.com/spencerduncan/redshipblueship/pull/763)). PR #763 also
  follows OoTMM on two tightenings, so with the tricks off logic kills Twinmold only with
  the Giant's Mask, magic and a sword ("Twinmold with Bow (MM)" admits the Bow), and the
  West Clock Town bank's heart piece needs the Giant's Wallet ("Bank Rewards Require One
  Less Wallet" admits the Adult Wallet).
- **You may opt into one shared Ocarina across both games** — off by default,
  frozen at file creation like every other combo rule: obtaining an ocarina in
  either game grants it in the other (PR
  [#675](https://github.com/spencerduncan/redshipblueship/pull/675)).
- **You choose the goal, and meeting it ends the paired game.** Combo → Cross-Game
  Rules → "[Both Games] Goal" offers OoTMM's goals: Ganon & Majora (the default), Any
  Final Boss, Ganon and Majora. A goal that leaves a half unproved says so with a toast
  at creation ("Not proven: Majora's Mask may be unfinishable."; PR [#760](https://github.com/spencerduncan/redshipblueship/pull/760)). The
  ending plays only in the game whose final-boss defeat meets the goal; a final boss
  beaten earlier is recorded and puts you back in its game, in Ganon's Tower or at a new
  cycle in South Clock Town (PR [#769](https://github.com/spencerduncan/redshipblueship/pull/769)). A half's own triforce hunt no longer ends a
  paired world under a boss goal, and OoT's own "Win" mode is generated as "Ganon's
  Boss Key" there (PR [#775](https://github.com/spencerduncan/redshipblueship/pull/775)).
- **Every randomizer file is a paired file.** Every OoT randomizer generation is a
  paired creation, so a single-game OoT randomizer file cannot be created (PR [#775](https://github.com/spencerduncan/redshipblueship/pull/775)).

**What does not ship yet:**

- **Nothing of the switch has been played yet.** Every claim above comes from tests,
  measurements and UI captures. The playtest is
  [`playtest-2026-09.md`](playtest-2026-09.md).
- **The in-game crossing views lag Majora's Mask.** Combo > Windows > Toggle Cross-Game
  Spoiler and the Combo Tracker list every crossing in both directions, by name, with
  whether each host check was collected. That state comes from each game's own save, and
  Majora's Mask's is read as of the last game switch or save (as of file creation until
  Majora's Mask is first entered), so a crossing collected in
  Majora's Mask shows as collected after the next save or switch (the note under each
  list says so; #755, #757).
- **Paired-world hints are partial.** OoT's hints have no pair-level Way of the Hero
  or barren analysis. Crossing hosts are never hinted, and an OoT item that crossed is
  hinted as "Termina" (PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743)).
- **Triforce Hunt is not yet a paired goal.** The Goal row lists it, but generation
  refuses it, and OoTMM's Triforce Quest is not offered (PRs [#740](https://github.com/spencerduncan/redshipblueship/pull/740), [#760](https://github.com/spencerduncan/redshipblueship/pull/760), [#775](https://github.com/spencerduncan/redshipblueship/pull/775)).
- **Some settings re-seed the world without changing a rule.** The pool-size sliders and
  every item class other than Progression are read by no rule since the switch, but they
  are part of the world's fingerprint, so changing one gives a different world from the
  same seed (PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743)).
- **Surplus filler can be dropped** when more MM items land in Hyrule than OoT items
  leave; only filler gives way (PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743)).
- **Netplay and crossings share one 64-slot shared-item array.** Crossings alone cannot
  fill it, but a netplay peer's grants take slots too, so a pickup can still be refused
  (loudly) while both happen (PR [#743](https://github.com/spencerduncan/redshipblueship/pull/743)).
- **Generation can still abort**, at file creation. A wall-clock stop fails the creation
  with a toast ("Not created: try a new seed or Majora's Mask options."). No creation in
  the 30-seed sample needed more than two fill batches, which bounds the failure rate
  below about 10%, not at zero. No partial or corrupt file is left behind.
- **Some MM randomizer options are disabled-with-reason** on Combo → MM Randomizer:
  their gameplay hooks are not yet dispatched in the single-executable build
  ([#438](https://github.com/spencerduncan/redshipblueship/issues/438), 14 of 23
  hook types remain). The page says which and why; an option that is enabled and
  does nothing is a bug worth reporting.
- **MM's enhancement toggles live on Combo → Majora's Mask** (named MM Enhancements until 2026-09-27). The curated MM
  enhancement toggles — the game-over prompt, `BetterSongOfDoubleTime`,
  `SkipSoTCutscenes`, a pointer to the shared `Autosave` checkbox on OoT's
  Enhancements page, and (since PR [#730](https://github.com/spencerduncan/redshipblueship/pull/730), [#693](https://github.com/spencerduncan/redshipblueship/issues/693)) MM's own autosave interval slider
  (`gEnhancements.Saving.AutosaveInterval`, 1–60 minutes, default 5, shown once
  Autosave is on) — are hosted there (PR [#695](https://github.com/spencerduncan/redshipblueship/pull/695), [#682](https://github.com/spencerduncan/redshipblueship/issues/682)). The slider sets
  Majora's Mask's interval only; Ocarina of Time's autosave interval is a fixed
  3 minutes.

### Back up your saves. Seriously.

The cross-game save (`.redsave`) format **has been re-versioned** since the last
revision of this document — it is now version 3 (durable crossings, PR
[#736](https://github.com/spencerduncan/redshipblueship/pull/736)), and this build reads versions 1 and 2 too (`src/common/save.h`). The format has grown several times since July as the
combo context gained its identity, commit and settings records. A refused or
corrupt `.redsave` is now quarantined with a reason rather than overwritten
([#533](https://github.com/spencerduncan/redshipblueship/issues/533), PR
[#568](https://github.com/spencerduncan/redshipblueship/pull/568)), and every
durable write goes through one commit point with a generation stamp (PR
[#569](https://github.com/spencerduncan/redshipblueship/pull/569)). None of that
is a promise that the *next* format change will migrate. Treat any progress made on
a pre-alpha build as disposable, and keep copies of files you care about.

**Paired files created before PR #680 (2026-09-17) are refused, not migrated.**
Increment 2 moved the entire paired generation to the file-create seam; a save
whose pair never crossed that seam has no frozen identity to hydrate from and is
refused on load rather than silently re-generated. This project is pre-release —
the operator has accepted invalidating existing saves rather than spending effort
on migration. **If a paired file from before this build is refused, create a new
file**; there is no recovery path for the old one.

**Paired files created between PR #680 and the switch (PR #743, 2026-09-27) are NOT
refused.** They load and keep playing the world they were created with: the old
overlay world, where a crossed item is also still in its home pool. They show nothing
of the single-bag fill, and the same seed now generates a different world. Create a new
file to play the current build.

**Paired files created before PR #772 (2026-09-28) are NOT refused, and are safe to
play.** Every arrival in MM now gives the session the cross-game slot number, so the
moon-crash reset and the owl save no longer wipe their MM half (PR [#772](https://github.com/spencerduncan/redshipblueship/pull/772)). What stays
wrong on such a file is display only: the file-select slot shows `[MM _]`, and the Combo
Tracker's Majora's Mask panel shows no MM data. A new file shows `[MM v]`.

**A seed string does not make the world it made on an older build.** PR [#763](https://github.com/spencerduncan/redshipblueship/pull/763)'s two
trick tightenings moved generated worlds, and since PR [#774](https://github.com/spencerduncan/redshipblueship/pull/774) the OoT half of every
newly generated world differs from what a build before it made from the same seed string
and settings (OoT's excluded locations now reach the settings fingerprint, in every
environment). A non-default goal is a different world from the same seed (PR [#760](https://github.com/spencerduncan/redshipblueship/pull/760)).
Nothing stored in an existing save is recomputed, so existing files are unaffected.

---

## Save loss and corruption

### ~~A flag set in the scene you leave through the portal can be lost~~ — RESOLVED ([#635](https://github.com/spencerduncan/redshipblueship/issues/635), community report; tracked in [#638](https://github.com/spencerduncan/redshipblueship/issues/638), PR [#650](https://github.com/spencerduncan/redshipblueship/pull/650))

Fixed by PR #650 (2026-09-11). Live scene flags are now flushed before every departure freeze, in
both games and on both switch paths. The original report, kept for matching old saves:

Collect the Heart Piece on the Clock Tower, walk straight out through the portal to
Ocarina of Time, come back: the Heart Piece is there again, and can be collected
again. The cross-game departure freezes MM's save **before** the scene-flag flush
that a normal scene transition performs, and that flush never runs — so anything
you picked up or triggered in the scene you left through the portal (collectibles,
chests, switches, in that scene only) is not in the frozen save. Progress in every
*other* scene is safe.

On builds older than PR #650, the workaround was to leave and re-enter the scene through any door
or loading zone before crossing.

### ~~F10 during MM's game-over screen hands over an empty health bar~~ — RESOLVED ([#626](https://github.com/spencerduncan/redshipblueship/issues/626), PR [#650](https://github.com/spencerduncan/redshipblueship/pull/650))

Fixed by PR #650 (2026-09-11). A dead MM health bar is now revived before every departure freeze.
The original report:

Health is a shared resource between the two games, and it is applied as-is on
arrival. Pressing the F10 debug hot-swap while MM's game-over prompt is up freezes
the save with zero health and switches; you arrive in OoT with an empty bar. The
normal game-over exit (choosing not to continue) already revives you on the way out
(PR [#625](https://github.com/spencerduncan/redshipblueship/pull/625)); the F10
route bypasses it.

On builds older than PR #650, the workaround was not to press F10 on the game-over screen.

### ~~F10 during OoT's game-over screen hands MM a one-heart bar~~ — RESOLVED ([#664](https://github.com/spencerduncan/redshipblueship/issues/664), PR [#753](https://github.com/spencerduncan/redshipblueship/pull/753))

Fixed by PR #753 (2026-09-27), the OoT twin of #650's revive. A dead OoT bar is revived before
the freeze to OoT's own continue value: three hearts, or full capacity with "Spawn with Full
Health". If a bottled fairy was spent at the killing blow, both games give the fairy's refill
instead (OoT 20 hearts, MM 10, clamped to capacity). Before the fix MM arrived with one heart,
its arrival floor. A pre-release OoT half already frozen dead stays dead in its blob, and OoT's
arrival floors it at one heart.

### ~~MM's paired half could be wiped by the moon crash or an owl save~~ — RESOLVED ([#765](https://github.com/spencerduncan/redshipblueship/issues/765), PR [#772](https://github.com/spencerduncan/redshipblueship/pull/772))

Fixed by PR #772 (2026-09-28). The creation event left MM's half with slot number 0, a real
flash slot, so the moon-crash reset and the owl save's readback copied an empty save buffer
over the live one (observed in a headless row: day, rupees and inventory wiped). The half is
now authored as MM's own new-file path authors it, and every arrival pins the cross-game slot
number, which also covers older files (see "Back up your saves"). The fix is locked
headlessly and has not been played; the playtest guide's scenario I4 checks it in game.

### ~~F10 hot-swap silently rolls back your progress~~ — RESOLVED ([#364](https://github.com/spencerduncan/redshipblueship/issues/364), PR [#400](https://github.com/spencerduncan/redshipblueship/pull/400))

F10 now freezes the departing game's state and sets a return entrance, the same
as the portal switch, and the frozen state is cleared when consumed. The historical
symptom — resuming from the *other* game's save bytes after an F10 switch — is
gone. The game-over case above is fixed too.

### ~~A malformed save permanently deadlocks all saving~~ — RESOLVED ([#370](https://github.com/spencerduncan/redshipblueship/issues/370), PR [#391](https://github.com/spencerduncan/redshipblueship/pull/391))

`saveMtx` is released on the exception path and the file is no longer truncated
before the write that could throw.

### ~~Cross-game save format will change~~ — it did; see "Back up your saves"

The `reserved` padding, the version window and the refused state that entry asked
for all exist now. The advice stands: the format may change again.

---

## Crashes and hangs

### ~~Opening Network → Anchor in a wide window displaces the game view~~ — RESOLVED ([#634](https://github.com/spencerduncan/redshipblueship/issues/634), community report; tracked in [#640](https://github.com/spencerduncan/redshipblueship/issues/640), PR [#651](https://github.com/spencerduncan/redshipblueship/pull/651))

Fixed by PR #651 (2026-09-11). `soh_port` now links whole-archive, so the page's registrar is kept,
and a menu page with no widgets consumes its own window position. The original report:

On Windows, open the menu, go to `Network → Anchor` while the window is wider than
roughly 800 px (maximized, for example): the page's content column is empty, and
when you close the menu the game view is pushed off to a black rectangle. The page
has no widgets because its menu registrar is dropped by the linker from the
`soh_port` archive, and the empty page still positions the main game window. Not
specific to fullscreen or DirectX 11.

On builds older than PR #651, the workaround was to narrow the window first or avoid the page.

### ~~Every normal exit heap-corrupts on Windows (Fault A)~~ — RESOLVED ([#396](https://github.com/spencerduncan/redshipblueship/issues/396))

**Fixed and operator-confirmed 2026-07-21.** `redship --version` now exits cleanly
with status `0`; the heap corruption on normal exit is gone.

Historical detail, for anyone reading old crash reports: Windows builds used to die
with `STATUS_HEAP_CORRUPTION` (`0xC0000374`, detected inside `ntdll`) during **normal
process exit**, the minimal repro being `redship --version`. The root cause was not
the `/FORCE:MULTIPLE` CRT duplication originally suspected: `OTRExporter/Main.cpp` and
`VersionInfo.cpp` were compiled into **both** the `OTRExporter_OoT` and `OTRExporter_MM`
static libs, so seven-plus global objects were constructed twice and destroyed twice —
the second destructor walking freed heap. The fix (PR
[#413](https://github.com/spencerduncan/redshipblueship/pull/413), submodule pointer
bump) namespaces the exporter globals per variant. A permanent strong-DATA-symbol CI
gate over the two exporter archives (PR
[#430](https://github.com/spencerduncan/redshipblueship/pull/430)) prevents the class
from recurring.

If you ever see `0xC0000374` on exit again, it is a **new** regression, not this one —
start from the exporter-archive symbol gate.

### ~~Crashes in CI/headless mode present as a 180-second hang~~ — RESOLVED ([#388](https://github.com/spencerduncan/redshipblueship/issues/388), PR [#394](https://github.com/spencerduncan/redshipblueship/pull/394))

The crash handler no longer blocks in `SDL_ShowSimpleMessageBox` without a display;
headless crashes fail fast with an exit code.

### ~~Audio subsystem can wedge across a switch~~ — RESOLVED ([#365](https://github.com/spencerduncan/redshipblueship/issues/365) PR [#416](https://github.com/spencerduncan/redshipblueship/pull/416); [#371](https://github.com/spencerduncan/redshipblueship/issues/371) / [#378](https://github.com/spencerduncan/redshipblueship/issues/378) PR [#398](https://github.com/spencerduncan/redshipblueship/pull/398); [#377](https://github.com/spencerduncan/redshipblueship/issues/377) PR [#412](https://github.com/spencerduncan/redshipblueship/pull/412))

OoT's audio buffer fill is guarded on its initialized flag, the sequence maps are
zero-initialized and bounds-checked before registration, and the audio probe observes
the reset handshake instead of bypassing it. If you still get silence or a hang after
a switch with custom music installed, it is a new report.

---

## Majora's Mask specific

MM is the newer half of the combo and still carries more debt than OoT.

### Dying in MM respawns you at the entrance instead of showing a game-over prompt — this is intentional

MM's vanilla death behaviour (reload at the area entrance with three hearts, no
"Continue?" prompt) is the combo's shipped default; it is not a regression. The
2ship game-over prompt enhancement (`gEnhancements.Kaleido.GameOver`) is off by default.
Since PR [#695](https://github.com/spencerduncan/redshipblueship/pull/695) ([#682](https://github.com/spencerduncan/redshipblueship/issues/682)) you can turn it on from Combo → Majora's Mask.
ADR 0009's death-decline autosave machinery and #626's F10-during-game-over case
only apply once that enhancement is enabled — operator ruling, 2026-09-16
([#653](https://github.com/spencerduncan/redshipblueship/issues/653)).

### ~~First arrival in Clock Town reads 08:00 instead of 06:00~~ — RESOLVED ([#636](https://github.com/spencerduncan/redshipblueship/issues/636), community report; tracked in [#639](https://github.com/spencerduncan/redshipblueship/issues/639), PR [#648](https://github.com/spencerduncan/redshipblueship/pull/648))

Fixed by PR #648 (2026-09-11). A first MM arrival now re-authors the new-file clock, so the dawn
sequence runs. The original report:

The first crossing from OoT into a new MM file lands in South Clock Town at Day 1,
8:00 AM, with no dawn sequence, instead of a new file's 6:00 AM. No time value
leaks from OoT: the 8:00 is MM's own title-screen attract-demo clock, which the
first-entry path never re-authors. You lose two hours of the first day and the
dawn telop; nothing else is wrong with the clock.

### ~~MM enhancements do not initialize in single-executable builds~~ — RESOLVED ([#384](https://github.com/spencerduncan/redshipblueship/issues/384) PR [#408](https://github.com/spencerduncan/redshipblueship/pull/408); [#516](https://github.com/spencerduncan/redshipblueship/issues/516) PRs [#518](https://github.com/spencerduncan/redshipblueship/pull/518), [#520](https://github.com/spencerduncan/redshipblueship/pull/520), [#616](https://github.com/spencerduncan/redshipblueship/pull/616))

The colliding registrar names were prefixed `MM_`, and the registrars that MM's
excluded boot sequence used to run are now called explicitly from the single-exe
boot path, with a link-time audit (`.github/scripts/check-registrar-elision.sh`)
and a runtime lock over the list. MM's enhancement archive still links as a plain
archive by design, so an MM toggle that appears enabled and does nothing is still a
plausible bug — report it.

### ~~MM code binds to OoT implementations in several places~~ — RESOLVED

Actor culling ([#382](https://github.com/spencerduncan/redshipblueship/issues/382),
PR [#402](https://github.com/spencerduncan/redshipblueship/pull/402)), framebuffer
effects ([#386](https://github.com/spencerduncan/redshipblueship/issues/386), PR
[#436](https://github.com/spencerduncan/redshipblueship/pull/436)), the C++ layout
mismatches ([#383](https://github.com/spencerduncan/redshipblueship/issues/383), PR
[#434](https://github.com/spencerduncan/redshipblueship/pull/434)) and the stub
signature drift ([#372](https://github.com/spencerduncan/redshipblueship/issues/372)
PR [#415](https://github.com/spencerduncan/redshipblueship/pull/415);
[#385](https://github.com/spencerduncan/redshipblueship/issues/385) PR
[#401](https://github.com/spencerduncan/redshipblueship/pull/401)) each got MM its own
definition. A symbol-collision baseline and a strong-symbol gate in CI keep the class
from returning.

### ~~Timers are not neutralized on the MM side of a switch~~ — RESOLVED ([#373](https://github.com/spencerduncan/redshipblueship/issues/373), PR [#419](https://github.com/spencerduncan/redshipblueship/pull/419))

### MM text that says the player's name shows blanks in a paired world — [#773](https://github.com/spencerduncan/redshipblueship/issues/773)

The paired creation does not carry OoT's typed name into Majora's Mask's character set, so
MM's half keeps the default all-space name, so an MM textbox that prints the player's name
is expected to show blanks (PR [#772](https://github.com/spencerduncan/redshipblueship/pull/772)). No such textbox has been looked at in a
paired half ([#773](https://github.com/spencerduncan/redshipblueship/issues/773), "Not verified"). The file-select slot still shows OoT's name.

### MM hook dispatch is still partial — [#438](https://github.com/spencerduncan/redshipblueship/issues/438)

14 of MM's 23 game-hook types have no dispatch point in the single-exe build.
Randomizer options and enhancements that depend on those hooks are shown
disabled-with-reason on the MM randomizer page rather than silently doing nothing.
The actor-init, actor-draw and open-text hooks (PR
[#512](https://github.com/spencerduncan/redshipblueship/pull/512)), the pause-menu
and file-select hooks (PR [#547](https://github.com/spencerduncan/redshipblueship/pull/547))
and the item/progression trio (PR
[#630](https://github.com/spencerduncan/redshipblueship/pull/630)) now dispatch.

---

## Switching and entrances

### ~~MM's clock-shuffle randomizer option was unreachable~~ — RESOLVED ([#678](https://github.com/spencerduncan/redshipblueship/issues/678), PR [#679](https://github.com/spencerduncan/redshipblueship/pull/679))

`RO_CLOCK_SHUFFLE` is now live and reachable on Combo → MM Randomizer. The two
`2ship_enh` translation units it depends on (`BetterSongOfDoubleTime.cpp`,
`SkipSoTCutscenes.cpp`) were dropped by plain archive linking because they
register only through a file-scope `RegisterShipInitFunc`; a targeted
`WHOLE_ARCHIVE` carve-out links and guards both, and `check-registrar-elision.sh`
now locks the registrars in. This is not a general `2ship_enh` link change
([#427](https://github.com/spencerduncan/redshipblueship/issues/427) item 3
stays a separate decision).

### ~~Test and default entrance links collide~~ — RESOLVED ([#374](https://github.com/spencerduncan/redshipblueship/issues/374), PR [#397](https://github.com/spencerduncan/redshipblueship/pull/397))

Duplicate entrance-link registrations are rejected instead of silently shadowing.

### ~~An entrance bound is an unchecked literal~~ — RESOLVED ([#380](https://github.com/spencerduncan/redshipblueship/issues/380), PR [#417](https://github.com/spencerduncan/redshipblueship/pull/417))

### ~~Leaving a game mid-session leaves its actors' state behind~~ — RESOLVED ([#666](https://github.com/spencerduncan/redshipblueship/issues/666) PR [#751](https://github.com/spencerduncan/redshipblueship/pull/751); [#750](https://github.com/spencerduncan/redshipblueship/issues/750) PR [#767](https://github.com/spencerduncan/redshipblueship/pull/767))

A departure through the portal or with F10 retires the game's play state without running each
live actor's teardown. Both games now retire that session at departure: every overlay that
still had actors is reset once, and per-actor extension data (such as a rando pot's check
identity) is dropped. Symptoms this fixes, from reading the code: MM's three-day clock actor
killing itself on the next arrival (no night, dawn or moon crash until a scene change), a
skipped "Dawn of the First Day" on a second file, and Romani Ranch's alien defense pointing at
actors from the discarded session; in OoT, a missing Zora diving-game Zora, Spirit and Forest Temple lifts and water, and the Treasure
Chest Shop keeper for the rest of the process. Each departure logs `[OoT] Abandoned session
retired: ...` or `[MM] ...`.

### OoT actor teardown writes to the save are skipped at a departure — [#770](https://github.com/spencerduncan/redshipblueship/issues/770)

Still open. The teardowns PR #767 does not run also write the save: a running room or
minigame timer's `timerState`, Sun's Song state,
a magic effect's `magicState`, `linkAge`, and three flags (the windmill Song of Storms, Lake
Hylia's raised water, a Light-Arrow sun switch). Whether re-entry already normalises each one
is not checked. State cleared only by destroy hooks is not retired either: a remote bombchu's
camera focus and the entrance-randomizer Epona state (PRs [#751](https://github.com/spencerduncan/redshipblueship/pull/751), [#767](https://github.com/spencerduncan/redshipblueship/pull/767)).

---

## Randomizer generation

### ~~OoT's plentiful item pool can wrap the wallet inside the fill's own logic~~ — RESOLVED ([#726](https://github.com/spencerduncan/redshipblueship/issues/726))

OoT's own fill now stops every progressive grant at the item's top tier, as the combo coordinator's
rounds already did. Before the fix, a **Plentiful** pool (one more wallet, bow, slingshot, bomb bag,
strength, scale and magic than balanced) walked the fill's simulated inventory past the top: on a
plentiful + tycoon-wallet seed the wallet read 0 after the fourth copy (observed), and every other
of those items read one tier past its top. `OoTPlentifulProgressive` (rando tier) locks it.

Plentiful worlds can differ from before the fix when the wrap happened mid-fill; balanced, scarce
and minimal worlds, and the shipped default, do not move (the golden rows are unchanged).

---

## Platform and packaging

### macOS is not supported

The macOS CI build is **disabled** (`.github/workflows/generate-builds.yml`,
`build-macos`) due to an unresolved linker issue. No macOS artifacts are produced.
Windows and Linux only.

### ~~MM mods and texture packs never load~~ — RESOLVED ([#670](https://github.com/spencerduncan/redshipblueship/issues/670), PRs [#704](https://github.com/spencerduncan/redshipblueship/pull/704), [#716](https://github.com/spencerduncan/redshipblueship/pull/716))

MM now mounts its asset mods in the single executable. Both games share one
`mods/` folder: the root is OoT's and `mods/mm/` is MM's, each searched
recursively, and a mod under one partition is never registered for the other
game. Layout and precedence are in `docs/MODDING.md`.

**Loose (unpacked) asset folders now mount for both games** (PR [#732](https://github.com/spencerduncan/redshipblueship/pull/732), [#705](https://github.com/spencerduncan/redshipblueship/issues/705)).
Put OoT's loose files in `<mods>/loose` and MM's in `<mods>/mm/loose`. Each game mounts its loose
folder after its packed mods, so a loose file wins over a packed one.

**MM's mods have their own order and enabled set** (PR [#764](https://github.com/spencerduncan/redshipblueship/pull/764), [#706](https://github.com/spencerduncan/redshipblueship/issues/706)): Combo → MM Mods
lists the archives under `mods/mm`, highest priority on top, with arrows to reorder, disable
and enable, and a Rescan button. Changes are saved at once and apply when MM next mounts its
mods: its first start in the session, or after a restart once it has loaded them (the page's
note says which). A scan that cannot see the whole folder leaves the list read-only.

Limits, all described in `docs/MODDING.md`:

- Symlinked subfolders inside `loose/` are not followed.
- Removing a file needs a restart.
- A top-level `version` file must be left out.
- While you are in MM, an OoT loose file is shadowed wherever MM's base archives ship the same path.

### You must supply your own ROMs

Asset extraction requires original Ocarina of Time and Majora's Mask ROMs. No ROMs
or extracted archives are distributed. See `docs/BUILDING.md` and
`docs/mm-archive-setup.md`.

### Config and settings are still Ship-of-Harkinian-shaped

The config file is still named `shipofharkinian.json` and lives in the `soh`
directory (`rsbs/src/main.cpp`). This is deliberate — it keeps existing Ship configs
working. The settings-migration issue
([#34](https://github.com/spencerduncan/redshipblueship/issues/34)) closed with PR
[#461](https://github.com/spencerduncan/redshipblueship/pull/461), which converged
MM's volume and tunic keys onto OoT's and added a config updater; the namespace
decision is ADR 0003 (`docs/adr/0003-settings-namespace.md`). A standalone 2Ship
install's config is not read.

### Some menu entries are stubs

Settings entries backed by unimplemented functionality are grayed out or labeled
where they were caught; the MM randomizer options page labels each row live,
partial, dormant or generation-only with a reason. This pass was not exhaustive —
an enabled-looking toggle that does nothing is a plausible bug, and worth reporting.

### The "Fipps" overlay font choice is gone

`Fipps-Regular.otf` shipped with the inherited port trees and asserted "All rights
reserved" with no license grant, so it was deleted (2026-09-21; see
`THIRD_PARTY_NOTICES.md`). It is no longer offered in the overlay font picker. It
was never the default, and a saved overlay-font selection that still names it — or
names anything else that is not loaded — falls back to **Press Start 2P** rather
than leaving a dead entry in the picker.

**Your saved selection is rewritten, once.** The setting is
`gSettings.OverlayFont` in `shipofharkinian.json` (older configs spell it
`gOverlayFont`; the game's own config migration renames it for you). Because the
fallback is a font that IS loaded, the first launch after this change stores
`Press Start 2P` over whatever the key used to say and flushes the config, so a
selection of the removed font is not kept for later. That is deliberate: the
alternative left the overlay drawn in ImGui's built-in face instead of a font this
project ships. No save file is touched — this is a settings key, not save data.

---

## CI and quality gates (for contributors)

The two gates that once **could not fail** — the more dangerous state, because
they read as coverage — are armed:
[#375](https://github.com/spencerduncan/redshipblueship/issues/375)
(`check-symbol-collisions.sh` had no committed baseline) and
[#376](https://github.com/spencerduncan/redshipblueship/issues/376) (orphaned
CTest labels, missing `--no-tests=error`, an unfailable `check-archives`, a
frame-budgeted watchdog; PRs [#409](https://github.com/spencerduncan/redshipblueship/pull/409),
[#437](https://github.com/spencerduncan/redshipblueship/pull/437)). Both are closed.

Duplicate-symbol bugs are only observable on Linux (Windows links with
`/FORCE:MULTIPLE`); `.github/workflows/link-check.yml` gives every push to a
`claude/**` branch a fast Linux link plus the three nm gates
([#387](https://github.com/spencerduncan/redshipblueship/issues/387), PR
[#548](https://github.com/spencerduncan/redshipblueship/pull/548)).

Two rows to know about before you read a red or green as a signal:

- **`build-windows` now runs both test tiers.** It used to run none ([#623](https://github.com/spencerduncan/redshipblueship/issues/623)).
  PR [#642](https://github.com/spencerduncan/redshipblueship/pull/642) added the `redship` tier, and since PR [#723](https://github.com/spencerduncan/redshipblueship/pull/723) ([#709](https://github.com/spencerduncan/redshipblueship/issues/709)) it also runs the whole
  `rando` tier, every golden row included. The `rando` tier adds about a minute.
  The first Windows run after a runner-image or compiler change rebuilds the sccache from zero and
  takes far longer. That is a cold cache, not a regression.
- **`IntSwitchOoTHmsToMm` always times out unattended** — it waits on the
  file-select hook, which needs a Start press the harness never sends
  ([#544](https://github.com/spencerduncan/redshipblueship/issues/544)). It is
  not a regression signal; it has never proved what its name claims. The other
  integration rows are real.

Also: clang-format is enforced against an incremental allowlist
(`.github/clang-format-paths.txt`), not the full tree
([#369](https://github.com/spencerduncan/redshipblueship/issues/369), open).

---

## Tracking

- [#492](https://github.com/spencerduncan/redshipblueship/issues/492) — Phase 3.1
  tracker (two-way combo randomizer; closing)
- [#500](https://github.com/spencerduncan/redshipblueship/issues/500) — Phase 3.2
  tracker (cross-game logic and beatability), with its increment epics
  [#644](https://github.com/spencerduncan/redshipblueship/issues/644) and
  [#645](https://github.com/spencerduncan/redshipblueship/issues/645)
- [#310](https://github.com/spencerduncan/redshipblueship/issues/310) — manual
  runtime QA pass (open); findings should be appended to this document rather than
  tracked separately
- Closed trackers, for history: [#321](https://github.com/spencerduncan/redshipblueship/issues/321)
  (pre-alpha v0.1.0 readiness), [#381](https://github.com/spencerduncan/redshipblueship/issues/381)
  (Phase 2 review follow-ups), [#392](https://github.com/spencerduncan/redshipblueship/issues/392)
  (Phase 3.0)
- Full open list: `gh issue list -R spencerduncan/redshipblueship`
