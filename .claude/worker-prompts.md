# Worker loop goals — 2026-09-10 wave (updated 2026-09-10)

**Where the phases stand.** Phase 3.1 (#492, two-way combo randomizer) has
shipped Waves 0-2 and most of Wave 3; what remains is #497's `SohMenu` residue and
ADR 0011 increment 2 (#498). Phase 3.2 (#500, ADR 0010, cross-game logic) has its
increment epics filed — **#644** (merged generation, full delivery) precedes
**#645** (the single-bag combo fill), and #645 is gated on the O4
solver-inventory audit. On 2026-08-14 a community member filed three reports
against a GitHub Actions build (#634, #635, #636); they are human-filed and
hands-off, tracked agent-side as #640, #638 and #639.

## Lanes — one card per lane in `.claude/lanes/`

Read `.claude/lanes/SHARED.md` first; it carries the append-only shared-file
protocol and the three-edit CTest registration rule that hard-fails the build if
you do fewer. Then read your own card and the issue or ADR it names.

| Lane | Branch | Serves | Owns, roughly |
|---|---|---|---|
| 1 | `claude/adr0009-4b-death-decline-autosave` | ADR 0009 decision 4b | `docs/adr/0009-*.md`; the MM kaleido death-prompt leg; `MM_Combo_*ExitToOoT` and the MM exit/commit functions in `games/mm/2s2h/GameExports_SingleExe.cpp` |
| 2 | `claude/adr0011-inc2-combo-settings-pane` | ADR 0011 increment 2 (#498) | `src/common/foreign_items.*`, `src/common/cvar_shared_keys.h`, the combo options view/window, the interim Cross-Game rows in `SohMenuRandomizer.cpp`, `docs/adr/0011-*.md` (+ folding PR #628's eight resolutions) |
| 3 | `claude/623-windows-ci-redship-tier` | #623 | `.github/workflows/generate-builds.yml` |
| 4 | `claude/tracker-docs-hygiene-2026-09` | tracker + docs sweep after the 2026-08 wave | `docs/known-issues.md`, this file's lane table, `.claude/lanes/lane*.md`; tracker edits on #500 / #492 / #497 and the ADR 0010 epics |
| 5 | `claude/solver-inventory-audit-o4` | ADR 0010 Decision 4 / open question O4 | `docs/solver-inventory.md` (new) |
| 6 | `claude/638-flush-before-freeze-626-dead-bar` | #638 (flush scene flags before every freeze), #626 (F10 dead bar) | `src/common/switch.cpp` / `context.cpp` freeze path; `Combo_CheckEntranceSwitch`; the hot-swap freeze in both `GameExports_SingleExe.cpp`; new scene-flag-freeze tests |
| 7 | `claude/640-soh-port-registrar-elision` | #640 (Anchor registrar elided from `soh_port`; empty-page `SetNextWindowPos` leak) | `games/oot/CMakeLists.txt`, `games/oot/soh/SohGui/Menu.cpp`, `.github/scripts/check-registrar-elision.sh` |
| 8 | `claude/639-first-arrival-clock` | #639 option A (new-file clock on first MM arrival) | `MM_Play_ConsumeStartupEntrance` in `games/mm/src/code/z_play.c`; `games/mm/2s2h/mm_resume_state_test.cpp` |

Shared-file hotspots this wave: `games/mm/2s2h/GameExports_SingleExe.cpp` is
touched by **lane 1** (the exit/commit functions) and **lane 6** (the hot-swap
freeze) — function-scoped claims, rebase rather than reorder. Nobody else edits it.
`src/common/context.cpp` is lane 6's this wave. The ADRs are single-owner: 0009 is
lane 1's, 0011 is lane 2's, 0010 is **read-only for everyone** (lane 5 writes a new
doc, not the ADR).

Ordering that is load-bearing: lanes 6 and 8 both sit on MM's switch path but in
different functions — lane 6's flush goes *before* the freeze on departure, lane 8's
re-authoring goes in the first-entry leg of the consume on arrival; neither should
move the other's code. Lane 2's pane is the interim host until #497 step 6 builds
the tier-4 Combo section; lane 2 does **not** build step 6. Lane 4 has no local
build; lane 5 is docs-only; every code lane verifies locally (ROM-staged build, both
ctest tiers) before its PR is merged.

This file deliberately holds almost no state. Its failure mode is going stale — an
earlier revision claimed "Wave 3" and "eleven commits awaiting push" for a day
after both were false, and a later one still described the Phase 3.1 lanes a month
after they had merged. Everything below lives somewhere that gets updated as work
lands.

## Where the plan actually lives

| What | Where |
|---|---|
| Phase 3.2 tracker (ADR 0010 increments, O4/O9, the 2026-09-10 sweep) | **#500** |
| ADR 0010 increment epics | **#644** (increment 2, merged generation) → **#645** (increment 3, single-bag fill) |
| Phase 3.1 tracker (closing; re-scoped 2026-09-10) | #492 |
| Per-lane worker cards | `.claude/lanes/lane<N>.md` |
| Combo-level settings (ADR 0011, increments 2-4) | #498 |
| MM hook dispatch coverage | #438 |
| Community reports (human-filed, hands-off) and their agent trackers | #634 → #640, #635 → #638, #636 → #639 |
| Player-visible known issues | `docs/known-issues.md` |
| Phase 3 roadmap and execution plan (reasoning behind the trackers) | `docs/phase3-roadmap.md`, `docs/phase3-execution-prompt.md` |
| Phase 3.0 tracker (closed contract, prior art) | #392 |
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
