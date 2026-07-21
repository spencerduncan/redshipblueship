# Known issues

**Applies to:** `v0.1.0-prealpha` and the `main` branch behind it.
**Last updated:** 2026-07-20.

RedShipBlueShip is **pre-alpha**. It boots Ocarina of Time and Majora's Mask from
one executable and can round-trip between them, and that is roughly where the
guarantees stop. This document is the honest list of what is broken, grouped by
what it does to you as a player rather than by issue number. Nothing here is a
surprise to the maintainers — every entry links its tracking issue.

If you hit something not on this list, please file it. If you hit something on
this list, the issue link is the place to add detail.

---

## Read this before you play

### Cross-game randomization does not exist yet

The headline feature — one seed producing a paired OoT+MM world where items cross
between the games — is **not implemented**. What ships today is the *plumbing*:
two games in one binary, a shared window and resource manager, and an
entrance-based switch between them.

Specifically:

- The cross-game item storage exists but **nothing populates it during play**.
  ADR 0002 (Phase 3 Lane A0) gave the cross-game context an origin-tagged item
  array (`sharedItemsTagged`) and retired the never-wired `sharedItems` and
  `saveSlot` fields in place; `sharedFlags` is kept with its bit semantics
  still unassigned, and `sourceIsRando`/`sharedRandoSeed` are designated Lane
  B's seed carrier. The producers and consumers — writing on suspend, reading
  after a switch — are Lane A1, still open: outside tests, nothing is written
  to any of these fields yet.
- MM's randomizer code (`games/mm/2s2h/Rando/`) is compiled but **discarded by the
  linker** — no symbol in the shipping binary references it, so it is not in the
  executable at all.
- OoT's randomizer is the stock Ship of Harkinian randomizer, single-game only.

Cross-game randomization is the entire content of Phase 3. Do not expect it in a
pre-alpha build.

### Back up your saves. Seriously.

The save path has open correctness bugs (below) and the cross-game save format
will be re-versioned during Phase 3. Treat any progress made on a pre-alpha build
as disposable.

---

## Save loss and corruption

### F10 hot-swap silently rolls back your progress — [#364](https://github.com/spencerduncan/redshipblueship/issues/364) (critical)

The F10 debug hotkey switches games **without freezing the departing game's save
state and without setting a return entrance**. The entrance-based switch (Happy
Mask Shop ↔ Clock Tower) does both; F10 does neither.

What you see: switch away with F10, come back later, and the game resumes from
whatever save bytes happen to be in the shared storage — in practice, the *other
game's* live save. Progress made before the swap is silently gone. There is no
error and no prompt.

There is also a crash hazard on the same path: with the other game's residue in
`gSaveContext`, Player's held-item ids can pass the port's narrow validity guard
and produce a wild indirect call on every update frame.

**Workaround: do not use F10. Switch games through the in-game entrance only.**

### A malformed save permanently deadlocks all saving — [#370](https://github.com/spencerduncan/redshipblueship/issues/370) (critical, open)

`SaveManager` does not release `saveMtx` on the exception path. If a save file
fails to parse (the known trigger is malformed UTF-8 in an OoT JSON meta save),
the mutex is never unlocked and **every subsequent save and load in the session
blocks forever**. The file is also truncated *before* the write that throws, so a
failure can leave a zero-length save behind.

Symptom: the game appears to keep running, but saving stops working and any UI
path that touches the save system hangs.

### Cross-game save format will change

The cross-game context blob has no reserved padding and its version check is
exact-match-refuse. When Phase 3 adds real shared-item storage, the format
changes and **old `.redsave` files will not migrate**. This is expected to happen
early in Phase 3.

---

## Crashes and hangs

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

### Crashes in CI/headless mode present as a 180-second hang — [#388](https://github.com/spencerduncan/redshipblueship/issues/388)

The crash handler calls `SDL_ShowSimpleMessageBox`, which blocks in the X11 poll
when there is no display. A crash therefore produces a timeout with no exit code,
no signal, and no stack. Affects automated runs, not normal desktop play.

### Audio subsystem can wedge across a switch — [#365](https://github.com/spencerduncan/redshipblueship/issues/365), [#371](https://github.com/spencerduncan/redshipblueship/issues/371), [#377](https://github.com/spencerduncan/redshipblueship/issues/377)

- OoT's audio thread is **never stopped** when MM becomes the active game. It keeps
  being notified every frame and runs its buffer-fill against torn-down state with
  no initialization guard ([#365](https://github.com/spencerduncan/redshipblueship/issues/365)).
- The sequence maps are `malloc`'d rather than zeroed, so recently-added bounds
  guards leave uninitialized holes that get dereferenced and `free()`d — a
  plausible contributor to the exit corruption above
  ([#371](https://github.com/spencerduncan/redshipblueship/issues/371)).
- The custom-sequence bounds guard runs *after* registration and checks the wrong
  bound ([#378](https://github.com/spencerduncan/redshipblueship/issues/378)).

Practical effect: audio glitches, silence, or a hang after a game switch,
especially with custom music installed.

---

## Majora's Mask specific

MM is the newer half of the combo and carries more debt than OoT.

### MM enhancements do not initialize in single-executable builds — [#384](https://github.com/spencerduncan/redshipblueship/issues/384)

2Ship's enhancement registrars (`RegisterMoonJump`, `RegisterItemUnequip`,
`RegisterEasyFrameAdvance`, `RegisterUnrestrictedItems`, `RegisterArrowCycle`, and
others) share **identical mangled names** with OoT's. Only OoT's archive is linked
whole, so MM's registrars are never pulled into the binary and never run.

What you see: enhancement and cheat toggles that appear in the menu but have no
effect while MM is the active game. Forcing MM's registrars into the link today
would produce duplicate-symbol link errors, which is why this is a real fix and
not a flag flip.

Related: some MM enhancement hooks are still stubbed out entirely in
`src/common/mm_stubs.c`.

### MM code binds to OoT implementations in several places

Where a file is excluded from the MM build but its OoT twin is not, MM's calls
silently bind to OoT's function bodies. These are not link errors — there is only
one definition — so nothing warns:

- **Actor culling** ([#382](https://github.com/spencerduncan/redshipblueship/issues/382), critical) —
  MM runs OoT's `Ship_ExtendedCulling*` against MM's differently-laid-out `Actor`
  struct. Silent memory corruption; expect actors popping in/out or worse.
- **Framebuffer effects** ([#386](https://github.com/spencerduncan/redshipblueship/issues/386)) —
  MM's framebuffer effects read OoT's screen dimensions. Latent today; will show up
  as misaligned effects at non-default resolutions.
- **C++ layout mismatches** ([#383](https://github.com/spencerduncan/redshipblueship/issues/383), critical) —
  `GameInteractor`, `ShipInit`, and `UIWidgets::WidgetOptions` have different layouts
  in the two ports while sharing one definition.
- **Stub signature drift** ([#372](https://github.com/spencerduncan/redshipblueship/issues/372),
  [#385](https://github.com/spencerduncan/redshipblueship/issues/385)) — some stubs
  have the wrong signature or never return a value. `GameInteractor_InvertControl`
  returns an enum ordinal where a ±1 multiplier is expected (inverted-camera options
  behave wrongly); non-void stubs in `soh/stubs.c` fall off the end and the fault
  handler dereferences the garbage.

### Timers are not neutralized on the MM side of a switch — [#373](https://github.com/spencerduncan/redshipblueship/issues/373)

MM's state-restore path does not clear live timers the way its OoT counterpart
does. A timer left running across a switch can fire against a world that no longer
matches it.

---

## Switching and entrances

### Test and default entrance links collide — [#374](https://github.com/spencerduncan/redshipblueship/issues/374)

The default and test entrance links both use MM entrance `0xC010`, and the test
link's return leg mis-routes to Hyrule Market instead of its intended destination.
Affects harness routes primarily, but a mis-routed return is visible in normal play
if you take the test link.

### An entrance bound is an unchecked literal — [#380](https://github.com/spencerduncan/redshipblueship/issues/380)

The integration-hook entrance bound is a bare literal with a comment describing a
range check that does not actually exist in the code.

---

## Platform and packaging

### macOS is not supported

The macOS CI build is **disabled** (`.github/workflows/generate-builds.yml`) due to
an unresolved linker issue. No macOS artifacts are produced. Windows and Linux
only.

### You must supply your own ROMs

Asset extraction requires original Ocarina of Time and Majora's Mask ROMs. No ROMs
or extracted archives are distributed. See `docs/BUILDING.md` and
`docs/mm-archive-setup.md`.

### Config and settings are still Ship-of-Harkinian-shaped — [#34](https://github.com/spencerduncan/redshipblueship/issues/34)

The config file is still named `shipofharkinian.json` and lives in the `soh`
directory. This is deliberate for now — it keeps existing Ship configs working —
but it means:

- **There is no migration from an existing 2Ship (MM) config.** MM-side settings
  from a standalone 2Ship install are not imported.
- A RedShipBlueShip-specific config namespace is Phase 3 work, and adopting it will
  require a migration step.

### Some menu entries are stubs

Settings entries backed by unimplemented functionality are grayed out or labeled
where they were caught. This pass was not exhaustive — an enabled-looking toggle
that does nothing is a plausible bug, and worth reporting.

---

## CI and quality gates (for contributors)

Two gates currently **cannot fail**, which is worse than having no gate because
they read as coverage:

- [#375](https://github.com/spencerduncan/redshipblueship/issues/375) —
  `check-symbol-collisions.sh` has no committed baseline, so it exits 0 in bootstrap
  mode on every run.
- [#376](https://github.com/spencerduncan/redshipblueship/issues/376) — orphaned
  CTest labels no workflow runs, missing `--no-tests=error`, an unfailable
  `check-archives`, and a frame-budgeted watchdog that always loses to the wall-clock
  timeout.

Also: clang-format is enforced against an incremental allowlist
(`.github/clang-format-paths.txt`), not the full tree
([#369](https://github.com/spencerduncan/redshipblueship/issues/369)).

---

## Tracking

- [#321](https://github.com/spencerduncan/redshipblueship/issues/321) — pre-alpha
  v0.1.0 readiness epic
- [#381](https://github.com/spencerduncan/redshipblueship/issues/381) — Phase 2
  review follow-ups
- Full open list: `gh issue list -R spencerduncan/redshipblueship`

Findings from the manual QA pass
([#310](https://github.com/spencerduncan/redshipblueship/issues/310)) should be
appended to this document rather than tracked separately.
