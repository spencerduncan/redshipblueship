# Worker loop goals — Wave 3 (updated 2026-07-16)

Supersedes **Wave 2** (the 2026-06-10 Lanes 5–6 that used to fill this file — all
complete, see git history). Wave 2 was: Lane 5 = SOH shuffle feature ports
#289–#293 / epic #235; Lane 6 = Phase 2 closeout / epic #202 / #212. That entire
roadmap is **done** and predates the MM single-exe stabilization effort
(#340→#353), which is the live work now. This file went stale for a month after
those lanes closed — §"Replanning" below exists so that does not recur.

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

---

## Active lane — MM single-exe cross-switch crash stabilization (crash-log-first, ULTRACODE)

> Run as an ULTRACODE task. The operator will hand you the **actual crash log**
> with this prompt. The log is the primary anchor — everything below is the map.

### 0. THE ONE FRAMING THAT MATTERS MOST
**The crash log is the PRIMARY anchor. Diagnose FROM it. Do NOT assume any candidate.**
PR #353 (`8b8d3f0`) already root-caused and CONTAINED one failure (the scene-load
failure-propagation gap). The build **still** crashes on switch → the fault is
elsewhere/downstream. If you assume "it's the propagation path again," you waste
the session. Instead: (1) extract the log's signature — assert text, faulting
symbol (mangled/demangled), faulting address (near-NULL READ vs WRITE? what
offset?), stack frames, and **which transition** it dies on (first HMS→MM
`MM_Game_Init` vs a later `MM_Game_Resume`); (2) note which breadcrumb log lines
are present/absent (§2 triage split); (3) map onto §2 BEFORE touching code; (4)
only then hypothesize and verify.

### 1. CURRENT MERGED STATE — what #353 landed (do NOT redo)
- **`MM_Actor_SpawnEntry(NULL)` guard** — `games/mm/src/code/z_actor.c:3872-3874`.
- **Post-busyloop NULL-player guard** — `games/mm/src/code/z_play.c:2516-2518`
  (`MM_Play_Init` returns cleanly when `GET_PLAYER` is NULL; `state.main`/`destroy`
  already set at 2450-2451).
- **Scene-load failure is LOUD + contained** — `MM_OTRPlay_SpawnScene`
  (`games/mm/2s2h/z_play_2SH.cpp:51-57`) logs `[MM] FATAL: failed to load scene
  resource` and early-returns on NULL `sceneSegment`.
- **`Object_GetSlot` mis-binding** to OoT's `GameInteractor_Should` —
  `games/mm/src/code/z_scene.c:116-126` (`RSBS_SINGLE_EXECUTABLE` guard); plus
  `VB_FASTER_FIRST_CYCLE` at `games/mm/2s2h/z_scene_2SH.cpp:299-316`.
- **New ROM-free CTest `mm-scene-execute`** (label `redship`) —
  `games/mm/2s2h/mm_scene_execute_test.cpp` via `extern "C" MM_SceneExecute_RunHeadless()`
  in `src/common/test_runner.cpp`. Extracted `MM_Play_ResolveLinkActorEntry`
  (`z_scene_2SH.cpp:53-60`).

**Systemic thesis (why these are a hidden CLASS):** (1) linker elision (#341) —
no `2ship_*` archive is `WHOLE_ARCHIVE` (`games/mm/CMakeLists.txt:324`) unlike
`soh_rando` (`games/oot/CMakeLists.txt:276`); self-registering TUs drop silently.
(2) silent no-op stubs (`src/common/mm_stubs.c`). (3) port-gaps. All invisible
because the only real-MM-gameplay tests (`int-boot-mm`) are ROM-gated and never
run on hosted CI (`integration-tests.yml` is `workflow_dispatch`-only).

### 2. RANKED CANDIDATE CRASH SURFACE (diagnostic map)
Switch path: `MM_Play_Init` (`z_play.c:2230`) → `MM_Play_SpawnScene` (2412) → on
success `MM_OTRPlay_InitScene` runs scene cmds (sets `linkActorEntry`,
`setupEntranceList`, room list) → `Room_SetupFirstRoom` (`status=1`, loads first
room) → `Actor_InitContext(...linkActorEntry)` (2498) spawns player → busyloop
`while (!Room_ProcessRoomRequest(...)) {}` (2501) → `MM_OTRfunc_800973FC`
(`z_play_2SH.cpp:64`) runs first-room cmds (72), then `func_80123140(play,
GET_PLAYER(play))` (73), then `MM_Actor_SpawnTransitionActors` (74) → #353 guard
(2516). `GET_PLAYER` = `(Player*)play->actorCtx.actorLists[ACTORCAT_PLAYER].first`
(`z64play.h:140`) — NULL when no player actor spawned.

Because #353 made total scene-load failure loud+contained, a remaining crash most
likely means **the scene DID load and the fault moved downstream**. Ranked:

- **#1 (TOP) — `func_80123140(play, GET_PLAYER(play))` at `z_play_2SH.cpp:73`, in
  the busyloop, BEFORE the 2516 guard.** `func_80123140` (`z_player_lib.c:602`)
  reads `player->actor.id` with no NULL check. Fires when scene+first-room load OK
  but the player never entered the PLAYER list — `linkActorEntry` NULL (no
  SPAWN_LIST) or `Actor_SpawnAsChildAndCutscene` returned NULL (player overlay
  elided per #341, or player object not loaded). **Sig:** SIGSEGV near-NULL READ
  small offset in `func_80123140`, via `MM_OTRfunc_800973FC` →
  `Room_ProcessRoomRequest` (`z_room.c:616`) → `z_play.c:2501`. Fix parallels
  #353: guard line 73 (skip `func_80123140` when `GET_PLAYER==NULL`) so control
  falls to the 2516 guard.
- **#2 — first-room load failure → `MM_OTRScene_ExecuteCommands(play, NULL)` at
  `z_play_2SH.cpp:72`** (#353 guards the *scene*, NOT the *room*).
  `MM_OTRfunc_8009728C` (`z_scene_2SH.cpp:561-563`) sets `roomRequestAddr =
  ResourceLoad(fileName).get()` — NULL if room file missing/mis-parsed, unchecked
  → `scene->commands.size()` (`:514`) derefs NULL. **Sig:** SIGSEGV in
  `MM_OTRScene_ExecuteCommands` off a NULL `S2H::Scene*`; the `printf("File Name
  %s\n", ...)` at `z_scene_2SH.cpp:560` is the **last log line before the crash**.
- **#3 — missing/misordered SPAWN_LIST vs ENTRANCE_LIST → NULL `setupEntranceList`
  deref.** `MM_Scene_CommandSpawnList` (`z_scene_2SH.cpp:64`) →
  `MM_Play_ResolveLinkActorEntry(setupEntranceList, curSpawn, entries)`; NULL if
  0x06 hasn't run. Also `Room_SetupFirstRoom:550`. Latent (vanilla emits 0x06
  before 0x00). **Sig:** near-NULL READ in `MM_Scene_CommandSpawnList` via
  `MM_OTRScene_ExecuteCommands:531` under `MM_OTRPlay_InitScene`.
- **#4 — player-object overlay elision → NULL `gActorOverlayTable[0].profile->objectId`
  WRITE at `z_scene_2SH.cpp:79`.** If `ovl_player_actor`'s profile isn't
  linked (#341), `.profile` is NULL → write to near-NULL. Same root as #1(b) but
  earlier. **Sig:** SIGSEGV WRITING a low address in `MM_Scene_CommandSpawnList`.
- **#5 (LOWER) — audio/graph bring-up on switch (`GameExports_SingleExe.cpp`).**
  First HMS→MM runs `MM_Game_Init` audio bring-up (779-784) + `MM_Game_Run`
  asserts (800-803). `MM_osCartRomInit`→NULL (`mm_stubs.c:63`) is on this path.
  **Sig:** trace in `AudioThread_*`/`MM_AudioMgr_Init`/`MM_Game_Run` asserts, or
  dying on FIRST MM entry vs a resume. `[MM] ...` fprintf breadcrumbs
  (`GameExports_SingleExe.cpp:694-806`) localize how far Init got.

**Fast triage split (with the log in hand):** `[MM] FATAL: failed to load scene
resource` present then crash → scene path (should be contained; check for partial
`Room_SetupFirstRoom` leaving `status==1`). No FATAL, `File Name %s` present,
crash in `MM_OTRScene_ExecuteCommands` → **#2**. No FATAL, crash in
`func_80123140` → **#1**. Crash WRITING low address in `MM_Scene_CommandSpawnList`
→ **#4**. Crash in `AudioThread_*`/`MM_Game_Run` asserts → **#5**. Note:
`MM_FaultDrawer_*` is stubbed no-op (`mm_stubs.c:66-76`) — a native MM fault
prints nothing; expect a raw segfault, not an MM fault screen.

### 3. ULTRACODE METHOD
1. **Fan out to diagnose** — one sub-agent per subsystem the log could implicate
   (scene-execute, room-load, player/overlay spawn, audio/graph bring-up, elision
   #341). Feed each the exact faulting symbol/address/frame; each reports whether
   the signature is *consistent* with its subsystem.
2. **Adversarially verify — refute by default.** For each survivor, try to
   DISPROVE it: does the faulting offset match the derefed field? does the frame
   chain exist on that path? is the predicted breadcrumb present/absent? Confirm
   only when the signature is inconsistent with every alternative.
3. **Fix exactly the confirmed fault** — minimal contained guard/early-return in
   #353's style. Don't refactor broadly; don't fix candidates the log doesn't force.
4. **LOCK it with a ROM-free test** (§4) in the `redship` label.
5. **Open a PR**, drive Linux+Windows CI to green.

### 4. ROM-FREE VERIFICATION PLAYBOOK
Every MM fix MUST be locked in the **`redship` CTest label** — the only tier
hosted CI compiles+runs without ROMs. Template: `games/mm/2s2h/mm_scene_execute_test.cpp`
(read it fully). Patterns:
- **A. Field-writing scene handler → mm-scene-execute pattern**
  (`mm_scene_execute_test.cpp:70-188`): little-endian wire buffer
  `[u32 count][per cmd: u32 opcode + payload]` via `MMSceneExec_PushU32` (payload
  widths must match `resource/importer/scenecommand/Set*Factory.cpp`) → wrap as
  `Ship::File`+`BinaryReader`(LE)+`ResourceInitData`(`SOH_Room`,ver 0,
  `RESOURCE_FORMAT_BINARY`) → `ResourceFactoryBinarySceneV0::ReadResource` →
  POISON output fields on a value-init'd `make_unique<PlayState>()` with distinct
  sentinels → `MM_OTRScene_ExecuteCommands` → assert. **Only exercise pure-write
  handlers** — anything touching object system/allocator/overlays/`ResourceLoad`
  null-derefs a zeroed PlayState (excluded: SetStartPositionList 0x00,
  `Scene_CommandCutsceneList` at `:453`). Verify pure-write in `z_scene_2SH.cpp`
  (executor at `:510`) before use.
- **B. Guard/early-return fix → direct call** (`:262-278`): heap-zeroed structs,
  call at the guard boundary (e.g. `MM_Actor_SpawnEntry(ctx, nullptr, play)` → NULL).
- **C. Entangled crash math → extract a pure helper** (`:200-207`), call directly.
- **MM-TU mechanics** (`mm_scene_execute_test.cpp:13-84`, `test_runner.cpp:24-74`):
  `test_runner.cpp` can't include MM `global.h` → put MM-type tests in a
  `games/mm/2s2h/*.cpp` under `#ifdef RSBS_SINGLE_EXECUTABLE`, expose
  `extern "C" int X_RunHeadless(void)` (0=pass); auto-globbed into 2ship_port
  (`games/mm/CMakeLists.txt:101`); the undefined ref force-pulls it. **Linkage:**
  `.cpp` callees (e.g. `MM_OTRScene_ExecuteCommands`) = plain C++ decl; `.c`
  callees (e.g. `MM_Actor_SpawnEntry`) = `extern "C"`. Wrong = link error.
- **Three REQUIRED CTest wiring edits:** (1) `gTests[]` row in
  `test_runner.cpp:517-543` (keep `archive-hotswap-logic` LAST); (2)
  `add_test(...)` in `SingleExecutable.cmake:213-234`; (3) **add the name to the
  `set_tests_properties(... LABELS "redship")` list** (`:238-245`) — load-bearing;
  a test not in this list is built but NOT run by CI. `redship` is display-free
  (no Xvfb); display-needing tests go in the `rando` label.

### 5. CI REALITY
`generate-builds.yml` triggers on `pull_request`; `build-linux`/`build-windows`
each build `redship` + run `ctest --label-regex "^redship$" --no-tests=error`
(ROM-free, both OSes; a mistyped label FAILS loudly). Nothing runs the tests on
push-without-PR. The `int-*` tier boots the real binary, needs ROMs, and
`integration-tests.yml` is `workflow_dispatch`-only — never on a PR. The deferred
deep `Play_Init` failure-*recovery* (make `MM_OTRPlay_SpawnScene`/`MM_Play_SpawnScene`
return an s32 so `Play_Init` unwinds cleanly instead of leaving a player-less
PlayState whose `state.main=MM_Play_Main` runs next frame — `z_play.c:2412,2450,2498`)
can only be VALIDATED on a real-ROM boot → stays a documented open item.

### 6. RECOMMENDED SEPARATE PR — the #341 elision-prevention track
Live class-level hazard regardless of the current crash cause; ROM-free.
- OoT wraps only `soh_rando` (`games/oot/CMakeLists.txt:276`, rationale `:268-276`).
  MM wraps NONE (`games/mm/CMakeLists.txt:324`). `--start-group` (root
  `CMakeLists.txt:262`) does NOT defeat member elision; Windows `/FORCE:MULTIPLE`
  (`:275`) silently drops duplicates. Exposure ~208 self-registering
  `RegisterShipInitFunc` TUs (`ShipInit.hpp`, `InitAll` at `BenPort.cpp:866`),
  concentrated in **2ship_enh** (182/196) + **2ship_rando** (21/137). Resource
  factories are registered explicitly (`BenPort.cpp:293-331`) and actors are
  table-driven → NOT at risk; scope to enh+rando.
- **Option A:** wrap `2ship_enh`+`2ship_rando` in `$<LINK_LIBRARY:WHOLE_ARCHIVE,...>`
  at `:324`. Watch for Linux duplicate-symbol vs `mm_stubs.c` (a hard link error
  there is the GOOD outcome — surfaces in CI; the `GameInteractor_Execute*` stubs
  at `mm_stubs.c:83-115` do NOT collide, MM's GI is excluded). Get **Linux** clean
  first (Windows `/FORCE:MULTIPLE` can hide a collision Linux exposes).
- **Option C:** `nm`-based post-link CI check — assert each MM archive's
  `_GLOBAL__sub_I_*` registrar symbols appear in linked `redship`. Detects the
  CLASS regardless of cause. Skip Option B (explicit registrar list — churn).
- Keep SEPARATE from the crash-fix PR unless the log proves elision is the root cause.

### 7. STUB + DEFERRED INVENTORY (audit only what the trace implicates)
`src/common/mm_stubs.c`: most stubs safe (libc/OS aliases `:23-59`; excluded
enhancement/UI `:83-191`). Flag if the trace lands near boot/audio: `MM_osCartRomInit`
→NULL (`:63`); the `GameInteractor_Execute*` no-op family (`:83-117`). Deferred
headless-safe test locks you MAY add opportunistically (pure writes, pattern A):
Mesh 0x0A (`z_scene_2SH.cpp:167`), PathList 0x0D (`:263`), CutsceneScriptList
(`:386` — NOT `CutsceneList` at `:453`). Nice-to-have, not the mission.

### 8. DELIVERABLE (this lane)
1. Crash-fix PR on `claude/<desc>`: minimal confirmed fix + ROM-free `redship`
   test that reproduces-then-locks it, green on Linux+Windows, squash-merged.
2. Written diagnosis: log signature, matched candidate (§2), how you refuted the
   others, why the fix addresses it.
3. (Recommended, separate PR) the #341 Option A + Option C track.
4. Any fault you can't lock ROM-free (e.g. deep `Play_Init` recovery) → documented
   open item; do not fake CI coverage.

**Closure criteria:**
- [ ] Crash-fix PR merged (squash, CI green) with the diagnosis recorded.
- [ ] The switch-to-MM crash in the provided log no longer reproduces (as far as
      ROM-free CI + code evidence can show; real-ROM confirmation is manual).
- [ ] #341 elision track scoped (merged, or a tracked follow-up issue).
- [ ] This file replanned (see below).

---

## Replanning + keeping this file current (REQUIRED)

This file is the living worker roadmap; it already went stale once (the Wave-2
lanes sat here for a month after they closed). Do not repeat that:

1. **Before starting:** re-verify the "Completed (Wave 2)" claims against `main`
   (issues actually closed, PRs merged) and correct anything wrong. Confirm the
   Active-lane file:line anchors against current `main` — if it moved, locate by
   symbol (`MM_Play_Init`, `MM_OTRfunc_800973FC`, `func_80123140`,
   `MM_OTRScene_ExecuteCommands`), not by line.
2. **As you work:** tick the Active-lane closure criteria here with merge SHAs,
   same discipline the Wave-2 lanes used.
3. **When the crash-stabilization lane closes: REPLAN.** Re-derive the roadmap
   toward pre-alpha readiness (**epic #321**). Likely next lanes: (a) the #341
   elision-prevention PR (§6); (b) remaining MM single-exe stubs/port-gaps the
   crash work surfaces; (c) the deferred deep `Play_Init` failure-recovery (needs
   a real-ROM boot to validate); (d) #321 pre-alpha gates (BYO-archive UX,
   known-issues doc, manual QA #310). **Rewrite THIS file as "Wave 4"** the way
   Wave 3 superseded Wave 2: move closed lanes to Completed, write fresh
   self-contained lane goals (closure criteria + steps + safety rails), date it,
   keep it accurate to `main` (not aspirational).
4. Commit the `worker-prompts.md` update alongside (or right after) the lane's PR.
