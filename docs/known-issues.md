# Known issues

**Applies to:** `main` at `c8947177` (2026-09-06) and the GitHub Actions builds cut from
it; the `v0.1.1-prealpha` tag (2026-07-03) is older than everything in the first section.
**Last updated:** 2026-09-10.

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

The headline feature is real but partial. Phase 3.1 (tracker
[#492](https://github.com/spencerduncan/redshipblueship/issues/492)) shipped; Phase
3.2 — cross-game *logic* — has not started
([#500](https://github.com/spencerduncan/redshipblueship/issues/500)).

**What ships at `c8947177`:**

- **One seed, one paired world, items crossing in both directions.** Generating an
  OoT randomizer seed also generates a paired Majora's Mask world; OoT items are
  placed in MM checks and MM items in OoT checks. Which items may cross is decided
  by a **rule-defined item class**, not a hand-written list (PR
  [#631](https://github.com/spencerduncan/redshipblueship/pull/631)); the reverse
  direction is armed and delivered (PR
  [#632](https://github.com/spencerduncan/redshipblueship/pull/632)).
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
  options *before* creating the file: `Randomizer → Cross-Game → Toggle MM
  Randomizer Options`.
- **The paired MM world is generated with logic set to Glitchless** by default,
  behind a deterministic attempt ladder, and foreign items are only hosted on
  checks the MM crawl can reach (PRs
  [#580](https://github.com/spencerduncan/redshipblueship/pull/580),
  [#581](https://github.com/spencerduncan/redshipblueship/pull/581)). Each half is
  beatable on its own terms.

**What does not ship yet:**

- **There is no cross-game logic.** Nothing proves that an item you need in one
  game is not locked behind a check in the other game that needs that same item.
  The **spoiler log carries that burden**: read it (`Randomizer → Cross-Game →
  Toggle Cross-Game Spoiler`, or the JSON next to your OoT spoiler) before you
  commit to a route. This is ADR 0010's territory — epics
  [#644](https://github.com/spencerduncan/redshipblueship/issues/644) (merged
  generation) and [#645](https://github.com/spencerduncan/redshipblueship/issues/645)
  (the single-bag fill with a beatability proof).
- **Crossings are still duplicates.** An item that crosses is *also* still in its
  home game's pool. Items leave origin pools only with #645.
- **Generation can abort.** MM's Glitchless fill has a fixed 10-second wall-clock
  budget; on a slow machine, a heavy MM profile can exhaust the attempt ladder and
  the paired world is refused. Today that refusal surfaces when you first cross
  into MM, because the MM half is still generated on arrival, not at file creation.
  The product decision (a ~30 s floor, an adaptive per-attempt budget, and a
  visible progress surface) is recorded on
  [#582](https://github.com/spencerduncan/redshipblueship/issues/582) and ships
  with #644. If your first crossing lands you on a refusal toast, the fix is to
  create a new file, ideally with a lighter MM profile.
- **Some MM randomizer options are disabled-with-reason** in the MM options pane:
  their gameplay hooks are not yet dispatched in the single-executable build
  ([#438](https://github.com/spencerduncan/redshipblueship/issues/438), 14 of 23
  hook types remain). The pane says which and why; an option that is enabled and
  does nothing is a bug worth reporting.

### Back up your saves. Seriously.

The cross-game save (`.redsave`) format **has been re-versioned** since the last
revision of this document — it is now version 2, and this build reads version 1
files too (`src/common/save.h`). The format has grown several times since July as the
combo context gained its identity, commit and settings records. A refused or
corrupt `.redsave` is now quarantined with a reason rather than overwritten
([#533](https://github.com/spencerduncan/redshipblueship/issues/533), PR
[#568](https://github.com/spencerduncan/redshipblueship/pull/568)), and every
durable write goes through one commit point with a generation stamp (PR
[#569](https://github.com/spencerduncan/redshipblueship/pull/569)). None of that
is a promise that the *next* format change will migrate. Treat any progress made on
a pre-alpha build as disposable, and keep copies of files you care about.

---

## Save loss and corruption

### A flag set in the scene you leave through the portal can be lost — [#635](https://github.com/spencerduncan/redshipblueship/issues/635) (community report; tracked in [#638](https://github.com/spencerduncan/redshipblueship/issues/638))

Collect the Heart Piece on the Clock Tower, walk straight out through the portal to
Ocarina of Time, come back: the Heart Piece is there again, and can be collected
again. The cross-game departure freezes MM's save **before** the scene-flag flush
that a normal scene transition performs, and that flush never runs — so anything
you picked up or triggered in the scene you left through the portal (collectibles,
chests, switches, in that scene only) is not in the frozen save. Progress in every
*other* scene is safe.

**Workaround: leave and re-enter the scene through any door or loading zone before
you cross**, or save (owl statue, Song of Time) first. The fix — flush live scene
flags before every freeze, in both games and on both switch paths — is in flight.

### F10 during MM's game-over screen hands over an empty health bar — [#626](https://github.com/spencerduncan/redshipblueship/issues/626)

Health is a shared resource between the two games, and it is applied as-is on
arrival. Pressing the F10 debug hot-swap while MM's game-over prompt is up freezes
the save with zero health and switches; you arrive in OoT with an empty bar. The
normal game-over exit (choosing not to continue) already revives you on the way out
(PR [#625](https://github.com/spencerduncan/redshipblueship/pull/625)); the F10
route bypasses it.

**Workaround: do not press F10 on the game-over screen.** Answer the prompt first,
or cross through the portal. The fix is in flight alongside #638.

### ~~F10 hot-swap silently rolls back your progress~~ — RESOLVED ([#364](https://github.com/spencerduncan/redshipblueship/issues/364), PR [#400](https://github.com/spencerduncan/redshipblueship/pull/400))

F10 now freezes the departing game's state and sets a return entrance, the same
as the portal switch, and the frozen state is cleared when consumed. The historical
symptom — resuming from the *other* game's save bytes after an F10 switch — is
gone. The one remaining F10 hazard is the game-over case above.

### ~~A malformed save permanently deadlocks all saving~~ — RESOLVED ([#370](https://github.com/spencerduncan/redshipblueship/issues/370), PR [#391](https://github.com/spencerduncan/redshipblueship/pull/391))

`saveMtx` is released on the exception path and the file is no longer truncated
before the write that could throw.

### ~~Cross-game save format will change~~ — it did; see "Back up your saves"

The `reserved` padding, the version window and the refused state that entry asked
for all exist now. The advice stands: the format may change again.

---

## Crashes and hangs

### Opening Network → Anchor in a wide window displaces the game view — [#634](https://github.com/spencerduncan/redshipblueship/issues/634) (community report; tracked in [#640](https://github.com/spencerduncan/redshipblueship/issues/640))

On Windows, open the menu, go to `Network → Anchor` while the window is wider than
roughly 800 px (maximized, for example): the page's content column is empty, and
when you close the menu the game view is pushed off to a black rectangle. The page
has no widgets because its menu registrar is dropped by the linker from the
`soh_port` archive, and the empty page still positions the main game window. Not
specific to fullscreen or DirectX 11.

**Workaround: narrow the window before opening that page, or avoid the page;
restarting restores the view.** The fix is in flight.

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

### First arrival in Clock Town reads 08:00 instead of 06:00 — [#636](https://github.com/spencerduncan/redshipblueship/issues/636) (community report; tracked in [#639](https://github.com/spencerduncan/redshipblueship/issues/639))

The first crossing from OoT into a new MM file lands in South Clock Town at Day 1,
8:00 AM, with no dawn sequence, instead of a new file's 6:00 AM. No time value
leaks from OoT: the 8:00 is MM's own title-screen attract-demo clock, which the
first-entry path never re-authors. You lose two hours of the first day and the
dawn telop; nothing else is wrong with the clock.

**Workaround: none needed for correctness** — play on, or play the Song of Time to
start a clean cycle if the lost two hours matter to your route. The fix (re-author
the new-file clock on first arrival so the vanilla dawn runs) is in flight.

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

### MM hook dispatch is still partial — [#438](https://github.com/spencerduncan/redshipblueship/issues/438)

14 of MM's 23 game-hook types have no dispatch point in the single-exe build.
Randomizer options and enhancements that depend on those hooks are shown
disabled-with-reason in the MM options pane rather than silently doing nothing.
The actor-init, actor-draw and open-text hooks (PR
[#512](https://github.com/spencerduncan/redshipblueship/pull/512)), the pause-menu
and file-select hooks (PR [#547](https://github.com/spencerduncan/redshipblueship/pull/547))
and the item/progression trio (PR
[#630](https://github.com/spencerduncan/redshipblueship/pull/630)) now dispatch.

---

## Switching and entrances

### ~~Test and default entrance links collide~~ — RESOLVED ([#374](https://github.com/spencerduncan/redshipblueship/issues/374), PR [#397](https://github.com/spencerduncan/redshipblueship/pull/397))

Duplicate entrance-link registrations are rejected instead of silently shadowing.

### ~~An entrance bound is an unchecked literal~~ — RESOLVED ([#380](https://github.com/spencerduncan/redshipblueship/issues/380), PR [#417](https://github.com/spencerduncan/redshipblueship/pull/417))

---

## Platform and packaging

### macOS is not supported

The macOS CI build is **disabled** (`.github/workflows/generate-builds.yml`,
`build-macos`) due to an unresolved linker issue. No macOS artifacts are produced.
Windows and Linux only.

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
where they were caught; the MM randomizer options pane labels each row live,
partial, dormant or generation-only with a reason. This pass was not exhaustive —
an enabled-looking toggle that does nothing is a plausible bug, and worth reporting.

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

- **`build-windows` runs zero tests** — it compiles both games and ~90 test rows
  and never invokes `ctest` ([#623](https://github.com/spencerduncan/redshipblueship/issues/623),
  fix in flight). A green Windows job today proves the link, not the tests.
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
