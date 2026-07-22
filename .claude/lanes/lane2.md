You are Lane 2 of six on RedShipBlueShip Phase 3.1 (#492). Small, fast, and it unblocks part of Lane 1 and part of Lane 4.

Read #492, then #488.

## Your work

Close #488: foreign-item host selection trusts the fill-time `.shuffled` bit alone, so a pinned OoT item can land on a check with no runtime give-path and strand — invisible, unwinnable, indistinguishable from a missing item.

`Rando::Foreign::PlaceForeignItems` (`Foreign.cpp:117-161`) selects hosts by `RANDO_SAVE_CHECKS[id].shuffled` plus `RITYPE_JUNK`, excluding only `RCTYPE_SHOP` / `RCTYPE_TINGLE_SHOP`. Replace that inline predicate with an allowlist of check types that have a **game-guaranteed** setter, anchored on `RCTYPE_CHEST` — the chest flag is set by vanilla actor code independent of any rando hook.

Extract it as `Rando::Foreign::IsEligibleHost(RandoCheckId)` with an `extern "C" int MM_Rando_Foreign_IsEligibleHost(uint16_t)` bridge, placed next to the existing `MM_Rando_Foreign_RecordPickup` bridge at `Foreign.cpp:204-206`. That bridge is what makes the lock non-vacuous, and it is the whole reason to extract rather than edit in place.

Report how many checks survive the tightened predicate. That number sizes the foreign pool — it feeds **#495** (the rule-defined pool), not #493, which is capped at `RSBS_FOREIGN_PLACEMENT_CAP` regardless. Say it on #495 so the number has a consumer.

## Files you own

`games/mm/2s2h/Rando/Foreign.cpp` and `Foreign.h`, and you are **first writer** on `src/common/tests/test_foreign_items.c` — append your rows now; Lane 1 rebases onto you.

You also own `games/mm/2s2h/Rando/Spoiler/Apply.cpp` for the duration of #488 step 6. Lane 5 has been told to treat `Rando/Spoiler/*` as read-only, so it is yours; hand it back when you merge.

## Two cross-lane asks you must make before you start

Both are mandatory steps of #488 in files another lane owns. Neither lane will offer — ask on the issue.

- **`games/mm/2s2h/Rando/MiscBehavior/OnFileCreate.cpp:245`** (Lane 4). Step 5 throws a `runtime_error` inside `OnFileCreate`'s existing try on a host shortfall. Lane 4 rewrites `:112-120` and `:132-137` for #499 — different region, same file. Agree an order.
- **`games/mm/2s2h/mm_rando_gen_test.cpp:296-303`** (Lane 3). Your secondary lock replaces the `FAIL(12)` post-condition with `MM_Rando_Foreign_IsEligibleHost(p.mmCheckId) != 0`. Note `FAIL(11)` at `:276-285` asserts `placedCount == poolCount`, so a supply shortfall breaks that row whether or not you touch it. Lane 3 lands first (#487 is P0) — rebase onto it.

## Do not touch

`src/common/context.h`, `foreign_items.h`, `foreign_items.c` (Lane 1 — if your predicate needs a new accessor, ask on #493 rather than adding one), `ForeignItemsSingleExe.cpp` (Lane 1), `Rando/StaticData/*` (read-only; if the host class needs a new field on `RandoStaticCheck` at `StaticData.h:20-28`, that is #488's embedded decision and the issue recommends deferring it to its own ADR — raise it, do not just do it), `MiscBehavior/CheckQueue.cpp` (Lane 6; read `:40-52` to understand the give path — the foreign branch is nested inside `if (randoSaveCheck.eligible)` at `:39-53`).

## Non-negotiables

- Lock at the default `redship` label. The bar: drive `MM_Rando_Foreign_IsEligibleHost` directly and assert it returns 0 for a disallowed host, so the check never enters `candidates`. RED today because the predicate reads no eligibility bit, accepts sentinel items, and accepts every non-shop check type.
- Do **not** phrase the bar around `Combo_SetForeignPlacement`. That function already rejects `mmCheckId == 0`, untagged items, duplicate hosts and overflow — and it lives in `src/common/foreign_items.c`, which is Lane 1's. Making it the observable would force you into a forbidden file and push MM check-class semantics into a deliberately game-agnostic layer.
- There is no relocation mechanism in the tree. `PlaceForeignItems` picks from a candidate vector and erases the pick; nothing re-homes a placement. Do not assert "rejected **or relocated**" — you would be inviting yourself to invent one.

## Stop and report if

The tightened predicate cannot host 4 items, or the current 4 pinned placements have been landing on unsafe hosts all along. Both change the operator's playtest expectations and #495's sizing, and both are worth more than a quiet fix that widens the predicate to hit a number.
