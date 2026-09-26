# Lane K8 (wave 5) — ADR 0010 O6: monotonicity enforcement over both graphs

**Branch:** `claude/inc3-o6-monotonicity-tooling`

**Task:** Deliver O6's three mechanisms. **Starts only after lane K4's
multiplicity PR is merged on `origin/main`; otherwise STOP and report
`blocked`.** (1) The CI grow-check (rando tier): over BOTH real engines
through the coordinator's surface, from the shipped profile's start, assume
the bag one copy at a time in several deterministic orders (forward,
reverse, two seeded shuffles), expanding after each grant, and assert the
reached check set and region set never shrink and the final closure is
order-independent; tricks off and at least one tricks-on set. Red half:
inject one negated edge through a stub or test-only hook and observe the
row fail; leave no negation in either region file. (2) The static probe: a
`.github/scripts/` script (with `--self-test` and a planted violation) that
scans `location_access/**`, `Rando/Logic/Regions/**` and `Logic.h` for
negations of PLAYER-STATE terms (distinguished from setting/trick/option
negations, which are legal), reporting file:line; wire it as a CI step that
fails on a new hit. The audit (§1.2, §2.3) says the baseline is clean;
report any hit with a reading rather than silencing it. (3) The review
rule: one bullet in `.claude/worker-prompts.md` Standing conventions and
the docs page. Golden rows green, golden files untouched. Builds; both
tiers.

**Files owned:** a new test TU; a new `.github/scripts/` probe; a CI step in
`.github/workflows/` (append); a new `docs/` page; one Standing-conventions
bullet; the shared append-only files. No edits to `combo_logic.*` or the
engines beyond a justified read-only accessor.

**Keywords:** `Refs #645, refs #500`.
