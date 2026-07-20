# Phase 3 lane prompts — ultracode worker briefs

> Written 2026-07-20 against `main` (77cf5508) + PR #404, after the #389–#400 merge
> weekend; every anchor was re-verified on this date and the whole doc then passed an
> adversarial verification sweep. **Do not trust line numbers in
> `docs/phase3-roadmap.md` / `docs/phase3-execution-prompt.md` where they conflict
> with this file; several of their claims are stale** (noted inline). Line anchors
> drift as PRs merge — in particular **PR #405 (merged 2026-07-20 after this doc's
> sweep) rewrote parts of `games/mm/CMakeLists.txt`, so anchors into that file are
> pre-#405 and must be re-derived**. Treat symbols as authoritative, lines as hints,
> and re-verify before editing.
>
> Companion docs: #392 (tracker epic), `docs/phase3-execution-prompt.md` (phase plan),
> `.claude/worker-prompts.md` (standing conventions — read it first, it is short).

## How to run these

**Venue.** `cloud` = a Linux worker without ROMs: it can compile, link, observe
duplicate symbols with `nm` (the project's only working ODR gate), and run the
`redship` (display-free) and `rando` (xvfb) CTest tiers — everything hosted CI runs.
`local` = the operator's Windows box: the only place with `oot.o2r`/`mm.o2r`, the only
place `redship.exe` actually executes on Windows (CI's Windows job is build-only), and
the scarce resource of the whole phase. Default to cloud.

**The single operator sitting.** All hands-on verdicts batch into ONE sitting, not
per-lane sessions: the #310 manual-QA checklist (still-open #321 gate — fold it in,
do not schedule it separately), F1's `redship.exe --version` exit-code check, G1's
four-mode integration-hook arming check, A1's item-crossing confirmation, B's seed
confirmation, and C1's foreign-item pickup/redemption. Each brief below files its
operator asks on #392 as it merges; whoever runs the sitting works from that list.

**Model.** `fable` = design decisions with long shadows and ODR/linker forensics
(wrong choices here compile, pass CI, and corrupt memory or serialization formats).
`opus` = well-scoped engineering where the shape is settled and the risk is in
execution. `sonnet` = mechanical, fully-specified sweeps with a hard verification
step.

**Merge-order prerequisite (do this before branching any worker).** PRs #404, #401,
#402 are all CI-green and all rewrite the test-registration region of
`CMake/SingleExecutable.cmake` (#401/#402 also share `src/common/test_runner.cpp`).
Merge them (suggested order: #404, then #401, then #402, rebasing each) before
spawning workers; every brief below assumes all three are on `main`. **The rebase
hides a conversion task:** #401's `add_test(NAME ActiveQueue ...)` and #402's
`add_test(NAME MMCullingBinding ...)` are in the pre-#404 plain style — after #404
merges they MUST be converted to `redship_add_test(...)` rows, or configure passes
and the `TestRegistrationComplete` guard fails CI ("dispatch entry has no CTest
row"). PR #405 (`games/mm/CMakeLists.txt` force-include restructuring) already
merged; it is why that file's line anchors below are suspect.

New tests are one appended
`redship_add_test(NAME <Name> COMMAND redship --test <dispatch-name>)` line in
`CMake/SingleExecutable.cmake` plus a `gTests[]` row and `#include` in
`src/common/test_runner.cpp` (test sources are textually `#include`d into
`test_runner.cpp`, not compiled standalone; C-linkage tests go inside the
`extern "C"` block — wrong placement mis-binds symbols, see
`CMake/RedshipTests.cmake:120-131`). The `TestRegistrationComplete` guard fails CI if
either half is forgotten.

**File-ownership ordering** (the three contended files):
- `games/mm/2s2h/GameExports_SingleExe.cpp`: #402 (merged) → **G1** → **A1**'s MM
  worker → **C**. Branch in that order; do not run G1 and A1-MM concurrently.
- `games/mm/CMakeLists.txt`: #405 (merged) → **G1** (sole owner while it runs) →
  **F2**'s warning-escalation hunk lands after G1's merge (F2 can do everything else
  first) → **C** inherits.
- `src/common/mm_stubs.c`: #402 (merged, +14 lines) → **G1** (owns it, absorbs #372).
- `src/common/context.h` / `context.cpp` / `save.cpp` /
  `tests/test_save_roundtrip.c`: **A0** exclusive while it runs. The shared fields
  are also referenced by `tests/test_roundtrip_integrity.c` and
  `tests/test_shared_state_roundtrip.c` — A0 retires-in-place to avoid editing them;
  A1 extends the latter.

**Explicit deferrals** (so silent omissions are distinguishable): Lane D (cross-game
logic), #383's UIWidgets item, #386 (MM FB_* framebuffer binding), #369 (whole-tree
reformat), #376 items 4–6 — all deferred out of 3.0 per #392. #379 (cleanup batch)
is unassigned backlog; H may absorb it opportunistically.

**Dependency graph.**

```
G1 (#395/#383 GameInteractor shim, absorbs #372)──┐
G2 (#384 renames + arm #375)──────────────────────┼──► C0 (2ship_rando reachable)
A0 (item-id ADR + struct carve)──┬─► A1 (producers/consumers, absorbs #373)─┐
                                 └─► B  (unified seed determinism)──────────┼─► C1 (foreign items)
F1 (#396 Fault A)     — independent                                         │
F2 (#403 + stub returns) — after #401 merges; CMake hunk after G1           │
H  (harness gates + correctness tail) — independent                         │
```

Start immediately in parallel: G1, G2, A0, F1, F2 (non-CMake parts), H. Then A1 + B
(after A0), then C0 (after G1 + G2), then C1 (after A0 + A1 + B + C0 — the epic's
"C without A is unverifiable" gate is real).

| ID | Lane | Venue | Model | Size/risk |
|---|---|---|---|---|
| G1 | GameInteractor extern-C shim (#395 + #383 + #372) | cloud (operator batch: arming check) | fable | M / high |
| G2 | #384 registrar renames + arm #375 baseline | cloud | sonnet | S / low |
| F1 | Fault A: OTRExporter global namespacing (#396) | cloud (operator batch: exit check) | opus | S / medium |
| F2 | MM fault-handler + non-void stub returns (#403) | cloud | sonnet | M / low |
| H | Harness gates + correctness tail (#376, #377, #365, #380) | cloud | opus | S each / low |
| A0 | Shared item-id ADR + ComboContext carve | cloud | fable | M / medium |
| A1 | Shared-state producers+consumers (+#373) | cloud (1–2 workers, ordered) | opus | M / low |
| B | Unified seed → paired world | cloud | opus | M / medium |
| C | 2ship_rando reachability + OoT→MM foreign items | cloud dev + operator batch | fable | L / high |

---

## G1 — GameInteractor extern-C shim (#395 + #383's GameInteractor item + #372)

**Venue:** cloud. **Model:** fable. **Branch after #402 merges** — it adds 63 lines
to `games/mm/2s2h/GameExports_SingleExe.cpp` and 14 to `src/common/mm_stubs.c`, both
yours. **File ownership:** `games/mm/2s2h/GameExports_SingleExe.cpp`,
`src/common/mm_stubs.c`, `games/mm/CMakeLists.txt` (sole owner — #401/#402/#404
don't touch it; #405 already merged into it, so re-derive its line anchors), plus any
new shim header under `games/mm/include/`.

You are fixing the confirmed cross-port ODR hazard tracked as #395 and the
GameInteractor item of #383, and absorbing #372 (a `mm_stubs.c` GameInteractor stub)
while you own that file. Read #395 including its comments; #383 has no comments — its
UIWidgets item's deferral is recorded at `docs/phase3-execution-prompt.md:102`, and
its ShipInit item was already fixed in `c8c47fa6` (`games/mm/2s2h/ShipInit.hpp:36`).
Do not touch the UIWidgets item.

Ground truth (verified 2026-07-20):
- OoT's `class GameInteractor` (`games/oot/soh/Enhancements/game-interactor/GameInteractor.h:192`)
  has exactly one non-static data member, `HOOK_ID nextHookId` (`:229`) — sizeof 4
  (probe-verified in #395). The shared `Instance` is allocated with this layout at
  `games/oot/soh/OTRGlobals.cpp:1709`, with a second lazy allocation at
  `games/oot/soh/Enhancements/game-interactor/GameInteractor_Hooks.cpp:517` — account
  for both.
- MM's `class GameInteractor` (`games/mm/2s2h/GameInteractor/GameInteractor.h:171`)
  has `std::vector<GIEvent> events` (`:178`) and a `std::variant` `currentEvent`
  (`:179`) before `nextHookId` (`:182`) — sizeof 104, `nextHookId` at offset 96
  (probe-verified). This header is force-included into every MM C++ TU (the
  `/FI` / `-include` machinery in `games/mm/CMakeLists.txt` — restructured by #405,
  find the current lines), while MM's implementation TUs are excluded
  ("GameInteractor (use OoT's)"). So all compiled MM code that touches
  `GameInteractor::Instance` reads/writes MM offsets into OoT's 4-byte allocation.
- Today's live out-of-bounds writers are the four `RegisterGameHook` calls in
  `MM_RegisterIntegrationTestHooks` (`GameExports_SingleExe.cpp:342/:373/:414/:497`
  pre-#402 — lines shift after it merges), gated behind `IntegrationTest_IsActive()`,
  so the write fires only under `--integration-test`. It is NOT the Fault A cause
  (#396 comments confirm). All four register the SAME hook type
  (`GameInteractor::OnGameStateMainStart`), so your registration surface can start as
  a single C entry point. They sit in four different integration-mode branches with
  four stderr markers (`:329` boot-MM, `:364` T1, `:408` T2, `:490` T4) — the arming
  check must cover all four modes, not just boot.
- `2ship_enh` accesses like `games/mm/2s2h/Enhancements/DemoBehavior.cpp:34`
  (`Instance->events.push_back`) bite only when those objects enter the link — which
  Lane C's work will cause. That is why this gates Lane C.

Constraints, all load-bearing:
1. **Do NOT apply the S2H namespace pattern here.** MM's only allocator is
   `BenPort.cpp:849`, an excluded TU — namespacing MM's class produces a
   never-allocated `S2H::GameInteractor::Instance`: four null derefs and silently
   un-armed integration tests, strictly worse than the OOB write. #395's body and
   `games/mm/CMakeLists.txt`'s "use OoT's" exclusion document why sharing is
   architectural; `ShipInit.hpp:12-35` documents the namespace-split precedent and
   why it applied THERE (header-only, no excluded allocator).
2. **MM VB ordinals alias OoT's** (`src/common/mm_stubs.c:82-92`: MM
   `VB_SETUP_TRANSITION` == OoT `VB_PLAY_RAINBOW_BRIDGE_CS` == 206). Routing MM hook
   registrations into OoT's registry raw runs OoT handlers against MM state. Keep MM
   hook ids in an MM-owned registry behind the shim, or prove ordinal-safety per
   routed hook and lock the proof.
3. **Design for Lane C's real access pattern, not just hooks.** MM's Rando code
   directly reads/writes `Instance->events` and `Instance->currentEvent`
   (`Rando/MiscBehavior/CheckQueue.cpp:40/:127-128`) — MM-only DATA members at
   offsets 8..96 with no OoT equivalent; they cannot be routed through a
   hook-registry shim. Your shim design must include (or explicitly plan for)
   MM-owned storage for the GIEvent queue, and note that `Rando::Init()`
   (`Rando.cpp:31`) and `MiscBehavior::Init()` (`MiscBehavior.cpp:12`) also make raw
   `RegisterGameHook` calls that Lane C must migrate onto the shim BEFORE
   `MM_Rando_Init` ever runs — otherwise C's first boot fires the OOB write
   unconditionally. Put this contract in your PR body and on #392.
4. Before deciding which side "wins" today, inspect a real Linux build's map/nm for
   the COMDAT fold winner of the shared `RegisterGameHook<...>` instantiations — the
   research pass could not determine it from source; do not build on an unverified
   assumption.

Shim precedents to imitate: `games/mm/2s2h/GameExports_SingleExe.cpp:909-941`
(`extern "C"` AudioEditor shims) and `:1249-1255` (`GameInteractor_ExecuteOnRoomInit`
C wrappers over C++ `ExecuteHooks` on the shared instance). Three prefix-header
precedents exist: `games/mm/include/mm_audio_prefix.h`,
`mm_frame_interpolation_prefix.h`, and #402's `mm_ship_utils_prefix.h`.

Also in scope (#372, same file): `GameInteractor_InvertControl` in
`src/common/mm_stubs.c` returns the enum ordinal instead of a ±1 multiplier —
verified sole definition in the single-exe link. Fix the return contract; note #372's
caveat that the predicted loud symptoms were never reported, so re-check the premise
before writing code, and close or comment accordingly.

Deliverables:
- The shim, with the four integration-test registrations migrated onto it and all
  four modes still arming.
- Compile-time regression locks: a static_assert / size-probe (see #402's
  `games/mm/2s2h/mm_culling_test.cpp` for the pattern) that fails the build if MM
  code can again bind MM's layout to the shared instance.
- A ROM-free `redship`-label CTest exercising the shim's registration path.
  `IntegrationTest_SetMode()` exists (`src/common/integration_test_hooks.h:93`) —
  use it to force `IsActive()` true in the unit harness; there is no excuse to skip
  this test.
- Issue hygiene: close #395 and #372; comment on #383 scoping it to the UIWidgets
  remainder; file the operator-batch arming check (all four modes) on #392.

If the fix turns out riskier than the bug, stop and report — a precise diagnosis
with no patch is an acceptable outcome per project convention.

---

## G2 — #384 registrar renames + arm the #375 collision baseline

**Venue:** cloud (the verification is Linux-only `nm`). **Model:** sonnet.
**Independent of all open PRs.** **File ownership:** the MM-side files listed below
(note item 10 spans four files), `.github/symbol-collision-baseline.txt` (new),
nothing else. Do NOT touch `games/mm/CMakeLists.txt` (G1 owns it) and do NOT attempt
WHOLE_ARCHIVE on `2ship_enh` — that is Lane C's decision and it has other blockers.

Read #384 and #375 including comments; read `.claude/worker-prompts.md`.

Task 1 — rename the colliding globals. Ten C++-mangled global symbol groups are
defined identically in both `soh_enh` and `2ship_enh` (latent only because
`2ship_enh`'s objects are currently elided). Rename each MM-side function to `MM_*`
and update every reference. **Items 1–7 are ShipInit registrars** — rename + update
the `RegisterShipInitFunc` call site in the same file. **Items 8–10 are NOT
registrars** — they have ordinary call sites, listed below. Verified list
(OoT anchor / MM anchor):
1. `RegisterMoonJump` — `games/oot/soh/Enhancements/Cheats/MoonJump.cpp:22` / `games/mm/2s2h/Enhancements/Cheats/MoonJump.cpp:12` (call site `:22`)
2. `RegisterItemUnequip` — `games/oot/soh/Enhancements/ItemUnequip.cpp:18` / `games/mm/2s2h/Enhancements/Equipment/ItemUnequip.cpp:36`
3. `RegisterEasyFrameAdvance` — OoT `Cheats/EasyFrameAdvance.cpp:18` / MM `:15`
4. `RegisterUnrestrictedItems` — OoT `Cheats/UnrestrictedItems.cpp:22` / MM `:8`
5. `RegisterN64WeirdFrames` — OoT `Restorations/N64WeirdFrames/N64WeirdFrames.cpp:118` / MM `:44`
6. `RegisterPauseBufferInputs` — OoT `Restorations/PauseBufferInputs.cpp:30` / MM `:16`
7. `RegisterArrowCycle` + `ArrowCycleMain` — OoT `ArrowCycle.cpp:252/:224` / MM `Equipment/ArrowCycle.cpp:284/:242`; `ArrowCycleMain`'s call site is the lambda at MM `:309`
8. `PatchArrowTipTexture` — OoT `cosmetics/authenticGfxPatches.cpp:82` / MM `GfxPatcher/AuthenticGfxPatches.cpp:76`, call site `:233`
9. `SetActorMaximumHealth` — OoT `soh/ObjectExtension/ActorMaximumHealth.cpp:16` / MM `Enhancements/Graphics/EnemyHealthBars.cpp:41`, call site `:199`. **Asymmetric:** the OoT copy is in `soh_port` (always linked), so this one collides the moment MM's object is ever pulled, regardless of archive treatment.
10. `SplitsPushImageButtonStyle` — OoT `timesplits/TimeSplits.cpp:302` / MM `Trackers/TimeSplits/Timesplits.cpp:145`. **Fans out across four MM files:** extern declaration in `Trackers/TimeSplits/Timesplits.h:76`, cross-TU calls in `TimesplitsSettings.cpp:559/:585/:645` and `TimeSplitsActions.cpp:100`, plus `Timesplits.cpp:193` — all in scope.

The MM pattern to follow is already established in those files: data references are
`MM_`-prefixed (`MM_gPlayState`), only these names drifted. The functions sit OUTSIDE
the `extern "C"` blocks (C++ mangling) — keep it that way.

**#384's list is explicitly not proven exhaustive.** After renaming, the authority is
a Linux build: intersect defined symbols of `libsoh_enh.a`, `lib2ship_enh.a` (and
`soh_port` per item 9) with `nm`. The collision script already filters weak/COMDAT
and `_GLOBAL__sub_` symbols (`.github/scripts/check-symbol-collisions.sh:62-64`), so
any residue is a STRONG duplicate — investigate each, then either eliminate it or
record it as a tolerated baseline entry with a one-line justification (the script's
baseline format anticipates tolerated entries).

Task 2 — arm the #375 tripwire. `.github/symbol-collision-baseline.txt` was never
committed, so `check-symbol-collisions.sh` runs bootstrap-mode and exits 0 on every
PR. After Task 1's renames land, run
`bash .github/scripts/check-symbol-collisions.sh <build-dir> --write-baseline` on
your Linux build and commit the baseline. Confirm the FAIL path works by locally
reintroducing one duplicate and watching the script exit 1 (do not commit that).
Known limitation to note in the PR, not fix: the gate skips weak/COMDAT symbols by
design (documented in the script's own header, `check-symbol-collisions.sh:20-26`),
so it would not have caught #395's class.

Done criteria: renames and baseline merged (one PR or two), `nm` intersection clean
or fully accounted, the collision gate demonstrably able to fail, #384 and #375
closed, `check-registrar-elision.sh` output unchanged (you rename, you do not change
what links).

---

## F1 — Fault A: namespace the duplicated OTRExporter globals (#396)

**Venue:** cloud for the change and the linker-level proof; ONE command on the
operator's box for crash clearance. **Model:** opus. **Independent** of every other
lane. **File ownership:** the OTRExporter submodule (fork
`spencerduncan/OTRExporter`, branch `claude/<description>` — the pinned
`claude/disable-ipo-single-exe` branch is the precedent) plus the submodule pointer
in the superproject.

**Read #396's COMMENTS, not just the body.** The body still frames #395 as the
leading candidate; the comments retract that, eliminate #371, and carry the
debugger-verified root cause and the chosen fix. Summary: `OTRExporter/Main.cpp` is
compiled into BOTH `OTRExporter_OoT` and `OTRExporter_MM` (same sources, only
`GAME_OOT`/`GAME_MM` differ — `OTRExporter/OTRExporter/CMakeLists.txt:88-95`), both
deliberately linked into `redship` for in-app ROM extraction. Under Windows
`/FORCE:MULTIPLE` (root `CMakeLists.txt:274`) the linker keeps ONE data symbol per
global but BOTH dynamic-init and atexit-dtor functions, so globals are constructed
twice and destroyed twice — the second destruction walks freed heap: `0xC0000374` on
every normal exit, including `redship --version`.

The seven with init/dtor pairs: `files` (`Main.cpp:46`, `extern` in `Main.h:7`),
`archive`, `archiveFileName`, `customArchiveFileName`, `customAssetsPath`,
`portVersionString` (`Main.cpp:34-43`), `resourceVersions` (`VersionInfo.cpp:8`).
**But do not anchor on the enumerated seven:** `Main.cpp` also defines
external-linkage mutable globals WITHOUT init/dtor pairs (`fileWriter` `:44`,
`fileStart`/`resStart` `:45`, `fileMutex` `:47`) whose data symbols also fold to one
shared storage across both variants — cross-variant shared state the crash sweep
didn't flag. **Namespace per-TU wholesale: sweep ALL external-linkage globals in the
affected TUs**, not just the seven.

The fix (decided in the issue thread — do not relitigate): per-variant namespacing of
the exporter globals in the submodule (option 1). Sharpenings:
- `archiveFileName`'s initializer VALUE differs per variant (`mm.o2r` vs `oot.o2r`,
  `Main.cpp:34-38`, no `#else`). Double-construction means the surviving value today
  depends on init order — a functional wrong-archive hazard on top of the crash.
  Your fix must keep each variant's own value.
- The submodule also supports `OTREXPORTER_SINGLE_GAME` single-variant builds and an
  `OTRExporter` ALIAS target (`OTRExporter/CMakeLists.txt:79-83`, `:103-105`) —
  whatever namespacing scheme you choose (e.g. macro-selected namespace keyed on
  `GAME_MM`/`GAME_OOT`) must keep those configurations compiling too.
- The ~20 duplicated function pairs were called harmless because the sources are
  identical — but `GAME_MM`/`GAME_OOT` ifdefs mean "identical" is not guaranteed;
  sweep them and namespace any that differ. Report the sweep result either way.
- Scope reassurance (verified): the globals have ZERO use sites outside the
  submodule — in-app extraction goes through the bundled ZAPD subprocess driver.
  Intra-submodule consumers beyond `DisplayListExporter.cpp:424/:541/:905` are
  `Exporter.cpp:15` (`resourceVersions`) and `ExporterArchiveO2R.cpp` (`archive`).
  Superproject impact = pointer bump only.
- `dupe_syms.txt` from the original sweep is mentioned in the thread but not attached
  anywhere — regenerate the before-state from your own build's map/nm.

Verification, split by venue:
- Cloud: superproject links on Linux and Windows CI; `nm`/map sweep shows no global
  defined by both exporter archives; full `redship` CTest tier green.
- Operator batch (one command): `build-cmake\redship.exe --version` exits 0 — it
  currently exits `0xC0000374`. Debug tooling if needed: `.claude/tools/dbg374.py`
  against `build-cmake/redship.map`. Do not rely on CI for this: the Windows CI job
  never executes `redship.exe`, test tiers mask exit codes via `_Exit`, and the
  headless handler may not intercept CRT heap-corruption fast-fail.

Also: #396's final comment suggests a permanent CI gate (nm/map intersection of the
`_OoT`/`_MM` variant archives — STRONG symbols, so unlike the #375 tripwire it WOULD
catch this class). Land it in this PR or file a follow-up issue explicitly — do not
drop it silently. Update `docs/known-issues.md` ("Every normal exit heap-corrupts on
Windows (Fault A)", `:88` ff.) after operator confirmation, then close #396.
`docs/phase3-roadmap.md:148/:239` still blame #371 for Fault A; don't be misled.

---

## F2 — MM fault-handler + retire the non-void-stub class (#403)

**Venue:** cloud — fully ROM-free. **Model:** sonnet. **Branch after #401 merges**
(the OoT twin of this fix; its PR body is your reference table). **File ownership:**
`games/mm/src/boot/fault.c`, `games/mm/src/code/stubs.c`. The per-source
warning-escalation hunk lives in `games/mm/CMakeLists.txt`, which G1 owns — land
everything else first and add the CMake hunk after G1's merge (coordinate on #392).

Read #403 and merged PR #401; read `.claude/worker-prompts.md`.

Verified defect state — three independent layers of latency, which the issue
undersells:
- `MM_Fault_FindFaultedThread` (`games/mm/src/boot/fault.c:555`) has its entire body,
  both returns included, inside `#if 0` (`:556-568`) — it falls off the end.
- Its ONLY reference, the `do/while (faultedThread == NULL)` loop at `:1054-1058`
  (wild-pointer store at `:1061`), is itself inside `MM_Fault_ThreadEntry`, whose
  entire body is ALSO `#if 0` (`:1019-1118`) — the consumer is preprocessor-disabled
  dead code today.
- `MM_Fault_Init`'s body is `#if 0` too (`:1131-1149`), and the `MM_osCreateThread`
  it would call (`:1146`) is an empty stub.

So: nothing can spin today, no runtime CTest can exercise the call-site fix, and
`-Werror=return-type` does not check `#if 0` regions. Frame the PR as **latent on
MM's own facts** (do NOT claim #385/#401 framed OoT's twin as latent — they didn't;
#385's OoT consumer was live). The value of this lane is retiring the CLASS, and the
enforceable lock is the warning escalation.

The real scope (the issue says three stubs; the tree says ~24 functions):
1. `fault.c`: give `MM_Fault_FindFaultedThread` a contract-correct return (revive the
   `#if 0` body if #401's `__osActiveQueue` storage in
   `rsbs/src/libultra/os/threadqueue.c` supports it; otherwise explicit NULL +
   comment). Fix the `do/while` loop in place (bounded retry + abort) even though it
   is disabled — whoever revives `ThreadEntry` inherits it fixed. Also
   `MM_Fault_ConvertAddress` (`:257`) and `MM_Fault_WaitForInputImpl` (`:297`) — both
   non-void with fully-`#if 0` bodies; the escalation flags them too.
2. `stubs.c`: TWENTY-TWO empty non-void functions need contract-correct returns
   (`MM_osProbeRumblePak:108`, `MM___osDisableInt:123`, `MM_osDriveRomInit:137`,
   `__osGetCause:234`, `__osGetCompare:238`, `__osGetConfig:242`, `__osGetSR:246`,
   `__osGetWatchLo:251`, `__osPopThread:260`, `__osProbeTLB:264`,
   `MM_osContStartQuery:304`, `MM_osGetThreadPri:308`, `MM_osContSetCh:330`,
   `MM_osViGetCurrentFramebuffer:338`, `osFlashInit:340`, `osFlashSectorErase:347`,
   `osFlashWriteBuffer:349`, `osFlashWriteArray:351`, `osFlashReadArray:353`,
   `osFlashCheckEraseEnd:361`, `MM_osViGetNextFramebuffer:388`,
   `MM_osSpTaskYielded:390`). **Mirror PR #401's return-value table** — it did the
   same sweep on the OoT side (found 24 after the issue named 4) and records the
   researched values: `PFS_ERR_NOPACK` for pak probes, 0 for `osCont*`/`osFlash*`
   success, NULL for `osDriveRomInit`, 0 (not `OS_TASK_YIELDED`) for
   `osSpTaskYielded`, etc. Where an OoT twin exists, match it; where none does,
   derive from the libultra contract and say so in the PR.
3. Escalation: per-source `-Werror=return-type` / `/we4716 /we4715` on `fault.c` and
   `stubs.c` via source-level `COMPILE_OPTIONS`, exactly as #401 did for
   `soh/stubs.c` — this out-ranks target-wide suppression (MM passes explicit
   `-Wno-return-type` on Darwin/Switch/CafeOS and `-Wno-error` on Linux;
   `SUPPRESS_WARNINGS`' `WARNING_OVERRIDE` is never applied to MM). This hunk waits
   for G1 (file ownership).

Do NOT rename or deduplicate the unprefixed shared-namespace stubs
(`__osGetCause`, `osFlashInit`, ...) in passing — adding returns is safe; symbol
dedup belongs to G2/#375.

Lock: the `-Werror` flip (verify locally: reintroduce an empty non-void body, watch
the build break, revert). A runtime CTest is optional and only meaningful if you
revive the `#if 0` bodies. PR body records the latent framing and the three-layer
dependency chain for whoever wires the real MM fault thread later.

Done criteria: #403 closed, CTest tier green, escalation demonstrably failing on
regression, no unrelated symbol renames.

---

## H — Harness gates + correctness tail (#376 items 1–3, #377, #365, #380)

**Venue:** cloud. **Model:** opus. **Independent**; one PR per item; each item starts
by RE-VERIFYING its premise against the current tree — several #381-era claims have
already been fixed in passing (e.g. `--no-tests=error` is now present in
`generate-builds.yml`'s ctest steps). If an item's premise is stale, comment and
close it instead of forcing a fix — that is a good outcome here.

- **#376 items 1–3** (CI harness gaps; items 4–6 are deferred per #392): check which
  of the orphaned-CTest-label / missing `--no-tests=error` / unfailable
  `check-archives` claims still hold post-#390/#394/#404, fix what remains, close or
  re-scope the issue. The phase's own theme is gates that cannot fail — B and C add
  new `rando`-label locks, so this harness must actually enforce.
- **#377**: `RSBS_AUDIO_PROBE` bypasses the blocking reset handshake it was added to
  observe, so the probe can cause the wedge it diagnoses. Fix the probe to observe
  without bypassing.
- **#365**: OoT's audio thread keeps running while MM is active;
  `CreateNextAudioBuffer` lacks an initialized guard. Note #362 (merged) already
  drains the OTR audio thread before archive hot-swap — scope what remains after it.
- **#380**: integration-hook entrance bound is an unchecked literal justified by a
  comment describing a check that does not exist. Add the real check (or the real
  comment).

Each fix that touches testable behavior gets a `redship`-label lock or a
demonstrated-failing CI gate, per the phase rule.

---

## A0 — Shared item-id ADR + ComboContext carve (serial section — solo owner)

**Venue:** cloud — fully ROM-free. **Model:** fable — this sets the phase's type
system and serialization shape; errors here compile clean and surface as save
corruption months later. **File ownership (exclusive while this runs):**
`src/common/context.h`, `src/common/context.cpp`, `src/common/save.cpp`,
`src/common/tests/test_save_roundtrip.c`, `docs/adr/0002-*.md`, plus a
`docs/known-issues.md` touch-up. Nothing else starts Lane A work until this merges.
Keep it to about a day.

Read #392 (epic), `docs/adr/0001-rsbs-vs-src-common.md` (format precedent),
`.claude/worker-prompts.md`.

The good news (stale-doc alert): `docs/phase3-execution-prompt.md` §3's warning that
"widening sharedItems means every existing .redsave silently stops loading" describes
the PRE-#399 loader. Already fixed and locked: ComboContext serializes as a fixed
1024-byte Tier-1 record (`RSBS_COMBO_CONTEXT_RECORD_SIZE`, `src/common/context.h:94-112`)
with `uint8_t reserved[640]` headroom (`:137-141`), a `<=` static_assert (`:147-155`),
a version window [`RSBS_SAVE_VERSION_MIN`=1, `RSBS_SAVE_VERSION`=2]
(`src/common/save.h:48-54`, `save.cpp:178-181`), and size-field-driven zero-extension
of shorter records (`save.cpp:193-210`, `:242-263`), locked by three CTests
(`test_save_roundtrip.c:389-558`). Your constraint is the **growth contract**
(`context.h:103-111`): carve new fields from `reserved` or append — never insert,
never change the offset or meaning of shipped bytes — and zero must be a valid
"unset" for every new field, because a zero-extended legacy record is
indistinguishable from fresh init.

Deliverable 1 — the ADR (`docs/adr/0002-<kebab>.md`), deciding:
- **The shared item representation.** Hard requirement from #392: an explicit tagged
  struct — origin game + id — such that a raw integer read fails at compile time.
  Do NOT bit-pack a game tag into the existing `uint16_t`; a packed representation
  makes a raw read *almost* work, which is exactly how the #356 entrance-id leak
  behaved. **`context.h` is included by both C and C++ TUs** (that is why it has the
  dual `static_assert`/`_Static_assert` at `:147-155`) — the tagged type and the
  raw-assignment-fails proof must hold in the C view too, so no `enum class` /
  explicit-constructor devices: a plain nested struct (struct-from-int assignment is
  ill-formed in both languages) plus C-compatible static asserts is the natural
  mechanism. OoT ids are `RandomizerGet` (`RG_*`,
  `games/oot/soh/Enhancements/randomizer/randomizerTypes.h:4419-4731`); MM ids are
  `RandoItemId` (`RI_*`, 237 enumerators — `games/mm/2s2h/Rando/Types.h:2313-2549`;
  note the path, there is no `StaticData/Types.h`; and the docs' "382-row item
  table" is wrong — the MM table is 234 rows). Both fit u16 with room; a
  `{u8 originGame; u8 flags; u16 id;}`-shaped 4-byte entry × 64 entries = 256 bytes,
  comfortably inside `reserved[640]`. Size the array generously ONCE.
- **The fate of the existing dead fields.** `sharedItems` (`uint16_t[32]`,
  `context.h:126`), `sharedFlags` (`uint32_t[64]`, `:122`), `saveSlot` (`:130`) have
  zero non-test references (re-verify with grep). Their offsets are shipped — retire
  in place (document as dead, keep bytes) or adopt; never delete or repurpose
  incompatibly. Recommend: retire `sharedItems` in place, carve the tagged array
  from `reserved`; keep `sharedFlags` (its shape is fine) and define its semantics
  later; retire `saveSlot` in place with a comment. **Renaming or retyping these
  members breaks two test TUs outside your ownership**
  (`tests/test_roundtrip_integrity.c:31-34/:136-142`,
  `tests/test_shared_state_roundtrip.c:46-94`) — retire-in-place avoids touching
  them; if the ADR decides otherwise, coordinate the edits explicitly.
- **Whether `sourceIsRando`/`sharedRandoSeed` (`:133`) are Lane B's carrier** — they
  exist, are serialized, and are dead. Reviving them is the default answer; say so
  explicitly so B can proceed.
- Note the two distinct version knobs: `COMBO_CONTEXT_VERSION` (inner content
  semantics, `context.cpp:194`, currently 1, not range-checked on load) vs
  `RSBS_SAVE_VERSION` (on-disk format, only bumps with
  `RSBS_COMBO_CONTEXT_RECORD_SIZE`).

Deliverable 2 — the struct carve implementing the ADR, with `ComboContext_Init`
zeroing, a compile-time proof that a raw integer cannot be assigned into the tagged
type (both languages), and extended `test_save_roundtrip.c` coverage. **Subtlety the
existing tests hide:** `Test_SaveComboLegacyRecord` derives its "legacy" length as
`offsetof(ComboContext, reserved)` (`test_save_roundtrip.c:458`). Carving fields out
of `reserved` moves that offset forward, silently redefining "legacy" to include
your new fields — the test stays green while no longer exercising the true shipped
pre-carve prefix. Pin the real pre-carve length (a constant, or a second
crafted-record test at the old offset) so the legacy-load guarantee stays honest.
New test code in that file is C++ compiled inside `test_runner.cpp` (the `.c` files
are `#include`d — that is why they already use `std::vector`); dispatch rows go in
`gTests[]`.

Deliverable 3 — housekeeping you are uniquely positioned for as `context.h`'s owner:
delete the dangling dead-API declarations (`Context_ProcessSwitch` at `context.h:188`
etc. — `switch.cpp:25-32` documents they have zero callers and assigns removal to
this file's owner); fix the stale comment at `context.cpp:259-263` claiming
switch.cpp is non-single-exe-only; update `docs/known-issues.md:28`'s
dead-plumbing entry to match the ADR's outcome.

Done criteria: ADR merged with Status: Accepted; CTest tier green with the legacy
prefix pinned as above; #392's Lane A checkbox annotated via comment (there is no
ADR checkbox in the epic body — comment instead); a short comment on #392 telling
A1/B workers the decisions are final.

---

## A1 — Shared-state producers and consumers (+ #373) (+ round-trip lock)

**Venue:** cloud — fully ROM-free by design. **Model:** opus. **After A0 merges.**
Can be ONE worker, or two on the disjoint files — but the MM-side worker branches
only **after G1 merges** (G1 owns `games/mm/2s2h/GameExports_SingleExe.cpp` first;
see preamble ordering). OoT side: `games/oot/soh/GameExports_SingleExe.cpp` + OoT
hook sites. **Do not touch** `context.h`, `save.cpp`, or the entrance tables.

Read A0's merged ADR first — its representation decisions are final. Read #392 and
`.claude/worker-prompts.md`. Note: the epic's Lane A text says "MM reads on resume";
that wording predates #400 — the verified consumer sites are the consumption points
below, and this brief overrides the epic on purpose.

The architecture fact that shapes everything: **the freeze/restore machinery carries
no ComboContext.** `Context_FreezeState`/`Context_RestoreState`
(`src/common/context.h:39-49`) move only a SaveContext blob. `gComboCtx` crosses the
switch because it is a process-global shared by both games in the single exe
(`src/common/context.cpp:196`) — your producers and consumers write and read it
directly at game-side hook points. Do NOT extend the freeze API, and do NOT imitate
`OoT_FreezeState` (`OTRGlobals.cpp:2850`) or `MM_FreezeState` (`BenPort.cpp`) — both
are dead code (zero callers / excluded TU).

Producer sites, verified, with their traps:
- `OoT_Game_Suspend` (`games/oot/soh/GameExports_SingleExe.cpp:778`) and
  `MM_Game_Suspend` (`games/mm/2s2h/GameExports_SingleExe.cpp:1074`). **Producers
  must live here (or cover both switch paths explicitly):** the F10 hot-swap path
  (#400, `Combo_FreezeActiveGameForHotSwap` from `rsbs/src/main.cpp`) bypasses
  `Combo_CheckEntranceSwitch` entirely — a producer hung only on the entrance path
  drops shared-item writes on every hotkey switch.
- `Combo_CheckEntranceSwitch` (`games/oot/soh/GameExports_SingleExe.cpp:1016-1044`,
  shared TU both games run) — has a `wasAlreadyPending` guard (`:1024/:1034`) that
  suppresses re-firing on subsequent transition frames; producer code here must
  tolerate repeat calls without re-firing.

Consumer sites, verified, with their traps:
- The startup-entrance consumption points where `Combo_ConsumeFrozenState` runs —
  `games/oot/src/code/z_play.c:442` and `games/mm/src/code/z_play.c:2288` — NOT the
  `Game_Resume` restores (`GameExports_SingleExe.cpp:833-865` OoT / `:1169-1187`
  MM), which are defense-in-depth the boot chain wipes (their own comments say so).
- **Presence-gating:** the consumption sites run only when a startup entrance is set
  for the arriving game (`Combo_HasStartupEntranceForGame`), and
  `Combo_ConsumeFrozenState` retires the blob — consume-once per arrival. A cold
  boot or a `.redsave` load WITHOUT a switch never reaches them. Decide and document
  how shared state applies on plain load (the `.redsave` load path
  `save.cpp:287-289` refreshes `gComboCtx` + shadows; your consumers may need a hook
  there too, or an explicit "applies on next switch only" semantic).
- **#373 is yours (absorbed):** the MM consumption point does not neutralize live
  timers, unlike its OoT twin — a real regression sitting exactly where your MM
  consumer goes. Fix it as part of the MM-side work and close #373.

Persistence traps, verified:
- OoT's `OnSaveFile` hook (where `RsbsSave_Save` runs,
  `games/oot/soh/SaveManager.cpp:151-189`) fires only for `SECTION_ID_BASE`
  (`:156-158`), and `OnExitGame` (`:179-189`) also saves — relevant to "what if the
  player never saves after switching back" (`gComboCtx` is memory-only until one of
  these fires). Both run inside `SaveFileThreaded`'s `lock_guard` on a PLAIN
  `std::mutex` (`SaveManager.cpp:1292-1301`, `SaveManager.h:200`) — any producer
  code hung there must not call back into `SaveManager::*` or it self-deadlocks.
- MM's own SaveManager TU is excluded from single-exe builds (its `:456` shadow
  mirror never runs) — in the single exe the MM shadow refreshes only at freeze time
  and `.redsave` load. Do not imitate it; read live MM state at pickup time.

Deliverable: items written on one side are visible on the other after a switch, both
directions, per the ADR's types. The lock (non-negotiable): a `redship`-label CTest
asserting a written shared item survives suspend → switch → resume → switch →
resume THROUGH THE REAL HOOK FUNCTIONS — extend
`src/common/tests/test_shared_state_roundtrip.c` (it reuses CTest row
`SharedRoundtrip` / dispatch `shared-roundtrip`; it currently covers only
`RequestSwitch`/`ClearSwitch` plumbing, `:5-14`) and add a NEW row for the hook-path
test (one `redship_add_test` line + `gTests[]` row).
`src/common/tests/test_hotswap_freeze.c` is the precedent for driving
`Combo_ConsumeFrozenState` headlessly — cover the F10 path too.

Done criteria: round-trip CTest green in CI, #373 closed, "zero non-test references"
no longer true for the ADR's shared fields, #392 Lane A ticked, operator-batch note
filed on #392 (one item crossing during real gameplay — batched, not a dedicated
sitting).

---

## B — Unified seed → paired world

**Venue:** cloud — the determinism test runs under xvfb without ROMs, exactly like
the existing `rando` CTest tier. **Model:** opus. **After A0 merges** (it rules on
the seed-field decision); parallel with A1. **File ownership:** OoT randomizer
generation surface (`games/oot/soh/Enhancements/randomizer/...`), `src/common/` menu
code if you wire the surface there, new test files. Coordinate with A1 if you touch
either `GameExports_SingleExe.cpp` (they own both, after G1).

Read #392, A0's ADR, `.claude/worker-prompts.md`.

Ground truth (verified 2026-07-20 — several docs claims are stale):
- `gComboCtx.sourceIsRando` / `sharedRandoSeed` exist (`src/common/context.h:133`),
  are serialized in every `.redsave`, and are DEAD: their only producers/consumers
  are the dead legacy path (`OoT_FreezeState`, `OTRGlobals.cpp:2861-2867` — compiled,
  zero callers; `MM_InitFirstEntrySaveContext`/`MM_FreezeState`,
  `BenPort.cpp:2153/:2220` — excluded TU). The roadmap's "seed propagation is wired
  both directions" is false. Default design (confirm against A0's ADR): revive these
  fields as the carrier; write them at OoT generation time, not at freeze time.
- OoT generation chain: `GenerateRandomizer(seed)`
  (`games/oot/soh/Enhancements/randomizer/randomizer.cpp:3510`) → thread →
  `Rando::Settings::SetAllToContext` → `RandoMain::GenerateRando`
  (`3drando/rando_main.cpp:13`) → seed derivation in `3drando/menu.cpp:76-99`
  (empty seed → random 10-digit string; `seedHash = SohUtils::Hash(seedString)`) →
  `Playthrough_Init` (`3drando/playthrough.cpp`).
- **The determinism subtlety:** `Playthrough_Init` seeds the RNG TWICE —
  `Random_Init(seed)` at `:24`, then `finalHash = Hash(settings-string + seed)` and
  `Random_Init(finalHash)` at `:67-68` (and `DontGenerateSpoiler` mixes the build
  version in at `:63-65`). Same numeric seed reproduces the fill only under
  identical settings. "One seed → paired world" therefore means: one seed + one
  pinned settings profile. **The pinning vehicle already exists:** `RSBS_DIAG_CVARS`
  (`3drando/menu.cpp:49-62`), used by the ShuffleSongs rando rows — use it.
- Portable primitives: both games carry byte-identical FNV-1a (`SohUtils::Hash` ≡ MM
  `Ship_Hash`) and byte-identical PCG-XSH-RR RNG cores (`games/oot/soh/ShipUtils.cpp:103-150`
  ≡ MM `2s2h/ShipUtils.cpp:305-334`) — same seed string hashes identically both
  sides. But MM's copies are in an excluded TU, and MM generation
  (`Rando::MiscBehavior::OnFileCreate`) is unreachable until Lane C. **Scope B to
  the OoT side + the shared contract**: seed and derived-settings digest written into
  `gComboCtx`, MM consumption explicitly deferred to Lane C — record the handoff
  contract in your PR body and on #392.
- Surface decision — escalate, don't decide: `ComboMenuBar`
  (`src/common/ComboMenuBar.cpp`) reserves `gOoT.Rando`/`gMM.Rando` CVar prefixes but
  is compiled-never-instantiated dead UI; the live menu is SoH's
  (`SohGui.cpp:109-110`, "Generate Randomizer" at `SohMenuRandomizer.cpp:600-610`).
  File the ComboMenuBar-vs-SohMenu question on #392 with your recommendation and
  proceed with the minimal path (CVar + existing SohMenu hook) so the lane doesn't
  block on UI.

Deliverables:
1. One seed (+ pinned settings profile) → `gComboCtx.sharedRandoSeed` +
   `sourceIsRando` set in the LIVE path at generation time, persisting through
   `.redsave` (the plumbing already round-trips — `test_shared_state_roundtrip.c`
   and `SaveComboLegacyRecord` lock it).
2. The lock: a same-seed-twice determinism CTest via `Rando_HeadlessSeedTest(seedStr)`
   (`extern "C"`, `3drando/menu.cpp:33`) — the first output-equality test in the
   `rando` label (the existing three rows assert only generation-succeeds).
   Register in the `rando` pattern (xvfb, TIMEOUT 180, `SDL_AUDIODRIVER=dummy`,
   `RSBS_DISABLE_OTR_INIT=1`). Three verified pitfalls to design around:
   - **Same seed → same spoiler path** (`Randomizer/<hash>.json`,
     `spoiler_log.cpp:390-395`), so the second run OVERWRITES the first and a naive
     diff compares the file against itself. Snapshot/copy the first JSON (or capture
     placements in-memory) before the second run; `gGeneral.SpoilerLog` also always
     points at the latest run.
   - The headless harness calls the synchronous 3-arg `GenerateRandomizer`
     (`menu.cpp:64-69`) directly — no thread, no CVar-driven
     excludedLocations/enabledTricks, no completion hook. Your determinism coverage
     is for empty exclusion/trick sets; say so in the test comment.
   - `Rando_HeadlessSeedTest` has never been called twice in one process
     (`Test_RandoGen` calls it once, `test_runner.cpp:241-255`); re-entering
     `Settings::CreateOptions`/`SetAllToContext` is unverified. Prefer two process
     invocations (two CTest rows or a wrapper that diffs), or validate re-entry
     explicitly.
3. A seed→settings derivation note (PR body or short doc section): what is pinned,
   what is derived, what MM will consume later.

Done criteria: determinism CTest green in CI twice in a row, seed visible in
`gComboCtx` in the live path (grep shows non-test producers), handoff contract for
Lane C recorded on #392.

---

## C — Lane C: make `2ship_rando` real, then OoT→MM foreign items

**Venue:** hybrid — all development and link/CTest locks on cloud; runtime item-give
verification goes to the operator batch. **Model:** fable — this is the phase's big
bet and every prior "compiles fine, does nothing / corrupts memory" landmine lives
here. **C0 starts after G1 and G2 merge. C1 additionally requires A0, A1, and B
merged** — the epic's "C without A is unverifiable" gate is binding; do not start C1
against unmerged types. **File ownership:** `games/mm/2s2h/Rando/**`,
`games/mm/CMakeLists.txt` (inherit from G1), MM-side `GameExports_SingleExe.cpp`
additions (inherit from A1), `CMake/`+root link topology if needed.

The MVP contract, verbatim from #392 — this is the scope fence:
> One seed produces a paired OoT+MM world in which at least one item class crosses
> games, the crossing survives a full round trip, and a spoiler log describes it.
One direction (OoT progression items in MM checks), one item class. Parity is the
named scope-creep risk; the spoiler log carries what logic would otherwise carry
(Lane D is deferred).

### C0 — reachability (the former Wave-0 spike, now executable)

Verified state: `2ship_rando` is complete — 234-row item table
(`Rando/StaticData/Items.cpp:27`; NOT the docs' 382), `Rando::GiveItem`
(`Rando/GiveItem.cpp:11`), fill (`Logic/GeneratePools.cpp:15` +
`Apply*LogicToSaveContext`), spoiler (`Spoiler/Generate.cpp:8`, JSON tagged
`2S2H_RANDO_SPOILER`) — and 100% linker-discarded: zero inbound references. Good
news the docs don't have: **`2ship_rando` does not collide with OoT** — the #384
collisions were all in `2ship_enh`, and G2 renamed them. It does define some
plain-global C++ functions (`Menu.cpp`: `ClearIncompatibleSetting:96`,
`SortExcludedChecks:128`, etc.; `Rando.cpp:13` `OnSaveLoadHandler`) that are merely
collision-free TODAY — after force-linking, re-run the `nm` intersection (G2's
armed gate now does this on every PR) rather than trusting the namespacing
rationale.

Reachability needs three things together (any one alone produces code that links and
silently does nothing):
1. **Linkage**: WHOLE_ARCHIVE on `2ship_rando` only, or an explicit
   undefined-symbol anchor via `MM_Rando_Init()`. Expect transitive drag: Rando
   includes `Enhancements/FrameInterpolation` (5 TUs) and
   `StoryCutscenes/SkipGiantsChamber.h`, so `2ship_enh` members and THEIR deps come
   too. Dependency-gap hints from include-census (re-enumerate with a fresh Linux
   link — the "~27 unresolved" comment is a pre-G1/G2 measurement): MM ShipUtils
   (26 including TUs — extend #402's `mm_ship_utils_prefix.h` pattern),
   CustomMessage (23), CustomItem (11), ObjectExtension, BenGui
   UIWidgets/Notification. **The BenGui gap includes DATA symbols**: `Rando/Menu.cpp:55-56`
   references `extern std::shared_ptr<...> mRandoCheckTrackerSettingsWindow` and
   `mBenMenu`, defined only in excluded BenGui TUs — stubbing means supplying object
   definitions, not just no-op functions. Stub honestly (visible no-op + logged) or
   port minimally; record each decision.
2. **Initialization**: `Rando::Init()` (`Rando.cpp:24`, only caller is excluded
   `BenPort.cpp:868`) AND `S2H::ShipInit::InitAll()`, called NOWHERE in the single
   exe — without it 21 Rando TUs (the whole `Logic/Regions/` graph, `Logic.cpp:139`)
   never populate. An `MM_Rando_Init()` from `MM_Game_Init` is the natural shape.
   Boot-order checks, TWO of them: `Ship::Context::GetInstance()->GetFileDropMgr()`
   at `Rando.cpp:29` must be live when `MM_Rando_Init` runs — and **a static-init
   landmine fires even earlier**: `Rando/Spoiler/RefreshOptions.cpp:9-10` defines a
   namespace-scope global whose dynamic initializer calls
   `Ship::Context::GetPathRelativeToAppDirectory(...)` — under WHOLE_ARCHIVE that
   runs BEFORE `main()`, before OoT creates the shared context. Make it lazy first
   or the exe dies at boot.
3. **Hooks — sequencing trap**: `Rando::Init()` (`Rando.cpp:31`) and
   `MiscBehavior::Init()` (`MiscBehavior.cpp:12`) make RAW `RegisterGameHook` calls
   on the shared instance, and `CheckQueue.cpp:40/:127-128` directly accesses
   `Instance->events`/`currentEvent` — MM-only DATA members. Migrate ALL of these
   onto G1's shim (which owns MM-side GIEvent storage per its brief) BEFORE
   `MM_Rando_Init` first executes — otherwise C0's first boot fires #395's OOB
   write unconditionally, no `--integration-test` gate this time.

C0 locks (all cloud): `strings redship | grep 2S2H_RANDO_SPOILER` present (absent
today — the historical proof-probe); `check-registrar-elision.sh` shows MM
registrars retained; the armed collision gate green; an MM rando-gen smoke CTest in
the `rando`-label style (the OoT `RandoGen` rows are the template) proving
`GeneratePools` + a logic apply + spoiler write run headlessly. Land C0 as its own
PR(s) before starting C1 — independently valuable, independently revertable.

### C1 — the foreign-item pipeline (one class, one direction)

- Placement: at generation time (B's handoff: seed + settings digest in `gComboCtx`),
  place N OoT progression items of the chosen class into MM checks by extending the
  MM fill with a foreign-item pool. Use A0's tagged item type at every boundary —
  raw `RG_*` ints must not enter MM tables (MM `RandoStaticItem` carries MM-only
  `ItemId`/`GetItemId`/`GID` fields that are meaningless for foreign items; OoT's
  own assetless-item precedent — `MOD_RANDOMIZER` + custom draw, e.g.
  `Randomizer_DrawTriforcePiece`, `draw.cpp:403` — is the presentation template).
- Give path: intercept in the `CheckQueue` giveItem/drawItem lambdas or a new
  foreign-item branch in `Rando::GiveItem`'s switch (`GiveItem.cpp:11`, default →
  `MM_Item_Give` at `:376`) — on receiving a foreign item, record it into the
  A-lane shared structure (tagged, origin OoT) in `gComboCtx` and present a generic
  "foreign item" textbox/model in MM. The OoT side redeems it from the shared
  structure at consume time (A1's consumers) via `Randomizer_Item_Give`
  (`randomizer.cpp:3644`).
- Spoiler: extend the MM spoiler JSON (and/or OoT's) so every cross-game placement
  is described — check name, item, origin game. The two games' spoiler files live in
  different app dirs with no shared infra; a minimal combined artifact (or a
  cross-reference field in each) is fine — note the decision.
- Locks (cloud): placement determinism folded into B's same-seed test; a ROM-free
  unit test that a foreign item entering the give path lands in the shared structure
  correctly tagged; round-trip survival via A1's test extended with a foreign-item
  entry.
- Operator batch (the single sitting): pick up the item in MM, switch, confirm
  redemption in OoT, confirm spoiler matches reality.

Done criteria: MVP contract demonstrably satisfied (CI locks + operator
confirmation), #392 Lane C ticked, and an honest list of what was stubbed in C0
filed as follow-up issues.

If at any point the dependency surface explodes (e.g. CustomMessage porting turns
into a subsystem port), stop and report with the measured gap — re-scoping C is a
maintainer decision, not a worker decision.
