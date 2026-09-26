# Lane K11 (wave 6) — the single-bag fill at the creation event: items leave origin pools (ADR 0010 D3/D5). PR HELD for the operator.

**Branch:** `claude/inc3-single-bag-fill-wiring`

**Prerequisites.** K9's PR ("compose the bag from the classification table")
and K10's PR ("persist cross-game placements") must both be on `origin/main`.
If either is missing, STOP and report `blocked`.

If K12 is also on `main`, support `triforce-hunt`. If it is not, refuse that
GOAL at creation and give a reason.

## Task

### 1. The seam (paired single-exe world)

- OoT's `Fill()` runs its restricted passes as it does today.
- The coordinator then replaces two things: the general advancement pass
  (`3drando/fill.cpp` `:1405-1414`) and the two post-fill overlay passes. The
  creation event runs the coordinator over K9's composed bag against both
  engines, under the frozen GOAL, rung and tricks.
- Each engine lands its placements through its own `place`.
- Each game's own junk pass fills its leftover hosts, traps included.
- Crossings persist through K10's write path.
- The `"combo"` spoiler reads the crossings from K10's storage.
- The identity publish then arms the MM shadow, in #680's order.
- Solo files stay byte-for-byte unchanged.

### 2. Attempt ladder and #582 budget

- A wall clock never climbs a rung.
- Exhaustion fails creation at file select, wholly: publish, then retract.
- The progress overlay shows the coordinator's phases.
- Measure one real creation end to end.

### 3. Removal (D3)

- Retire `kForeignPoolV1` and the MM pinned pool.
- State what `poolSize*` and `direction` mean under one bag, per ADR 0011.
- Keep the arrival compare-and-refuse machinery.

### 4. The D5 pair-level locks, over the real engines

- An OoT progression item hosted only in MM: the goal is provable.
- Remove that MM host: the goal becomes unprovable.
- The same two checks in the reverse direction.

### 5. Goldens

Re-pin all four goldens in ONE commit, "Re-pin goldens: single-bag fill
replaces the overlay passes". Summarize the moved fields in the PR. State the
save and format invalidation.

### 6. ADR 0010

Add a dated amendment.

## Delivery rule

Open the PR, wait for terminal-green CI, and STOP. The gate reports `held`
with "ready: operator hold". The operator decides.

**Builds:** yes; both tiers.

**Files owned:**
- the creation-event seam in `ForeignItemsSingleExe.cpp`;
- `fill.cpp`'s general pass;
- `OnFileCreate.cpp`'s paired branch;
- the `CreationProgressOverlay.cpp` phases;
- the pinned pools in `foreign_items.{h,c}`;
- `tests/golden/*`;
- an ADR 0010 amendment.

**Keywords:** `Refs #645, refs #500, refs #582`. Do not close the epic.
