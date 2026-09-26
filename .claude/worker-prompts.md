# Worker loop goals — 2026-09-26 wave (wave 6)

**Where the phases stand.** Wave 5 moved increment 3 of epic **#645** from
"placement converges, the proof does not" to "the proof holds once the bag
fits". Nothing it built is wired into a production path yet. Here is what
landed:

- **The multiplicity contract** (PR #728, `combo_logic.h` ABI 3).
  - One `assumeOwnItem` call is one copy, and neither engine de-duplicates.
  - Progressives clamp at their top tier behind a round-scoped flag, and
    counters clamp at their maxima.
  - Bag rows carry `bagFlags` (`RSBS_COMBO_BAG_SURPLUS`). The proof places
    required rows only; surplus is placed after the proof, and leftover hosts
    go to each game's own pass (`Combo_Logic_LeftoverHosts`).
  - **Traps are counted pool rows in both ports**, so "a trap never crosses"
    is a caller convention the wiring must keep, not a property of the
    coordinator.
- **The O8 classification table** (PR #725, `src/common/shared_items.*`).
  - The classes are trap > progression > renewable > junk, with one source
    per game.
  - Nothing reads it yet. Follow-ups: #731 (one class per cross-game shared
    quantity) and #733 (MM heart pieces gate two checks).
- **The O6 tooling** (PR #734): the `ComboLogicMonotonicity` grow-check over
  both engines, the negation probe with its read baseline, and the review
  rule below.
- **The world-moving bundle** (PR #729):
  - #583: forward drop order, re-pinned.
  - #681: criterion 3 is profile-conditional. This added a fourth golden,
    `GoldenSeedDigestArmedCaps`.
  - #643: O5, 46 time slices; no golden moved.
  - #719: the Deku-Stick trick gate; no golden moved.
- **Smaller changes:**
  - the MM autosave interval slider (PR #730, #693);
  - loose asset mods for both games (PR #732, #705);
  - the whole `rando` tier on Windows CI (PR #723, #709).

**Measured** (lane K4's #645 comment of 2026-09-26):

- `beat-either` is proved on the 321+191 bag: 3/3 seeds, first attempt, 0
  roll-backs.
- `beat-both` fails on every bag that fits `RSBS_COMBO_LOGIC_BAG_CAP` 512.
  The only reason is that the capped MM half is a stride sample (#727).
- With the caps at 4096 (an uncommitted experiment), the whole 2489-row bag
  proves `beat-both` on the first attempt in 44.5-50.9 s. That is 1.49-1.70x
  the #582 30 s floor, at 17.9 ms per round inside the fill.
- The bag as K8 read it:
  - OoT: 546 rows (321 progression, 105 surplus, 114 filler, 6 ice traps).
  - MM: 2282 rows (250 progression, 114 surplus, 1918 filler).

  A bag of progression and surplus only (about 790 rows) should cut an
  attempt to roughly a third. Wave 6 composes that bag, persists the
  crossings, wires the fill (held for the operator), carries the shared
  triforce count, and fixes OoT's own plentiful overshoot.

Community reports #634/#635/#636 remain human-filed and hands-off. Their agent
trackers #640/#638/#639 are all closed (PRs #651, #650, #648).

## Lanes — one card per lane in `.claude/lanes/`

Read `.claude/lanes/SHARED.md` first. It carries the append-only shared-file
protocol and the three-edit CTest registration rule, which hard-fails the
build if you make fewer than three edits. Then read your own card and the
issue or ADR it names.

| Lane | Branch | Serves | Owns, roughly |
|---|---|---|---|
| K9 | `claude/inc3-bag-composition-o8` | #645, #727, #731, #733: compose the bag from the O8 table (progression and surplus only), raise the caps, re-measure | `src/common/combo_logic.{h,c}`, `src/common/shared_items.{h,c}`, the bag-building and classify functions of BOTH engine TUs, the measure/bag-model/multiplicity tests |
| K10 | `claude/inc3-crossing-persistence-o7` | #645 O7: persist cross-game placements, print them in the one spoiler, rebuild the coordinator's tables from storage | `src/common/context.{h,c}` (the carve or a new append-only block), the persistence/hydrate functions of `foreign_items.{h,c}` (not the pool tables) and of both engine TUs, the spoiler's `"combo"` section writer |
| K11 | `claude/inc3-single-bag-fill-wiring` | #645 D3/D5: the single-bag fill at the creation event; items leave origin pools; both pinned pools retire; all four goldens re-pinned in one commit. **PR HELD for the operator** | the creation-event seam (`ForeignItemsSingleExe.cpp`, `fill.cpp`'s general pass, `OnFileCreate.cpp`'s paired branch), `kForeignPoolV1` and the MM pinned pool, `tests/golden/*`, an ADR 0010 amendment; starts only after K9 and K10 are on `main` |
| K12 | `claude/inc3-o10-shared-triforce-count` | ADR 0010 O10: one shared triforce piece count across both worlds | the triforce carrier in `src/common/shared_resources.*`, a new triforce function in each engine TU, each port's hunt-win seam, the coordinator's `triforce-hunt` goal predicate |
| F26 | `claude/726-oot-progressive-overshoot` | #726: OoT's own fill overshoots progressive copies past the top tier (plentiful wraps the wallet) | the OoT clamp in `games/oot/soh/Enhancements/randomizer/logic.cpp` (inside `RSBS_SINGLE_EXECUTABLE`), a plentiful-profile lock row |
| H3 | `claude/wave6-tracker-docs-hygiene` | Tracker and docs hygiene for wave 6 (#645 body, ADR 0010 amendments, solver-inventory status, known issues, this file) | `docs/solver-inventory.md` (status annotations only), `docs/adr/0010-*.md` (amendments only), `docs/known-issues.md`, this file, `.claude/lanes/*.md`; agent issue bodies. No local build. |

**Shared-file hotspots this wave.**

- **Both engine TUs** (`ComboLogicEngineOoT.cpp`, `ComboLogicEngineSingleExe.cpp`)
  are edited by three lanes, each at function granularity:
  - **K9**: bag-building and classify;
  - **K10**: new persistence/hydrate functions;
  - **K12**: a triforce carrier.

  Expect merges; never rewrite another lane's functions.
- `combo_logic.*` and `shared_items.*` are **K9's**.
- `context.*` and the persistence half of `foreign_items.*` are **K10's**. The
  pinned-pool half is retired by **K11**.
- `logic.cpp`'s clamp is **F26's**.
- ADR 0010 is appended by **H3** and later by **K11**, append-only.
- `tests/golden/*` moves only in **K11**, in one re-pin commit.

**Load-bearing ordering.**

- **K11 starts only after K9's and K10's PRs are merged**, and reports
  `blocked` otherwise.
- **K11 supports `triforce-hunt` only if K12 is on `main`** when it starts. If
  it is not, K11 refuses that GOAL at creation, with a reason.
- **Production wiring is K11's alone, and its PR is HELD.** It is gated in
  full but not merged; the operator decides when. No other lane wires the
  coordinator into `fill.cpp`, `OnFileCreate.cpp` or the creation event.

**Proving a lane moved no world.** Every lane except K11 proves it with the
four golden rows staying green and the golden files untouched, and names the
rows: `GoldenSeedDigestDefault`, `GoldenSeedDigestProfileV1`,
`GoldenPairedAttemptDigest`, `GoldenSeedDigestArmedCaps`. F26 changes OoT's
worlds under plentiful only. The shipped default is not plentiful, so if any
golden moves, F26 stops and reports.

**Local builds.** H3 does no local build. At most two lanes build at once.

This file deliberately holds almost no state. Its failure mode is going stale
— an earlier revision claimed "Wave 3" and "eleven commits awaiting push" for a
day after both were false, and a later one still described the Phase 3.1
lanes a month after they had merged. Everything below lives somewhere that
gets updated as work lands.

## Where the plan actually lives

| What | Where |
|---|---|
| Phase 3.2 tracker (ADR 0010 increments, O4/O9, wave sweeps) | **#500** |
| ADR 0010 increment epics | **#644** (increment 2, merged PR #680; CLOSED 2026-09-21, all prerequisites delivered) → **#645** (increment 3, single-bag fill; O4 ruled composition; coordinator, both exports and the first measurement merged in wave 4; multiplicity (PR #728), the O8 table (PR #725) and the O6 tooling (PR #734) merged in wave 5; lanes K9/K10/K11 (held)/K12 in wave 6) |
| The O4 solver-inventory audit | `docs/solver-inventory.md` (PR #647); recommended composition; **RULED composition** (operator, 2026-09-17; ADR 0010 amendment) |
| MM per-trick vocabulary (O9) | #578 (closed): parts 1-2 (PR #686, PR #696) and part 3's two passes (PR #703, PR #713) merged, 25 of 86 keys bound, 26 after the Deku-Stick gate (PR #729, #719); #697 open for the owed seams and the `MMRT_PALACE_GUARD_SKIP` judgement |
| Phase 3.1 tracker (closed) | #492 |
| Combo-level settings (ADR 0011) | #498, `docs/adr/0011-combo-level-settings.md` |
| MM hook dispatch coverage | #438 |
| Community reports (human-filed, hands-off) and their agent trackers | #634 → #640 (resolved, PR #651), #635 → #638 (resolved, PR #650), #636 → #639 (resolved, PR #648); the human issues stay open for the operator |
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
  `GoldenPairedAttemptDigest`, `GoldenSeedDigestArmedCaps` — which compare one run against `tests/golden/`. If
  your change is meant to move a world, re-pin deliberately
  (`cmake --build <dir> --target regen-golden-digests`) in its own commit stating
  which fields moved and why; if it is not, a red golden row is the bug report.
  All four rows run in a ROM-staged local run as well as on both CI legs: the three
  archive-sensitive seed rows generate from an archive-free sandbox under
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
- **Reachability conditions never negate player state (ADR 0010 O6).** In either
  graph (OoT `location_access/**`, MM `Logic/Regions/**` and `Logic.h`, and the
  helpers they call) a condition may REQUIRE an item, event, flag, count, age or
  time but never its ABSENCE: no `!` (or `not`, `^ true`, `? false :`) over a
  player-state term, no `< k` / `== 0` on a count, no `if (HAS_X) return false;`. Negating a setting, trick or option
  is fine. Model "before event X" as time/region state, never as `!event`. The
  static probe (`python3 .github/scripts/check-monotonicity-negations.py`, CI job
  `monotonicity-probe`) and the `ComboLogicMonotonicity` rando row enforce it; a
  new probe-baseline entry needs a written reading proving the site monotone.
  Details: `docs/monotonicity.md`.

See `CLAUDE.md` for build, test, and architecture basics.
