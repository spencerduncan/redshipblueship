# Lane A2 — #578 part 2: bind the first candidate tricks

**Branch:** `claude/578-mm-tricks-part2` (starts only after part 1/lane A1 merges)

**Task:** For each candidate trick with a concrete seam in
`games/mm/2s2h/Rando/Logic/Regions/*.cpp`, replace the `TODO` with a
`MM_TRICK(MMRT_*)`-guarded `||` disjunct, built on part 1's table, storage
and predicate. Tricks-off logic must not change: all four determinism digests
stay byte-identical to `main` (no re-pin expected here). Lock each binding
non-vacuously (trick off = unreachable through that edge; trick on =
reachable). Skip entrance-rando- or grotto-mapping-blocked seams and list
them with reasons. File one agent issue for keys that remain unbound
("#578 part 3").

**Files owned:** `games/mm/2s2h/Rando/Logic/Regions/*.cpp` (touched seams
only — not `Moon.cpp`, that is lane C's), new tests. Do not edit `Logic.h`,
`Tricks.*`, `Foreign.cpp` or `OptionsUiSingleExe.cpp` unless a binding is
impossible without it.

**Keywords:** `Refs #578 #645` (does not close #578; part 3 remains).
