You are Lane 4 of eight in the 2026-09-10 wave: **tracker and docs hygiene after the 2026-08 wave and the month-long gap.**

- **Branch:** `claude/tracker-docs-hygiene-2026-09`
- **Serves:** the bookkeeping the 2026-08-02 sweep comments on #492, #500 and #564 asked for and nobody did; `docs/known-issues.md`, which still told players cross-game randomization "does not exist yet" when three community reports (#634, #635, #636) were filed against a build that has it.

## Scope

- Tracker (via `gh`, agent-authored issues only): file the ADR 0010 increment-2 and increment-3 epics under #500 (**#644**, **#645**), adopt #582/#583/#585 under #644 and #584 (closed) under #645, file the O5 time-slice pin (**#643**), sweep #500 at `c8947177`; retitle #497 to its `SohMenu` remainder and strike the delivered steps in its body; re-scope #492 without closing it.
- Local branch cleanup in the main checkout, read-only unless proven safe: delete only a `claude/*`, `worktree-agent-*` or `review/*` branch with a merged PR at its tip or a tip that is an ancestor of `origin/main`; never `main`, the WIP branch, a worktree-checked-out branch, or any branch of this wave.
- Docs PR: refresh `docs/known-issues.md` (every entry re-verified by `gh issue view` and, where it makes a code claim, in code); rewrite this wave's lane table in `.claude/worker-prompts.md` without touching Standing conventions; replace the stale Phase 3.1 lane prompts with these eight cards.

## Owns

`docs/known-issues.md`, the lane table and phase intro in `.claude/worker-prompts.md`, `.claude/lanes/lane1.md` … `lane8.md`. `SHARED.md` stays as is.

## Hard rules

Human-filed issues (#634, #635, #636) are untouched — a docs link by number is fine, an interaction is not. Nothing is closed. Every state claim written into an issue or the doc is checked at `c8947177`, not copied from an older comment (the 2026-08-02 sweep was wrong about O1 — it had already landed in PR #581).

## Verify

No local build for this lane. The docs PR must pass CI's clang-format and docs checks; the tracker edits are verified by re-reading them on GitHub.
