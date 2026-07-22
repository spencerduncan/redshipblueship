You are Lane 1 of six on RedShipBlueShip Phase 3.1 (#492): the **serialization spine**. Yours is the only lane that changes `.redsave` format, and the phase's longest pole.

Read #492, then #490, then #498, then #493.

## Step 1 — #490, merged before anything else

Defuse the `foreignPlacements` cap trap in `src/common/context.h`. **This is not a cap bump.** `grantCursors` and `sharedItemOverflowCount` are carved after `foreignPlacements` and the offset static_asserts are expressed *in terms of the cap*, so they follow a bump instead of catching it.

All four of #490's Work steps are in scope, including the two that are easy to skip: **step 3 is the actual lock** (extend `Test_SaveComboLegacyRecord` in `src/common/tests/test_save_roundtrip.c` with a v2 fixed-offset case using `SaveTestWriteCraftedSlot`, asserting bytes at literal offsets 672/676/736 land in `grantCursors[0]` and `sharedItemOverflowCount`), and step 4 is the explanatory comment at `src/common/tests/test_grant_sources.c:91-98`. Without step 3 this ships as a comment-and-assert change with no red-today lock.

**Why this is first, stated precisely** — do not let anyone "optimize" it away on discovering the weaker version: the 672/736 asserts do **not** guard your step 3 carve (that block lands after `sharedItemOverflowCount`, leaving those offsets untouched). What #490 actually buys is (a) the rewritten DO-NOT-BUMP comment at `context.h:161-167`, which is what tells your own step 3 to carve a second block rather than widen the cap, and (b) plain textual conflict avoidance, since both issues edit `context.h:161-185` and the assert chain at `:401-427`.

## Step 2 — the byte budget, before you carve anything

`reserved[264]` has **five** claimants, not one: your `foreignPlacementsOoT` block (#493 step 2, which would shrink it to 216), Lane 4's MM-profile digest (#497 step 7 *and* #499 step 4 — #499's Tier-1 lock does not compile until it lands), #498's combo-settings term, and #460's netplay grant fields.

You own `context.h`, so you own the budget. Publish the allocation on #490 or #493 and get Lane 4's digest size before you carve. One format version covering all known carves beats four sequential re-versions.

## Step 3 — the prep commit, and it must be ADDITIVE

Origin-dispatch the pool/name surface (`Combo_GetForeignItemName`, `Combo_GetForeignItemByName`) so each pool's definition stays in the single TU where its enum is in scope.

**This cannot be a breaking signature change.** Four call sites live in other lanes' files: `games/mm/2s2h/Rando/Foreign.cpp:180` (Lane 2), `games/mm/2s2h/Rando/Spoiler/Apply.cpp:93` (Lane 2, via #488), `games/mm/2s2h/mm_rando_gen_test.cpp:291` (Lane 3), `src/common/tests/test_foreign_items.c:95` (Lane 2). Add the origin-aware entry points and keep the existing symbols as thin wrappers, so the commit lands standalone. Lane 6 branches from it for the icon accessor.

Name collision is **not hypothetical**: `kForeignPoolV1` has the literal string `"Bomb Bag"` (`ForeignItemsSingleExe.cpp:61`) and MM's `RI_BOMB_BAG_20` row is also literally `"Bomb Bag"` (`StaticData/Items.cpp:37`). Origin-dispatch of the name→item inverse is mandatory, and the cross-pool collision assertion goes RED the moment an MM pool exists.

## Step 4 — write the #498 ADR before #493's code

Three decisions belong to #498 but get made inside #493 whether or not anyone writes them down: where combo settings live, **what gates generation** (move the `sourceIsRando`/`sharedRandoSettingsHash` stamp at `3drando/playthrough.cpp:116-118` above `Fill()` at `:81`, versus an explicit pre-generation opt-in), and one symmetric pool versus two. Record them as an ADR under #498. Making them implicitly inside an XL PR is how they become undiscoverable.

## Step 5 — #493, the reverse direction

Follow the issue's ordered Work section, **minus step 5's MM-side give** — Lane 6 owns `MM_AwardSharedItem` and creates `games/mm/2s2h/Rando/ForeignItemsSingleExe.cpp`. You add `kForeignPoolMMV1` to that TU after Lane 6 merges.

**Rebase onto Lane 2's #488 before step 7, not before step 3.** Steps 1–6 and 8–9 need nothing from `Foreign.cpp`. The dependency is pattern-mirroring for the OoT host allowlist, and it is soft — MM anchors on `RCTYPE_CHEST` over `Rando::StaticData::Checks`, while yours anchors on `RCTYPE_STANDARD`/`ACTOR_EN_BOX` plus the `GetFinalGIEntry` + `GiveItemEntryWithoutActor` funnel against a different type enum. Shared shape, no shared code. Do not idle the phase's longest pole on its smallest lane.

## Files you own

`src/common/context.h`, `context.cpp`, `foreign_items.h`, `foreign_items.c`, `src/common/tests/test_save_roundtrip.c`, `src/common/tests/test_grant_sources.c`, `games/oot/soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp`, `SeedContext.cpp`, `randomizer.cpp`, `randomizerTypes.h`, `3drando/playthrough.cpp`, `3drando/spoiler_log.cpp`, `3drando/menu.cpp`, and `MM_ConsumeSharedItems` in `games/mm/2s2h/GameExports_SingleExe.cpp`.

Read-only, no claim needed: `z_en_box.c` (#493 step 8 explicitly says **do not** try to recover the RC on the actor path — it is diagnosis, not a work site).

## Do not touch

`Rando/Foreign.cpp` (Lane 2 — expires to Lane 4 after #488 merges, not to you), `Rando/Spoiler/*` (Lane 2 for `Apply.cpp`, else read-only), `z_sram_NES.c` / `mm_stubs.c` / `mm_rando_gen_test.cpp` (Lane 3), `SohGui/*` and `MiscBehavior/OnFileCreate.cpp` (Lane 4), `TrackersGuiSingleExe.cpp` (Lane 5), `MiscBehavior/CheckQueue.cpp` and `DrawItem.cpp` (Lane 6).

## Non-negotiables

- **Append** `RG_*` sentinel enumerators after the last existing one, before `RG_MAX`. Inserting invalidates every existing spoiler and every pinned determinism digest — spoilers store raw ints.
- Mirror the forward-side seeding exactly (`Foreign.cpp:137-142`): `Ship_Hash` over `sharedRandoSeed`, `sharedRandoSettingsHash`, the per-file final seed, and a `":foreign-rev-v1"` tag, with a nonzero fallback when the hash is 0, feeding a **local** xorshift. Never `Random_Init`'s stream, or the fill's reproducibility moves.
- Locks are **per row**, not per lane. `ForeignItemGiveReverse` is the default `redship` label. The determinism-digest extension must carry `LABEL rando` with the display-tier timeout and environment, copying the existing `RandoGen` row.
- Non-vacuity differs per step. For #493: a test that pokes the placement table is vacuous — it must enter the production give path through a bridge, as `MM_Rando_Foreign_RecordPickup` does (`Foreign.cpp:204`). For #490 it is the opposite shape: literal byte offsets in a crafted 1024-byte Tier-1 record driven through the real `Load`, never `offsetof` compared against `offsetof`.
- Two placement tables means every clear / invalidate / serialize / spoiler site updates in **both** or they desynchronize — the #440 class. Enumerate the sites before editing any.
- #493 step 5 requires adding new TUs to `.github/clang-format-paths.txt`. It is an explicit allowlist that errors on stale entries, and `src/common/*` is deliberately absent from it.

## Stop and report if

The table shape needs an ADR 0002 amendment, the determinism digest changes shape, or `reserved[]` cannot fit the five claimants. Do not force it — getting a format carve wrong compiles clean and breaks saves the operator already holds.
