# ADR 0010: Cross-game logic and goal-parametric beatability — one bag, one linked fixpoint, frozen at creation

- Status: **Accepted** (2026-07-31) — the operator answered nine of the eleven
  open questions the same day; the answers are folded in below and recorded in
  **Accepted answers**. **O4** (the combo-fill implementation shape) and **O9**
  (MM's per-trick vocabulary) remain tracked open items, owned by the
  increment-3 epic and the MM trick-vocabulary research respectively.
- For: #500 (Phase 3.2 tracker — cross-game logic and beatability, Lane D
  promoted); shaped throughout by #564 (one-game alignment audit)
- Depends on:
  - **[ADR 0002](0002-origin-tagged-shared-items.md)** (Accepted) — the
    origin-tag invariant (no raw game-local id crosses a game boundary outside
    a `SharedItem`) and the `ComboContext` growth contract. Every boundary
    object below obeys it.
  - **[ADR 0004](0004-menu-information-architecture.md)** (Accepted; §6/§4.1a
    amended 2026-07-30 for one-game semantics) — presentation states for
    frozen identity keys. This ADR supersedes one §4.1a consequence (the
    paired `RO_LOGIC` default) — see increment 1.
  - **[ADR 0009](0009-combo-settings-and-reverse-pool.md)** (Accepted;
    decisions 1/2 amended 2026-07-30) — combo settings authoring, the frozen
    profile record, `comboSettingsHash` (claim 2, reserved), the three-tense
    pairing predicates. The GOAL setting and both trick sets defined here fold
    into that identity machinery; no new mechanism is invented.
  - **#564's target creation-event contract** (steps 0-10) — the event this
    ADR's guarantees are evaluated inside.
  - Phase-1 substrate: **landed** — #568 (REFUSED as a first-class slot state,
    quarantine + latch + surface, for #533) and #569 (one game-thread commit
    choke point + monotonic commit generation in both artifacts, for #537);
    **in flight** — #570 (creation-time MM profile freeze + arrival
    compare-and-refuse, for #498/#564 phase 2 step 9).
- Sources: the #500 design corpus (the OoTMM beatability research and the
  merged-generation feasibility probe, both 2026-07-30) and five operator
  rulings quoted verbatim in the Context. OoTMM citations are anchored at
  OoTMM commit `669aaf5` as read by the corpus; redship citations are
  re-verified at `origin/main` = `aafee46b`.

Everything in the **Decisions** below is final for Phase 3.2. The operator's
2026-07-31 answers to nine of the eleven questions are folded into the
decisions they touch and recorded verbatim-in-substance in **Accepted
answers** at the end; the two that remain (**O4**, **O9**) are tracked in
**Still open** and neither blocks increment 1.

## Context

### Where 3.1 left the combo

Phase 3.0/3.1 shipped a deliberate bargain: **free-form placement plus a
spoiler log**, with the spoiler carrying the burden logic would otherwise
carry (`docs/phase3-roadmap.md`, Lane D: "not placing MM's Bow behind an OoT
check that requires MM's Bow is the hardest problem in the phase"). At
`aafee46b`:

- Both directions of the crossing are **post-fill overlays of duplicates**.
  Nothing is removed from either origin pool, so the crossing is logically
  inert: pair beatability factors exactly into "OoT beatable" (guaranteed by
  its own fill) AND "MM beatable" (unguaranteed), and any pair-level
  beatability assertion written today passes vacuously.
- OoT has real machinery: a static region graph with 4-state age/time
  reachability (`games/oot/soh/Enhancements/randomizer/location_access.h`),
  assumed fill, `CheckBeatable` (`3drando/fill.cpp:589-604`),
  `IsBeatableWithout` (`fill.cpp:300-308`), sphere playthrough, a headless
  harness, and CI-pinned determinism digests.
- MM has a real region graph (`games/mm/2s2h/Rando/Logic/Logic.cpp:10`,
  populated by ShipInit registrars; traversal `FindReachableRegions` at
  `Logic.cpp:126-188`; a 45-slice time model, `Logic.h:20+`) but **no
  beatability predicate anywhere in `games/mm`**, a Glitchless solver that
  is a forward fill mutating the live `gSaveContext`
  (`GlitchlessLogic.cpp:38, :78, :282`) under a 10s wall-clock abort
  (`:87-89`), and a paired world that defaults to **Nearly No Logic** — a
  shuffle plus a scene blacklist, zero reachability
  (`Rando/Logic/NearlyNoLogic.cpp:12-89`; default resolved at
  `Rando/Foreign.cpp:126-128` by CVar-existence probe).
- A failed paired generation still reverts to a **silent vanilla Termina**
  (`Rando/MiscBehavior/OnFileCreate.cpp:317-329`) — the divergence class the
  one-game ruling names corruption.

### The substrate that landed under this ADR's feet

The #564 alignment plan's phase 0/1 is in: REFUSED is a first-class slot
state with quarantine and an armed-session latch (#568, for #533), and every
durable write marshals through one game-thread commit choke point stamping a
monotonic commit generation into both artifacts (#569, for #537). #570 (in
flight) freezes the MM option profile into the pairing identity at creation
and makes arrival compare-and-refuse. **Refusal no longer converts to data
loss**, which is what makes every guarantee below shippable: this ADR's
failure mode everywhere is "refuse loudly through the #533 surface", and
that surface now exists.

### The operator rulings this ADR encodes

Recorded verbatim because the decisions below are their design consequences,
not proposals.

**One game (2026-07-29, on #500):**

> "freezing at creation is the correct semantics for sure. keep that idea in
> mind with all designs. this is *one game* from a semantic standpoint."

Binding consequences (recorded on #500 and #564): one identity fixed at one
creation event; arrival-time divergence is corruption to detect and refuse,
never a choice to honor; merged generation at OoT file-create is the natural
implementation; combo-level settings are the one game's settings.

**Beatability is goal-parametric (2026-07-31):**

> "beatability is defined by the game type and other rules. mm lets you pick
> how many remains are required to get to majora. that many is beating the
> game if beating majora is part of the win condition. maybe the setting is
> beat either game. maybe its triforce hunt."

**One bag, linked at sphere zero (2026-07-31):**

> "needing to cross games to make progress in either game is expected
> behavior. consider the base behavior of this for the original users of
> this: speedrunner types. for someone who knows how to break this game,
> there are very few unwinnable situations. the base algorithm would be 'put
> every item from both games into a bag, then randomly distribute to each
> check'. you can use sphere expansion or something to include beatability.
> in that sense, you can think of each game as its own sphere expansion that
> are both linked at sphere zero (assuming you can easily get to castle town
> in your oot which is easy with settings)."

**Tricks are part of the proof (2026-07-31):**

> "both games have a graph of tricks/glitches that the user can choose as a
> valid path for the randomizer to include in beatability."

**Reuse the solvers (2026-07-31):**

> "each game already has its own solver of this problem."

with the refinement:

> "you *can* merge them in code if that results in an easier to maintain end
> product; just know what's there."

---

## Decision 1 — The beatability contract is goal-parametric

**"Beatable" is not a fixed predicate. It is: the combo's GOAL expression is
provable by the deterministic reachability fixpoint, under the world's frozen
rules — and the GOAL is a combo-level setting, frozen at creation, part of
the pairing identity.**

### 1.1 The GOAL setting

A new combo-level (ADR 0003 tier-4, #498-owned) setting — working name
`gCombo.Rando.Goal` — with an extensible value set, at minimum:

| Value | Goal expression |
|---|---|
| `beat-both` (default) | `OOT_GOAL && MM_GOAL` |
| `beat-either` | `OOT_GOAL \|\| MM_GOAL` |
| `triforce-hunt` | combo-level piece requirement met (accounting: **one shared piece count across both worlds** — answer O10) |

This is OoTMM's own shape — its `goal` setting defaults to `'both'` and is
evaluated as one boolean over the merged event set
(`packages/core/src/settings/data.ts:76-89`; `isGoalReached`,
`pathfind.ts:712-739`: `events.has('OOT_GANON') && events.has('MM_MAJORA')`;
OoTMM @ `669aaf5`, per the #500 corpus). OoTMM never had a "pair" concept
separate from "the world"; neither does the one game.

Identity mechanics, all existing machinery (ADR 0009):

- The resolved GOAL value and its parameters **fold into
  `comboSettingsHash`** (ADR 0009 claim 2, 4 B, reserved) and into the frozen
  record #570 stamps. No new carve.
- Frozen at **creation-event step 1** (#564 contract): nothing may be read
  from a CVar after the freeze line, the GOAL included.
- Arrival or load divergence from the frozen GOAL is **corruption to refuse**
  through the landed #533/#568 machinery — same treatment as any identity
  term, per the one-game ruling and #570's compare-and-refuse.
- Growth contract: stored value 0 means **unset** (a pre-3.2 legacy record).
  A legacy record makes no beatability claim; it is displayed as such, never
  silently promoted to `beat-both`.

### 1.2 The goal's parameters are each half's own authored settings

The ruling's operative sentence — "mm lets you pick how many remains are
required to get to majora. that many is beating the game" — means the goal
expression does not invent new knobs. Each half's existing, authored access
settings ARE the goal parameters, and the corpus's open question "what does
MM-beatable even mean" is thereby settled in shape:

**`MM_GOAL`** = Majora's Lair reached and Majora defeated, provable through
MM's own authored gate chain, with MM's own authored parameters:

- Moon access: `CAN_PLAY_SONG(OATH) && MeetsMoonRequirements()` on the
  `THE_MOON` exit (`Rando/Logic/Regions/Central.cpp:83`), where
  `MeetsMoonRequirements()` is
  `RemainsCount() >= RANDO_SAVE_OPTIONS[RO_ACCESS_MOON_REMAINS_COUNT] &&
  MoonMaskCount() >= RANDO_SAVE_OPTIONS[RO_ACCESS_MOON_MASKS_COUNT]`
  (`Rando/Logic/Logic.h:409-412`; `RO_ACCESS_MOON_REMAINS_COUNT` defaults to
  4, `StaticData/Options.cpp:38`).
- Lair access:
  `RemainsCount() >= RANDO_SAVE_OPTIONS[RO_ACCESS_MAJORA_REMAINS_COUNT] &&
  MoonMaskCount() >= RANDO_SAVE_OPTIONS[RO_ACCESS_MAJORA_MASKS_COUNT]` on
  the `MAJORAS_LAIR` entrance (`Rando/Logic/Regions/Moon.cpp:115`;
  `RO_ACCESS_MAJORA_REMAINS_COUNT` defaults to 0, `Options.cpp:22`).
- MM's own triforce machinery exists for the hunt variant:
  `RO_SHUFFLE_TRIFORCE_PIECES`, `RO_TRIFORCE_PIECES_MAX`,
  `RO_TRIFORCE_PIECES_REQUIRED` (`Options.cpp:70, :75-76`).

**`OOT_GOAL`** = per OoT's own goal settings:

- `RSK_TRIFORCE_HUNT` ("Triforce Hunt": Off / Win / Ganon's Boss Key,
  `settings.cpp:418`) with `RSK_TRIFORCE_HUNT_PIECES_TOTAL` /
  `RSK_TRIFORCE_HUNT_PIECES_REQUIRED` (`settings.cpp:433, :441`). Off means
  Ganon defeated.
- Ganon's-path gates as parameters: `RSK_RAINBOW_BRIDGE` (Vanilla / Always
  open / Stones / Medallions / Dungeon rewards / Dungeons / Tokens / Greg,
  `settings.cpp:190`) and its count settings, `RSK_GANONS_TRIALS` /
  `RSK_TRIAL_COUNT` (`settings.cpp:274, :284`), `RSK_GANONS_BOSS_KEY`.

Both halves' goal parameters are already inside the frozen identity: OoT's
settings hash folds its own options, and #570's widened `mmProfileDigest`
covers MM's (per ADR 0009 decision 1's amendment, a digest narrower than the
generator's input set is vacuous). The GOAL value composes them; it does not
duplicate them.

**Consequence, stated plainly:** beatability under `beat-either` promises
exactly the goal expression and nothing else — and that expression is a plain
OR. Two halves of one answer, per the operator (2026-07-31, answering O1):

> "yeah, if its beat either its okay if one is unbeatable. but don't try to
> insist that only one is beatable."

- **An unbeatable half is permitted, including by choice.** If only one half's
  goal is provable, the other half may contain unreachable required items —
  that is the contract working, not failing. A configuration whose own
  authored parameters make a half unbeatable is a legitimate `beat-either`
  world: it is accepted, not refused, with a **visible warning at creation**
  naming the half that carries no proof (the warning recommendation stands),
  and the frozen record says so afterwards.
- **The OR is never narrowed to an XOR.** The fill and the solver must not
  bias toward, prefer, or enforce "exactly one half beatable".
  `OOT_GOAL || MM_GOAL` is satisfied the moment either disjunct is provable;
  if both turn out provable, that is a **welcome outcome** and must never be
  constrained away, perturbed, or re-rolled to restore asymmetry. Any rung,
  heuristic, or lock that makes both-halves-beatable harder to reach under
  `beat-either` is a bug against this ADR.

A player who wants both halves finishable *by guarantee* selects `beat-both`.
Per-half strictness beyond the goal ("all locations reachable") remains each
half's own existing axis (`RSK_ALL_LOCATIONS_REACHABLE`; OoTMM's
`allLocations` analogue) and composes with any GOAL.

## Decision 2 — One bag: items leave origin pools, and cross-game progression is base behavior

**The destination fill is a single combined bag over both games' items,
distributed across both games' check sets. Needing to cross games to make
progress in either game is expected base behavior, not an advanced tier.**
This settles the corpus's first open question — the one the tracker said
"gates everything after" — in the removed-from-origin direction.

Terms, defined precisely because the two regimes get confused:

- **Duplicate overlay** (today, and increments 1-2, explicitly
  *transitional*): a crossed item is an extra copy placed post-fill in the
  other game; the origin world keeps its own copy and stays self-beatable;
  pair beatability factors per-game; every pair-level beatability assertion
  is vacuous.
- **Removed-from-origin** (the decided destination, increment 3): the union
  of both games' item pools is one bag; one fill assigns bag items to the
  union of both games' shuffled checks; an item may exist only in the other
  game; each half is in general NOT self-beatable and the goal proof is
  genuinely pair-level. Origin tags travel with every placement (ADR 0002);
  the fill's output is origin-tagged end to end.

### 2.1 The reachability model: two sphere expansions linked at sphere zero

One fixpoint over the linked graph. Each game is its own sphere expansion
over its own region graph; the two are joined through the crossing edges
(Happy Mask Shop ↔ Clock Tower, `src/common/entrance.h`). Under settings
that leave the crossing reachable from the start — OoT child start reaches
Castle Town's Happy Mask Shop cheaply — the link participates from **sphere
zero**, which is the operator's framing.

The nuance to hold onto: **the crossing is an edge with a requirement
expression, like any other edge.** If settings gate it (a closed Deku Tree
path, an entrance requirement, a time-of-day constraint), the link enters
the expansion at whatever sphere its requirement is first met, and the
fixpoint handles that with no special case. There is no "phase where the
games connect"; there is an edge whose guard becomes true at some sphere,
possibly zero. OoTMM's precedent for the guard's content: it auto-ANDs
`can_reset_time` onto every entrance into MM except the designated game
link, and re-stamps Day 1 into the MM time mask on every OoT→MM edge
(`entrances.ts:210-212`; `pathfind.ts:596-599`; OoTMM @ `669aaf5`).
Redship adopts that precedent by **accepted answer O2**: a Day-1 re-stamp
plus a reset-time guard on entry to MM, with a symmetric requirement on the
return trip. This is the corpus's "crossing-entrance safety predicate"
question, concretized as "author the crossing edge's requirement expression"
— the exact per-direction expressions for Happy Mask Shop ↔ Clock Tower are
authored by the increment epic under that shape.

### 2.2 The logic ladder: no-logic base, provable rungs on top

The audience inverts the ladder. The **base mode is single-bag no-logic**:
put every item from both games into a bag, distribute randomly across both
games' checks, spoiler log carries the burden. Its target user is the
operator's "speedrunner types" — players who can break both games, for whom
"there are very few unwinnable situations", and for whom an unwinnable seed
is an accepted cost of the mode, exactly as each game's existing No Logic
already is (`RO_LOGIC_NO_LOGIC`; OoTMM's `logic: 'none'` bypasses the
pathfinder entirely and may produce unbeatable seeds by design,
`pathfind.ts:242-256`).

Beatability modes **layer on top** via deterministic sphere expansion, per
Decision 1: a rung's guarantee is "the GOAL expression is provable under
this rung's rules". The rung set, frozen at creation like everything else:

| Rung | Guarantee | Who it is for |
|---|---|---|
| `none` (base) | no proof; spoiler carries the burden | players who can break both games |
| `beatable(tricks = T)` | GOAL provable with trick set T enabled | everyone else, tuned by T (Decision 3) |
| `all-reachable(tricks = T)` | GOAL provable and every location reachable | completionists; composes per-half strictness axes |

A maximal-tricks `beatable` config approximates the speedrunner while
*keeping the proof* — see Decision 3.4. The shipped default rung mirrors Ship
of Harkinian's own rando defaults (Glitchless, no tricks, all locations
reachable) per accepted answer O11. This combo ladder is distinct from the
**attempt ladder** (deterministic seed re-roll on fill dead-end,
increment 1); the two are named separately everywhere below because
conflating them produced the silent-vanilla-revert bug class.

### 2.3 What the guarantee is, formally

The right formalism — established by the corpus against the reference
implementation and adopted here — is a **least fixed point of a monotone
operator over a finite join-semilattice**, the Datalog / points-to-analysis
class, not plain graph reachability. The dependency `area → location → item
→ guard → area` is cyclic regardless of exit topology, so there is no
a-priori DAG; the DAG (sphere decomposition) is the **stratification of the
least fixed point**, an output (`getSpheres`, `solve.ts:563-580`;
`makeSpheresRaw`, `analysis.ts:66-105`; OoTMM @ `669aaf5`).

The reference's load-bearing mechanics, all adopted as disciplines here:

- **The guarantee is the fill's exit condition, never a check bolted after
  it.** `LogicPassSolver.run()` loops "pathfind over the partial placement;
  if goal reached, stop; else place exactly one more required item"
  (`solve.ts:405-438`) — the loop can only terminate in a state where the
  goal is provable. The placement primitive is **assumed (reverse) fill**
  (`randomAssumed`, `solve.ts:1189-1217`); the trailing `fillAll()` dumps
  the rest with no logic at all (`solve.ts:1248-1257`), sound **only**
  because the reachability operator is monotone in items.
- **Monotonicity is protected by a negation ban**: runtime `!` over
  `has()`/`event()` throws at expression-build time (`expr/builder.ts:61-74`).
  Redship's conditions are C++ lambdas, so the ban becomes a review rule
  plus mechanical enforcement (accepted answer O6: review rule **and** static
  probe **and** CI grow-check — all three, not a choice) — but the invariant
  itself is not optional; assumed fill and the trailing no-logic dump are
  unsound without it.
- **Exclusive world states are per-path constraint accumulation, not state
  queries**: constraint bitsets with contradiction detection
  (`compile.ts:34-45, :251-277`) model MM's mutually-exclusive region states
  and "which game's world-state does this path assume" without duplicating
  graphs.
- **The join must be a real join.** OoTMM's `mergeAreaData` unions times and
  intersects constraint flags, and iteration re-explores on a covering test
  (`pathfind.ts:155-170, :348-351`). Redship MM's `FindReachableRegions`
  guards on first visit and **overwrites** `regionTimeStates`
  (`Logic.cpp:126-188`) — a region first reached with a poor time set is
  never re-explored when a better one appears. That is a correctness bug
  class in MM's crawl **today** and is fixed as part of increment 1's
  factoring, before any cross-game fact is computed from that crawl.
- **Failure handling is deterministic restart** (`retry()`,
  `solve.ts:339-359`, `attemptsMax = 100`), which is the attempt ladder's
  precedent.
- **Resource exhaustion is designed out, not counted**: the
  `renewable`/`license`/source-event triad plus a placement restriction
  forcing critical renewables into renewable locations (`locations.ts:87-115`;
  `solve.ts:1204-1206`). This connects directly to redship's shared-resource
  discipline (#525's kind-tagged slots, the #540/#554/#555 harvest/apply
  gates): "very few unwinnable" is the *base mode's* bargain for players who
  can break the games; the **beatable rungs' fill must respect
  renewable-vs-consumable discipline**, because consumable-funded paths are
  exactly what a non-speedrunner cannot re-earn. The ladder's stricter rungs
  exist for them.

**Never a stochastic post-check.** The reference has no softlock hunter; its
two stochastic passes are hint refinement (`monteCarloZigZag`,
`analysis-foolish.ts:54-172` — Monte-Carlo sampling over a deterministic
oracle, output feeds hint quality only) and an entrance-shuffle fillability
stress test (`validate()` → `forwardFill`, run only when ER changed the
world). Redship builds neither for beatability; a stochastic pass may only
ever be layered on top of the deterministic core for hint quality (future
hints epic), never as the guarantee.

## Decision 3 — Tricks and glitches parametrize the proof

> "both games have a graph of tricks/glitches that the user can choose as a
> valid path for the randomizer to include in beatability."

### 3.1 What exists at source

**OoT already has the full system.** SoH carries a per-trick option table:
`RandomizerTrick` (`RT_*`, `randomizerTypes.h:4188+`),
`TrickOption::LogicTrick` rows built by the `OPT_TRICK` macro
(`settings.cpp:113`), surfaced as the "Logical Tricks" option group
(`settings.cpp:2200`), read by logic through
`ctx->GetTrickOption(RandomizerTrick)` (`settings.cpp:2889-2890`). A trick
gates a logic edge by widening its guard disjunctively — 425 conditions
under `location_access/` consult a trick, e.g. Deku Tree B1:
`logic->IsAdult || ctx->GetTrickOption(RT_DEKU_B1_SKIP) || ...`
(`location_access/dungeons/deku_tree.cpp:118`, likewise `:106`). Enabled
tricks are already folded into OoT's settings hash (the scope #564 V4 told
the MM digest to copy).

**MM does not have it yet.** 2ship's logic dialect has no per-trick option:
the only coarseness knob is `RO_LOGIC` itself, and the region graph carries
the seams as comments — "TODO: Trick for doing without the Bunny Hood"
(`Rando/Logic/Regions/Central.cpp:365`), "if someone wants to make it a
trick later feel free" (`MilkRoad.cpp:29`), and more. Until MM grows a
per-trick vocabulary (open question O9), its trick dimension collapses to
its `RO_LOGIC` mode; the model below is written so that is a degenerate
case, not a special one.

### 3.2 In the unified model, the trick set parametrizes edge requirements

The enabled trick set `T` is an input of the reachability operator: the
sphere-expansion fixpoint runs over the graph **with the chosen tricks'
guard-widenings satisfied**. Beatability is therefore always
"provable under the selected tricks" — `beatable(tricks = T)` — and the
trick set joins the GOAL expression as the second axis of the beatability
parameter space. This is not new machinery in either engine: it is exactly
what `GetTrickOption` in a guard already does; the unified statement just
names it as a proof parameter.

### 3.3 Trick selections are frozen combo identity

Per one-game semantics, **both games' trick selections are combo-identity
terms**: frozen at creation (contract step 1), folded into the pairing
digest (OoT's hash already folds its tricks; MM's per #570's widened
digest scope when its vocabulary exists), rendered post-creation in ADR 0004
§6 state 4 (frozen, read-only, with reason). A post-creation trick toggle is
divergence to refuse — the proof was computed under `T`, and a world played
under `T' ≠ T` is not the world that was proved. Across the game boundary,
trick sets travel **only as opaque identity terms / digest input**: no
game-local trick enum crosses raw (ADR 0002; the coordinator layer sees "a
digest over MM's trick selections", never an `RT_*` or an MM trick id).

### 3.4 The bridge to the base audience

A maximal-tricks `beatable` configuration is the formal approximation of the
player who can break both games — near-base-mode freedom of routing, but the
proof still holds. The ladder therefore reads, from loosest to strictest:
`none` (no proof, the bag alone) → `beatable(T = maximal)` (speedrunner
routes, proved) → `beatable(T = ∅)` (conservative Glitchless) →
`all-reachable`. The trick axis is what makes the ladder continuous rather
than a cliff between "anything goes" and "granny logic".

## Decision 4 — Informed reuse: the solvers stay authoritative; composition is the default posture

> "each game already has its own solver of this problem." /
> "you *can* merge them in code if that results in an easier to maintain end
> product; just know what's there."

**The non-negotiable is informed reuse.** Both ports already own a
battle-tested solver: SoH's 3drando logic/fill (region table +
`ReachabilitySearch`/assumed fill/`CheckBeatable`/spheres,
`3drando/fill.cpp`) and 2ship's Rando logic (registrar-built region map +
`FindReachableRegions` + the Glitchless forward fill,
`Rando/Logic/`). Whoever implements increment 3 must first know both in
depth: a **solver-inventory audit** — graph representation, trick gating,
fill algorithm, evaluation entry points, save/state coupling — is an
explicit first step of the increment-3 epic, before any line of the combo
pass is written.

**Default shape: composition by coordinated alternation.** Realize the
unified fixpoint as the standard fixpoint-of-two-monotone-operators
composition: each game's own solver runs its own reachability expansion
against its own graph; between rounds the coordinator exchanges **only
boundary state** — items obtained from checks the other side granted,
crossing-edge availability, shared-resource state — and iterates until
neither side unlocks anything new. Convergence follows from both operators
being monotone on the same finite lattice (Decision 2.3's disciplines are
what make MM's operator honestly monotone). The boundary exchange is
origin-tagged `SharedItem`-shaped data throughout, so ADR 0002 holds by
construction: each solver names only its own ids. Upstream diffs in both
trees stay small; the new code is a **thin coordination layer in
`src/common`**. The single-bag fill composes the same way: the combo pass
draws from the union bag but delegates per-check placement legality and
per-round reachability queries to the owning game's solver.

**Code-level unification is permitted, on maintainability grounds.** If the
audit concludes that merging the two solvers into one engine yields an
easier-to-maintain end product, that choice is legitimate. The trade-offs,
stated so the choice is made with eyes open:

| | Composition (default) | Unification |
|---|---|---|
| Upstream diffs | small, both trees | large port; permanent divergence cost against SoH/2ship |
| ADR 0002 | clean by construction (boundary = SharedItem-shaped) | needs explicit care; a unified engine is one namespace by temptation |
| Logic dialects | two, forever (C++ lambda styles differ) | one dialect, one engine to maintain |
| MM's live-save mutation | contained behind the query seam (memcpy swap discipline, `GlitchlessLogic.cpp:38/:282`) | forced to fix it properly (detached simulated save, as OoT's `Logic::mSaveContext`) |
| Determinism digests | re-pin once (boundary observable) | re-pin everything |

**The contract is binding regardless of shape**: single bag, unified
fixpoint semantics, sphere-zero-linked expansion, goal expression as the
fill's exit condition, trick parametrization, identity frozen at creation.
The implementation question this leaves the increment-3 epic is "what does
the thin coordination layer look like" (or, if unification wins the audit,
"what does the merged engine's boundary with ADR 0002 look like") — open
question O4 — not "is there a combo-level guarantee".

## Decision 5 — Three increments

Restructured under the pool ruling: the destination is increment 3;
increments 1 and 2 are **transitional** (crossings remain duplicate
overlays) and are what make 3 reachable.

### Increment 1 — per-game guarantees under the pair (DECIDED; can start immediately)

While crossings are duplicates, pair beatability factors per-game, so the
smallest honest step closes the open half — MM's — and hardens the seams:

1. **Paired-MM logic default flips Nearly No Logic → Glitchless** at its
   single resolution point (`Rando/Foreign.cpp:126-128`). An explicit player
   choice of a no-logic mode is still honored (existence-probe semantics
   unchanged) — choosing away the proof is legitimate and is recorded in the
   frozen identity like everything else. This supersedes, on acceptance, ADR
   0004 §4.1a's "no raised profile ships" consequence for this one default;
   the dead-end-rate concern recorded there is answered by (2), not ignored.
2. **The deterministic attempt ladder replaces the silent vanilla revert.**
   On a fill dead-end: `seed_n = Hash(master ‖ ":glitchless-attempt-" ‖ n)`,
   attempt index recorded in the save and the spoiler so the world stays a
   pure function of identity (spec details: accepted answer O3 — the
   implementer picks hash recipe, attempt maximum and recording sites under
   this ADR's determinism rules). Terminal failure **refuses** through the
   #533/#568 surface — at arrival while generation still runs there, moving
   to file select with increment 2 — never `SAVETYPE_VANILLA`
   (`OnFileCreate.cpp:317-329` loses its catch-all job).
3. **Reachability-gated foreign hosts**: factor the check-tracker crawl
   (`Rando/CheckTracker/CheckTracker.cpp:240-305`) into `Rando::Logic` — 
   adopting the join-and-recheck discipline while in there, fixing the
   `regionTimeStates` overwrite (Decision 2.3) — and intersect
   `PlaceForeignItems`' candidate list with the reachable-check set. Narrows
   a list; cannot dead-end a fill.
4. **CI locks over pinned seeds**: extend the headless MMRandoGen rows —
   paired generation under the ladder succeeds for N pinned master seeds
   (red today: nothing retries); every `gComboCtx.foreignPlacements` entry
   is in the reachable set (red today: the xorshift picks over all junk
   holders, including Moon/temple-interior checks).

Honest limit, stated so nobody writes the vacuous lock: increment 1 makes
"OoT beatable ∧ MM beatable" true by construction, which under duplicates
equals `beat-both` — but a *pair-level* beatability assertion is still
theatre until items leave origin pools. Pair locks land with increment 3,
paired with removal (per the tracker's lock guidance).

### Increment 2 — merged generation, FULL delivery (DECIDED)

The #500 probe's delivery **option 1**, which #564 ruled is the one that
satisfies one-game semantics (the hybrid is acceptable only as a transition
and is "option 1 with the bytes thrown away"):

- The whole creation runs at the OoT file-create seam
  (`games/oot/src/code/z_sram.c`, after `Context_InvalidateSessionOnNewGame`
  and before `Save_SaveFile` — `z_sram.c:306` at HEAD), executing #564's
  creation-event contract steps 1-9: freeze (profiles, GOAL, tricks, seeds)
  **before OoT's `Fill()`** (ADR 0009 D2's amendment), both fills, both
  crossing passes, validation + attempt ladder, one spoiler, atomic identity
  publish, author-and-arm the MM shadow
  (`Context_UpdateShadowCopy(GAME_MM, ...)` +
  `Context_ArmShadowAsFrozen(GAME_MM, MM_ENTR_SOUTH_CLOCK_TOWN_0)`).
- **Arrival becomes hydrate-or-refuse** with zero generation capability —
  #570's compare-and-refuse, completed by deleting the arrival-time
  generation dispatch.
- **`sRandoInitDone` splits core/asset two-phase**
  (`games/mm/2s2h/GameExports_SingleExe.cpp:1404-1512`): creation-time
  generation must init MM's rando core without latching away the real MM
  boot's asset work (GfxPatcher, tracker icons, asset-gated DLs). The
  once-only-init class from the resume contract, in reverse.
- **Generation failure fails the creation, at file select**, on the #533
  surface, wholly — no partial identity, no vanilla Termina. This is where
  the attempt ladder's terminal case lands permanently.
- Feasibility is settled, not hoped: the rando-determinism CI row already
  runs OoT generation plus the full paired MM generation in one process with
  MM never booted and no MM archives mounted
  (`src/common/test_runner.cpp` → `MM_Rando_HeadlessForeignDigest`).
- Cost acknowledged: SeedDeterminism / MMRandoGen / HeadlessForeignDigest
  digests re-pin.

**2026-08-04 operator decision (#582) — the fill budget.** PR #581's
increment-1 review measured the open question this section's "Creation-time
compute budget" cost flagged as owed: under the heavy Glitchless profile, 14
of 66 master seeds' first attempts failed, and every one of those 14 was the
fill's 10-second wall-clock abort — zero were deterministic dead-ends
(100% wall-clock abort, 0% dead-ends). The operator ruled on that evidence:
**increment 2 ships with a visible generation-progress surface and a ~30
second cap as the floor, layered with a per-attempt adaptive budget
calibrated to host speed.** This preserves PR #581 §2a's determinism rule —
a timeout never climbs a ladder rung, because a wall-clock abort is a
function of machine speed, not of the seed, and letting it advance the
ladder would hand two players on different hardware two different worlds
under one frozen identity. The adaptive per-attempt budget changes how long
a slow host waits before that rule fires; it does not change the rule. Sizing
the ~30s floor, the adaptive calibration, and the progress surface's copy is
left to the increment-2 implementer; #582 stays open as that epic's
implementation tracker.

**2026-09-17 — increment 2 as built (epic #644).** The design note for what
actually shipped, recorded here because the increment's claims are about ORDER
and an order is only checkable against a written one.

*The seam order, exactly.* The creation of a paired file is one event spanning
two instants that no player-reachable edit can separate, because nothing after
the first instant reads a CVar:

1. **Freeze** — `Playthrough_Init`, immediately after `Random_Init(finalHash)`
   and **before `Fill()`**: `sourceIsRando` / `sharedRandoSeed` /
   `sharedRandoSettingsHash`, then `mmProfileDigest`
   (`MM_Rando_ComputeProfileStamp`), then the give-capability publish
   (`MM_Rando_PublishProfileGiveCaps`), then the combo record and
   `comboSettingsHash` last — ADR 0011 decision 4.1's order. The pre-`Fill()`
   gate `Combo_ForeignPairingRequested()` is asked here and logged, its first
   production caller. Nothing in the block consumes the RNG stream, which is why
   the move does not itself change a generated world.
2. **OoT's `Fill()`**, unchanged.
3. **OoT's spoiler**, unchanged, at its own path.
4. **Reverse crossing pass** (`OoT_PlaceForeignItems`), still at the end of
   `Playthrough_Init` — see the deviation below.
5. — the player names the file —
6. **`z_sram.c` `Save_InitFile`**, after `Context_InvalidateSessionOnNewGame`
   and `Randomizer_InitSaveFile`, before `Save_SaveFile`:
   `OoT_RunPairedCreationEvent(slot)`. It snapshots the whole unified
   `gSaveContext` buffer, calls `MM_Rando_GenerateAtCreation`, and restores it.
7. Inside that call: `MM_Rando_InitCore()` → `MM_Sram_InitNewSave()` →
   `GameInteractor_ExecuteOnSaveInit(0)` (the real `OnFileCreate` chain: MM's
   fill, the forward crossing pass, the attempt ladder) → **the one spoiler
   artifact** → `Context_UpdateShadowCopy(GAME_MM, …)` +
   `Context_ArmShadowAsFrozen(GAME_MM, MM_ENTR_SOUTH_CLOCK_TOWN_0)`.
8. Back in the seam: the shortfall surface, then `Save_SaveFile()` — whose first
   `.redsave` therefore already carries a complete, armed MM half.

*The identity publish point.* The identity is published by the **freeze**
(step 1) and RETRACTED on any failure: `Playthrough_Init` snapshots the previous
terms and rolls them back if `Fill()` or the reverse pass fails, and
`OoT_RunPairedCreationEvent` zeroes every term (including the combo record's
`formatVersion` occupancy tag and the armed MM shadow) if the MM half fails.
There are two states, "fully frozen" and "untouched"; #564 step 8's
all-or-nothing is implemented as publish-then-retract rather than
publish-at-the-end, because the freeze must precede `Fill()` and the fill is not
the last thing that can fail.

*What the arrival still authors.* Nothing about a paired RANDO half — its start
state and its clock come from the creation event, because
`MM_Sram_InitNewSave` writes exactly the new-file clock `#639`'s arrival-side
re-author existed to restore, and a creation-authored half never passes through
the title demo that broke it. **PR #674's grant stays exactly where it is, and
its gate is unchanged**: `MM_Play_GrantComboArrivalIntroRewards` still runs on
the `!hadFrozenState` leg and still returns early for `SAVETYPE_RANDO`. What
changed is which files answer that gate which way. A paired rando half now
arrives with `hadFrozenState == 1` on its FIRST crossing (the creation armed the
shadow), so the leg is skipped; the files that still reach it are exactly the
ones whose MM half is a vanilla bootstrap — a vanilla OoT file crossing, a
session with no paired OoT world, a REFUSED pairing, and a pre-increment-2 file
with no authored half. Those are precisely the pairing kinds #654's ruling is
about, so the arrival remains the intro event for them and moves nowhere.

*The budget (#582).* `src/common/gen_budget.{h,c}`. Per-attempt wall-clock
budget = `clamp(30 s × hostScale, 30 s, 90 s)`; `hostScale` is
`measured / reference` in integer percent, clamped to `[100 %, 300 %]`, measured
once per process by timing a fixed 24 M-iteration integer loop against a pinned
18 ms reference [corrected 2026-09-26, #708: this read "55 ms"; see the
Amendments entry of that date] (the development workstation this increment was measured on —
an arbitrary reference on purpose, because only the ratio is used and a
runtime-sampled reference would make every host's scale drift). The whole
creation additionally gets `2 ×` the per-attempt budget, checked BETWEEN ladder
attempts so it can never truncate an attempt about to succeed; worst-case wait
is that total plus one in-flight attempt, ~90 s at scale 1.0. The budget is
delivered into the fill through `Rando::Logic::gRsbsGlitchlessTimeoutMsOverride`
— the channel that already existed for the locks — and only ever fills a ZERO,
so an armed test override is never clobbered (the alternative is a lock that
silently tests the shipped budget). **PR #581 §2a is untouched**: neither the
per-attempt abort nor the new total abort climbs a rung; both stop the ladder.

*The progress surface.* `Combo_GenProgress_*` in the same file: a
phase/attempt/elapsed/budget record with a greppable stderr leg that always
fires and a registered sink for presentation. Two sessions per paired creation —
one around OoT's staged generation, one around the file-create seam — because
the two are separated by however long the player spent in the menu, and only the
second is a wait. An in-frame progress BAR needs a render-during-blocking-work
seam that does not exist in this tree and would live in `SohGui`; the channel is
built and wired, the bar is not, and that is stated rather than implied. The
second session's elapsed time is also **P12's measurement**, printed on every
real creation.

*The shortfall surface (#583).* Surfaced at creation on the shared overlay, with
the counts that explain it (placed / requested / eligible hosts / reachable
eligible hosts), read through `MM_Rando_LastPlacementStats`. It is not an error:
while crossings are duplicate overlays the origin world keeps its own copy, so a
missing crossing costs "fewer extras". **The drop order is NOT changed here.**
#583's option 1 (shuffle-then-truncate, so the dropped entries stop being the
pool's tail) moves every generated world and therefore belongs in the same
commit as the re-pin, or in none; it is recorded as remaining rather than
smuggled in beside a surface change.

*The spoiler shape (#660).* ONE artifact: OoT's spoiler document plus a single
top-level `"combo"` key carrying the identity tuple, the frozen combo record,
MM's whole spoiler under `combo.mm`, and BOTH crossing directions plus the
shortfall under `combo.crossings`. Augmenting rather than inventing a schema
keeps every existing OoT spoiler reader working. MM's own
`RSBSPAIR<masterSeed>.json` is still written by `OnFileCreate` — the MM-only
harnesses assert on it — and is DELETED by the join, so exactly one artifact
survives a paired creation. The join runs inside `MM_Rando_GenerateAtCreation`,
because that is the only window in which MM's world and OoT's document both
exist: the caller's snapshot bracket takes MM's world away the instant the call
returns.

*Deviations from the literal contract, and why.*

- **OoT's `Fill()` did not move to the file-create seam, and neither did the
  reverse crossing pass.** Moving `Fill()` means moving the Generate button's
  whole flow, which lives in `SohGui` and is what `Randomizer_IsSeedGenerated()`
  — the predicate that decides a new file is a rando file at all — reads. The
  reverse pass's inputs are OoT's finished fill (which exists only where it is)
  and the frozen record (which now exists before the fill), and it consumes no
  RNG stream, so relocating it would change nothing observable while re-pinning
  four locks. What the contract is actually FOR is delivered: one freeze, before
  `Fill()`, with no CVar read after it, so no edit in the gap can change the
  world. Both move when they have a reason to — increment 3's single-bag fill,
  where the two directions become one draw over one bag and the whole fill
  relocates with them.
- **A pre-increment-2 paired file that never crossed is REFUSED at its first
  arrival**, by name, with copy that says to re-create it. Its `.redsave`
  Tier-3 is all zeroes, so there is no half to hydrate, and "arrival has zero
  generation capability" leaves no legitimate way to author one there. This is a
  stated migration cost, not an oversight: a transitional generation path at the
  arrival would be the second creation event the increment exists to delete.
- **The compare-and-refuse now runs on EVERY arrival**, not only the first. The
  old gate sat after the `if (hadFrozenState) … return;` block, so a return leg
  was never compared. Refusing means "do not apply the frozen half" — the blob
  stays armed and untouched, the slot is latched, and Termina is vanilla, which
  is what the refusal copy has always promised.

*Cost paid.* `SeedDeterminism` / `RandoDeterminism` / `MMRandoGen` /
`MMPairedAttemptDeterminism` / `HeadlessForeignDigest` re-pin ONCE, for #585's
`FindReachableRegions` join alone: the freeze move and the creation-seam move
are RNG-neutral and were verified byte-stable before #585 landed on top of them.

Increment 2 is the **prerequisite of increment 3**: a single-bag fill is a
single generation event by definition — both worlds' placements must be
decided at one seam before either spoiler exists.

**2026-09-16 (#654) — the MM intro rewards join the bag here, not before.** The
operator ruled that the cross-game arrival *is* MM's intro event: the intro
rewards (Ocarina of Time, Deku Mask, Song of Time, Song of Healing, magic) are
CHECKS whose contents are awarded on entering MM, and every combo MM half plays
the first-cycle gates as if the ocarina were held regardless of whether it is.
The #654 fix delivers the arrival contract and the unconditional gates for a
vanilla pairing only; the **intended end state — the ocarina as an ordinary pool
item, findable at a check in either game — is increments 2-3's**, because moving
it into the pool is a change to the fill, the pools and the starting-items
defaults and re-pins the determinism digests this section already lists as an
acknowledged cost. `RC_CLOCK_TOWER_ROOF_OCARINA` / `RC_CLOCK_TOWER_ROOF_SONG_OF_TIME`
(`SCENE_OKUJOU`) also inherit a reachability question these increments must
answer: the Clock Tower interior is the cross-game portal, so the roof is not
reachable the vanilla way in a combo file. The creation-event and arrival contract
is recorded in **ADR 0009, "Operator rulings 2026-09-16" (b)**; this bullet is the
pointer, not a second copy.

### Increment 3 — the single-bag combo fill (DECIDED scope; the phase's destination)

Items leave origin pools. One fill, at the creation event, draws from the
union bag and places across both games' shuffled check sets, per Decisions
2-4:

- **Needs (from the substrate):** the pre-Fill pairing gate
  `Combo_ForeignPairingRequested()` (ADR 0009 decision 2 — designed, still
  unimplemented; #493's named gap); the ADR-0002-clean boundary language
  (origin-tagged `SharedItem` + host check id + game-neutral bounds — exact
  scalars and carve budget: accepted answer O7, the increment-3 epic sizes
  the carve against `reserved[132]` [corrected 2026-08-04, #584: now
  `reserved[124]` — two carves (#569, #581) landed after this ADR was
  accepted; see ADR 0009's byte-budget amendment] under the append-only
  second-block rule);
  the negation-ban and constraint-bitset disciplines in force in both graphs
  (Decision 2.3, enforcement: answer O6, all three mechanisms); MM's
  crawl join-fix landed (increment 1.3); the solver-inventory audit and the
  composition-vs-unification choice (Decision 4, O4).
- **The fill's exit condition is the GOAL expression provable** under the
  frozen rung and trick set (Decisions 1-3), via assumed fill against the
  linked fixpoint; the attempt ladder is the failure policy; the trailing
  no-logic dump is legal only under the monotonicity disciplines.
- **The base rung ships here too**: single-bag `none` is this same fill with
  the proof obligation off — the operator's "bag → randomly distribute"
  algorithm — so the speedrunner base mode and the proved rungs are one
  code path with the exit condition parametrized, not two fills.
- Pair-level beatability locks land **here**, paired with removal: generate
  a paired world with an OoT item hosted only in MM; assert the goal
  provable; assert removing the MM host flips it unprovable. Both halves
  required — without removal the lock is theatre.

## Decision table

### Decided by this ADR (operator-ruled or corpus-settled; not up for re-litigation in the epics)

| # | Decision |
|---|---|
| D1 | Beatability is **goal-parametric**: a combo-level GOAL setting (`beat-both` default, `beat-either`, `triforce-hunt`; extensible), frozen at creation, folded into `comboSettingsHash` + the frozen record, divergence refused via #533/#568/#570 machinery |
| D2 | Each half's authored access settings ARE the goal parameters (MM: `RO_ACCESS_MOON_REMAINS_COUNT` / `RO_ACCESS_MAJORA_REMAINS_COUNT` / masks counts / triforce rows; OoT: `RSK_TRIFORCE_HUNT*`, `RSK_RAINBOW_BRIDGE*`, `RSK_GANONS_TRIALS`, `RSK_GANONS_BOSS_KEY`) |
| D3 | **Items leave origin pools**: single combined bag over both games, cross-game progression is expected base behavior; duplicates in increments 1-2 are transitional |
| D4 | Reachability is **one deterministic fixpoint over the linked graph** — two sphere expansions joined at sphere zero; the crossing is an ordinary requirement edge; the guarantee is the fill's **exit condition**, never a stochastic post-check; no softlock hunter is ever built |
| D5 | The **logic ladder** is base-first: `none` (single-bag free-form, speedrunner bargain) with provable rungs layered on top (`beatable(tricks)`, `all-reachable`) |
| D6 | **Trick sets parametrize the proof** and are frozen combo identity, crossing the boundary only as opaque digest input (ADR 0002); OoT's `RT_*`/`GetTrickOption` system is the model |
| D7 | **Informed reuse**: solver-inventory audit first; composition (alternating fixpoint, thin `src/common` coordinator, SharedItem-shaped boundary) is the default posture; code-level unification permitted on demonstrated maintainability grounds; the contract binds either way |
| D8 | Increment 1 (paired default Glitchless + attempt ladder + reachability-gated hosts + pinned-seed CI locks) and increment 2 (merged generation, FULL delivery, creation-event contract, `sRandoInitDone` split, fail-at-file-select) as specified; increment 2 precedes 3 |
| D9 | Beatable rungs respect **renewable-vs-consumable discipline** (#525/#540/#554/#555's shared-resource machinery; OoTMM's renewable/license triad as precedent); exhaustion is designed out, not counted |
| D10 | **License mechanics if porting OoTMM material**: MIT of record (root LICENSE, 1072 bytes verbatim MIT [corrected 2026-09-20, see Amendment below — was misstated as 1093 bytes]) despite the two `"license": "ISC"` package.json fields (near-certain scaffolding leftovers — discrepancy recorded here so nobody rediscovers it); ported files carry the copyright line; a repo `THIRD_PARTY_NOTICES` accompanies any port; ~~an upstream issue asks OoTMM to fix the fields~~ **[VOIDED 2026-09-20 — see Amendment below]**; algorithms *reimplemented from reading* are not a port, world-data YAML/CSV taken wholesale is |
| D11 | Scope exclusions for 3.2: **entrance randomization** and **networking/multiworld** are OUT; each exclusion becomes its own epic (below) |

### Accepted answers (operator, 2026-07-31)

Eleven questions went to the operator on 2026-07-31; nine came back answered
and bind as written here. The O-numbers are preserved because the prose above
cites them by number. (Settled earlier and struck from the corpus's ten: pool
removal (→ D3), "what does MM-beatable mean" (→ D1/D2), profile-freeze timing
(one-game ruling), bidirectionality (a single bag is inherently
bidirectional), license mechanics (→ D10).)

| # | Question | The answer, binding |
|---|---|---|
| O1 | **MM-beatability parameters at the combo surface**: which of MM's goal parameters are surfaced/validated per GOAL value; disposition of the dead `RO_ACCESS_MAJORA_REMAINS` row (`Options.cpp:36`, no consumer — implement or retire); whether `beat-either` permits a half whose own parameters make it unbeatable by choice | **Both halves of the `beat-either` question, per §1.2's amendment**: an unbeatable half *is* permitted, including one made unbeatable by its own authored parameters (the creation-time warning stands) — and the goal expression stays a plain OR, so the fill/solver must never bias toward or enforce only-one-beatable; both halves provable is a welcome outcome, never constrained away. **The dead `RO_ACCESS_MAJORA_REMAINS` row is RETIRED, not implemented**: it never gains a consumer and never becomes a working control. Retirement is *not* enumerator deletion — deleting it renumbers the 44 enumerators after it and `RANDO_SAVE_OPTIONS` is indexed by that number in every already-written MM rando save (`Options.cpp:23-36`) — so the always-zero row stays for id-space totality and the combo pane keeps drawing it disabled-with-reason (ADR 0004 §5). Recording the retirement at the row is a small code change carried by an increment-1-adjacent PR, not by this ADR. Per-GOAL surfacing/validation of the *live* parameters is GOAL-UI work the increment epic owns |
| O2 | **The crossing edge's requirement expression**, per direction, for Happy Mask Shop ↔ Clock Tower: time-slice re-stamp on entry to MM (OoTMM re-stamps Day 1 and ANDs `can_reset_time`; redship must pick its equivalent), return-trip requirement, interaction with arrival hydrate-or-refuse | **Accepted as recommended — the OoTMM precedent**: Day-1 re-stamp into the MM time state plus a reset-time guard on entry to MM, with a symmetric requirement on the return trip. The increment epic authors the exact per-direction expressions under that shape (§2.1) |
| O3 | **Attempt-ladder spec**: hash recipe and separator, max attempts, where the attempt index is recorded in save + spoiler, terminal-failure UX copy on the #533 surface | **Accepted as recommended**: the implementer picks the hash recipe/separator, the attempt maximum and the save+spoiler recording sites, bound by this ADR's determinism rules (the world stays a pure function of the frozen identity) and the #533/#568 terminal-failure surface |
| O5 | **Canonical MM time-slice list**: redship has 45 (`Logic.h:20+`), OoTMM has 46 (missing `NIGHT2_AM_05_30`); adopt the u64 representation, add the slice or justify its absence, pin the list where both the crawl and any data port read it | **Accepted**: adopt the 46 slices — add `NIGHT2_AM_05_30` — with the u64 representation, pinned where both the crawl and any data port read it, **unless** the increment epic's source review justifies otherwise |
| O6 | **Monotonicity enforcement mechanism** for lambda logic: review rule + grep/clang-tidy probe + a CI check that adds items and asserts the reachable set never shrinks — which combination, and where it runs | **Accepted: all three mechanisms**, not a choice among them — the review rule, the static probe (grep/clang-tidy over the lambda logic), and the CI grow-check that adds items and asserts the reachable set never shrinks |
| O7 | **Boundary/constraint carrier scalars and carve budget**: exactly which game-neutral fields cross (SharedItem + host check id + earliness bound?), sized against `reserved[132]` under the append-only second-block rule and the 64-byte floor | **Accepted**: the increment-3 epic sizes the carve, against `reserved[132]` under the append-only second-block rule and the 64-byte floor [corrected 2026-08-04, #584: the live figure is `reserved[124]`, not 132 — the #569 and #581 carves landed after this answer was ratified; the append-only rule and the 64-byte floor are unaffected, and this correction does not reopen O7's answer] |
| O8 | **Shared-item classification authority**: one owner per shared item's junk/progression/renewable classification (OoTMM's `SHARED_BOMBCHU` two-settings wart is the cautionary tale) | **Accepted**: a single-owner classification table, one owner per shared item, living in the sanctioned ADR 0002 TU pair (`src/common/shared_items.h` / `src/common/shared_items.c`, deliberately free of game headers) — never a per-game duplicate that can disagree with itself |
| O10 | **Combo triforce-hunt accounting**: one shared piece count across both worlds (a shared-resource-style carrier) vs per-half counts ANDed/ORed; both engines' existing piece machinery is the substrate | **Decided: ONE shared triforce piece count across both worlds**, carried shared-resource-style — not per-half composition. Both engines' existing piece machinery (OoT `RSK_TRIFORCE_HUNT_PIECES_*`, MM `RO_TRIFORCE_PIECES_*`) is the substrate feeding that one combo-level count |
| O11 | **Shipped default rung** of the logic ladder (`beatable(T = ∅)` is the conservative candidate; base `none` is the operator's named audience) — and whether the default GOAL stays `beat-both` | **Decided: "copy what ship has for the rando settings."** At source, Ship of Harkinian defaults logic to Glitchless (`RSK_LOGIC_RULES` → `RO_LOGIC_GLITCHLESS`, `settings.cpp:1282`), every trick to "Disabled" (`TrickOption`'s default index 0, `option.cpp:361-364`), and `RSK_ALL_LOCATIONS_REACHABLE` to `RO_GENERIC_ON` (`settings.cpp:1283`). The combo's shipped default follows those defaults: the **proved, no-tricks rung** — `beatable(T = ∅)` — and **never** the base `none` rung, despite `none` being the operator's named audience. SoH's all-locations-reachable default carries through on the strictness axis §1.2 keeps per-half, which in ladder terms presents as `all-reachable(T = ∅)` if the combo surfaces strictness as a rung rather than as the per-half axis; that presentation choice is the increment epic's, and either way the default is the strictest SoH ships. Consistent with increment 1.1's paired-MM flip to Glitchless. **Default GOAL stays `beat-both`**: SoH has no combo-GOAL precedent to copy, and OoTMM's own `goal` likewise defaults to `'both'` (§1.1) |

### Still open (tracked; neither blocks increment 1)

| # | Question | What it decides | Owner |
|---|---|---|---|
| O4 | **The combo-fill implementation shape** (after the Decision 4 audit): composition's coordinator contract (each engine's exported query surface, snapshot/restore around MM's mutating queries, which TU owns the boundary under ADR 0002's one-sanctioned-TU rule) — or unification, if the audit makes that case | Increment 3's engineering core | **RULED 2026-09-17: composition** (operator; see the 2026-09-17 Amendment below). The coordinator contract is the audit's §4.1 surface with the three code-forced amendments; unification is rejected |
| O9 | **MM per-trick vocabulary**: does MM grow an `RT_*`-equivalent option table (the graph's TODO seams name the first candidates) before increment 3, or does its trick dimension ship RO_LOGIC-coarse at first and refine later | Trick axis symmetry | Pending a source inventory of the MM trick vocabularies — 2ship upstream, the original MM Randomizer, and OoTMM's MM tricks — research running separately from this ADR [annotated 2026-09-21 and 2026-09-26: the vocabulary exists — 86 keys declared, 20 reserved, 25 bound at `3b860bf3`; see the Amendments entries of those dates] |

## Non-goals → future epics

Excluded or deferred by this ADR. Per the operator's instruction, **each
line becomes its own epic issue** filed against the tracker after this ADR
is accepted (listed here; deliberately not filed by this ADR):

1. **Epic: cross-game entrance randomization.** Operator-excluded from 3.2.
   `src/common/entrance.h` was designed for generalization; the epic
   inherits O2's crossing-edge expression and D4's linked-fixpoint
   convention, and adds OoTMM's ER-fillability lesson (the `forwardFill`
   stress test exists because ER can be reachability-satisfiable but not
   incrementally fillable).
2. **Epic: networking / multiworld over the combo.** Operator-excluded from
   3.2. Builds on ADR 0006/0007 and #460's per-session identity handshake;
   a grant is acceptable only when sender identity == receiver creation
   identity (#564 V15).
3. **Epic: cross-game asset rendering.** **Permanently blocked** as things
   stand — 151 object-namespace collisions plus lazy archive mounting
   (`docs/resource-namespace-audit.md`); the shipped answer is native
   model-less presentation in both directions. The epic exists to hold the
   "unless the namespace is reworked" line so nobody rediscovers the
   blocker mid-increment; it is not scheduled work.
4. **Epic: hints v2 (cross-game hints).** MM area taxonomy (#500 work item
   4, including the dead `StaticData::RandoStaticRegion` decoy), foreign-host
   gossip-stone hints (today structurally impossible: the junk filter
   excludes every host), OoT-side cross-game hint type + serialization,
   check-tracker surfacing of foreign hosts, and — strictly on top of the
   deterministic core — OoTMM-style probabilistic foolish analysis as hint
   refinement.
5. **Epic: OoTMM world-data port.** The long-horizon unification target
   (Decision 4's "A only ever via the data port" corpus verdict survives as
   this epic): ~100 world files / ~1320 areas / ~6045 pool rows under D10's
   license mechanics. Only meaningful if the Decision 4 audit chooses
   unification, or if hand-maintaining two logic dialects proves the larger
   cost.

## Consequences

**Good:**

- The combo gets a real win condition with a real guarantee, parametrized
  the way both communities already think (goal + tricks + strictness), and
  the guarantee is constructive — the fill cannot terminate un-provable on
  a proved rung.
- Increment 1 is immediately startable, entirely inside MM's existing
  seams, and closes the one genuinely open beatability hole (the MM half)
  while the identity substrate (#570) finishes landing.
- The composition posture keeps both upstream diffs small and makes ADR
  0002 hold by construction; the audit requirement makes the alternative an
  informed choice rather than a rewrite instinct.
- Every failure path terminates on the already-landed #533/#568 REFUSED
  surface — no new failure UX is invented, and refusal no longer risks data
  loss (#569).

**Costs, accepted with eyes open:**

- **Freeze-at-creation UX**: after file-create, GOAL, tricks, logic rung and
  every goal parameter are read-only (ADR 0004 §6 state 4). The player who
  wants to "loosen logic mid-run" is refused; that is the one-game ruling's
  price, and the pane copy must say "already decided", not "not available".
- **Creation-time compute budget**: the whole proof (and the attempt
  ladder's retries) runs inside the creation event at file select. OoTMM
  ships thousands of whole-world re-solves as normal user options, so the
  fixpoint itself is cheap — but MM's 10s Glitchless wall-clock abort
  (`GlitchlessLogic.cpp:87-89`) is unmeasured under creation flow and must be
  measured, not assumed, before increment 2 ships. Re-roll time is a
  budgeted product cost. **Measured and decided 2026-08-04 (#582, PR #581):**
  14/66 heavy-profile first attempts failed, 100% of those by wall-clock
  abort and 0% by dead-end — the abort is the *dominant* failure mode, not a
  tail case. The operator decision (see increment 2 above) is a visible
  generation-progress surface plus a ~30s floor, layered with a per-attempt
  adaptive budget keyed to host speed; #582 remains open as the
  implementation tracker.
- **Determinism re-pins, twice**: increment 2 re-pins the
  SeedDeterminism/MMRandoGen/HeadlessForeignDigest rows; increment 3 re-pins
  everything again when the fill unifies. Batched deliberately; never
  silently.
- **Increment 3's blast radius is the largest in the phase**: both fills'
  ordering, the spoiler identity (one artifact, both worlds — #564 V23),
  boundary carve bytes against a 132-byte budget [corrected 2026-08-04, #584:
  now a 124-byte budget — see ADR 0009's amendment] with a 64-byte floor, and
  the CI tier cost (MM logic needs registrars + a live save; the `rando`
  label runs under a display and is skipped by `--test all`). Budget for
  slower feedback.
- **`beat-either` is exactly what it says**: the un-required half may be
  unfinishable. Documented at the setting and warned at creation, not
  discovered in a bug report — and the OR is never quietly narrowed to an XOR
  (§1.2).
- **Two logic dialects persist** under the composition default — the same
  reachability fact is written as a C++ lambda two different ways, forever,
  unless the audit chooses unification and pays its port cost instead.

**Risks:**

- MM's Glitchless solver mutates the live `gSaveContext`
  (`GlitchlessLogic.cpp:38/:282`); every coordinator round must run the
  memcpy swap discipline or corrupt the session. This is the single biggest
  structural obstacle the audit must size (and the strongest standing
  argument unification will make for itself).
- The vacuity trap: pair-level beatability locks written before items leave
  origin pools pass today and keep passing after a broken change. Locks
  land with removal (increment 3), never before.
- `RandoHintCrossGame` still sounds like cross-game hint plumbing and still
  is not (it is the #441 arrival-wipe regression lock); the dead
  `StaticData::RandoStaticRegion` still reads like the area taxonomy Lane D
  wants and still has zero references. Both traps are inherited by every
  epic above and are re-stated here because they will outlive this ADR's
  authors' attention.

---

## Amendments

### 2026-09-17 — MM code anchors re-measured at `7bab54bd` (#662)

The six MM-code line citations below had drifted from the lines they name,
most recently by PR #680's merge (which shifted `Logic.cpp` further down).
Re-measured directly against the file bodies at `origin/main` = `7bab54bd`
(PR #680, ADR 0010 increment 2, merged 2026-09-17) and corrected in place
above; this entry is the amendment-log record, not a text change to any
decided position.

| Citation | Was | Now (`7bab54bd`) |
|---|---|---|
| `FindReachableRegions` traversal | `Logic.cpp:92-136` | `Logic.cpp:126-188` |
| Glitchless save-mutation seam (snapshot / error-path restore / success-path restore) | `GlitchlessLogic.cpp:22, :57, :258` | `GlitchlessLogic.cpp:38, :78, :282` |
| Glitchless wall-clock abort check | `GlitchlessLogic.cpp:64` | `GlitchlessLogic.cpp:87-89` |

No decided text changed. The anchors will drift again as the increment-3
epic (#645) edits these same files; re-measure at that point rather than
trusting this table.

### 2026-09-20 — O4 answered: composition (operator ruling)

**Operator ruling, 2026-09-17, verbatim: "composition is the way here, lets
us keep bringing upstream in."** This closes open question O4. The
solver-inventory audit (`docs/solver-inventory.md`, Decision 4's explicit
first step) reported on 2026-09-10 (PR #647); the operator ruled on the
recommendation on 2026-09-17.

**The combo fill is built by composition**: the two solvers stay
authoritative and untouched in their dialects, and a game-header-free
coordinator in `src/common` (working name `combo_logic.c/.h`, the twin of
`foreign_items.c`) owns only the union bag, the two origin-keyed placement
tables and the round loop. Each engine exports a C-linkage query surface
over game-neutral scalars from a TU beside its foreign-items TU (audit
§4.1: begin/assume-own-item/expand/crossing-open/check-reached/
reached-empty-hosts/goal-reached/place, plus MM's snapshot/restore).
Unification is rejected: the two dialects encode different world-state
lattices (audit §5 — OoT: age × day/night over a detached save; MM: a
45-slice time mask over the live save with a free cycle reset), and merging
them would fork both ports' region files permanently. The operator's stated
reason for the ruling is keeping the ability to bring upstream logic fixes
in.

Three amendments the code forces are adopted with the recommendation (audit
§6.1):

1. Every MM query, the crawl included, writes the live save, so one
   snapshot/restore brackets a whole coordinator round and the round's
   placements are re-applied after restore, as MM's own fill already does.
2. The union bag is only OoT's last general pass (`fill.cpp`'s
   remaining-advancement fill plus MM's whole shuffled pool) — OoT's
   restricted-pool passes and both games' junk `FastFill`s stay per-game,
   and MM's forward fill does not run for paired worlds.
3. The crossing observables are region facts (`RR_MARKET_MASK_SHOP` child
   access on the OoT side; `RR_CLOCK_TOWER_INTERIOR ∈ reachable` on the MM
   side), not new graph edges, so neither port's region files change for
   increment 3.

Consequences for the increment-3 epic (#645): its first act is no longer a
decision but two measurements the audit could not make statically (§6.3) —
the cost of one linked round against the #582 creation budget floor, and
whether an assumed fill over MM's forward-authored graph converges on the
shipped profile without retries. D7's "composition is the default posture"
is now the decided posture; #576 (the OoTMM world-data port, unification
tail) is closed as not-applicable to this phase (see below).

No other decided text in this ADR changes. The O4 row above is annotated in
place per this amendment's precedent (the 2026-09-17 anchors amendment); the
underlying decision record is this dated paragraph.

### 2026-09-20 — D10 correction: OoTMM LICENSE size, and the upstream-issue clause voided

Two corrections to D10, found while closing O4:

1. **Byte count.** D10 as written stated OoTMM's root `LICENSE` is 1093
   bytes. Re-measured directly against the upstream file
   (`gh api repos/OoTMM/OoTMM/contents/LICENSE --jq .size`, read-only, this
   amendment's date): it is **1072 bytes**. The MIT-of-record conclusion is
   unaffected; only the byte count was wrong. Corrected in place above per
   the same precedent as the 2026-09-17 line-anchor corrections — this
   paragraph is the amendment-log record, not a text change to any decided
   position.
2. **The upstream-issue clause is VOID.** D10 as written calls for "an
   upstream issue asks OoTMM to fix the fields" (the two stray
   `"license": "ISC"` `package.json` entries). The operator's standing
   no-upstream-reports directive — never file, draft, propose, or mention an
   issue/PR/comment to HarbourMasters, OoTMM, or any other external repo,
   and never offer it as an option; reading external repos is fine —
   postdates and overrides this clause of D10. **Voided 2026-09-20**, reason:
   the no-upstream-reports directive overrides it; nobody acts on the
   ISC/MIT discrepancy externally. The discrepancy itself stays recorded in
   D10 for posterity. Reading external repos read-only (as this correction's
   re-measurement did) remains fine — the directive bars outbound reports,
   not inbound reads.


### 2026-09-21 — The bar exists: creation progress is on screen (#582)

Decision 5's increment-2 text above, under *The progress surface*, says "an
in-frame progress BAR needs a render-during-blocking-work seam that does not
exist in this tree and would live in `SohGui`; the channel is built and wired,
the bar is not, and that is stated rather than implied." **That sentence is
superseded as of this date.** The seam was found rather than built: OoT's ROM
extraction (`OTRGlobals::RunExtract`) already presents live ImGui frames from
inside a blocking loop, with the sequence `HandleEvents` / `IsFrameReady` /
`Gui::StartDraw` / `Interpreter::StartFrame` / `RunGuiOnly` / `Gui::EndDraw` /
`Interpreter::EndFrame`, and `RunGuiOnly` is `Run()` minus the display-list
execution — it touches the interpreter's own RSP/RDP state, which `Run()`
re-initialises at its top, and never OoT's `gfxCtx`. So the frame the outer
update is in the middle of building is unaffected, and the same sequence is
safe to pump from inside the creation call.

What was added: `src/common/gen_progress_overlay.{h,c}` (the state machine —
HIDDEN → SHOWN → DISMISSED or FAILED, and a fraction that is a watermark rather
than an estimate, because both the attempt ladder and the per-attempt
elapsed/budget ratio reset and a freshly computed bar would rewind at every
re-roll), `games/oot/soh/SohGui/CreationProgressOverlay.cpp` (the painter, with a
latched render-thread id, a re-entrancy latch, and a live-render-loop
precondition so a test harness's real window is never rendered into), and one
heartbeat inside MM's Glitchless fill loop.

Two properties of the painter are worth recording because they are what keep a
headless creation and a windowed one the same generation. The install REFUSES when
nothing here is presentable, leaving the painter slot empty so the fill's heartbeat
guard is false and the fill does not read its clock a second time — a headless
paired creation is the pre-#582 code path, not a path through bailing guards. And a
pumped frame is submitted with ImGui's mouse and keyboard suppressed, because
`Gui::StartDraw` → `Gui::DrawMenu` submits SoH's whole menu and every registered
`GuiWindow`, so without the suppression a click during a creation would run a menu
handler re-entrantly on the render thread. Both that suppression and the painter's
own re-entrancy latch are RAII guards rather than trailing assignments: the draw
does resource lookups and file I/O (`Gui::CheckSaveCvars` → `ConsoleVariables::Save`),
and an escape past a trailing restore would have left the process input-dead and
the overlay permanently refusing.

**Three positions this does not move.** (1) PR #581 §2a still holds: the
repaint decision is made with milliseconds the fill had already computed for its
own timeout check, and no wall clock decides anything about a world. (2) The
budget numbers are unchanged — but presentation time is now CREDITED BACK to
**both** of the creation's wall-clock stops, because a painted frame waits for
vblank and charging that to a stop would give a host with a window measurably less
generation headroom than the same host headless: two abort probabilities for one
seed on one machine, decided by whether anything was on screen. The two stops are
credited separately and each in its own clock, because they are compared against
baselines in different clocks: the fill's PER-ATTEMPT timeout at its own call site
(`tick += paintDuration`, a `GetUnixTimestamp()` delta), and the ladder's TOTAL
budget through a `Combo_GenProgress_PresentationBegin/End` bracket around every
painter call, which `Combo_GenProgress_GenerationElapsedMs()` subtracts and which
`OnFileCreate.cpp` compares instead of raw elapsed. Crediting only the first —
which is what this work's first cut did, and what review caught — left the
asymmetry intact at the second stop, where a windowed host would fail a creation
the same host completed headless. Wall elapsed is still what a player is shown and
what the P12 line measures; only the number that DECIDES excludes presentation.
(3) The creation
event's snapshot bracket is unchanged in effect and gained a second use: the WHOLE
pumped frame — `Gui::StartDraw` through `Interpreter::EndFrame` — runs with OoT's
snapshot swapped in and MM's in-flight bytes swapped back after, because
`Gui::StartDraw` → `Gui::DrawMenu` Draw()s every registered `GuiWindow` and SoH's
item and check trackers are two of them; inside the bracket `gSaveContext` holds
MM's world through OoT's layout. Review of the first cut found this recorded
wrongly here and implemented wrongly there: it named `Gui::EndDraw` /
`DrawFloatingWindows` as the tracker draw site (in this tree `DrawFloatingWindows`
draws no SoH window at all — it is ImGui's multi-viewport platform pass) and
wrapped only the overlay's own ImGui draw, one statement AFTER the trackers had
already drawn. The correction is recorded rather than quietly fixed because the
failure mode is instructive: the hazard was identified correctly, fenced in the
wrong place, and no row went red, since the only assertion pointed at it compared
`gSaveContext` AFTER the creation returned — where the seam restores OoT's snapshot
unconditionally — and stayed green with the bracket deleted. The lock now is a
`GuiWindow` registered beside the trackers, recording the bytes it is shown from
inside that same `DrawMenu` loop.

**Which phases this surface can show, stated because the brief's list is wider
than the answer.** `FREEZE`, `OOT_FILL`, `SPOILER` and `CROSSINGS` are reported
from `3drando/playthrough.cpp`, which runs on the menu-side `randoThread`; the
painter refuses there by design (that thread must not touch ImGui, the GL context
or the Fast3D interpreter), so those phases live on the stderr leg and on file
select's own "generating" caption. In the blocking CREATION path the overlay shows:
"Preparing to build your paired world" → "Preparing Majora's Mask's logic tables" →
"Building the Majora's Mask world (attempt n of N)" → "Writing the paired spoiler"
→ "Saving" → done/failed. The second and fourth captions were added by the same
review: MM's rando-core init before the ladder and the spoiler join after it had no
report at all, so the surface's first sight was a bar captioned with the IDLE
phase's name. Both stretches are now also timed on every creation, so their cost is
a number in the log rather than an estimate in a comment.

The rest of the *progress surface* paragraph (two sessions per creation, the
stderr leg, the second session as P12's measurement) is unchanged and still
accurate.

### 2026-09-21 -- O9: MM's per-trick vocabulary now exists, part-bound

Section 3.1 above (and the O9 row) still read "MM does not have it yet" /
"pending a source inventory". That premise is stale as of `#578` parts 1-2
(PR #686, PR #696), re-measured directly against
`games/mm/2s2h/Rando/StaticData/TrickIds.h` and
`games/mm/2s2h/Rando/StaticData/Tricks.cpp` at `origin/main` on this date:

- **86** `MMRT_*` keys are declared in `TrickIds.h` (append-only, `MMRT_MAX`
  sentinel; the enumerator's number is folded into every written MM rando
  save and the paired profile identity string, so the space is frozen the
  same way OoT's settings hash is).
- **20** of those 86 are declared `reserved = true` in `Tricks.cpp`'s table
  and are unconditionally inert (`IsTrickEnabled` returns false) -- OoTMM
  combo tricks that need an OoT-side item, not expressible in MM-only terms
  until increment 3's single bag exists.
- **10** of the remaining 66 are wired into a region's logic guard as an
  `MM_TRICK(...)`-guarded disjunct (`kBoundTricks` in
  `games/mm/2s2h/Rando/OptionsUiSingleExe.cpp` is the authoritative list; a
  `mm-trick-bindings` lock fails if that array drifts from what is actually
  consulted). Two shipped with the substrate (PR #686); eight more followed
  (PR #696). The remaining plain widenings over already-declared keys are
  tracked as `#697` (`#578` part 3, in flight at this date).

So O9's first question -- does MM grow an `RT_*`-equivalent option table --
is answered **yes**, and the second -- ship coarse and refine later -- is
what is happening: the vocabulary is declared in full, consulted in part.
O9 stays open until the remaining bindings (and any tightenings #697 finds)
land; this paragraph corrects the stale "does not have it yet" premise in
3.1 and records the numbers as measured, it does not close O9. No other
decided text in this ADR changes.

### 2026-09-26 -- Decision 5's gen-budget reference is 18 ms, not 55 ms (#708)

Decision 5's increment-2 text, under *The budget (#582)*, said the host
calibration is measured "against a pinned 55 ms reference". The source pins
**18 ms** (`src/common/gen_budget.c`, `GENBUDGET_REFERENCE_MS 18u`, beside the
24 M-iteration `GENBUDGET_CALIBRATION_ITERATIONS` workload). **The ADR was the
wrong one**; the number is corrected in place above with a bracketed pointer
to this entry. The evidence, all read on this date:

- **The code never held 55.** `git log -S GENBUDGET_REFERENCE_MS` names one
  commit, the squash of PR #680 (`7bab54bd`). Across all eight of that PR's
  pre-squash commits, from the first (`913778eb`, which introduced
  `gen_budget.c`) to the last, the pair is `24000000u` iterations against
  `18u` ms, unchanged. The loop was never resized, so "the ADR described an
  earlier, larger workload" is ruled out.
- **The 55 entered with the prose alone.** `git log -S "55 ms"` on this ADR
  names the same squash; its source is PR #680's docs commit `490e280f`
  ("Record ADR 0010 increment 2 as built"), which touched no code.
- **PR #680's own body says 18**: "measured once per process by timing a
  fixed 24 M-iteration integer loop against a **pinned 18 ms reference**
  (this workstation, 2026-09-17)".
- **The reference host still measures 18.** The workload's own stderr line,
  on the development workstation, reads
  `host calibration 18ms against a 18ms reference -> scale 100%` (#708's
  observation) and `19ms ... -> scale 105%` (the #722 measurement runs,
  whose #645 comment reports a host scale of 100-105%). Had the reference
  been 55 ms, that host would measure roughly a third of the reference and
  every figure in the #722 comment would have been labelled with a
  different scale.

**Nothing behaved wrongly and no budget moves.** Only the ratio is used, the
constant in source is self-consistent with the workload it pins, and it is
not re-measured here (`gen_budget.c` reserves re-measurement for a deliberate
change of reference machine, as its own commit). No other decided text in
this ADR changes.

### 2026-09-26 -- O9: the per-trick vocabulary, re-counted after #578 part 3

The 2026-09-21 O9 entry above recorded 86 keys declared, 20 reserved and
**10** bound. Part 3 of #578 (#697) has since merged twice (PR #703 and PR
#713), so the bound figure is stale. Re-counted directly at `origin/main` =
`3b860bf3`:

- **86** `MMRT_*` keys declared in
  `games/mm/2s2h/Rando/StaticData/TrickIds.h` (before the `MMRT_MAX`
  sentinel) -- unchanged.
- **20** of them declared reserved in `Tricks.cpp`'s `MMRT(...)` table --
  unchanged; still inert until the single bag exists.
- **25** bound: `kBoundTricks` in
  `games/mm/2s2h/Rando/OptionsUiSingleExe.cpp` lists 25 keys, and the set of
  keys consulted through `MM_TRICK(...)` anywhere under
  `games/mm/2s2h/Rando/Logic/` is the same 25 (counted by distinct key, not
  by call site). The `mm-trick-bindings` lock keeps the two equal.
- So **41** declared, non-reserved keys are not yet consulted by any edge.

One key was bound and then withdrawn during part 3's review:
`MMRT_PALACE_GUARD_SKIP` is deliberately absent from `kBoundTricks` (its
comment there records why: whether the bare `CAN_BE_DEKU` on
`RR_DEKU_PALACE_OUTSIDE -> RR_DEKU_PALACE_INSIDE_LOWER` is the guards or the
poison water its sibling edges model is not settled by the file, and under
the water reading the binding over-widens; #697's review comment of
2026-09-22 adds that it would hand the fill a zero-item Human Link route).
#697 stays open for that judgement and for the seams its review recorded as
owed. #578, the epic, is closed.

O9's position is therefore unchanged in kind from the 2026-09-21 entry --
the vocabulary is declared in full and consulted in part -- with the part
now 25 of 66 bindable keys. The O9 row is annotated in place with these
numbers. No other decided text in this ADR changes.

### 2026-09-26 -- O5 adopted: the canonical MM time-slice list is 46 (#643)

The O5 row accepted "adopt the 46 slices -- add `NIGHT2_AM_05_30` -- with the
u64 representation, pinned where both the crawl and any data port read it",
and the 2026-09-17 ruling folded it into increment 3's re-pin. It has now been
done, on branch `claude/world-moving-bundle-583-681-643-719`:

- `TIME_NIGHT2_AM_05_30` sits after `TIME_NIGHT2_AM_05_00` in
  `games/mm/2s2h/Rando/Logic/Logic.h`; every Day-3 and Night-3 enumerator moves
  up by one (names unchanged); `TIME_ALL_SLICES` is `0x3FFFFFFFFFFF`.
- **The pin** is a set of `static_assert`s beside the enum:
  `TIME_SLICE_COUNT == 46`, `TIME_ALL_SLICES == (1 << TIME_SLICE_COUNT) - 1`,
  and a count that fits the u64. `HALF_DAY_TIME_RANGES` is written with the
  enumerators rather than ordinals, and asserts pin that the six half-days tile
  slices 0..45 contiguously. There is one definition and no second copy.
- The source review O5 allowed for found no authored guard that needs the new
  boundary (`Logic/Regions/` names no 05:30 slice), so the slice is vocabulary:
  it is there for a ported guard and for the coordinator's and any #576 data
  port's enumeration.
- **Measured:** the three golden rows (`GoldenSeedDigestDefault`,
  `GoldenSeedDigestProfileV1`, `GoldenPairedAttemptDigest`) pass with the
  golden files untouched on that commit, so under the pinned profiles the
  crawl's results (`mmReachableCount`, `mmReachableHash`) and the MM fill did
  not move and O5 costs no re-pin. Both pinned profiles run with clock shuffle
  off; a clock-shuffle world, whose crawl takes the sequential path, is not
  pinned by any golden and was not measured.

The O5 row reads **adopted (46 slices), 2026-09-26** with this entry. P7's
decision (`docs/solver-inventory.md` section 6.2) is taken here, but that
document is outside this change's file set and still reads 45 in three places:
the world-state dimensions row (`:27`), the O5 entry in its open-question list
(`:688`) and the P7 row's status (`:1141`). Until a follow-up annotates them,
this entry and the code are the current statement.

### 2026-09-26 -- O11: Deku-Stick combat is trick-gated, like the Powder Keg (#719)

O11 names `beatable(T = empty)` as the shipped rung. `CanKillEnemy` offered a
Deku Stick as an ungated weapon on 17 actor rows while OoTMM's
`MMRT_DEKU_STICK_FIGHTING` ships default-off, so T was not empty in that
respect. The 17 disjuncts now read `CAN_FIGHT_WITH_DEKU_STICK`
(`MM_TRICK(MMRT_DEKU_STICK_FIGHTING) && HAS_ITEM(ITEM_DEKU_STICK)`), the key is
bound, and `mm-trick-table` check (j) holds a red/green pair per actor id. The
stick as a fire source is not the trick and stays ungated. The operator's
"sync and gate" ruling for the keg and GBT cases is the precedent. Measured:
the three golden rows pass unchanged, so the pinned worlds did not move. The
2026-09-26 O9 count above becomes **26** bound keys (40 declared, non-reserved
keys not yet consulted).

### 2026-09-26 -- O5: merged with PR #729, and no golden moved

The 2026-09-26 O5 entry above was written on the branch
`claude/world-moving-bundle-583-681-643-719`. That branch merged to `main` as
PR #729 (`e606a4c7`), and #643 was closed by it. The O5 commit (`ce554649`)
needed **no golden re-pin**. The bundle's one re-pin (`d8dbcc8c`) belongs to
#583's shuffled forward drop order, which changed only the item-id field of the
forward `foreign<n>` triples. The same PR added a fourth golden,
`GoldenSeedDigestArmedCaps` (`tests/golden/seed-digest-armed-caps.txt`), which
pins a world with every give-capability family armed.

O5's adoption is therefore complete on `main`. Item 10 of the #645 epic asked
that "any pinned-seed rows the renumbering moves are re-pinned in the same
commit as item 9's re-pin". There was nothing to carry, because the
renumbering moved no pinned world. The three places where
`docs/solver-inventory.md` still read 45 slices are annotated in that document
on this date.

### 2026-09-26 -- O6 delivered: all three mechanisms are on `main` (PR #734)

Accepted answer O6 asked for **all three** monotonicity mechanisms. PR #734
(`4a3bf058`) lands all three over both graphs. It is built on the multiplicity
contract described below (ABI 3).

- **The CI grow-check.** This is the `ComboLogicMonotonicity` row (rando tier,
  `src/common/tests/test_combo_logic_monotonicity.c`). It drives **both real
  engines through the coordinator's own surface**, granting one copy at a
  time. It runs four orders, each with tricks off and with every trick on. It
  asserts four things:
  - G1: after every grant, no reached check, region, OoT age/time bit or MM
    time slice is lost;
  - G2: the result is order-independent, checked against a one-shot round;
  - G3: the tricks-off closure is contained in the tricks-on closure, and is
    strictly smaller at some step on each engine;
  - G4: `Combo_Logic_RunRound`, run over growing prefixes, never loses a
    candidate host and never drops a crossing or goal flag.

  Each lock's red half was observed on an edge planted at runtime. No region
  file was touched.
- **The static probe.** This is `.github/scripts/check-monotonicity-negations.py`
  (with `--self-test`, 24 plants), run as the build-free CI job
  `monotonicity-probe`. It checks against
  `.github/monotonicity-negation-baseline.txt`, where every accepted entry
  must carry a written reading.
  - At `e606a4c7` neither graph negated player state.
  - The only sites that needed a reading were the three
    `logic->StoneCount() == 3` comparisons in OoT: two in `hyrule_field.cpp`
    and one in `temple_of_time.cpp`. `StoneCount()` is a sum of three
    set-item booleans, so `== 3` means exactly `>= 3`. The sites are
    **baselined as monotone**.
- **The review rule.** This is a Standing-conventions bullet in
  `.claude/worker-prompts.md`, and it names every spelling of a negation.
  `docs/monotonicity.md` gives the full statement, and lists what neither
  mechanism sees: block-bodied guards, locals, macros, non-literal ternaries,
  and the `location_access.cpp` helpers.

Closures measured on the shipped profile (checks / regions):

| Engine | Tricks off | Tricks on |
|---|---|---|
| OoT | 1,789 / 624 | 1,794 / 626 |
| MM | 2,256 / 314 | 2,256 / 314 |

MM's final closures are equal. Its tricks-on closure is strictly larger at 218
of 2,283 steps.

One finding was filed rather than fixed (#733). MM's `RI_HEART_PIECE`, which
O8 classes as junk, grows the closure by two checks through
`CHECK_MAX_HP(4)`. Monotonicity still holds; the gap is in the bag model's
classification.

O6 is **delivered**. CI enforces it; this text does not.

### 2026-09-26 -- O8 delivered: the classification table, with trap as a fourth class (PR #725)

Accepted answer O8 asked for a single-owner classification table in the ADR
0002 TU pair. PR #725 (`e638538c`) delivers it:

- **Owner.** `src/common/shared_items.{h,c}` is the owner. It stores one
  `(class, arming word)` per `(origin, id)`, taken from one walk of each
  game's source. Queries take a `SharedItem`.
- **Sources.** There is one source per game, in that game's own engine TU:
  `OoT_ComboLogic_ClassifyItem` and `MM_ComboLogic_ClassifyItem`. The owner
  refuses and counts a second registration, and refuses and counts a `NULL`.
- **Classes.** In precedence order: trap > progression > renewable > junk.
  Progression is each fill's own predicate, taken as-is: OoT's
  `Item::IsAdvancement()` and MM's GlitchlessLogic non-junk test.

**Why trap is a separate class and not junk.**

- Junk and renewable rows absorb surplus: they are what a plentiful pool
  displaces, and what fills hosts when there are more hosts than items. A trap
  does neither. Its count is fixed by a frozen setting, and it is a trap only
  inside its own game, which disguises it.
- The trap rule runs first because MM's own fill predicate calls `RI_TRAP`
  non-junk (`RITYPE_LESSER`). In any other order a trap would classify as
  progression.

**Crossing.** `Combo_ItemClassMayCrossUnder` lets an item cross only if it is
progression and its arming conditions hold. The arming word has two halves:
the low half holds the give-capability bits, and the high half holds OoT's
confinement families, one setting per bit.

**Not done yet:**

- **Nothing reads the table yet.** Wiring it into the bag is lane K9 in wave 6.
- **The table does not yet reconcile #525's cross-game shared quantities.**
  Bombchus, double defense and hearts get different classes in the two games
  (#731), and MM heart pieces gate two checks (#733, above).

O8 is **delivered as a table**. What the table does to the fill arrives with
the wiring.

### 2026-09-26 -- Increment 3: multiplicity is the assume contract, and "a trap never crosses" is a caller convention (PR #728)

**The ruling.** The operator ruled on 2026-09-26: "yeah multiplicity is fine."
Items are assumed once per copy, as OoT's own assumed fill does.

**The contract.** PR #728 (`9041897c`) makes this the coordinator contract:
`combo_logic.h` `RSBS_COMBO_LOGIC_ENGINE_ABI` goes from 2 to 3. The vtable's
shape is unchanged; what changed is the meaning of `assumeOwnItem`:

- One call is one copy. The engine counts every call and never de-duplicates.
- A repeat of a set item is inert. A progressive clamps at its top tier. A
  counter clamps at its maximum.
- The result is order-independent. The clamps are what make it so.
- The own-origin harvest runs once per placed host per round.
- `beginQuery` owns the starting state. MM has no detached save, so for MM the
  caller puts the live save in the file-creation state first.

**The OoT clamp is round-scoped.** `Rando::gComboLogicRoundClamp` in
`logic.cpp` is on only between the OoT engine's `beginQuery` and `endQuery`.
OoT's own fill therefore still runs upstream's unclamped arithmetic. Under a
plentiful pool that arithmetic wraps the wallet to 0 on the fourth copy
(#726; wave 6, lane F26).

**The MM clamp is in its give path.** `MmGiveOneCopy` clamps small keys,
stray fairies and skull tokens at maxima derived from MM's static check table,
and clamps triforce pieces at `RO_TRIFORCE_PIECES_MAX`.

**The bag model:**

- One row per copy.
- `ComboLogicBagItem.bagFlags` carries `RSBS_COMBO_BAG_SURPLUS`. Unknown flags
  are refused.
- The proof places and assumes **required rows only**.
- Surplus rows are placed after the proof, in bag order. Under `beatable` and
  `none` they go on any empty host; under `all-reachable`, only on the proven
  world's reached hosts. A confirming round then re-checks the exit
  condition, so a surplus copy is never load-bearing.
- When hosts run out, the remaining surplus rows are dropped, last row first.
  That is the coordinator's rule, not either native fill's. OoT keeps a random
  subset (a caller matches that by pre-shuffling with OoT's RNG); MM is
  all-or-nothing.
- A required row is never dropped. A required overflow is `ERR_NO_CANDIDATE`.
- Hosts left unplaced are reported through `Combo_Logic_LeftoverHosts`, for
  each game's own pass.

**Correction: traps.** The bag-model notes recorded on #645 on 2026-09-26 said
"no trap crosses games this increment", read as a property of the bag. **It is
not a property of the coordinator.**

- In both ports traps are counted pool rows fixed by settings, not junk-pass
  products: MM's `RI_TRAP` x `RO_TRAP_AMOUNT`, and OoT's fixed ice traps plus
  the `RSK_ICE_TRAP_PERCENT` conversion during pool generation.
- The coordinator would place a trap row handed to it on either game.
- **"A trap never crosses" is therefore a caller convention.** The wiring must
  keep traps out of the bag and hand each game's trap count to that game's own
  pass over its leftover hosts.
- The O8 predicate `Combo_ItemClassMayCrossUnder` already refuses a trap. But
  a refusal at the bag waits on the O8 table being wired in (lane K9); until
  the bag builder consults it, nothing on the coordinator's path enforces the
  refusal.

### 2026-09-26 -- Increment 3: the measured numbers after multiplicity

This entry extends the 2026-09-22 measurement (PR #722; the #645 comment of
that date) to ABI 3. The source is lane K4's #645 comment of 2026-09-26. The
conditions are the same: the `combo-logic-measure` row, the development
workstation at 100-105% host scale, the shipped profile, tricks off.

- **One linked round is still exactly 2 alternations.** Inside the fill it
  costs 10.5-10.7 ms at 32-512 rows and **17.9 ms at 2,489 rows**. That growth
  is the super-linear placement re-apply the 2026-09-22 extrapolation
  predicted.
- **`beat-either` is proved** on the 512-row bag (OoT 321 + MM 191, the
  2026-09-22 composition): 3 of 3 seeds, first attempt, 0 roll-backs, 1.00
  rounds per placed item. Under ABI 2 the same bag was `goal-unprovable` after
  3 attempts on every seed. Bags that carry only a sample of OoT's rows are
  **not** evidence, because OoT's native placements carry OoT's goal there.
- **`beat-both` fails at every bag that fits `RSBS_COMBO_LOGIC_BAG_CAP`
  (512).** The cause is the bag, not the fill. The capped MM half is a stride
  sample of MM's 2,168-row vanilla pool, so `goalMM=0` holds before any
  placement. A round that assumes MM's whole pool reads `goalMM=1` (#727).
- **The whole 2,489-row union bag proves `beat-both`** on the first attempt
  with 0 roll-backs, with the caps raised to 4096. That was a local
  experiment and was not committed.
  - One attempt measures **44.5-50.9 s end to end, 1.49-1.70x the #582 30 s
    floor**; `beat-either` falls in the same range.
  - This supersedes the 2026-09-22 linear extrapolation of 0.63-0.87x.
  - The `none` rung places the same bag in 130 ms.
- **The bag, as lane K8 read it** (PR #734, shipped profile):
  - OoT: 546 rows (321 progression, 105 surplus, 114 filler, 6 ice traps).
  - MM: 2,282 rows (250 progression, 114 surplus, 1,918 filler).

  The full-bag attempt is therefore dominated by MM filler that O8 classifies
  out. Rounds scale with items placed, so a bag of progression and surplus
  only (about 790 rows) is expected to cut an attempt to roughly a third.
  **That is an expectation, not a measurement.** Measuring it is wave 6's
  lane K9, which also raises the caps (#727).

**Not measured:** any plentiful or trap profile over the real engines (the
shipped profile has neither, so every real-engine surplus and drop figure is
0), tricks-on profiles, other hosts, and play.

### 2026-09-27 -- The OoT tier clamp is no longer round-scoped (#726)

The 2026-09-26 multiplicity amendment says the OoT clamp is round-scoped and
that OoT's own fill still runs upstream's unclamped arithmetic. **That is no
longer true of the tier half.**

- **Progressive rows clamp on every grant.** In `logic.cpp`, wallet,
  strength, scale, bomb bag, bow, slingshot, sticks, nuts and magic now stop at
  `ComboLogicProgressiveTopTier` (magic: 2) in OoT's native fill too, not only
  between `beginQuery` and `endQuery`.
- **Why it was needed.** On a plentiful + tycoon profile the native full-world
  harvest read the wallet at 0 and every other of those kinds one past its top
  (observed by `OoTPlentifulProgressive` before the fix).
- **Why unconditional is safe.** The clamp only touches grants, and
  `Item::UndoEffect` (the only removal caller) has no caller, so a clamped grant
  is never paired with an unclamped removal.
- **Counters stay round-scoped.** `Rando::gComboLogicRoundClamp` still gates
  the key, token, triforce-piece, heart and bean maxima.
- **Test-only suppression.** `gComboLogicTierClampSuppressed` exists so
  `combo-logic-multiplicity`'s clamp-off legs still observe upstream's
  arithmetic. Nothing else sets it.
- **Worlds.** The four goldens did not move. A plentiful + tycoon world can
  move: seed `RSBSUNIFIED1` changed 120 of 442 OoT locations.
