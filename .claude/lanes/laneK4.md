# Lane K4 (wave 5) — increment 3: multiplicity in the assume contract, and the bag's surplus / filler / trap rules

**Branch:** `claude/inc3-multiplicity-contract`

**Task:** Implement the operator's 2026-09-26 ruling on #645: the
coordinator passes item multiplicity to the engines (assume once per copy,
as OoT's own assumed fill does) and both engines handle repeats correctly.
(1) A bag entry carries a count, or one entry per copy with a stable
delivery key — decide and state why, within ADR 0002 (only `SharedItem` +
host id + host origin cross). Fix and lock the engine hazards: OoT's
progressive rows (`SetUpgrade(x, CurrentUpgrade+1)`, no upper clamp — a
grant past the top tier walks into the neighbouring bitfield; observe the
lowering on main, then the clamp) and MM's counter gives (`++` on small
keys, stray fairies, skull tokens, triforce pieces: exactly once per copy,
never past their maxima). Say how per-round state guarantees an assumed
copy is never removed within a round. (2) After the GOAL proof holds (or
under rung `none`), reconcile remaining copies and hosts by a stated,
deterministic rule: more hosts than items -> each game's OWN junk fill,
traps included as that game's filler class, never a crossing; more items
than hosts (plentiful: OoT `RO_ITEM_POOL_PLENTIFUL`, MM
`RO_PLENTIFUL_ITEMS`) -> a stated drop order, preferring what each fill
does today; exact fit. Lock all three shapes over stub engines, with
same-seed-same-drops asserted. (3) Re-run `combo-logic-measure` with
multiplicity: is `beat-either` / `beat-both` provable at the 512-item bag,
rounds per placed item, roll-backs, the new per-attempt extrapolation
against the #582 floor; one comment on #645. If proof still fails, find the
next mechanism by measurement. (4) Rewrite the contract text in
`combo_logic.h` (repeats, counts, starting-state ownership, what `place`
may do, the surplus/filler rule); list every contract change in the PR.
(5) Not wired into production; the three golden rows stay green with the
golden files untouched. Builds; both tiers.

**Files owned:** `src/common/combo_logic.{h,c}`; the `assumeOwnItem`/`place`/
pool-export functions of `ComboLogicEngineOoT.cpp` and
`ComboLogicEngineSingleExe.cpp` (function granularity — K5 adds `classify`
to the same TUs); `test_combo_logic.c`; `test_combo_logic_measure.c`; the
shared append-only files.

**Keywords:** `Refs #645, refs #582, refs #500`.
