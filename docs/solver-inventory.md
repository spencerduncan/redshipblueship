# Solver inventory audit — ADR 0010 Decision 4 (open question O4)

- Status: **audit report**, read-only research; every anchor is `path:line` at
  `origin/main` = `c8947177` (2026-09-10). No code was changed and nothing was
  built.
- For: #500 (Phase 3.2 tracker). Discharges ADR 0010 Decision 4's explicit
  first step of the increment-3 epic — "a solver-inventory audit: graph
  representation, trick gating, fill algorithm, evaluation entry points,
  save/state coupling — before any line of the combo pass is written"
  (`docs/adr/0010-cross-game-logic-and-beatability.md:462-469`) — and answers
  **O4** (`:683`).
- Reads: ADR 0010, ADR 0002, ADR 0009 (D2 + amendment), ADR 0011 (D3.5, D4.1);
  PRs #580 and #581; issues #578, #582, #585. Where one of those documents
  makes a claim about a solver, Appendix A says whether the claim holds at
  `c8947177`.
- Method: the code was read, not the comments about the code. Counts come from
  `grep -c` over enumerator prefixes and macro invocations; they are stated as
  approximate where an enumerator like `RR_MAX` inflates them by one.

## 0. Summary

| Dimension | OoT (SoH 3drando, `games/oot/soh/Enhancements/randomizer/`) | MM (2Ship Rando, `games/mm/2s2h/Rando/`) | Redship layer (`src/common/` + the two `*SingleExe.cpp` TUs) |
|---|---|---|---|
| Graph storage | `std::array<Region, RR_MAX> areaTable` (`location_access.cpp:573`), rebuilt from 35 init functions on every fill attempt (`location_access.cpp:958-1022`; `3drando/fill.cpp:1225`) | `std::map<RandoRegionId, RandoRegion> Regions` (`Logic/Logic.cpp:13`), populated once at boot by `RegisterShipInitFunc` registrars in 17 files under `Logic/Regions/` plus the `RR_MAX` root (`Logic.cpp:334-354`) | none — no cross-game graph exists; the crossing is a runtime entrance link (`src/common/entrance.h:27-49`), not an edge in either graph |
| Edge kinds | `exits` (list of `Entrance`, condition lambda; shuffleable), `locations` (`LocationAccess`), `events` (`EventAccess`) — `location_access.h:139-142` | `connections` (intra-scene, keyed by region), `exits` (keyed by entrance id, resolved to a region through `GetRegionIdFromEntrance`), `checks`, `events`, `timeStayRestrictions` — `Logic/Logic.h:238-252` | — |
| Condition form | capture-less `bool(*)()` (`ConditionFn`, `location_access.h:16`) reading `logic->` (`Rando::Logic`) and `ctx->` (options/tricks) | capture-less `std::function<bool()>` reading the **live** `gSaveContext` through macros (`Logic.h:272-334`) and the thread-local `gCurrentRegionTime` (`Logic.cpp:16`) | — |
| World-state dimensions | region × {childDay, childNight, adultDay, adultNight} (`location_access.h:149-152`) | region × u64 time-slice mask (45 slices) × `canStayOverTime` (`Logic.h:99-102`); no age; Link's form is inventory (`CAN_BE_*`, `Logic.h:278-284`) | — |
| Reachability | worklist fixpoint `ReachabilitySearch` (`fill.cpp:513-559`) with immediate recursion into newly opened regions; monotone bit-setting | three crawls: recursive first-visit `FindReachableRegions` (`Logic.cpp:95-139`, used by the fill; #585), join-correct `CrawlReachableRegions` (`:147-265`), and the give-and-recrawl closure `ComputeReachableCheckSet` (`:283-331`) | `ComputeReachableCheckSet` is consumed by the forward placement pass (`Rando/Foreign.cpp:594`); the reverse pass has **no** reachability term |
| Beatability predicate | `CheckBeatable` (`fill.cpp:589-604`): `RG_TRIFORCE` reached | **none**; `RR_MOON_MAJORAS_LAIR ∈ reachable` is derivable from the crawl (`Regions/Moon.cpp:115`) but no "Majora defeated" predicate exists (`Moon.cpp:68` TODO; no boss row in `CanKillEnemy`, `Logic.h:692-850`) | none (pair beatability is vacuous while crossings are duplicate overlays — ADR 0010 §2) |
| Fill | assumed (reverse) fill `AssumedFill` (`fill.cpp:835-936`) in restricted-pool passes then all-locations, `FastFill` remainder (`:1413-1414`); 5 outer attempts (`:1219`) | forward fill `ApplyGlitchlessLogicToSaveContext` (`Logic/GlitchlessLogic.cpp:24-285`): place-as-reached with weighted junk-swap backtracking; terminates only when every shuffled check was reached (all-locations-reachable by construction) | attempt ladder (`MiscBehavior/OnFileCreate.cpp:300-400`, 10 rungs, timeout is not a rung); two post-fill overlay passes (`Foreign.cpp:493-682`; `ForeignItemsSingleExe.cpp:400-541` OoT-side) |
| Fill mutates | solver-private state only: a **detached** `SaveContext` (`logic.cpp:2310-2315`) plus `areaTable` bits and `ItemLocation` fields | the **live** `gSaveContext`, bracketed by `memcpy` snapshot/restore (`GlitchlessLogic.cpp:37-38`, `:78`, `:282`; `Logic.cpp:289-290`, `:326`) | `gComboCtx` placement tables only |
| Tricks | 266 `RT_*` keys (`randomizerTypes.h:4142-4416`), `OPT_TRICK` rows (`settings.cpp:113`, `:1355+`), read via `ctx->GetTrickOption` (`SeedContext.cpp:548`); 425 consults under `location_access/` + ~30 in `logic.cpp` helpers | none — `RO_LOGIC` only (`Types.h:2933-2936`); 16 trick-shaped `TODO` seams in `Regions/`; the Powder-Keg trick is compiled in unconditionally (`Logic.h:289-291`; #578 finding (a)) | trick sets cross the boundary only as digest input (`Rando/Foreign.cpp:183-232`; `3drando/playthrough.cpp:59-61`) |
| RNG | PCG-XSH-RR in a solver-private `rando_state` (`3drando/random.cpp:3-10`; `ShipUtils.cpp:133-175`), seeded twice: `seed`, then `Hash(seed ‖ settingsStr)` (`playthrough.cpp:31`, `:83-84`) | the same PCG in the process-global `Ship_Random` state (`2s2h/ShipUtils.cpp:309-344`), seeded per ladder attempt (`OnFileCreate.cpp:342`; `Foreign.cpp:317-333`); crawls consume none | both placement passes use a local xorshift32 seeded from identity (`Foreign.cpp:348-357`, `:633-635`; OoT `ForeignItemsSingleExe.cpp:364-373`, `:468-472`) |
| Only non-determinism | none found in the generation path | the fill's 10 s wall-clock abort (`GlitchlessLogic.cpp:30`, `:87-89`) → `GenerationTimeout` (`Logic.h:207-209`) | — |
| Generation entry | `Playthrough_Init` (`playthrough.cpp:27-186`) via `GenerateRandomizer` (`3drando/menu.cpp:621`) from the GUI thread (`randomizer.cpp:3541`) or `Rando_HeadlessSeedTest` (`menu.cpp:35-69`) | `Rando::MiscBehavior::OnFileCreate` (`OnFileCreate.cpp:24-476`) on the `OnSaveInit` hook — file select, the arrival gate (`GameExports_SingleExe.cpp:3061`, dispatch ≈`:3288`), or the headless harness (`mm_rando_gen_test.cpp:1044`) | creation stamp + freeze after OoT's `Fill()` and spoiler (`playthrough.cpp:125-163`); MM generated later at arrival (increment 2 moves it) |
| Spoiler | one JSON per OoT world, `Randomizer/<hash-icons>.json` (`3drando/spoiler_log.cpp:340-403`) | one JSON per MM world, `randomizer/RSBSPAIR<seed>.json` (`OnFileCreate.cpp:427-428`; `Spoiler/Generate.cpp:15-116`) | two artifacts today, each stamped with the pairing identity; #564 V23's one-artifact spoiler does not exist yet |
| Size | `location_access/` 12,427 LOC (2,661 `LOCATION(`, 2,394 `Entrance(`, 400 `EventAccess(`); `3drando/` 8,954 (`fill.cpp` 1,459); `logic.cpp` 2,761; ~1,013 regions, ~2,527 checks, ~304 items, 321 events, 37 areas | `Logic/Regions/` 5,878 LOC (2,422 `CHECK(`, 285 `EXIT(`, 419 `CONNECTION(`, 117 `EVENT(`, 37 `STAY(`); `Logic/` 2,042; give path 1,524 (`GiveItem.cpp` 379, `RemoveItem.cpp` 472, `ConvertItem.cpp` 673); 315 regions, ~2,257 checks, ~236 items, 82 events, 47 options | `foreign_items.h/.c` 896 + 820; `shared_items.h/.c` 274 + 316; OoT pool TU 648; MM pool TU 927; `Foreign.cpp` 873 |

---

## 1. The OoT solver (Ship of Harkinian's 3drando)

### 1.1 Data model

**Regions.** `class Region` (`location_access.h:126-241`) carries a name, a
`SceneID`, a `timePass` flag, a set of `RandomizerArea` (hint areas), and
three edge lists: `events` (`std::vector<EventAccess>`), `locations`
(`std::vector<LocationAccess>`), and `exits` (`std::list<Rando::Entrance>` — a
list, not a vector, because the entrance shuffler holds raw pointers into it,
`:143-147`). Reachability state is four booleans per region — `childDay`,
`childNight`, `adultDay`, `adultNight` (`:149-152`) — plus `addedToPool`
(`:153`). The whole table is `std::array<Region, RR_MAX> areaTable`
(`location_access.cpp:573`), indexed by the `RandomizerRegion` enum
(`randomizerTypes.h:523-1573`, ~1,013 values; the MQ dungeon variants are
separate regions). `RegionTable_Init()` (`location_access.cpp:958-1022`)
`fill`s the array with an "Invalid Region" placeholder (`:969`), calls the 35
per-area init functions (`:971-1007`), and links every location to its parent
region and every exit to its connected region's `entrances` back-list
(`:1010-1021`). It runs at the top of **every** fill attempt (`fill.cpp:1225`),
so the graph is rebuilt per attempt and the table is reset per attempt.

**Conditions.** All three edge kinds hold a capture-less `ConditionFn`
(`typedef bool (*ConditionFn)()`, `location_access.h:16`) written with the
`LOCATION(check, condition)` macro (`:55-57`, which also stringifies the
condition for the spoiler/tracker). Lambdas reach the solver through two file
globals: `Rando::Context* ctx` and `std::shared_ptr<Rando::Logic> logic`
(`:19-20`, defined `location_access.cpp:955-956`, bound in `RegionTable_Init`
`:960-961`). The same file's own comment says of this: "I hate this but every
alternative I can think of right now is worse" (`:18`).

**Logic mode is applied at the wrapper, not in the lambda.** Each wrapper
returns `true` outright unless `RSK_LOGIC_RULES` is `RO_LOGIC_GLITCHLESS`:
`EventAccess::ConditionsMet` (`location_access.h:30-36`),
`LocationAccess::GetConditionsMet` (`:71-77`), `Entrance::GetConditionsMet`
(`entrance.cpp:24-30`). Under No Logic every guard is open — and the fill
short-circuits to `FastFill` anyway (`fill.cpp:854-857`).

**Events** are `LogicVal` enumerators (`randomizerTypes.h:33-371`, 321 of
them) stored as `bool inLogic[LOGIC_MAX]` inside `Rando::Logic`
(`logic.h:172`; `Get`/`Set` at `logic.cpp:2527-2533`). An `EventAccess` fires
once (`Region::UpdateEvents` skips already-set events, `location_access.cpp:455-457`)
and is evaluated under each of the region's held age/time states (`:459-465`).
`grottoEvents` (`:17`, `:962-966`) is a shared event list reused by grotto
regions.

**Locations and items.** `Rando::ItemLocation` (`item_location.h:9-82`) is the
per-check fill record: `placedItem`, `delayedItem`, `addedToPool`, hint
flags, `wothCandidate`/`barrenCandidate`, the location's hint `areas`, price,
and tracker status. The table is `std::array<ItemLocation, RC_MAX>` inside the
`Rando::Context` singleton (`SeedContext.h:185`; ~2,527 checks). Items are
`RandomizerGet` (`randomizerTypes.h:4419-4731`, ~304) resolved through
`Rando::StaticData::RetrieveItem`; the classification the fill uses is
`Item::IsAdvancement()` / `IsMajorItem()` / `GetCategory()` (`item.h:47`, `:60`, `:63`).
The working item pool is a file global, `std::vector<RandomizerGet> itemPool`
(`3drando/item_pool.cpp:13`), built by `GenerateItemPool` (`:149`).

**Age and time of day** are not properties of the lambda's inputs; they are
four flags on `Logic` — `IsChild`, `IsAdult`, `AtDay`, `AtNight`
(`logic.h:27-32`) — that the *caller* sets before evaluating a condition.
`CheckConditionAtAgeTime(bool& age, bool& time)` zeroes all four, sets the two
references passed in, and evaluates (`location_access.cpp:19-42`;
`entrance.cpp:87-98`). So a lambda always sees exactly one (age, time) pair,
and a region's four bits are the OR over the parent's four bits AND the
condition under each.

**Entrances.** `Rando::Entrance` (`entrance.h:43-106`) holds parent/connected
region keys, the condition, a type (`EntranceType`, `:14-41`), and the
shuffler's `target`/`reverse`/`assumed`/`replacement` pointers (`:93-96`). The
static entrance table lives in `entrance.cpp` (e.g. the Happy Mask Shop
interior pair at `:300-301`, type `Interior`); `EntranceShuffler::ShuffleAllEntrances`
(`:1199`) rewires `exits` in place and validates with `ValidateEntrances`
(`:814`; `fill.cpp:607-656`).

**Hint areas.** `RandomizerArea` (`randomizerTypes.h:382-421`, 37 values incl.
`RA_NONE`/`RA_LINKS_POCKET`); `SetAreas` (`fill.cpp:679-705`) propagates areas
from regions into `ItemLocation::areas` after entrance shuffle.

### 1.2 The reachability algorithm

`ReachabilitySearch(targetLocations, ignore, calculatingAvailableChecks, startingRegion)`
(`fill.cpp:513-559`) is the one query. Its shape:

1. `ResetLogic` (`:315-329`): sets the ER sphere-zero validation flags to
   "already validated", optionally applies the starting inventory, then
   `Regions::AccessReset()` (`location_access.cpp:1079-1098` — every region's
   four bits and `addedToPool` cleared, every exit's pool flag cleared, and the
   starting age's Day bit set on `RR_ROOT`) and `ctx->LocationReset()`.
2. `do { for each region in gals.regionPool: ProcessRegion } while (gals.logicUpdated)`
   (`:541-546`). `regionPool` starts as `{RR_ROOT}` and grows as exits open
   (`:201-204`).
3. `ProcessRegion` (`:456-510`): `ApplyTimePass` (a `timePass` region that has
   any access for an age grants both Day and Night for that age **and writes
   the same bits onto `RR_ROOT`**, `location_access.cpp:431-448` — this is how
   "you can wait for night anywhere you can wait" propagates globally);
   `UpdateEvents`; `ProcessExits` (`:177-228` — for each exit, `UpdateToDAccess`
   `:83-110` sets each of the four target bits the parent holds and the exit
   condition passes for, then **recurses immediately** into the newly-touched
   region `:207`); `PropagateTimeTravel` (`:64-80`, the Temple of Time/Door of
   Time age crossover); then every location through `AddCheckToLogic`.
4. `AddCheckToLogic` (`:391-454`): `LocationAccess::ConditionsMet` ORs the four
   held states (`location_access.cpp:44-58`); on success the location is
   marked `addedToPool`, an **empty** location goes to `accessibleLocations`
   (`:409-411`), and a **placed** item's effect is applied to `Logic`
   (`ApplyOrStoreItem` → `ItemLocation::ApplyPlacedItemEffect`, `:381-388`,
   `:437-439`) with `logicUpdated = true` — i.e. the search simulates
   collecting whatever the fill has already placed. Reaching `RG_TRIFORCE` with
   `stopOnBeatable` returns early (`:445-449`).
5. The result is `accessibleLocations` filtered to empties within
   `targetLocations` (`:547-558`), unless `calculatingAvailableChecks` (the
   check tracker's mode, `randomizer_check_tracker.cpp:2185`, which also skips
   undiscovered shuffled exits, `:182-186`).

**Formal shape.** The state lattice per search is (region bits, event bits,
`Logic` inventory, `addedToPool` flags); every write in the loop is a
false→true set or a monotone inventory add, so one `do/while` reaches the
least fixed point. Sphere structure is not tracked in this query;
`GeneratePlaythrough` (`:562-586`) is the separate sphere-collecting variant
(item effects deferred to `newItemLocations`, `:383`, and the sphere restarted
on any newly enabled event, `:490-495`, `:571-577`). `CheckBeatable` (`:589-604`)
is the same loop with `stopOnBeatable = true`; `IsBeatableWithout` (`:301-312`)
temporarily removes one placement and calls it. `ValidateEntrances`
(`:607-656`) is the only caller that runs the sphere-zero/time-pass
validation (`ValidateOtherEntrance` `:113-139`, `ValidateSphereZero` `:152-174`).

**Non-local reads inside lambdas.** `Region::AnyAgeTime` (`location_access.h:215-232`)
and the free functions `SpiritShared`, `CanPlantBean`, `BothAges`,
`ChildCanAccess`, `AdultCanAccess` (`:246-255`) let a condition read *another
region's* current bits (e.g. `fire_temple.cpp:23`, `location_access.cpp:731`).
Still monotone (bits only grow within a search), but it means a lambda is a
function of the whole `areaTable`, not of (inventory, age, time) alone. A
composition layer therefore cannot evaluate OoT lambdas in isolation; it must
run the expansion.

**Negation, measured.** In `location_access/` there are exactly two `!logic->`
sites, both `!logic->IsFireLoopLocked()` (`dungeons/fire_temple.cpp:23`, `:56`),
which is a *settings* predicate (`logic.cpp:2536-2540`, reads `RSK_KEYSANITY`),
and 45 `!ctx->GetOption`/`IsNot(` sites, all settings. Zero negations of
item, event, or age state in the region files. Inside `logic.cpp`'s helper
bodies the five `!` over state are early-return guards of the form
`if (!HasItem(x)) return false;` (`:304`, `:513`) or effect-application
branches (`:1800`, `:1855`, `:1866`), all monotone. **The OoT guard
vocabulary is negation-free over world state at `c8947177`**; ADR 0010 answer
O6's review rule holds today and the static probe would lock a clean baseline.

### 1.3 The fill

`Fill()` (`fill.cpp:1215-1459`) runs up to five attempts (`:1219`). One
attempt, in order:

| Step | Anchor | Notes |
|---|---|---|
| Rebuild graph, reset items, location pool, item pool, starting inventory | `:1225-1229` | |
| Junk into excluded locations | `FillExcludedLocations`, `:1002-1011`, called `:1230` | |
| Entrance shuffle (with 8 temporary shop items so ER validation has shields) | `:1235-1244` | `ENTRANCE_SHUFFLE_FAILURE` → next attempt |
| `SetAreas` | `:1245` | |
| Shops: vanilla or shopsanity prices, then `AssumedFill(shopItems, shopLocations)` | `:1255-1308` | placed first because Gohma's reward needs a buyable shield in logic (`:1253`) |
| Scrub / merchant prices | `:1310-1355` | |
| Dungeon rewards: vanilla, or assumed fill over the nine reward locations (+ Link's Pocket) | `RandomizeDungeonRewards` `:940-998`, called `:1360` | |
| Per dungeon "own dungeon" items: small keys + key ring + boss key together, then map+compass | `RandomizeOwnDungeon` `:1014-1067`, loop `:1363-1365` | song locations are excluded from the candidate set when songs are restricted (`:1026-1036`) |
| Songs to song locations / dungeon rewards | `:1370-1392` | |
| Any-dungeon / overworld pools (keys, boss keys, Ganon's key, Gerudo keys, rewards) | `RandomizeDungeonItems` `:1077-1165`, called `:1395` | maps/compasses after (`:1152-1164`) |
| Link's Pocket | `:1167-1184`, called `:1399` | |
| **Remaining advancement items, assumed fill over `allLocations`** | `:1405-1407` | the pass increment 3 replaces |
| **Remainder, `FastFill`** (junk top-up via `GetJunkItem`) | `:1413-1414`; `:813-826`; `item_pool.cpp:70-77` | the "trailing no-logic dump" ADR 0010 D2.3 names |
| `GeneratePlaythrough` | `:1418` | |
| Success iff `playthroughBeatable && !placementFailure` → pare-down, WotH, barren, overrides, hints | `:1421-1447` | |
| Else reset locations + logic, retry | `:1449-1455`; `return -1` after five (`:1458`) | |

`AssumedFill(items, allowedLocations, setHintable)` (`:835-936`) is the
placement primitive: refuse if more items than locations (`:838-852`,
`placementFailure`); `FastFill` under No Logic (`:854-857`); up to 10 retries
(`:860`); shuffle the batch (`:877`); pop one item; `logic->Reset()` then
**assume** every item still in this batch *and* every advancement item still in
`itemPool` (`:884-890`); `ReachabilitySearch(allowedLocations)` (`:893`); no
accessible empty → roll back this batch and retry (`:896-909`); else place at a
uniformly random accessible location (`:912-913`). If All Locations Reachable
is off, each placement is followed by `CheckBeatable()` and, once beatable,
the rest of the batch is `FastFill`ed (`:925-932`). `Context::PlaceItemInLocation`
applies the item effect immediately under Glitchless (`SeedContext.cpp:143-145`).

Note the retry granularity: `AssumedFill`'s 10 retries reshuffle *one batch*;
`Fill()`'s 5 attempts rebuild everything including entrance shuffle. Both are
deterministic functions of the RNG stream (see 1.6).

### 1.4 Playthrough, hints, spoiler

- `GeneratePlaythrough` (`:562-586`) collects `ctx->playthroughLocations` as a
  `std::vector<std::vector<RandomizerCheck>>` of spheres (`SeedContext.h:127`)
  and entrance spheres. There is **no per-location sphere index** field; a
  location's sphere is its index in that vector.
- `PareDownPlaythrough` (`:709-749`) walks spheres backwards calling
  `IsBeatableWithout` per item (each a full `CheckBeatable`), erasing items the
  seed does not need; `CalculateWotH` (`:755-772`) does the same for
  way-of-the-hero candidacy and then re-runs `ReachabilitySearch(allLocations)`
  to restore `addedToPool` state; `CalculateBarren` (`:785-810`) is area-based.
  Cost is O(#playthrough items) full fixpoints — the most expensive part of a
  successful attempt.
- `CreateAllHints` (`3drando/hints.cpp:801-811`) → static hints, then gossip
  stone hints; hint selection draws from the fill RNG (`:114`, `:119`, `:377`,
  `:391`, `:422`, `:503`), so hints are part of the deterministic stream.
- `SpoilerLog_Write` (`3drando/spoiler_log.cpp:340-403`) writes settings,
  excludes, starting inventory, enabled tricks, MQ dungeons, playthrough,
  hints, entrances, and every location to `Randomizer/<hash-icons>.json`
  (`:391`) and sets the `SpoilerLog` CVar (`:395`). `GenerateHash` (`:47-58`)
  derives the five icon indexes from the seed hash string.

### 1.5 Tricks

- Vocabulary: `RandomizerTrick` (`randomizerTypes.h:4142-4416`; 267 lines
  incl. `RT_MAX`, so 266 keys). Rows are built with
  `#define OPT_TRICK(rsk, ...) mTrickOptions[rsk] = TrickOption::LogicTrick(rsk, __VA_ARGS__)`
  (`settings.cpp:113`) from `:1355` on; each row carries quest, area, a tag set,
  name and description (`option.h:349-397`). A block of glitch-class rows is
  commented out (`:1328-1352`), but tagged-`GLITCH` rows such as
  `RT_GROUND_JUMP`, `RT_HOVER_BOOST_SIMPLE` are live (`:1385-1420`). They are
  grouped under "Logical Tricks" (`RSG_TRICKS`, `:2199`) and indexed by area
  (`tricks.cpp:6`).
- Resolution: `Context::FinalizeSettings(excludedLocations, enabledTricks)`
  (`settings.cpp:2933`) writes the caller's set into
  `std::array<OptionValue, RT_MAX> mTrickOptions` (`SeedContext.h:187`);
  guards read `ctx->GetTrickOption(RT_x)` (`SeedContext.cpp:548`).
- Use: 425 consults in `location_access/` and ~30 inside `logic.cpp` helpers
  (`:487-502`, `:667`, `:724`, `:799-818`, `:939-946`, `:1067-1107`, `:1222`,
  `:1414-1440`, `:2569-2588`). The idiom is always disjunctive widening —
  `X || (ctx->GetTrickOption(RT_...) && Y)` — which is why the trick set is a
  monotone parameter of the operator (ADR 0010 §3.2 holds at source).
- Identity: every trick's option text is concatenated into `settingsStr`
  (`playthrough.cpp:59-61`), so it is in both `rsbsSettingsHash` (`:77`) and the
  RNG reseed (`:83-84`); the spoiler lists them (`spoiler_log.cpp:195`).

### 1.6 RNG and determinism

- Generator: PCG-XSH-RR (`ShipUtils::next32`, `ShipUtils.cpp:140-159`;
  constants `random.cpp:4-5`) with rejection-sampled `Random(min, max)` →
  `[min, max)` (`ShipUtils.cpp:162-175`). The fill uses a **private** state,
  `rando_state` (`random.cpp:3`), not `ShipUtils`' default stream.
- Seeding: `Random_Init(seed)` before settings are finalized (random-valued
  options draw here, `playthrough.cpp:31`, `:42`), then
  `Random_Init(Hash(std::to_string(seed) + settingsStr))` (`:83-84`) — the
  "double reseed" MM mirrors. `DontGenerateSpoiler` appends the build version
  to the string (`:79-81`).
- Consumers, in stream order: `FinalizeSettings`, `GenerateItemPool`
  (`item_pool.cpp:192`, `:925`, `:935`), shop/scrub/merchant prices, entrance
  shuffle, every `AssumedFill` shuffle/pick, `FastFill`, hints. An empty seed
  string draws ten digits from the default stream (`menu.cpp:628-634`), which
  is the only unseeded draw in the path.
- Digest: `Rando_HeadlessSeedDeterminismDigest` (`menu.cpp:508`) emits
  `seed`, `settingsHash`, `sourceIsRando`, `placementHash`, `placedCount`,
  `foreignOoTHash`, `foreignOoTCount`, `comboSettingsHash`; the SeedDeterminism
  CTest row byte-diffs two processes (`CMake/CheckSeedDeterminism.cmake`).

### 1.7 Entry points and the pairing stamp

- GUI: `GenerateRandomizerImgui` (`randomizer.cpp:3471`) on a `std::thread`
  (`:3541`) → `RandoMain::GenerateRando` (`3drando/rando_main.cpp:13-16`) →
  `GenerateRandomizer(excludes, tricks, seed)` (`menu.cpp:621-652`) →
  `Playthrough::Playthrough_Init` (`playthrough.cpp:27`).
- Headless: `Rando_HeadlessSeedTest` (`menu.cpp:35-69`) creates the context,
  options and tables if the GUI never did, applies `RSBS_DIAG_CVARS`, and
  calls the same `GenerateRandomizer`.
- Inside `Playthrough_Init`: `Random_Init`, resets (`:34-40`, including
  `ctx->GetLogic()->Reset()` which detaches the save — see 1.8),
  `FinalizeSettings`, settings string + hashes, `Fill()` (`:87`; negative
  return propagates), `GenerateHash`, `SpoilerLog_Write` (`:100`), then the
  **pairing stamp** — `sourceIsRando`, `sharedRandoSeed`,
  `sharedRandoSettingsHash` (`:125-127`), `mmProfileDigest =
  MM_Rando_ComputeProfileStamp()` (`:140`), `Combo_ResolveComboSettings` +
  `Combo_FreezeComboSettings` (`:157-163`), and finally
  `OoT_PlaceForeignItems()` (`:178`). Everything after `Fill()` is reached only
  on a successful fill and spoiler write (ADR 0009 D2's post-condition).
- File creation is a separate, later seam: `OoT_Sram_InitSave`
  (`games/oot/src/code/z_sram.c:237`) → `Context_InvalidateSessionOnNewGame`
  (`:320`) → `Save_SaveFile` (`:340`). Increment 2 moves the whole creation
  here.

### 1.8 State: what is global, what mutates, what is safe

| Global | Where | Lifetime / reset |
|---|---|---|
| `areaTable` | `location_access.cpp:573` | rebuilt by `RegionTable_Init` per fill attempt; bits reset per query by `AccessReset` |
| `ctx`, `logic` raw pointers | `:955-956` | rebound in `RegionTable_Init` |
| `Rando::Context` singleton (`itemLocationTable`, `mOptions`, `mTrickOptions`, `playthroughLocations`, `overrides`) | `SeedContext.h:183-199` | per generation via `ItemReset`/`LocationReset`/`HintReset` |
| `Rando::Logic` (`inLogic[]`, `mSaveContext`, the four age/time flags, `CurrentRegionKey`/`CurrentCheckKey`) | `logic.h:21-173` | `Reset(bool resetSaveContext)` `logic.cpp:2655` |
| `itemPool`, `placementFailure`, `grottoEvents`, `rando_state` | `item_pool.cpp:13`, `fill.cpp:23`, `location_access.cpp:17`, `random.cpp:3` | per generation |

**Every query mutates.** `ReachabilitySearch`, `CheckBeatable`,
`GeneratePlaythrough` set region bits, `ItemLocation::addedToPool`, `Logic`
inventory (placed items are *applied* as they are reached) and `inLogic`.
`IsBeatableWithout` clears and restores a placement (`fill.cpp:304-310`);
`PareDownPlaythrough` moves placements through `delayedItem` (`:730-731`,
`:746-748`). None of this is the live game save **provided** `Logic` is
detached: `Logic::Reset(true)` → `NewSaveContext()` allocates a fresh
`SaveContext` (`logic.cpp:2310-2315`, `InitSaveContext` `:2168`), and
`Playthrough_Init` calls it first (`playthrough.cpp:37`). Two hazards for a
coordinator:

1. On save load the port points `Logic` at the live save
   (`SetSaveContext(&gSaveContext)`, `logic.cpp:2164-2166`; the OoT pool TU's
   own comment records that this is how in-game progressive gives resolve,
   `ForeignItemsSingleExe.cpp:16-19`). Any reachability query issued without a
   preceding `Reset(true)` evaluates against — and applies item effects
   **into** — the live OoT save. The query surface must begin with the detach.
2. `NewSaveContext` frees a `new`-allocated object with `free()`
   (`:2312`, `:2314`). Benign on the shipped toolchains for a trivially
   destructible C struct, but undefined; a coordinator calling `Reset(true)`
   thousands of times per fill (the assumed fill already does, `fill.cpp:884`,
   `:926`) leans on it. Inherited from the port; recorded, not a task.

The solver is not thread-safe (file-scope globals throughout), and the GUI
path runs it on a worker thread (`randomizer.cpp:3541`).

### 1.9 Sizes

| Unit | LOC / count |
|---|---|
| `location_access/` (35 region files) | 12,427 LOC; 2,661 `LOCATION(`, 2,394 `Entrance(`, 400 `EventAccess(` |
| `3drando/` | 8,954 LOC (`fill.cpp` 1,459; `hint_list.cpp` 2,482; `shops.cpp` 1,660; `item_pool.cpp` 945; `hints.cpp` 811; `spoiler_log.cpp` 410) |
| `logic.cpp` / `logic.h` | 2,761 / 173 |
| `location_access.cpp` / `.h` | 1,218 / 304 |
| `entrance.cpp` / `.h` | 1,748 / 161 |
| `settings.cpp` | 3,367 |
| `randomizerTypes.h` | 7,289 |
| Enumerators | ~1,013 `RR_`, ~2,527 `RC_`, ~304 `RG_`, 266 `RT_`, 321 `LOGIC_`, 37 `RA_`, ~231 `RSK_` |

---

## 2. The MM solver (2Ship2Harkinian's Rando)

### 2.1 Data model

**Regions.** `struct RandoRegion` (`Logic/Logic.h:238-252`): `name`,
`sceneId`, `checks` (`std::map<RandoCheckId, pair<std::function<bool()>, string>>`),
`exits` (`std::map<s32 entranceId, RandoRegionExit{returnEntrance, condition, string}>`,
`:232-236`), `connections` (`std::map<RandoRegionId, ...>` — intra-scene
edges), `events` (`std::vector<pair<RandoEvent, std::function<bool()>>>`),
`oneWayEntrances` (`std::set<s32>`), and the time fields `timeSlices`
(declared "unused in current implementation", `:248`), `canStayOverTime`
(`:249`), `timeStayRestrictions` (`:250-251`). The graph is
`std::map<RandoRegionId, RandoRegion> Regions` (`Logic.cpp:13`), populated by
`RegisterShipInitFunc` lambdas — 314 `Regions[RR_...] = RandoRegion{...}`
assignments across the 17 files under `Logic/Regions/` — plus the virtual root
`RR_MAX` (`Logic.cpp:334-354`: the two starting-item checks and the save-warp /
owl-warp exits). Region ids: `RandoRegionId` (`Types.h:2552-2868`, 315
values + `RR_MAX`).

**There is no separate entrance table.** An exit is keyed by the *target*
entrance id and resolved to a region by `GetRegionIdFromEntrance`
(`Logic.cpp:18-39`), which builds a static map on first call from every
region's exits' `returnEntrance` and `oneWayEntrances`. "Which region owns
entrance E" therefore means "which region declares E as the entrance you
arrive at". This is also how a save's `entrance` field seeds the crawl
(`:153`, `:301`).

**Conditions** are written with the `CHECK`/`EXIT`/`CONNECTION`/`EVENT`/`STAY`
macros (`Logic.h:336-367`) as capture-less lambdas over the macro vocabulary
at `:272-334`: `HAS_ITEM` = `INV_CONTENT(item) == item` (`:277`); form
predicates `IS_DEKU`/`IS_ZORA`/… read the live `playerForm` and `CAN_BE_X` is
"is X or has X's mask" (`:278-284`); `CAN_PLAY_SONG` requires the ocarina, the
quest bit and the button set (`:295-297`, `canPlaySong` `:386-452`);
`CAN_ACCESS(x)` reads the event counter (`:313`); `KEY_COUNT` reads
`foundDungeonKeys` (`:323`); `CAN_AFFORD` reads the check's rolled price
(`:324-326`); `CAN_USE_EXPLOSIVE` admits the Powder Keg unconditionally
(`:289-291`; #578 finding (a)). `CanKillEnemy` (`:692-850`) is the enemy
table; `CanAccessDungeon` (`:454-486`) the dungeon-access option;
`MoonMaskCount`/`RemainsCount`/`MeetsMoonRequirements` (`:488-511`) the goal
parameters.

**World state is the live save.** Every macro dereferences `gSaveContext`;
`RANDO_SAVE_CHECKS`, `RANDO_SAVE_OPTIONS`, `RANDO_EVENTS` are `#define`s onto
`gSaveContext.save.shipSaveInfo.rando.{randoSaveChecks, randoSaveOptions, randoEvents}`
(`Rando/Rando.h:9-11`; struct at `games/mm/include/z64save.h:382-391`).
`RandoSaveCheck` (`:372-380`) is `{randoItemId, shuffled, eligible, cycleObtained, obtained, skipped, price}`.
There is no detached simulated save anywhere in `games/mm`.

**Events** are `RandoEvent` (`Types.h:3042-3126`, 82 values) stored as a
**u8 count** per event (`randoEvents[RE_MAX]`, `z64save.h:384`); several
events are registered many times so that `RANDO_EVENTS[x] >= n` means
something (`Logic.cpp:162-179`, the `RE_ACCESS_ZORA_EGG >= 7` case at
`Regions/West.cpp:215`).

**Items and checks.** `RandoItemId` (`Types.h:2312-2549`, ~236) with
`RandoStaticItem` (`StaticData/StaticData.h:37-46`) carrying `randoItemType ∈
{BOSS_KEY, HEALTH, JUNK, LESSER, MAJOR, MASK, SKULLTULA_TOKEN, SMALL_KEY, STRAY_FAIRY}`
(`Types.h:2299-2310`). `RandoStaticCheck` (`StaticData.h:20-28`) is
`{id, name, type, sceneId, flagType, flag, vanilla item}` — no area field;
`StaticData::RandoStaticRegion`/`Regions` (`:70-79`) is declared and dead.
`RandoCheckId` `:4-2296` (~2,257).

**Goal.** The Moon is reached from `RR_CLOCK_TOWER_ROOF` by
`CAN_PLAY_SONG(OATH) && MeetsMoonRequirements()` (`Regions/Central.cpp:83`);
the roof itself only after `AFTER(TIME_NIGHT3_AM_12_00)` from South Clock Town
(`:212`). Majora's Lair is a one-way exit from `RR_MOON` guarded by
`RemainsCount() >= RO_ACCESS_MAJORA_REMAINS_COUNT && MoonMaskCount() >= RO_ACCESS_MAJORA_MASKS_COUNT`
(`Regions/Moon.cpp:115`). `RR_MOON_MAJORAS_LAIR` (`:66-76`) holds two pots and a
`TODO: 1) Add a check for Game Completion?` (`:68`); `CanKillEnemy` has no
Majora row. So "MM beatable" is derivable as *lair reachable* but not as
*Majora defeatable* — a gap for ADR 0010 D1's `MM_GOAL` (§2.11).

### 2.2 The time-slice model

- 45 named slices, `TimeSlice` (`Logic.h:23-69`; last enumerator `= 44`),
  `TIME_SLICE_COUNT` (`:73`), `TIME_ALL_SLICES = 0x1FFFFFFFFFFF` (45 bits,
  `:75`). Six half-day ranges `HALF_DAY_TIME_RANGES` = {0–6, 7–15, 16–22,
  23–30, 31–36, 37–44} (`:85-92`).
- Per crawl, each reached region gets `RegionTimeState{timeSlices, canStayOverTime}`
  (`:99-102`); the root starts at Day 1 06:00 (`InitialTimeState`,
  `Logic.cpp:62-64`) or at the owned slices under Clock Shuffle
  (`InitializeRegionTimeStates`, `:67-80`).
- `ExpandTimeForward` (`TimeLogic.cpp:16-84`): with no `STAY` restrictions and
  no Clock Shuffle, a region you can wait in yields **all later slices** by a
  bitwise fill (`:18-31`) — time only moves forward and the three-day reset is
  free (the root is reachable from South Clock Town via
  `CONNECTION(RR_MAX, true)`, `Central.cpp:215`). Otherwise slices are walked
  in order and expansion stops at the first unowned slice or failed `STAY`
  (`:33-74`); under Clock Shuffle item-gated `STAY`s are **ignored** (`:60-63`),
  an intentional over-approximation.
- Checks read time through `AT/BEFORE/AFTER/BETWEEN` (`Logic.h:680-686`) over
  `gCurrentRegionTime`, ANDed with `ClockFilter()` (`:663-668`); the composite
  `IS_DAY1..IS_NIGHT3` (`:654-659`) fold clock ownership in. 37 `STAY(`
  restrictions exist across `Regions/`.
- Clock Shuffle puts `RI_TIME_*` into the pool (`GeneratePools.cpp:187-202`);
  the fill rebroadcasts owned time into every region's state when one is
  collected (`GlitchlessLogic.cpp:161-179`).

**O5 (45 vs 46 slices).** The enum's Night-2 run is `PM_06_00, PM_08_00,
PM_09_00, PM_10_00, PM_11_00, AM_12_00, AM_04_00, AM_05_00` (`Logic.h:47-54`,
eight slices) against nine for Night 1 (`:31-39`, which has `AM_02_30`). No
`AM_05_30` slice exists anywhere; `grep AM_05_30 Regions/` is empty, so no
authored guard needs the 46th boundary today. Adding `TIME_NIGHT2_AM_05_30`
between `:54` and `:55` would renumber every Day-3/Night-3 slice (names stay,
values shift), and must update `TIME_ALL_SLICES` (`:75`), ranges 3–5 of
`HALF_DAY_TIME_RANGES` (`:89-91`), and nothing in `TimeSliceFromGameTime`
(`Logic.cpp:42-59`, which only uses `range.startSlice`). Every crawl's time
masks change, so it is a global re-pin — consistent with the ADR's "adopt 46
unless the source review justifies otherwise" landing on increment 2 or 3, not
before. The u64 representation has room (`:74-75`).

### 2.3 Reachability: three crawls

| Crawl | Anchor | Join | Mutates the live save? | Consumers |
|---|---|---|---|---|
| `FindReachableRegions` (recursive DFS) | `Logic.cpp:95-139` | **no** — first-visit guard (`:115`, `:129`), target time state overwritten on first reach (`:119-120`, `:133-134`); a region reached later with more time is never re-explored (#585) | reads only; sets `gCurrentRegionTime` | the Glitchless **fill** (`GlitchlessLogic.cpp:97-99`) |
| `CrawlReachableRegions(startEntrance)` (frontier fixpoint) | `:147-265` | **yes** — `merged = old \| current`, re-explore on change (`:224-227`); events counted per *registration* (`:179`, `:247-261`) | **yes** — zeroes `RANDO_EVENTS` (`:158-160`) and re-fires them into the save | check tracker (`CheckTracker/CheckTracker.cpp:255`), the closure |
| `ComputeReachableCheckSet()` (closure) | `:283-331` | inherits the crawl's | **yes, then restores** — heap `memcpy` snapshot (`:289-290`), `GiveItem(ConvertItem(placed))` per newly-reached check (`:315-321`), restore + `MM_GameEvents_Queue` truncation (`:326-329`) | forward placement gate (`Foreign.cpp:594`), both MM digests, the #580 locks |

`EvaluateReachableChecks` (`:267-281`) evaluates each reached region's check
lambdas under that region's joined time. None of the three takes a "granted
items" argument; the world state **is** `gSaveContext`, and "given these
items" is expressed by giving them into the save before crawling. None
produces sphere indices. The logic mode does not gate evaluation — unlike
OoT, MM lambdas are always evaluated; `RO_LOGIC` only selects which fill runs
(`OnFileCreate.cpp:221-240`), so the tracker crawl of a Nearly-No-Logic world
still reports Glitchless reachability.

**Negation, measured.** Zero negations of item/event/flag state in
`Regions/*.cpp` (`!HAS_ITEM`, `!CAN_*`, `!Flags_GetRandoInf`,
`!RANDO_EVENTS`, `!CHECK_QUEST_ITEM`, `!CHECK_WEEKEVENTREG`: none). In
`Logic.h` the one `!` over state is `!HaveEnemySoul` inside a guard that
returns false (`:694`), i.e. "requires the soul" — monotone. `STAY` conditions
in the region files are positive or literal `false`. **MM's guard vocabulary
is also negation-free at `c8947177`.** The monotonicity risk in MM is not in
the guards; it is in (a) `RANDO_EVENTS` being wiped by every crawl and (b) the
fill's `RemoveItem` backtracking (2.4), both of which live outside the
operator.

### 2.4 Pools and fill

**Pools.** `GeneratePools(saveInfo, checkPool, itemPool)` (`GeneratePools.cpp:15-288`)
walks `Regions` and, per check, applies the per-type shuffle options
(`:59-125`), rolls shop/Tingle prices from `Ship_Random` (`:123`, `:137`),
turns a user-excluded check into `shuffled = true, randoItemId = RI_JUNK,
skipped = true` **outside** the pool (`:145-155`), and otherwise pushes the
check and its vanilla item (`:157-158`). Then the location-less items:
sword/shield (`:164-165`), boss and enemy souls (`:170-184`), clock items
(`:187-202`), swim (`:205-207`), ocarina buttons (`:210-214`), triforce pieces
(`:217-223`); starting items are removed one-for-one (`:226-231`); Plentiful
duplicates majors (`:234-278`, one `Ship_Random` draw per lesser item `:254`);
traps (`:281-287`). `OnFileCreate` then balances the two pools by padding
`RI_JUNK`, dropping junk, or consolidating four heart pieces into a container
(`OnFileCreate.cpp:171-216`).

**The Glitchless fill is a forward fill, not an assumed fill.**
`ApplyGlitchlessLogicToSaveContext` (`GlitchlessLogic.cpp:24-285`):

1. Stack-snapshot the live save (`:37-38`; `sizeof(SaveContext)` ≈ 0xC000 per
   `src/common/game.h:49`), seed `regionsInLogic = {RR_MAX}` (`:40`), keep
   `eventsInLogic` as a set of event-registration **pointers** (`:42`), shuffle
   the item pool (`:54-59`).
2. Loop (`:85`): wall-clock check (`:87-89`); crawl every region in
   `regionsInLogic` with `FindReachableRegions` (`:97-99`); fire newly true
   events (`:111-123`); for every check whose lambda now passes and is not yet
   in logic (`:126-183`): if it is in `checkPool`, **pop the next pool item and
   place it there** (`:141-143`, writes `RANDO_SAVE_CHECKS[...].randoItemId`
   `:156-157`), else credit the vanilla item; `GiveItem(ConvertItem(item))`
   into the live save (`:158`); junk/health placements are remembered with the
   current `weight` (`:145-149`); a clock item rebroadcasts time (`:161-179`).
3. `itemPool.empty()` → done (`:186-189`). Otherwise if nothing changed this
   round (`:192`): choose a junk-holding check by weighted random (weights grow
   with each progress round, `:51`, `:201-212`, `:257`), swap in the first
   untried non-junk item — `RemoveItem(old)`, `GiveItem(new)` (`:242-253`) —
   and loop; when every non-junk item has been tried at that check, leave it and
   pick another (`:232-239`). Errors: `"No checks with junk"` (`:195`),
   `"No non-junk items left"` (`:229`), and the timeout (`:88`) which alone
   throws `GenerationTimeout`. Every error path restores the snapshot (`:78`).
4. On success, copy the placements into the snapshot and restore it
   (`:276-282`) — the post-fill save is *pre-fill state + placements*, not the
   "everything collected" state.

Consequences worth stating plainly:

- The loop can only end with every shuffled check having been reached in a
  forward simulation, so MM Glitchless is **all-locations-reachable by
  construction** and has no "beatable-only" rung. That is also why it
  dead-ends: a check that needs an item the pool has already spent behind it.
- Soundness rests on `FindReachableRegions` under-approximating (#585): a
  world it accepts is reachable; some worlds it rejects would have been fine.
- "Weight" is a sphere-forward bias, not a sphere: late junk is likelier to be
  swapped for progression.
- The item-effect model is the **real give path** — `GiveItem.cpp` (379 LOC),
  `ConvertItem.cpp` (673, progressive resolution), `RemoveItem.cpp` (472,
  which exists only so the fill can backtrack). OoT's equivalent is the
  ~400-line `Logic::ApplyItemEffect` switch (`logic.cpp:1736`) into a
  detached struct.

The other modes: `ApplyNearlyNoLogicToSaveContext` (`NearlyNoLogic.cpp:12-89`)
shuffles and then swaps nine key items out of a per-item scene blacklist
(temples, Moon); `ApplyNoLogicToSaveContext` (`NoLogic.cpp:12-26`) shuffles;
Vanilla writes vanilla items with `shuffled = true` (`OnFileCreate.cpp:221-230`).

### 2.5 The attempt ladder (#581) and the reachable-check gate (#580)

- **Ladder** (`OnFileCreate.cpp:300-400`): for a paired world, one generation
  attempt is the lambda `runGenerationOnce` (`:154-297`: starting items,
  `GeneratePools`, balance, `GrantStartingItems`, dispatch by `RO_LOGIC`,
  foreign placement). The loop (`:335-386`) restores a `static SaveContext
  sPreAttemptSave` image (`:333`, `:337`) and clears foreign placements before
  each retry, reseeds with `MixPairedFinalSeedForAttempt(attempt)`
  (`:340-342`; recipe `Foreign.cpp:317-333` — attempt 0 is byte-identical to
  the pre-ladder derivation, `n ≥ 1` appends `":glitchless-attempt-n"`),
  catches `GenerationTimeout` **first** and rethrows without climbing
  (`:348-373`), climbs on any other `std::exception` up to
  `kPairedGenMaxAttempts = 10` (`Foreign.h:100`; `:374-385`), and records the
  winning index in `gComboCtx.mmPairedAttempt` (+1-displaced, `:393`) and the
  spoiler (`rsbsPairedAttempt`, `:423`). The outer catch (`:459-474`) still
  reverts to `SAVETYPE_VANILLA`; the arrival gate turns that into
  `RsbsSave_RefuseSlotGeneration` (`GameExports_SingleExe.cpp` ≈`:3318`).
- **Gate** (`Foreign.cpp:493-682`): after the direction gate (`:532`), pool
  draw (`:565-569`) and `wanted` (`:569`), the closure is computed once
  (`:594`), candidates are `IsEligibleHost ∩ reachable` in id order
  (`:612-622`), the local xorshift is seeded from
  `sharedRandoSeed:sharedRandoSettingsHash:finalSeed:"foreign-v1"` (`:633-635`),
  and hosts are drawn without replacement (`:640-664`). Fewer candidates than
  `wanted` is under-supply (`:667-680`), recorded in the spoiler
  (`Spoiler/Generate.cpp:103-111`); a refused table insert throws (`:656-663`)
  and is a ladder rung.

### 2.6 Spoiler

`Rando::Spoiler::GenerateFromSaveContext` (`Spoiler/Generate.cpp:15-116`):
`options` (every row, `:21-24`), starting items, shuffled `checks` with the
foreign display name substituted for a hosting check (`:39-49`),
`rsbsPairing` = the three identity terms (`:80-84`), `foreign` (`:86-96`),
`foreignShortfall` (`:103-111`); `OnFileCreate` adds `inputSeed` and
`rsbsPairedAttempt` (`:416-424`) and writes `<inputSeed>.json` (`:427-428`).
Loading is `ApplyToSaveContext` (`Spoiler/Apply.cpp`, 465 LOC) — an
untrusted-file path gated on the pairing identity (#610). There is no
playthrough, sphere, WotH or hint section.

### 2.7 Tricks

None. The only coarseness knob is `RO_LOGIC` (`Types.h:2933-2936`). Sixteen
trick-shaped seams are comments (`Regions/Central.cpp:365`, `MilkRoad.cpp:28-29`,
`:55`, `:62`, `:116`, `:132`, `SnowheadTemple.cpp:18`, `:91`, `:195`, `:200`,
`South.cpp:13`, `:125`, `WoodfallTemple.cpp:146`, `:202`,
`GreatBayTemple.cpp:120`, `East.cpp:186`). The Powder-Keg-as-explosive trick
is unconditional (`Logic.h:289-291`). #578 owns the vocabulary and must land
before increment 3 because trick sets freeze into identity (ADR 0010 §3.3).

### 2.8 RNG and determinism

- `Ship_Random(min, max)` (`2s2h/ShipUtils.cpp:331-344`) is the same PCG as
  OoT's, in a process-global `state` (`:310`) seeded by `Ship_Random_Seed`
  (`:313-316`); an unseeded first draw self-seeds from `std::random_device`
  (`:322-325`).
- Seeding: `Ship_Random_Seed(Ship_Hash(inputSeed))` (`OnFileCreate.cpp:107-108`)
  and again per ladder attempt (`:342`). The paired input seed is
  `"RSBSPAIR" + sharedRandoSeed` (`Foreign.cpp:87-92`); the final seed folds
  `MMOptionsString()` — every `RANDO_SAVE_OPTIONS` value in table order
  (`:94-109`) — mirroring OoT's double reseed.
- Consumers: pools (prices, Plentiful), the fill's shuffles and junk pick, the
  no-logic shuffles. The crawls and closure consume none (asserted by the
  `Logic.h:155`/`:179-183` contracts and locked by MMRandoGen FAIL(28)).
  Foreign placement uses its own xorshift (2.5).
- The one non-determinism is the 10 s wall-clock budget
  (`GlitchlessLogic.cpp:30`, test override `:21`, `Logic.h:225`). #581 measured
  it as the *dominant* first-attempt failure (14/66 heavy-profile seeds, 0
  dead-ends); #582 holds the budget decision.
- Digest: `MM_Rando_HeadlessForeignDigest` (`mm_rando_gen_test.cpp:1004-1140`)
  emits `mmFinalSeed`, `mmPlacementHash`, `mmReachableCount`,
  `mmReachableHash`, `mmPairedAttempt`, and every foreign placement;
  `MM_Rando_HeadlessPairedAttemptDigest` (`:2373`) is the ladder's two-process
  twin.

### 2.9 Entry points

- Boot: `MM_Rando_Init` (`GameExports_SingleExe.cpp:1410-1420`, once-only via
  `sRandoInitDone`) → `S2H::ShipInit::InitAll()` (the region registrars) →
  `Rando::Init()` (`Rando.cpp:24-42`: spoiler options, behaviours, check
  tracker, file-drop handler, `OnSaveLoad` hook).
- Generation: `Rando::MiscBehavior::OnFileCreate` (`OnFileCreate.cpp:24`) on
  `GameInteractor::OnSaveInit`, reached from MM's file select, from the arrival
  gate `MM_Rando_PairOnCrossGameArrival` (`GameExports_SingleExe.cpp:3061`;
  legacy combo-settings freeze ≈`:3093`, profile digest compare ≈`:3161-3172`,
  the real dispatch `GameInteractor_ExecuteOnSaveInit` ≈`:3288`), and from the
  headless harness (`mm_rando_gen_test.cpp:1044`, `:2411`). The save **is** the
  output: there is no override table to apply later.
- The pairing decision is `Rando::Foreign::PairingActive()` =
  `Combo_ForeignPairingActive()` (`Foreign.cpp:83-85`; `OnFileCreate.cpp:32`),
  and the profile is resolved once by `ResolvePairedProfile` (`Foreign.cpp:248-308`,
  Glitchless pin `:168-173`, compare-or-stamp `:263-306`) — the same
  computation `MM_Rando_ComputeProfileStamp` (`:765-769`) runs side-effect-free
  at OoT's creation event.

### 2.10 State: what is global, what mutates

| Global | Anchor | Note |
|---|---|---|
| `gSaveContext` (inventory, flags, `RANDO_SAVE_*`, `RANDO_EVENTS`, `entrance`, `playerForm`) | `z64save.h:382-391`; `Rando.h:9-11` | the world state, the fill's output, and the crawl's start point (`Logic.cpp:301`) |
| `Regions` | `Logic.cpp:13` | built once; `operator[]` inserts a default on a miss (`EnsureRegionTimeState`, `:86`) |
| `GetRegionIdFromEntrance` cache | `:19-20` | built on **first call**; if called before the registrars run it caches an empty map forever |
| `gCurrentRegionTime` | `:16` | `thread_local`; set per region during evaluation |
| `Ship_Random` state | `ShipUtils.cpp:310` | process-global |
| `MM_GameEvents_Queue` | outside the save | `GiveItem`'s triforce branch pushes into it (`Logic.cpp:291-296`) |
| ladder / placement statics: `sPreAttemptSave`, `sSelectState`, `sLastPlacementStats`, `sLastPairedGen*` | `OnFileCreate.cpp:333`; `Foreign.cpp:348`, `:487`, `:339-340` | session-scoped |
| tracker `checksInLogic` | `CheckTracker.cpp:238` | refreshed every 20 frames (`:243`) |

**Every query mutates the live save.** Even the "read-only" crawl zeroes and
re-fires `RANDO_EVENTS` (`Logic.cpp:158-160`, `:256`). The closure gives items
and restores. The fill gives, removes, and restores-with-placements. This is
the "single biggest structural obstacle" ADR 0010's Risks section names
(`:780-784`), and the memcpy discipline is the only isolation that exists. The
discipline is sound for the save; it does not cover `MM_GameEvents_Queue`
(handled separately, `:295`, `:327-329`) or anything the give path might
touch outside the save (nothing found statically — see §6.3).

### 2.11 Findings specific to MM

1. **O5** — 45 slices; a 46th would be a global re-pin (2.2).
2. **O1** — `RO_ACCESS_MAJORA_REMAINS` (`Types.h:2875`) has a tombstone row
   (`StaticData/Options.cpp:23-44`), is drawn disabled-with-reason
   (`OptionsUiSingleExe.cpp:218`), and is locked as retired
   (`mm_rando_options_test.cpp:244-264`). No consumer exists in `Regions/` or
   `Logic.h`. The retirement is complete; nothing further for increment 3.
3. **#585** holds: `FindReachableRegions` is first-visit-wins (`Logic.cpp:115`,
   `:129`); the join-correct crawl exists beside it; the fill still uses the
   old one. The fix is a one-line consumer swap plus a global re-pin.
4. **`MM_GOAL` has no "Majora defeated" predicate.** ADR 0010 D1 defines
   `MM_GOAL` as "Majora's Lair reached and Majora defeated"; the graph gives
   the first (`Moon.cpp:115`) and not the second (`Moon.cpp:68`; no boss row in
   `CanKillEnemy`). Increment 3's exit condition needs one authored.
5. **`beatable(T = ∅)` is not what MM Glitchless computes** (#578 finding (a)):
   the keg disjunct is unconditional.
6. **The Glitchless fill is ALR-only.** There is no way to ask it for
   "beatable, not all reachable"; that rung must come from a different fill
   (the coordinator's assumed fill) or from adding an exit condition to the
   forward fill.
7. **`GetRegionIdFromEntrance` init-order hazard** (2.10) — could not be
   confirmed as reachable statically; recorded in §6.3.

### 2.12 Sizes

| Unit | LOC / count |
|---|---|
| `Logic/Regions/` (17 files) | 5,878 LOC; 314 region registrations; 2,422 `CHECK(`, 285 `EXIT(`, 419 `CONNECTION(`, 117 `EVENT(`, 37 `STAY(` |
| `Logic/` | 2,042 (`Logic.h` 856, `Logic.cpp` 359, `GeneratePools.cpp` 292, `GlitchlessLogic.cpp` 289, `TimeLogic.cpp` 123, `NearlyNoLogic.cpp` 93, `NoLogic.cpp` 30) |
| Give path | `GiveItem.cpp` 379, `RemoveItem.cpp` 472, `ConvertItem.cpp` 673 |
| `Foreign.cpp` / `.h` | 873 / 219 |
| `ForeignItemsSingleExe.cpp` (MM pool) | 927 (`kForeignPoolMMV1` `:268-526`) |
| `StaticData/` | `Checks.cpp` 2,334, `Items.cpp` 557, `Options.cpp` 102 |
| `Types.h` | 3,137 |
| `OnFileCreate.cpp` | 476 |
| Enumerators | 315 `RR_`, ~2,257 `RC_`, ~236 `RI_`, 82 `RE_`, 47 `RO_` |

---

## 3. The redship layer already in place

### 3.1 Origin-tagged `SharedItem` and the two pools

- `SharedItem{originGame, flags, id}` (ADR 0002 §1; `src/common/context.h:306`)
  is the only form in which an item id may cross a game boundary. The
  durable carrier is `gComboCtx.sharedItemsTagged[64]` (`:560`,
  `RSBS_SHARED_ITEM_CAP` `:164`), with the producer/consumer API in
  `shared_items.h` (`Combo_RecordSharedItem` `:139`,
  `Combo_RedeemSharedItemsForGame` `:186`, the sourced-grant seam `:233`).
- Each game's pool table is defined in the one TU where its enum is in scope
  and **registered** into an origin-indexed registry
  (`foreign_items.h:98-137`): OoT's `kForeignPoolV1` — four `PROGRESSION`
  rows (`ForeignItemsSingleExe.cpp:125-146`, registrar `:232-237`) — and MM's
  `kForeignPoolMMV1` (`Rando/ForeignItemsSingleExe.cpp:268-526`, registrar
  `:618-624`). Each row carries `name`, `article`, `itemClass` and `iconName`
  so `src/common` never translates an id (`foreign_items.h:47-78`). Lookups
  are keyed on `(originGame, name)` (`:111-118`, `:207`). Exclusions are
  criterion-attributed tables in both TUs (`ForeignItemsSingleExe.cpp:184-216`;
  MM `:559-608`), the observable of the six criteria at `foreign_items.h:365-385`.

### 3.2 The frozen combo record, direction gate, item-class rule

- `ComboSettingsRecord` is twelve bytes — `formatVersion, direction,
  poolSizeOoT, poolSizeMM, itemClassOoT, itemClassMM, goal, logicRung, spare0,
  spare1` (`context.h:500-511`) — with every value space pinned
  (`foreign_items.h:275-344`): `RSBS_COMBO_DIR_{OFF,FORWARD,REVERSE,BOTH}`,
  `RSBS_COMBO_GOAL_{BEAT_BOTH,BEAT_EITHER,TRIFORCE_HUNT}`,
  `RSBS_COMBO_RUNG_{NONE,BEATABLE,ALL_REACHABLE}`, six `RSBS_ITEMCLASS_*` bits.
  The GOAL and rung ADR 0010 D1/§2.2 define therefore already have an encoding
  and a frozen home; the trick set T is deliberately not in the record
  (`:286-289`) — it lives in the two half-digests.
- Resolver: `Combo_ResolveComboSettings` (`:497`) resolves to the shipped
  defaults in increment 1 (tier-4 keys are increment 2's); the three tenses
  are `Combo_ForeignPairingRequested` (`:509`), `Combo_ComboSettingsFrozen`
  (`:524`), `Combo_ForeignPairingActive` (`:235`).
- **`Combo_ForeignPairingRequested` is implemented but unwired.** It exists at
  `foreign_items.c:77-84` (direction ≠ OFF over the resolved record) and its
  only caller in the tree is `src/common/tests/test_combo_settings.c:200`.
  Nothing reads it before `Fill()`. ADR 0010 :615 describes it as "designed,
  still unimplemented; #493's named gap" — half-true now: the predicate
  exists; the pre-Fill *gate* it was designed for does not.
- Direction gate: both passes read `Combo_ComboDirectionArms(origin)`
  (`Foreign.cpp:532`; `ForeignItemsSingleExe.cpp:421`). Pool size:
  `Combo_ComboPoolSizeFor` clamped to `RSBS_FOREIGN_PLACEMENT_CAP = 8`
  (`context.h:216`). Item class: `Combo_ForeignPoolDrawFor` filters a pool by
  the frozen bitset in pool order (`foreign_items.h:463`; used `Foreign.cpp:566`,
  `ForeignItemsSingleExe.cpp:489`).
- Digest: `Combo_ComputeComboSettingsHash` = FNV-1a over
  `canonical(record) ‖ ":" ‖ LE32(sharedRandoSettingsHash) ‖ ":" ‖ LE32(mmProfileDigest)`
  (`foreign_items.h:581-599`); field-level divergence bits (`:653-720`).

### 3.3 The two placement passes (both post-fill duplicate overlays)

| | Forward (OoT items into MM checks) | Reverse (MM items into OoT checks) |
|---|---|---|
| Function | `Rando::Foreign::PlaceForeignItems` (`Foreign.cpp:493-682`) | `OoT_PlaceForeignItems` (`ForeignItemsSingleExe.cpp:400-541`) |
| When | inside each ladder attempt, after the fill, before the spoiler (`OnFileCreate.cpp:286-294`) | end of `Playthrough_Init`, after the stamp and freeze (`playthrough.cpp:178`) |
| Host predicate | `IsEligibleHost` (`Foreign.cpp:426-466`): Tier-A chest with `FLAG_CYCL_SCENE_CHEST`, shuffled, not skipped, holding a legal `RITYPE_JUNK` item | `OoT_Foreign_IsEligibleHostImpl` (`:309-357`): `ACTOR_EN_BOX` chest, not commerce, fill placed `ITEM_CATEGORY_JUNK` |
| Reachability term | **yes** — `∩ ComputeReachableCheckSet()` (`:594`, `:618`) | **none** — every eligible chest in `1..RC_MAX` order is a candidate (`:447-453`) |
| Draw | pool order under the class filter; hosts by xorshift without replacement | both pool entry and host drawn without replacement (`:524-531`) |
| Record | `Combo_SetForeignPlacement` → `gComboCtx.foreignPlacements[8]` (`context.h:588`) | `Combo_SetForeignPlacementOoT` → `foreignPlacementsOoT[8]` (`:629`) |
| Failure | under-supply is loud, not fatal; table refusal throws (a ladder rung) | `-1`/`-2` return codes propagate through `Playthrough_Init` (`:179-182`); partial is normal |

Neither pass removes anything from an origin pool; both leave the host's own
junk item physically in the table (ADR 0002's degrade invariant). The
asymmetry in the reachability column is a finding: increment 1.3 gated the
forward direction only.

### 3.4 The creation event and where the freeze sits

At `c8947177` the creation event is split across two seams and two times:

1. **OoT generation** (file select, before the file exists):
   `Fill()` (`playthrough.cpp:87`) → spoiler (`:100`) → pairing stamp
   (`:125-127`) → `mmProfileDigest` (`:140`) → combo record resolve + freeze
   (`:157-163`) → reverse pass (`:178`). The MM profile digest and the combo
   record are therefore frozen **after** OoT's `Fill()`, not before it. ADR
   0009 D2's amendment requires "the MM option profile freezes BEFORE OoT's
   `Fill()`" (`0009:251-264`); ADR 0011 D3.5 measured the same gap at
   `91bc133b` and it is unchanged (`0011:514-533`).
2. **OoT file create** (`z_sram.c:320`, `:340`).
3. **MM generation** at first arrival (`GameExports_SingleExe.cpp:3061`,
   dispatch ≈`:3288`) or at the headless harness, under the ladder, with
   compare-and-refuse against the stamped digest (`Foreign.cpp:263-285`;
   arrival gate ≈`:3161-3172`).

ADR 0010 increment 2 collapses 1–3 into one seam at `z_sram.c:306`-ish with
the freeze before `Fill()`; increment 3 requires that. Until then, OoT's
placement pass cannot read MM's option *values* (only a digest —
`MM_Rando_ComputeProfileStamp` publishes none, `Foreign.cpp:765-769`), which
is why criterion 3 stays blanket (ADR 0011 D3.5).

### 3.5 The headless generation path the rando tier uses

`Test_RandoDeterminism` (`src/common/test_runner.cpp:858-…`) runs
`Rando_HeadlessSeedDeterminismDigest` (OoT generation + digest) and then
`MM_Rando_HeadlessForeignDigest` (`:887`) in **one process with MM never
booted and no MM archives mounted**: the MM harness calls `MM_Rando_Init`
for the registrars, stages a pre-`OnSaveInit` save state, and dispatches
`GameInteractor_ExecuteOnSaveInit(0)` (`mm_rando_gen_test.cpp:1044`) — the
real `OnFileCreate` chain. The rows are registered at
`test_runner.cpp:2385-2410` and need a display (`Fast3dWindow` bring-up), so
`--test all` skips them (`:2771-2780`); CTest runs them under the `rando`
label (`CMake/SingleExecutable.cmake:840`, `:953`, `:963`, `:1102`).
ADR 0010's "feasibility is settled" claim for increment 2 (`:579-582`) holds:
both generations already run in one process from one master seed.

### 3.6 The determinism digests that pin all of it

| Row | Anchors | Folds |
|---|---|---|
| RandoDeterminism / SeedDeterminism (two-process) | `test_runner.cpp:858`; `menu.cpp:508`; `CMake/CheckSeedDeterminism.cmake` | OoT `seed`, `settingsHash`, `placementHash`, `placedCount`, `foreignOoTHash/Count`, `comboSettingsHash`; MM `mmFinalSeed`, `mmPlacementHash`, `mmReachableCount/Hash`, `mmPairedAttempt`, `foreign0..3` |
| MMRandoGen | `mm_rando_gen_test.cpp`; `SingleExecutable.cmake:963` | placement-in-closure (FAIL 30), premise (29), coverage (33), closure purity + save byte-identity (28), under-supply (31/32) |
| MMPairedAttempt / MMPairedAttemptDeterminism | `:2373`; `CMake/CheckPairedAttemptDeterminism.cmake` | injected rung → attempt 1; two-process byte-diff of seed + attempt + placements |
| MMPairedExhaustion | `test_runner.cpp:1212` | 10-rung exhaustion refuses; timeout does not climb |
| MMPairSwitchEntry | `:1164` | real cold-boot arrival + placements-in-closure |
| ForeignPlacementOoT | `SingleExecutable.cmake:953` | reverse pass on the real fill |

Any change to either fill's traversal, RNG consumption order, or the
placement passes re-pins these; ADR 0010 budgets two re-pins (`:759-762`).

---

## 4. Composition assessment (Decision 4's default)

### 4.1 The coordinator contract, concretely

The coordinator owns three things and nothing else: the **union bag**
(origin-tagged items not yet placed), the **placement** (host check → item,
origin-tagged both ways), and the **round loop**. It never evaluates a guard,
never names an `RG_*`/`RI_*`/`RR_*`, and never touches a save. Each engine
exports a C-linkage query surface over game-neutral scalars:

```
// OoT side (new TU beside ForeignItemsSingleExe.cpp; RG_/RC_ in scope only here)
int      OoT_Logic_BeginQuery(void);                 // Logic::Reset(true): DETACH the save, AccessReset, starting inventory
void     OoT_Logic_AssumeOwnItem(uint16_t rgId);     // Item::ApplyEffect into the detached save (assumed-fill "have it")
int      OoT_Logic_Expand(void);                     // one ReachabilitySearch(allLocations); returns 1 if anything new opened
int      OoT_Logic_CrossingOpen(void);               // RegionTable(<mask-shop region>)->Child()  [4.5]
int      OoT_Logic_CheckReached(uint16_t rc);        // ItemLocation::IsAddedToPool()
int      OoT_Logic_ReachedEmptyHosts(uint16_t* out, int cap); // accessible empties (assumed-fill candidates)
int      OoT_Logic_GoalReached(void);                // RG_TRIFORCE reached (CheckBeatable's condition)
void     OoT_Logic_Place(uint16_t rc, SharedItem it);// OoT-origin: PlaceItemInLocation; MM-origin: junk cover + table entry
void     OoT_Logic_EndQuery(void);                   // (nothing today; the detach already isolated)

// MM side (new TU beside Rando/ForeignItemsSingleExe.cpp; RI_/RC_/RR_ in scope only here)
int      MM_Logic_Snapshot(void);                    // memcpy gSaveContext + MM_GameEvents_Queue depth  [4.4]
void     MM_Logic_Restore(void);
void     MM_Logic_AssumeOwnItem(uint16_t riId);      // GiveItem(ConvertItem(ri)) into the (snapshotted) live save
int      MM_Logic_Expand(void);                      // CrawlReachableRegions(SCT arrival) + EvaluateReachableChecks; 1 if new
int      MM_Logic_CrossingOpen(void);                // RR_CLOCK_TOWER_INTERIOR ∈ reachable  [4.5]
int      MM_Logic_CheckReached(uint16_t rc);
int      MM_Logic_ReachedEmptyHosts(uint16_t* out, int cap);
int      MM_Logic_GoalReached(void);                 // RR_MOON_MAJORAS_LAIR ∈ reachable && <Majora predicate>  [2.11.4]
void     MM_Logic_Place(uint16_t rc, SharedItem it); // MM-origin: RANDO_SAVE_CHECKS write; OoT-origin: junk cover + table
```

The boundary carrier is `SharedItem` plus a host check id plus the origin of
the host — the same three scalars accepted answer O7 sizes the carve for.
`AssumeOwnItem`/`Place` receive only their **own** origin's ids (the
coordinator routes by `item.originGame`), which is ADR 0002 holding by
construction: MM reports "check 201 reached"; the coordinator looks up
`placement[MM][201] = SharedItem{OOT, RG_LENS_OF_TRUTH}` and calls
`OoT_Logic_AssumeOwnItem(RG_LENS_OF_TRUTH)`. No MM code ever sees the `RG`.

### 4.2 What each engine has and what it lacks

| Needed export | OoT today | MM today |
|---|---|---|
| Pure reachability over (world state + granted items) | `ReachabilitySearch` after `Logic::Reset(true)` + `ApplyEffect` — exists; the detach is the purity (`fill.cpp:884-893`) | closure exists (`Logic.cpp:283-331`) but "granted items" means "give into the live save"; needs the snapshot/give/crawl/restore wrapped as one call |
| Region reachability | region bits, `Region::Child()/Adult()` (`location_access.h:171-176`) | `crawl.reachableRegions` (`Logic.h:145-148`) |
| Per-check reachability | `ItemLocation::IsAddedToPool` (`item_location.h:14`) | `EvaluateReachableChecks` (`Logic.cpp:267-281`) |
| Sphere index | only post-fill via `playthroughLocations`; a per-round counter in the coordinator suffices for increment 3 | none; same coordinator counter |
| Goal predicate | `CheckBeatable` (`fill.cpp:589-604`) | **missing** the Majora half (§2.11.4) |
| Crossing observable | `RegionTable(RR_MARKET_MASK_SHOP)->Child()` | `RR_CLOCK_TOWER_INTERIOR ∈ reachable` |
| Item effect of own item | `Item::ApplyEffect` (`logic.cpp:1736`) | `GiveItem(ConvertItem(ri))` (`Logic.cpp:320`) |
| Placement write | `Context::PlaceItemInLocation` (`SeedContext.cpp:137-153`) | `RANDO_SAVE_CHECKS[rc].randoItemId/shuffled` (`GlitchlessLogic.cpp:156-157`) |
| Junk cover for a foreign host | `ITEM_CATEGORY_JUNK` placement (`ForeignItemsSingleExe.cpp:351-356`) | `RI_JUNK`-class placement (`Foreign.cpp:457-465`) |

### 4.3 The alternating expansion to a fixpoint

One reachability round under a partial placement `P` and an assumed set `A`
(everything in the bag not yet placed):

```
OoT_Logic_BeginQuery();  for a in A ∩ OoT-origin: OoT_Logic_AssumeOwnItem(a)
MM_Logic_Snapshot();     for a in A ∩ MM-origin:  MM_Logic_AssumeOwnItem(a)
repeat
    changed  = OoT_Logic_Expand()
    if OoT_Logic_CrossingOpen(): for each OoT host h reached with P[OoT][h].origin == MM: MM_Logic_AssumeOwnItem(P[OoT][h].id)
    changed |= MM_Logic_Expand()
    if MM_Logic_CrossingOpen():  for each MM host h reached with P[MM][h].origin == OOT: OoT_Logic_AssumeOwnItem(P[MM][h].id)
until !changed
read: goal = GOAL(OoT_Logic_GoalReached(), MM_Logic_GoalReached()); candidates = both ReachedEmptyHosts
MM_Logic_Restore()
```

Both operators are monotone in their own state (§1.2, §2.3 — no state
negations in either dialect), the exchanged facts are monotone (a granted
item is never withdrawn within a round), and the lattice is finite, so the
loop terminates; with today's graphs it will converge in two or three
alternations because the crossing opens at sphere zero under default OoT
settings (child start reaches the Market) and MM's crossing region is
reachable from its root unconditionally. This is exactly ADR 0010 D4's
"fixpoint-of-two-monotone-operators" shape; nothing in either engine has to
know it is participating.

**The single-bag assumed fill on top** is OoT's own `AssumedFill` loop
(`fill.cpp:877-934`) lifted one level: pop an item from the union bag, run
the round above with the rest assumed, pick a host uniformly from the union of
both `ReachedEmptyHosts`, `Place` it on the owning side, repeat; on an empty
candidate set roll back the batch (OoT's 10-retry discipline). The exit
condition is the GOAL expression under the frozen rung (D2.3: "the guarantee
is the fill's exit condition"), and under `RSBS_COMBO_RUNG_NONE` the round is
skipped and hosts are drawn from all empties (the operator's "bag → randomly
distribute" base mode as the same code path, D5). The trailing `FastFill` of
each game's junk stays per-game.

### 4.4 What must be snapshotted/restored around MM

Per round (not per query — one snapshot brackets the whole round because
`AssumeOwnItem` gives into the live save):

- `gSaveContext`, whole struct (`sizeof(SaveContext)` ≈ 48 KB per
  `game.h:49`; heap-allocated as the closure does, `Logic.cpp:289`). This
  covers inventory, `RANDO_EVENTS`, `RANDO_SAVE_CHECKS` (the fill's own
  placements are written here — so **placements made in a round are
  re-applied after restore**, exactly as `GlitchlessLogic.cpp:276-282` does),
  `entrance`, `playerForm`.
- `MM_GameEvents_Queue` depth (`Logic.cpp:295`, `:327-329`).
- Nothing else was found: `GiveItem`/`ConvertItem` consume no `Ship_Random`
  (locked by MMRandoGen FAIL(28)); `gCurrentRegionTime` is rewritten per
  region; `GetRegionIdFromEntrance`'s cache is immutable after the first call.
  Whether the give path touches state outside the save in any branch the
  closure has not yet exercised is a §6.3 item.

On the OoT side the discipline is the detach (`Logic::Reset(true)`), plus one
rule the coordinator must enforce: if an OoT save is loaded when the
coordinator runs (increment 2's seam runs at file create, before
`Save_SaveFile`), `Logic::mSaveContext` must be re-pointed at `&gSaveContext`
afterwards or the next in-game progressive give resolves against a stale
simulated save (§1.8 hazard 1).

### 4.5 The crossing edge as a requirement edge

The crossing is a runtime link (`src/common/entrance.h:27-49`,
`Entrance_RegisterDefaultLinks` `:134`): OoT entrance `0x0530` (Happy Mask
Shop door) ↔ MM entrance `0xC010` (`ENTRANCE(CLOCK_TOWER_INTERIOR, 1)`,
the MM→OoT trigger), with MM arrival at `0xD800`
(`ENTRANCE(SOUTH_CLOCK_TOWN, 0)`). Mapped onto the two graphs:

- **OoT side.** The Market → `RR_MARKET_MASK_SHOP` exit is guarded by
  `logic->IsChild && logic->AtDay && logic->CanOpenOverworldDoor(RG_MASK_SHOP_KEY)`
  (`location_access/overworld/market.cpp:37`); the region is
  `areaTable[RR_MARKET_MASK_SHOP]` (`:140`). The OoT→MM edge is therefore a
  new exit on that region whose condition is `logic->IsChild` (the shop is
  child-only, day-entered) and whose target is *foreign*: the coordinator's
  observable is `RegionTable(RR_MARKET_MASK_SHOP)->Child()`. Under OoT's own
  entrance shuffle the interior is an `Interior`-type shuffleable pair
  (`entrance.cpp:300-301`), so the epic must either pin that pair out of the
  shuffle pool or resolve the crossing region through the shuffled
  replacement; ADR 0010 D11 excludes cross-game ER, not OoT's own.
- **MM side.** `RR_CLOCK_TOWN_SOUTH → RR_CLOCK_TOWER_INTERIOR` is `EXIT(ENTRANCE(CLOCK_TOWER_INTERIOR, 1), …, true)`
  (`Regions/Central.cpp:204`) and back (`:70`), and `0xD800` resolves to
  `RR_CLOCK_TOWN_SOUTH` (its return entrance at `:204`; also the root's
  save-warp exit, `Logic.cpp:341`). The MM→OoT edge is a new exit on
  `RR_CLOCK_TOWER_INTERIOR` (guard `true`) whose target is foreign; the
  observable is `RR_CLOCK_TOWER_INTERIOR ∈ reachableRegions`.
- **O2's Day-1 re-stamp and reset-time guard.** In this model they are
  already implied: MM's crawl seeds from `RR_MAX` at Day 1 06:00
  (`Logic.cpp:62-64`, `:154`) and every region reachable from the root is
  reachable *after a cycle reset*, since `CONNECTION(RR_MAX, true)` on South
  Clock Town (`Central.cpp:215`) makes the reset free. An OoT→MM arrival
  therefore lands in exactly the state a save-warp would, and "can reset time"
  has no MM-graph analogue to guard — MM never models being unable to return
  to Day 1. The return trip requires reaching `RR_CLOCK_TOWER_INTERIOR`, which
  is unconditional from the root. The one authored term is on the OoT side
  (`IsChild`), plus whatever time-of-day stamp the epic chooses for the
  return landing in the Market (the Market is a `timePass` region under
  child access in practice; this audit did not verify the flag).

### 4.6 The single-bag fill's interaction with OoT's ordered passes

OoT's `Fill()` runs six restricted-pool assumed fills before the general one
(§1.3). Under one bag those pools are settings-defined subsets ("own dungeon",
"song locations", shop slots) that have no MM counterpart; the honest reading
of D3 is that the **union bag is the last general pass** — the
`remainingAdvancementItems` fill at `fill.cpp:1405-1407` plus MM's whole
shuffled pool — while OoT's restricted passes keep running first, per-game,
exactly as now. That keeps every dungeon-item setting meaningful and confines
the OoT-side seam to one `#ifdef RSBS_SINGLE_EXECUTABLE` block replacing
`:1405-1414`. MM's forward fill is then not run for a paired world: its job
(placing MM's shuffled pool) is done by the coordinator, and its
all-locations-reachable guarantee is replaced by the coordinator's rung. The
decision "which OoT pools join the bag" is the epic's; this audit records that
the code shape makes "only the general pass" the cheap answer.

### 4.7 Which TU owns the boundary under ADR 0002

ADR 0002's rule is "no raw game-local id crosses a game boundary outside a
`SharedItem`" (`0002:57-65`), realised as: pool tables live in the one TU per
game where the enum is in scope, `src/common` includes no game header, and
ADR 0010 answer O8 / ADR 0011 §3.4 name `shared_items.h/.c` as the sanctioned
game-header-free pair for classification (`0011:499-512`). Applied here:

- The **coordinator** (bag, placement, round loop, GOAL/rung evaluation over
  two booleans) is a new game-header-free TU in `src/common/` (working name
  `combo_logic.c/.h`), the twin of `foreign_items.c` — it consumes the O8
  classification table for "is this bag item progression or junk" and the
  frozen record for GOAL/rung/direction/classes.
- Each engine's query surface is `extern "C"` and lives in that game's
  single-exe TU (`games/oot/soh/Enhancements/randomizer/` and
  `games/mm/2s2h/Rando/`, beside the pool TUs), declared game-neutrally in the
  `src/common` header. That is precisely the pattern `OoT_PlaceForeignItems`
  / `MM_Rando_Foreign_*` already follow (`foreign_items.h:826-890`;
  `Foreign.cpp:793-804`).
- The bag's item universe is the two registered pools generalised: every
  advancement item each engine is willing to let leave, published as
  `ComboForeignItemDef`-shaped rows with `itemClass` — which is also where
  criterion 3's profile-conditional narrowing lands once the freeze precedes
  `Fill()` (ADR 0011 D3.5).

### 4.8 Estimated change footprint (composition)

| Where | What | Size |
|---|---|---|
| `src/common/combo_logic.{c,h}` (new) | bag, placement (two origin-keyed tables, sized per O7 against `reserved[124]`), round loop, assumed-fill driver, GOAL/rung evaluation, spoiler section | ~600–900 LOC + a ROM-free lock for the loop over a synthetic pair of stub engines |
| OoT export TU (new) | the nine functions in 4.1 over existing primitives | ~150–250 LOC |
| `3drando/fill.cpp` | one `#ifdef` seam replacing `:1405-1414` for paired worlds; export of the leaving advancement items | ~40–80 LOC diff |
| `3drando/playthrough.cpp` | freeze moves before `Fill()` (increment 2, already owed) | ~30 LOC |
| MM export TU (new) | snapshot/restore, assume, expand, goal, place; Majora predicate | ~150–250 LOC |
| `Rando/MiscBehavior/OnFileCreate.cpp` | paired branch dispatches to the coordinator instead of `ApplyGlitchlessLogicToSaveContext` (`:235-236`); solo path untouched | ~40 LOC diff |
| `Rando/Logic/Logic.cpp` | `FindReachableRegions` consumer swap for #585 (or retire it) | ~5 LOC + re-pin |
| Locks | the D5 pair-level lock (host in MM only; remove → unprovable), digest re-pins | test code |
| **Total** | | **≈1.5–2 k LOC**, both port trees touched at one seam each |

---

## 5. Unification assessment

What a single merged engine would have to become, read off the two models:

- **One region type carrying both lattices.** OoT's state is 2⁴ per region
  (age × time-of-day, `location_access.h:149-152`); MM's is 2⁴⁵ time slices ×
  `canStayOverTime` per region (`Logic.h:99-102`), plus `STAY` restrictions
  and clock ownership. They are not the same dimension with different
  resolution — OoT's "time" is a two-valued attribute propagated through
  `timePass` regions with a global write to `RR_ROOT` (`location_access.cpp:437-444`),
  MM's is a forward-only expansion over a cycle that resets for free. A merged
  region needs both, tagged by which world it belongs to (D2.3's constraint
  bitsets), and every evaluator must know which to consult.
- **One condition dialect.** OoT lambdas read `logic->` helpers over a
  detached save with the four age/time flags set by the caller; MM lambdas
  read `gSaveContext` macros with `gCurrentRegionTime` set by the caller.
  Unifying means either porting 5,878 LOC of MM guards to the OoT helper style
  over a detached MM save (which does not exist and would need the ~1.5 k LOC
  give path re-targeted or re-implemented), or porting 12,427 LOC of OoT
  guards into MM's macro style over a live save (and abandoning the detach,
  which the ADR rightly calls unacceptable). #578 already found all three
  reference graphs "structurally incompatible" with 2ship's registrar graph
  at the data level.
- **One trick table**: 266 `RT_*` rows plus MM's ~62–84 (#578), game-tagged —
  the easy part.
- **One fill**: assumed fill generalises (it is what §4.3 does anyway); MM's
  forward fill and `RemoveItem`-based backtracking retire.
- **What it retires**: one of the two crawls, one fill, NNL/NoLogic, one
  spoiler writer. **What it does not retire**: both check trackers consume
  their engine's own query (`randomizer_check_tracker.cpp:2185`;
  `CheckTracker.cpp:255`), OoT's hint system consumes `ItemLocation` areas and
  the playthrough spheres, MM's spoiler-load path consumes `RANDO_SAVE_CHECKS`
  — every UI and load path of the retired engine has to be re-wired to the
  survivor.
- **Footprint**: ≥ 6–7 k LOC of hand-translated logic plus engine and UI
  re-wiring, a permanent divergence from both ports' region files (every
  upstream logic fix becomes a manual re-port), and a one-time re-pin of every
  digest. Against that, the maintainability gain is one dialect for the
  cross-game *edges* — of which there are two.

The ADR permitted unification "if that results in an easier to maintain end
product". Measured against the code, the end product is not easier to
maintain: the two dialects are not incidental styling, they encode two
different world-state models, and the crossing between them is two edges.

---

## 6. Recommendation for O4, prerequisites, and limits

### 6.1 Recommendation

**Composition, as ADR 0010 D4 defaults, with the contract in §4.1 and three
amendments the code forces:** (1) the coordinator's round brackets *all* MM
calls in one snapshot/restore and re-applies MM placements after restore, the
way the fill already does, because every MM query — the "read-only" crawl
included — writes the live save; (2) the union bag is the last general pass
only, with OoT's restricted-pool passes and both games' junk `FastFill`s
staying per-game, and MM's forward fill not running for paired worlds; (3)
the crossing observables are region facts (`RR_MARKET_MASK_SHOP` child
access; `RR_CLOCK_TOWER_INTERIOR` reachability) rather than new graph edges,
so neither port's region files change for increment 3. Unification is not
recommended: the two dialects encode different state lattices (§5), and the
port cost buys nothing the two crossing edges need.

### 6.2 Prerequisites before increment 3 (with the ADR term each discharges)

| # | Prerequisite | ADR term | Status at `c8947177` |
|---|---|---|---|
| P1 | Move the MM profile freeze and the combo-record freeze **before** OoT's `Fill()`, at one creation seam; publish MM option *values* to `src/common` | ADR 0009 D2 amendment (`0009:251-264`); ADR 0011 D3.5/O8; ADR 0010 increment 2 | not done: freeze sits at `playthrough.cpp:140-163`, after `Fill()` (`:87`) |
| P2 | Wire `Combo_ForeignPairingRequested()` as the pre-Fill gate | ADR 0009 D2; ADR 0010 `:615` | implemented (`foreign_items.c:77-84`), test-only caller |
| P3 | Fix or retire the fill's `FindReachableRegions` (join discipline) | ADR 0010 D2.3 (`:360-367`); #585 | crawl fixed, fill not; global re-pin owed |
| P4 | Author `MM_GOAL`'s "Majora defeated" predicate; expose lair reachability | ADR 0010 D1/D2 (`:186-200`) | missing (`Moon.cpp:68`; no boss row) |
| P5 | Export surfaces per §4.1 on both sides, incl. the OoT detach/re-attach rule | D4 "each engine's exported query surface" (`:683`) | none exist |
| P6 | Reachability-gate the reverse placement pass | increment 1.3 (`:535-540`), applied to both directions | forward only (`Foreign.cpp:594`); reverse ungated (`ForeignItemsSingleExe.cpp:447-453`) |
| P7 | Decide O5 (45 vs 46 slices) inside a re-pin increment | O5 (`:672`) | 45; no consumer needs the 46th today |
| P8 | O6 tooling: the static negation probe and the CI grow-check | O6 (`:673`) | baseline is clean in both dialects (§1.2, §2.3); nothing locks it |
| P9 | Gate the Powder-Keg disjunct so `beatable(T = ∅)` means tricks-off | O11 (`:677`); #578 finding (a) | unconditional (`Logic.h:289-291`) |
| P10 | MM per-trick vocabulary lands so T freezes into identity | §3.3, O9; #578 | absent |
| P11 | One spoiler artifact for the pair | #564 V23 (`:764-765`) | two artifacts (§0) |
| P12 | Measure the linked fixpoint's creation-time cost against the ~30 s floor | Consequences (`:746-758`); #582 | unmeasured (§6.3) |
| P13 | `sRandoInitDone` core/asset split | increment 2 (`:571-575`) | single guard (`GameExports_SingleExe.cpp:1411-1415`) |
| P14 | Decide which OoT restricted pools join the bag (recommendation: only the general pass, §4.6) | D3 "one bag" | open |
| P15 | Pin or resolve the Happy Mask Shop interior under OoT's own entrance shuffle | §2.1 crossing edge; D11 | shuffleable today (`entrance.cpp:300-301`) |

### 6.3 What this audit could not determine statically

- **Runtime cost** of one linked round (OoT search + MM snapshot/give/crawl)
  and how many rounds a union-bag assumed fill needs per item; only
  measurement against the #582 budget can say whether increment 3 fits the
  ~30 s floor. Order-of-magnitude reasoning: OoT's search is milliseconds; the
  MM closure ran inside sub-second CI rows in #580; the fill will call the
  round O(#bag items) times.
- **Dead-end behaviour** of an assumed fill over MM's graph. MM's regions were
  authored for a forward fill; whether the union-bag fill converges without
  retries on the shipped profile is empirical.
- Whether `GetRegionIdFromEntrance`'s first-call cache can be populated before
  the registrars run on any real path (§2.10) — a static read cannot prove an
  ordering.
- Whether any branch of `GiveItem`/`ConvertItem` reachable only with foreign
  items present touches state outside `gSaveContext` (the closure's
  exercised branches are locked; the coordinator's assumed set is wider).
- Whether the OoT `Logic` ever runs with `mSaveContext == &gSaveContext`
  during a creation-seam generation (increment 2's seam runs after
  `Context_InvalidateSessionOnNewGame`; the audit could not see the load
  order).
- Whether MM's crawl can run **without a display** (#500 work item 1) — the
  rando tier still runs under a display; a windowless crawl would make a
  ROM-free lock for the coordinator possible.
- The Market's `timePass` flag and the right time-of-day stamp for the return
  landing (§4.5).
- `Logic::NewSaveContext`'s `new`/`free` mismatch is UB by the letter; whether
  it can bite under any supported toolchain is not decidable from source.

### 6.4 Candidate issues (for the orchestrator to file; none filed here)

1. Reverse placement pass has no reachability gate — increment 1.3's
   asymmetry (`ForeignItemsSingleExe.cpp:447-453`). (P6)
2. `Combo_ForeignPairingRequested` is implemented but has no production
   caller; the pre-Fill gate ADR 0009 D2 designed is unwired
   (`foreign_items.c:77-84`). (P2)
3. `MM_GOAL` has no "Majora defeated" predicate (`Regions/Moon.cpp:68`;
   `Logic.h:692-850`). (P4)
4. `GetRegionIdFromEntrance` caches an empty map if first called before the
   registrars run (`Logic.cpp:18-39`) — probe the init order.
5. Two spoiler artifacts per pair against #564 V23's one-artifact
   requirement. (P11)
6. O5 carrier: the consumer list for a 46th time slice (`Logic.h:75`,
   `:89-91`; `TimeLogic.cpp:18-31`). (P7)
7. Happy Mask Shop interior is in OoT's `Interior` shuffle pool; the crossing
   region must be pinned or resolved dynamically (`entrance.cpp:300-301`). (P15)
8. ADR 0010 line anchors have drifted (`GlitchlessLogic.cpp:22/:57/:64/:258` →
   `:37-38`, `:78`, `:87-89`, `:282`; `Logic.cpp:92-136` → `:95-139`) — a doc
   fix so the increment-3 epic is not led to the wrong lines.
9. Inherited: `Logic::NewSaveContext` frees `new`-allocated memory
   (`logic.cpp:2312-2314`). Record only; no upstream report.

---

## Appendix A — Claims in ADR 0010 / #500 / #580 / #581 / #585, verified

| Claim | Source | Holds at `c8947177`? |
|---|---|---|
| OoT: static region graph with 4-state age/time reachability | ADR 0010 `:58-60`; #500 | **Yes** — `location_access.h:149-152`, `:243` |
| OoT: assumed fill, `CheckBeatable` (`fill.cpp:589-604`), `IsBeatableWithout` (`:300-308`) | `:60-61` | **Yes**; `IsBeatableWithout` is `:301-312` |
| OoT logic is headless and save-independent (`NewSaveContext`, `logic.cpp:2314`; `Reset(true)` at `:2647-2650`) | #500 | **Yes** with the §1.8 caveat; `Reset` is now `:2655` |
| MM: registrar-built region map, `FindReachableRegions` at `Logic.cpp:92-136`, 45-slice model at `Logic.h:20+` | `:63-66` | **Yes**; function is `:95-139`, enum `:23-69` |
| MM has **no beatability predicate** anywhere | `:65-66`; #500 | **Yes** — `grep -rn Beatable games/mm/2s2h/` is empty; lair reachability is derivable, Majora defeat is not (§2.11.4) |
| MM Glitchless mutates the live `gSaveContext` (`:22`, `:57`, `:258`) under a 10 s abort (`:64`) | `:66-69`, `:780-784` | **Yes**; anchors drifted to `:37-38`, `:78`, `:282`, `:30`/`:87-89` |
| Paired MM defaults to Nearly No Logic (`Foreign.cpp:126-128`) | `:69-72` | **No longer** — flipped to Glitchless by #581 (`Foreign.cpp:168-173`) |
| A failed paired generation reverts to a silent vanilla Termina | `:73-75` | **Partly superseded** — `OnFileCreate.cpp:459-474` still reverts; the arrival gate now refuses loudly (`GameExports_SingleExe.cpp` ≈`:3318`); increment 2 removes the revert |
| `FindReachableRegions` guards on first visit and overwrites `regionTimeStates`; "fixed as part of increment 1's factoring" | `:360-367` | **Bug confirmed** (`Logic.cpp:115`, `:119-120`, `:129`, `:133-134`); **fix is partial by design** — the join lives in the new crawl, the fill still uses the old one (#585) |
| No cross-game fact is computed from the buggy crawl | #580 | **Yes** — the gate reads `ComputeReachableCheckSet` (`Foreign.cpp:594`) which uses `CrawlReachableRegions` (`Logic.cpp:301`) |
| `RANDO_EVENTS` is a count and per-id dedup capped `>= n` gates | #580 | **Yes** — `z64save.h:384` (u8), `West.cpp:215`, per-registration keying `Logic.cpp:179` |
| The ladder does not re-roll a wall-clock abort | #581 | **Yes** — `OnFileCreate.cpp:348-373` catches `GenerationTimeout` before `std::exception` |
| Attempt 0 is byte-identical to the pre-ladder derivation | #581 | **Yes** — `Foreign.cpp:327-332` appends the tag only for `attempt > 0` |
| `RO_ACCESS_MAJORA_REMAINS` retired in place, never renumbered | O1 `:669`; #581 | **Yes** — `Options.cpp:23-44`, `Types.h:2875` |
| `Combo_ForeignPairingRequested` is "designed, still unimplemented" | `:615` | **Stale** — implemented at `foreign_items.c:77-84`, unwired |
| OoT's 425 trick consults under `location_access/` | `:405-407` | **Yes** — exactly 425 |
| MM has no per-trick option; TODO seams at `Central.cpp:365`, `MilkRoad.cpp:29` | `:411-418` | **Yes** |
| Increment 2 feasibility: one process runs OoT generation plus full paired MM generation with MM never booted | `:579-582` | **Yes** — `test_runner.cpp:876-887`; `mm_rando_gen_test.cpp:1004-1044` |
| The MM profile freeze sits after `Fill()` and `ComputeProfileStamp` publishes no values | ADR 0011 D3.5 | **Yes** — `playthrough.cpp:87`, `:140`; `Foreign.cpp:765-769` |
| Both placement passes read the frozen direction and class | ADR 0011 inc. 3/4 | **Yes** — `Foreign.cpp:532`, `:566`; `ForeignItemsSingleExe.cpp:421`, `:489` |
| Every foreign placement is in the reachable closure | #580 | **Forward only** — the reverse pass has no such term (§3.3) |
