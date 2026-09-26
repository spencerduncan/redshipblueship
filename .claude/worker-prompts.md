# Worker loop goals — 2026-09-26 wave (wave 5)

**Where the phases stand.** Wave 4 landed increment 3's first real slice for
epic **#645**, none of it wired into a production path: the combo-logic
coordinator (PR #701, follow-up #717), both real solver exports it drives
(OoT PR #715, MM PR #714), and the first cost/convergence measurement over
them (PR #722, `combo-logic-measure`). The measured verdict (#645 comment of
2026-09-22): one linked round is 2 alternations and 4-10 ms; one attempt
extrapolates to 0.63-0.87x the #582 30 s floor, ten retries do not fit the
2x total budget; placement converges but the GOAL is unprovable on every
seed, because both engines de-duplicate assumed items by id within a round
(OoT's 321 advancement rows are 105 distinct ids). **Operator ruling
2026-09-26: multiplicity** — the coordinator passes copies, the engines
handle repeats — and the bag model must also cover plentiful surplus
(OoT `RO_ITEM_POOL_PLENTIFUL`, MM `RO_PLENTIFUL_ITEMS`), more checks than
items (junk fill), and traps as a per-game filler class (OoT `RG_ICE_TRAP`,
MM `RI_TRAP`; no trap crosses games this increment). Also merged in wave 4:
golden determinism digests, enforced locally since #718 (#688, PR #700);
MM per-trick bindings to 25 of 86 keys (#697, PRs #703/#713); MM `mods/`
mounting (#670, PRs #704/#716); the on-screen creation progress bar (#582,
PR #707). Community reports #634/#635/#636 remain human-filed and
hands-off, tracked agent-side as #640/#638/#639.

## Lanes — one card per lane in `.claude/lanes/`

Read `.claude/lanes/SHARED.md` first; it carries the append-only shared-file
protocol and the three-edit CTest registration rule that hard-fails the build if
you do fewer. Then read your own card and the issue or ADR it names.

| Lane | Branch | Serves | Owns, roughly |
|---|---|---|---|
| K4 | `claude/inc3-multiplicity-contract` | #645: multiplicity in the assume contract, and the surplus / filler / trap rules of the bag (the 2026-09-26 ruling) | `src/common/combo_logic.{h,c}`, the `assumeOwnItem`/`place`/pool-export functions of BOTH engine TUs, `test_combo_logic.c`, `test_combo_logic_measure.c` |
| K5 | `claude/inc3-o8-classification-table` | #645: ADR 0010 O8, the single-owner item classification table (progression / junk / renewable / trap) with per-game sources | `src/common/shared_items.{h,c}`, a NEW `classify` export in each engine TU (function granularity), `test_shared_items*.c`; NOT `combo_logic.*` |
| K8 | `claude/inc3-o6-monotonicity-tooling` | #645: ADR 0010 O6, the CI grow-check over both engines and the static negation probe | a new test TU, a new `.github/scripts/` probe with `--self-test`, a CI step (append), a `docs/` page, one Standing-conventions bullet here; branches only after K4 merges |
| W | `claude/world-moving-bundle-583-681-643-719` | #583 drop order, #681 criterion-3 narrowing, #643 O5 46th slice, #719 Deku-stick gate — each with its own golden re-pin | the files each item names, `tests/golden/*`, ADR 0010/0011 amendment paragraphs (append only); NOT `combo_logic.*` or the engine TUs |
| G2 | `claude/693-autosave-interval-host` | #693: host MM's autosave interval on the Combo → MM Enhancements page | the MM Enhancements page and its manifest (`kHostedMmEnhancementCount` and its lock) |
| C1 | `claude/709-windows-rando-tier-trial` | #709: measure running the whole `rando` tier on the Windows CI runner | `.github/workflows/generate-builds.yml` (the Windows gate step); evidence is CI |
| M3 | `claude/705-loose-asset-mods` | #705: loose (unpacked) asset directories mount as mods for both games | the mods mount path in both games, `docs/MODDING.md`, a redship-tier row |
| H2 | `claude/wave5-tracker-docs-hygiene` | Tracker + docs hygiene for wave 5 (#645 body, #708, solver-inventory status, ADR 0010 O9, this file, known issues) | `docs/solver-inventory.md` (status annotations only), `docs/adr/0010-*.md` (amendments only), `docs/known-issues.md`, this file, `.claude/lanes/*.md`. No local build. |

Shared-file hotspots this wave: **both engine TUs**
(`ComboLogicEngineOoT.cpp`, `ComboLogicEngineSingleExe.cpp`) are edited by
**K4** (assume/place/pool functions) and **K5** (a new `classify` export)
at function granularity — expect a merge, never a rewrite of the other's
functions. `combo_logic.*` is **K4 only**; `shared_items.*` is **K5 only**.
ADR 0010 is appended by **H2** (amendments) and **W** (O5 / criterion-3
answer rows, by dated amendment) — both append-only. This file is written by
**H2** (header, lane table) and **K8** (one Standing-conventions bullet).
`tests/golden/*` moves only in **W**, one re-pin commit per intended change.

Ordering that is load-bearing: **K8 branches only after K4 merges** (the
grow-check assumes copies one at a time through the multiplicity contract)
and reports `blocked` otherwise. Every lane except W proves it moved no
world by the three golden rows — `GoldenSeedDigestDefault`,
`GoldenSeedDigestProfileV1`, `GoldenPairedAttemptDigest` — staying green
with the golden files untouched, and says so by row name. Local-build
status is on each card (H2 and C1: no local build).

This file deliberately holds almost no state. Its failure mode is going stale
— an earlier revision claimed "Wave 3" and "eleven commits awaiting push" for a
day after both were false, and a later one still described the Phase 3.1
lanes a month after they had merged. Everything below lives somewhere that
gets updated as work lands.

## Where the plan actually lives

| What | Where |
|---|---|
| Phase 3.2 tracker (ADR 0010 increments, O4/O9, wave sweeps) | **#500** |
| ADR 0010 increment epics | **#644** (increment 2, merged PR #680; CLOSED 2026-09-21, all prerequisites delivered) → **#645** (increment 3, single-bag fill; O4 ruled composition; coordinator, both exports and the first measurement merged in wave 4; multiplicity ruled 2026-09-26; lanes K4/K5/K8 in wave 5) |
| The O4 solver-inventory audit | `docs/solver-inventory.md` (PR #647); recommended composition; **RULED composition** (operator, 2026-09-17; ADR 0010 amendment) |
| MM per-trick vocabulary (O9) | #578 (closed): parts 1-2 (PR #686, PR #696) and part 3's two passes (PR #703, PR #713) merged, 25 of 86 keys bound; #697 open for the owed seams and the `MMRT_PALACE_GUARD_SKIP` judgement |
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
  merge gate with no golden coverage at all). No golden row has a skip path on any
  gate any more: a sandbox that cannot be built FAILS the row, because a broken
  harness is a finding and a skip would put the gate back to enforcing nothing. A
  SKIPPED golden row means somebody re-added a skip. What the rows still do NOT give
  you: they pin the archive-free world, not a player's (#702). Full policy:
  `docs/determinism-goldens.md`.
- This project is pre-release: invalidating an existing save to land a fix is
  acceptable and does not need product sign-off, but every PR that invalidates a
  save format or a paired file's identity must say so explicitly in its body.

See `CLAUDE.md` for build, test, and architecture basics.
