# Lane C — #658: MM_GOAL's "Majora defeated" predicate

**Branch:** `claude/658-majora-defeated-predicate`

**Task:** Author a "Majora defeated" predicate for MM logic (a boss row, or
rows for distinct phases, in `CanKillEnemy`, or a dedicated
`CanDefeatMajora()`), authored from the vanilla fight's requirements in
2Ship's own condition vocabulary. Wire it where the goal is evaluated (the
`Regions/Moon.cpp` TODO) so `RR_MOON_MAJORAS_LAIR ∈ reachable` is no longer
the strongest derivable goal fact. Do not export it through
`GameExports_SingleExe.cpp` yet (no consumer needs it now). The fill must not
consume it yet — assert all four determinism digests stay byte-identical, no
re-pin expected.

**Files owned:** `games/mm/2s2h/Rando/Logic/Regions/Moon.cpp`,
`CanKillEnemy` or a new function appended at the END of
`games/mm/2s2h/Rando/Logic/Logic.h` (lane A1 concurrently edits
`CAN_USE_EXPLOSIVE` near `:289` of the same header — keep hunks far apart),
new tests.

**Keywords:** `Fixes #658`, `Refs #645`.
