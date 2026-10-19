# Tracker reachability audit — 2026-10-18

Read-only audit of the reachability logic behind the three check-tracker surfaces, for gaps
under the paired (OoT + MM) game. Anchors were verified at main `96b8ce9a`; the one later
local commit (`edf3cd04`) is docs-only and moves none of the lines cited. Nothing was built or
run. The findings are filed as #990 to #999 (2026-10-18) and the report lands with PR #989.
Method is in the appendix.

## Verdict

The core of each tracker is sound: neither tracker reads the other game's bytes, neither
runs against a detached logic save, the crossing is pinned out of every OoT entrance pool,
MM's arrival seed resolves to South Clock Town, both games' shared resources land in the
fields their logic reads, and OoT's foreign hosts are judged like ordinary checks. Eighty
such questions were asked and answered with code (section 4).

Two real defects and one design hole stand out:

1. **P1 — an arrival into OoT never recomputes Available Checks.** Items redeemed from
   Termina and every shared resource applied at arrival are invisible to the OoT Check
   Tracker until an unrelated trigger fires. Every paired file with Enable Available Checks
   on hits this on every return trip.
2. **P1 — the shared wallet tier breaks "Shuffle Child's Wallet".** An MM wallet applied into
   OoT raises the wallet tier without the possession flag, so about 115 shop, scrub and
   merchant checks show available while the game pins rupees at 0. The same path drains the
   shared rupee pool on every OoT visit until the child wallet is found. The second half is a
   gameplay defect, not a tracker one, and it does not need the tracker option on.
3. **P2 — no surface answers a cross-game reachability question.** The Combo Tracker
   projects status only; MM's native in-logic display lost its menu route in #458 U6 and now
   opens only from the console; crossing rows carry found state and nothing else.

The rest is test gaps (three rows would cover the whole area), one inherited live-save
mutation, and five latent items.

## 1. Findings

| # | Sev | Surface | Finding | Issue |
|---|-----|---------|---------|--------|
| R1 | P1 | OoT Check Tracker | Arrival into OoT never recalculates Available Checks | #990 |
| R2 | P1 | OoT Check Tracker + shared resources | Shared wallet tier vs Shuffle Child's Wallet: false availability and a rupee drain | #991 (tracker), #992 (rupee drain) |
| R3 | P2 | Combo Tracker, MM Check Tracker | No cross-game reachability projection; MM in-logic display is console-only since U6; crossing rows have no reachable mark | #993 |
| R4 | P2 | tests | No test drives either native tracker's reachability, on any file, let alone across an arrival | #994 |
| R5 | P3 | MM Check Tracker | The tracker's crawl zeroes and re-fires `RANDO_EVENTS` in the live save, unbracketed (inherited) | #995 |
| R6 | P3 | tests | The `mmReachable` determinism digest pins a retired closure; two comments are stale | #996 |
| R7 | P3 | OoT logic | Combo triforce hunt + OoT hunt mode GBK: OoT's graph grants the boss key at OoT's own threshold (latent) | #997 |
| R8 | P3 | tests | The crossing pin is unlocked in the one-way (spawn / warp song / owl) target pools | #998 |
| R9 | P3 | OoT Entrance Tracker | The crossing door is never listed or marked discovered (by design; cosmetic) | not filed (by design) |
| R10 | P3 | OoT logic (inherited) | `GetMinimumPrice` reads range 1 twice, so a reversed price range hides affordable checks | #999 |
| R11 | — | save | Moon-crash rollback of OoT's half is never reconciled into OoT check statuses: **unreachable today**, guard-dependent | not filed (unreachable) |

Already filed and not re-reported: #973 (Happy Mask Shop interior credited by OoT logic),
#971 (Combo Item Tracker Shared column reads the pool), #967, #963, #962, #855, #573.

### R1 — Arrival into OoT never recalculates Available Checks (P1)

**What happens.** The OoT Check Tracker's available set is event-driven. It is recomputed
by `RecalculateAvailableChecks` (`randomizer_check_tracker.cpp:2289`) or deferred through
the `recalculateAvailable` flag (`:279`, consumed at `:1235`). The flag is armed in exactly
two places: `CheckTrackerLoadGame` (`:637`, from `OnLoadGame`) and SoH's `SaveFile` with
`fullSave` (`:1111-1115`). A cross-game arrival dispatches neither: the frozen OoT save is
restored at `Play_ConsumeStartupEntrance` without `Save_LoadFile` (the #482 note at `:918`;
`GameExports_SingleExe.cpp:2793-2796`), and the crossing commit goes through
`RsbsSave_Save`, not SoH's `SaveFile` (`src/common/switch.cpp:264`; `context.h:1627-1653`).

On the arrival, `z_play.c:586-588` runs `OoT_ApplySharedResources()` then
`OoT_ConsumeSharedItems()`. The apply writes wallet tier, health, double defense, rupees,
magic and more straight into `gSaveContext` (`GameExports_SingleExe.cpp:3731-3915`). The
redeem gives each OoT-origin item found in Termina through `OoT_Item_Give(NULL, …)` /
`Randomizer_Item_Give(NULL, …)` (`ForeignItemsSingleExe.cpp:107/115`). That give does reach
`GameInteractor_ExecuteOnItemReceiveHooks` (`z_parameter.c:1833`), but neither handler
recalculates: `RandomizerOnItemReceiveHandler` returns unless a check is queued
(`hook_handlers.cpp:599-611`), and `CheckTrackerItemReceive` returns on
`!IsSaveLoaded()` because `OoT_gPlayState` is assigned only at `z_play.c:640`, after the
apply (`GameInteractor.cpp:47`). The remaining hooks do not cover it: `CheckTrackerTransition`
only updates area and shop state (`:657-680`), the flag-set handlers return under
`IS_RANDO` (`:763`, `:791`), the entrance hook recalculates only on a first discovery of a
shuffled entrance (`randomizer_entrance.c:830-845`).

The next recalculation reads the new state correctly (logic reads the live save:
`Hearts()` at `logic.cpp:1567`, `HasItem` via `CheckInventory`). Nothing schedules one.

**Scenario.** Enable Available Checks on. A child crosses into Termina, opens an MM chest
holding OoT's Bomb Bag, collects MM heart pieces, walks back through the Clock Tower. In
the Market the Bomb Bag is in the inventory and the heart capacity is higher. The tracker
still shows the set computed before departure: bomb-gated checks and heart-gated tricks
stay locked. It updates only on the next OoT check collected, a shop or merchant identify,
a first-discovered shuffled entrance, toggling the option, or a file reload.

**Related, not verified.** The critic noted two more triggers with the same root: the F10
hot-swap return leg goes through the same suspend/arrival path, and toggling Enable
Available Checks while MM is active calls `RecalculateAvailableChecks` directly
(`:2560-2564`), which returns early with no play state and does not arm the flag. Neither
was walked end to end.

**Fix direction.** Arm the deferred flag on every arrival into OoT. Add a check-tracker
entry point that sets `recalculateAvailable = true` (and refreshes area totals), called from
`z_play.c` right after `OoT_ConsumeSharedItems`. Calling `RecalculateAvailableChecks` there
directly does nothing, because `IsSaveLoaded` is false until `:640`. The flag is consumed at
the first `DrawElement` once the play state is live. The toggle callback should arm the
flag too instead of calling directly.

**Lock.** None exists. `TrackerArrivalRehydration` locks `areasSpoiled` only;
`IntPairedFirstCrossing` never enables the option or reads `ItemLocation::IsAvailable`.
A ROM-free row can drive the real apply/redeem with the arrival flag set and assert the
flag is armed (see R4).

### R2 — Shared wallet tier vs Shuffle Child's Wallet (P1)

**What happens.** SoH models the child wallet as a separate possession flag:
`RG_CHILD_WALLET` is `CheckRandoInf(RAND_INF_HAS_WALLET)`, `RG_ADULT_WALLET` is
`CurrentUpgrade(UPG_WALLET) >= 1` (`logic.cpp:420-427`). With Shuffle Child's Wallet on
(`settings.cpp:789`, default off) the flag is set only by the child-wallet give
(`savefile.cpp:460-462` skips it at creation; `item.cpp:301-305` resolves the first
progressive wallet to the child wallet). The per-frame handler pins rupees to 0 while the
flag is clear (`hook_handlers.cpp:2897-2899`) and hides the counter (`:2054-2057`). In SoH
alone, tier ≥ 1 without the flag cannot happen.

The paired apply produces exactly that state. `OoT_ApplySharedResources` raises the tier
from the pool with no regard for the flag (`GameExports_SingleExe.cpp:3733-3736`); the
wallet kind is always armed (`shared_resources.c:208-241`); MM's wallets feed the kind
(`ComboLogicEngineSingleExe.cpp:2117-2120`; MM harvest at `GameExports_SingleExe.cpp:4401`).
`OoT_SharedAmmoGrantBlocked` (`:3419-3433`) is the existing shuffled-ability guard and
covers only sticks, nuts and bombchus. `GetWalletCapacity()` tests the tycoon, giant and
adult rows before the child row (`location_access.cpp:116-127`), so it returns 200 or more,
and about 115 `LOCATION` rules under `location_access/` gate on
`GetCheckPrice() <= GetWalletCapacity()`.

**Two consequences.**

- *Tracker (needs Enable Available Checks and the option).* After any MM wallet is found
  before OoT's child wallet, the next recalculation marks every reachable shop, Business
  Scrub and merchant slot priced at or below the new capacity as available. None can be
  bought. Both item trackers correctly show no wallet (`randomizer_item_tracker.cpp:540`,
  `ItemAdapterSingleExe.cpp:431`), so the three trackers disagree.
- *Rupee drain (needs only the option; any tier, tracker off).* The rupees apply writes the
  pool into OoT (`:3769-3772`), the frame handler zeroes it, and the next consumable
  harvest reads 0 against the applied watermark and debits the pool
  (`shared_resources.c:282-317`). Rupees earned in Termina disappear on each OoT visit
  until the child wallet is found.

The fill does not share the mistake: each engine credits only its own origin's copies
(`combo_logic.h:1392-1397`), so this is tracker-and-apply only, not a generated-world
defect.

**Decision needed first.** Under one-game semantics, what does an MM wallet mean for
OoT's child-wallet ability? OoTMM, the reference for game choices, has no separate child
wallet; its shared wallet is the wallet. The two fixes are: (a) a shared wallet implies
possession, so the apply also sets `RAND_INF_HAS_WALLET` when the pool carries a wallet
(the OoTMM reading); or (b) the child wallet gates the shared kind, like the ammo bags,
and the rupee apply is suppressed while the flag is clear so the pool is not drained, with
`GetWalletCapacity` returning 0 without the child wallet when paired. Either way, add a
ROM-free row: child wallet shuffled, pool wallet tier 1 and rupees 50, OoT apply, then
assert the capacity the logic reads and that a harvest keeps the 50.

### R3 — No cross-game reachability projection (P2)

Three facts, each verified three ways:

- `ComboTrackerCheckStatus` has six values and `ComboTrackerCheckRow` has no availability
  field (`combo_tracker_view.h:127-135`, `:165-177`). Neither adapter reads
  `ItemLocation::IsAvailable` or MM's `checksInLogic`. The Combo Tracker draws status,
  name and found item only (`ComboTrackerWindow.cpp:179-230`), where SoH's own tracker
  draws a lock/unlock glyph (`randomizer_check_tracker.cpp:2103`).
- #458 U6 removed MM's four Combo > Windows rows (`SohMenuCombo.cpp:598-605`; ADR 0004
  `:806-830`, which calls those rows "the only menu route to MM's own item and check
  tracking"). MM's native Check Tracker, the only surface that computes MM in-logic
  (`CheckTracker.cpp:278-305`, default mode COLORED), and its Settings window now open
  only through the `gWindows.*` console keys (`TrackersGuiSingleExe.h:59`). Combo >
  Tracker Settings opens SoH's `gOpenWindows.CheckTrackerSettings`, a different key. OoT's
  native tracker keeps its Randomizer row but is `OoTActiveGated` (`SohGui.cpp:188`), so in
  Termina no menu-reachable surface shows availability for either game.
- Crossing rows carry `{host, item, found}` and nothing else
  (`combo_tracker_view.h:411-450`; `ComboTrackerWindow.cpp:546-570`). OoT's native tracker
  names a host's crossed item with " (MM)" only once the check is seen or collected
  (`:2115-2122`, `:2160-2162`), so an unchecked, available host is an ordinary unlocked
  row. "Which crossing hosts can I reach now?" has no answer; for MM hosts it cannot be
  assembled by hand either.

No ADR or epic promised an availability column in the Combo Tracker. The implied promise
is parity: #458's ruling is "one check tracker" and ADR 0004 says the unified windows
"supersede" MM's rows. The MM in-logic tint, on by default before U6, was lost from the
menu in that supersession. MM's `checksInLogic` is a TU-static map filled only while the
MM window draws (`CheckTracker.cpp:275`, `:307-308`), so nothing could project it today
even if the view had a field.

**Options.** (1) Add an availability bit to `ComboTrackerCheckRow` and
`ComboTrackerForeignRow`, filled per adapter in its own TU: OoT from
`ItemLocation::IsAvailable()` (valid only with Enable Available Checks on and after a
recalc), MM from an exported in-logic set, computed for the LIVE panel through a
bracketed crawl (R5) and, for the inactive game, from a bitset captured at the departure
freeze and stored beside the shadow, reported STALE. Draw SoH's glyph and an "N reachable"
count. (2) Restore a menu route to MM's native Check Tracker and its Settings window.
(3) Record in known-issues and ADR 0004 that MM in-logic tracking left the menu in U6.
This is the operator's call; (1) is the parity answer, (2) is the cheap one.

### R4 — No test drives either tracker's reachability (P2)

Nothing in the tree calls `RecalculateAvailableChecks`, reads `IsAvailable`, or drives
`RefreshChecksInLogic`. `TrackerArrivalRehydration` locks `areasSpoiled` and item-tracker
notes only. `ComboTrackerView` / `ComboTrackerWindow` / `ComboItemTrackerWindow` assert
status, freshness and skip writes. `MMTrackersGui` asserts registration and gating.
`MMComboLogicEngine` leg 7 runs an unbracketed tracker-style crawl on purpose and proves
only that the engine ignores the residue. The `IntPairedFirstCrossing` rows check the start
inventory, not tracker logic. The OoT engine always calls `ReachabilitySearch` with
`calculatingAvailableChecks = false` (`ComboLogicEngineOoT.cpp:718, 1506, 1992`), so the
tracker-mode branch (`fill.cpp:527`, `:545-548`, `:414-418`) has no coverage at all.

Three rows would close this:

1. **OoT tracker-mode availability across an arrival** (rando tier: needs a generated
   paired world; ROM-free). Load the paired creation save with Logic attached to it, assert a
   known sphere-0 check is available and a known gated check is not, run the real
   `OoT_ApplySharedResources` / `Combo_RedeemSharedItemsForGame(GAME_OOT, …)` with the
   arrival flag set, assert the deferred flag is armed, recalc, assert the gated check flips.
   Add #973's assertion (the Mask Shop checks are never available) and R2's (child wallet
   shuffled, pool tier 1: capacity reads what the ruling says).
2. **MM in-logic after an arrival redeem** (rando tier, the `MMComboLogicEngine` bring-up).
   Factor the predicate out of `RefreshChecksInLogic` into a gate-free callable. With
   `save.entrance = 0xD800`, assert a hookshot-gated check
   (`RC_CLOCK_TOWN_SOUTH_CHEST_LOWER`, `Central.cpp:241`) is out of logic, apply a pool
   carrying the hookshot tier through `MM_ApplySharedResources`, assert it is in logic and
   not obtained, mark it obtained, assert it drops out; assert `RANDO_EVENTS` is
   byte-identical before and after (once R5 is fixed).
3. **Fill-vs-tracker superset** (redship tier). On the paired creation save, run the
   coordinator's final round with nothing assumed and record each game's sphere-zero
   reached hosts; run each tracker's computation on the same save; assert tracker-available
   ⊇ fill-reached minus obtained, and print the other direction. This is the row that
   catches the #973 class.

Optional integration assertion: in the PFC drive, set `EnableAvailableChecks=1` before the
load and, on each OoT arrival leg, assert a recalculation frame counter advanced past the
arrival.

### R5 — MM tracker crawl mutates the live save (P3, inherited)

`RefreshChecksInLogic` calls `CrawlReachableRegions(gSaveContext.save.entrance)` directly
(`CheckTracker.cpp:294`), every 20 frames while the window is drawn in COLORED or HIDDEN
mode (COLORED is the default). The crawl zeroes `RANDO_EVENTS[0..RE_MAX)` and re-fires them
(`Logic.cpp:283-285`, `:381`). `RANDO_EVENTS` is
`gSaveContext.save.shipSaveInfo.rando.randoEvents` (`Rando.h:11`; `z64save.h:389`, "purely
for logic tracking"), inside the SaveContext the departure freeze copies whole and the
`.redsave` persists. `Logic.h:216-218` says a caller that must not perturb the live save
brackets the crawl; `ComputeReachableCheckSet` does, the tracker does not.

Nothing reads the field outside logic, the engine (inside its own bracket) and tests, so no
gameplay, hint, harvest or foreign-host decision changes. The cost is that MM save bytes
depend on a UI toggle, which confuses `.redsave` byte forensics and any byte-diff of the MM
shadow. The 2s2h tracker has done this since the import; the factoring into
`CrawlReachableRegions` (ADR 0010 increment 1.3) kept the behaviour. Fix: save and restore
the `RE_MAX` bytes around the tracker's crawl, or give the crawl a local event-count
array. No upstream report (operator rule).

### R6 — The `mmReachable` digest pins a retired closure (P3)

`mm_rando_gen_test.cpp:801-815` folds `ComputeReachableCheckSet()` into `mmReachableHash`
and says the closure "is now a GENERATION INPUT (the foreign-host gate filters candidates
by it)". That gate, `PlaceForeignItems`, is retired (`Foreign.cpp:612-617`); a paired
creation returns through `RunPairedSingleBagFill` (`OnFileCreate.cpp:248-268`) and nothing
in production calls the closure. `Foreign.cpp:46-48` still calls it "the reachability gate
on host candidates". The closure is neither the coordinator's paired proof (it never credits
MM-origin items the bag put on OoT hosts) nor the tracker's crawl (which does not simulate
collection). A green determinism row is therefore read as "the paired MM reachability did
not move", which it does not establish. Fix: drop the digest or replace it with the
coordinator's final-round reached-host sets for both games, and correct both comments.

### R7 — Combo triforce hunt with OoT's own hunt in GBK mode (P3, latent)

`Combo_GoalKeepsOwnHuntWin` returns true for the combo triforce hunt
(`combo_goal.c:123-126`), so the #768 rewrite at `playthrough.cpp:100-112` leaves an
authored GBK mode alone. `item_pool.cpp:337-339` then places the boss key at
`RC_TRIFORCE_COMPLETED`, whose rule is OoT's own threshold (`root.cpp:21`). In play the key
is granted only at the combo requirement (`triforce_hunt.c:184-188`;
`randomizer.cpp:4122-4131`), and that give ends the game. The fill's OoT round therefore
credits the boss-key door after OoT's half alone. Nothing is placed behind it today
(Ganondorf's Lair holds a static hint; the escape has no locations, `ganons_castle.cpp:767-771`),
so no seed and no tracker row is affected. Decide before any location lands there: generate
OoT's hunt as Win under an armed combo hunt, or compare against the combo requirement when
paired.

### R8 — Crossing pin unlocked in the one-way target pools (P3, test gap)

The Spawn and Warp Song target pools include `Interior` and `SpecialInterior`
(`entrance.cpp:1486-1495`). Only `BuildOneWayTargets` routing through
`GetShufflePoolEntrances` (`:693`, filter at `:679-685`) keeps the crossing out. `OoTEntrancePin`
probes the Interior pool only (`:1829-1832`); `RandoEntrancePin` never turns on spawn,
warp-song or owl-drop shuffle (`oot_entrance_pin_test.cpp:155-160`). An upstream merge that
restores upstream's direct `GetShuffleableEntrances` call would let a warp song target the
Mask Shop door, and `Combo_CheckEntranceSwitch` (`z_play.c:1178`) would cross from a warp
song, adult included, against the child-only ruling, with every row green. Fix: enable the
three one-way options in `RandoEntrancePin`, or add a raw-vs-filtered probe for the one-way
target lists.

### R9 — The Entrance Tracker never lists the crossing (P3, by design)

Every count and list comes from `entranceCtx->entranceOverrides`
(`randomizer_entrance_tracker.cpp:654-668`, `:700-704`); the pinned pair never gets a row,
and the departure returns before `Entrance_SetEntranceDiscovered`
(`randomizer_entrance.c:291-294` vs `:312`). The tracker says it tracks shuffled entrances
only, and the pin test calls the pair "vanilla in the entrance tracker" by design. One of
three verifiers refuted it as not a gap. Cosmetic; if wanted, a synthetic display-only row
"MK Mask Shop Entry → South Clock Town (Majora's Mask)" that never enters the override
table.

### R10 — Minimum-price fallback reads range 1 twice (P3, inherited)

`GetMinimumPrice`, case `RO_PRICE_RANGE`: `range2` is read from `priceSettings.range1`
(`location_access.cpp:78-82`), so the minimum is always range 1. The generator accepts
either order (`shops.cpp:125-129`), and the option text calls the sliders the first and
second part of the range. With range 1 above range 2, every unidentified shop, scrub or
merchant slot counts as costing range 1 in tracker mode (`:108-111`) and shows unavailable
until identified. Inherited from the initial import; not in known-issues. The paired game
adds only that a wallet from MM makes it bite on checks the player never saw. Fix in tree:
read `priceSettings.range2`.

### R11 — Moon-crash rollback and OoT check statuses (unreachable today)

`RestoreLastCommitForReset` replaces the OoT shadow in place and reports `ootMoved`
(`save.cpp:2005-2009`); `MM_Combo_ResetFromLastCommitOnMoonCrash` only logs it
(`GameExports_SingleExe.cpp:2967-2970`), and `OoT_Combo_ReconcileCheckStateToLoadedHalf` has
one caller, the load seam (`SaveManager.cpp:311`). So if OoT's half ever rolled back under a
running session, OoT's heap check statuses and the available set would follow the discarded
half. The critic raised this and two verifiers walked it; the third showed it cannot be
reached: the only way the shadow and the file differ is a skipped or failed crossing commit
(`switch.cpp:243-274`), and `StageCommit` bumps `commitGeneration` before the write
(`save.cpp:361`), so a failed write leaves the file one generation behind and the reset
refuses with `RSBS_RESET_STALE` (`:1934-1945`); the skip cases return `NO_SLOT` or
`NOT_WRITABLE`. `CheckMoonCrashKeepsOoTAcrossCrossing` locks the normal path. Recorded as a
latent trap: the reconciliation hook is missing, and only the generation guard stands
between it and a visible defect.

## 2. Refuted

- *The MM tracker ignores the logic mode and the combo rung, so No Logic worlds or rung
  "none" are tinted with glitchless reachability.* The rung is never authored and is frozen
  at BEATABLE (`foreign_items.c:59`, `:87`), so rung "none" is unreachable; a paired file
  defaults `RO_LOGIC` to Glitchless (`Foreign.cpp:183-188`) and the paired branch returns
  before the `RO_LOGIC` dispatch (`OnFileCreate.cpp:248-268`). Both trackers show
  glitchless reachability on every paired file, which is what the fill proved. Inherited
  behaviour, no paired-game effect.

## 3. Not covered

The completeness critic named four areas no finder walked end to end: the F10 hot-swap
return leg (same arrival path as R1, unconfirmed), toggling Enable Available Checks while
MM is active (same root as R1), the Combo Tracker's crossing-row found state after a
moon-crash restore (moot given R11), and the fix constraint recorded under R1.

## 4. Checked and sound

Eighty questions were answered "no gap" with evidence. The ones a reader is most likely to
ask:

**OoT Check Tracker**
- No recalculation can run while MM is active: departure NULLs `OoT_gPlayState`
  (`graph.c:503` from `OoT_Game_Suspend`) and `IsSaveLoaded` gates every path.
- No recalculation can overlap a coordinator round: generation is allowed only on file
  select with no save loaded (`SohMenuRandomizer.cpp:621`); `endQuery` re-attaches
  `Logic::mSaveContext` (`ComboLogicEngineOoT.cpp:1053-1067`, locked by `OoTLogicExport`
  claim 5).
- Tracker mode writes nothing into the live save: `Item::ApplyEffect` skips the item effect
  under `CalculatingAvailableChecks` (`item.cpp:50-56`), `Reset(false)` memsets `inLogic`
  only; `ValidateSphereZero` / `ValidateOtherEntrance` do not run (`fill.cpp:200-205`).
- A `.redsave` load or a crossing-commit reload always goes through `Save_LoadFile`, so
  `OnLoadGame` arms the flag.
- Age and time: `RR_ROOT` is seeded from the starting age, `PropagateTimeTravel` derives
  the other age; `IsChild`/`IsAdult` are evaluated per age-time bit, so an adult does not
  see child-only checks as available. After the return trip the arrival restores the
  frozen `dayTime` and Link is forced to child; the departure required `AtDay`, so the
  Market layer matches.
- A collected OoT host of an MM item recalculates (`hook_handlers.cpp:524-528`); the junk
  cover (`RG_BLUE_RUPEE`) carries `LOGIC_NONE` and credits nothing; the host is pushed
  like any reachable check (`fill.cpp:414-418`).
- Shared resources land where logic reads: hookshot in `INV_CONTENT`, Kokiri sword in the
  owned-equip bit, magic level, health capacity, double defense, triforce pieces; tiers are
  written as absolute levels, so out-of-order arrival is fine.
- The #973 class is exactly the Mask Shop region: every downstream mask event starts from
  `LOGIC_CAN_BORROW_MASKS`, and #948's night door rewrite leaves logic conservative.
- The forced #768 value is what the tracker reads (settings saved from `ctx` after the
  rewrite).
- `CalculatingAvailableChecks` staying true after a recalc cannot leak into a fill: every
  generation-side search calls `Reset(true)` first.
- The pinned crossing is never "undiscovered" for `ACProcessUndiscoveredExits`: the skip
  also requires `IsShuffled()` (`fill.cpp:190-193`).

**MM Check Tracker**
- Draws only while MM is active with a play state (`TrackersGuiSingleExe.cpp:427-429`;
  `CheckTracker.cpp:482-490`), so it never crawls OoT bytes.
- The arrival sets `save.entrance = 0xD800`, which resolves to `RR_CLOCK_TOWN_SOUTH`
  (the only non-one-way exit keyed on it, `Central.cpp:240`); the root's save-warp exit
  reaches it anyway. The frozen clock does not skew the crawl (the time model ignores the
  current clock; regions get all slices without clock shuffle).
- `MM_ApplySharedResources` and the arrival redeem write the fields the `Logic.h` guards
  read, and the 20-frame throttle refreshes on the first post-arrival frame (u32 wrap).
- The paired start `{RI_OCARINA, RI_SONG_TIME}` is granted into the save, so the root need
  not model it; the rooftop hosts stay shuffled.
- `RR_CLOCK_TOWER_INTERIOR` has no checks or events; nothing is reachable only through
  the tower; the Mask Salesman's two checks live in the root.
- An MM host of an OoT item is `shuffled = true` and judged by its own lambda; the foreign
  pickup flips `obtained` (`Foreign.cpp:697-745`).
- Tracker and fill share `CrawlReachableRegions` (`CheckTracker.cpp:294`;
  `ComboLogicEngineSingleExe.cpp:721`); the tracker's seed is a superset of the fill's.
- Grotto and enemy-drop grouping assigns each check to the right scene header.

**Combo Tracker**
- Reads MM's live save only while MM is active with a play state, the shadow otherwise;
  the OoT heap reads STALE while MM runs. No reachability is computed, so no mutation.
- A skip set from the Combo Tracker goes through `UpdateAllOrdering` →
  `RecalculateAllAreaTotals` and `RecalculateAvailableChecks` on the OoT side
  (`TrackerAdapterSingleExe.cpp:282-306`); MM's refresh ignores `skipped`.

**Fill vs tracker**
- The coordinator exchanges nothing across a closed crossing (`combo_logic.c:623-630`,
  `:839-846`); `crossingOpen` is a closure fact (`RegionTable(RR_MARKET_MASK_SHOP)->Child()`
  behind `IsChild && AtDay && CanOpenOverworldDoor(RG_MASK_SHOP_KEY)`), consistent with the
  child-only ruling and the tracker's region model.
- Each engine credits only its own origin's copies, so no shared tier is proved through
  the other game's copy that the apply would not deliver; the ocarina kind is never armed.
- The divergence that does exist is in the safe direction: the fill ignores MM-side
  shared tiers that physically arrive in OoT, so a player can be ahead of the proof, never
  behind it (R2 is the one exception, and it is apply-side).

**Entrances**
- The crossing pair is pinned out of Interior, InteriorReverse, Mixed and every one-way
  target pool (R8 is the lock gap, not a code gap); no override names `0x0530`/`0x01D1`;
  a refused crossing returns the unshuffled outside entrance.

## 5. Suggested order of work

1. R1 fix + its row (small, contained; every paired file with the option on).
2. R2 ruling (one-game semantics of the wallet), then the apply fix and its row. The rupee
   drain half is a gameplay defect worth its own issue even if the tracker half waits.
3. R4's three rows; R5's bracket and R6's digest cleanup can ride with row 2.
4. R3 is a design decision for the operator: availability bit in the Combo Tracker, a
   menu route back to MM's native tracker, or a known-issues entry.
5. R7, R8, R10 as quick wins in any lane that touches the area.

## Appendix — method

Workflow of 84 Opus agents over `96b8ce9a`: eight finders with distinct lenses (OoT
triggers and save pointer; OoT logic content vs the paired world; MM crawl; Combo
projection; fill-time vs tracker-time semantics; tests and locks; entrances and seeds;
inherited defects), each returning findings with file:line anchors plus a checked-no-gap
list; three adversarial verifiers per finding (refute by code, reproduce the scenario on
paper, severity and scope), majority vote; a completeness critic that re-read the refuted
item and raised R11, which was verified the same way. 24 candidates, 1 refuted, 80
checked-no-gap entries; the survivors above are merged by root cause (the arrival
staleness was found four ways, the wallet twice, the test gaps seven times). The two P1
anchors were re-read by hand before this document was written; R11's resolution is from
`save.cpp:355-365` and `:1934-1945`, read by hand.
