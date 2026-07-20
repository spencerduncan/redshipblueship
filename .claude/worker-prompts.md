# Worker loop goals — Wave 4 (updated 2026-07-19)

**Current phase: Phase 3.0 — Basic Combo Randomizer.** Phase 2 closed with PR #368
(merged as c8c47fa6): the first full OoT↔MM gameplay round trip.

This file deliberately holds almost no state. Its failure mode is going stale — an
earlier revision claimed "Wave 3" and "eleven commits awaiting push" for a day
after both were false, and that misled planning. Everything below lives somewhere
that gets updated as work lands.

## Where the plan actually lives

| What | Where |
|---|---|
| Phase 3 tracker (lanes, waves, current state) | **#392** |
| Phase 3 execution plan and wave assignments | `docs/phase3-execution-prompt.md` |
| Phase 3 roadmap (MVP definition, lanes A–D) | `docs/phase3-roadmap.md` |
| Phase 2 follow-ups still open | #381 |
| Pre-alpha v0.1.0 readiness gates | #321 |
| Player-visible known issues | `docs/known-issues.md` |
| Prior local-iteration postmortems | `docs/ci-gameplay-repro-postmortem.md` |

Read #392 first. It is the live one; the docs are the reasoning behind it.

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

See `CLAUDE.md` for build, test, and architecture basics.
