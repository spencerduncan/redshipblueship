# Lane F26 (wave 6): OoT's own fill overshoots progressive copies past the top tier (#726)

**Branch:** `claude/726-oot-progressive-overshoot`

## The bug

Under `RO_ITEM_POOL_PLENTIFUL`, OoT's own assumed fill grants extra progressive
copies through `Item::ApplyEffect` -> `SetUpgrade(x, CurrentUpgrade+1)`. Nothing
clamps that grant. The wallet's two-bit field therefore wraps to 0 on the
fourth copy, which lowers a capacity the fill then reasons with. PR #728's C1
legs observed this.

PR #728 added a clamp in `logic.cpp`, but it is on only between the combo
engine's `beginQuery` and `endQuery`.

## Task

1. **Verify on main** with a plentiful profile: the wallet reads 0 after the
   fourth copy in the simulated save.
2. **Fix at the root**, inside the single-exe guard. Either clamp at the top
   tier for OoT's own fill too, or make the round clamp unconditional and give
   the reasoning.
3. **Lock it** with a plentiful-profile row that is RED on main and GREEN after
   the fix.
4. **Describe a plentiful seed's world before and after**, using a same-seed
   comparison.

## Goldens

This changes OoT's worlds only under plentiful. The shipped default is not
plentiful, so all four goldens must stay untouched. If any golden moves, stop
and report before re-pinning.

## Scope

**Builds:** yes, both tiers.

**Files owned:**
- the clamp in `games/oot/soh/Enhancements/randomizer/logic.cpp` (inside
  `RSBS_SINGLE_EXECUTABLE`);
- the new lock row;
- the shared append-only files.

**PR:** "Clamp OoT's own progressive grants at the top tier so plentiful seeds
stop wrapping the wallet (#726)".

**Keywords:** `Fixes #726`.
