# Lane K3 — increment 3: measure the linked round and the assumed fill over the real solvers

**Branch:** `claude/inc3-linked-round-measurements`

**Task:** Read `docs/solver-inventory.md` §6.3, §4.3, §4.6; epic #645; #582
(the ~30 s creation floor, 90 s ceiling, host-calibrated budget); and the
three merged K PRs (coordinator, OoT export, MM export) — if any is not on
`origin/main`, STOP and report `blocked`. Deliver a measurement harness, NOT
production wiring: a rando-tier row (and a `redship --test` entry) that, on
the shipped default profile with MM never booted into play, registers both
real engines, builds the union bag (OoT's last general advancement pass plus
MM's whole shuffled pool, per audit §4.6 — add a minimal read-only accessor
in an engine's own export TU if it does not already expose its bag, and say
so), runs the coordinator, and prints: (1) ms per linked round
(min/median/max), alternations per round, rounds per placed item, total wall
time, and the multiple of the #582 30 s floor that represents on this host;
(2) convergence — items placed, roll-backs, dead-ends, whether GOAL became
provable, under the proved no-tricks rung and under `none`; (3) the same
again for two more seeds (variance), and once with `beat-either`. The row
asserts only sanity (terminates; leaves production state byte-identical;
same seed reproduces) — never a timing (PR #581 §2a: a wall clock never
decides anything). Do not replace `fill.cpp`'s general pass or
`OnFileCreate.cpp`'s paired branch. Prove no generated world changed, against
a main binary. Then post the numbers as ONE comment on #645 (confirm the
author is `spencerduncan` first) with a reading: does increment 3 fit the
floor as-is, and if not, where the time goes and the cheapest lever. State
what the measurement cannot tell (other hosts, non-default profiles, tricks
on).

**Files owned:** a new test TU (placed where both engines' registrations are
visible), minimal read-only accessors in the two export TUs if needed,
shared append-only files.

**Keywords:** `Refs #645, refs #582, refs #500`.
