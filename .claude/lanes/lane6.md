You are Lane 6 of six on RedShipBlueShip Phase 3.1 (#492): **arrival and presentation** — what happens at the moment a foreign item is awarded, and what the player sees.

Read #492, then #502, then #494. #502 is carved out of #493 step 5; read that step for context but **not** the rest of #493.

## Why this lane exists

Two pieces of the cross-game item experience are unowned by every other lane, and both are cheap relative to their value. Neither depends on #493's `.redsave` carve, and both are **O(1) in pool size** — doing them does not get more expensive if the pool grows, and doing them first does not save the pool work any effort.

## Step 1 — #502, wire the MM-side award

`MM_AwardSharedItem` is still the Lane-A1 logging stub: it only `fprintf`s "Lane C wires the MM give" (`games/mm/2s2h/GameExports_SingleExe.cpp:1772-1776`). `MM_ConsumeSharedItems` already calls `Combo_RedeemSharedItemsForGame(GAME_MM, MM_AwardSharedItem, nullptr)` at `:1786-1788`, so the whole consumer walk is live and lands on a no-op.

Create `games/mm/2s2h/Rando/ForeignItemsSingleExe.cpp` under `RSBS_SINGLE_EXECUTABLE` with `extern "C" int MM_ForeignItem_Give(uint16_t riId)`, and replace the stub with the real give. Do **not** clear the entry — `Combo_RedeemSharedItemsForGame` sets `RSBS_SHARED_ITEM_REDEEMED`. MM Rando TUs are glob-collected into `2ship_rando`, which links WHOLE_ARCHIVE, so a new TU survives with no CMake edit.

**Lane 1 extends this same TU later** with `kForeignPoolMMV1` (the reverse-direction source pool). You create the file and the give; Lane 1 adds the pool after you merge. Keep the file small and the boundary obvious.

**The NULL-play hazard is real and is the main risk in this step.** MM's redemption point runs *before* `MM_gPlayState` is assigned: `MM_ConsumeSharedItems()` is called at `z_play.c:2406`, while `MM_gPlayState = this` happens at `:2468`. `Rando::GiveItem`'s default branch calls `MM_Item_Give(MM_gPlayState, ...)`, and `Item_GiveImpl` (`z_parameter.c:4136`) has only partial 2S2H nullptr guards — `Inventory_IncrementSkullTokenCount(play->sceneId)`, `MM_Health_ChangeBy(play, ...)` and `Magic_Add(play, ...)` are unguarded derefs. `Rando::GrantStartingItems` (`StartingItems.cpp:66-68`) proves a NULL-play give works for *some* items, not for any item.

So: either trace each candidate `RI_*` for NULL-play safety and restrict to the safe set, or move MM redemption to the gameplay-gated frame-tick safe point that `src/common/shared_items.h:52-64` already defines and nothing wires. Pick one deliberately and say which on the issue — a badly chosen item crashes the arrival path.

## Step 2 — #494, foreign-item presentation

Correct the framing first: **names already work.** The pickup textbox interpolates the item's real display name (`Rando/MiscBehavior/CheckQueue.cpp:70-72`) and both spoiler surfaces name items individually (`Rando/Spoiler/Generate.cpp:41-42`, `:63-74`). What is a shared `RI_RUPEE_HUGE` stand-in is only the **textbox icon** and the **get-item model**, and both are pool-size-invariant.

**Slice 6 is a correctness item, not polish, and it is the one to prioritise:** no draw path consults `Rando::Foreign::IsForeignCheck`, so a foreign check currently sits in the MM world *disguised as the junk item the fill left there*. The player has no way to know a check holds a cross-game item until they collect it. That is true identically at 4 items and at 32.

The other half of the issue is the OoT side: `OoT_AwardSharedItem` (`games/oot/soh/GameExports_SingleExe.cpp:1035-1039`) awards silently — a foreign item arrives in OoT with no notification at all.

Work the tiers in order of blocking factor. The icon tier needs an answer to whether the receiving game can resolve the origin game's assets at all (`oot.o2r` vs `mm.o2r` mounting) — establish that before promising a real icon, and fall back to a distinct generic foreign marker if it turns out both archives are not simultaneously addressable. A real 3D model is the only tier that scales per-item, so it is explicitly out of scope.

Closing slice 4 also unparks #427 item 1, which has been waiting on exactly this work ("When Lane C1 touches presentation, replace with an explicit bridge" — the `Notification::Emit` cross-bind).

## Files you own

`games/mm/2s2h/Rando/MiscBehavior/CheckQueue.cpp`, `games/mm/2s2h/Rando/DrawItem.cpp`, `games/mm/2s2h/Rando/ActorBehavior/*`, the new `games/mm/2s2h/Rando/ForeignItemsSingleExe.cpp`, `MM_AwardSharedItem` in `games/mm/2s2h/GameExports_SingleExe.cpp`, and `OoT_AwardSharedItem` in `games/oot/soh/GameExports_SingleExe.cpp`.

## Do not touch

`src/common/foreign_items.h` / `.c` and `context.h` (Lane 1), `Rando/Foreign.cpp` (Lane 2 — you may call `IsForeignCheck`, not edit it), `z_sram_NES.c` / `mm_rando_gen_test.cpp` (Lane 3), `SohGui/*` and `OnFileCreate.cpp` (Lane 4), `TrackersGuiSingleExe.cpp` / `Rando/Spoiler/*` (Lane 5 / Lane 2).

## Your one upstream dependency

Tier 2a of #494 — a `Combo_GetForeignItemIconName`-style accessor — lands in `src/common/foreign_items.h` and `ForeignItemsSingleExe.cpp` (OoT), both Lane 1's. **Wait for Lane 1's prep commit** (the additive origin-dispatch of the pool/name surface) and branch from it, then ask Lane 1 to add the icon accessor alongside. Everything else in this lane needs nothing from Lane 1.

## Non-negotiables

- Locks at the default `redship` label. Presentation is display-tier and the honest lock is on the **resolution surface**, not the pixels: assert a non-null icon/name descriptor for every pool entry, RED while the icon is hardcoded to `RI_RUPEE_HUGE`. Do not claim a visual result CI did not prove — the operator verifies appearance.
- The step-1 give must be locked non-vacuously: plant the shared item through the real `Combo_RecordSharedItem` and drive the real `MM_AwardSharedItem`, then assert exactly one award and the entry marked `RSBS_SHARED_ITEM_REDEEMED`, and that a second redeem awards zero. Calling `Rando::GiveItem` directly from a test is vacuous.
- Any edit to a vendored 2S2H TU under `Enhancements/` or `ActorBehavior/` needs an `RSBS_SINGLE_EXECUTABLE` guard, or it is an upstream-sync landmine.

## Stop and report if

The archive story rules out cross-game asset resolution entirely. That caps presentation at a generic-but-distinct marker permanently, which is a product decision the operator should make rather than something to work around — and it would also settle how far #495's pool can grow before the experience degrades.
