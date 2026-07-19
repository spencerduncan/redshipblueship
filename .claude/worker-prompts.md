# Worker loop goals — Wave 3 (updated 2026-07-19, rev 7: camera/input + MM A button fixed)

**Rev 7 (2026-07-19, phase-5 local iteration):** operator retest confirmed §8.2's
fixes (clipping, splash, Market doors all GOOD). The two remaining symptoms are
fixed and red/green proven — full account in docs/ci-gameplay-repro-postmortem.md
§8.3. (a) The stuck camera AND the suspected input breakage were ONE fault: a
once-per-process latch (sInitRegs) de-synced from a per-entry re-mint (Main() ->
func_800636C0 re-mallocs gGameInfo and zeroes the REG backing store), so every
OoT re-entry ran the camera on all-zero OREG/R_CAM_DATA constants — it
translated with Link but never reoriented, and Player's stick-to-world yaw comes
from the camera, so the controls felt wrong too. An independent sweep found NO
input defect; do not re-chase it. (b) MM's missing A button was mm_stubs
signature drift, second instance and the first that does not crash:
OTRConvertHUDXToScreenX stubbed float(float) vs the real int32_t(int32_t), so
the A button's perspective viewport (the only one in MM's HUD) collapsed.
Method note worth keeping: the phase-4 camera assert summed eye+at displacement
and would NEVER have caught (a) — a summed metric launders the anchored-camera
state. Judge components separately; prefer a direct state assert
(OoT_Camera_RegsSeeded) over a geometry heuristic. Eleven commits on
claude/ci-cross-game-crash-repro-f6iu2s awaiting push (gh auth still absent).


**Rev 6 (2026-07-18, phase-4 local iteration):** all four operator-reported
return-leg bugs root-caused and fixed, red/green verified on the workstation —
full account in docs/ci-gameplay-repro-postmortem.md §8.2. Headlines: MM
clipping was the shared-mixer DMEM base (0x3C0 vs MM's 0x3B0 window —
rebased to 0x330); missing Market doors were in-place transition-actor id
negation persisting in the cached scene resource across the destroy-skipping
switch (normalized at scene bind, all three handlers, counterfactual-proven);
the splash replay was CustomLogoTitle dead-coding #350's Title_Main gate
since 89a395a5 whole-archived soh_enh (authoritative skip hoisted into
Title_Init after hook dispatch; all gates now presence+game-scoped); the
Lon Lon crash class traces to frozen nextCutsceneIndex re-authoring cutscene
scene layers into an unguarded alternate-header vector index (neutralization
extended: cutscene queue/trigger, timers, eventInf, minigame/dog/magic
scratch; OoT bounds guard added to match MM's). Plus: a base-less save file
minted by section-only autosaves killed every boot via an uncaught nlohmann
throw in StartupCheckAndInitMeta — save loading hardened both directions
(quarantine on read, refuse base-less mint on write). Harness: door-presence
+ demo-state + camera-follow asserts, RSBS_GP_WARP_FRAMES soak knob,
RSBS_AUDIO_PROBE peak/sat/maxrun. C4013 hygiene committed with /we4013 lock.
Eight commits on claude/ci-cross-game-crash-repro-f6iu2s awaiting push
(gh auth still absent on the workstation). Operator verdicts owed: clipping
gone, splash gone, doors present, camera on their route, Lon Lon stability.

**Rev 5 (2026-07-18, phase-3 local iteration):** MM audio works in single-exe
for the first time, and the whole cross-game resume-audio/save contract is
fixed — active-game synth dispatch on the shared audio thread, the PreNMI
resetTimer latch (dropped every sequence start after any suspend, BOTH
games), never-re-run audio bring-up, stale seqIds in frozen saves,
archive-scoped sequence/font enumeration (the real fix behind 5cc341df's
guards), the OoT-side Opening_Init save wipe (OoT continuity silently reset
every return trip), forced-child-canon returns with an adult-boot harness
variant (RSBS_GP_BOOT_AGE=adult), Clock Town intro suppression at the
consumption point, and the Path-factory flat overwrite (killed the first
path-bearing OoT scene loaded after any MM visit — masked by resource
caching). Full account: docs/ci-gameplay-repro-postmortem.md §8.1. New
tooling: RSBS_AUDIO_PROBE=1 (audio-liveness probes end to end),
DBG374_CPPEH=1 (dbg374 logs C++ throws with map-resolved frames).
Still open from the phase-3 list: the silent MM-first→OoT exit in OoT
Main(), and the C4013 census/we4013 lock (prototypes exist for all 8; the
census needs a clean-build re-run to confirm which caller TUs still miss
them). Operator verdicts owed: MM music correctness, no Tatl intro, child
Link on return.

Supersedes **Wave 2** (the 2026-06-10 Lanes 5–6 that used to fill this file — all
complete, see git history). Wave 2 was: Lane 5 = SOH shuffle feature ports
#289–#293 / epic #235; Lane 6 = Phase 2 closeout / epic #202 / #212. That entire
roadmap is **done** and predates the MM single-exe stabilization effort
(#340→#353), which is the live work now. This file went stale for a month after
those lanes closed — §"Replanning" below exists so that does not recur.

**Rev 2 (2026-07-16):** the operator's actual crash log arrived and was decoded.
The live crash is NOT on the MM side — it is the **OoT return leg** of a
cross-game switch (§A). An unmerged fix already exists on
`claude/cutscene-crash-redship-4aukbd`. Rev 1's ranked MM-side map was written
before the log existed; it is preserved as the contingency map (§B).

**Rev 3 (2026-07-16, later):** §A is DONE — the entrance-leak fix merged as
PR #356 (`cff754fc`, with the dedicated `StartupEntrance` redship CTest). The
operator then reported a switch crash still reproducing on the #356 PR
artifact, consistent with §B: the original log's MM fragment ends at
`File Name ...Z2_INSIDETOWER_room_00`, the exact line before first-room command
execution. §B candidates #1 and #2 are now guarded — PR #357 (`1f5fa3a5`):
FATAL log on room-resource load failure, NULL-room early-return in
`MM_OTRfunc_800973FC`, NULL-player skip of `func_80123140`, locked ROM-free in
`mm-scene-execute`. **Awaiting the operator's re-test on a build ≥ `1f5fa3a5`
and, if anything still crashes, the fresh crash log** — the new FATAL/WARNING
breadcrumbs will name the failing surface. §B candidates #3-#5 remain unguarded
and live.

**Rev 4 (2026-07-18): crash-decode CORRECTIONS + local-iteration results.**
Read `docs/ci-gameplay-repro-postmortem.md` §8 for the full account; the load-
bearing updates to THIS file's earlier analysis:

- **§A's decode is partially obsolete.** The
  `Cutscene_HandleConditionalTriggers: entrance: 49168` line in later operator
  logs is **MM's own z_demo.c printing its own (valid) MM arrival entrance**,
  NOT proof of the OoT-side leak — the #356 leak was real and is fixed, but do
  not treat that log line alone as a #356 recurrence. The remaining
  return-leg crash class was a **graph-coroutine use-after-free**: resuming a
  suspended game's frame loop walked into the arena the re-entered `Main()`
  had re-initialized (0xAB fill). Fixed (2cee2601 + follow-ups) by retiring
  the coroutine at suspend (`*_Graph_ResetRunFrameContext`); the switchover
  contract is now cold gamestate-chain start + frozen SaveContext +
  game-tagged startup entrance, with MM's arena re-armed and its frozen save
  restored in-chain at `MM_Play_ConsumeStartupEntrance` (locked by
  `mm-resume-arena` / `mm-startup-restore`, soak-asserted by the rupee
  continuity sentinel).
- **Portal semantics changed** (operator-requested): OoT HMS door (0x0530) →
  MM **South Clock Town tower-exit spawn 0xD800** (arrival); MM **Clock Tower
  door 0xC010** (trigger) → OoT outside-HMS 0x01D1. Historical references
  below to "the MM target (0xC010)" describe the pre-redesign link table;
  0xD800 is deliberately NOT a trigger (MM's Song of Time reset, save-warp,
  and title attract all target it).
- The gameplay repro (`int-gameplay-roundtrip`, 3-cycle soak) PASSES on the
  operator's Windows workstation as of this rev; it is the tripwire for this
  whole crash class (demonstrated: disabling the arena re-arm dies loudly with
  "GAME CLASS MALLOC FAILED" on cycle 2).

## Completed (Wave 2)
- **Lane 5** — shuffle features / epic #235: absorbed via PR #315; #235 closed.
- **Lane 6** — Phase 2 closeout / epic #202: #202, #211, #212, #231, #232, #233,
  #286, #288 closed; milestone 3 dispositioned. #34 (settings namespace) remains
  open by design, deferred to Phase 3.
- **Waves 1 (issues #154–#160) and Lanes 1–4** — recorded in this file's git history.

## Common rules (all lanes)
- Repo `spencerduncan/redshipblueship`, default branch `main`, all merges **squash**.
- Read the root `CLAUDE.md` first. Work on a `claude/<description>` branch; push as
  you work; open a PR when ready for CI; squash-merge once CI is **fully** green.
  Never merge red or partial CI. If blocked/uncertain, push and report.
- **This environment cannot compile (submodules uninitialized) or boot (no ROMs).
  CI is your only compile/test verifier** — get changes onto a PR; do not trust
  reasoning about link/runtime results.
- Always `git fetch` before touching a branch. Never force-push commits you didn't
  write without reading them first. Never push to `main`.
- End commit messages + PR bodies with the trailers CLAUDE.md specifies.
- File:line anchors below were verified against main `8b8d3f0a` (2026-07-16); if
  main has moved, locate by symbol, not by line.

---

## Active lane — cross-game switch crash stabilization

### A. PRIMARY: land the entrance-leak fix (crash is decoded — this is the live fault)

**The 2026-07-16 crash log (build `d81a23d`, "Copper Bravo 9.1.1", Windows) is
fully decoded. Do not re-derive it from scratch — verify it, then land the fix.**

Decoded signature:

- `Cutscene_HandleConditionalTriggers: entrance: 49168` (OoT `z_demo.c`) followed
  ~26ms later by `Exception: 0xc0000005`. **49168 = 0xC010 =
  `MM_ENTR_CLOCK_TOWER_INTERIOR_1`** (`src/common/entrance.h:37`) — an **MM**
  entrance id sitting in **OoT's** `gSaveContext.entranceIndex`.
- OoT's `entranceIndex` is a direct linear index into
  `gEntranceTable[ENTR_MAX = 0x614]` (`games/oot/include/z64scene.h:330`,
  `variables.h:123`; indexing sites e.g. `games/oot/src/code/z_play.c:500-514`).
  0xC010 reads ~32× past the table → garbage → segfault inside
  `OoT_Play_Init` → `Cutscene_HandleConditionalTriggers`.
- Context markers prove it is the **return/resume leg of a switch**: "Replacing
  resource factory" (archive hot-swap only happens in `EnsureGameArchivesLoaded`,
  `rsbs/src/main.cpp:532`) and SDL controllers re-added immediately before the
  crash. The same log's stdout stream shows the OoT→MM leg **succeeding** all the
  way into `Z2_INSIDETOWER` first-room load ("File Name
  scenes/nonmq/Z2_INSIDETOWER/...") — #353's MM-side guards held.
- Crash-dump caveats (teach these to your future self): the traceback is
  nearest-symbol resolution in a static exe — the repeated
  `Combo_SetSharedGraphics` frames are symbolization artifacts, NOT real frames.
  The dump's `Scene: SCENE_MARKET_DAY` + full Market actor list is the pre-switch
  OoT PlayState. The `GFX Stack: games/mm/src/code/graph.c:269` ×3 is stale MM
  `OPEN_DISPS` bookkeeping in the **shared** GraphicsContext (MM's
  `Graph_ExecuteAndDraw`) — expected residue, not the fault (worth a cleanup
  follow-up note, nothing more). Build `d81a23d` is not an ancestor of main —
  treat the log's line numbers (`z_demo.c:1605`, `CrashHandler.cpp:72`) as
  build-relative.

**Root cause (established, and matches an existing unmerged fix):** the
cross-game "startup entrance" is a **single game-agnostic global**
(`src/common/entrance.cpp:26`). On an OoT→MM entrance switch, `main.cpp:527-528`
writes the MM target (0xC010) into it. The resume hooks on BOTH sides read it
**without clearing** (`games/oot/soh/GameExports_SingleExe.cpp:354-361`,
`games/mm/2s2h/GameExports_SingleExe.cpp:863-865`), F10 switches never overwrite
it (`main.cpp:469-471` sets no startup entrance), and no consumer checks which
game the value was meant for. Any OoT resume/`Play_Init` that observes a stale
MM value applies it to `gSaveContext.entranceIndex` → OOB. (MM is naturally
OOB-resistant: its entrance ids are bit-packed scene/spawn, not a linear index.)

**The fix already exists — unmerged:** branch
`origin/claude/cutscene-crash-redship-4aukbd`, commit `7c6e7eec`
("Fix OoT Market cutscene crash from cross-game entrance leaking into
entranceIndex", 2026-07-15). It gives the startup entrance **game affinity**
(tagged with the destination game; game-scoped accessors; 1-arg setter kept as a
wildcard shim), adds an `ENTR_MAX` bounds guard in `OoT_Play_Init` as defense in
depth, and extends `Test_StartupEntrance` to assert an MM-tagged 0xC010 is
invisible to OoT (the exact crash condition). 8 files, +157/−18. It was never
PR'd. Its base is `b1133b39` (4 commits behind main), but **`git merge-tree`
confirms it merges cleanly onto `8b8d3f0a`** (verified 2026-07-16).

**Steps:**
1. `git fetch origin`, read `7c6e7eec` in full (commit message = the diagnosis).
   Branch `claude/<desc>` off fresh `origin/main`; cherry-pick `7c6e7eec`.
2. Re-verify its assumptions against current main — #349 (boot straight into
   OoT) and #350 (splash skip on switch) landed after its base and touched
   adjacent switch-flow code. Confirm by symbol: the resume hooks, the
   `main.cpp` switch block (`:440-545`), `OoT_Play_Init`'s startup consumption
   (`games/oot/src/code/z_play.c:398`), MM's (`games/mm/src/code/z_play.c:2244`).
3. Test wiring: `Test_StartupEntrance` (`src/common/test_runner.cpp:404`,
   `gTests[]` row `:524`) currently reaches CI **only via `AllTests`** — there is
   no dedicated `add_test` row. Add `add_test(NAME StartupEntrance COMMAND
   redship --test startup-entrance)` in `CMake/SingleExecutable.cmake` (rows at
   `:213-234`) AND add the name to the `set_tests_properties(... LABELS
   "redship")` list (`:238-245`) — a test missing from that list is built but
   never run by CI. Optionally extend the test to assert the F10-return
   sequence: set slot for MM, simulate OoT resume, assert OoT falls back to the
   frozen return entrance.
4. Open the PR referencing this decoded log + `7c6e7eec` (credit the original
   session's diagnosis); drive Linux+Windows CI (`generate-builds.yml`) to full
   green; squash-merge.
5. Ask the operator to re-test on a fresh build — the crashing build `d81a23d`
   is of unknown lineage, so post-merge confirmation must be on a build cut from
   main. If a crash still reproduces, capture the new log and go to §B.

### B. CONTINGENCY: MM-side switch-crash map (rev-1 candidates — use only if a crash survives §A)

The known log shows the OoT→MM leg completing scene+first-room load, so these
are NOT the live fault. They remain plausible latent surfaces; diagnose FROM the
new log, never by assumption.

**Merged state — what #353/#344 already landed (do NOT redo):**
- `MM_Actor_SpawnEntry(NULL)` guard — `games/mm/src/code/z_actor.c` (locate by
  symbol).
- Post-busyloop NULL-player guard — `games/mm/src/code/z_play.c:2509-2519`
  (`MM_Play_Init` returns cleanly when `GET_PLAYER` is NULL).
- Scene-load failure LOUD + contained — `MM_OTRPlay_SpawnScene`
  (`games/mm/2s2h/z_play_2SH.cpp:51-57`): logs `[MM] FATAL: failed to load scene
  resource`, early-returns on NULL `sceneSegment`.
- `Object_GetSlot` mis-binding guard — `games/mm/src/code/z_scene.c` under
  `RSBS_SINGLE_EXECUTABLE`; `VB_FASTER_FIRST_CYCLE` at
  `games/mm/2s2h/z_scene_2SH.cpp`.
- ROM-free CTests `mm-scene-parse` / `mm-scene-execute` (label `redship`) —
  `games/mm/2s2h/mm_scene_execute_test.cpp`.

**Ranked residual candidates** (switch path: `MM_Play_Init` → `MM_Play_SpawnScene`
→ `MM_OTRPlay_InitScene` → `Room_SetupFirstRoom` → `Actor_InitContext` → busyloop
`while (!Room_ProcessRoomRequest(...)) {}` → `MM_OTRfunc_800973FC`
(`z_play_2SH.cpp:64`) → post-busyloop guard):
- **#1 — `func_80123140(play, GET_PLAYER(play))` at `z_play_2SH.cpp:73`, inside
  the busyloop, BEFORE the post-busyloop guard.** Reads `player->actor.id`
  unguarded; fires when scene+room load OK but the player never spawned
  (`linkActorEntry` NULL or spawn returned NULL — see #341 elision). **Sig:**
  near-NULL READ in `func_80123140` via `Room_ProcessRoomRequest`. Fix parallels
  #353: skip when `GET_PLAYER == NULL` so control falls to the existing guard.
- **#2 — first-room load failure → `MM_OTRScene_ExecuteCommands(play, NULL)` at
  `z_play_2SH.cpp:72`** (#353 guards the *scene*, not the *room*).
  `roomRequestAddr = ResourceLoad(...).get()` NULL-unchecked
  (`z_scene_2SH.cpp:561-563`). **Sig:** the `File Name %s` print
  (`z_scene_2SH.cpp:560`) is the LAST line before the crash.
- **#3 — missing/misordered SPAWN_LIST vs ENTRANCE_LIST → NULL
  `setupEntranceList` deref** in `MM_Scene_CommandSpawnList` (`z_scene_2SH.cpp`).
  Latent (vanilla emits 0x06 before 0x00).
- **#4 — player-object overlay elision → NULL
  `gActorOverlayTable[0].profile->objectId` WRITE** in
  `MM_Scene_CommandSpawnList`. **Sig:** SIGSEGV WRITING a low address.
- **#5 — audio/graph bring-up on switch** (`GameExports_SingleExe.cpp` init
  asserts; `MM_osCartRomInit`→NULL in `mm_stubs.c`). **Sig:** dies on FIRST MM
  entry, trace in `AudioThread_*`/`MM_Game_Run` asserts; `[MM] ...` breadcrumbs
  localize how far Init got.

**Fast triage split:** `[MM] FATAL` present → scene path (should be contained;
check partial `Room_SetupFirstRoom` state). `File Name %s` is last line → #2.
Crash in `func_80123140` → #1. WRITE to low address in spawn-list handler → #4.
Audio/asserts on first entry → #5. `MM_FaultDrawer_*` is a stubbed no-op — a
native MM fault prints nothing; expect a raw segfault, not an MM fault screen.

### Method (both parts; ULTRACODE for §B diagnosis)
1. **Anchor on the log.** Extract signature: faulting symbol, address class
   (near-NULL? READ vs WRITE?), which transition leg, which breadcrumbs
   present/absent. Map onto the candidate surface BEFORE touching code.
2. **Fan out to diagnose** (§B) — one sub-agent per implicated subsystem; each
   reports whether the signature is *consistent* with its subsystem.
3. **Adversarially verify — refute by default.** Confirm a candidate only when
   the signature is inconsistent with every alternative.
4. **Fix exactly the confirmed fault** — minimal contained guard in #353's
   style; no broad refactors.
5. **Lock it with a ROM-free test** in the `redship` label; PR; full green; merge.

### ROM-free verification playbook
Every fix MUST be locked in the **`redship` CTest label** — the only tier hosted
CI compiles+runs without ROMs. Template: `games/mm/2s2h/mm_scene_execute_test.cpp`
(read it fully). Patterns:
- **A. Field-writing scene handler → mm-scene-execute pattern**: little-endian
  wire buffer `[u32 count][per cmd: u32 opcode + payload]` (payload widths must
  match `resource/importer/scenecommand/Set*Factory.cpp`) → wrap as
  `Ship::File`+`BinaryReader`(LE)+`ResourceInitData`(`SOH_Room`, ver 0,
  `RESOURCE_FORMAT_BINARY`) → `ReadResource` → POISON output fields on a
  value-init'd `PlayState` with sentinels → `MM_OTRScene_ExecuteCommands` →
  assert. **Only pure-write handlers** — anything touching object
  system/allocator/overlays/`ResourceLoad` null-derefs a zeroed PlayState.
- **B. Guard/early-return fix → direct call**: heap-zeroed structs, call at the
  guard boundary (e.g. `MM_Actor_SpawnEntry(ctx, nullptr, play)` → NULL).
- **C. Entangled crash math → extract a pure helper**, call directly.
- **Common-code fixes (§A) → plain test_runner tests**: `src/common` logic
  (entrance table, context, save) is directly testable in
  `src/common/test_runner.cpp` — no MM-TU gymnastics needed.
- **MM-TU mechanics**: `test_runner.cpp` can't include MM `global.h` → put
  MM-type tests in a `games/mm/2s2h/*.cpp` under `#ifdef RSBS_SINGLE_EXECUTABLE`,
  expose `extern "C" int X_RunHeadless(void)` (0=pass); auto-globbed into
  2ship_port; the undefined ref force-pulls it. `.cpp` callees = plain C++ decl;
  `.c` callees = `extern "C"`. Wrong = link error.
- **Three REQUIRED CTest wiring edits:** (1) `gTests[]` row in
  `test_runner.cpp` (keep `archive-hotswap-logic` LAST — it re-inits the
  entrance table); (2) `add_test(...)` in `CMake/SingleExecutable.cmake:213-234`;
  (3) **add the name to the `set_tests_properties(... LABELS "redship")` list**
  (`:238-245`) — load-bearing; a test not in this list is built but NOT run by
  CI. `redship` is display-free (no Xvfb); display-needing tests go in the
  `rando` label.

### CI reality
`generate-builds.yml` triggers on `pull_request`; build-linux/build-windows each
build `redship` + run `ctest --label-regex "^redship$" --no-tests=error`
(`generate-builds.yml:262`; `rando` label under xvfb at `:267`). Nothing runs
tests on push-without-PR. The `int-*` tier boots the real binary, needs ROMs,
and `integration-tests.yml` is `workflow_dispatch`-only — never on a PR. The
deferred deep `Play_Init` failure-*recovery* (make the spawn-scene path return
s32 so `Play_Init` unwinds instead of leaving a player-less PlayState) can only
be VALIDATED on a real-ROM boot → stays a documented open item.

**Gameplay crash repro (2026-07-18):** `docs/ci-gameplay-repro-postmortem.md`
explains why this crash class shipped through green CI and documents
`redship --integration-test int-gameplay-roundtrip` — the programmatic version
of the operator's manual repro (debug save → live gameplay → production
OoT↔MM round trip incl. the resume leg → debug warp → door transition),
parameterized via `RSBS_GP_*` env vars, with crash-log artifact upload wired
into `integration-tests.yml` and a documented agent loop for build→repro→
observe→bisect on any ROM-equipped machine.

### Recommended separate PR — the #341 elision-prevention track
Live class-level hazard regardless of the current crash cause; ROM-free.
- OoT wraps only `soh_rando` in WHOLE_ARCHIVE (`games/oot/CMakeLists.txt:276`,
  rationale `:268-275`). MM wraps NONE. `--start-group` does NOT defeat member
  elision; Windows `/FORCE:MULTIPLE` silently drops duplicates. Exposure ~208
  self-registering `RegisterShipInitFunc` TUs, concentrated in **2ship_enh** +
  **2ship_rando**. Resource factories are registered explicitly and actors are
  table-driven → NOT at risk; scope to enh+rando.
- **Option A:** wrap `2ship_enh`+`2ship_rando` in
  `$<LINK_LIBRARY:WHOLE_ARCHIVE,...>`. A hard duplicate-symbol link error vs
  `mm_stubs.c` is the GOOD outcome — surfaces in CI. Get **Linux** clean first
  (Windows `/FORCE:MULTIPLE` can hide a collision Linux exposes).
- **Option C:** `nm`-based post-link CI check — assert each MM archive's
  `_GLOBAL__sub_I_*` registrar symbols appear in linked `redship`. Detects the
  CLASS regardless of cause. Skip Option B (explicit registrar list — churn).
- Keep SEPARATE from the crash-fix PR unless a log proves elision is the root cause.

### Stub + deferred inventory (audit only what a trace implicates)
`src/common/mm_stubs.c`: most stubs safe (libc/OS aliases; excluded
enhancement/UI). Flag if a trace lands near boot/audio: `MM_osCartRomInit`→NULL;
the `GameInteractor_Execute*` no-op family. Small cleanup candidates surfaced by
the log decode: stale MM `OPEN_DISPS` records in the shared GraphicsContext
survive a switch (cosmetic — pollutes crash dumps' "GFX Stack"); consider
resetting gfx-stack bookkeeping on game switch. Deferred headless-safe test
locks you MAY add opportunistically (pure writes, pattern A): Mesh 0x0A,
PathList 0x0D, CutsceneScriptList. Nice-to-have, not the mission.

### Deliverable (this lane)
1. §A fix PR merged: `7c6e7eec` rebased onto main + dedicated
   `StartupEntrance` CTest row, green on Linux+Windows, squash-merged, with the
   diagnosis recorded in the PR body.
2. Operator confirmation on a fresh main build that the reported crash is gone;
   if anything still crashes, a new decoded log → §B loop.
3. (Recommended, separate PR) the #341 Option A + Option C track.
4. Any fault you can't lock ROM-free (e.g. deep `Play_Init` recovery) →
   documented open item; do not fake CI coverage.

**Closure criteria:**
- [x] §A entrance-leak fix merged (squash, CI green) with test coverage and the
      diagnosis recorded — PR #356, main `cff754fc` (2026-07-16).
- [ ] Crash no longer reproduces on a fresh build from main (operator-confirmed;
      real-ROM confirmation is manual). Operator re-test must be on a build ≥
      `1f5fa3a5` (both #356 and #357 included).
- [ ] §B dispositioned: candidates #1/#2 guarded+locked via PR #357
      (`1f5fa3a5`); #3-#5 remain live pending the operator's next log.
- [ ] #341 elision track scoped (merged, or a tracked follow-up issue).
- [ ] This file replanned (see below).

---

## Replanning + keeping this file current (REQUIRED)

This file is the living worker roadmap; it already went stale once (the Wave-2
lanes sat here for a month after they closed). Do not repeat that:

1. **Before starting:** re-verify the "Completed (Wave 2)" claims against `main`
   (issues actually closed, PRs merged) and correct anything wrong. Confirm the
   Active-lane file:line anchors against current `main` — if it moved, locate by
   symbol (`Combo_CheckEntranceSwitch`, `Entrance_SetStartupEntrance`,
   `OoT_Play_Init`, `MM_OTRfunc_800973FC`, `func_80123140`,
   `MM_OTRScene_ExecuteCommands`), not by line.
2. **As you work:** tick the Active-lane closure criteria here with merge SHAs,
   same discipline the Wave-2 lanes used.
3. **When the crash-stabilization lane closes: REPLAN.** Re-derive the roadmap
   toward pre-alpha readiness (**epic #321**). Likely next lanes: (a) the #341
   elision-prevention PR; (b) remaining MM single-exe stubs/port-gaps the crash
   work surfaces; (c) the deferred deep `Play_Init` failure-recovery (needs a
   real-ROM boot to validate); (d) #321 pre-alpha gates (BYO-archive UX,
   known-issues doc, manual QA #310). **Rewrite THIS file as "Wave 4"** the way
   Wave 3 superseded Wave 2: move closed lanes to Completed, write fresh
   self-contained lane goals (closure criteria + steps + safety rails), date it,
   keep it accurate to `main` (not aspirational).
4. Commit the `worker-prompts.md` update alongside (or right after) the lane's PR.
