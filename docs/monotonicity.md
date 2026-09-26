# Monotonicity enforcement (ADR 0010 answer O6)

The combo fill's guarantee is the least fixed point of a **monotone** operator
([ADR 0010 §2.3](adr/0010-cross-game-logic-and-beatability.md)). Assumed
(reverse) fill "pretends the player has" every item not yet placed, and the
trailing no-logic dump places the rest anywhere. Both are sound only if gaining
an item can never take reachability away. OoTMM enforces this by construction:
its expression builder throws on `!` over `has()`/`event()`. Redship's
conditions are C++ lambdas in two dialects, so ADR 0010 answer O6 requires
**all three** of the mechanisms below. It is not a choice among them.

| Mechanism | Where | Runs | Sees |
|---|---|---|---|
| The review rule | `.claude/worker-prompts.md` (Standing conventions) and this page | every review | intent: a condition written as "the player lacks X" |
| The static probe | `.github/scripts/check-monotonicity-negations.py` + `.github/monotonicity-negation-baseline.txt` | CI job `monotonicity-probe` (build-free, `generate-builds.yml`) | the condition **source text**, with file:line |
| The CI grow-check | `src/common/tests/test_combo_logic_monotonicity.c`, CTest `ComboLogicMonotonicity` (`rando` tier) | every CI run of the rando tier, and the local gate | the **operator itself**, through both real engines |

The probe is cheap and names a line. The grow-check catches what source text
cannot show: helper bodies (OoT's `logic.cpp`, MM's `Logic.h`), give-path
arithmetic (the wallet wrap the multiplicity clamp exists for), event counters,
MM's time join, and anything a macro hides.

## The review rule

> **Reachability conditions never negate player state.** In either graph (OoT
> `location_access/**`, MM `Logic/Regions/**`, MM `Logic.h`, and the helpers
> those call), a condition may REQUIRE an item, event, flag, count, age or time,
> but never its ABSENCE. That rules out `!` over a player-state term, `< k`,
> `<= k` or `== 0` on a count, `== *_NONE` on an inventory slot, and
> `if (HAS_X) return false;`. Negating a setting, trick or option
> (`!ctx->GetOption(...)`, `!RANDO_SAVE_OPTIONS[...]`, `!MM_TRICK(...)`) is legal,
> because it is constant for the whole fill. A new static-probe baseline entry
> needs a written reading proving the site monotone. Rewriting the condition
> positively is the preferred fix.

"Before event X" is the usual way a violation arrives: "the NPC is here only
until you finish the dungeon". Model that as a separate time/region state
(OoT's age and time-of-day bits, MM's time slices and `STAY` restrictions), not
as `!event`. ADR 0010 §2.3's "exclusive world states are per-path constraint
accumulation, not state queries" is the rule. The upstream ports already follow
it: neither graph negates player state at the baseline below.

## The static probe

```
python3 .github/scripts/check-monotonicity-negations.py            # the gate
python3 .github/scripts/check-monotonicity-negations.py --list     # every classified site
python3 .github/scripts/check-monotonicity-negations.py --self-test
```

**What it scans:** `games/oot/soh/Enhancements/randomizer/location_access/**`,
`games/mm/2s2h/Rando/Logic/Regions/**` and `games/mm/2s2h/Rando/Logic/Logic.h`.
Comments and string literals are blanked first, with offsets kept so lines
still map.

**What it reports:** every negation (`!term`, never `!=`) and every relational
comparison, classified by what the term reads:

- **PLAYER-STATE:** inventory, events, flags, counters, age/time context, region
  access. For OoT that is `logic->…` (except an allowlisted settings-only member),
  `Here`, `AnyAgeTime`, `CanPlantBean`, `GetWalletCapacity`, and `RG_`/`LOGIC_`/`RR_`
  ids. For MM that is `HAS_*`, `CAN_*`, `KEY_COUNT`, `CUR_UPG_VALUE`, `Flags_Get*`,
  `RANDO_EVENTS`, `CHECK_QUEST_ITEM`, the time operators, `ITEM_`/`RI_`/`RE_` ids,
  and so on.
- **SETTING:** `ctx->GetOption`, `GetTrickOption`, `RSK_`/`RT_`/`RO_`,
  `RANDO_SAVE_OPTIONS`, `MM_TRICK`/`MMRT_`, `SettingClocks`, and check prices
  (`GetCheckPrice`, `RANDO_SAVE_CHECKS[..].price`), which are frozen at creation.

| Class | Meaning | Gate |
|---|---|---|
| `VIOLATION-NEGATION` | `!` over a player-state term | fails unless baselined |
| `VIOLATION-COMPARE` | `state < k`, `state <= k`, `state == 0`, `state == *_NONE` ("true only while the player has little") | fails unless baselined |
| `VIOLATION-GUARD` | `if (STATE) return false;`: player state that forces false | fails unless baselined |
| `NEEDS-READING` | an unknown negated term, or `state == k` / `state != k` with k > 0 (monotone only if k is the maximum) | fails unless baselined |
| `LEGAL-SETTING` | negation of settings only | reported with `--list` |
| `LEGAL-GUARD` | `if (!STATE) return false;` means "requires STATE", which is monotone | reported with `--list` |

Inside `if (C) return false;` the polarity of every term flips. That is why MM's
`if (RANDO_SAVE_OPTIONS[RO_SHUFFLE_ENEMY_SOULS] && !HaveEnemySoul(e)) return false;`
is legal.

**The baseline** (`.github/monotonicity-negation-baseline.txt`) has one line per
accepted site: `path | class | term | reading`. The key is the path, class and
normalized term, not the line number. An entry with an empty reading is refused
(exit 2). Every accepted entry is printed with its reading on every run, so the
baseline records each reading in the log rather than hiding the site. An entry
that no longer matches any site is **stale** and fails the gate, so a fix
removes its entry in the same change.

**Anti-vacuity:** the scan exits 2 if either tree yields implausibly few files or
condition sites. It also exits 2 if the classifier finds almost none of the
settings negations the OoT region files are known to contain. `--self-test`
plants sixteen violations in fixture trees (negations of items, events, region
access, age, MM flags, event counters and time operators; downward comparisons;
a state-forced false; an equality on a count) and asserts that:

- each one is classified and fails the gate;
- comments and strings are ignored;
- the legal shapes pass;
- a baselined site passes only with a reading;
- a stale entry fails;
- empty or undersized trees are refused.

### Baseline at `origin/main` `e606a4c7` (2026-09-26)

- OoT: 35 files, 5,453 `LOCATION`/`Entrance`/`EventAccess` sites. MM: 18 files,
  3,285 `CHECK`/`CONNECTION`/`EXIT`/`EVENT`/`STAY` sites.
- **Zero player-state negations in either graph.** The solver-inventory audit's
  §1.2 and §2.3 claim holds for `!`. There are 46 legal settings negations (39 OoT
  `!ctx->GetOption(...)`, 2 OoT `!logic->IsFireLoopLocked()`, which reads
  `RSK_KEYSANITY` only, and 5 MM `!SettingClocks()`). There is 1 legal guard (MM's
  enemy-soul requirement).
- **Three `NEEDS-READING` sites, accepted with a reading:** `logic->StoneCount() == 3`
  at `overworld/hyrule_field.cpp` (two sites, the Ocarina of Time and its song)
  and `overworld/temple_of_time.cpp` (the Door of Time). The audit did not count
  equality comparisons. `Logic::StoneCount()` is
  `HasItem(RG_KOKIRI_EMERALD) + HasItem(RG_GORON_RUBY) + HasItem(RG_ZORA_SAPPHIRE)`,
  a sum of three set-item booleans. So 3 is its maximum and `== 3` is exactly
  `>= 3`. A repeated stone copy is inert. These sites are monotone.
- MM has no accepted entry.

### What the probe does not see

- **OoT's `logic.cpp` helper bodies are not scanned.** They call members without
  `logic->`, and they mix effect-application code with conditions (the audit's
  §1.2 lists five `!` over state there: early-return guards and effect branches).
  A text scan of that file produces dozens of unclassifiable sites. The grow-check
  covers those helpers semantically, because every region lambda calls them.
- **Locals holding state:** `uint32_t n = ClockCount(); return n < 3;` reads as a
  bare local.
- **Ternaries** (`HAS_X ? false : true`), negation through a macro defined outside
  the scanned files, and any semantics a macro hides.

All of these are what the grow-check is for.

## The CI grow-check (`ComboLogicMonotonicity`, rando tier)

It runs a real headless OoT generation and MM's rando bring-up, applies MM's
shipped profile, and then works through both engines using the coordinator's
own surface (`Combo_Logic_GetEngine`, `Combo_Logic_RunRound`):

- **G1, the walk.** It opens a round from the starting state and expands. For
  every bag row in a given order it grants **one copy** (`assumeOwnItem`) and
  expands. After each grant it reads the full reached **check** set
  (`checkReached` over the id space) and the full reached **region** set (OoT: each
  region's four age/time bits; MM: each region's joined time slices, through the
  engine's read-only `MM_ComboLogic_TestRoundRegions`). Losing any check, region,
  age/time bit or time slice fails the row, and the output names the step and the
  item granted. The four orders are forward, reverse and two seeded shuffles. Each
  walk ends by confirming that one more expand reports nothing new.
- **G2, order independence.** All four orders close on the same set, and that set
  equals a one-shot round that assumes the whole bag at once.
- **G3, monotone in tricks** (ADR 0010 §3.2). Every trick is forced on (OoT: all
  266 `RT_*` options; MM: all 86 frozen `randoSaveTricks` bits). At every forward
  step, the tricks-off closure is contained in the tricks-on closure. On OoT, some
  steps must be strictly larger, which shows the tricks-on run measures a
  different operator.
- **G4, the coordinator.** `Combo_Logic_RunRound` runs over 17 growing prefixes of
  the union bag, forward and reverse, with tricks off and on. Every round returns
  OK, and neither side's candidate-host count, crossing flag or goal flag ever
  drops. Both candidate counts, OoT's crossing flag and both goal flags must
  actually move across the prefixes, so a constant cannot satisfy "never
  dropped". MM's crossing flag is already open at the empty prefix.
- **The red half.** Test-only TUs (`ComboLogicMonotonicityOoT.cpp`,
  `ComboLogicMonotonicitySingleExe.cpp`) plant **one negated edge** at runtime
  into each live graph: "passable only while the player does NOT hold X", on the
  single inbound edge of a region reached at the start. No region file changes.
  The same walk must go red **at exactly X's grant**, on both the region and the
  check observable, and MM's own in-round shrink counter must see it. After the
  edge is removed, the walk must be green and identical to the unplanted walk.

**The bag** follows the operator's multiplicity ruling (2026-09-26): "keep in
mind that there are usually options here. like plentiful drops is an option (and
then depending on the options there might be more checks than items. ice traps
are a thing too for randos)". Each game's bag has one row per copy:

- every progression copy of its pool;
- one **surplus** copy of each distinct progression id (the plentiful shape,
  which is where a give path past its top tier or counter maximum could lower
  something);
- every junk and renewable copy (the more-hosts-than-items shape);
- every **trap** copy the world holds.

On the shipped profile that is 546 OoT rows (321 progression, 105 surplus, 114
filler, 6 ice traps) and 2,282 MM rows (250 progression, 114 surplus, 1,918
filler). MM's `RI_TRAP` is not granted. It is not in MM's vanilla pool (traps
enter only under `RO_SHUFFLE_TRAPS`, which is off by default), and its give
reaches `OfferTrapItem()` outside the save, where no snapshot undoes it. Traps
are per-game filler this increment, and none crosses. Classes come from the O8
owner (`Combo_ItemClassOf`). OoT's fill-item hosts are emptied for the walks and
restored afterwards, so the search harvests nothing the bag did not grant. A
non-fill item such as the Triforce stays in place, so the goal a round reads
belongs to the world.

**Measured on 2026-09-26** (workstation, shipped profile):

| Game | Starting closure | Final closure (tricks off) | Final closure (tricks on) | Forward steps where tricks-on is strictly larger |
|---|---|---|---|---|
| OoT | 37 checks, 19 regions | 1,789 checks, 624 regions | 1,794 checks, 626 regions | 523 of 547 |
| MM | 510 checks, 50 regions | 2,256 checks, 314 regions | 2,256 checks, 314 regions | 218 of 2,283 |

- The OoT red half went red at step 25 and the MM red half at step 10. In both,
  the planted region and one of its checks were lost at the negated item's grant.
- G4: both goals and OoT's crossing go from 0 to 1 across the union-bag prefixes.
- The whole row takes about 30 s. It asserts no timing.

**What it does not claim:**

- It covers one profile per game (the shipped default), with tricks all off and
  all on.
- It walks four orders, not all orders. The per-step check makes each walk
  sensitive to every prefix it passes through, which is why it grants one copy at
  a time.
- It does one expand per grant, and relies on both engines computing a full
  closure per call. The end-of-walk confirmation asserts that.

## When a gate goes red

- **The probe reports a `NEW HIT`.** Rewrite the condition to require what the
  player has, not what they lack. If the site really is monotone (for example an
  equality at a quantity's maximum), add a baseline line with the reading that
  proves it. Adding a settings-only `logic->` member to
  `OOT_LOGIC_SETTING_MEMBERS` is a review decision that must cite the member's
  body.
- **The grow-check reports `LOST`.** The line names the engine, the step, the item
  granted, and the check or region lost. That item's give path, or a condition
  that reads it, lowers reachability. Treat it as a defect in whatever changed,
  never as a flaky row. The multiplicity clamps (`combo_logic.h`, ABI 3) are the
  precedent: the unclamped wallet wrap was exactly such a loss.
- **The red half does not go red.** One of the observables has gone blind (for
  example the check read, the region accessor, or a stale graph). Fix the
  observable before trusting any green run.
