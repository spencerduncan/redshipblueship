# Lane Q — #688: a real golden digest, so "the world did not change" is enforceable

**Branch:** `claude/688-golden-determinism-digest`

**Task:** Read #688 in full, `CMake/CheckSeedDeterminism.cmake`,
`CMake/CheckPairedAttemptDeterminism.cmake`, the digest writer
(`3drando/menu.cpp`'s `seed=`/`placementHash=`/`foreignOoTHash=`/
`comboSettingsHash=` block, `RSBS_SEED_DIGEST_OUT`), the MM paired digest
path, and the determinism rows in `CMake/SingleExecutable.cmake`. **The
defect:** every determinism row runs one seed twice and diffs the two runs
against each other — it detects nondeterminism only, never a moved world,
despite comments in `foreign_items.h`, `ForeignItemsSingleExe.cpp` and
`SingleExecutable.cmake` (~`:446`) claiming pins exist. Deliver: (1) golden
rows beside the self-diff rows (keep the self-diffs), stored expected
digests in-tree for the OoT seed digest, the paired MM world, and both
foreign-placement tables, on the shipped default profile plus at least one
profile exercising both crossing directions, failing with a field-by-field
diff; (2) measure (not assume) whether Windows and Linux produce
byte-identical digests for the same seed — run locally on Windows, read
Linux off a CI run of your branch, and if they differ, find why and report
it as a seed-portability finding; pin per-platform goldens only if they
cannot be made to agree; (3) a documented, reviewable re-pin procedure (one
command regenerates the goldens; the diff of the golden files IS the review
artifact) in a new `docs/` page plus one bullet in
`.claude/worker-prompts.md`; (4) correct the false "digests are pinned"
comments in the three files named above — do not edit ADRs (lane H owns
`docs/adr/` this wave), list in your report the ADR sentences that need a
dated amendment; (5) prove the oracle bites: a deliberate one-line placement
perturbation (then reverted) turns a golden row RED while the self-diff rows
stay GREEN — quote the observed output.

**Files owned:** the two `CMake/Check*Determinism.cmake` scripts (or new
siblings), new golden files under a new directory, the determinism rows in
`CMake/SingleExecutable.cmake` (append only), the three comment sites, the
new docs page, one bullet in `.claude/worker-prompts.md` under Standing
conventions, shared append-only files.

**Keywords:** `Fixes #688`, `Refs #645`.
