You are Lane 5 of eight in the 2026-09-10 wave: **the ADR 0010 Decision 4 solver-inventory audit — the report open question O4 is waiting on.**

- **Branch:** `claude/solver-inventory-audit-o4`
- **Serves:** ADR 0010 (`docs/adr/0010-cross-game-logic-and-beatability.md`), Decision 4 "Informed reuse: the solvers stay authoritative; composition is the default posture" (`:454-506`) and still-open **O4** (`:683`): the combo-fill implementation shape — composition's coordinator contract, or unification if the audit makes that case. The increment-3 epic **#645** is gated on this report; #576 (OoTMM world-data port) cannot start before it.

## Scope

Write `docs/solver-inventory.md`: an inventory of both solvers as they exist at `origin/main`, precise enough that #645 can take the composition-vs-unification decision from it without re-deriving anything. Per engine:

- **OoT** — `games/oot/soh/Enhancements/randomizer/3drando/fill.cpp` (`CheckBeatable`, `IsBeatableWithout`), `location_access.*` (the static `areaTable`, capture-less `ConditionFn`, the four child/adult × day/night states), `logic.cpp` (`Logic::NewSaveContext`, `Reset`), the `RT_*` trick table.
- **MM** — `games/mm/2s2h/Rando/Logic/Logic.cpp` (`FindReachableRegions`, first-visit-wins per #585), `GlitchlessLogic.cpp` (the fill, the 10 s `GenerationTimeout`), `Logic.h` (`RandoRegionId`-keyed graph, u64 time slices — 45 today, #643 pins 46), the crawl PR #580 factored out, the absence of a per-trick vocabulary (#578).

For each: the exported query surface, which queries **mutate** state (MM's do, so a coordinator needs snapshot/restore), state representation, region-graph shape, beatability entry points, monotonicity hazards (negations, per §2.3), and where a `SharedItem`-shaped boundary could sit under ADR 0002's one-sanctioned-TU rule. Close with a recommendation on composition vs unification and the concrete coordinator contract composition would need — **recommend, do not decide**; the decision is #645's and lands as an ADR 0010 amendment closing O4.

## Owns

`docs/solver-inventory.md` only. No code, no ADR edits.

## Verify

Every file:line anchor checked at your branch's base. Link the PR from #645 and #644 when it opens (both list the audit as in flight). Docs-only: CI's format checks are the gate; no local build.
