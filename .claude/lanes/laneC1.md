# Lane C1 (wave 5) — #709: measure the whole rando tier on the Windows CI runner

**Branch:** `claude/709-windows-rando-tier-trial`

**Task:** The Windows gate step in `.github/workflows/generate-builds.yml`
runs only `--tests-regex '^Golden'`; the Linux leg runs the whole `rando`
tier under xvfb. Switch the Windows step to the whole `rando` label, push,
and MEASURE on the PR's CI, at least twice (re-run the job): wall time
added to the Windows job, any row that fails or flakes on the runner
(display, timeouts, archive staging), sccache hit rate. Decision rule set
by the orchestrator: land it if the job grows by under 10 minutes and every
row is green on both runs; otherwise revert in the same PR to a documented
allowlist (Golden rows plus the rows that proved stable) and report the
numbers. Update #709 with the measurements either way. No local build
required; the evidence is CI.

**Files owned:** `.github/workflows/generate-builds.yml` (the Windows gate
step).

**Keywords:** `Fixes #709` only if the full tier lands; `Refs #709`
otherwise.
