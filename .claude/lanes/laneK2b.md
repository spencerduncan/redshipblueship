# Lane K2b — increment 3: MM's solver export for the combo-logic coordinator

**Branch:** `claude/inc3-mm-logic-export`

**Task:** Implement lane K1's merged `src/common/combo_logic.h` contract
over MM's real solver. **Nothing you write may run in a production
path** — you register an engine, nobody calls the coordinator yet. Prove no
generated world changed with a real comparison against a main binary. New TU
under `games/mm/2s2h/Rando/` beside the foreign-items TU.
Snapshot/restore = whole `gSaveContext` (heap, as `Logic.cpp`'s closure does)
plus `MM_GameEvents_Queue` depth. Assume-own-item = `GiveItem(ConvertItem(ri))`
into the snapshotted live save. Expand = `CrawlReachableRegions` from the
South Clock Town arrival plus `EvaluateReachableChecks`. Crossing-open =
`RR_CLOCK_TOWER_INTERIOR` reachable. Goal-reached = lair reachable AND the
Majora-defeated predicate (PR #690). Place = `RANDO_SAVE_CHECKS` write for
MM-origin, junk cover + table entry for OoT-origin, placements re-applied
after restore exactly as `GlitchlessLogic.cpp` does. Check
`src/common/mm_stubs.c` for related stubs. Register at init. Read the
MERGED lane K1 PR and `docs/solver-inventory.md` §2, §4.1, §4.2, §4.4, §4.5
and ADR 0002 first; if K1's PR is not on `origin/main`, STOP and report
`blocked`.

**Locks (rando tier):** snapshot/restore byte-exact over the whole struct
and the queue depth; monotone under assume; identical queries agree; the
give path touches nothing outside the snapshot for the items exercised —
state plainly which `GiveItem`/`ConvertItem` branches were NOT exercised
(audit §6.3 lists this as unknown).

**Files owned:** the new TU, its test, `games/mm/CMakeLists.txt` source list
if needed (mind the `2ship_enh` elision class — confirm the registrar
actually links and runs with `check-registrar-elision.sh`), shared
append-only files.

**Keywords:** `Refs #645, refs #500`.
