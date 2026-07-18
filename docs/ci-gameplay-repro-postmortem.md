# Post-mortem: why CI is blind to the cross-game gameplay crash class — and the programmatic repro that closes the gap

Status: living document, written 2026-07-18 on `claude/ci-cross-game-crash-repro-f6iu2s`.
Companion to `.claude/worker-prompts.md` (Wave 3, cross-game switch crash stabilization).

## 0. TL;DR

Three consecutive crash fixes in the cross-game-switch class (#356 entrance leak,
#357 MM room guards, #367 VB enum aliasing) each shipped through fully-green CI and
were each caught only by the human operator playing the game. That is not bad luck;
it is structural. The crash class lives in a state space CI never enters: **a real
save, interpreted by `Play_Init` on the OoT return leg of a switch, followed by live
gameplay frames and a scene transition.** CI's runnable tiers exercise none of those
four things, and the tier that boots the real game (`int-*`) has never actually run
in hosted CI because it gates on ROM-derived archives that cannot exist there.

This document (a) audits exactly what CI runs and why each tier misses the class,
(b) corrects a false premise — hosted CI does *not* have ROM-extracted assets,
(c) inventories the in-engine hooks that let a test do everything the operator does
by hand, and (d) documents the `int-gameplay-*` harness added alongside this doc,
plus the CI wiring and the loop a continuous agent runs to iterate
fix→build→repro→observe without a human.

## 1. The crash class and the manual repro

Signature (2026-07-16 and 2026-07-17 operator logs, decoded in
`.claude/worker-prompts.md`):

- `Cutscene_HandleConditionalTriggers: entrance: 49168` in an OoT scene —
  49168 = 0xC010 = `MM_ENTR_CLOCK_TOWER_INTERIOR_1`, an MM entrance id observed by
  OoT code — followed by `Exception: 0xc0000005`; most recently RIP=0 (a call
  through a NULL pointer) in `SCENE_MARKET_DAY`.
- Always on the **OoT return leg** of an OoT→MM→OoT switch, during or shortly after
  `OoT_Play_Init` runs with restored (frozen) save state.

The only repro that has ever caught these, performed by the operator by hand:

1. start the game and load **their real save** with debug mode enabled;
2. open the **debug map-select** screen and pick Market;
3. play a **few seconds of live gameplay** (actors updating);
4. walk to the door — a **scene transition**.

Four ingredients: real save state, debug warp, frame-stepped gameplay, scene
transition. Section 3 shows each has a direct programmatic equivalent already in
the codebase.

## 2. Why CI can't trigger these crashes

### 2.1 What actually runs per PR

`generate-builds.yml` triggers on `pull_request` and is the only test-running
workflow a PR gets:

| Step | What it exercises | Gameplay? |
|---|---|---|
| `generate-rsbs-otr` | Builds `soh.o2r` + `2ship.o2r` **port-asset** archives (`extract_assets.py --norom`) | no |
| build-linux / build-windows | Full compile + link | no |
| `check-registrar-elision.sh` | nm-level link sanity (#341/#361) | no |
| `ctest --label-regex "^redship$"` (Linux only) | Headless logic tests in `src/common/test_runner.cpp` — entrance table, freeze/restore round-trips with **synthetic byte-pattern blobs**, `.redsave` serialization, scene-command parse/execute against a zeroed `PlayState` | no |
| `ctest --label-regex "^rando$"` under `xvfb-run` (Linux only) | Seed generation; brings up a real `Fast3dWindow` on llvmpipe but never boots a game | no |

`static-analysis.yml` (clang-tidy), `clang-format.yml`, and `python-tests.yml`
(tools/ only) add no runtime coverage. `test-builds-on-distros.yml` is
`workflow_dispatch`-only and compile-only.

### 2.2 The int-* tier exists but has never run in hosted CI

`integration-tests.yml` holds the only tests that boot the real binary
(`redship --integration-test int-boot-oot / int-boot-mm / int-switch-* /
int-archive-hotswap-cycle`, CTest label `integration`). Two independent gates keep
it from ever running:

1. **Trigger**: the workflow is `workflow_dispatch`-only (deliberately — see the
   comment at the top of the file: when it ran per-PR, every test step skipped
   while burning ~25 min of compute).
2. **Assets**: even when dispatched, the "Check for game archives" step looks for
   `oot.o2r`/`mm.o2r` and skips every `int-*` step when absent — and they are
   always absent on hosted runners (see 2.3).

So every PR that shipped one of these crashes was green because **the only tier
that could conceivably catch a runtime crash was never executed.**

### 2.3 Correcting the record: hosted CI does NOT have ROM-extracted assets

The premise "the generate-rsbs-otr job builds the o2r archives, so CI has
ROM-extracted assets" is false, and the distinction matters:

- `GenerateSohOtr` / `Generate2ShipOtr` (what CI runs — `CMakeLists.txt:369-380`,
  `:411-428`) invoke `OTRExporter/extract_assets.py` with **`--norom`**: they pack
  the repo's own `assets/custom` trees into `soh.o2r` / `2ship.o2r`. These are
  **port-asset archives** (fonts, shaders, UI textures, port-added content).
- The archives the game needs to boot scenes — `oot.o2r` / `mm.o2r` — are built by
  `ExtractAssets` / `ExtractMMAssets` (`CMakeLists.txt:350-357`, `:385-400`), which
  require `OTRExporter/oot.z64` and `OTRExporter/mm.z64`: original ROMs that cannot
  be distributed and can never be present on a GitHub-hosted runner.

So "the asset excuse" **does hold** for hosted runners, precisely and only for
`oot.o2r`/`mm.o2r`. Consequence: the full gameplay repro can run in CI only on a
**ROM-equipped self-hosted runner** (or the operator's machine driven by the same
one-command harness). Everything else in this doc is built so that the moment such
a runner exists (repo variable, see §5), the harness runs unattended; until then it
is a one-command repro for any ROM-equipped machine, which already removes the
human from the loop's *repro* step.

### 2.4 Even if int-* had run, it would not have caught this class

This is the deeper finding: the existing integration tests were designed as
boot/routing smoke tests, and every one of the four crash ingredients is missing:

- **No real save.** `int-switch-oot-hms-to-mm` triggers the switch from the
  **file-select screen** (`OnPresentFileSelect` hook,
  `games/oot/soh/GameExports_SingleExe.cpp`) — no file is ever loaded, so
  `gSaveContext` holds boot defaults, not an aged save with cutscene flags, quest
  state, and an entrance history. The crash's first domino
  (`Cutscene_HandleConditionalTriggers` consuming restored save state in
  `Play_Init`) can never fall.
- **The freeze/restore path is deliberately skipped.** The switch hooks call
  `Combo_CheckCrossGameEntrance(...)` directly — the comment says "minus the
  freeze, which T3 covers". But the production path the operator exercises is
  `Combo_CheckEntranceSwitch` (`games/oot/src/code/z_play.c:969`, `:1033`;
  `games/mm/src/code/z_play.c:631`), which **freezes the live SaveContext** and
  later restores it in `OoT_Game_Resume`. The frozen-state restore + startup
  entrance consumption in `OoT_Play_Init` (`z_play.c:398-417`) is exactly where
  the 0xC010 leak detonated — and no integration test drives it.
- **The OoT return leg never reaches gameplay, and its pass condition is weak.**
  `int-switch-mm-clocktown-south-to-oot` declares PASS after 10 firings of
  `OnGameStateMainStart` — a hook that fires for *any* game state, including the
  title screen. OoT boots fresh (there is no prior OoT suspend in that test, so
  `GameRunner_SwitchTo` runs `OoT_Game_Init`, not `OoT_Game_Resume`), lands on the
  title screen, ticks 10 frames, and passes without ever creating a `PlayState`.
  The MM-side hooks got this right after #344 (`MM_SceneLoadComplete()` checks a
  live `PlayState`, spawned player, loaded room); the OoT side has no equivalent.
- **No gameplay frames, no scene transition.** Nothing ever runs actors in a real
  scene, and nothing drives `play->transitionTrigger` / `nextEntranceIndex`
  through the transition state machine — the second crash surface (the SCENE_MARKET_DAY
  RIP=0 crash happened *after* arrival, in live play near a transition).

### 2.5 Platform and crash-signal gaps

- **Platform**: the operator plays Windows builds; every runtime test CI has is
  Linux-only. The bugs so far were logic bugs that would fault on either OS, but a
  Windows-only manifestation (SEH, MSVC codegen, `/FORCE:MULTIPLE` link
  divergence — see #341) would stay invisible even to a perfect Linux harness.
  (Mitigation: keep the harness OS-portable so a Windows runner can join the
  matrix later; see §6.)
- **Crash-signal plumbing**: when an `int-*` test segfaults today, CI gets an exit
  code and whatever stderr made it out. Nothing uploads the libultraship
  CrashHandler log or the `logs/` directory as an artifact; a hang manifests as
  `timeout` exit 124 with zero diagnostics. (Fixed in §5: failure-path artifact
  upload + `SHIP_HOME` pinning so the log location is deterministic.)

## 3. Programmatic equivalents of the manual repro

Every ingredient of the operator's loop already had an in-engine mechanism; nothing
about the repro is inherently manual:

| Manual step | Programmatic equivalent (pre-existing code) |
|---|---|
| Load a save with debug mode | `OoT_Sram_InitDebugSave()` (`games/oot/src/code/z_sram.c:36`) authors a full debug save in place; `Select_LoadGame` (`games/oot/src/overlays/gamestates/ovl_select/z_select.c:25-83`) and the boot branch of `Enhancements/Warping.cpp Warp()` (`:47-98`) show the exact field set (fileNum 0xFF, cutsceneIndex, dayTime, entranceIndex) |
| Open debug map select, pick Market | Map select does **not** use the transition system — it sets `gSaveContext.entranceIndex` and `SET_NEXT_GAMESTATE(..., OoT_Play_Init, PlayState)` (`z_select.c:81-82`). Market row = `ENTR_MARKET_SOUTH_EXIT` 0x00B1 (`z_select.c:214`). In-game warp = the `entrance` console command trio (`debugconsole.cpp:425-428`): `play->nextEntranceIndex` + `transitionTrigger = TRANS_TRIGGER_START` + `transitionType` |
| A few seconds of live gameplay | `OnPlayerUpdate` GameInteractor hook — fires once per frame at the end of `Player_Update` (`z_player.c:12495`), i.e. only while a player actor is live in a real scene. `OnSceneInit` (`z_play_otr.cpp:61`) and `OnTransitionEnd` (`z_play.c:1000`) mark arrivals |
| Walk to the door | Identical to a real exit poly (`z_player.c:5139-5185`): set the same three transition fields; the play update loop consumes them (`z_play.c:768`, `:956-1003`), runs `Combo_CheckEntranceSwitch(play->nextEntranceIndex)` (`z_play.c:969`) — **with** the SaveContext freeze — and re-enters `OoT_Play_Init` |

MM's side mirrors this: `play->nextEntrance` + `transitionTrigger` (`games/mm/src/code/z_play.c:631` runs `Combo_CheckEntranceSwitch` in `TRANS_MODE_SETUP`), with `MM_SceneLoadComplete()` (`games/mm/2s2h/GameExports_SingleExe.cpp`) as the "gameplay is real" predicate.

On save injection: the debug save is a *late-game-shaped* save (full inventory,
10 hearts, most quest flags), which is the closest programmatic stand-in for the
operator's aged save. Loading the operator's actual save is also mechanically
possible — SoH saves are JSON (`Save/file<N>.sav`, `SaveManager::LoadFile`) and the
unified `.redsave` has a headless-tested load path (`src/common/save.cpp`,
`rsbs::SaveManager`) — but needs a sanitized copy checked into the repo or staged
on the runner. Listed as follow-up infra in §6, not part of this harness.

## 4. The harness: `int-gameplay-roundtrip`

New integration-test mode `INT_TEST_GAMEPLAY_ROUNDTRIP`
(`redship --integration-test int-gameplay-roundtrip`), driven by a phase machine
shared between the two game sides (`src/common/integration_test_hooks.{h,cpp}`;
OoT driver in `games/oot/soh/GameExports_SingleExe.cpp`, MM driver in
`games/mm/2s2h/GameExports_SingleExe.cpp`). All hooks are registered only when the
mode is active — zero release-behavior change.

Phases (each transition logged as `[GP-TEST] phase: X -> Y`):

1. **boot** — at the first `OnZTitleUpdate` (or `OnPresentFileSelect`, whichever
   fires first), author a debug save and enter Play directly at
   `RSBS_GP_BOOT_ENTRANCE` (default 0x01D1, outside the Happy Mask Shop) —
   the map-select flow, programmatically.
2. **oot-pre-switch** — after `OnSceneInit` confirms arrival at the expected
   entrance, run `RSBS_GP_FRAMES` live `OnPlayerUpdate` frames, then fire the
   Happy Mask Shop door (0x0530) through the real transition machinery. This is
   the leg the old `int-switch-*` tests faked: here `Combo_CheckEntranceSwitch`
   runs from `z_play.c` and **freezes the live debug save**.
3. **mm-stabilize** — MM boots/resumes into South Clock Town (0xD800, the
   tower-exit spawn — the portal arrival is "as if you just walked out of the
   Clock Tower"); wait for `MM_SceneLoadComplete()` + `SCENE_CLOCKTOWER` + 10
   stable frames. On arrivals after the first, hard-assert the save-continuity
   sentinel the previous MM leg froze (a rupee count written before the exit) —
   this fails loudly if the frozen MM save ever stops surviving the boot
   chain's `SaveContext_Init`/`Sram_InitNewSave` wipes (the in-chain restore
   lives in `MM_Play_ConsumeStartupEntrance`, z_play.c).
4. **mm-play** — `RSBS_GP_FRAMES` live MM frames, then fire the Clock Tower
   door (0xC010, the MM→OoT trigger) through MM's transition machinery
   (freezes MM's save). The trigger/arrival split is deliberate: 0xD800 is
   targeted by MM's own cycle resets (Song of Time, save-warp, title attract),
   so it must never be a switch trigger.
5. **oot-return** — **the crash surface.** OoT *resumes* (`OoT_Game_Resume`
   restores the frozen SaveContext and applies the startup/return entrance), and
   `OoT_Play_Init` runs on restored state. `OnSceneInit` hard-asserts the arrival
   entrance is 0x01D1 — an MM id surviving into OoT (the #356 class)
   fails here even when it happens not to crash. Then `RSBS_GP_FRAMES` live frames.
   If `RSBS_GP_CYCLES` > 1, loop back to step 2 — the multi-cycle path is what
   exercises MM's *resume* leg (cold gamestate-chain boot against a re-armed
   arena; see §8).
6. **oot-warp** — debug-warp to `RSBS_GP_WARP_ENTRANCE` (default 0x00B1, the
   map-select Market row): a full scene transition on post-round-trip state.
7. **oot-exit** — one more door transition to `RSBS_GP_EXIT_ENTRANCE` (default
   0x0033, Market's south gate), then `RSBS_GP_FRAMES` frames → **PASS**.

Failure modes are all loud:

- Wrong arrival entrance → `[GP-TEST] FAIL` + state dump + non-zero exit.
- No progress → per-side frame watchdogs (`frames*4 + 3600`) dump play-state
  pointers before failing, so a hang is attributable without a debugger.
- Crash after logger init → libultraship CrashHandler writes registers, a
  symbolized backtrace, and the game callbacks' Scene/Room/Actors dump into
  `./logs/<AppName>.log` (there is **no separate crash file** — the dump is a
  CRITICAL record in the rotating log; CI uploads it, §5).
- Crash before logger init (window/GL bring-up) → the rsbs early signal handler
  prints the gameplay phase state to stderr and exits `128+signal`, so CI can
  distinguish "crashed" from "assertion failed".

CTest rows (label `integration`, plus `integration-soak`):
`IntGameplayRoundtrip` (300 s timeout) and `IntGameplayRoundtripSoak`
(`RSBS_GP_CYCLES=3`, 900 s). Scene/frame parameters are env vars on purpose: a
sweep varies them without new build-system rows.

### Scope honesty — what this harness does NOT reproduce

- **The operator's actual save.** The debug save is late-game-shaped, but the
  crash logs' exact cutscene/event flag constellation may matter. §6 covers save
  injection.
- **Windows.** The harness is OS-portable, but until a Windows runner exists it
  runs where CI runs (Linux). A Windows-only manifestation (SEH, MSVC codegen)
  needs the operator or a Windows runner. The bug *class* so far (logic bugs,
  wild reads, null calls) faults on Linux too.
- **Walking.** Player position is not scripted; the transition is fired directly.
  The crash class is scene-transition/state-corruption driven, so the transition
  is the point. If a future crash needs physical movement or collision state, add
  a scripted-input phase (deterministic `ControlDeck` injection) — noted as a
  possible extension, not built.

## 5. CI wiring

Changes to `.github/workflows/integration-tests.yml`:

- **New step** "Gameplay Roundtrip (crash repro)" after the existing int-* steps,
  gated the same way (needs `oot.o2r` + `mm.o2r`). It sweeps
  `RSBS_GP_WARP_ENTRANCE` over the `gp_warp_entrances` dispatch input
  (comma-separated, default `0x00B1`), with `gp_frames`/`gp_cycles` inputs for
  soak depth.
- **Crash artifacts**: on any failure the job now tails and uploads `./logs/`
  (artifact `integration-crash-logs`, 14-day retention) — previously a segfault
  left nothing but an exit code.
- **ROM-equipped runner hook**: `runs-on` honors the repo variable
  `RSBS_ROM_RUNNER` (JSON, e.g. `["self-hosted","linux","rsbs-rom"]`), same
  pattern as `LINUX_RUNNER` in `test-builds-on-distros.yml`. The moment such a
  runner exists, dispatching this workflow runs the full gameplay repro
  unattended; adding `pull_request`/`schedule` triggers becomes worthwhile then,
  and only then.

### CI-minutes cost

- On **hosted runners today**: the workflow costs ~25-40 min per dispatch
  (deps + full build) and the entire int tier — including the new test —
  **skips** for lack of `oot.o2r`/`mm.o2r`. That is why no `schedule:` was added:
  a nightly soak on hosted runners would burn ~30 min/night testing nothing.
  The build/unit/rando half of the job is already covered per-PR by
  `generate-builds.yml`, so hosted dispatches of this workflow are only useful
  as a smoke check of the workflow itself.
- On a **ROM-equipped self-hosted runner** (own hardware, no billed minutes):
  warm build ~5-10 min; each gameplay run is bounded at 5 min (typical pass
  ≪ 2 min); a 10-scene warp sweep ≈ 20-50 min. A nightly soak
  (`gp_cycles=3`, 10-scene sweep) is cheap there and is the recommended steady
  state. Gate: keep the sweep behind `workflow_dispatch` inputs until the
  self-hosted runner exists; then add a `schedule:` trigger guarded by
  `if: vars.RSBS_ROM_RUNNER` so hosted runners never pay for it.

## 6. What still cannot be done in CI, and the infra that would close it

1. **Hosted runners can never run the gameplay tier** — `oot.o2r`/`mm.o2r` are
   ROM-derived and cannot be distributed (§2.3). This is a hard legal constraint,
   not a technical one. **Closure: a self-hosted runner with the archives staged**
   (set `RSBS_ROM_RUNNER`); until then, the same one-command repro runs on the
   operator's machine (§7), which already removes the human from the repro *loop*
   even though their hardware stays in it.
2. **Display is NOT the blocker.** libultraship's SDL2 backend hard-requires a GL
   4+ context (`SDL_GL_CreateContext` with no fallback — `gfx_sdl2.cpp:476`), so
   `SDL_VIDEODRIVER=dummy` cannot work and no headless/null backend exists. But
   Xvfb + llvmpipe (`libgl1-mesa-dri`) satisfies it on hosted runners — proven
   every PR by the `rando` tier's full `Fast3dWindow` bring-up under `xvfb-run`.
   The int tier already starts Xvfb on `:99`. No infra needed.
3. **Operator-save injection.** SoH saves are JSON; the unified `.redsave` load
   path is headless-tested. Closure: the operator exports a sanitized save
   (played-through state, no personal naming), it gets staged on the ROM runner
   (or committed if clean), and the harness gains an `RSBS_GP_SAVE=<path>` branch
   that loads it instead of calling `OoT_Sram_InitDebugSave`. Follow-up issue —
   the debug save covers the class until then.
4. **Windows coverage.** The crash logs are Windows builds; CI runtime coverage
   is Linux-only. Closure: a Windows entry in the matrix once a runner exists
   (hosted `windows-latest` *could* run the harness if archives existed — same
   legal blocker as #1, so in practice this also means self-hosted).
5. **Crash fidelity.** The in-process backtrace is nearest-symbol in a static
   exe (see the 2026-07-16 log decode caveats in `.claude/worker-prompts.md`).
   Core-dump collection (`ulimit -c` + `gdb -batch`) was considered and skipped:
   an 8 GB+ process makes core artifacts impractical on shared runners. If a
   self-hosted runner appears, enabling cores there is a one-line addition to the
   workflow.

## 7. The continuous-agent loop

Everything below is one command per step, designed so an agent (or the operator)
can iterate fix→build→repro→observe with no gameplay skill involved.

### Build (any Linux box with ROMs; self-hosted runner; operator's machine)

```bash
git submodule update --init
cmake -B build-cmake -S . -GNinja -DCMAKE_BUILD_TYPE:STRING=Release -DREDSHIP_BUILD_SHARED=ON
cmake --build build-cmake --config Release --parallel
# One-time asset staging (needs OTRExporter/oot.z64 and OTRExporter/mm.z64):
cmake --build build-cmake --target ExtractAssets ExtractMMAssets
```

### Repro

```bash
# Default: 1 round trip, warp to Market, exit through the south gate
xvfb-run -a ./build-cmake/redship --integration-test int-gameplay-roundtrip

# Soak: 3 round trips, longer gameplay windows, sweep a different scene
xvfb-run -a env RSBS_GP_CYCLES=3 RSBS_GP_FRAMES=300 RSBS_GP_WARP_ENTRANCE=0x00CD \
  ./build-cmake/redship --integration-test int-gameplay-roundtrip

# Or via ctest (same binary, wired timeouts):
ctest --test-dir build-cmake -R IntGameplayRoundtrip --output-on-failure
```

(On the operator's Windows machine: same flags, no xvfb — run it in a normal
session; the window will flash through the phases.)

### Observe

- **Exit 0** = pass. Stdout shows every `[GP-TEST] phase:` transition and config.
- **Exit ≥128** = crashed before the logger was up (signal = code−128); the phase
  state was dumped to stderr.
- **Other non-zero** = a `[GP-TEST] FAIL:` line says which phase/assertion; for
  crashes after logger init, the CrashHandler dump (registers + backtrace +
  Scene/Room/Actors) is at the end of `./logs/<AppName>.log` — note the filename
  contains spaces (`logs/RedShipBlueShip v… (…).log`), quote it.
- In CI: the failed run's `integration-crash-logs` artifact contains that log;
  the step log has the last 300 lines inline.

### Sweep / soak in CI (once `RSBS_ROM_RUNNER` is set)

```bash
gh workflow run integration-tests -R spencerduncan/redshipblueship \
  --field gp_warp_entrances=0x00B1,0x01D1,0x00CD,0x0033 \
  --field gp_cycles=3 --field gp_frames=300
gh run watch   # or poll `gh run list --workflow=integration-tests`
gh run download <run-id> -n integration-crash-logs   # on failure
```

### Bisect a regression

The repro is deterministic enough for `git bisect run`:

```bash
git bisect start <bad> <good>
git bisect run bash -c '
  cmake --build build-cmake --config Release --parallel &&
  xvfb-run -a timeout 300 ./build-cmake/redship --integration-test int-gameplay-roundtrip'
```

### Division of labor

The intended steady state: PR-tier CI (`generate-builds.yml`) keeps catching
logic regressions ROM-free; every crash-class fix adds a `redship`-label unit
lock as before (worker-prompts "ROM-free verification playbook"); and the
gameplay tier — dispatched by an agent after each suspect merge, or nightly on a
ROM runner — is the tripwire for the state-space CI's unit tiers cannot enter.
When it trips, the artifact log + `[GP-TEST]` phase trail localize the leg, the
worker-prompts triage table maps the signature to a subsystem, and the fix lands
with a new ROM-free lock. The human's remaining role is confirming fixes feel
right in real play — not discovering crashes.

## 8. Validation of the thesis: the Windows local-iteration findings (2026-07-17/18)

Two nights of running this harness on the operator's ROM-equipped Windows
workstation — the first environment where the full gameplay tier ever executed —
found and fixed **five production faults**, none of which any CI tier could have
seen. This is the harness doing exactly what §2 predicted it would:

1. **MM HUD stub-signature crash** (every single-exe build): mm_stubs.c void
   stubs for the CosmeticEditor `Gfx_Draw*_DropShadowOverride` family fed MM's
   HUD a garbage display-list write pointer — WRITE AV at 0xA7 in
   `MM_Interface_DrawItemButtons` on the first HUD-visible MM frame. Fixed in
   `games/mm/2s2h/CosmeticGfxSingleExe.cpp` (compiled against the real header),
   locked by `cosmetic-gfx-stub`.
2. **Graph-coroutine resume-into-poison** (the operator's return-leg crash
   class): resuming a suspended game's frame loop walked into an arena OoT's
   re-entered `Main()` had re-initialized (0xAB fill). Fixed by retiring the
   coroutine at suspend (`OoT_Graph_ResetRunFrameContext` /
   `MM_Graph_ResetRunFrameContext`) — the switchover contract is now **cold
   gamestate-chain start + frozen SaveContext + game-tagged startup entrance**.
3. **MM cycle-2 arena exhaustion** (found by the 3-cycle soak, cycle-2 MM
   re-entry): the cold-start contract leaks the retired MM session's entire
   system arena by design (the switch path cannot run `GameState_Destroy`, and
   Play's `GameState_Realloc(&state, 0)` had absorbed the whole 32MB arena),
   and `MM_SystemHeap_Init` ran only in `MM_Game_Init` — so the second cold
   boot's first gamestate malloc returned NULL and died in an unchecked
   `memset(NULL, ...)` ("WRITE at 0x20" = NULL + the vectorized memset's first
   store offset; decoded with `.claude/tools/dbg374.py` + `redship.map`).
   Fixed by re-arming the arena on every resume (`MM_ResumeColdBootPrep`:
   `MM_SystemHeap_Init` + `Regs_Init` — mirrors OoT, whose `Main()` re-init is
   why OoT never had this fault), plus an OoT-parity loud NULL check in MM's
   graph loop ("GAME CLASS MALLOC FAILED" abort instead of a silent AV).
   Locked by `mm-resume-arena`.
4. **MM frozen-save wiped by its own boot chain** (cycle-2+ continuity loss):
   the cold chain runs `Setup → MM_SaveContext_Init` (memset) and
   `TitleSetup → MM_Sram_InitNewSave` AFTER `MM_Game_Resume`'s restore, so MM
   continuity silently reset every round trip. Fixed by restoring the frozen
   save at the startup-entrance consumption point in `MM_Play_Init`
   (`MM_Play_ConsumeStartupEntrance`) — the first spot after the last wipe and
   before the save is interpreted — with respawn/day-transition state
   neutralized for the arrival spawn. Locked headless by `mm-startup-restore`
   and end-to-end by the soak's rupee-sentinel continuity assert. The main
   loop now also arms a startup entrance for hotkey (non-entrance) switches
   into a frozen game, so that path shares the same restore instead of
   silently booting a new file.
5. **Portal redesign closing a latent trigger hazard**: the MM→OoT trigger was
   `0xD800` (South Clock Town spawn 0) — the exact entrance MM's own Song of
   Time reset, save-warp, and title attract demo target, so an in-game cycle
   reset could fire a spurious cross-game switch. The portal is now the Clock
   Tower door in both directions: OoT HMS door (0x0530) → arrive SCT tower
   exit (0xD800); MM tower door (0xC010) → arrive outside HMS (0x01D1).
   Trigger and arrival are disjoint by construction. (Confirmed live: the
   attract demo's `0xD800` prints now pass through as inert arrivals.)
6. **MM FrameInterpolation cross-bind** (found by the operator's first look at
   MM 3D frames): both ports define an identically-named extern-C
   `FrameInterpolation_*` family — and the C++ `FrameInterpolation_Interpolate`
   mangles identically because both games name their matrix types Mtx/MtxF —
   so under `/FORCE:MULTIPLE` SoH's implementation won the whole family and
   MM recorded/replayed its matrices through OoT's interpolation engine.
   Fixed by the `MM_` prefix shim `games/mm/include/mm_frame_interpolation_prefix.h`,
   included from both `FrameInterpolation.h` and `include/gfx.h` (the
   OPEN_DISPS/CLOSE_DISPS macros locally declare two of the functions, so the
   shim must be visible to every macro-expanding TU, not just header
   includers). Verified by a dumpbin public-symbol intersection of the
   soh_*/2ship_* libs — the same audit that found the sibling
   `AudioCollection` class ODR collision (filed separately).
7. **The MM 3D render killer: prototypes dead inside `#if 0`**
   (`games/mm/include/PR/gu.h`): a vendoring-era `#if 0` block swallowed every
   `MM_gu*` declaration, so all MM matrix-builder calls (`MM_guPerspective`,
   `MM_guLookAt`, `MM_guOrtho`, `MM_guRotate`, `MM_guPosition`) compiled as
   implicit C declarations — float args promoted to double under the
   unprototyped MSVC x64 convention while the definitions read the normal
   float ABI. Every matrix MM built was register noise: all 3D rendered as
   flashing fullscreen triangles (the flat colors being MM's own fog-colored
   clear fills showing through), 2D texrects were pixel-perfect, OoT (live
   prototypes) and upstream 2Ship (unprefixed names declared by LUS's gu.h)
   were untouched. The hunt that found it is a reusable playbook: raw
   display-list dumps on both games (`RSBS_DUMP_GFX=<frame>`, hooks now in
   both graph.c files) proved emission-side health, an A/B perspNorm
   fingerprint (`0xFFFF` vs OoT's `0x000A`) isolated the projection, paired
   store/apply/emit probes proved healthy inputs turning to NaN inside the
   callee, and capstone disassembly of the call site showed the
   `cvtps2pd` + integer-register promotion pattern that convicts an
   unprototyped call. Fix: the prototypes are live again (with `near`/`far`
   parameter names avoided — windows headers define them away). The C4013
   census after the fix shows 8 remaining implicit declarations in MM
   (`MM_osSetIntMask`, `MM_osAiSetFrequency`, `MM_sprintf`, `MM_guS2DInitBg`,
   `MM_osMotorInit`) — all integer/pointer ABIs that cannot scramble, filed
   for cleanup. Recommended lock: promote C4013 to an error (`/we4013`) on
   the MM targets so this class becomes a build break.

Windows-specific operational notes for this tier (also see the repro commands
in §7): force the OpenGL backend (`Window.Backend.Id=1` — DXGI deadlocks the
second game's first present) and SDL audio (`Window.AudioBackend="sdl"` —
Wasapi deadlocks the audio handoff); read exit codes via PowerShell
`$LASTEXITCODE` (git-bash mangles NTSTATUS); test modes fast-exit (`_Exit`)
because normal CRT teardown heap-corrupts (0xC0000374, tracked separately —
minimal repro `redship --version`). Crash decoding without PDBs:
`.claude/tools/dbg374.py` (launch + catch + registers + stack scan + arena
forensics, symbols via `redship.map`) and `.claude/tools/stacks.py` (attach +
all-thread stacks) — the map regenerates on every relink, and dbg374 resolves
its globals from the map at crash time for exactly that reason.

Open faults filed from these sessions (not part of the harness change): the
exit-teardown heap corruption above; the DXGI present deadlock; the Wasapi
handoff deadlock; `int-switch-oot-hms-to-mm` needing a human START press
(`OnPresentFileSelect` never fires unattended); MM intra-frame wedges being
invisible to the frame-count watchdog (a wall-clock watchdog thread is the
candidate fix); unified-save `Load` not marking restored blobs as frozen
(`Context_UpdateShadowCopy` never sets `hasBeenFrozen`), so a loaded `.redsave`
MM blob is ignored by the resume/Play_Init restore path after an app restart;
the `AudioCollection` C++ class ODR collision between the two ports (audio
path — being fixed in a separate session); and the MM-first → OoT switch
chain. That last one is now partially fixed and partially open: the first
fault (reproduced unattended via the new `RSBS_AUTO_SWITCH_FRAME=<n>`
hotkey-switch hook + dbg374) was `OoT_AudioLoad_Init` globbing
`"audio/sequences*"` over the SHARED ResourceManager — with MM's archives
already loaded, MM's sequence ids (110-127) overran
`OoT_seqCachePolicyMap[MAX_AUTHENTIC_SEQID]`, corrupting the adjacent
`sequenceMap` pointer (AV on the next write). Both games' loops now
bounds-guard and loudly skip foreign entries (`audio_load.c` /
`load.c`), which also exposed the deeper correctness bug: foreign
sequences whose ids land in-bounds silently SHADOW the native game's
music in the map — in both boot orders — so the real fix is
archive-scoped enumeration (landed 2026-07-18, see §8.1). With the
guards in place the MM-first switch survives audio init but the process
still exits silently later in OoT's `Main()` with no crash dump — next
fault in the chain, undiagnosed.

## 8.1 Phase-3 local iteration (2026-07-18, session 3): audio bring-up and the resume contract

A third night on the workstation, working the operator's priority list
(MM audio, Tatl intro, child-canon returns, archive scoping). Eight more
production faults, each found by the previous fix's probe. The headline
method lesson: **an env-gated probe that renders an invisible property
visible (`RSBS_AUDIO_PROBE=1` — per-60-buffer nonzero-sample counts per
game, plus synth/handshake state) converted "audio doesn't work" into
five separately attributable faults in one session.**

1. **MM total silence — the synth consumer was never linked.** The only
   caller of `MM_AudioMgr_CreateNextAudioBuffer` (drains MM's audio
   command ring, runs `MM_AudioSynth_Update`) is in BenPort.cpp, which
   single-exe excludes. MM's producers ran every frame; the shared audio
   thread (OTRGlobals.cpp) only ever called OoT's synth — intentional
   zero-fill while OoT is quiesced. Fix: active-game dispatch on the
   shared thread, gated on MM's `gAudioCtxInitalized`; MM suspend drains
   the shared thread first (same UAF race OoT closed); update rate
   pinned to 3 while MM is active.
2. **The PreNMI resetTimer latch — audio died forever after any
   suspend, both games.** `*_PreNMIInternal` sets `resetTimer = 1`; on
   N64 the console then resets, but the port RESUMES and nothing clears
   it — and `*AudioLoad_SyncInitSeqPlayer` silently drops EVERY sequence
   start while it is nonzero. OoT's side has been broken this way since
   switching existed (masked: no probe, and the operator's attention was
   on MM). New `OoT_Audio_ResumeFromPreNMI`/`MM_Audio_ResumeFromPreNMI`
   clear it on resume. Sibling gaps fixed in the same contract: the
   audio bring-up never re-runs (`OoT_AudioMgr_Init` hides it behind a
   static `hasInitialized`; MM's only runs in Game_Init) — both resumes
   now call `*_Audio_InitSound`; and the frozen saves carry the
   suspended session's LIVE seqId/ambience ids, so arrival scenes
   skipped starting music — both consumption points now declare audio
   dead (`NA_BGM_DISABLED`).
3. **Archive-scoped enumeration** (the §8 item, landed): sequence and
   font ids live in the resource payload, so foreign in-range entries
   silently shadowed native music/fonts — OoT ids 0-109 all pass MM's
   128 bound, both boot orders. `ResourceMgr_ListFilesForGame` filters
   the shared glob by owning archive via the existing `sMMArchivePaths`
   registry (`Combo_ArchivePathIsMM`); the fonts loops also gained the
   bounds guard they never had (`sf->fntIndex` is a raw s32 from
   resource data written into `fontMap[]` unchecked — a live OOB write).
4. **The OoT return leg silently reset ALL OoT continuity** — the OoT
   twin of §8 item 4: the resume fast-forward passes through
   `Opening_Init` (z_opening.c), which re-authors the ADULT debug save
   over the SaveContext `OoT_Game_Resume` had just restored. Observable:
   the return leg loaded scene 34 (adult Market) instead of 32. The
   restore now (also) runs at OoT's startup-entrance consumption in
   `OoT_Play_Init` — first point after the last wipe, mirroring
   `MM_Play_ConsumeStartupEntrance` — with plain-spawn neutralization
   (respawnFlag, nextDayTime, audio ids) and presence-gating. Child Link
   is forced there on every cross-game arrival (operator decision:
   child-canon MM trip): `Inventory_SwapAgeEquipment` BEFORE flipping
   linkAge, plus post-swap equip normalization for authored saves with
   empty child buckets. The harness return-leg assert
   (`linkAge==LINK_AGE_CHILD`) caught the wipe on its first run;
   `RSBS_GP_BOOT_AGE=adult` boots the debug save as adult so the swap
   branch runs end-to-end.
5. **MM's Clock Town first-visit intro is ACTOR-triggered** —
   `ObjTokeiTobira` fires its Tatl/camera cutscene on any layer-0
   SCENE_CLOCKTOWER load while `WEEKEVENTREG_59_04` is clear, and
   Elf_Msg6 force-interrupts (text 0x216) on `WEEKEVENTREG_31_04` — so
   the consumption point's cutsceneIndex=0 reset could not stop it. It
   now pre-sets the same flag set every upstream 2S2H intro-skip path
   uses (59_04, 31_04, ENTERED_E/W/N, INSIDETOWER HMS switch). NOTE:
   `MM_InitFirstEntrySaveContext` (BenPort.cpp), the obvious-looking fix
   site, is DEAD CODE in single-exe (BenPort.cpp and its caller
   switch.cpp are both excluded).
6. **Path factory flat overwrite — a production landmine masked by
   resource caching.** Unlike Room/Cutscene (per-archive dispatchers,
   #344), MM's Path factory registration flat-overwrote the shared
   SOH_Path loader slot; every OoT Path resource FIRST-loaded after
   MM_Game_Init parsed with MM's wire format (extra fields) →
   `MemoryStream` read past the buffer → `std::out_of_range` rethrown
   out of the resource future → process death (exception 0xE06D7363, NOT
   an AV — invisible to AV-focused tooling). The stock harness route
   never hit it because its scenes' paths were cached before MM
   initialized; the adult-boot variant first-loads child Market on the
   return leg. Path now uses the same dispatcher (new
   `OoT_CreatePathFactory`). Tooling: dbg374 gained `DBG374_CPPEH=1` —
   logs every C++ throw with map-resolved exe stack frames.
7. **Link-set changes arm dormant COMDAT collisions.** The audio
   dispatch newly pulled MM's kaleido + PauseOwlWarp objects, which (a)
   needed `sOwlWarpEntrancesForMods` (defined in excluded ShipUtils.cpp;
   single-exe now defines the vanilla table in GameExports_SingleExe.cpp)
   and (b) flipped which game's `FlagTable` copy-constructor COMDAT wins
   — both ports define FlagTable with identical mangles but DIFFERENT
   layouts (SoH: std::map member; MM: std::vector<FlagEntry>), so every
   process crashed at static init (AV in `_Tree::_Copy` under
   soh_enh:TimeSplits.cpp's initializer). MM's SaveEditor types now live
   in namespace S2H (the AudioCollection treatment). Lesson for the
   backlog: the remaining intersection entries (UIWidgets Options
   structs, HookInfo, Notification::Options, WeirdAnimation) are
   landmines armed by ANY link-set change; the
   check-symbol-collisions.sh baseline should be burned down, not
   tolerated. A map-file COMDAT-provider audit
   (scratchpad comdat_provider_audit.py pattern) shows which archive
   serves each family after a link change.

Verified at session end: 24/24 redship tests; child and adult-boot
round trips PASS; probed 3-cycle soak PASS with nonzero audio on every
leg of every cycle on both games and rupee sentinels 101/102/103.
Operator verdicts still owed: MM music correctness (right tracks, right
instruments), no Tatl intro on arrival, child Link visible on return.
