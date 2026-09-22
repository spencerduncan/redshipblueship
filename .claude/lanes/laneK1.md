# Lane K1 — increment 3: the combo-logic coordinator and engine surface, over stub engines

**Branch:** `claude/inc3-coordinator-core`

**Task:** Read `docs/solver-inventory.md` §4 (§4.1 the contract, §4.3 the
round loop and assumed fill, §4.4, §4.6, §4.7) and §6.1, ADR 0010 Decisions
1, 2, 4, 5 and its Amendments (O4 = composition), ADR 0002 (no raw game-local
id crosses outside a `SharedItem`), ADR 0011 (the frozen record), and epic
#645. Deliver, `src/common` only, **not wired into any production path**:
(1) a game-header-free engine-surface header (`src/common/combo_logic.h`)
as a registered vtable (`ComboLogicEngine`) plus
`Combo_Logic_RegisterEngine(GameId, const ComboLogicEngine*)` — a vtable so a
ROM-free test can register stub engines; write it precise enough that lanes
K2a/K2b cannot guess wrong; (2) the coordinator (`src/common/combo_logic.c`):
the union bag, two origin-keyed placement tables (memory only — never touch
`gComboCtx.reserved`), the alternating expansion to a fixpoint with MM's
snapshot/restore-per-round bracket, the single-bag assumed fill, GOAL
evaluation, `none` as the same path with the proof skipped, injected private
RNG; (3) ROM-free `redship`-tier locks over two synthetic stub engines:
fixpoint terminates and is order-independent, crossing exchange both
directions, the pair-level lock **with removal** (an OoT-origin item hosted
only in the MM stub makes GOAL provable; removing that host flips it
unprovable), `beat-either` permits an unbeatable half without bias, `none`
places without proof, same seed reproduces / different seed differs
(sensitivity control), a monotonicity-violating engine is detected not
looped on.

**Files owned:** new `src/common/combo_logic.{h,c}`, new
`src/common/tests/test_combo_logic.c`, the three shared append-only files,
the `src/common` CMake source list if needed. Nothing under `games/`.

**Keywords:** `Refs #645, refs #500`.
