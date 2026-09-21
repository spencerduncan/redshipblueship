# Worker loop goals — 2026-09-17 wave (wave 3; updated 2026-09-17)

**Where the phases stand.** Phase 3.2 (#500, ADR 0010, cross-game logic)
increment 2 **merged** as PR #680 (`7bab54bd`): the whole paired creation now
runs once, at the OoT file-create seam, and MM arrival is hydrate-or-refuse
with zero generation capability. Increment 3 (#645, the single-bag fill and
the beatability proof) is next; its gate, the O4 solver-inventory audit
(`docs/solver-inventory.md`, PR #647), is delivered and recommends composition,
but the operator has not yet ruled on O4 itself. This wave lands increment 3's
prerequisites (#656/#657/#658/#659/#661/#667), MM's per-trick vocabulary
(#578, split into parts), the `SohMenu` remainder (#497) and what it unblocks
(#682), a workstation build-cache hazard (#676), the repo's license file, and
this tracker/docs sweep. Phase 3.1 (#492) is closed. Community reports
#634/#635/#636 remain human-filed and hands-off, tracked agent-side as
#640/#638/#639 (resolved in earlier waves; #634/#640's Anchor-page regression
is the only one still open, unrelated to this wave).

## Lanes — one card per lane in `.claude/lanes/`

Read `.claude/lanes/SHARED.md` first; it carries the append-only shared-file
protocol and the three-edit CTest registration rule that hard-fails the build if
you do fewer. Then read your own card and the issue or ADR it names.

| Lane | Branch | Serves | Owns, roughly |
|---|---|---|---|
| A1 | `claude/578-mm-tricks-part1` | #578 part 1 (MM per-trick vocabulary substrate, ADR 0010 O9) | `games/mm/2s2h/Rando/Logic/Logic.h` (`CAN_USE_EXPLOSIVE` macro only), `Regions/GreatBayTemple.cpp:120`, new `StaticData/Tricks.*`, the MM rando save-options storage, `Foreign.cpp` (`ProfileIdentityString` only), `OptionsUiSingleExe.cpp` (new Tricks section), `mm_rando_options_test.cpp` |
| A2 | `claude/578-mm-tricks-part2` | #578 part 2 (bind first candidate tricks to region seams) | `games/mm/2s2h/Rando/Logic/Regions/*.cpp` (touched seams only, not `Moon.cpp`); starts only after A1 merges |
| B | `claude/inc3-prereqs-657-667-659-656` | Increment-3 prerequisites #657, #667, #659, #656 | `src/common/foreign_items.c/.h`, `src/common/tests/test_combo_settings.c`, `games/oot/soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp`, `playthrough.cpp` (gate site only), `games/mm/2s2h/Rando/Logic/Logic.cpp` (entrance cache only), `Foreign.cpp` only if #667 forces a consumer change, `docs/adr/0011-*.md` (amendment only) |
| C | `claude/658-majora-defeated-predicate` | #658 (MM_GOAL's "Majora defeated" predicate) | `games/mm/2s2h/Rando/Logic/Regions/Moon.cpp`, `CanKillEnemy`/a new function appended at the END of `games/mm/2s2h/Rando/Logic/Logic.h` |
| D | `claude/497-sohmenu-remainder` | #497 (ADR 0004 remainder: capability gating, shared-intent marker, tier-4 Combo section) | `games/oot/soh/SohGui/SohMenu.cpp/.h`, `SohMenuRandomizer.cpp` (interim rows only), new `SohMenuCombo.cpp`, `src/common/combo_settings_view.*` (function granularity, if needed), `docs/adr/0004-*.md` (amendment) |
| G | `claude/682-mm-enhancement-toggles` | #682 (host MM enhancement toggles in the unified menu) | `games/mm/CMakeLists.txt` (carve-outs), provider TUs under `games/mm/2s2h/Enhancements/` (guards only), `.github/scripts/check-registrar-elision.sh`, `mm_registrar_coverage_test.cpp`, lane D's MM sub-section extension point, `src/common/cvar_shared_keys.h` (append); starts only after D merges |
| E | `claude/676-sccache-showincludes-deps` | #676 (sccache `/showIncludes` replay poisons ninja's MSVC deps across worktrees) | `CMakeLists.txt` / `CMake/DefaultCXX.cmake` (sccache-conditional block only), `docs/BUILDING_WINDOWS.md`, one bullet in this file's Standing conventions, an optional `scripts/` tool |
| F | `claude/wave3-tracker-docs-sweep` | Tracker + docs sweep after increment 2 (#662 and wave-3 bookkeeping) | `docs/adr/0010-*.md` (anchors + amendment log only), `docs/known-issues.md`, this file, `.claude/lanes/*.md` |
| H | `claude/661-pin-mask-shop-entrance` | #661 (pin the Happy Mask Shop interior out of OoT's entrance shuffle) | `games/oot/soh/Enhancements/randomizer/entrance.cpp` (pool construction), `randomizer_entrance_tracker.cpp` only if a display fix is needed |
| L | `claude/root-license-and-third-party-notices` | Root `LICENSE` (MIT, if appropriate) and `THIRD_PARTY_NOTICES.md` | `LICENSE`, `THIRD_PARTY_NOTICES.md`, `docs/CREDITS.md`, the README's license line. No local build. |

Shared-file hotspots this wave: `games/mm/2s2h/Rando/Logic/Logic.h` is touched
by **lane A1** (the `CAN_USE_EXPLOSIVE` macro near `:289`) and **lane C** (a new
function appended at the END of the header, kept far from A1's hunk) —
function-scoped claims, rebase rather than reorder, trivial merge expected.
ADR 0010 is **read-only for everyone except lane F this wave** (anchor
corrections and one amendment-log entry only; no decided text changes).
`.claude/worker-prompts.md` (this file) is written by both **lane F** (the
header, lane table, hotspots) and **lane E** (one bullet under Standing
conventions) — expect a small, easy merge.

Ordering that is load-bearing: **lane A2 branches only after lane A1 merges**
(it builds on A1's `MMRT_*` table, storage and `MM_TRICK(...)` predicate);
**lane G branches only after lane D merges** (it hosts its rows in lane D's
tier-4 Combo section extension point). Both lanes report `blocked` and stop
rather than branching early if their prerequisite is not yet on `main`. Lane F
and lane L have no local build; every code lane verifies locally (ROM-staged
build, both ctest tiers) before its PR is merged. Determinism digests
(`SeedDeterminism`, `MMRandoGen`, `MMPairedAttemptDeterminism`,
`HeadlessForeignDigest`) move only where a lane's brief says a re-pin is
allowed (lane A1, gating the Powder Keg and the GBT boss-key edge); every
other lane asserts the digests stay byte-identical.

This file deliberately holds almost no state. Its failure mode is going stale
— an earlier revision claimed "Wave 3" and "eleven commits awaiting push" for a
day after both were false, and a later one still described the Phase 3.1
lanes a month after they had merged. Everything below lives somewhere that
gets updated as work lands.

## Where the plan actually lives

| What | Where |
|---|---|
| Phase 3.2 tracker (ADR 0010 increments, O4/O9, wave sweeps) | **#500** |
| ADR 0010 increment epics | **#644** (increment 2, merged as PR #680) → **#645** (increment 3, single-bag fill, gated on #578, the O4 ruling, and #656/#657/#658/#659/#661/#667) |
| The O4 solver-inventory audit | `docs/solver-inventory.md` (PR #647); recommends composition; **decision awaiting the operator** |
| MM per-trick vocabulary (O9) | #578, split into part 1 (substrate, lane A1), part 2 (first bindings, lane A2), part 3 (remaining bindings, filed once part 2 knows the unbound set) |
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
  Full policy: `docs/determinism-goldens.md`.
- This project is pre-release: invalidating an existing save to land a fix is
  acceptable and does not need product sign-off, but every PR that invalidates a
  save format or a paired file's identity must say so explicitly in its body.

See `CLAUDE.md` for build, test, and architecture basics.
