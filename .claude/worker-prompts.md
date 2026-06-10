# Wave 2 worker loop goals (2026-06-09)

Four independent lanes. Each section is a complete, self-contained loop goal for one code worker:
run it as a `/loop` goal (self-paced) or as a plain dispatch prompt. Lanes 1–3 can run as cloud
workers; Lane 4 must run on the maintainer's local machine (it prunes local worktrees).

Supersedes the Wave 1 prompts (issues #154–160, all closed — see git history of this file).

## Common rules (all lanes)

- Repo: `spencerduncan/redshipblueship`. Default branch `main`. All merges are **squash merges**.
- **You are explicitly authorized to open PRs, merge them once green, and close issues for your
  lane.** This overrides the repo's usual "push branch only" convention — closure is the goal.
  If your environment's GitHub credentials cannot merge, stop at "CI green + ready to merge" and
  report that state instead.
- CI is the authoritative gate: `clang-format`, `clang-tidy`, `generate-builds` (Linux + Windows),
  and `integration-tests-linux` (runs `ctest --label-regex "^redship$"` — BootOoT/BootMM/Roundtrip
  live there). Full wall clock ≈ 25–40 min; Windows is the slow job.
- Local builds (if your env supports them): `git submodule update --init`, `cmake -B build -S .`,
  `cmake --build build --parallel`. Needs 8 GB+ RAM. Prebuilt-deps image:
  `ghcr.io/spencerduncan/redshipblueship-build:latest`. When in doubt, trust CI over local.
- **Always `git fetch` before touching any branch** — stale local copies of work branches exist
  and have caused confusion before.
- Loop protocol: each iteration = (1) fetch + assess state against the closure criteria,
  (2) take the smallest next step, (3) push, (4) wait for CI (`gh pr checks <n> --watch` or poll
  ~5 min), (5) on red, diagnose and fix — max 3 attempts per distinct failure, then stop and
  report, (6) on green, advance. Exit the loop only when every closure criterion is true.
- Post a one-line progress comment on your lane's driving issue at each milestone so other lanes
  and humans can see state. Report outcomes faithfully — failed checks get quoted, not summarized
  away.
- Never force-push over commits you didn't write without fetching and reading them first.

---

## Lane 1 — merge PR #249 (scrolling texture interpolation)

**Goal:** PR #249 is merged to main and issue #234 is closed.

**Closure criteria:**
- [ ] PR #249 squash-merged with fresh (post-update) green CI
- [ ] post-merge CI on `main` green
- [ ] issue #234 closed with a comment linking the merge

**State as of 2026-06-09:** head `claude/texture-interp-fix-234`; GitHub reports
CLEAN/MERGEABLE, but the green CI is from 2026-04-25 — stale. Main has since absorbed the
unified save system (#284/#294), the `combo/` directory removal (#285), GCC 16 toolchain work
(#295), and the 2S2H sync (#296). A semantic break is possible even with no textual conflict.

**Steps:**
1. `gh pr update-branch 249` (fall back to: fetch, rebase onto `origin/main`, force-push).
2. Wait for all checks on the updated head.
3. If red: diagnose. Likely candidates are header moves from the `combo/` removal
   (`SharedGraphics` now lives in `src/common/`) and interpolation hooks touched by upstream
   sync. Fix on the branch, push, re-wait.
4. On green: squash-merge keeping `(#234)` in the title. Confirm main's post-merge run is green.
5. Close #234 with a link to the merge commit.

---

## Lane 2 — drive PR #247 to merge (article chain #255 → #256 → #257 → #258)

**Goal:** PR #247 (Custom Messages → Hooks/ShipInit refactor) is merged; issues #228, #253,
#254, #255, #256, #257, #258 are all closed.

**Closure criteria:**
- [ ] zero `TODO_EN_ARTICLE` / `TODO_DE_ARTICLE` / `TODO_FR_ARTICLE` anywhere on the branch
- [ ] RG_CRAWL gimessage entries exist for EN/DE/FR in `Messages/ItemMessages.cpp`
- [ ] PR #247 updated against current main, full CI green, squash-merged
- [ ] #228, #253–#258 closed with evidence comments; a comment on #235 announces the gate is
      clear for #289–#293

**State as of 2026-06-09:** head `claude/custom-messages-hooks-228`, remote tip `842537df18`
(force-pushed 2026-04-29) — **the rebase (step 1, #254) is already done.** The tip contains
exactly 9 `TODO_EN_ARTICLE`, 9 `TODO_DE_ARTICLE`, 9 `TODO_FR_ARTICLE` slots in
`games/oot/soh/Enhancements/randomizer/item_list.cpp` (the 9 rows PR #248 added: 8 masks +
RG_CRAWL). Beware stale local copies of this branch (`7fed3ab4b9` predates the rebase) — start
from origin's tip. Work the steps strictly in sequence, one commit + push per step.

**Steps (read each issue body first — it has the exact spec):**
1. If #254 is still open, close it: the rebase landed in `842537df18` on 2026-04-29.
2. **#255 (EN):** fill the 9 `TODO_EN_ARTICLE` slots — 8 mask rows get `"the "` (lowercase,
   trailing space, matching the existing table convention); RG_CRAWL gets `""`. Touch no other
   rows. Commit, push, close #255.
3. **#256 (DE):** fill the 9 `TODO_DE_ARTICLE` slots in **accusative case** (`den/die/das` per
   noun gender — verify against neighboring rows); RG_CRAWL gets `""`. Also add the German
   RG_CRAWL gimessage in `Messages/ItemMessages.cpp` (the old `GIMESSAGE_NO_GERMAN` left DE
   untranslated; the rebased branch uses `OnOpenText` hooks — add DE alongside EN/FR). Commit,
   push, close #256.
4. **#257 (FR):** fill the 9 `TODO_FR_ARTICLE` slots — masks get `"le "` (verify gender, `la `
   where the noun requires); RG_CRAWL gets `""`. Commit, push, close #257.
5. **#258 (verify, read-only):** confirm zero TODO placeholders repo-wide on the branch; confirm
   EN/DE/FR RG_CRAWL entries; spot-check article plausibility; `gh pr update-branch 247`; wait
   for full CI including `integration-tests-linux` (BootOoT + Roundtrip run there). Pay special
   attention to `src/common/mm_stubs.c` — the PR touches it and main has changed it since April
   (#294); a duplicate or already-removed stub is the most likely build break. If a check fails,
   fix is allowed only for trivial integration breakage; anything substantive gets a follow-up
   issue per the #258 spec. Post verification results on #253.
6. On green: squash-merge PR #247. Close #228, #253, #258. Comment on #235: shuffle features
   #289–#293 and the 9.2.x absorption are now unblocked (they must still land one at a time).

---

## Lane 3 — MM tracker architecture sync (#297)

**Goal:** issue #297 closed via a merged PR that brings MM's trackers up to the upstream
baseline architecture.

**Closure criteria:**
- [ ] `ItemTracker.{cpp,h}` matches the upstream architecture from 2S2H PR #1460
      (`TrackerItemType` / `TrackerGroup` / `DrawItemTrackerSlot` API)
- [ ] `CheckTracker.cpp` uses the display-mode-CVAR architecture from 2S2H PR #1368
- [ ] `Enhancements/Trackers/DisplayOverlay.{cpp,h}` still wires up correctly
- [ ] MM boots and cross-game switching still works (CI integration tests green)
- [ ] PR squash-merged; #297 closed with a note that the #238 PR-A revival (individual tracker
      improvement ports, e.g. upstream #1492/#1633) is now unblocked

**Context:** files live under `games/mm/2s2h/Enhancements/Trackers/ItemTracker/` and
`games/mm/2s2h/Rando/CheckTracker/`. Upstream is HarbourMasters/2ship2harkinian. This is a
baseline-bumping sync (effort L), not a cherry-pick — our tracker code predates the upstream
overhaul, which is why direct cherry-picks failed for the Wave 1 worker (its notes are on
branch `claude/2s2h-sync-238`, commit `b89a340191`, and in issue #297). **Out of scope:** the
improvement commits that piggyback on the new architecture — those are a later "#238 PR-A"
follow-up.

**Single-exe gotchas:** MM symbols carry the `MM_` prefix in single-exe builds; check
`src/common/mm_stubs.c` for tracker-adjacent stubs; menu integration must adapt to the RSBS
unified menu rather than porting 2S2H's BenGui menu verbatim.

**Steps:** work on branch `claude/mm-tracker-sync-297`. Port in reviewable commits
(ItemTracker first, then CheckTracker, then DisplayOverlay validation), push early and often so
progress survives session restarts. Open the PR once it compiles; iterate against CI; on green,
squash-merge and close #297. This lane never touches `item_list.cpp`/`randomizer.cpp`, so it
runs freely in parallel with Lane 2.

---

## Lane 4 — admin closeout (LOCAL machine only)

**Goal:** the tracker reflects reality and the local checkout stops lying about in-flight work.

**Closure criteria:**
- [ ] #254 closed (evidence: rebase landed in `842537df18`, 2026-04-29; 9 TODO slots per
      language verified present 2026-06-09) — skip if Lane 2 already closed it
- [ ] Phase 0 (milestone 1) and Phase 1 (milestone 2) closed:
      `gh api -X PATCH repos/spencerduncan/redshipblueship/milestones/<n> -f state=closed`
- [ ] `git worktree prune` run (clears the five stale `/tmp/rsbs-*` entries)
- [ ] parked worktrees removed: anything with clean status, no commits ahead of `origin/main`,
      HEAD at a stale base (most sit at `dd0e78d617` or `48a2d45b8e`)
- [ ] confirmed-merged local branches deleted (audited 2026-06-09, content landed via squash):
      `claude/shuffle-features-235`, `claude/config-updaters-d7e80a`, `claude/appimage-fix-232`,
      `claude/reconcile-otrglobals-3ef93e`, `claude/enhancements-ui-237`,
      `claude/entrance-tracker-239`, `claude/language-system-229`,
      `claude/rom-extraction-ux-9be948e1`, `claude/docker-worker-hardening`,
      `claude/optimize-windows-ci-43307fyg`, `claude/2s2h-sync-238`, `claude/sync-save-buffer-203`,
      `claude/oot-state-restoration-170`, plus empty `fix/issue-233-segfault-quit`
- [ ] a final report listing everything removed and everything deliberately skipped

**Safety rails:**
- Verify before each removal: `git status` clean in that worktree AND (zero commits ahead of
  `origin/main` OR the branch is in the audited list above).
- **Never touch:** `main`, `claude/custom-messages-hooks-228` (live PR #247),
  `claude/texture-interp-fix-234` (live PR #249), `claude/build-gcc16-ci` (primary checkout's
  current branch), `claude/ecstatic-lalande-f82e36`, `claude/exciting-bartik-4d94e5`, or any
  worktree with uncommitted changes.
- **Do not delete remote branches.** List candidates (e.g. the stale
  `claude/fix-randomizer-compilation-6LcCr`, Jan 2026) in the final report for the maintainer
  to confirm.
