# Lane H2 (wave 5) — tracker and docs hygiene for wave 5

**Branch:** `claude/wave5-tracker-docs-hygiene`

**Task:** No local build; docs and tracker only; CI must be green. (1)
#645's body: record the 2026-09-26 multiplicity ruling with the operator's
bag-model notes (copies, plentiful surplus, more checks than items, traps
as per-game filler), the 2026-09-22 measured verdict, and the in-flight
wave-5 lanes (K4, K5, K8, W); tick O4, the two §6.3 measurements (PR #722)
and the wave-4 prerequisites (#701, #714, #715, #717, #722). (2) #708: ADR
0010 said the gen-budget reference is 55 ms, `gen_budget.c` pins 18 ms;
`git log -S` on both, PR #680's pre-squash commits and body, and the
reference host's own calibration line all say 18, so the ADR is corrected
by dated amendment. (3) `docs/solver-inventory.md` §6.2: P5 delivered
(#714/#715), P8 in flight (K8), P10 re-counted, P12 measured (#722), P14
annotated with how #722 read the bag (a superset of the general pass). ADR
0010 O9: dated amendment re-counting MM's trick vocabulary (86 declared,
20 reserved, 25 bound). (4) The `.claude/worker-prompts.md` header and
lane table for wave 5, one card per wave-5 lane here, and
`docs/known-issues.md` brought to `3b860bf3` (progress bar #707, MM mods
#704/#716, mask-shop pin #691, trick bindings #686/#696/#703/#713, the MM
Enhancements page #695).

**Files owned:** `docs/solver-inventory.md` (status annotations only),
`docs/adr/0010-*.md` (amendments only), `docs/known-issues.md`,
`.claude/worker-prompts.md`, `.claude/lanes/*.md`; issue bodies and
comments on agent-authored issues only.

**Keywords:** `Refs #645, refs #500`, `Fixes #708`.
