# Worker loop goals — 2026-09-21 wave (wave 4; updated 2026-09-21)

**Where the phases stand.** Wave 3 closed epic #644 (ADR 0010 increment 2,
merged PR #680) and its remaining prerequisites for increment 3: #656, #657,
#659, #667 (PR #689), #658 (PR #690), and #661 (PR #691, which also closed
#644). O4 (composition vs. unification) is **ruled: composition** (operator,
2026-09-17; ADR 0010 amendment). Increment 3 itself is epic **#645** and this
wave lands its first real slice: the combo-logic coordinator and stub-engine
locks (lane K1), the two real solver exports it is written against (lanes
K2a/K2b), and the first cost/convergence measurements over those exports
(lane K3) — none of it wired into a production path yet. Alongside increment
3: a real golden determinism digest so "the world did not change" is
enforceable rather than asserted (#688, lane Q), the remaining plain
widenings in MM's per-trick vocabulary (#578 part 3, #697, lane T3), MM's
`mods/` folder never mounting in single-exe (#670, lane M), the on-screen
generation-progress bar (#582, lane P), license-attribution and font-license
follow-up (lane N), and this tracker/docs sweep (lane H). Phase 3.1 (#492)
and Phase 3.2's increment-2 epic (#644) are both closed. Community reports
#634/#635/#636 remain human-filed and hands-off, tracked agent-side as
#640/#638/#639 (resolved in earlier waves; #634/#640's Anchor-page regression
is the only one still open, unrelated to this wave).

## Lanes — one card per lane in `.claude/lanes/`

Read `.claude/lanes/SHARED.md` first; it carries the append-only shared-file
protocol and the three-edit CTest registration rule that hard-fails the build if
you do fewer. Then read your own card and the issue or ADR it names.

| Lane | Branch | Serves | Owns, roughly |
|---|---|---|---|
| K1 | `claude/inc3-coordinator-core` | #645 increment 3: the combo-logic coordinator and engine surface, locked over stub engines | new `src/common/combo_logic.{h,c}`, new `src/common/tests/test_combo_logic.c`; `src/common` only, not wired into any production path |
| K2a | `claude/inc3-oot-logic-export` | #645: implement K1's engine surface over OoT's real solver | new TU beside `ForeignItemsSingleExe.cpp` (guard `RSBS_SINGLE_EXECUTABLE`), its test; starts only after K1 merges |
| K2b | `claude/inc3-mm-logic-export` | #645: implement K1's engine surface over MM's real solver | new TU under `games/mm/2s2h/Rando/` beside the foreign-items TU, its test; starts only after K1 merges |
| K3 | `claude/inc3-linked-round-measurements` | #645: measure the linked round's cost and the assumed fill's convergence over the real K2a/K2b exports | a new measurement test TU, minimal read-only accessors in the K2a/K2b export TUs if needed; starts only after K1, K2a and K2b all merge |
| Q | `claude/688-golden-determinism-digest` | #688 (the determinism rows prove reproducibility, not stability — no golden digest exists to re-pin) | `CMake/Check*Determinism.cmake` (or new siblings), new golden files, the determinism rows in `CMake/SingleExecutable.cmake` (append only), the three false "digests are pinned" comments, a new docs page, one bullet in this file's Standing conventions |
| T3 | `claude/578-mm-tricks-part3` | #697 (#578 part 3: bind the remaining plain widenings in MM's per-trick vocabulary) | `games/mm/2s2h/Rando/Logic/Regions/*.cpp` (touched seams only), the `MMTrickBindings` test, `OptionsUiSingleExe.cpp` (`kBoundTricks` only) |
| M | `claude/670-mm-mods-folder-mount` | #670 (MM never mounts its `mods/` folder in single-exe) | MM's archive-mount path in `games/mm/2s2h/` (`GameExports_SingleExe.cpp` / `BenPort.cpp`), `src/common` mount helpers if any, `docs/MODDING.md`, a new test |
| P | `claude/582-creation-progress-bar` | #582 (the on-screen generation-progress bar; the phase channel exists, nothing paints) | the progress sink/overlay under `games/oot/soh/SohGui/` or `src/common/`, the creation call site at function granularity, its tests |
| H | `claude/wave4-tracker-docs-hygiene` | Tracker + docs hygiene after wave 3 (this file, epic #644/#645 bookkeeping, the solver-inventory audit's delivered prerequisites, ADR 0010 O9) | `docs/solver-inventory.md` (status annotations only), `docs/adr/0010-*.md` (amendment only), this file, `.claude/lanes/*.md`. No local build. |
| N | `claude/license-elections-and-fipps-removal` | License follow-up: attribution name, remove the all-rights-reserved Fipps font, ship required license texts, elect MIT/CC0 wherever upstream offers it | `LICENSE`, `THIRD_PARTY_NOTICES.md`, `docs/CREDITS.md`, `docs/known-issues.md`, both `OTRGlobals.cpp`/`BenPort.cpp` `LoadFont("Fipps", ...)` call sites, both games' `assets/custom/fonts/` |

Shared-file hotspots this wave: `games/mm/2s2h/Rando/Logic/Regions/*.cpp` is
touched only by **lane T3** (no other lane this wave binds MM trick edges).
`.claude/worker-prompts.md` (this file) is written by both **lane H** (this
header, the lane table, hotspots) and **lane Q** (one bullet under Standing
conventions, the re-pin procedure) — expect a small, easy merge. ADR 0010 is
**read-only for everyone except lane H this wave** (the O9 amendment-log
entry only; no decided text changes). `docs/solver-inventory.md` is likewise
**lane H only** this wave (status annotations on already-decided rows; no
rewritten analysis).

Ordering that is load-bearing: **lane K2a and lane K2b both branch only
after lane K1 merges** (they implement the contract `combo_logic.h`
declares); **lane K3 branches only after K1, K2a and K2b all merge** (it
measures the two real exports through the coordinator). All three report
`blocked` and stop rather than branching early if their prerequisite is not
yet on `main`. Lane H and lane N have local-build status stated on their own
card (H: no build, docs only; N: builds, both tiers). Determinism digests
(`SeedDeterminism`, `MMRandoGen`, `MMPairedAttemptDeterminism`,
`HeadlessForeignDigest`) move only where a lane's brief says a re-pin is
allowed; every other lane asserts the digests stay byte-identical. None of
K1/K2a/K2b/K3's work is wired into a production path, so it cannot move a
generated world by construction — each lane still states that explicitly
rather than relying on the digests to prove it.

This file deliberately holds almost no state. Its failure mode is going stale
— an earlier revision claimed "Wave 3" and "eleven commits awaiting push" for a
day after both were false, and a later one still described the Phase 3.1
lanes a month after they had merged. Everything below lives somewhere that
gets updated as work lands.

## Where the plan actually lives

| What | Where |
|---|---|
| Phase 3.2 tracker (ADR 0010 increments, O4/O9, wave sweeps) | **#500** |
| ADR 0010 increment epics | **#644** (increment 2, merged PR #680; CLOSED 2026-09-21, all prerequisites delivered) → **#645** (increment 3, single-bag fill; O4 ruled composition; coordinator/exports/measurement in flight, lanes K1/K2a/K2b/K3) |
| The O4 solver-inventory audit | `docs/solver-inventory.md` (PR #647); recommended composition; **RULED composition** (operator, 2026-09-17; ADR 0010 amendment) |
| MM per-trick vocabulary (O9) | #578: part 1 (substrate, PR #686) and part 2 (first bindings, PR #696) merged; part 3 (#697, remaining plain widenings, lane T3) in flight |
| Phase 3.1 tracker (closed) | #492 |
| Combo-level settings (ADR 0011) | #498, `docs/adr/0011-combo-level-settings.md` |
| MM hook dispatch coverage | #438 |
| Community reports (human-filed, hands-off) and their agent trackers | #634 → #640 (open: Anchor page), #635 → #638 (resolved), #636 → #639 (resolved) |
| Player-visible known issues | `docs/known-issues.md` |
| Phase 3 roadmap and execution plan (reasoning behind the trackers) | `docs/phase3-roadmap.md`, `docs/phase3-execution-prompt.md` |
| Per-lane worker cards | `.claude/lanes/lane<name>.md` |
| Prior local-iteration postmortems | `docs/ci-gameplay-repro-postmortem.md` |

Read the tracker or issue your card names first. They are the live ones; the docs
are the reasoning behind them.

## Standing conventions

- Branch off `origin/main` as `claude/<description>`. Work in your own git worktree.
- **Touch only the files your lane names.** Waves are parallelized on file
  ownership; editing outside your scope produces conflicting PRs. Where two issues
  share a file, the plan assigns one owner — check before assuming.
- Do not build locally when other agents are running. CI builds (~32 min Linux,
  ~46 min Windows).
- If you touch a file in `.github/clang-format-paths.txt`, run
  `bash run-clang-format.sh` before committing — CI runs it then
  `git diff --exit-code`. clang-format 14, pinned.
- Commit subjects name the concrete fix, never "fix bug". The body explains the
  mechanism and the failure it prevents. Comments explain *why*.
- Push as you work; open a PR when the change is ready for CI; squash-merge only
  once CI is fully green. Never merge red or partial CI — push and report instead.
- **Closing keywords: repeat the keyword per issue.** GitHub closes only the issue directly after the keyword — `Fixes #659 #656` closes #659 and leaves #656 open. Write `Fixes #659, fixes #656`. `Fixes` is for agent-authored issues only; use `Refs #N` for everything else.
- When modifying MM code in single-exe mode, check `src/common/mm_stubs.c` for
  related stubs. Signature drift there has caused two separate faults.
- Configure with `-DCMAKE_C_COMPILER_LAUNCHER=sccache -DCMAKE_CXX_COMPILER_LAUNCHER=sccache`
  and let the build's own guard handle the shared cache: configure prints
  `sccache: partitioning the C/C++ cache key by source tree (#676)`, which means
  ninja's MSVC `/showIncludes` dependency records cannot come from another lane's
  worktree any more. A lane worktree's first build is therefore a real cold build
  (~25 min, not ~8), and the old "wipe the object dirs after merging main or
  editing a widely included header" step is no longer needed. If that line is
  missing from your configure output, it is needed again — see
  `docs/BUILDING_WINDOWS.md`.
- If an issue's premise turns out to be wrong, do not force a fix. Report what you
  found and recommend closing or re-scoping. Several findings have changed shape
  under scrutiny; that is a good outcome.
- Human-filed issues and PRs are hands-off. Anything authored by an account other
  than `spencerduncan` was filed by a person, and only the operator talks to
  people. Never comment on, label, assign, retitle, edit, close, or auto-close
  (`Fixes`/`Closes`/`Resolves #N`) it, and never reply to a person's comment on
  an agent issue or PR; surface it in your report instead. To work on one, file
  a secondary tracking issue titled `[agent] #N: <summary>` with the
  `agent-tracking` label, link the human issue from its body, verify the premise
  in code there, and point PRs at the agent issue (`Fixes #<agent>`,
  `Refs #<human>`). When unsure whether an author is a person, treat them as one.
- No upstream reports unless the operator explicitly asks. Never file, draft,
  propose, or mention an issue, PR, or comment to HarbourMasters or any other
  external repo, and do not offer it as an option. Document inherited defects in
  this tracker only, with the lineage evidence. PRs against the operator's own
  forks (libultraship, ZAPDTR, OTRExporter) are not upstream reports.
- **A green determinism row is not evidence that a world did not change.**
  `SeedDeterminism`/`RandoDeterminism`/`MMPairedAttemptDeterminism` diff two runs
  of your own binary against each other, so they detect nondeterminism only. The
  rows that fail on a MOVED world are the golden ones —
  `GoldenSeedDigestDefault`, `GoldenSeedDigestProfileV1`,
  `GoldenPairedAttemptDigest` — which compare one run against `tests/golden/`. If
  your change is meant to move a world, re-pin deliberately
  (`cmake --build <dir> --target regen-golden-digests`) in its own commit stating
  which fields moved and why; if it is not, a red golden row is the bug report.
  All three rows run in a ROM-staged local run as well as on both CI legs: the two
  archive-sensitive ones generate from an archive-free sandbox under
  `build-cmake/golden-archive-free/` (they used to SKIP there, which left the local
  merge gate with no golden coverage at all). A golden row reported SKIPPED locally
  now means the sandbox could not be built — read its message, do not read it as a
  pass. What the rows still do NOT give you: they pin the archive-free world, not a
  player's (#702). Full policy:
  `docs/determinism-goldens.md`.
- This project is pre-release: invalidating an existing save to land a fix is
  acceptable and does not need product sign-off, but every PR that invalidates a
  save format or a paired file's identity must say so explicitly in its body.

See `CLAUDE.md` for build, test, and architecture basics.
