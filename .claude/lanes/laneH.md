# Lane H (wave 4) — tracker and docs hygiene after wave 3

**Branch:** `claude/wave4-tracker-docs-hygiene`

> Note: wave 3 also had a "lane H" (`claude/661-pin-mask-shop-entrance`,
> #661, delivered PR #691). This card replaces it for wave 4 — same letter,
> different lane, per the orchestrator's own convention.

**Task:** No local build; docs and tracker only; CI must be green. (1)
Stale-open issues #656, #657, #659, #667 (fixed by merged PR #689, whose
`Fixes #659 #656 #657 #667` line only closed #659) and #660 (fixed by merged
PR #680, whose `Fixes #583 #585 #660` line only closed #583): for each,
confirmed the fix on `origin/main` by file:line, confirmed the author is
`spencerduncan`, closed with a one-line comment naming the PR and evidence.
Epic #644: its remaining children (#661, #662) were both already closed
(PR #691, PR #684) — closed #644 with a summary. (2) Marked
`docs/solver-inventory.md` §6.2 rows P1, P2, P3, P4, P6, P9, P10, P11, P13,
P15 and the matching §6.4 items delivered with their PR numbers, appended in
place (no rewritten analysis, no new status column). Added a dated ADR 0010
amendment under §3.1/O9 with the measured trick-vocabulary numbers (86 keys
declared, 10 bound, 20 reserved, per `TrickIds.h`/`Tricks.cpp` and #697) —
amendment only, no rewrite. (3) Updated #645's body: ticked the delivered
prerequisites, corrected #578's status to parts 1-2 merged / part 3 (#697)
in flight, and added #688 (golden digest, lane Q) to the in-flight list.
(4) Rewrote this file's header and lane table for wave 4, and wrote one
condensed card per wave-4 lane under `.claude/lanes/`. Kept the
closing-keyword rule (repeat the keyword per issue) — it was missing from
Standing conventions and is exactly what made #656/#657/#659/#660/#667 go
stale; added it as its own bullet.

**Files owned:** `docs/solver-inventory.md` (status annotations only),
`docs/adr/0010-*.md` (amendment only), `.claude/worker-prompts.md`,
`.claude/lanes/*.md`.

**Keywords:** `Refs #645, refs #644, refs #500`.
