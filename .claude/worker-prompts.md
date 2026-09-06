# Worker loop goals — Phase 3.1 lanes (updated 2026-07-22)

**Current phase: Phase 3.1 — Two-Way Combo Randomizer (#492).** Phase 3.0 closed
its contract (milestone 17/17): four hand-pinned OoT items crossing into MM
checks, one direction, surviving a round trip, described by a spoiler log.

## Lanes — one prompt per lane in `.claude/lanes/`

Read `.claude/lanes/SHARED.md` first; it carries the append-only shared-file
protocol and the three-edit CTest registration rule that hard-fails the build if
you do fewer.

| Lane | Issues | Owns, roughly |
|---|---|---|
| 1 | #490 → #498 ADR → #493 | `context.h`, `foreign_items.*`, OoT rando pool + spoiler |
| 2 | #488 | `Rando/Foreign.cpp`, `Spoiler/Apply.cpp` (for #488 only) |
| 3 | #487 → #491 | `z_sram_NES.c`, `mm_stubs.c`, `mm_rando_gen_test.cpp` |
| 4 | #497 → #499 | `SohGui/*`, ADR 0003/0004, `cvar_shared_keys.h`, `OnFileCreate.cpp` |
| 5 | #489 → #496 | `TrackersGuiSingleExe.*`, trackers, `combo_spoiler_view.*` |
| 6 | #502 → #494 | `CheckQueue.cpp`, `DrawItem.cpp`, both `*_AwardSharedItem` |

Lane 1 is the only lane that changes `.redsave` format and owns the
`reserved[264]` byte budget across all five claimants — Lane 4 must hand it a
digest size before carving, not after.

Ordering that is load-bearing: **Lane 3 lands first** (#487 is P0 and Lane 5's
verification depends on it). Lane 1 rebases onto Lane 2 **before #493 step 7**,
not before step 3 — gating the longest pole on the smallest lane idles the phase.

This file deliberately holds almost no state. Its failure mode is going stale — an
earlier revision claimed "Wave 3" and "eleven commits awaiting push" for a day
after both were false, and that misled planning. Everything below lives somewhere
that gets updated as work lands.

## Where the plan actually lives

| What | Where |
|---|---|
| Phase 3.1 tracker (waves, corrections, current state) | **#492** |
| Per-lane worker prompts | `.claude/lanes/lane<N>.md` |
| Phase 3.1 waves and the four sequencing corrections | `docs/phase3-roadmap.md` §5 |
| Phase 3.2 (cross-game logic, Lane D promoted) | #500 |
| Phase 3.0 tracker (closed contract, prior art) | #392 |
| Phase 3 execution plan and wave assignments | `docs/phase3-execution-prompt.md` |
| Phase 2 follow-ups still open | #381 |
| Pre-alpha v0.1.0 readiness gates | #321 |
| Player-visible known issues | `docs/known-issues.md` |
| Prior local-iteration postmortems | `docs/ci-gameplay-repro-postmortem.md` |

Read #492 and your lane prompt first. They are the live ones; the docs are the
reasoning behind them, and #392 is the closed phase they build on.

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
- When modifying MM code in single-exe mode, check `src/common/mm_stubs.c` for
  related stubs. Signature drift there has caused two separate faults.
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

See `CLAUDE.md` for build, test, and architecture basics.
