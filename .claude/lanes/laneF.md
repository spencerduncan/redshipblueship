# Lane F — tracker and docs sweep after increment 2 (#662 and wave-3 bookkeeping)

**Branch:** `claude/wave3-tracker-docs-sweep`

**Task:** No local build; docs and tracker edits only. Re-measure and correct
ADR 0010's six drifted MM code anchors (`docs/adr/0010-*.md`), with an
amendment-log entry naming the SHA measured against. Update the #644 and #645
epic bodies to reflect PR #680's merge, the operator's 2026-09-17 rulings
(#578 split, #643 fold, #661 pin, #497 go-ahead, #682 sequencing, #676 in
flight, #582's remaining scope), and the O4 audit's delivered-but-unruled
status. Post one dated ruling-recording comment each on #643, #578, #661,
#497, #682, #676, #582. Close #663 record-only. Rewrite this file
(`.claude/worker-prompts.md`) for wave 3 and add one lane card per lane in
`.claude/lanes/`. Refresh `docs/known-issues.md` for the post-#680 nightly.

**Files owned:** `docs/adr/0010-*.md` (anchors + one amendment-log line only
— never decided text), `docs/known-issues.md`, `.claude/worker-prompts.md`,
`.claude/lanes/*.md`. Nothing under `src/` or `games/`.

**Keywords:** `Fixes #662`, `Refs #644 #645 #643 #578 #663 #582 #661 #497 #682 #676`.
