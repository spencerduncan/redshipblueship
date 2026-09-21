# Lane T3 — #697: MM trick bindings part 3, the plain widenings only

**Branch:** `claude/578-mm-tricks-part3`

**Task:** Read #697 in full (its per-seam status table and its three KINDS
of remaining work), the merged part-1 and part-2 PRs (#686, #696),
`games/mm/2s2h/Rando/StaticData/TrickIds.h` / `Tricks.*`, and the
`MMTrickBindings` row's test. Deliver ONLY kind (1): plain widenings over
ALREADY-DECLARED `MMRT_*` keys, where no decision is owed. Each binding is a
`||` disjunct guarded by `MM_TRICK(...)`, item terms preserved as
conjuncts, two-way edges gated in both directions. Do NOT append keys (grows
`MMRT_MAX`, the frozen save array and the profile digest), do NOT tighten
any tricks-off edge, and do NOT bind the 20 reserved keys. Work region by
region; bind as many as justified from the key's tooltip plus the region
file's own TODO text; for each one skipped, say why. Tricks-off logic must
not change: prove it with an artifact comparison against a main binary, not
a green row. Extend the table-driven `MMTrickBindings` lock with a
red/green pair per new binding. Update #697's status table (confirm the
author is `spencerduncan`) as a comment.

**Files owned:** `games/mm/2s2h/Rando/Logic/Regions/*.cpp` (touched seams
only), the bindings test, `OptionsUiSingleExe.cpp` (`kBoundTricks` only),
shared append-only files.

**Keywords:** `Refs #697, refs #578`.
